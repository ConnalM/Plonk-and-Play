#pragma once

// Board-neutral operating contract shared by the future Core and Taster role
// entry points.  It deliberately creates no Arduino/ESP-IDF service and owns
// no sporting, presentation, network or storage implementation.

#include <cstdint>

namespace pp::taster_link::role_contract {

// Trusted local boot code supplies these facts.  A network request can never
// manufacture commissioning, target qualification or bootstrap safety.
struct Approval {
  bool commissioned = false;
  bool targetQualified = false;
  bool exclusiveOwnership = false;
  bool idleForBootstrap = false;
  bool enabled() const { return commissioned && targetQualified && exclusiveOwnership; }
};

enum class StartupState : uint8_t { Cold, Waiting, Operating, Recovery, Fault };

// One role composition may register its Wi-Fi event observer for a boot. The
// process-lifetime guard is shared operating policy, not a radio implementation.
class EventSlot {
 public:
  static bool claim() {
    if (installed_) return false;
    installed_ = true;
    return true;
  }

 private:
  inline static bool installed_ = false;
};

}  // namespace pp::taster_link::role_contract
