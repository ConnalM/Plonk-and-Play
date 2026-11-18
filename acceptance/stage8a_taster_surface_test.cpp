#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "../../firmware/include/pp/system_time.h"
#include "../../firmware/include/pp/taster_surface.h"
#include "../../firmware/include/pp/wp7_presentation_contract.h"

using namespace pp;

class FakeAuthority final : public TasterAuthority {
public:
  TasterSnapshot value{};
  uint32_t pending = 0;
  bool pendingSetup = false;
  uint32_t lastStart = 0;
  SessionOperation lastOperation = SessionOperation::Pause;
  uint32_t lastSetup = 0;
  LapFinishBehaviour lastFinish = LapFinishBehaviour::Immediate;
  uint32_t nextResult = 0;
  RequestResult resultValue = RequestResult::Accepted;
  RequestRejection resultReason = RequestRejection::None;

  TasterSnapshot snapshot() const override { return value; }
  bool submitSetup(uint32_t correlation, SessionMode mode, uint32_t laps,
                   uint16_t minutes, LapFinishBehaviour finish, uint32_t) override {
    assert(!pending);
    pending = correlation; pendingSetup = true; lastSetup = correlation;
    stagedMode = mode; stagedLaps = laps; stagedMinutes = minutes; lastFinish = finish;
    return true;
  }
  bool submitStart(uint32_t correlation) override {
    assert(!pending); lastStart = correlation; pending = correlation; pendingSetup = false; return true;
  }
  bool submitOperation(uint32_t correlation, SessionOperation operation, bool) override {
    assert(!pending); lastOperation = operation; pending = correlation; pendingSetup = false; return true;
  }
  bool result(uint32_t correlation, RequestResult& result, RequestRejection& reason) const override {
    if (nextResult != correlation) return false;
    result = resultValue; reason = resultReason; return true;
  }
  void complete(RequestResult result = RequestResult::Accepted) {
    assert(pending); nextResult = pending; resultValue = result;
    if (result == RequestResult::Accepted && pendingSetup) {
      value.proposal.mode = stagedMode; value.proposal.lapTarget = stagedLaps;
      value.proposal.durationMinutes = stagedMinutes; ++value.proposalRevision;
    }
    pending = 0;
  }

private:
  SessionMode stagedMode = SessionMode::LapRace;
  uint32_t stagedLaps = 10;
  uint16_t stagedMinutes = 10;
};

