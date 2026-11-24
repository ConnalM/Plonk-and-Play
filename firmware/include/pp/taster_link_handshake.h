#pragma once
#include "taster_link_wire.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace pp::taster_link::handshake {

// Host prototype. Uses standard HKDF-SHA256 and HMAC-SHA256 through a
// provider. No storage, commissioning, socket, or firmware ingress.
// One fresh object per connection; no retries on an existing transcript.
using Secret=std::array<uint8_t,32>;
using PairId=std::array<uint8_t,16>;
using Nonce=std::array<uint8_t,32>;
using Tag=std::array<uint8_t,32>;
constexpr size_t Header=6, HelloSize=54, ChallengePrefix=54,
                 ChallengeSize=86, ProofSize=38, TranscriptSize=108;
enum class Kind:uint8_t { Hello=1,Challenge=2,Proof=3,Finish=4 };
enum class Stage:uint8_t { Fresh,AwaitChallenge,AwaitProof,AwaitFinish,FinishPending,Established,Failed };
struct Packet { std::array<uint8_t,ChallengeSize> bytes{}; size_t length=0; };
struct DirectionKeys { Secret toCore{},toTaster{}; };
class Crypto {
public:
  virtual ~Crypto()=default;
  virtual bool random(uint8_t* out,size_t length) const=0; // CSPRNG or failure
  virtual bool hkdf(const Secret& psk,const uint8_t* salt,size_t saltSize,
                    const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize) const=0;
  virtual bool hmac(const Secret& key,const uint8_t* data,size_t length,Tag& out) const=0;
  virtual bool equal(const uint8_t* a,const uint8_t* b,size_t length) const=0; // constant time
  virtual void wipe(uint8_t* bytes,size_t length) const=0; // non-elidable
};

template<size_t N> inline bool nonzero(const std::array<uint8_t,N>& a) {
  uint8_t x=0; for(auto b:a) x|=b; return x!=0;
}
// Core owner must generate a fresh nonzero boot epoch using production
// entropy each boot. Retire link numbers even for unsuccessful handshakes.
// Exhaustion fails closed; never wrap or reset this object within a boot.
class LinkIds {
public:
  explicit LinkIds(uint64_t bootEpoch):epoch_(bootEpoch) {}
  LinkIds(const LinkIds&)=delete;
  LinkIds& operator=(const LinkIds&)=delete;
  bool next(Session& out) {
    if(!epoch_ || last_==UINT64_MAX) return false;
    out={epoch_,++last_}; return true;
  }
private:
  const uint64_t epoch_;
  uint64_t last_=0;
};

inline void prefix(Packet& p,Kind kind,size_t length) {
  p={}; p.length=length;
  p.bytes[0]='P';p.bytes[1]='P';p.bytes[2]='T';p.bytes[3]='H';
  p.bytes[4]=1;p.bytes[5]=static_cast<uint8_t>(kind);
}
inline bool shape(const Packet& p,Kind kind,size_t length) {
  return p.length==length && p.bytes[0]=='P' && p.bytes[1]=='P' &&
    p.bytes[2]=='T' && p.bytes[3]=='H' && p.bytes[4]==1 &&
    p.bytes[5]==static_cast<uint8_t>(kind);
}

class Handshake {
public:
  enum class Role { Core,Taster };
  Handshake(Role role,const Crypto& crypto):role_(role),crypto_(crypto) {}
  ~Handshake() { clearKeys(); }
  Handshake(const Handshake&)=delete;
  Handshake& operator=(const Handshake&)=delete;
  Stage stage() const { return stage_; }
  bool established() const { return stage_==Stage::Established; }
  // Only after mutual proof, and after Core's entire Finish was transmitted.
  // Consumer must copy/install into a session-owned provider before this
  // object dies, and securely retire that provider on disconnect.
  const DirectionKeys* keys() const { return established()?&keys_:nullptr; }
  Session session() const { return established()?session_:Session{}; }
  void abort() { fail(); }

