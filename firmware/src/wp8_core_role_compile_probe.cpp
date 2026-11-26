#if defined(PP_WP8_CORE_ROLE_COMPILE_PROBE)
// Opt-in compile-only Core composition probe.  Never referenced by startup,
// setup, loop, app_main or a callback.
#include "pp/taster_link_startup_core_esp.h"

#if !defined(INADDR_NONE)
#error "Core ESP adapter must preserve lwIP INADDR_NONE after Arduino headers"
#endif
static_assert(static_cast<uint32_t>(INADDR_NONE)==UINT32_MAX,
              "Core ESP adapter must preserve the lwIP sentinel value");

namespace pp::taster_link::core_compile_probe {
namespace st=startup;
void composition(RaceControlModule& race,const Noticeboard& notice,
                 const TasterPresentationPreferencesController& prefs,
                 transport::EspCrypto::EntropyReady entropy,nvs_sec_cfg_t& cfg,
                 const st::Approval& approval,uint16_t port,socket_runtime::Budget budget){
  auto& role=st::CoreRuntime::bootInstance(race,notice,prefs,entropy,port,budget);
  (void)role.begin(cfg,false);role.poll(approval);(void)role.linkState();
}
} // namespace pp::taster_link::core_compile_probe
#endif
