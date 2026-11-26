#include "pp/taster_link_role_contract.h"

#include <cassert>
#include <iostream>

namespace rc = pp::taster_link::role_contract;

int main() {
  rc::Approval approval{};
  assert(!approval.enabled());
  approval.commissioned = true;
  assert(!approval.enabled());
  approval.targetQualified = true;
  assert(!approval.enabled());
  approval.exclusiveOwnership = true;
  assert(approval.enabled());

  // Bootstrap-idle evidence is deliberately independent of basic approval.
  approval.idleForBootstrap = false;
  assert(approval.enabled());
  approval.idleForBootstrap = true;
  assert(approval.enabled());

  static_assert(static_cast<unsigned>(rc::StartupState::Cold) == 0u);
  static_assert(static_cast<unsigned>(rc::StartupState::Waiting) == 1u);
  static_assert(static_cast<unsigned>(rc::StartupState::Operating) == 2u);
  static_assert(static_cast<unsigned>(rc::StartupState::Recovery) == 3u);
  static_assert(static_cast<unsigned>(rc::StartupState::Fault) == 4u);

  // Exactly one boot-lifetime event registration may succeed.
  assert(rc::EventSlot::claim());
  assert(!rc::EventSlot::claim());
  std::cout << "S4 shared role-contract host behaviour PASS\n";
}
