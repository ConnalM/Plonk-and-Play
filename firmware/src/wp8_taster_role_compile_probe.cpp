#if defined(PP_WP8_TASTER_ROLE_COMPILE_PROBE)
// Opt-in compile-only Taster composition probe.  Never referenced by startup,
// setup, loop, app_main or a callback.
#include "pp/taster_link_startup_taster_esp.h"

namespace pp::taster_link::taster_compile_probe {
namespace st=startup;
void composition(transport::EspCrypto::EntropyReady entropy,nvs_sec_cfg_t& cfg,
                 const st::Approval& approval,uint16_t port){
  auto& role=st::TasterRuntime::bootInstance(entropy,port);
  (void)role.begin(cfg,false);role.poll(approval);
  (void)role.view();(void)role.selection();(void)role.linkState();
  role.observeReleased();(void)role.beginTouch();role.cancelTouch();
}
} // namespace pp::taster_link::taster_compile_probe
#endif
