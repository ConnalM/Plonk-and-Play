#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/include/pp/wp7_presentation_contract.h"

int main() {
  using namespace pp;

  assert(!pausedWarningLightsEnabled());
  assert(pausedWarningPhase(0) == 0);
  assert(pausedWarningPhase(499) == 0);
  assert(pausedWarningPhase(500) == 0);
  assert(pausedWarningPhase(1000) == 0);
  assert(!pausedWarningPhaseChanged(0, 1));
  assert(!pausedWarningPhaseChanged(1, 0));

  std::cout << "STAGE8A_PAUSED_WARNING_DISABLED PASS\n";
  return 0;
}
