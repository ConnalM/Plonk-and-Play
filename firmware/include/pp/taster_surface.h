#pragma once

#include "noticeboard.h"
#include <algorithm>

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
  // A remote transport can abandon only the local waiting indicator after an
  // uncertain outcome.  It must not invent a result for the sporting core.
  virtual bool pendingUncertain(uint32_t) { return false; }
  virtual ~TasterAuthority() = default;
};

// Pure control model for tests and non-Arduino clients.  Values held here are
// only the user's pending desire; accepted setup and lifecycle always come
// from TasterAuthority::snapshot().
class TasterSurfaceModel {
public:
  explicit TasterSurfaceModel(TasterAuthority& authority, uint64_t clientId = 0x544153544552ULL,
                              uint32_t initialCorrelation = 1)
      : authority_(authority), clientId_(clientId), nextCorrelation_(initialCorrelation) {}

  void sync() {
    snapshot_ = authority_.snapshot();
    if (!desiredPending_ ||
        (proposalMatchesDesired() && snapshot_.proposalRevision != desiredRevision_)) {
      desiredPending_ = false;
      selectedMode_ = snapshot_.proposal.mode;
      lapValue_ = snapshot_.proposal.lapTarget ? snapshot_.proposal.lapTarget : 10;
      durationValue_ = clampValue(snapshot_.proposal.durationMinutes, 10);
      desiredRevision_ = snapshot_.proposalRevision;
    }
  }

  const TasterSnapshot& snapshot() const { return snapshot_; }
  SessionMode selectedMode() const { return selectedMode_; }
  uint32_t selectedValue() const {
    return selectedMode_ == SessionMode::Endurance ? durationValue_ : lapValue_;
  }
  uint32_t lapValue() const { return lapValue_; }
  uint32_t durationValue() const { return durationValue_; }
  bool setupPending() const { return desiredPending_ || requestPending_; }
  bool startEnabled() const {
    return !setupPending() && snapshot_.state.lifecycle == SessionLifecycle::Ready &&
           snapshot_.readiness == StartReadiness::Ready &&
           (selectedMode_ != SessionMode::LapRace || lapValue_ <= kTasterLapMaximum);
  }
  uint32_t clientId() const { return clientId_; }
  uint32_t allocateCorrelation() {
    const uint32_t value = nextCorrelation_++;
    return value ? value : nextCorrelation_++;
  }

  void toggleMode() {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready) return;
    selectMode(selectedMode_ == SessionMode::Endurance ? SessionMode::LapRace
                                                        : SessionMode::Endurance);
  }

  void selectMode(SessionMode mode) {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready ||
        (mode != SessionMode::LapRace && mode != SessionMode::Endurance)) return;
    selectedMode_ = mode;
    if (selectedMode_ == SessionMode::Endurance) durationValue_ = clampValue(durationValue_, 10);
    queueSetup();
  }

  void adjust(int delta) {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready || delta == 0) return;
    uint32_t& selected = selectedMode_ == SessionMode::Endurance ? durationValue_ : lapValue_;
    selected = adjustedValue(selected, delta);
    queueSetup();
  }

  // Apply a local value change without submitting a persistence operation.
  // This is used while a finger is held on +/- so the display follows the
  // user's gesture immediately, while one final authoritative setup request
  // is sent when the gesture is released.  The normal adjust() path remains
  // unchanged for non-touch clients.
  bool adjustPreview(int delta) {
    if (snapshot_.state.lifecycle != SessionLifecycle::Ready || delta == 0) {
      return false;
    }
    uint32_t& selected = selectedMode_ == SessionMode::Endurance
                             ? durationValue_
                             : lapValue_;
    const uint32_t before = selected;
    selected = adjustedValue(selected, delta);
    if (selected == before) return false;
    if (!desiredPending_) desiredRevision_ = snapshot_.proposalRevision;
    desiredPending_ = true;
    return true;
  }

  // Publish the final value of a held adjustment through the existing
  // authority/persistence boundary.  If another setup request is already in
  // flight, service() will submit the latest desired value after it resolves.
  bool commitAdjustment() {
    if (!desiredPending_) return false;
    queueSetup();
    return true;
  }

  bool start() {
    if (!startEnabled() || requestPending_) return false;
    pendingWasSetup_ = false;
    pendingCorrelation_ = allocateCorrelation();
    requestPending_ = authority_.submitStart(pendingCorrelation_);
    return requestPending_;
  }

  bool operation(SessionOperation operation, bool confirmed = false) {
    if (requestPending_) return false;
    pendingWasSetup_ = false;
    pendingCorrelation_ = allocateCorrelation();
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
    if (!authority_.result(pendingCorrelation_, value, rejection)) {
      if (authority_.pendingUncertain(pendingCorrelation_)) {
        // Connection/authority loss leaves the real-world result unknown.
        // Discard local intent and wait for a new authoritative snapshot;
        // neither result enum is fabricated here.
        requestPending_ = false;
        pendingWasSetup_ = false;
        desiredPending_ = false;
      }
      sync(); return;
    }
    requestPending_ = false;
    if (value == RequestResult::Rejected && pendingWasSetup_) {
      desiredPending_ = false;
    }
    sync();
    if (value == RequestResult::Accepted && pendingWasSetup_ && proposalMatchesDesired()) {
      desiredPending_ = false;
      selectedMode_ = snapshot_.proposal.mode;
      lapValue_ = snapshot_.proposal.lapTarget ? snapshot_.proposal.lapTarget : 10;
      durationValue_ = clampValue(snapshot_.proposal.durationMinutes, 10);
      desiredRevision_ = snapshot_.proposalRevision;
    }
    if (desiredPending_ && snapshot_.state.lifecycle == SessionLifecycle::Ready &&
        snapshot_.proposalRevision != desiredRevision_) queueSetup();
  }

  // A physical gesture was invalidated before it was released (for example
  // authority transfer or a stale link).  Remove local preview state so a
  // later reconnect cannot commit an old held adjustment.
  void cancelInteraction() {
    if (requestPending_) return;
    desiredPending_ = false;
    selectedMode_ = snapshot_.proposal.mode;
    lapValue_ = snapshot_.proposal.lapTarget ? snapshot_.proposal.lapTarget : 10;
    durationValue_ = clampValue(snapshot_.proposal.durationMinutes, 10);
    desiredRevision_ = snapshot_.proposalRevision;
  }

