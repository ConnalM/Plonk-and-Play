#pragma once
#if !defined(ESP_PLATFORM) || !defined(ARDUINO)
#error "Role startup requires ESP-IDF/Arduino"
#endif
#include "taster_link_platform_esp.h"
#include "taster_link_esp_crypto.h"
#include "taster_link_core_port.h"
#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif

namespace pp::taster_link::startup {
namespace pf=platform;
namespace lc=lifecycle;
namespace pv=pairing;
namespace hs=handshake;
namespace sr=socket_runtime;
namespace tr=transport;
namespace ds=discovery;

// Only trusted local boot/owner code supplies these facts, NEVER network
// requests. Default-deny. Target qualification and protected device-key
// provisioning are prerequisites, not manufactured by this composition.
struct Approval {
  bool commissioned=false;
  bool targetQualified=false; // actual SDK, entropy, interface, timing/resources
  bool exclusiveOwnership=false;
  bool idleForBootstrap=false; // Core race/engine/storage idle; Taster exclusive STA
  bool enabled() const{return commissioned && targetQualified && exclusiveOwnership;}
};
enum class State:uint8_t {Cold,Waiting,Operating,Recovery,Fault};
inline bool initCrypto(){
#if defined(MBEDTLS_USE_PSA_CRYPTO)
  return psa_crypto_init()==PSA_SUCCESS;
#else
  return true;
#endif
}
inline bool usableVault(pv::State s){return s==pv::State::Empty || s==pv::State::Paired;}

// Select exactly one role per boot. The callback target has boot lifetime;
// no callback unregister/destruction or role switching at runtime. The
// owner must retain the singleton and referenced Core objects for the
// whole boot. Call install before any Wi-Fi start. No driver operations in
// the callback itself. Existing integrated firmware never calls this.
class RoleEvents {
public:
  static bool install(pf::EspCoreNetwork& network){
    if(installed_)return false;
    core_=&network;return registerHook();
  }
  static bool install(pf::EspRadio& radio){
    if(installed_)return false;
    taster_=&radio;return registerHook();
  }
private:
  static bool registerHook(){
    // Arduino's event callback registration occurs on the boot owner
    // before radio startup. Target must verify registration succeeds and
    // all events are delivered. No callback target is ever freed.
    (void)WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t){
      if(core_)core_->noteEvent(event);
      if(taster_)taster_->noteEvent(event);
    });
    installed_=true;return true;
  }
  inline static bool installed_=false;
  inline static pf::EspCoreNetwork* core_=nullptr;
  inline static pf::EspRadio* taster_=nullptr;
};

// Core startup integration. Construct only from the future Core role's
// setup, after permanent Race Control/Noticeboard/preferences creation.
// Existing Browser AP/STA manager remains the radio owner. The role entry
// point invokes begin before Browser Wi-Fi, then poll after its ordinary
// race/control/input/engine/storage/Browser work. No renderer dependency.
class CoreRuntime final {
public:
  static CoreRuntime& bootInstance(pp::RaceControlModule& race,const pp::Noticeboard& notice,
       const pp::TasterPresentationPreferencesController& preferences,
       tr::EspCrypto::EntropyReady entropy,uint16_t port,sr::Budget budget){
    static CoreRuntime self(race,notice,preferences,entropy,port,budget);
    return self;
  }
  State state() const{return state_;}
  lc::CoreState linkState() const{return owner_?owner_->state():lc::CoreState::Offline;}
  // cfg is caller-owned protected, device-specific material; caller wipes
  // it immediately after return. No erase/key generation/default NVS.
  bool begin(nvs_sec_cfg_t& cfg,bool keyProtectionApproved){
    if(state_!=State::Cold)return false;
    if(!port_ || !entropy_ || !RoleEvents::install(network_) ||
       !initCrypto() || !store_.begin(cfg,keyProtectionApproved)){
      state_=State::Fault;return false;
    }
    policy_.prepare({},false,true,false,true,false);
    if(!usableVault(vault_.load())){state_=State::Recovery;return false;}
    state_=State::Waiting;return true;
  }
  void poll(const Approval& approval){
    if(state_==State::Cold || state_==State::Recovery || state_==State::Fault)return;
    const auto n=network_.sample();
    const bool enabled=approval.enabled() && store_.protectedReady() && budget_.permits() &&
      lc::coreNetwork(n) && entropy_ && entropy_();
    // This runtime is sole owner of the vault. Empty bootstrap may borrow
    // only its own exchange; paired credential references exclude mutation.
    policy_.prepare(n,enabled,store_.protectedReady(),approval.targetQualified,
      approval.exclusiveOwnership && vault_.state()==pv::State::Empty,
      approval.idleForBootstrap);
    // A single PPTH allocator for the boot, never regenerate on reconnect.
    if(!owner_){
      if(!enabled){state_=State::Waiting;return;}
      uint64_t epoch=0;
      if(!hs::newBootEpoch(crypto_,epoch)){state_=State::Fault;return;}
      ids_.emplace(epoch);
      owner_.emplace(clock_,crypto_,vault_,portBridge_,*ids_,policy_,port_,budget_);
    }
    // Keep the same boot owner/allocator on withdrawal or AP change.
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
  CoreRuntime& operator=(const CoreRuntime&)=delete;
  tr::EspCrypto::EntropyReady entropy_;const uint16_t port_;const sr::Budget budget_;
  pf::EspCoreNetwork network_;tr::EspClock clock_;tr::EspCrypto crypto_;
  pv::EspStore store_;pf::EspCorePolicy policy_;pv::Vault vault_;
  tr::RaceControlPort portBridge_;
  std::optional<hs::LinkIds> ids_;std::optional<lc::CoreOwner> owner_;
  State state_=State::Cold;
};

// Taster role startup. Contains no RaceControlModule, InputModule, engine,
// Browser, History or records instance. Renderer gets only the gated link
// surface. The later renderer still owns actual physical touch/hold and
// confirmation; this runtime does not synthesise them.
class TasterRuntime final {
public:
  static TasterRuntime& bootInstance(tr::EspCrypto::EntropyReady entropy,uint16_t port){
    static TasterRuntime self(entropy,port);return self;
  }
  State state() const{return state_;}
  lc::TasterState linkState() const{return owner_?owner_->state():lc::TasterState::Unloaded;}
  bool begin(nvs_sec_cfg_t& cfg,bool keyProtectionApproved){
    if(state_!=State::Cold)return false;
    if(!port_ || !entropy_ || WiFi.getMode()!=WIFI_OFF ||
       !RoleEvents::install(radio_) || !initCrypto() ||
       !store_.begin(cfg,keyProtectionApproved)){
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
    // Do not require RF entropy before enabling scans: radio is initially
    // off. EspCrypto itself requires live entropy before and after random.
    // Mutation is allowed only for initial bootstrap in exclusive STA.
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
  TasterRuntime& operator=(const TasterRuntime&)=delete;
  tr::EspCrypto::EntropyReady entropy_;const uint16_t port_;
  pf::EspRadio radio_;tr::EspClock clock_;tr::EspCrypto crypto_;
  pv::EspStore store_;pf::LocalMaintenance maintenance_;pv::Vault vault_;
  ds::EspHintStore hints_;std::optional<lc::TasterOwner> owner_;
  State state_=State::Cold;
};
} // namespace pp::taster_link::startup
