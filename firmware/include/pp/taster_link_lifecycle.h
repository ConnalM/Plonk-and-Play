#pragma once
#include "taster_link_discovery.h"
#include "taster_link_mbed_tls.h"

namespace pp::taster_link::lifecycle {
namespace sr=socket_runtime;
namespace tr=transport;
namespace bs=bootstrap;
namespace pv=pairing;
namespace ds=discovery;
namespace hs=handshake;

// This header coordinates the reviewed components, not the production
// scheduler. One owner thread per device. Boot-load the vault before use;
// no independent mutation while this owner exists. Platform policy must
// enforce actual race-idle, network and resource conditions. TLS, storage
// and radio calls are NOT measured active-race CPU/latency bounds.
class CorePolicy {
public:
  virtual ~CorePolicy()=default;
  // True only while safe to reserve/continue first use, including after
  // this attempt commits. No active race, no other credential borrower,
  // no competing candidate, actual AP interface and maintenance authority.
  virtual bool bootstrapSafe() const=0;
  // Validate admitted socket's actual AP path (not just an IP heuristic).
  // Revalidate the reserved path through bootstrapSafe throughout use.
  virtual bool bootstrapPeer(int fd,const sr::Network&) const=0;
};
inline bool coreNetwork(const sr::Network& n){
  return n.ready && n.generation && n.local==n.core && sr::usableAddress(n.local);
}
inline bool tasterNetwork(const sr::Network& n){
  return n.ready && n.generation && sr::usableAddress(n.local) &&
    sr::usableAddress(n.core) && n.local!=n.core;
}
inline int openListener(const sr::Network& network,uint16_t port){
  int fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(fd<0)return -1;
  const auto a=sr::address(network.local,port);const int one=1;
  if(!sr::nonblocking(fd) || !sr::bindInterface(fd,network) || ::setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one))!=0 ||
     ::bind(fd,reinterpret_cast<const sockaddr*>(&a),sizeof(a))!=0 || ::listen(fd,1)!=0)
    sr::closeFd(fd);
  return fd;
}
enum class CoreState:uint8_t {Offline,Available,Bootstrap,Paired,Recovery,Fault};
class CoreOwner final:private bs::Admission {
public:
  CoreOwner(const tr::ClockSource& clock,const hs::Crypto& crypto,pv::Vault& vault,
            tr::CorePort& port,hs::LinkIds& ids,const CorePolicy& policy,
            uint16_t tcpPort,sr::Budget budget)
    :clock_(clock),crypto_(crypto),vault_(vault),port_(port),ids_(ids),
     policy_(policy),tcpPort_(tcpPort),budget_(budget){}
  ~CoreOwner(){stop();}
  CoreOwner(const CoreOwner&)=delete;
  CoreOwner& operator=(const CoreOwner&)=delete;
  CoreState state() const{return state_;}
  bool listening() const{return listener_>=0 || (paired_ && paired_->listening());}
  bool established() const{return paired_ && paired_->established();}
  Session session() const{return established()?paired_->session():Session{};}
  unsigned refused() const{return refused_;}
  void stop(){
    enabled_=false;
    abandonBootstrap();
    paired_.reset();
    if(!terminal())state_=CoreState::Offline;
  }
  // Ordinary Core race/control/engine work MUST run first. Resource and
  // commissioning approval are separate from first-use safety. Paired
  // service continues during races; empty service does not.
  void poll(const sr::Network& network,bool commissioned,bool resourcesApproved){
    const auto now=clock_.now();
    if(seen_ && now<last_){stop();state_=CoreState::Fault;return;}
    seen_=true;last_=now;
    if(terminal()){stop();return;}
    if(!network.same(network_)){stop();network_=network;}
    if(terminal())return;
    enabled_=commissioned && resourcesApproved && budget_.permits() &&
      tcpPort_ && coreNetwork(network_) && vault_.role()==pv::Role::Core;
    if(!enabled_){stop();return;}
    if(exchange_ || channel_){
      if(!permitted()){abandonBootstrap();if(!terminal())state_=CoreState::Offline;return;}
      if(exchange_){
        exchange_->poll(now);
        if(exchange_->outcome()==bs::Outcome::Running)return;
        const auto outcome=exchange_->outcome();
        clearBootstrap();
        if(outcome==bs::Outcome::Complete && vault_.active()){
          retry_.reset();state_=CoreState::Paired;
        }else if(outcome==bs::Outcome::Uncertain || vault_.state()!=pv::State::Empty)
          state_=CoreState::Recovery;
        else {retry_.failure(now);state_=CoreState::Offline;}
        return; // next poll may open paired listener, never overlap
      }
      const auto result=channel_->pollHandshake(now);
      if(result==bs::TlsState::Secure){
        exchange_.emplace(pv::Role::Core,vault_,crypto_,*channel_,admission());
        if(!exchange_->start(now)){abandonBootstrap();retry_.failure(now);}
      }else if(result!=bs::TlsState::Negotiating){abandonBootstrap();retry_.failure(now);}
      return;
    }
    if(vault_.state()==pv::State::Paired && vault_.active()){
      sr::closeFd(listener_);
      if(!paired_){
        const auto& m=*vault_.active();
        paired_.emplace(clock_,crypto_,m.pair(),m.psk(),port_,ids_,tcpPort_,budget_);
      }
      state_=CoreState::Paired;paired_->poll(network_,true,true);return;
    }
    paired_.reset();
    if(vault_.state()!=pv::State::Empty){
      sr::closeFd(listener_);state_=CoreState::Recovery;return;
    }
    if(!policy_.bootstrapSafe()){sr::closeFd(listener_);state_=CoreState::Offline;return;}
    if(!retry_.due(now))return;
    if(listener_<0){
      listener_=openListener(network_,tcpPort_);
      if(listener_<0){retry_.failure(now);return;}
      state_=CoreState::Available;
    }
    if(acceptSeen_ && now-lastAccept_<100000)return;
    acceptSeen_=true;lastAccept_=now;
    int fd=::accept(listener_,nullptr,nullptr);
    if(fd<0)return;
    if(!policy_.bootstrapSafe() || !policy_.bootstrapPeer(fd,network_)){
      sr::closeFd(fd);if(refused_!=UINT32_MAX)++refused_;retry_.failure(now);return;
    }
    // Close the bootstrap listener while the single candidate owns the
    // reservation. Kernel backlog is discarded; no simultaneous paired
    // listener or second TLS candidate.
    sr::closeFd(listener_);reserved_=true;state_=CoreState::Bootstrap;
    channel_.emplace(crypto_,admission());
    if(!channel_->start(fd,pv::Role::Core,now)){abandonBootstrap();retry_.failure(now);}
  }
private:
  const bs::Admission& admission() const{return *this;}
  bool permitted() const override{
    return enabled_ && reserved_ && !paired_ && policy_.bootstrapSafe();
  }
  bool terminal() const{return state_==CoreState::Recovery || state_==CoreState::Fault;}
  void clearBootstrap(){
    exchange_.reset();channel_.reset();reserved_=false;sr::closeFd(listener_);
  }
  void abandonBootstrap(){
    const bool attempted=exchange_.has_value();
    if(exchange_)exchange_->abort();
    const bool uncertain=attempted && (exchange_->outcome()==bs::Outcome::Uncertain ||
      vault_.state()!=pv::State::Empty);
    clearBootstrap();
    if(uncertain)state_=CoreState::Recovery;
    else if(!terminal())state_=CoreState::Offline;
  }
  const tr::ClockSource& clock_;const hs::Crypto& crypto_;pv::Vault& vault_;
  tr::CorePort& port_;hs::LinkIds& ids_;const CorePolicy& policy_;
  const uint16_t tcpPort_;const sr::Budget budget_;
  sr::Network network_{};sr::Retry retry_{};
  int listener_=-1;std::optional<sr::CoreService> paired_;
  std::optional<bs::MbedFirstUseChannel> channel_;std::optional<bs::Exchange> exchange_;
  CoreState state_=CoreState::Offline;uint64_t last_=0,lastAccept_=0;
  bool enabled_=false,reserved_=false,seen_=false,acceptSeen_=false;unsigned refused_=0;
};

