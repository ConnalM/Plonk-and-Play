#pragma once
#include "taster_link_startup_common_esp.h"
#include "taster_link_platform_core_esp.h"
#include "taster_link_core_port.h"

namespace pp::taster_link::startup {
namespace pf=platform;
namespace lc=lifecycle;
namespace pv=pairing;
namespace hs=handshake;
namespace sr=socket_runtime;
namespace tr=transport;

// Core-only boot hook.  The compile probe never calls it at runtime.
class CoreRoleEvents {
public:
  static bool install(pf::EspCoreNetwork& network){
    if(!RoleEventSlot::claim())return false;core_=&network;
    (void)WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t){if(core_)core_->noteEvent(event);});
    return true;
  }
private:
  inline static pf::EspCoreNetwork* core_=nullptr;
};

// Construct only from a future Core-role setup, after permanent Race Control,
// Noticeboard and preferences creation.  It has no renderer dependency.
class CoreRuntime final {
public:
  static CoreRuntime& bootInstance(pp::RaceControlModule& race,const pp::Noticeboard& notice,
       const pp::TasterPresentationPreferencesController& preferences,
       tr::EspCrypto::EntropyReady entropy,uint16_t port,sr::Budget budget){
    static CoreRuntime self(race,notice,preferences,entropy,port,budget);return self;
  }
  State state() const{return state_;}
  lc::CoreState linkState() const{return owner_?owner_->state():lc::CoreState::Offline;}
  bool begin(nvs_sec_cfg_t& cfg,bool keyProtectionApproved){
    if(state_!=State::Cold)return false;
    if(!port_ || !entropy_ || !CoreRoleEvents::install(network_) || !initCrypto() ||
       !store_.begin(cfg,keyProtectionApproved)){state_=State::Fault;return false;}
    policy_.prepare({},false,true,false,true,false);
    if(!usableVault(vault_.load())){state_=State::Recovery;return false;}
    state_=State::Waiting;return true;
  }
  void poll(const Approval& approval){
    if(state_==State::Cold || state_==State::Recovery || state_==State::Fault)return;
    const auto n=network_.sample();
    const bool enabled=approval.enabled() && store_.protectedReady() && budget_.permits() &&
      lc::coreNetwork(n) && entropy_ && entropy_();
    policy_.prepare(n,enabled,store_.protectedReady(),approval.targetQualified,
      approval.exclusiveOwnership && (vault_.state()==pv::State::Empty ||
        (owner_ && owner_->state()==lc::CoreState::Bootstrap)),approval.idleForBootstrap);
    if(!owner_){
      if(!enabled){state_=State::Waiting;return;}
      uint64_t epoch=0;if(!hs::newBootEpoch(crypto_,epoch)){state_=State::Fault;return;}
      ids_.emplace(epoch);
      owner_.emplace(clock_,crypto_,vault_,portBridge_,*ids_,policy_,port_,budget_);
    }
    owner_->poll(n,enabled,approval.targetQualified);
    if(owner_->state()==lc::CoreState::Recovery)state_=State::Recovery;
    else if(owner_->state()==lc::CoreState::Fault)state_=State::Fault;
    else state_=enabled?State::Operating:State::Waiting;
  }
private:
  CoreRuntime(pp::RaceControlModule& race,const pp::Noticeboard& notice,
       const pp::TasterPresentationPreferencesController& preferences,
       tr::EspCrypto::EntropyReady entropy,uint16_t port,sr::Budget budget)
    :entropy_(entropy),port_(port),budget_(budget),crypto_(entropy),
     policy_(race,network_),vault_(pv::Role::Core,store_,crypto_,policy_),
     portBridge_(race,notice,preferences){}
  CoreRuntime(const CoreRuntime&)=delete;
  tr::EspCrypto::EntropyReady entropy_;const uint16_t port_;const sr::Budget budget_;
  pf::EspCoreNetwork network_;tr::EspClock clock_;tr::EspCrypto crypto_;
  pv::EspStore store_;pf::EspCorePolicy policy_;pv::Vault vault_;
  tr::RaceControlPort portBridge_;std::optional<hs::LinkIds> ids_;
  std::optional<lc::CoreOwner> owner_;State state_=State::Cold;
};
} // namespace pp::taster_link::startup
