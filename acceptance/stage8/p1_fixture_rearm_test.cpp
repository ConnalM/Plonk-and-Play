#include "pp/core.h"

// P1's Browser fixture is source-facing: it holds the simulated source active
// until the main-loop release deadline, then the ordinary detector performs
// its clear/debounce/re-arm work. This test exercises that detector boundary
// for two successive fixture passages without manufacturing bus events.
int main() {
  pp::SimulatedDetector detector(pp::InputModule::simulatedDetectorIdentity());
  pp::Time trigger = 0;

  detector.sample(false, 0, trigger);
  detector.sample(true, 1, trigger);
  if (!detector.sample(true, 20'001, trigger) || trigger != 20'001) return 1;
  if (detector.sample(true, 40'000, trigger)) return 2;

  // The P1 release guard drives the source inactive; the detector requires a
  // complete clear interval before accepting another active source period.
  detector.sample(false, 40'001, trigger);
  detector.sample(false, 60'001, trigger);
  detector.sample(true, 60'002, trigger);
  if (!detector.sample(true, 80'002, trigger) || trigger != 80'002) return 3;
  if (detector.sample(true, 100'001, trigger)) return 4;

  return 0;
}