private:
  static constexpr uint32_t kTasterLapMaximum = 999;

  static uint32_t clampValue(uint32_t value, uint32_t fallback) {
    if (value < 1 || value > 999) return fallback;
    return value;
  }

  // Ten-step holds stop at a boundary first, then wrap on the next tick.
  // This prevents 995 + 10 becoming 5 and keeps zero unrepresentable.
  static uint32_t adjustedValue(uint32_t value, int delta) {
    // Preserve the shared target until the user explicitly edits it here.
    // The first adjustment enters the Taster's range at its upper boundary.
    if (value > kTasterLapMaximum) return kTasterLapMaximum;
    value = clampValue(value, 1);
    if (delta > 0) return value >= kTasterLapMaximum ? 1U :
                          static_cast<uint32_t>(std::min<int64_t>(kTasterLapMaximum, static_cast<int64_t>(value) + delta));
    return value <= 1 ? kTasterLapMaximum :
                     static_cast<uint32_t>(std::max<int64_t>(1, static_cast<int64_t>(value) + delta));
  }

  void queueSetup() {
    desiredPending_ = true;
    pendingWasSetup_ = true;
    desiredRevision_ = snapshot_.proposalRevision;
    if (requestPending_) return;
    pendingCorrelation_ = allocateCorrelation();
    // Taster's two supported modes use their authoritative P&P finish
    // policies: a LAPS race completes the full configured distance, while a
    // TIMED race (the Endurance model) finishes the current lap after expiry.
    const LapFinishBehaviour finish =
        selectedMode_ == SessionMode::Endurance
            ? LapFinishBehaviour::CompleteCurrentLap
            : LapFinishBehaviour::CompleteFullRaceDistance;
    requestPending_ = authority_.submitSetup(pendingCorrelation_, selectedMode_,
                                              selectedMode_ == SessionMode::Endurance
                                                  ? (snapshot_.proposal.lapTarget ? snapshot_.proposal.lapTarget : 10)
                                                  : lapValue_,
                                              selectedMode_ == SessionMode::Endurance
                                                  ? static_cast<uint16_t>(durationValue_)
                                                  : static_cast<uint16_t>(snapshot_.proposal.durationMinutes
                                                                              ? snapshot_.proposal.durationMinutes : 10),
                                              finish,
                                              snapshot_.proposalRevision);
  }

  bool proposalMatchesDesired() const {
    return snapshot_.proposal.mode == selectedMode_ &&
           (selectedMode_ == SessionMode::Endurance
                ? snapshot_.proposal.durationMinutes == durationValue_
                : snapshot_.proposal.lapTarget == lapValue_);
  }

  TasterAuthority& authority_;
  uint64_t clientId_;
  TasterSnapshot snapshot_{};
  SessionMode selectedMode_ = SessionMode::LapRace;
  uint32_t lapValue_ = 10;
  uint32_t durationValue_ = 10;
  uint32_t desiredRevision_ = 0;
  uint32_t pendingCorrelation_ = 0;
  uint32_t nextCorrelation_ = 1;
  bool desiredPending_ = false;
  bool requestPending_ = false;
  bool pendingWasSetup_ = false;
};

} // namespace pp
