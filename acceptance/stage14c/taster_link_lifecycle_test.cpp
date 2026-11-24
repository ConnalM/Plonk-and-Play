// Real Mbed TLS and PPTH TCP owners; simulated discovery/association,
// fake protected stores and small CorePort fixture. No ESP/hardware evidence.
#include "pp/taster_link_lifecycle.h"
#include "pp/taster_link_crypto_primitives.h"
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <cassert>
#include <iostream>
#include <thread>
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif

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

namespace lc=tl::lifecycle;
namespace p=tl::pairing;
namespace d=tl::discovery;
namespace b=tl::bootstrap;
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

class Store final:public p::Store {
public:
  bool present=false;unsigned writes=0;p::Record record{};
  bool protectedReady() const override{return true;}
  p::Read read(p::Record& out) override{
    if(!present)return p::Read::Missing;
    out=record;return p::Read::Found;
  }
  bool replace(const p::Record& in) override{
    record=in;present=true;++writes;return true;
  }
};
struct Owner final:p::Maintenance,b::Admission{
  bool allowed=true;
  bool locallyAuthorised() const override{return allowed;}
  bool safeLifecycle() const override{return allowed;}
  bool transportQuiescent() const override{return allowed;}
  bool permitted() const override{return allowed;}
};

class HintStore final:public d::HintStore{
public:
  bool present=false,fail=false;unsigned writes=0;d::HintRecord record{};
  d::Read read(d::HintRecord& out) override{
    if(!present)return d::Read::Missing;
    out=record;return d::Read::Found;
  }
  bool replace(const d::HintRecord& in) override{
    ++writes;if(fail)return false;
    record=in;present=true;return true;
  }
};
class CorePolicy final:public lc::CorePolicy{
public:
  bool safe=true,allowPeer=true;
  bool bootstrapSafe() const override{return safe;}
  bool bootstrapPeer(int fd,const sr::Network&) const override{
    return allowPeer && fd>=0;
  }
};
class Radio final:public lc::Radio{
public:
  d::Candidate a{{2,1,2,3,4,5},6,-60},other{{2,1,2,3,4,6},6,-10};
  bool visible=true,multiple=false,online=true,quiet=true,scanning=false;
  unsigned scans=0,associations=0,stops=0;
  uint64_t token=0,generation=1,selected=0;
  d::Bssid selectedBssid{};
  const uint32_t core=htonl(0x7f000001),local=htonl(0x7f000002);
  bool beginScan(d::Selector& s,uint64_t now) override{
    assert(!scanning && !selected);++scans;
    token=s.beginScan(now);scanning=token!=0;return scanning;
  }
  bool pollScan(d::Selector& s,uint64_t now) override{
    if(!scanning)return false;
    if(visible)assert(s.observe(token,a));
    if(multiple)assert(s.observe(token,other));
    s.finishScan(token,true,now);scanning=false;return true;
  }
  bool associate(const d::Attempt& attempt) override{
    assert(!scanning && !selected);
    ++associations;selected=attempt.token;selectedBssid=attempt.candidate.bssid;++generation;
    return true;
  }
  sr::Network associated(const d::Attempt& attempt) const override{
    if(!online || !selected || selected!=attempt.token ||
       selectedBssid!=attempt.candidate.bssid || selectedBssid!=a.bssid)return {};
    return {true,generation,local,core};
  }
  bool stop() override{
    ++stops;scanning=false;selected=0;selectedBssid={};return quiet;
  }
};
struct Rig{
  Crypto crypto;Clock clock;Port port{clock};hs::LinkIds ids{77};
  Owner co,to;Store cs,ts;HintStore hints;Radio radio;CorePolicy policy;
  p::Vault cv{p::Role::Core,cs,crypto,co},tv{p::Role::Taster,ts,crypto,to};
  sr::Network cn{true,1,radio.core,radio.core};
  uint16_t tcp=fixturePort(radio.core);
  lc::CoreOwner core{clock,crypto,cv,port,ids,policy,tcp,{13,10}};
  std::optional<lc::TasterOwner> taster;
  bool commissioned=true,resources=true;
  Rig(){
    assert(cv.load()==p::State::Empty && tv.load()==p::State::Empty);
    taster.emplace(clock,crypto,tv,hints,radio,tcp);
    assert(taster->start());
  }
  void step(){
    clock.value+=1000;core.poll(cn,commissioned,resources);taster->poll(commissioned);
    std::this_thread::yield();
  }
  template<class F>void until(F f,unsigned max=35000){
    for(unsigned i=0;i<max && !f();++i)step();
    assert(f());
  }
  bool fresh(){
    return core.established() && taster->state()==lc::TasterState::Connected &&
      taster->view().state==h::State::Fresh && taster->view().mayControl;
  }
};
int main(){
#if defined(MBEDTLS_USE_PSA_CRYPTO)
  assert(psa_crypto_init()==PSA_SUCCESS);
#endif
  using T=lc::TasterState;using C=lc::CoreState;
  {
    Rig r;r.resources=false;
    for(unsigned i=0;i<500;++i)r.step();
    assert(!r.core.listening() && !r.port.opens && !r.taster->view().mayControl);
    r.resources=true;
    r.until([&]{return r.fresh();});
    assert(r.cs.writes==1 && r.ts.writes==1 && r.hints.writes==1 &&
      r.cv.active()->pair()==r.tv.active()->pair() && r.port.opens==1);
    assert(!r.taster->view().readyForNewTouch);
    const auto first=r.core.session();
    r.taster->observeReleased();assert(r.taster->beginTouch());
    assert(r.taster->command(tl::Action::Setup,tl::Mode::Laps,12));
    r.until([&]{return r.port.laps==12 && r.fresh() &&
      r.taster->view().snapshot.proposalLaps==12;});
    assert(r.port.executions==1);
    // A further raw connection must never evict the admitted paired peer.
    int extra=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(extra>=0);
    auto addr=sr::address(r.radio.core,r.tcp);
    assert(::connect(extra,reinterpret_cast<const sockaddr*>(&addr),sizeof(addr))==0);
    for(unsigned i=0;i<300;++i)r.step();
    sr::closeFd(extra);
    assert(r.core.session()==first && r.port.opens==1 && r.port.executions==1);
    // Losing STA immediately suppresses input before the next poll.
    r.radio.online=false;
    assert(!r.taster->view().mayControl && !r.taster->command(tl::Action::Start));
    r.step();assert(r.taster->state()==T::Waiting);
    r.radio.visible=false;r.radio.multiple=true;
    for(unsigned i=0;i<1500;++i)r.step();
    assert(r.radio.associations==1 && !r.taster->view().mayControl);
    r.radio.online=r.radio.visible=true;
    r.until([&]{return r.fresh();});
    assert(r.core.session().link>first.link && r.port.executions==1 &&
      r.hints.writes==1 && r.cs.writes==1 && r.ts.writes==1);
    assert(!r.taster->view().readyForNewTouch);
    // Paired link remains available while bootstrap readiness is false.
    r.policy.safe=false;
    for(unsigned i=0;i<100;++i)r.step();
    assert(r.fresh());
    // Recreate the Taster owner with the persisted stores and paired vault.
    // Core may still hold old socket until EOF; no duplicate/replayed command.
    r.taster.reset();
    p::Vault reloaded(p::Role::Taster,r.ts,r.crypto,r.to);
    assert(reloaded.load()==p::State::Paired);
    r.taster.emplace(r.clock,r.crypto,reloaded,r.hints,r.radio,r.tcp);
    assert(r.taster->start());
    r.until([&]{return r.fresh();});
    assert(r.cs.writes==1 && r.ts.writes==1 && r.hints.writes==1 && r.port.executions==1);
    r.taster.reset(); // release borrowed reloaded vault before destruction
  }
  {
    Rig r;r.radio.multiple=true;r.until([&]{return r.taster->state()==T::Choose;});
    assert(!r.radio.associations && !r.taster->view().mayControl);
    assert(!r.taster->choose(r.taster->selection().scanToken()-1,r.radio.a.bssid));
    assert(r.taster->choose(r.taster->selection().scanToken(),r.radio.a.bssid));
    // Disable after choice, before association. No stale token may survive.
    r.commissioned=false;r.step();assert(r.taster->state()==T::Waiting);
    r.commissioned=true;r.until([&]{return r.taster->state()==T::Choose;});
    assert(r.taster->choose(r.taster->selection().scanToken(),r.radio.a.bssid));
    r.until([&]{return r.fresh();});
    assert(r.radio.associations==1 && r.hints.writes==1);
  }
  {
    Rig r;r.policy.safe=false;
    for(unsigned i=0;i<200;++i)r.step();
    assert(!r.core.listening() && !r.port.opens && !r.cs.writes);
    r.policy.safe=true;
    r.until([&]{return r.tv.state()==p::State::Paired && r.cv.state()==p::State::Empty;});
    r.policy.safe=false;r.radio.online=false;r.step();
    assert(r.taster->state()==T::Recovery && !r.taster->view().mayControl &&
      !r.port.opens && !r.cs.writes && r.ts.writes==1);
  }
  {
    Rig r;r.hints.fail=true;
    r.until([&]{return r.taster->state()==T::Recovery;});
    assert(r.cs.writes==1 && r.ts.writes==1 && r.hints.writes==1 &&
      !r.taster->view().mayControl && !r.port.executions);
  }
  {
    Rig r;r.until([&]{return r.fresh();});
    --r.clock.value;r.core.poll(r.cn,true,true);r.taster->poll(true);
    assert(r.core.state()==C::Fault && r.taster->state()==T::Fault &&
      !r.core.listening() && !r.taster->view().mayControl);
  }
  std::cout<<"S3g2g Core/Taster first-use and remembered lifecycle over real TCP PASS\n";
}
