#include "pp/taster_link_pairing.h"
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "pp/taster_link_crypto_primitives.h"
#include <cassert>
#include <iostream>

namespace p=pp::taster_link::pairing;
namespace h=pp::taster_link::handshake;
namespace w=pp::taster_link::wire;
class Crypto final:public h::Crypto {
public:
  enum class Random {Good,Zero,Fail};
  Random mode=Random::Good;
  mutable unsigned calls=0,wipes=0;
  bool random(uint8_t* out,size_t n) const override {
    ++calls;
    if(mode==Random::Fail){std::memset(out,0xBB,n);return false;}
    if(mode==Random::Zero){std::memset(out,0,n);return true;}
    return RAND_bytes(out,static_cast<int>(n))==1;
  }
  static bool raw(const uint8_t* k,size_t kn,const uint8_t* b,size_t bn,uint8_t* out){
    unsigned n=0;return HMAC(EVP_sha256(),k,static_cast<int>(kn),b,bn,out,&n)!=nullptr && n==32;
  }
  bool hkdf(const h::Secret& key,const uint8_t* salt,size_t sn,const uint8_t* info,size_t in,
            uint8_t* out,size_t on) const override {
    return h::ppthHkdf32(key,salt,sn,info,in,out,on,&raw,[](uint8_t* b,size_t n){OPENSSL_cleanse(b,n);});
  }
  bool hmac(const h::Secret& key,const uint8_t* b,size_t n,h::Tag& out) const override {
    return raw(key.data(),key.size(),b,n,out.data());
  }
  bool equal(const uint8_t* a,const uint8_t* b,size_t n) const override {return CRYPTO_memcmp(a,b,n)==0;}
  void wipe(uint8_t* b,size_t n) const override {++wipes;OPENSSL_cleanse(b,n);}
};
class Store final:public p::Store {
public:
  enum class Fault {None,Read,BeforeWrite,AfterWrite,ReadbackMismatch};
  Fault mode=Fault::None;
  bool present=false,secure=true,afterWrite=false;
  unsigned writes=0,reads=0;
  p::Record stored{};
  bool protectedReady() const override{return secure;}
  p::Read read(p::Record& out) override {
    ++reads;
    if(mode==Fault::Read) return p::Read::Error;
    if(!present) return p::Read::Missing;
    out=stored;
    if(mode==Fault::ReadbackMismatch && afterWrite) out[20]^=1;
    return p::Read::Found;
  }
  bool replace(const p::Record& record) override {
    ++writes;
    if(mode==Fault::BeforeWrite) return false;
    stored=record;present=true;afterWrite=true;
    return mode!=Fault::AfterWrite;
  }
};
class Local final:public p::Maintenance {
public:
  bool authority=false,safe=true,quiet=true;
  bool locallyAuthorised() const override{return authority;}
  bool safeLifecycle() const override{return safe;}
  bool transportQuiescent() const override{return quiet;}
};
static bool authenticates(const p::Material& core,const p::Material& taster,const Crypto& crypto,h::LinkIds& ids) {
  using R=h::Handshake::Role;
  h::Handshake c(R::Core,crypto),t(R::Taster,crypto);
  h::Packet hello,challenge,proof,finish;
  return t.beginTaster(taster.pair(),hello) &&
    c.acceptHello(hello,core.pair(),core.psk(),ids,challenge) &&
    t.acceptChallenge(challenge,taster.psk(),proof) &&
    c.acceptProof(proof,finish) && c.finishSent() && t.acceptFinish(finish) &&
    c.session()==t.session() && c.keys()->toCore==t.keys()->toCore;
}
int main(){
  Crypto crypto;Local owner;Store cs,ts;
  p::Material first(crypto),replacement(crypto),bad(crypto);
  crypto.mode=Crypto::Random::Zero;assert(!bad.generate() && !bad.valid());
  crypto.mode=Crypto::Random::Fail;assert(!bad.generate() && !bad.valid());
  crypto.mode=Crypto::Random::Good;
  assert(first.generate() && replacement.generate() && first.pair()!=replacement.pair() &&
         first.psk()!=replacement.psk());
  h::Secret zero{};assert(!bad.import(first.pair(),zero) && !bad.valid());
  p::Vault core(p::Role::Core,cs,crypto,owner),taster(p::Role::Taster,ts,crypto,owner);
  assert(core.load()==p::State::Empty && taster.load()==p::State::Empty);
  assert(!core.active() && !core.revision() && !cs.writes);
  assert(core.install(0,first)==p::Result::Denied);
  owner.authority=true;owner.safe=false;
  assert(core.install(0,first)==p::Result::Denied);
  owner.safe=true;owner.quiet=false;
  assert(core.install(0,first)==p::Result::Denied);
  owner.quiet=true;
  assert(core.install(1,first)==p::Result::Stale);
  assert(core.install(0,bad)==p::Result::Invalid);
  assert(!cs.writes);
  assert(core.install(0,first)==p::Result::Applied && taster.install(0,first)==p::Result::Applied);
  assert(core.revision()==1 && core.active() && taster.active());
  assert(cs.stored!=ts.stored); // role-specific persistent record
  h::LinkIds ids(12);
  assert(authenticates(*core.active(),*taster.active(),crypto,ids));
  assert(core.install(1,replacement)==p::Result::WrongState);
  owner.quiet=false;assert(core.revoke(1)==p::Result::Denied);
  owner.quiet=true;owner.safe=false;assert(core.revoke(1)==p::Result::Denied);
  owner.safe=true;assert(core.revoke(0)==p::Result::Stale);
  assert(cs.writes==1);
  const auto old=cs.stored;
  assert(core.revoke(1)==p::Result::Applied && !core.active() && core.revision()==2);
  assert(core.state()==p::State::Revoked);
  for(size_t i=12;i<60;++i) assert(cs.stored[i]==0);
  // Boot in the gap between revocation and replacement is unpaired, not
  // recovered from an older slot or a factory default.
  p::Vault gap(p::Role::Core,cs,crypto,owner);
  assert(gap.load()==p::State::Revoked && !gap.active() && gap.revision()==2);
  assert(core.install(2,replacement)==p::Result::Applied && core.revision()==3);
  assert(!authenticates(*core.active(),*taster.active(),crypto,ids));
  assert(taster.revoke(1)==p::Result::Applied && taster.install(2,replacement)==p::Result::Applied);
  assert(authenticates(*core.active(),*taster.active(),crypto,ids));
  p::Vault reboot(p::Role::Core,cs,crypto,owner);
  assert(reboot.load()==p::State::Paired && reboot.revision()==3 &&
         authenticates(*reboot.active(),*taster.active(),crypto,ids));
  // Wrong role, corrupt bytes, inaccessible storage and insecure backend
  // never fall through to Empty or automatically erase/rewrite.
  Store broken;broken.present=true;broken.stored=cs.stored;
  p::Vault wrongRole(p::Role::Taster,broken,crypto,owner);
  assert(wrongRole.load()==p::State::Fault && !wrongRole.active() && !broken.writes);
  broken.stored[42]^=1;
  p::Vault corrupt(p::Role::Core,broken,crypto,owner);
  assert(corrupt.load()==p::State::Fault && !corrupt.active() && !broken.writes);
  assert(corrupt.install(0,replacement)==p::Result::WrongState);
  broken.stored=cs.stored;broken.mode=Store::Fault::Read;
  p::Vault unreadable(p::Role::Core,broken,crypto,owner);
  assert(unreadable.load()==p::State::Fault && !unreadable.active());
  broken.mode=Store::Fault::None;broken.secure=false;
  p::Vault insecure(p::Role::Core,broken,crypto,owner);
  assert(insecure.load()==p::State::Fault && !insecure.active());
  broken.secure=true;
  assert(insecure.load()==p::State::Fault); // latched, not retry
  owner.quiet=false;
  p::Vault notQuiet(p::Role::Core,broken,crypto,owner);
  assert(notQuiet.load()==p::State::Fault);
  owner.quiet=true;
  // Failed revoke: no active RAM credential, no automatic fallback, but a
  // power-cycle may still find old committed flash if failure preceded write.
  for(auto mode:{Store::Fault::BeforeWrite,Store::Fault::AfterWrite,Store::Fault::ReadbackMismatch}) {
    Store f;f.present=true;f.stored=old;
    p::Vault v(p::Role::Core,f,crypto,owner);
    assert(v.load()==p::State::Paired);
    f.mode=mode;
    assert(v.revoke(1)==p::Result::StorageFault && !v.active() && v.state()==p::State::Fault);
    assert(v.install(0,replacement)==p::Result::WrongState);
    f.mode=Store::Fault::None;
    p::Vault recovery(p::Role::Core,f,crypto,owner);
    assert(recovery.load()==(mode==Store::Fault::BeforeWrite?p::State::Paired:p::State::Revoked));
  }
  // Failed installation after verified revoke cannot restore the old pair.
  for(auto mode:{Store::Fault::BeforeWrite,Store::Fault::AfterWrite,Store::Fault::ReadbackMismatch}) {
    Store f;f.present=true;f.stored=old;
    p::Vault v(p::Role::Core,f,crypto,owner);
    assert(v.load()==p::State::Paired && v.revoke(1)==p::Result::Applied);
    f.mode=mode;
    assert(v.install(2,replacement)==p::Result::StorageFault && !v.active());
    f.mode=Store::Fault::None;
    p::Vault recovery(p::Role::Core,f,crypto,owner);
    const auto state=recovery.load();
    assert(state==(mode==Store::Fault::BeforeWrite?p::State::Revoked:p::State::Paired));
    if(recovery.active()) assert(recovery.active()->pair()==replacement.pair());
  }
  Store max;max.present=true;
  p::encode(max.stored,p::Role::Core,UINT32_MAX,&first);
  p::Vault exhausted(p::Role::Core,max,crypto,owner);
  assert(exhausted.load()==p::State::Paired && exhausted.revoke(UINT32_MAX)==p::Result::Invalid && !max.writes);
  std::cout<<"S3g2b pairing vault, revocation, replacement and fault isolation PASS\n";
}
