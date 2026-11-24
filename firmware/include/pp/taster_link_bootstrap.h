#pragma once
#include "taster_link_pairing.h"

namespace pp::taster_link::bootstrap {
namespace hs=handshake;
namespace pv=pairing;

// First use only, over a fresh standard TLS session. This interface is
// deliberately not transport::Stream: raw TCP is not a provisioning channel.
// The exclusive platform owner must establish TLS 1.3 or TLS 1.2 ECDHE with an AEAD suite,
// no early data/session resumption/renegotiation/plaintext fallback, and bind the selected
// STA association to this attempt. First-use peer identity is unverified
// (TOFU); TLS protects against passive interception, NOT an active MITM.
// Nonblocking progress: positive bytes, zero would block, negative failure.
// Platform TLS negotiation, endpoint binding and timeout are separate.
class EncryptedChannel {
public:
  virtual ~EncryptedChannel()=default;
  virtual bool secure() const=0;
  virtual int read(uint8_t*,size_t)=0;
  virtual int write(const uint8_t*,size_t)=0;
  virtual void close()=0;
};
// Trusted owner policy, not network input. Core must be genuinely empty,
// READY/no active race, with no live/admitted race peer or other bootstrap
// candidate. Owner grants exclusive reservation and keeps ordinary race
// work first; re-evaluate at every call and at durable commit. Taster must
// be unremembered, selected current association, no race peer/borrower.
// Neither this interface nor TLS itself confers sporting authority.
class Admission {
public:
  virtual ~Admission()=default;
  virtual bool permitted() const=0;
};

enum class Outcome:uint8_t { Fresh,Running,Complete,Failed,Uncertain };
enum class Kind:uint8_t { Request=1,Offer=2,Stored=3,Done=4 };
constexpr size_t Header=6,PairBytes=16,KeyBytes=32,
                 OfferSize=Header+PairBytes+KeyBytes,AckSize=Header+PairBytes;
constexpr uint64_t TimeoutUs=10000000,IoUs=2000000;
using Frame=std::array<uint8_t,OfferSize>;
inline void prefix(Frame& f,Kind kind) {
  f[0]='P';f[1]='P';f[2]='B';f[3]='S';f[4]=1;f[5]=static_cast<uint8_t>(kind);
}
inline bool shape(const Frame& f,Kind kind){
  return f[0]=='P' && f[1]=='P' && f[2]=='B' && f[3]=='S' &&
    f[4]==1 && f[5]==static_cast<uint8_t>(kind);
}

// One TLS channel/attempt. No replacement of remembered material, even
// when the old peer is disconnected. No automatic revoke/retry of uncertain
// storage. Taster commits before acknowledging; Core commits only after
// exact acknowledgement. Distributed commit is NOT atomic. A power cut or
// lost final response can require recovery. Complete means this exchange
// finished locally, not READY, authenticated PPTH, or remote durable proof
// beyond the protocol acknowledgement.
class Exchange {
public:
  Exchange(pv::Role role,pv::Vault& vault,const hs::Crypto& crypto,
           EncryptedChannel& channel,const Admission& admission)
    :role_(role),vault_(vault),crypto_(crypto),channel_(channel),
     admission_(admission),material_(crypto){}
  ~Exchange(){if(outcome_==Outcome::Running) abort();clear();}
  Exchange(const Exchange&)=delete;
  Exchange& operator=(const Exchange&)=delete;
  Outcome outcome() const{return outcome_;}
  // On completion the caller constructs a NEW ordinary PPTH connection
  // using the committed vault. Do not keep credentials from this object.
  bool start(uint64_t now){
    if(outcome_!=Outcome::Fresh)return false;
    outcome_=Outcome::Running;started_=last_=since_=now;
    if(!pv::validRole(role_) || role_!=vault_.role() ||
       vault_.state()!=pv::State::Empty || !channel_.secure() ||
       !admission_.permitted())return fail();
    expectedRevision_=vault_.revision();
    if(role_==pv::Role::Core) expect(Kind::Request,Header,now);
    else queue(Kind::Request,Header,now);
    return true;
  }
  // At most one read OR write call, one completed record transition per
  // poll; fixed buffers, no receive drain or retry of a persistence action.
  bool poll(uint64_t now){
    if(outcome_!=Outcome::Running)return false;
    if(now<last_ || now-started_>=TimeoutUs || now-since_>=IoUs ||
       !channel_.secure() || !admission_.permitted())return fail();
    last_=now;
    // Before mutation require exactly the original empty vault revision.
    // After mutation require the exact committed material still available.
    if(!committed_){
      if(vault_.state()!=pv::State::Empty || vault_.revision()!=expectedRevision_)
        return fail();
    }else if(!vault_.active() || vault_.revision()!=expectedRevision_+1 ||
             vault_.active()->pair()!=material_.pair() ||
             !crypto_.equal(vault_.active()->psk().data(),material_.psk().data(),KeyBytes))
      return fail();
    if(writing_){
      const int n=channel_.write(frame_.data()+used_,length_-used_);
      if(n<0 || static_cast<size_t>(n)>length_-used_)return fail();
      used_+=static_cast<size_t>(n);
      if(used_==length_)return sent(now);
      return true;
    }
    const int n=channel_.read(frame_.data()+used_,length_-used_);
    if(n<0 || static_cast<size_t>(n)>length_-used_)return fail();
    used_+=static_cast<size_t>(n);
    if(used_!=length_)return true;
    if(!shape(frame_,kind_))return fail();
    return received(now);
  }
  void abort(){if(outcome_==Outcome::Running)(void)fail();}
private:
  void clearFrame(){crypto_.wipe(frame_.data(),frame_.size());}
  void clear(){clearFrame();material_.clear();}
  void expect(Kind kind,size_t len,uint64_t now){
    clearFrame();kind_=kind;length_=len;used_=0;since_=now;writing_=false;
  }
  void queue(Kind kind,size_t len,uint64_t now){
    expect(kind,len,now);writing_=true;prefix(frame_,kind);
    if(len>=AckSize)std::memcpy(frame_.data()+Header,material_.pair().data(),PairBytes);
    if(kind==Kind::Offer)std::memcpy(frame_.data()+AckSize,material_.psk().data(),KeyBytes);
  }
  bool matching() const{
    return crypto_.equal(frame_.data()+Header,material_.pair().data(),PairBytes);
  }
  bool install(){
    // Both owner admission and existing Vault maintenance fences apply.
    if(!admission_.permitted() || !channel_.secure() ||
       vault_.state()!=pv::State::Empty || vault_.revision()!=expectedRevision_)return false;
    mutationAttempted_=true;
    if(vault_.install(expectedRevision_,material_)!=pv::Result::Applied)return false;
    committed_=true;return true;
  }
  bool received(uint64_t now){
    switch(kind_){
      case Kind::Request:
        if(role_!=pv::Role::Core || !material_.generate())return fail();
        queue(Kind::Offer,OfferSize,now);return true;
      case Kind::Offer:{
        if(role_!=pv::Role::Taster)return fail();
        hs::PairId pair{};hs::Secret key{};
        std::memcpy(pair.data(),frame_.data()+Header,PairBytes);
        std::memcpy(key.data(),frame_.data()+AckSize,KeyBytes);
        const bool ok=material_.import(pair,key);
        crypto_.wipe(pair.data(),pair.size());crypto_.wipe(key.data(),key.size());
        clearFrame();
        if(!ok || !install())return fail();
        queue(Kind::Stored,AckSize,now);return true;
      }
      case Kind::Stored:
        if(role_!=pv::Role::Core || !matching() || !install())return fail();
        queue(Kind::Done,AckSize,now);return true;
      case Kind::Done:
        if(role_!=pv::Role::Taster || !committed_ || !matching())return fail();
        return finish();
    }
    return fail();
  }
  bool sent(uint64_t now){
    switch(kind_){
      case Kind::Request:expect(Kind::Offer,OfferSize,now);return true;
      case Kind::Offer:expect(Kind::Stored,AckSize,now);return true;
      case Kind::Stored:expect(Kind::Done,AckSize,now);return true;
      case Kind::Done:return finish();
    }
    return fail();
  }
  bool finish(){
    if(!committed_)return fail();
    outcome_=Outcome::Complete;clear();channel_.close();return false;
  }
  bool fail(){
    outcome_=mutationAttempted_?Outcome::Uncertain:Outcome::Failed;
    clear();channel_.close();return false;
  }
  const pv::Role role_;pv::Vault& vault_;const hs::Crypto& crypto_;
  EncryptedChannel& channel_;const Admission& admission_;pv::Material material_;
  Frame frame_{};Kind kind_=Kind::Request;Outcome outcome_=Outcome::Fresh;
  uint64_t started_=0,last_=0,since_=0;uint32_t expectedRevision_=0;
  size_t used_=0,length_=0;bool writing_=false,committed_=false,mutationAttempted_=false;
};

} // namespace pp::taster_link::bootstrap
