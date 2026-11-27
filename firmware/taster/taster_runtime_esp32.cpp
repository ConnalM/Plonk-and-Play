#if !defined(PP_S4_PRODUCTION_TASTER)
#error "taster_runtime_esp32.cpp must only be compiled as the Taster role"
#endif
#include <Arduino.h>
#include "pp/board_display_esp32s3.h"
#include "pp/system_time.h"
#include "pp/taster_remote_authority.h"
#include "pp/wp7_physical_racing_ui.h"

namespace {
bool unqualifiedEntropy() { return false; }
pp::Esp32S3BoardDisplay display;
auto& tasterLink = pp::taster_link::startup::TasterRuntime::bootInstance(unqualifiedEntropy, 0x5050);
pp::RemoteTasterAuthority authority(tasterLink);
pp::Wp7PhysicalRacingUi surface(display, authority);
pp::taster_link::startup::Approval approval{};
}

void setup() {
  Serial.begin(115200);
  display.begin();
  surface.begin();
  // No protected pairing vault, commissioning, entropy, idle or resource
  // qualification is supplied by this bench image.  Keep the link disabled.
  surface.completeStartup(false);
}

void loop() {
  tasterLink.poll(approval);
  surface.tick(pp::systemTime());
}
