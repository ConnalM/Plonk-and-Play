#pragma once

#include "noticeboard.h"

namespace pp {

// This is the deliberately small authority boundary used by the integrated
// Taster surface.  It is a control/presentation client; it never owns race
// lifecycle, timing, classification or results.
struct TasterSnapshot {
  NoticeboardState state{};
  ProposedRaceSetup proposal{};
  StartReadiness readiness = StartReadiness::LifecycleNotStartable;
  uint32_t proposalRevision = 0;
};

class TasterAuthority {
public:
  virtual TasterSnapshot snapshot() const = 0;
  virtual bool submitSetup(uint32_t correlation, SessionMode mode,
                           uint32_t lapTarget, uint16_t durationMinutes,
                           LapFinishBehaviour finish, uint32_t revision) = 0;
  virtual bool submitStart(uint32_t correlation) = 0;
  virtual bool submitOperation(uint32_t correlation, SessionOperation operation,
                               bool confirmed) = 0;
  virtual bool result(uint32_t correlation, RequestResult& value,
                      RequestRejection& rejection) const = 0;
  virtual ~TasterAuthority() = default;
};

// Pure control model for tests and non-Arduino clients.  Values held here are
// only the user's pending desire; accepted setup and lifecycle always come
// from TasterAuthority::snapshot().
class TasterSurfaceModel {
public:
  explicit TasterSurfaceModel(TasterAuthority& authority, uint64_t clientId = 0x544153544552ULL)
      : authority_(authority), clientId_(clientId) {}

  void sync() {
    snapshot_ = authority_.snapshot();
    if (!desiredPending_ ||
        (proposalMatchesDesired() && snapshot_.proposalRevision != desiredRevision_)) {
      desiredPending_ = false;
      selectedMode_ = snapshot_.proposal.mode;
      selectedValue_ = selectedMode_ == SessionMode::Endurance
                           ? snapshot_.proposal.durationMinutes
                           : snapshot_.proposal.lapTarget;
      desiredRevision_ = snapshot_.proposalRevision;
    }
  }

  const TasterSnapshot& snapshot() const { return snapshot_; }
  SessionMode selectedMode() const { return selectedMode_; }
  uint32_t selectedValue() const { return selectedValue_; }
  bool setupPending() const { return desiredPending_ || requestPending_; }
  bool startEnabled() const {
    return !setupPending() && snapshot_.state.lifecycle == SessionLifecycle::Ready &&
           snapshot_.readiness == StartReadiness::Ready;
  }
  uint32_t clientId() const { return clientId_; }

  void toggleMode() {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready) return;
    selectMode(selectedMode_ == SessionMode::Endurance ? SessionMode::LapRace
                                                        : SessionMode::Endurance);
  }

  void selectMode(SessionMode mode) {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready ||
        (mode != SessionMode::LapRace && mode != SessionMode::Endurance)) return;
    selectedMode_ = mode;
    if (selectedValue_ < 1 || selectedValue_ > 999) selectedValue_ = 10;
    queueSetup();
  }

  void adjust(int delta) {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready || delta == 0) return;
    int64_t value = static_cast<int64_t>(selectedValue_) + delta;
    if (value < 1) value = 1;
    if (value > 999) value = 999;
    selectedValue_ = static_cast<uint32_t>(value);
    queueSetup();
  }

  bool start() {
    if (!startEnabled() || requestPending_) return false;
    pendingWasSetup_ = false;
    pendingCorrelation_ = nextCorrelation_++;
    requestPending_ = authority_.submitStart(pendingCorrelation_);
    return requestPending_;
  }

  bool operation(SessionOperation operation, bool confirmed = false) {
    if (requestPending_) return false;
    pendingWasSetup_ = false;
    pendingCorrelation_ = nextCorrelation_++;
    requestPending_ = authority_.submitOperation(pendingCorrelation_, operation, confirmed);
    return requestPending_;
  }

  bool restart(bool confirmed = true) {
    return operation(SessionOperation::RestartRace, confirmed);
  }

  bool abandon(bool confirmed = true) {
    return operation(SessionOperation::EndRace, confirmed);
  }

  // Called by the host loop after each authoritative Browser/Taster tick.
  // A rejected request is reconciled from P&P; a newer local desire is sent
  // once the previous request has a result.
  void service() {
    if (!requestPending_) { sync(); return; }
    RequestResult value = RequestResult::Rejected;
    RequestRejection rejection = RequestRejection::None;
    if (!authority_.result(pendingCorrelation_, value, rejection)) { sync(); return; }
    requestPending_ = false;
    if (value == RequestResult::Rejected && pendingWasSetup_) {
      desiredPending_ = false;
    }
    sync();
    if (value == RequestResult::Accepted && pendingWasSetup_ && proposalMatchesDesired()) {
      desiredPending_ = false;
      selectedMode_ = snapshot_.proposal.mode;
      selectedValue_ = selectedMode_ == SessionMode::Endurance
                           ? snapshot_.proposal.durationMinutes : snapshot_.proposal.lapTarget;
      desiredRevision_ = snapshot_.proposalRevision;
    }
    if (desiredPending_ && snapshot_.state.lifecycle == SessionLifecycle::Ready &&
        snapshot_.proposalRevision != desiredRevision_) queueSetup();
  }

private:
  void queueSetup() {
    desiredPending_ = true;
    pendingWasSetup_ = true;
    desiredRevision_ = snapshot_.proposalRevision;
    if (requestPending_) return;
    pendingCorrelation_ = nextCorrelation_++;
    requestPending_ = authority_.submitSetup(pendingCorrelation_, selectedMode_,
                                              selectedMode_ == SessionMode::Endurance ? 1 : selectedValue_,
                                              selectedMode_ == SessionMode::Endurance ? static_cast<uint16_t>(selectedValue_) : 0,
                                              LapFinishBehaviour::Immediate,
                                              snapshot_.proposalRevision);
  }

  bool proposalMatchesDesired() const {
    return snapshot_.proposal.mode == selectedMode_ &&
           (selectedMode_ == SessionMode::Endurance
                ? snapshot_.proposal.durationMinutes == selectedValue_
                : snapshot_.proposal.lapTarget == selectedValue_);
  }

  TasterAuthority& authority_;
  uint64_t clientId_;
  TasterSnapshot snapshot_{};
  SessionMode selectedMode_ = SessionMode::LapRace;
  uint32_t selectedValue_ = 10;
  uint32_t desiredRevision_ = 0;
  uint32_t pendingCorrelation_ = 0;
  uint32_t nextCorrelation_ = 1;
  bool desiredPending_ = false;
  bool requestPending_ = false;
  bool pendingWasSetup_ = false;
};

} // namespace pp
