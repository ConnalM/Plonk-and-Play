#pragma once

#include "physical_taster_authority.h"
#include "taster_link_startup_taster_esp.h"

namespace pp {

// Presentation/control adapter for the independently-built wireless Taster.
// It owns no sporting state: every displayed value is copied from the latest
// authenticated display snapshot and every gesture is passed to the existing
// link gate.  Until the role's commissioning/qualification approval exists,
// the startup owner remains cold and this adapter stays visibly non-startable.
class RemoteTasterAuthority final : public PhysicalTasterAuthority {
 public:
  explicit RemoteTasterAuthority(taster_link::startup::TasterRuntime& runtime)
      : runtime_(runtime) {}

  TasterSnapshot snapshot() const override { refresh(); return snapshot_; }

  bool submitSetup(uint32_t correlation, SessionMode mode, uint32_t lapTarget,
                   uint16_t durationMinutes, LapFinishBehaviour,
                   uint32_t) override {
    const auto wireMode = mode == SessionMode::Endurance
                              ? taster_link::Mode::Timed
                              : taster_link::Mode::Laps;
    const uint16_t value = mode == SessionMode::Endurance ? durationMinutes
                                                           : static_cast<uint16_t>(lapTarget);
    return send(correlation, taster_link::Action::Setup, wireMode, value, false);
  }
  bool submitStart(uint32_t correlation) override {
    return send(correlation, taster_link::Action::Start);
  }
  bool submitOperation(uint32_t correlation, SessionOperation operation,
                       bool confirmed) override {
    using A = taster_link::Action;
    A action{};
    switch (operation) {
      case SessionOperation::Pause: action = A::Pause; break;
      case SessionOperation::Resume: action = A::Resume; break;
      case SessionOperation::EndRace: action = A::Abandon; break;
      case SessionOperation::RaceAgain: action = A::RaceAgain; break;
      default: return false;
    }
    return send(correlation, action, taster_link::Mode::None, 0, confirmed);
  }
  bool result(uint32_t, RequestResult&, RequestRejection&) const override {
    // The reviewed wire protocol deliberately holds control until a newer
    // authoritative snapshot. It exposes no local result cache to replay;
    // surface reconciliation is therefore snapshot-driven.
    return false;
  }
  NoticeboardState stateAt(Time) const override { refresh(); return snapshot_.state; }
  SessionLifecycle lifecycle() const override { refresh(); return snapshot_.state.lifecycle; }
  const TasterPresentationPreferences* committedPreferences() const override {
    refresh(); return preferencesPresent_ ? &preferences_ : nullptr;
  }
  void pollResults() override { refresh(); }
  bool beginTouch() override { return runtime_.beginTouch(); }
  void observeReleased() override { runtime_.observeReleased(); }

 private:
  static SessionLifecycle lifecycleFor(taster_link::wire::DisplayLifecycle value) {
    using L = taster_link::wire::DisplayLifecycle;
    switch (value) {
      case L::Ready: return SessionLifecycle::Ready;
      case L::Starting: return SessionLifecycle::Starting;
      case L::Racing: return SessionLifecycle::Racing;
      case L::Paused: return SessionLifecycle::Paused;
      case L::Restarting: return SessionLifecycle::Restarting;
      case L::Finished: return SessionLifecycle::Finished;
      default: return SessionLifecycle::Faulted;
    }
  }
  static SessionMode modeFor(taster_link::wire::DisplayMode value) {
    return value == taster_link::wire::DisplayMode::Timed ? SessionMode::Endurance :
           value == taster_link::wire::DisplayMode::Laps ? SessionMode::LapRace :
           SessionMode::None;
  }
  static StartReadiness readinessFor(taster_link::wire::Readiness value) {
    using R = taster_link::wire::Readiness;
    return value == R::Ready ? StartReadiness::Ready :
           value == R::InvalidSetup ? StartReadiness::InvalidRaceSetup :
           value == R::CapabilityUnavailable ? StartReadiness::CapabilityUnavailable :
           StartReadiness::LifecycleNotStartable;
  }
  bool send(uint32_t correlation, taster_link::Action action,
            taster_link::Mode mode = taster_link::Mode::None,
            uint16_t value = 0, bool confirmed = false) {
    if (!correlation || pendingCorrelation_ || !runtime_.command(action, mode, value, confirmed)) return false;
    pendingCorrelation_ = correlation;
    return true;
  }
  void refresh() const {
    const auto view = runtime_.view();
    if (view.state != taster_link::health::State::Fresh || !view.hasSnapshot) {
      snapshot_ = {};
      snapshot_.state.lifecycle = SessionLifecycle::Faulted;
      snapshot_.readiness = StartReadiness::LifecycleNotStartable;
      preferencesPresent_ = false;
      return;
    }
    const auto& remote = view.snapshot;
    TasterSnapshot local{};
    local.state.lifecycle = lifecycleFor(remote.lifecycle);
    local.state.sessionMode = modeFor(remote.activeMode);
    local.state.finishBehaviour = remote.activeFinish == taster_link::wire::FinishPolicy::CurrentLap
        ? LapFinishBehaviour::CompleteCurrentLap : LapFinishBehaviour::CompleteFullRaceDistance;
    local.state.lapTarget = remote.activeLaps;
    local.state.durationMinutes = remote.activeMinutes;
    local.state.raceElapsed = remote.elapsedUs;
    local.state.remainingDuration = remote.remainingUs;
    local.state.overtime = remote.overtimeUs;
    local.state.redLightCount = remote.redCount;
    local.state.redLightsLit = remote.redLit;
    local.state.startLightsGreen = (remote.flags & taster_link::wire::Green) != 0;
    local.state.resultSealed = (remote.flags & taster_link::wire::ResultSealed) != 0;
    local.state.resultValid = (remote.flags & taster_link::wire::ResultValid) != 0;
    local.state.entryCount = remote.entryCount;
    for (uint8_t i = 0; i < remote.entryCount; ++i) {
      const auto& from = remote.lanes[i]; auto& to = local.state.entries[i];
      to.lane = from.lane; to.laps = from.laps; to.classifiedLaps = from.classified;
      to.position = from.position; to.lastLapTime = from.lastLapUs;
      to.bestLapTime = from.bestLapUs; to.gapTime = from.gapUs;
    }
    local.proposal.mode = modeFor(remote.proposalMode);
    local.proposal.lapTarget = remote.proposalLaps;
    local.proposal.durationMinutes = remote.proposalMinutes;
    local.proposalRevision = remote.proposalRevision;
    local.readiness = readinessFor(remote.readiness);
    snapshot_ = local;
    preferencesPresent_ = (remote.flags & taster_link::wire::PreferencesPresent) != 0;
    if (preferencesPresent_) {
      memcpy(preferences_.lane1Label, remote.labels[0].data(), remote.labels[0].size());
      memcpy(preferences_.lane2Label, remote.labels[1].data(), remote.labels[1].size());
      preferences_.lane1Graphic = remote.graphics[0] == taster_link::wire::Graphic::RedEtype
          ? TasterCarGraphic::RedEtype : TasterCarGraphic::GreenMgb;
      preferences_.lane2Graphic = remote.graphics[1] == taster_link::wire::Graphic::RedEtype
          ? TasterCarGraphic::RedEtype : TasterCarGraphic::GreenMgb;
    }
  }
  taster_link::startup::TasterRuntime& runtime_;
  mutable TasterSnapshot snapshot_{};
  mutable TasterPresentationPreferences preferences_{};
  mutable bool preferencesPresent_ = false;
  uint32_t pendingCorrelation_ = 0;
};
} // namespace pp
