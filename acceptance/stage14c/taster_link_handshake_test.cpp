#include "pp/taster_link_handshake.h"
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <cassert>
#include <iostream>

namespace h=pp::taster_link::handshake;
namespace w=pp::taster_link::wire;
using namespace pp::taster_link;

// OpenSSL primitives; deterministic entropy only in explicitly marked
// fixtures, and real RAND_bytes for the additional random reconnect smoke.
class TestCrypto final : public h::Crypto {
public:
  bool random(uint8_t* out,size_t length) const override {
    if(failRandom) return false;
    if(realRandom) return RAND_bytes(out,static_cast<int>(length))==1;
    for(size_t i=0;i<length;++i) out[i]=static_cast<uint8_t>(counter+i);
    ++counter;
    return true;
  }
  bool hkdf(const h::Secret& psk,const uint8_t* salt,size_t saltSize,
            const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize) const override {
    if(failKdf) return false;
    auto* c=EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF,nullptr);
    if(!c) return false;
    size_t n=outSize;
    const bool ok=EVP_PKEY_derive_init(c)>0 &&
      EVP_PKEY_CTX_hkdf_mode(c,EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND)>0 &&
      EVP_PKEY_CTX_set_hkdf_md(c,EVP_sha256())>0 &&
      EVP_PKEY_CTX_set1_hkdf_salt(c,salt,static_cast<int>(saltSize))>0 &&
      EVP_PKEY_CTX_set1_hkdf_key(c,psk.data(),static_cast<int>(psk.size()))>0 &&
      EVP_PKEY_CTX_add1_hkdf_info(c,info,static_cast<int>(infoSize))>0 &&
      EVP_PKEY_derive(c,out,&n)>0 && n==outSize;
    EVP_PKEY_CTX_free(c);return ok;
  }
  bool hmac(const h::Secret& key,const uint8_t* data,size_t length,h::Tag& out) const override {
    if(failHmac) return false;
    unsigned n=0;
    return HMAC(EVP_sha256(),key.data(),static_cast<int>(key.size()),data,length,out.data(),&n)!=nullptr && n==out.size();
  }
  bool equal(const uint8_t* a,const uint8_t* b,size_t n) const override { return CRYPTO_memcmp(a,b,n)==0; }
  void wipe(uint8_t* p,size_t n) const override { OPENSSL_cleanse(p,n); }
  mutable unsigned counter=1;
  bool failRandom=false,failKdf=false,failHmac=false,realRandom=false;
};

class SessionMac final : public w::Mac {
public:
  SessionMac(const TestCrypto& crypto,const h::Secret& key):crypto_(crypto),key_(key){}
  ~SessionMac() override { crypto_.wipe(key_.data(),key_.size()); }
  bool sign(const uint8_t* data,size_t n,uint8_t* tag) const override {
    h::Tag t{}; if(!crypto_.hmac(key_,data,n,t)) return false;
    std::memcpy(tag,t.data(),t.size());return true;
  }
  bool verify(const uint8_t* data,size_t n,const uint8_t* tag) const override {
    h::Tag t{}; return sign(data,n,t.data()) && crypto_.equal(t.data(),tag,t.size());
  }
private:
  const TestCrypto& crypto_;
  h::Secret key_;
};

