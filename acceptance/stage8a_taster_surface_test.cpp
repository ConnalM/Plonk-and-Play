#include <cassert>
#include <cstdint>
#include <iostream>
#include "../../firmware/include/pp/system_time.h"
#include "../../firmware/include/pp/taster_surface.h"

using namespace pp;

class FakeAuthority final : public TasterAuthority {
public:
  TasterSnapshot value{};
  uint32_t pending = 0;
  bool pendingSetup = false;
  uint32_t lastStart = 0;
  SessionOperation lastOperation = SessionOperation::Pause;
  uint32_t lastSetup = 0;
  uint32_t nextResult = 0;
  RequestResult resultValue = RequestResult::Accepted;
  RequestRejection resultReason = RequestRejection::None;

  TasterSnapshot snapshot() const override { return value; }
  bool submitSetup(uint32_t correlation, SessionMode mode, uint32_t laps,
                   uint16_t minutes, LapFinishBehaviour, uint32_t) override {
    assert(!pending);
    pending = correlation; pendingSetup = true; lastSetup = correlation;
    stagedMode = mode; stagedLaps = laps; stagedMinutes = minutes;
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

  // Mode selection is a proposal mutation, not local race state.
  taster.toggleMode();
  assert(taster.selectedMode() == SessionMode::Endurance);
  authority.complete(); taster.service();
  assert(authority.value.proposal.mode == SessionMode::Endurance);
  taster.toggleMode();
  assert(taster.selectedMode() == SessionMode::LapRace);
  authority.complete(); taster.service();
  assert(authority.value.proposal.mode == SessionMode::LapRace);
  taster.selectMode(SessionMode::Endurance);
  assert(taster.selectedMode() == SessionMode::Endurance);
  assert(taster.selectedValue() == 8);
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