// Radio boundary. Implementation owns the exclusive Taster STA, current
// scan tokens, asynchronous driver events and live association epoch.
// beginScan/pollScan feed the SAME selector; no invented candidates.
// stop must cancel stale scan/association, release Wi-Fi work and prevent
// automatic driver reconnect; false means recovery is necessary, not a
// promise of later quiescence. Concrete ESP owner integration remains open.
class Radio {
public:
  virtual ~Radio()=default;
  virtual bool beginScan(ds::Selector&,uint64_t)=0;
  virtual bool pollScan(ds::Selector&,uint64_t)=0;
  virtual bool associate(const ds::Attempt&)=0;
  virtual sr::Network associated(const ds::Attempt&) const=0;
  virtual bool stop()=0;
};

// Single nonblocking TCP dial for bootstrap. Ordinary PPTH uses the
// previously reviewed TasterService. No fd is shared with either channel.
class Dial {
public:
  enum class State:uint8_t {Idle,Waiting,Connected,Failed};
  ~Dial(){close();}
  Dial()=default;Dial(const Dial&)=delete;Dial& operator=(const Dial&)=delete;
  void close(){sr::closeFd(fd_);state_=State::Idle;}
  State start(const sr::Network& n,uint16_t port,uint64_t now){
    close();since_=now;
    if(!tasterNetwork(n) || !port)return fail();
    fd_=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(fd_<0 || !sr::nonblocking(fd_) || !sr::bindInterface(fd_,n))return fail();
    const auto local=sr::address(n.local,0),remote=sr::address(n.core,port);
    if(::bind(fd_,reinterpret_cast<const sockaddr*>(&local),sizeof(local))!=0)return fail();
    if(::connect(fd_,reinterpret_cast<const sockaddr*>(&remote),sizeof(remote))==0)
      return state_=State::Connected;
    if(errno!=EINPROGRESS && errno!=EALREADY && !sr::wouldBlock(errno))return fail();
    return state_=State::Waiting;
  }
  State poll(uint64_t now){
    if(state_!=State::Waiting)return state_;
    if(now<since_ || now-since_>=2000000 || fd_<0 || fd_>=FD_SETSIZE)return fail();
    fd_set w,e;FD_ZERO(&w);FD_ZERO(&e);FD_SET(fd_,&w);FD_SET(fd_,&e);timeval zero{};
    const int rc=::select(fd_+1,nullptr,&w,&e,&zero);
    if(rc<0)return sr::wouldBlock(errno)?state_:fail();
    if(!rc)return state_;
    int error=0;socklen_t n=sizeof(error);
    if(::getsockopt(fd_,SOL_SOCKET,SO_ERROR,&error,&n)!=0 || error ||
       !FD_ISSET(fd_,&w) || FD_ISSET(fd_,&e))return fail();
    return state_=State::Connected;
  }
  int take(){
    if(state_!=State::Connected)return -1;
    const int fd=fd_;fd_=-1;state_=State::Idle;return fd;
  }
private:
  State fail(){sr::closeFd(fd_);return state_=State::Failed;}
  int fd_=-1;uint64_t since_=0;State state_=State::Idle;
};

