// Linux loopback TCP acceptance only. The production ESP socket, RNG,
// provisioning, scheduler and resource budget are separate integration work.
#include "pp/taster_link_core_port.h"
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>
#include <iostream>
#include <thread>
#include <algorithm>

namespace pp {
static Time clockUs=1000000;
Time systemTime(){return clockUs;}
}
using namespace pp;
namespace tl=pp::taster_link;
namespace tr=tl::transport;
namespace hs=tl::handshake;
namespace w=tl::wire;
namespace h=tl::health;

class Crypto final : public hs::Crypto {
public:
  bool random(uint8_t* p,size_t n) const override {return RAND_bytes(p,static_cast<int>(n))==1;}
  bool hkdf(const hs::Secret& psk,const uint8_t* salt,size_t saltSize,
            const uint8_t* info,size_t infoSize,uint8_t* out,size_t outSize) const override {
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
  bool hmac(const hs::Secret& key,const uint8_t* data,size_t n,hs::Tag& out) const override {
    unsigned size=0;return HMAC(EVP_sha256(),key.data(),static_cast<int>(key.size()),data,n,
      out.data(),&size)!=nullptr && size==out.size();
  }
  bool equal(const uint8_t* a,const uint8_t* b,size_t n) const override {return CRYPTO_memcmp(a,b,n)==0;}
  void wipe(uint8_t* p,size_t n) const override {OPENSSL_cleanse(p,n);}
};
class Clock final : public tr::ClockSource {
public:
  explicit Clock(uint64_t offset=0):offset_(offset){}
  uint64_t now() const override {return clockUs+offset_;}
private:uint64_t offset_;
};
class Socket final : public tr::Stream {
public:
  explicit Socket(int fd):fd_(fd) {
    assert(fd_>=0);
    int flags=fcntl(fd_,F_GETFL,0);assert(flags>=0 && fcntl(fd_,F_SETFL,flags|O_NONBLOCK)==0);
    int one=1;assert(setsockopt(fd_,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))==0);
  }
  ~Socket() override {close();}
  Socket(const Socket&)=delete;
  Socket& operator=(const Socket&)=delete;
  bool blockWrites=false,blockReads=false,tamperNext=false;
  unsigned reads=0,writes=0;
  int read(uint8_t* p,size_t n) override {
    ++reads;if(blockReads) return 0;
    return io(::recv(fd_,p,std::min(n,size_t(19)),MSG_DONTWAIT));
  }
  int write(const uint8_t* p,size_t n) override {
    ++writes;if(blockWrites) return 0;
    const size_t size=std::min(n,size_t(23));
    if(tamperNext) {
      uint8_t corrupt[23];std::memcpy(corrupt,p,size);corrupt[0]^=1;
      const auto count=::send(fd_,corrupt,size,MSG_DONTWAIT|MSG_NOSIGNAL);
      if(count>0) tamperNext=false;
      return io(count);
    }
    return io(::send(fd_,p,size,MSG_DONTWAIT|MSG_NOSIGNAL));
  }
  void close() override {if(fd_>=0){::close(fd_);fd_=-1;}}
private:
  static int io(ssize_t n){
    if(n>0) return static_cast<int>(n);
    if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return 0;
    return -1;
  }
  int fd_;
};
class Listener {
public:
  Listener(){
    fd_=::socket(AF_INET,SOCK_STREAM,0);assert(fd_>=0);
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(::bind(fd_,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
    assert(::listen(fd_,2)==0);
    socklen_t size=sizeof(address_);assert(getsockname(fd_,reinterpret_cast<sockaddr*>(&address_),&size)==0);
    int flags=fcntl(fd_,F_GETFL,0);assert(flags>=0 && fcntl(fd_,F_SETFL,flags|O_NONBLOCK)==0);
  }
  ~Listener(){::close(fd_);}
  // Connect only in fixture setup outside the owner poll, to a local listener.
  int connectFixture(){
    int fd=::socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
    assert(::connect(fd,reinterpret_cast<sockaddr*>(&address_),sizeof(address_))==0);
    return fd;
  }
  int admit(bool occupied){
    int fd=::accept(fd_,nullptr,nullptr);
    if(fd>=0 && occupied){::close(fd);return -1;}
    return fd;
  }
private:int fd_=-1;sockaddr_in address_{};
};
class Owner final : public StartAuthority {
public:
  PublishedControlAuthority published;
  bool authorizesStart(uint64_t id) const override {return published.authorizes(id);}
  bool hasController() const override {return published.hasController();}
  StandaloneAuthorityStamp standaloneStamp() const override {return published.standaloneStamp();}
  bool claimStandalone(uint64_t generation) const override {return published.claimStandalone(generation);}
  void connected(bool value){
    ControlLease::Snapshot s{};s.owner=7;s.connectedOwner=value?7:0;published.publish(s);
  }
};
struct Rig {
  Bus bus;
  Bus::Endpoint rc=bus.attach(Role::RaceControl),eng=bus.attach(Role::RaceEngine),
    browser=bus.attach(Role::Presentation);
  ActiveSessionDefinition active;
  Owner owner;
  RaceControlModule control{bus,rc,active};
  RaceEngineModule engine{bus,eng,active};
  Noticeboard notice{control,engine,active};
  tr::RaceControlPort port{control,notice};
  Rig(){
    for(auto t:{Type::StartRequest,Type::SetupRequest,Type::SessionOperationRequest,
      Type::PauseSettled,Type::CompetitionComplete,Type::RaceIntegrityFault}) assert(bus.subscribe(rc,t));
    for(auto t:{Type::GoScheduled,Type::SessionOperation,Type::InputEvent,
      Type::EnduranceExpired,Type::InputSettlement,Type::FinishSettled,
      Type::RaceIntegrityFault}) assert(bus.subscribe(eng,t));
    assert(bus.subscribe(browser,Type::RequestResult));
    control.setControllerStartAuthority(browser,owner);
    ProposedRaceSetup s{};
    s.activeLanes=2;s.mode=SessionMode::LapRace;s.lapTarget=10;s.durationMinutes=5;
    s.finish=LapFinishBehaviour::CompleteFullRaceDistance;
    s.entries[0].startFinish={InputIdentity(1,1),1,InputRole::StartFinish};
    s.entries[1].startFinish={InputIdentity(2,1),2,InputRole::StartFinish};
    s.entries[0].mugId=1;s.entries[1].mugId=2;
    control.setProposedRaceSetup(s);control.setRequiredCapabilityAvailable(true);
    assert(control.readiness()==StartReadiness::Ready);
  }
  void tick(){control.tick(clockUs);engine.tick();}
};
struct Link {
  Socket ts,cs;
  tr::Peer core,taster;
  Rig& rig;
  const Clock& tc;
  h::Client& client;
  Link(Listener& listener,Rig& r,const Clock& cc,const Clock& tc_,
       const Crypto& crypto,const hs::PairId& pair,const hs::Secret& key,
       const hs::Secret& tasterKey,hs::LinkIds& ids,h::Client& health)
    :ts(listener.connectFixture()),cs(listener.admit(false)),
     core(cs,cc,crypto,pair,key,r.port,ids),
     taster(ts,tc_,crypto,pair,tasterKey,health),rig(r),tc(tc_),client(health) {
    assert(core.start() && taster.start());
  }
  void step(){
    clockUs+=1000;rig.tick();
    auto pollBounded=[](tr::Peer& peer,Socket& sock) {
      const auto before=sock.reads+sock.writes;
      if(peer.alive()) peer.poll();
      assert(sock.reads+sock.writes-before<=1);
    };
    pollBounded(core,cs);pollBounded(taster,ts);
    std::this_thread::yield();
  }
  template<class Pred> void until(Pred pred,unsigned limit=2000){
    for(unsigned i=0;i<limit && !pred();++i) step();
    assert(pred());
  }
  h::View view(){return client.view(tc.now());}
  void ready(){
    until([&]{return core.established() && taster.established() &&
      view().state==h::State::Fresh && view().mayControl;});
    assert(core.session()==taster.session());
  }
  void touch(tl::Action a,tl::Mode mode=tl::Mode::None,uint16_t value=0){
    client.observeReleased(tc.now());assert(client.beginTouch(tc.now()));
    assert(taster.command(a,mode,value));
    assert(client.pending() && !view().mayControl);
  }
};
int main(){
  Listener listener;Rig rig;Crypto crypto;Clock cc,tc(1000000);
  hs::PairId pair{};hs::Secret key{},wrong{};
  assert(crypto.random(pair.data(),pair.size()) && crypto.random(key.data(),key.size()) &&
    crypto.random(wrong.data(),wrong.size()));
  hs::LinkIds ids(123); // fixed boot fixture, not a production entropy policy
  h::Client health;
  {
    Link bad(listener,rig,cc,tc,crypto,pair,key,wrong,ids,health);
    bad.until([&]{return !bad.core.alive() && !bad.taster.alive();});
    assert(!rig.control.remoteTasterContext().session.valid() && !health.connected());
  }
  tl::Session old{};
  uint64_t race=0,go=0;
  {
    Link link(listener,rig,cc,tc,crypto,pair,key,key,ids,health);link.ready();
    old=link.core.session();
    assert(link.view().snapshot.proposalLaps==10 && !link.view().readyForNewTouch);
    // A second connection is rejected without replacing the live peer.
    int extra=listener.connectFixture();assert(listener.admit(link.core.alive())==-1);::close(extra);
    assert(link.core.established() && rig.control.remoteTasterContext().session==old);
    // A command captured before a full Browser takeover/release is stale.
    link.touch(tl::Action::Setup,tl::Mode::Laps,99);
    rig.owner.connected(true);rig.owner.connected(false);
    link.until([&]{return !health.pending() && link.view().mayControl;});
    assert(rig.control.proposedRaceSetup()->lapTarget==10);
    assert(!link.view().readyForNewTouch);
    link.touch(tl::Action::Setup,tl::Mode::Laps,12);
    link.until([&]{return !health.pending();});
    assert(rig.control.proposedRaceSetup()->lapTarget==12);
    assert(link.view().awaitingPostResultSnapshot && !link.view().mayControl);
    health.observeReleased(tc.now());assert(!health.beginTouch(tc.now()));
    link.until([&]{return link.view().mayControl;});
    assert(link.view().snapshot.proposalLaps==12 && !link.view().readyForNewTouch);
    assert(!rig.bus.queued(rig.browser));
    // START executes, but its reply is deliberately lost before transmission.
    link.touch(tl::Action::Start);
    link.until([&]{return rig.control.state()==SessionLifecycle::Starting;});
    assert(health.pending());
    race=rig.control.remoteTasterContext().raceId;go=rig.control.scheduledGo();
    assert(race && go && rig.active.current() && !rig.active.current()->officialRecordEligible());
    link.taster.close();assert(link.view().uncertainOutcome && !link.view().mayControl);
    link.until([&]{return !link.core.alive();});
    assert(rig.control.state()==SessionLifecycle::Starting && rig.control.scheduledGo()==go);
  }
  {
    Link restored(listener,rig,cc,tc,crypto,pair,key,key,ids,health);restored.ready();
    assert(!(restored.core.session()==old) && restored.view().uncertainOutcome);
    assert(restored.view().snapshot.raceId==race && !restored.view().readyForNewTouch);
    // Continued periodic pings/snapshots keep the same race alive over GO.
    restored.until([&]{return restored.view().snapshot.lifecycle==w::DisplayLifecycle::Racing;},10000);
    assert(rig.control.state()==SessionLifecycle::Racing && rig.control.scheduledGo()==go);
    assert(rig.control.remoteTasterContext().raceId==race);
    rig.owner.connected(true);
    restored.until([&]{return !restored.view().mayControl && !restored.view().snapshot.tasterAllowed;});
    health.observeReleased(tc.now());assert(!health.beginTouch(tc.now()));
    rig.owner.connected(false);
    restored.until([&]{return restored.view().mayControl;});
    assert(!restored.view().readyForNewTouch);
    // Backpressure cannot leave an unbounded queue or indefinitely fresh UI.
    restored.cs.blockWrites=true;
    restored.until([&]{return !restored.core.alive() && !restored.taster.alive();});
    assert(restored.view().state==h::State::Stale && !restored.view().mayControl);
    assert(rig.control.state()==SessionLifecycle::Racing && rig.control.scheduledGo()==go);
  }
  {
    Link corrupt(listener,rig,cc,tc,crypto,pair,key,key,ids,health);corrupt.ready();
    corrupt.touch(tl::Action::Pause);
    corrupt.ts.tamperNext=true;
    corrupt.until([&]{return !corrupt.core.alive() && !corrupt.taster.alive();});
    assert(rig.control.state()==SessionLifecycle::Racing);
    assert(corrupt.view().uncertainOutcome && !corrupt.view().mayControl);
  }
  {
    // A peer that stops mid-handshake occupies the one candidate only for
    // the bounded incomplete-record deadline; it never acquires authority.
    Link partial(listener,rig,cc,tc,crypto,pair,key,key,ids,health);
    partial.step();partial.step();partial.ts.blockWrites=true;
    partial.until([&]{return !partial.core.alive() && !partial.taster.alive();});
    assert(!rig.control.remoteTasterContext().session.valid());
    assert(rig.control.state()==SessionLifecycle::Racing);
  }
  crypto.wipe(key.data(),key.size());crypto.wipe(wrong.data(),wrong.size());
  std::cout<<"S3g1 real loopback TCP, authenticated Core handlers, loss and recovery PASS\n";
}