  bool beginTaster(const PairId& pair,Packet& out) {
    out={};
    if(role_!=Role::Taster || stage_!=Stage::Fresh || !nonzero(pair)) return fail();
    Nonce nonce{};
    if(!crypto_.random(nonce.data(),nonce.size()) || !nonzero(nonce)) return fail();
    prefix(out,Kind::Hello,HelloSize);
    std::memcpy(out.bytes.data()+Header,pair.data(),pair.size());
    std::memcpy(out.bytes.data()+Header+pair.size(),nonce.data(),nonce.size());
    std::memcpy(transcript_.data(),out.bytes.data(),HelloSize);
    stage_=Stage::AwaitChallenge;
    return true;
  }
  bool acceptHello(const Packet& hello,const PairId& paired,const Secret& psk,
                   LinkIds& ids,Packet& out) {
    out={};
    if(role_!=Role::Core || stage_!=Stage::Fresh || !shape(hello,Kind::Hello,HelloSize) ||
       !nonzero(paired) || !nonzero(psk) ||
       !crypto_.equal(hello.bytes.data()+Header,paired.data(),paired.size())) return fail();
    Nonce peer{},local{};
    std::memcpy(peer.data(),hello.bytes.data()+Header+paired.size(),peer.size());
    if(!nonzero(peer) || !ids.next(session_) ||
       !crypto_.random(local.data(),local.size()) || !nonzero(local) || local==peer) return fail();
    prefix(out,Kind::Challenge,ChallengeSize);
    auto* p=out.bytes.data()+Header;
    wire::put(p,session_.epoch,8);wire::put(p,session_.link,8);
    std::memcpy(p,local.data(),local.size());
    std::memcpy(transcript_.data(),hello.bytes.data(),HelloSize);
    std::memcpy(transcript_.data()+HelloSize,out.bytes.data(),ChallengePrefix);
    if(!derive(psk) || !writeProof(challengeKey_,out,ChallengePrefix)) { out={};return fail(); }
    stage_=Stage::AwaitProof; return true;
  }
  bool acceptChallenge(const Packet& challenge,const Secret& psk,Packet& out) {
    out={};
    if(role_!=Role::Taster || stage_!=Stage::AwaitChallenge ||
       !shape(challenge,Kind::Challenge,ChallengeSize) || !nonzero(psk)) return fail();
    const auto* p=challenge.bytes.data()+Header;
    session_.epoch=wire::get(p,8);session_.link=wire::get(p,8);
    Nonce peer{},local{};
    std::memcpy(peer.data(),p,peer.size());
    std::memcpy(local.data(),transcript_.data()+Header+PairId{}.size(),local.size());
    if(!session_.valid() || !nonzero(peer) || peer==local) return fail();
    std::memcpy(transcript_.data()+HelloSize,challenge.bytes.data(),ChallengePrefix);
    if(!derive(psk) || !checkProof(challengeKey_,challenge,ChallengePrefix)) return fail();
    prefix(out,Kind::Proof,ProofSize);
    if(!writeProof(proofKey_,out,Header)) { out={};return fail(); }
    stage_=Stage::AwaitFinish; return true;
  }
  bool acceptProof(const Packet& proof,Packet& out) {
    out={};
    if(role_!=Role::Core || stage_!=Stage::AwaitProof || !shape(proof,Kind::Proof,ProofSize) ||
       !checkProof(proofKey_,proof,Header)) return fail();
    prefix(out,Kind::Finish,ProofSize);
    if(!writeProof(finishKey_,out,Header)) { out={};return fail(); }
    stage_=Stage::FinishPending;return true;
  }
  // This is a transport precondition, not a claim of peer delivery.
  bool finishSent() {
    if(role_!=Role::Core || stage_!=Stage::FinishPending) return fail();
    return establish();
  }
  bool acceptFinish(const Packet& finish) {
    if(role_!=Role::Taster || stage_!=Stage::AwaitFinish || !shape(finish,Kind::Finish,ProofSize) ||
       !checkProof(finishKey_,finish,Header)) return fail();
    return establish();
  }
private:
  bool derive(const Secret& psk) {
    // RFC5869 extract/expand with entire fixed public transcript as salt,
    // domain-separated info per purpose. No raw PSK used as record key.
    auto deriveOne=[&](const char* info,Secret& dst) {
      return crypto_.hkdf(psk,transcript_.data(),transcript_.size(),
          reinterpret_cast<const uint8_t*>(info),std::strlen(info),dst.data(),dst.size());
    };
    return deriveOne("P&P PPTH v1 core challenge",challengeKey_) &&
           deriveOne("P&P PPTH v1 taster proof",proofKey_) &&
           deriveOne("P&P PPTH v1 core finish",finishKey_) &&
           deriveOne("P&P PPTL v1 Taster to Core",keys_.toCore) &&
           deriveOne("P&P PPTL v1 Core to Taster",keys_.toTaster);
  }
  bool writeProof(const Secret& key,Packet& p,size_t offset) const {
    Tag tag{};
    if(!crypto_.hmac(key,transcript_.data(),transcript_.size(),tag)) return false;
    std::memcpy(p.bytes.data()+offset,tag.data(),tag.size());
    return true;
  }
  bool checkProof(const Secret& key,const Packet& p,size_t offset) const {
    Tag tag{};
    return crypto_.hmac(key,transcript_.data(),transcript_.size(),tag) &&
        crypto_.equal(tag.data(),p.bytes.data()+offset,tag.size());
  }
  void clearProofKeys() {
    crypto_.wipe(challengeKey_.data(),challengeKey_.size());
    crypto_.wipe(proofKey_.data(),proofKey_.size());
    crypto_.wipe(finishKey_.data(),finishKey_.size());
  }
  void clearKeys() {
    clearProofKeys();
    crypto_.wipe(keys_.toCore.data(),keys_.toCore.size());
    crypto_.wipe(keys_.toTaster.data(),keys_.toTaster.size());
  }
  bool establish() { clearProofKeys();stage_=Stage::Established;return true; }
  bool fail() { clearKeys();session_={};stage_=Stage::Failed;return false; }
  const Role role_;
  const Crypto& crypto_; // provider must outlive this object
  Stage stage_=Stage::Fresh;
  Session session_{};
  std::array<uint8_t,TranscriptSize> transcript_{};
  Secret challengeKey_{},proofKey_{},finishKey_{};
  DirectionKeys keys_{};
};
} // namespace pp::taster_link::handshake