enum class TasterState:uint8_t {
  Unloaded,Searching,Choose,Waiting,Associating,Bootstrap,Authenticating,Connected,Recovery,Fault
};
class TasterOwner final:private bs::Admission {
public:
  static constexpr uint64_t AssociationUs=10000000,InitialAuthUs=15000000;
  TasterOwner(const tr::ClockSource& clock,const hs::Crypto& crypto,pv::Vault& vault,
              ds::HintStore& hints,Radio& radio,uint16_t port)
    :clock_(clock),crypto_(crypto),vault_(vault),selector_(hints),radio_(radio),port_(port){}
  ~TasterOwner(){retire();(void)radio_.stop();}
  TasterOwner(const TasterOwner&)=delete;
  TasterOwner& operator=(const TasterOwner&)=delete;
  TasterState state() const{return state_;}
  const ds::Selector& selection() const{return selector_;}
  // Only call after boot load of vault and platform initialisation.
  bool start(){
    if(state_!=TasterState::Unloaded)return false;
    if(!port_ || vault_.role()!=pv::Role::Taster){state_=TasterState::Fault;return false;}
    const auto c=vault_.state()==pv::State::Empty?ds::Credential::Empty:
      vault_.active()?ds::Credential::Paired:ds::Credential::Fault;
    if(selector_.load(c,vault_.active()?vault_.active()->pair():hs::PairId{})!=ds::State::Idle){
      state_=TasterState::Recovery;return false;
    }
    if(!radio_.stop()){state_=TasterState::Fault;return false;}
    state_=TasterState::Waiting;return true;
  }
  bool choose(uint64_t token,const ds::Bssid& b){
    if(state_!=TasterState::Choose || !enabled_)return false;
    const auto now=clock_.now();
    if(seen_ && now<last_){fault();return false;}
    return selector_.choose(token,b,now);
  }
  void poll(bool commissioned){
    const auto now=clock_.now();
    if(seen_ && now<last_){fault();return;}
    seen_=true;last_=now;
    if(terminal() || state_==TasterState::Unloaded)return;
    enabled_=commissioned;
    if(!enabled_){
      if(state_!=TasterState::Waiting)retry(now);
      return;
    }
    if(state_==TasterState::Waiting){
      if(!backoff_.due(now))return;
      if(!radio_.beginScan(selector_,now)){fault();return;}
      state_=TasterState::Searching;return;
    }
    if(state_==TasterState::Searching || state_==TasterState::Choose){
      if(state_==TasterState::Searching && !radio_.pollScan(selector_,now)){fault();return;}
      selector_.tick(now);
      switch(selector_.state()){
        case ds::State::Scanning:return;
        case ds::State::Choose:state_=TasterState::Choose;return;
        case ds::State::Connecting:
          attempt_=selector_.attempt();attemptSince_=now;initialComplete_=false;
          if(!radio_.associate(attempt_)){retry(now);return;}
          state_=TasterState::Associating;return;
        case ds::State::NoneFound:case ds::State::WaitingRemembered:
        case ds::State::Incomplete:case ds::State::Retry:retry(now);return;
        default:fault();return;
      }
    }
    if(state_==TasterState::Associating){
      if(now-attemptSince_>=AssociationUs){retry(now);return;}
      const auto n=radio_.associated(attempt_);
      if(!tasterNetwork(n))return;
      network_=n;
      if(attempt_.intent==ds::Intent::Remembered){
        if(!vault_.active() || vault_.active()->pair()!=attempt_.expectedPair){recover();return;}
        startPaired(now);return;
      }
      if(vault_.state()!=pv::State::Empty){recover();return;}
      state_=TasterState::Bootstrap;reserved_=true;
      if(dial_.start(network_,port_,now)==Dial::State::Failed)retry(now);
      return;
    }
    if(!radio_.associated(attempt_).same(network_)){retry(now);return;}
    if(state_==TasterState::Bootstrap){
      if(exchange_){
        exchange_->poll(now);
        if(exchange_->outcome()==bs::Outcome::Running)return;
        const auto outcome=exchange_->outcome();
        clearBootstrap();
        if(outcome==bs::Outcome::Complete && vault_.active()){
          initialComplete_=true;startPaired(now);
        }else if(outcome==bs::Outcome::Uncertain || vault_.state()!=pv::State::Empty)recover();
        else retry(now);
        return;
      }
      if(channel_){
        const auto result=channel_->pollHandshake(now);
        if(result==bs::TlsState::Secure){
          exchange_.emplace(pv::Role::Taster,vault_,crypto_,*channel_,admission());
          if(!exchange_->start(now))retry(now);
        }else if(result!=bs::TlsState::Negotiating)retry(now);
        return;
      }
      const auto result=dial_.poll(now);
      if(result==Dial::State::Failed){retry(now);return;}
      if(result==Dial::State::Connected){
        channel_.emplace(crypto_,admission());
        if(!channel_->start(dial_.take(),pv::Role::Taster,now))retry(now);
      }
      return;
    }
    if(state_==TasterState::Authenticating || state_==TasterState::Connected){
      if(!paired_ || !vault_.active()){recover();return;}
      paired_->poll(network_,true);
      if(state_==TasterState::Authenticating){
        if(now-authSince_>=InitialAuthUs){retry(now);return;}
        const auto v=health_.view(clock_.now());
        if(paired_->established() && v.state==health::State::Fresh){
          // Real PPTH and a fresh snapshot, not a supplied pair-ID event.
          if(!radio_.associated(attempt_).same(network_) ||
             !selector_.authenticated(attempt_.token,attempt_.candidate.bssid,vault_.active()->pair())){
            recover();return;
          }
          state_=TasterState::Connected;backoff_.reset();health_.cancelTouch();
        }
      }else if(!paired_->established() || health_.view(clock_.now()).state!=health::State::Fresh)
        retry(now);
    }
  }
  // Rendering and input must use these gates, never a raw cached snapshot
  // or direct access to TasterService. Caller still enforces physical
  // release, 700ms hold and confirmation. No command replay on reconnect.
  health::View view(){
    if(state_==TasterState::Connected && enabled_ &&
       radio_.associated(attempt_).same(network_) && paired_ && paired_->established())
      return health_.view(clock_.now());
    auto v=health_.view(clock_.now());
    v.state=v.hasSnapshot?health::State::Stale:health::State::Offline;
    v.mayControl=v.readyForNewTouch=v.gestureActive=false;
    v.snapshot.tasterAllowed=false;v.snapshot.redLit=0;v.snapshot.phase=wire::Phase::None;
    v.snapshot.flags&=static_cast<uint16_t>(~wire::Green);return v;
  }
  void observeReleased(){if(view().mayControl)health_.observeReleased(clock_.now());}
  bool beginTouch(){return view().mayControl && health_.beginTouch(clock_.now());}
  void cancelTouch(){health_.cancelTouch();}
  bool command(Action a,Mode m=Mode::None,uint16_t value=0,bool confirmed=false){
    if(!view().mayControl || !paired_){health_.cancelTouch();return false;}
    return paired_->command(a,m,value,confirmed);
  }
private:
  const bs::Admission& admission() const{return *this;}
  bool permitted() const override{
    return enabled_ && reserved_ && !paired_ && state_==TasterState::Bootstrap &&
      radio_.associated(attempt_).same(network_);
  }
  bool terminal() const{return state_==TasterState::Recovery || state_==TasterState::Fault;}
  void startPaired(uint64_t now){
    const auto* m=vault_.active();
    if(!m){recover();return;}
    paired_.emplace(clock_,crypto_,m->pair(),m->psk(),health_,port_);
    authSince_=now;state_=TasterState::Authenticating;
  }
  void clearBootstrap(){
    exchange_.reset();channel_.reset();dial_.close();reserved_=false;
  }
  void retire(){
    if(exchange_)exchange_->abort();
    clearBootstrap();paired_.reset();health_.disconnect();
  }
  void recover(){retire();(void)radio_.stop();state_=TasterState::Recovery;}
  void fault(){retire();(void)radio_.stop();state_=TasterState::Fault;}
  void retry(uint64_t now){
    const bool uncertain=(exchange_ && exchange_->outcome()==bs::Outcome::Uncertain) ||
      (attempt_.valid() && attempt_.intent==ds::Intent::FirstUse &&
       selector_.state()!=ds::State::Connected &&
       (vault_.state()!=pv::State::Empty || initialComplete_));
    retire();
    if(uncertain){recover();return;}
    if(selector_.state()==ds::State::Connected)selector_.disconnected();
    else if(selector_.state()==ds::State::Connecting)selector_.failed(selector_.attempt().token,true);
    else if(selector_.state()==ds::State::Scanning)
      selector_.finishScan(selector_.scanToken(),false,now);
    else if(selector_.state()==ds::State::Choose)
      selector_.cancelChoice();
    if(!radio_.stop()){state_=TasterState::Fault;return;}
    attempt_={};network_={};backoff_.failure(now);state_=TasterState::Waiting;
  }
  const tr::ClockSource& clock_;const hs::Crypto& crypto_;pv::Vault& vault_;
  ds::Selector selector_;Radio& radio_;const uint16_t port_;
  health::Client health_;sr::Retry backoff_;Dial dial_;
  std::optional<bs::MbedFirstUseChannel> channel_;std::optional<bs::Exchange> exchange_;
  std::optional<sr::TasterService> paired_;
  ds::Attempt attempt_{};sr::Network network_{};
  TasterState state_=TasterState::Unloaded;uint64_t last_=0,attemptSince_=0,authSince_=0;
  bool enabled_=false,reserved_=false,seen_=false,initialComplete_=false;
};

} // namespace pp::taster_link::lifecycle
