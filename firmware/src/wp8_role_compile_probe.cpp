#if defined(PP_WP8_ROLE_COMPILE_PROBE)
// Compile-only role composition probe. Deliberately not called by setup,
// loop, app_main or any registered callback. No role activation, key
// provisioning, firmware flash or runtime test. Compile BOTH role paths
// with the actual ESP-IDF/Arduino SDK and configuration.
#include "pp/taster_link_startup_esp.h"

// The adapter must not permanently remove or redefine the lwIP sentinel.
#if !defined(INADDR_NONE)
#error "ESP adapter must preserve lwIP INADDR_NONE after Arduino headers"
#endif
static_assert(static_cast<uint32_t>(INADDR_NONE)==UINT32_MAX,
              "ESP adapter must preserve the lwIP sentinel value");

namespace pp::taster_link::compile_probe {
namespace st=startup;
// External linkage ensures the compiler checks these complete call paths.
void core(RaceControlModule& race,const Noticeboard& notice,
          const TasterPresentationPreferencesController& prefs,
          transport::EspCrypto::EntropyReady entropy,nvs_sec_cfg_t& cfg,
          const st::Approval& approval,uint16_t port,socket_runtime::Budget budget){
  auto& role=st::CoreRuntime::bootInstance(race,notice,prefs,entropy,port,budget);
  (void)role.begin(cfg,false);
  role.poll(approval);
  (void)role.linkState();
}
void taster(transport::EspCrypto::EntropyReady entropy,nvs_sec_cfg_t& cfg,
            const st::Approval& approval,uint16_t port){
  auto& role=st::TasterRuntime::bootInstance(entropy,port);
  (void)role.begin(cfg,false);
  role.poll(approval);
  (void)role.view();(void)role.selection();(void)role.linkState();
  role.observeReleased();(void)role.beginTouch();role.cancelTouch();
}
}
#endif