struct Exchange { h::Packet hello,challenge,proof,finish; };
using Role=h::Handshake::Role;
static void noKeys(const h::Handshake& x) { assert(!x.keys() && !x.session().valid() && !x.established()); }
static Exchange complete(h::Handshake& t,h::Handshake& c,h::LinkIds& ids,
                         const h::PairId& pair,const h::Secret& key) {
  Exchange p;
  assert(t.beginTaster(pair,p.hello));noKeys(t);
  assert(c.acceptHello(p.hello,pair,key,ids,p.challenge));noKeys(c);
  assert(t.acceptChallenge(p.challenge,key,p.proof));noKeys(t);
  assert(c.acceptProof(p.proof,p.finish));noKeys(c);
  assert(c.stage()==h::Stage::FinishPending);
  assert(c.finishSent());assert(t.acceptFinish(p.finish));
  assert(c.established() && t.established() && c.session()==t.session());
  assert(c.keys()->toCore==t.keys()->toCore && c.keys()->toTaster==t.keys()->toTaster);
  assert(c.keys()->toCore!=c.keys()->toTaster && c.keys()->toCore!=key && c.keys()->toTaster!=key);
  return p;
}
static w::Read feed(w::StreamDecoder& d,const w::Packet& p) {
  w::Frame f{}; auto r=w::Read::NeedMore;
  for(size_t i=0;i<p.length;++i) {
    r=d.push(p.bytes[i],f);
    if(r!=w::Read::NeedMore) { if(r==w::Read::Ready) assert(i+1==p.length);break; }
  }
  return r;
}
int main() {
  TestCrypto crypto;
  h::PairId pair{};pair.fill(0x42);
  h::Secret key{};key.fill(0x73);
  h::Secret wrong{};wrong.fill(0x74);
  h::LinkIds ids(0x123456789abcdef0ULL);
  h::Handshake t(Role::Taster,crypto),c(Role::Core,crypto);
  const auto p=complete(t,c,ids,pair,key);
  assert(p.hello.length==54 && p.challenge.length==86 && p.proof.length==38 && p.finish.length==38);
  assert(c.session().link==1);
  // Direction keys actually work with the existing authenticated record codec.
  SessionMac tSend(crypto,t.keys()->toCore),cReceive(crypto,c.keys()->toCore);
  SessionMac cSend(crypto,c.keys()->toTaster),tReceive(crypto,t.keys()->toTaster);
  w::Packet oldPing,oldPong;
  assert(w::seal(w::ping(t.session(),1,{7,100}),w::Direction::ToCore,tSend,oldPing));
  assert(w::seal(w::pong(c.session(),1,{7,100,200,201}),w::Direction::ToTaster,cSend,oldPong));
  w::StreamDecoder atCore(c.session(),w::Direction::ToCore,cReceive);
  w::StreamDecoder atTaster(t.session(),w::Direction::ToTaster,tReceive);
  assert(feed(atCore,oldPing)==w::Read::Ready && feed(atTaster,oldPong)==w::Read::Ready);
  w::StreamDecoder wrongDirectionKey(c.session(),w::Direction::ToCore,cSend);
  assert(feed(wrongDirectionKey,oldPing)==w::Read::Fatal);
  // Fresh reconnect derives fresh keys and rejects both directions of old traffic.
  h::Handshake t2(Role::Taster,crypto),c2(Role::Core,crypto);
  complete(t2,c2,ids,pair,key);
  assert(c2.session().link==2 && !(c2.session()==c.session()));
  assert(c2.keys()->toCore!=c.keys()->toCore && c2.keys()->toTaster!=c.keys()->toTaster);
  SessionMac newCore(crypto,c2.keys()->toCore),newTaster(crypto,t2.keys()->toTaster);
  w::StreamDecoder newAtCore(c2.session(),w::Direction::ToCore,newCore);
  w::StreamDecoder newAtTaster(t2.session(),w::Direction::ToTaster,newTaster);
  assert(feed(newAtCore,oldPing)==w::Read::Fatal && feed(newAtTaster,oldPong)==w::Read::Fatal);
  // Captured Hello/Proof cannot establish another Core connection.
  h::Handshake replayCore(Role::Core,crypto);h::Packet challenge,finish;
  assert(replayCore.acceptHello(p.hello,pair,key,ids,challenge));
  assert(!replayCore.acceptProof(p.proof,finish));noKeys(replayCore);
  // Captured challenge cannot establish a different Taster connection.
  h::Handshake replayTaster(Role::Taster,crypto);h::Packet hello,proof;
  assert(replayTaster.beginTaster(pair,hello));
  assert(!replayTaster.acceptChallenge(p.challenge,key,proof));noKeys(replayTaster);
  // Wrong pair and wrong key independently fail, with no exposed keys.
  auto otherPair=pair;otherPair[0]^=1;
  h::Handshake mismatch(Role::Core,crypto);
  assert(!mismatch.acceptHello(p.hello,otherPair,key,ids,challenge));noKeys(mismatch);
  h::Handshake wt(Role::Taster,crypto),wc(Role::Core,crypto);
  assert(wt.beginTaster(pair,hello) && wc.acceptHello(hello,pair,key,ids,challenge));
  assert(!wt.acceptChallenge(challenge,wrong,proof));noKeys(wt);noKeys(wc);
  // Mutate each byte of each authenticated handshake record. Fresh attempt
  // every time; no use of a failed object. Each mutation must fail closed.
  for(size_t i=0;i<h::ChallengeSize;++i) {
    h::Handshake a(Role::Taster,crypto),b(Role::Core,crypto);
    assert(a.beginTaster(pair,hello) && b.acceptHello(hello,pair,key,ids,challenge));
    challenge.bytes[i]^=1;
    assert(!a.acceptChallenge(challenge,key,proof));noKeys(a);
  }
  for(size_t i=0;i<h::ProofSize;++i) {
    h::Handshake a(Role::Taster,crypto),b(Role::Core,crypto);
    assert(a.beginTaster(pair,hello) && b.acceptHello(hello,pair,key,ids,challenge));
    assert(a.acceptChallenge(challenge,key,proof));proof.bytes[i]^=1;
    assert(!b.acceptProof(proof,finish));noKeys(b);
  }
  for(size_t i=0;i<h::ProofSize;++i) {
    h::Handshake a(Role::Taster,crypto),b(Role::Core,crypto);
    assert(a.beginTaster(pair,hello) && b.acceptHello(hello,pair,key,ids,challenge));
    assert(a.acceptChallenge(challenge,key,proof) && b.acceptProof(proof,finish));
    finish.bytes[i]^=1;
    assert(!a.acceptFinish(finish));noKeys(a);noKeys(b);
  }
  // Loss of Finish: Core has not committed, Taster has no keys.
  h::Handshake lt(Role::Taster,crypto),lc(Role::Core,crypto);
  assert(lt.beginTaster(pair,hello) && lc.acceptHello(hello,pair,key,ids,challenge));
  assert(lt.acceptChallenge(challenge,key,proof) && lc.acceptProof(proof,finish));
  noKeys(lt);noKeys(lc);lc.abort();lt.abort();noKeys(lt);noKeys(lc);
  assert(!lc.finishSent());
  // Wrong order, reflected proof, malformed size, provider failure.
  h::Handshake order(Role::Core,crypto);assert(!order.finishSent());noKeys(order);
  h::Handshake rt(Role::Taster,crypto),rc(Role::Core,crypto);
  assert(rt.beginTaster(pair,hello) && rc.acceptHello(hello,pair,key,ids,challenge));
  assert(rt.acceptChallenge(challenge,key,proof));
  proof.bytes[5]=static_cast<uint8_t>(h::Kind::Finish);
  assert(!rt.acceptFinish(proof));noKeys(rt);
  h::Handshake trunc(Role::Core,crypto);
  auto bad=p.hello;--bad.length;
  assert(!trunc.acceptHello(bad,pair,key,ids,challenge));noKeys(trunc);
  h::Handshake invalidId(Role::Core,crypto);h::LinkIds zero(0);
  assert(!invalidId.acceptHello(p.hello,pair,key,zero,challenge));noKeys(invalidId);
  crypto.failRandom=true;h::Handshake entropy(Role::Taster,crypto);
  assert(!entropy.beginTaster(pair,hello));noKeys(entropy);crypto.failRandom=false;
  crypto.failKdf=true;h::Handshake kdf(Role::Core,crypto);
  assert(!kdf.acceptHello(p.hello,pair,key,ids,challenge));noKeys(kdf);crypto.failKdf=false;
  crypto.failHmac=true;h::Handshake mac(Role::Core,crypto);
  assert(!mac.acceptHello(p.hello,pair,key,ids,challenge));noKeys(mac);crypto.failHmac=false;
  // New Core boot identity, real CSPRNG smoke. Previously paired key remains
  // valid, but its old traffic keys and link identity do not.
  crypto.realRandom=true;
  h::LinkIds reboot(0x123456789abcdef1ULL);
  h::Handshake bt(Role::Taster,crypto),bc(Role::Core,crypto);
  complete(bt,bc,reboot,pair,key);
  assert(bc.session().link==1 && !(bc.session()==c.session()));
  assert(bc.keys()->toCore!=c.keys()->toCore);
  c.abort();t.abort();noKeys(c);noKeys(t);
  std::cout<<"S3d mutual authentication, reconnect and record key tests PASS\n";
}