int main() {
  FakeAuthority authority;
  authority.value.state.lifecycle = SessionLifecycle::Ready;
  authority.value.proposal.mode = SessionMode::LapRace;
  authority.value.proposal.lapTarget = 10;
  authority.value.proposal.durationMinutes = 10;
  authority.value.proposalRevision = 1;
  authority.value.readiness = StartReadiness::Ready;

  TasterSurfaceModel taster(authority, 11);
  taster.sync();
  assert(taster.selectedMode() == SessionMode::LapRace);
  assert(taster.selectedValue() == 10);
  assert(taster.startEnabled());


  // A second local edit is retained while the first P&P mutation is pending.
  taster.adjust(-1);
  taster.adjust(-1);
  assert(taster.selectedValue() == 8);
  assert(taster.setupPending());
  assert(!taster.startEnabled());
  authority.complete();
  taster.service();
  assert(taster.selectedValue() == 8);
  assert(taster.setupPending());
  authority.complete();
  taster.service();
  assert(!taster.setupPending());
  assert(authority.value.proposal.lapTarget == 8);

  // A held +/- gesture changes the local presentation immediately but sends
  // one final authoritative setup request when released, rather than one NVS
  // transaction per repeat tick.
  TasterSurfaceModel held(authority, 33);
  authority.value.state.lifecycle = SessionLifecycle::Ready;
  authority.value.proposal.mode = SessionMode::LapRace;
  authority.value.proposal.lapTarget = 10;
  authority.value.proposal.durationMinutes = 10;
  authority.value.readiness = StartReadiness::Ready;
  held.sync();
  assert(held.adjustPreview(-1));
  assert(held.adjustPreview(-1));
  assert(held.selectedValue() == 8);
  assert(held.setupPending());
  assert(authority.pending == 0);
  assert(held.commitAdjustment());
  assert(authority.lastSetup != 0);
  authority.complete();
  held.service();
  assert(!held.setupPending());
  assert(authority.value.proposal.lapTarget == 8);

  // The same deferred gesture path applies to the independent Endurance
  // duration value and still uses Finish Current Lap.
  held.selectMode(SessionMode::Endurance);
  authority.complete();
  held.service();
  assert(authority.value.proposal.mode == SessionMode::Endurance);
  assert(authority.lastFinish == LapFinishBehaviour::CompleteCurrentLap);
  assert(held.adjustPreview(-1));
  assert(held.adjustPreview(-1));
  assert(held.selectedValue() == 8);
  assert(authority.pending == 0);
  assert(held.commitAdjustment());
  authority.complete();
  held.service();
  assert(authority.value.proposal.durationMinutes == 8);

  // A rejected final setup request reconciles the local preview back to the
  // last authoritative value and re-enables START only after that rollback.
  authority.value.proposal.mode = SessionMode::LapRace;
  authority.value.proposal.lapTarget = 10;
  authority.value.proposal.durationMinutes = 10;
  TasterSurfaceModel rejected(authority, 44);
  rejected.sync();
  assert(rejected.startEnabled());
  assert(rejected.adjustPreview(-1));
  assert(!rejected.startEnabled());
  assert(rejected.commitAdjustment());
  authority.complete(RequestResult::Rejected);
  rejected.service();
  assert(rejected.selectedMode() == SessionMode::LapRace);
  assert(rejected.selectedValue() == 10);
  assert(!rejected.setupPending());
  assert(rejected.startEnabled());

  // The repeat cadence is deterministic and bounded from 250 ms (4/s) to
  // 100 ms (10/s), with no wrap at the limit.
  assert(adjustmentRepeatIntervalMs(0) == 250U);
  assert(adjustmentRepeatIntervalMs(1) == 225U);
  assert(adjustmentRepeatIntervalMs(6) == 100U);
  assert(adjustmentRepeatIntervalMs(100) == 100U);

  // The +/- hold timing keeps its original three-second phase through an
  // intermittent GT911 contact gap.  Empty samples pause adjustment; they do
  // not reset the hold or replay missed ticks when contact returns.
  assert(!adjustmentReleaseConfirmed(1000U, 1249U));
  assert(adjustmentReleaseConfirmed(1000U, 1250U));
  assert(adjustmentHoldDelta(1, 2999U) == 1);
  assert(adjustmentHoldDelta(1, 3000U) == 10);
  assert(adjustmentHoldDelta(-1, 3000U) == -10);
  // A late 3,150 ms poll skips the 3,100 ms slot and resumes at 3,200 ms:
  // fixed deadlines, no catch-up burst and no drift from the late poll.
  assert(adjustmentDeadlineAfter(3100U, 100U, 3150U) == 3200U);
  assert(adjustmentDeadlineAfter(3100U, 100U, 3090U) == 3100U);
  // The 250 ms adjustment-only release confirmation exceeds the 201 ms
  // physical loop outlier while a genuine release remains deterministic.
  assert(kAdjustmentReleaseConfirmationMs == 250U);
  // Rapid taps remain taps; PAUSE retains its established 700 ms threshold.
  assert(!touchHoldThresholdReached(699U));
  assert(touchHoldThresholdReached(700U));

  char clock[32]{};
  formatClockMinutesSeconds(1000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "00:01") == 0);
  formatClockMinutesSeconds(60000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "01:00") == 0);
  formatClockMinutesSeconds(600000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "10:00") == 0);
  formatClockMinutesSeconds(9000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "00:09") == 0);
  formatClockMinutesSeconds(75000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "01:15") == 0);
  formatClockMinutesSeconds(3600000000ULL, clock, sizeof(clock));
  assert(std::strcmp(clock, "60:00") == 0);
  formatDurationMinutesSeconds(999, clock, sizeof(clock));
  assert(std::strcmp(clock, "999:00") == 0);
  assert(std::strcmp(tasterRaceHeading(SessionMode::Endurance), "ENDURANCE") == 0);
  assert(std::strcmp(tasterRaceHeading(SessionMode::LapRace), "LAPS") == 0);
  assert(kTasterHeadingLabelX == 35);
  assert(pausedWarningPhase(0) == 0);
  assert(pausedWarningPhase(499) == 0);
  assert(pausedWarningPhase(500) == 1);
  assert(pausedWarningLampLit(0, 0));
  assert(!pausedWarningLampLit(1, 0));
  assert(pausedWarningLampLit(2, 0));
  assert(!pausedWarningLampLit(3, 0));
  assert(pausedWarningLampLit(4, 0));
  assert(!pausedWarningLampLit(0, 1));
  assert(pausedWarningLampLit(1, 1));
  assert(!pausedWarningLampLit(2, 1));
  assert(pausedWarningLampLit(3, 1));
  assert(!pausedWarningLampLit(4, 1));

  // Mode selection is a proposal mutation, not local race state.
  taster.toggleMode();
  assert(taster.selectedMode() == SessionMode::Endurance);
  authority.complete(); taster.service();
  assert(authority.value.proposal.mode == SessionMode::Endurance);
  assert(authority.lastFinish == LapFinishBehaviour::CompleteCurrentLap);
  taster.toggleMode();
  assert(taster.selectedMode() == SessionMode::LapRace);
  authority.complete(); taster.service();
  assert(authority.value.proposal.mode == SessionMode::LapRace);
  assert(authority.lastFinish == LapFinishBehaviour::CompleteFullRaceDistance);
  taster.selectMode(SessionMode::Endurance);
  assert(taster.selectedMode() == SessionMode::Endurance);
  assert(taster.selectedValue() == 10);
  taster.adjust(-2);
  assert(taster.selectedValue() == 8);
  authority.complete(); taster.service();
  authority.complete(); taster.service();
  assert(authority.value.proposal.mode == SessionMode::Endurance);

  // START is generic and carries no competing setup payload at this boundary.
  assert(taster.start());
  assert(authority.lastStart != 0);
  authority.complete(); taster.service();

  // Multiple Taster clients can share the same P&P authority independently.
  TasterSurfaceModel second(authority, 22);
  assert(second.clientId() != taster.clientId());
  authority.value.state.lifecycle = SessionLifecycle::Racing;
  second.sync();
  assert(!second.startEnabled());
  assert(second.operation(SessionOperation::Pause));
  assert(authority.lastOperation == SessionOperation::Pause);
  authority.complete(); second.service();

  FakeAuthority boundaryAuthority;
  boundaryAuthority.value.state.lifecycle = SessionLifecycle::Ready;
  boundaryAuthority.value.proposal.mode = SessionMode::LapRace;
  boundaryAuthority.value.proposal.lapTarget = 10;
  boundaryAuthority.value.proposal.durationMinutes = 10;
  boundaryAuthority.value.readiness = StartReadiness::Ready;
  TasterSurfaceModel boundary(boundaryAuthority, 33);
  boundary.sync();
  // Long-hold ten-step changes reach a boundary before wrapping; zero is never
  // exposed. The same preview path backs LAPS and ENDURANCE.
  for (unsigned i = 0; i < 99; ++i) assert(boundary.adjustPreview(10));
  assert(boundary.lapValue() == 999);
  assert(boundary.adjustPreview(10));
  assert(boundary.lapValue() == 1);
  assert(boundary.adjustPreview(-10));
  assert(boundary.lapValue() == 999);
  boundary.selectMode(SessionMode::Endurance);
  for (unsigned i = 0; i < 99; ++i) assert(boundary.adjustPreview(10));
  assert(boundary.durationValue() == 999);
  assert(boundary.adjustPreview(10));
  assert(boundary.durationValue() == 1);

  // A Browser-owned target remains visible and authoritative until the user
  // explicitly edits laps on the Taster. Mode and duration changes carry it
  // through unchanged.
  FakeAuthority browserAuthority;
  browserAuthority.value.state.lifecycle = SessionLifecycle::Ready;
  browserAuthority.value.proposal.mode = SessionMode::LapRace;
  browserAuthority.value.proposal.lapTarget = 1200;
  browserAuthority.value.proposal.durationMinutes = 10;
  browserAuthority.value.readiness = StartReadiness::Ready;
  TasterSurfaceModel browserSetup(browserAuthority, 55);
  browserSetup.sync();
  assert(browserSetup.selectedValue() == 1200);
  assert(browserAuthority.lastSetup == 0);
  // The Taster is a standalone control surface: it never starts a shared
  // Browser target outside its 1–999 local race range, and must not change it
  // merely to make START available.
  assert(!browserSetup.startEnabled());
  assert(!browserSetup.start());
  assert(browserAuthority.lastStart == 0);
  assert(browserAuthority.lastSetup == 0);
  assert(browserAuthority.value.proposal.lapTarget == 1200);

  browserSetup.selectMode(SessionMode::Endurance);
  browserAuthority.complete();
  browserSetup.service();
  browserSetup.adjust(1);
  browserAuthority.complete();
  browserSetup.service();
  assert(browserAuthority.value.proposal.durationMinutes == 11);
  assert(browserAuthority.value.proposal.lapTarget == 1200);
  browserSetup.selectMode(SessionMode::LapRace);
  browserAuthority.complete();
  browserSetup.service();
  assert(browserSetup.selectedValue() == 1200);
  assert(browserAuthority.value.proposal.lapTarget == 1200);

  assert(browserSetup.adjustPreview(-10));
  assert(browserSetup.selectedValue() == 999);
  assert(browserAuthority.pending == 0);
  assert(browserAuthority.value.proposal.lapTarget == 1200);
  assert(browserSetup.commitAdjustment());
  browserAuthority.complete();
  browserSetup.service();
  assert(browserAuthority.value.proposal.lapTarget == 999);
  assert(browserSetup.startEnabled());
  assert(browserSetup.start());
  browserAuthority.complete();
  browserSetup.service();
  assert(browserSetup.adjustPreview(1));
  assert(browserSetup.selectedValue() == 1);
  assert(browserSetup.adjustPreview(-1));
  assert(browserSetup.selectedValue() == 999);

  authority.value.state.lifecycle = SessionLifecycle::Paused;
  second.sync();
  assert(second.operation(SessionOperation::Resume));
  authority.complete(); second.service();
  assert(second.operation(SessionOperation::RestartRace));
  authority.complete(); second.service();
  assert(second.abandon(true));
  authority.complete(); second.service();
  authority.value.state.lifecycle = SessionLifecycle::Finished;
  second.sync();
  assert(second.operation(SessionOperation::RaceAgain));
  authority.complete(); second.service();

  std::cout << "STAGE8A_TASTER_SURFACE PASS\n";
  return 0;
}
