#pragma once
#include "taster_link_startup_common_esp.h"
#include "taster_link_platform_taster_esp.h"

namespace pp::taster_link::startup {
namespace pf=platform;
namespace lc=lifecycle;
namespace pv=pairing;
namespace tr=transport;
namespace ds=discovery;

// Taster-only boot hook.  The compile probe never calls it at runtime.
class TasterRoleEvents {
public:
  static bool install(pf::EspRadio& radio){
    if(!RoleEventSlot::claim()) return false;
    taster_=&radio;
    (void)WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t){if(taster_)taster_->noteEvent(event);});
    return true;
  }
private:
  inline static pf::EspRadio* taster_=nullptr;
};

// Taster composition has no RaceControl, detector, Browser or record service.
// Renderer integration receives only the gated link surface from this owner;
// it retains physical touch/hold and confirmation and synthesises nothing.
class TasterRuntime final {
public:
  static TasterRuntime& bootInstance(tr::EspCrypto::EntropyReady entropy,uint16_t port){
    static TasterRuntime self(entropy,port);return self;
  }
  State state() const{return state_;}
  lc::TasterState linkState() const{return owner_?owner_->state():lc::TasterState::Unloaded;}
  bool begin(nvs_sec_cfg_t& cfg,bool keyProtectionApproved){
    if(state_!=State::Cold)return false;
    if(!port_ || !entropy_ || WiFi.getMode()!=WIFI_OFF || !TasterRoleEvents::install(radio_) ||
       !initCrypto() || !store_.begin(cfg,keyProtectionApproved)){
      state_=State::Fault;return false;
    }
    maintenance_.update(false,true,false,true);
    if(!usableVault(vault_.load())){state_=State::Recovery;return false;}
    owner_.emplace(clock_,crypto_,vault_,hints_,radio_,port_);
    if(!owner_->start()){state_=State::Recovery;return false;}
    state_=State::Waiting;return true;
  }
  void poll(const Approval& approval){
    if(!owner_ || state_==State::Cold || state_==State::Recovery || state_==State::Fault)return;
    const bool enabled=approval.enabled() && store_.protectedReady();
    // Do not require RF entropy before enabling scans: radio is initially off.
    // EspCrypto requires live entropy before and after random. Mutation is
    // allowed only for initial bootstrap in exclusive STA.
    maintenance_.update(enabled,store_.protectedReady(),
      approval.idleForBootstrap && owner_->state()==lc::TasterState::Bootstrap,
      approval.exclusiveOwnership && owner_->state()==lc::TasterState::Bootstrap &&
      vault_.state()==pv::State::Empty);
    owner_->poll(enabled);
    if(owner_->state()==lc::TasterState::Recovery)state_=State::Recovery;
    else if(owner_->state()==lc::TasterState::Fault)state_=State::Fault;
    else state_=enabled?State::Operating:State::Waiting;
  }
  health::View view(){return owner_?owner_->view():health::View{};}
  const ds::Selector* selection() const{return owner_?&owner_->selection():nullptr;}
  bool choose(uint64_t token,const ds::Bssid& b){return owner_ && owner_->choose(token,b);}
  void observeReleased(){if(owner_)owner_->observeReleased();}
  bool beginTouch(){return owner_ && owner_->beginTouch();}
  void cancelTouch(){if(owner_)owner_->cancelTouch();}
  bool command(Action a,Mode m=Mode::None,uint16_t value=0,bool confirmed=false){
    return owner_ && owner_->command(a,m,value,confirmed);
  }
private:
  TasterRuntime(tr::EspCrypto::EntropyReady entropy,uint16_t port)
    :entropy_(entropy),port_(port),crypto_(entropy),
     vault_(pv::Role::Taster,store_,crypto_,maintenance_),hints_(store_){}
  TasterRuntime(const TasterRuntime&)=delete;
  tr::EspCrypto::EntropyReady entropy_;const uint16_t port_;
  pf::EspRadio radio_;tr::EspClock clock_;tr::EspCrypto crypto_;
  pv::EspStore store_;pf::LocalMaintenance maintenance_;pv::Vault vault_;
  ds::EspHintStore hints_;std::optional<lc::TasterOwner> owner_;
  State state_=State::Cold;
};
} // namespace pp::taster_link::startup
