// Source/host acceptance of the shared socket owner and PPTH KDF helper.
// Uses Linux TCP and OpenSSL, not an ESP build or hardware timing evidence.
#include "pp/taster_link_socket_runtime.h"
#include "pp/taster_link_crypto_primitives.h"
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <cassert>
#include <iostream>
#include <thread>

namespace tl=pp::taster_link;
namespace tr=tl::transport;
namespace sr=tl::socket_runtime;
namespace hs=tl::handshake;
namespace w=tl::wire;
namespace h=tl::health;

static bool raw(const uint8_t* key,size_t keySize,const uint8_t* data,size_t n,uint8_t* out){
  unsigned size=0;
  return HMAC(EVP_sha256(),key,static_cast<int>(keySize),data,n,out,&size)!=nullptr && size==32;
}
static void wipe(uint8_t* p,size_t n){OPENSSL_cleanse(p,n);}
class Crypto final : public hs::Crypto {
public:
  enum class Entropy {Good,Zero,Fail};
  Entropy entropy=Entropy::Good;
  bool random(uint8_t* p,size_t n) const override {
    if(entropy==Entropy::Fail) return false;
    if(entropy==Entropy::Zero) {std::memset(p,0,n);return true;}
    return RAND_bytes(p,static_cast<int>(n))==1;
  }
  bool hkdf(const hs::Secret& key,const uint8_t* salt,size_t saltSize,
            const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize) const override {
    return hs::ppthHkdf32(key,salt,saltSize,info,infoSize,out,outSize,&raw,&::wipe);
  }
  bool hmac(const hs::Secret& key,const uint8_t* data,size_t n,hs::Tag& out) const override {
    return raw(key.data(),key.size(),data,n,out.data());
  }
  bool equal(const uint8_t* a,const uint8_t* b,size_t n) const override {return CRYPTO_memcmp(a,b,n)==0;}
  void wipe(uint8_t* p,size_t n) const override {::wipe(p,n);}
};
static void kdfTest(Crypto& crypto) {
  hs::Secret key{};key.fill(0x73);
  std::array<uint8_t,hs::TranscriptSize> salt{};
  for(size_t i=0;i<salt.size();++i) salt[i]=static_cast<uint8_t>(i);
  const char* labels[]={"P&P PPTH v1 core challenge","P&P PPTH v1 taster proof",
    "P&P PPTH v1 core finish","P&P PPTL v1 Taster to Core","P&P PPTL v1 Core to Taster"};
  for(const auto* label:labels) {
    const auto* info=reinterpret_cast<const uint8_t*>(label);const auto size=std::strlen(label);
    hs::Secret actual{},reference{};
    assert(crypto.hkdf(key,salt.data(),salt.size(),info,size,actual.data(),actual.size()));
    auto* ctx=EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF,nullptr);assert(ctx);
    size_t n=reference.size();
    assert(EVP_PKEY_derive_init(ctx)>0 &&
      EVP_PKEY_CTX_hkdf_mode(ctx,EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND)>0 &&
      EVP_PKEY_CTX_set_hkdf_md(ctx,EVP_sha256())>0 &&
      EVP_PKEY_CTX_set1_hkdf_salt(ctx,salt.data(),static_cast<int>(salt.size()))>0 &&
      EVP_PKEY_CTX_set1_hkdf_key(ctx,key.data(),static_cast<int>(key.size()))>0 &&
      EVP_PKEY_CTX_add1_hkdf_info(ctx,info,static_cast<int>(size))>0 &&
      EVP_PKEY_derive(ctx,reference.data(),&n)>0 && n==reference.size());
    EVP_PKEY_CTX_free(ctx);assert(actual==reference);
    // Failure of either HMAC operation must never leave derived material.
    for(unsigned failAt:{1u,2u}) {
      unsigned calls=0;actual.fill(0xAA);
      auto injected=[&](const uint8_t* k,size_t kn,const uint8_t* b,size_t bn,uint8_t* out){
        if(++calls==failAt) {std::memset(out,0xBB,32);return false;}
        return raw(k,kn,b,bn,out);
      };
      assert(!hs::ppthHkdf32(key,salt.data(),salt.size(),info,size,actual.data(),actual.size(),injected,&wipe));
      assert(!hs::nonzero(actual));
    }
    actual.fill(0xAA);
    assert(!crypto.hkdf(key,salt.data(),salt.size()-1,info,size,actual.data(),actual.size()));
    assert(!hs::nonzero(actual));
  }
  uint64_t epoch=1;
  crypto.entropy=Crypto::Entropy::Fail;assert(!hs::newBootEpoch(crypto,epoch) && !epoch);
  crypto.entropy=Crypto::Entropy::Zero;assert(!hs::newBootEpoch(crypto,epoch) && !epoch);
  crypto.entropy=Crypto::Entropy::Good;assert(hs::newBootEpoch(crypto,epoch) && epoch);
}
struct Clock final:tr::ClockSource {
  uint64_t value=1000000;
  uint64_t now() const override {return value;}
};
// Deliberately small CorePort fixture for socket lifetime assertions.
// Actual sporting handler/noticeboard integration is covered by S3g1 CI.
class Port final:public tr::CorePort {
public:
  tl::CommandGate gate;
  tl::Context context{};
  unsigned opens=0,closes=0,executions=0;
  uint32_t laps=10;
  Clock& clock;
  explicit Port(Clock& c):clock(c){}
  bool open(tl::Session s) override {
    if(context.session.valid() || !gate.open(s)) return false;
    context={s,1,1,0,1,tl::Lifecycle::Ready,true};++opens;return true;
  }
  void close() override {assert(context.session.valid());gate.disconnect();context={};++closes;}
  tl::Status submit(const tl::Command& q,uint64_t now) override {return gate.submit(q,context,now);}
  bool execute() override {
    tl::Command q{};
    if(!gate.take(context,clock.now(),q)) return false;
    ++executions;
    if(q.action==tl::Action::Setup && q.mode==tl::Mode::Laps) {
      laps=q.value;++context.proposalRevision;
      gate.complete(q.session,q.id,true);
    } else gate.complete(q.session,q.id,false);
    return true;
  }
  tl::Status result(tl::Session s,uint64_t id) const override {return gate.result(s,id);}
  bool sample(uint64_t now,w::DisplaySnapshot& out) const override {
    out={};out.session=context.session;out.authorityGeneration=context.authorityGeneration;
    out.lifecycleRevision=context.lifecycleRevision;out.proposalRevision=context.proposalRevision;
    out.coreTimeUs=now;out.lifecycle=w::DisplayLifecycle::Ready;
    out.proposalMode=w::DisplayMode::Laps;out.tasterAllowed=true;
    out.proposalLaps=laps;out.proposedLanes=2;out.readiness=w::Readiness::Ready;
    return w::validDisplay(out);
  }
};
static uint16_t fixturePort(uint32_t ip){
  int fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(fd>=0);
  auto a=sr::address(ip,0);
  assert(::bind(fd,reinterpret_cast<const sockaddr*>(&a),sizeof(a))==0);
  socklen_t n=sizeof(a);assert(::getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n)==0);
  const auto port=ntohs(a.sin_port);sr::closeFd(fd);return port;
}
int main(){
  Crypto crypto;kdfTest(crypto);
  assert((!sr::Budget{10,10}.permits() && !sr::Budget{12,10}.permits() &&
    !sr::Budget{100,9}.permits() && !sr::Budget{13,14}.permits()));
  assert((sr::Budget{13,10}.permits() && sr::Budget{16,13}.permits()));
  sr::Retry retry;
  const uint64_t initial=100;
  for(const uint64_t delay:{500000ULL,1000000ULL,2000000ULL,4000000ULL,8000000ULL,8000000ULL}) {
    retry.failure(initial);assert(retry.waitUs()==delay);
    assert(!retry.due(initial+delay-1) && retry.due(initial+delay));
  }
  retry.reset();assert(retry.due(initial) && !retry.waitUs());
  assert(!sr::usableAddress(0) && !sr::usableAddress(htonl(0xE0000001U)));
  const uint32_t ip=htonl(INADDR_LOOPBACK);
  const auto tcpPort=fixturePort(ip);
  Clock clock;Port port(clock);h::Client health;
  hs::PairId pair{};hs::Secret key{};
  assert(crypto.random(pair.data(),pair.size()) && crypto.random(key.data(),key.size()));
  uint64_t epoch=0;assert(hs::newBootEpoch(crypto,epoch));hs::LinkIds ids(epoch);
  sr::Network cn{true,1,ip,ip},tn=cn;
  {
    sr::CoreService insufficient(clock,crypto,pair,key,port,ids,tcpPort,{10,10});
    insufficient.poll(cn,true,true);assert(!insufficient.listening() && !port.opens);
  }
  sr::CoreService core(clock,crypto,pair,key,port,ids,tcpPort,{13,10});
  sr::TasterService taster(clock,crypto,pair,key,health,tcpPort);
  core.poll(cn,false,true);assert(!core.listening());
  core.poll(cn,true,false);assert(!core.listening());
  taster.poll(tn,false);assert(!taster.attempts());
  // Failed connect with no listener; no busy reconnect.
  auto step=[&](bool approve=true) {
    clock.value+=1000;core.poll(cn,true,approve);taster.poll(tn,true);
    std::this_thread::yield();
  };
  auto until=[&](auto pred,bool approve=true,unsigned limit=20000) {
    for(unsigned i=0;i<limit && !pred();++i) step(approve);
    assert(pred());
  };
  until([&]{return taster.retryWaitUs()!=0;},false);
  const auto attempts=taster.attempts();assert(attempts==1);
  for(unsigned i=0;i<200;++i) step(false);
  assert(taster.attempts()==attempts && taster.retryWaitUs()==sr::Retry::FirstUs);
  auto fresh=[&]{return core.established() && taster.established() &&
    health.view(clock.now()).state==h::State::Fresh && health.view(clock.now()).mayControl;};
  until(fresh);
  assert(core.session()==taster.session() && core.session().epoch==epoch && port.opens==1);
  assert(!taster.retryWaitUs());
  const auto first=core.session();
  // Reject an extra accepted socket without evicting the established peer.
  int extra=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(extra>=0);
  auto dest=sr::address(ip,tcpPort);
  assert(::connect(extra,reinterpret_cast<const sockaddr*>(&dest),sizeof(dest))==0);
  until([&]{return core.refused()==1;});
  sr::closeFd(extra);
  assert(core.session()==first && port.opens==1 && !port.closes);
  // A real authenticated command through the new concrete socket owners.
  health.observeReleased(clock.now());assert(health.beginTouch(clock.now()));
  assert(taster.command(tl::Action::Setup,tl::Mode::Laps,12));
  until([&]{return port.laps==12 && fresh() && !health.pending();});
  assert(port.executions==1 && health.view(clock.now()).snapshot.proposalLaps==12);
  // Same address, new network epoch: retire the Taster socket immediately.
  ++tn.generation;step();
  assert(!taster.established() && !health.view(clock.now()).mayControl);
  assert(taster.retryWaitUs()==sr::Retry::FirstUs);
  until(fresh);
  const auto second=core.session();assert(!(second==first) && second.link>first.link);
  assert(port.opens==2 && port.closes==1 && port.executions==1);
  assert(!health.view(clock.now()).readyForNewTouch);
  // Capacity approval withdrawal closes listener and peer; no command may
  // be replayed when approval is subsequently restored.
  core.poll(cn,true,false);assert(!core.listening() && !core.established());
  until([&]{return !taster.established();},false);
  assert(!health.view(clock.now()).mayControl);
  until(fresh);
  assert(core.session().link>second.link && port.executions==1);
  // Core AP loss must close both accepted and listening descriptors.
  cn.ready=false;++cn.generation;step();
  assert(!core.listening() && !core.established() && !port.context.session.valid());
  tn.ready=false;++tn.generation;step();
  const auto stopped=taster.attempts();for(unsigned i=0;i<500;++i) step();
  assert(taster.attempts()==stopped && !health.view(clock.now()).mayControl);
  cn.ready=tn.ready=true;++cn.generation;++tn.generation;
  until(fresh);
  assert(port.executions==1 && port.laps==12);
  // Clock regression fails closed even if network approval remains true.
  --clock.value;core.poll(cn,true,true);taster.poll(tn,true);
  assert(!core.listening() && !taster.established() && !health.view(clock.now()).mayControl);
  clock.value+=10000000;core.poll(cn,true,true);taster.poll(tn,true);
  assert(!core.listening() && !taster.established());
  crypto.wipe(key.data(),key.size());
  std::cout<<"S3g2a socket owner lifecycle, budget, retry and PPTH KDF PASS\n";
}
