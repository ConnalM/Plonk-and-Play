#include <cassert>
#include <cstring>
#include <iostream>

#include "../firmware/include/pp/wp7_presentation_contract.h"

using namespace pp;

int main() {
  char value[32]{};
  formatVisibleLapCell(value, sizeof(value), SessionMode::LapRace, false, 0, 10);
  assert(std::strcmp(value, "") == 0);
  formatVisibleLapCell(value, sizeof(value), SessionMode::LapRace, true, 9, 999);
  assert(std::strcmp(value, "9/999") == 0);
  formatVisibleLapCell(value, sizeof(value), SessionMode::LapRace, true, 99, 100);
  assert(std::strcmp(value, "99/100") == 0);
  formatVisibleLapCell(value, sizeof(value), SessionMode::Endurance, false, 0, 10);
  assert(std::strcmp(value, "") == 0);
  formatVisibleLapCell(value, sizeof(value), SessionMode::Endurance, true, 4, 10);
  assert(std::strcmp(value, "4") == 0);

  // Operational readiness, not elapsed time or startability, controls the
  // transition from the welcome screen to the local READY setup surface.
  assert(!welcomeCanLeave(false, true, true));
  assert(!welcomeCanLeave(true, false, true));
  assert(!welcomeCanLeave(true, true, false));
  assert(welcomeCanLeave(true, true, true));
  assert(!welcomeCanLeaveAfterStartup(false, true, true, true));
  assert(welcomeCanLeaveAfterStartup(true, true, true, true));
  assert(!welcomeCanLeaveAfterMinimum(true, true, true, true, false,
                                      100, 4000));
  assert(!welcomeCanLeaveAfterMinimum(true, true, true, true, true,
                                      100, 3099));
  assert(welcomeCanLeaveAfterMinimum(true, true, true, true, true,
                                     100, 3100));
  // A slow bootstrap may keep the splash up beyond the minimum, but it may
  // leave only after the real local controller readiness gate is true.
  assert(!welcomeCanLeaveAfterMinimum(false, true, true, true, true,
                                      100, 10000));
  assert(welcomeCanLeaveAfterMinimum(true, true, true, true, true,
                                     100, 10000));
  assert(std::strcmp(welcomeCustomerStatus(false), "STARTING P&P...") == 0);
  assert(std::strcmp(welcomeCustomerStatus(true), "STARTUP FAILED") == 0);

  assert(!startButtonVisualChanged(false, false, true, true));
  // A pending setup acknowledgement may change controller state without
  // changing the latched button appearance.
  assert(!startButtonVisualChanged(false, false, true, true));
  assert(startButtonVisualChanged(false, false, false, true));
  assert(startButtonVisualChanged(true, false, true, true));

  // A pending -> accepted acknowledgement of the same locally displayed
  // READY value is not a new visible setup state.
  assert(visibleSetupMode(SessionLifecycle::Ready, SessionMode::LapRace,
                          SessionMode::Endurance) == SessionMode::Endurance);
  assert(visibleSetupValue(SessionLifecycle::Ready, SessionMode::LapRace,
                           10, 10, 27) == 27);
  assert(visibleSetupMode(SessionLifecycle::Ready, SessionMode::Endurance,
                          SessionMode::Endurance) == SessionMode::Endurance);
  assert(visibleSetupValue(SessionLifecycle::Ready, SessionMode::Endurance,
                           10, 10, 27) == 27);
  // An authoritative change that is actually displayed is still visible.
  assert(visibleSetupValue(SessionLifecycle::Ready, SessionMode::LapRace,
                           28, 10, 28) !=
         visibleSetupValue(SessionLifecycle::Ready, SessionMode::LapRace,
                           27, 10, 27));
  // Active screens always use authoritative session values, not stale READY
  // selections.
  assert(visibleSetupMode(SessionLifecycle::Racing, SessionMode::LapRace,
                          SessionMode::Endurance) == SessionMode::LapRace);
  assert(visibleSetupValue(SessionLifecycle::Racing, SessionMode::LapRace,
                           5, 12, 99) == 5);
  assert(visibleSetupValue(SessionLifecycle::Racing, SessionMode::Endurance,
                           5, 12, 99) == 12);

  assert(kRacingSignalRed == 0xF800U);
  assert(kRacingSignalGreen == 0x07E0U);
  assert(racingSignalLampColor(false, 2, 0) == kRacingSignalRed);
  assert(racingSignalLampColor(false, 2, 2) == kRacingSignalOff);
  assert(racingSignalLampColor(true, 5, 4) == kRacingSignalGreen);
  assert(!pauseHoldFeedbackActive(SessionLifecycle::Racing, true, 699U));
  assert(pauseHoldFeedbackActive(SessionLifecycle::Racing, true, 700U));
  assert(!pauseHoldFeedbackActive(SessionLifecycle::Starting, true, 700U));
  assert(!pauseHoldFeedbackActive(SessionLifecycle::Racing, false, 700U));

  std::cout << "WP8_PRESENTATION_CONTRACT_PASS\n";
  return 0;
}
