#pragma once

#include "taster_link_display_payload.h"
#include "taster_surface.h"
#include "taster_presentation_preferences.h"

namespace pp::taster_link::wire {

// Projection from one coherent Core-owned sample. The caller must collect
// state, proposal, readiness, committed preferences, context and time on the
// Core owner loop, not on a socket worker. No reverse projection or mutation.
#define PP_WIRE_ENUM(native,wire) static_assert(static_cast<uint8_t>(native)==static_cast<uint8_t>(wire),"update explicit snapshot mapping")
PP_WIRE_ENUM(SessionLifecycle::Ready,DisplayLifecycle::Ready);
PP_WIRE_ENUM(SessionLifecycle::Starting,DisplayLifecycle::Starting);
PP_WIRE_ENUM(SessionLifecycle::Racing,DisplayLifecycle::Racing);
PP_WIRE_ENUM(SessionLifecycle::Paused,DisplayLifecycle::Paused);
PP_WIRE_ENUM(SessionLifecycle::Restarting,DisplayLifecycle::Restarting);
PP_WIRE_ENUM(SessionLifecycle::Finished,DisplayLifecycle::Finished);
PP_WIRE_ENUM(SessionLifecycle::Faulted,DisplayLifecycle::Faulted);
PP_WIRE_ENUM(SessionMode::None,DisplayMode::None);
PP_WIRE_ENUM(SessionMode::LapRace,DisplayMode::Laps);
PP_WIRE_ENUM(SessionMode::OpenPractice,DisplayMode::Practice);
PP_WIRE_ENUM(SessionMode::Endurance,DisplayMode::Timed);
PP_WIRE_ENUM(StartReadiness::Ready,Readiness::Ready);
PP_WIRE_ENUM(StartReadiness::LifecycleNotStartable,Readiness::NotStartable);
PP_WIRE_ENUM(StartReadiness::InvalidRaceSetup,Readiness::InvalidSetup);
PP_WIRE_ENUM(StartReadiness::CapabilityUnavailable,Readiness::CapabilityUnavailable);
PP_WIRE_ENUM(LapFinishBehaviour::Immediate,FinishPolicy::Immediate);
PP_WIRE_ENUM(LapFinishBehaviour::CompleteCurrentLap,FinishPolicy::CurrentLap);
PP_WIRE_ENUM(LapFinishBehaviour::CompleteFullRaceDistance,FinishPolicy::FullDistance);
PP_WIRE_ENUM(RestartMethod::None,Restart::None);
PP_WIRE_ENUM(RestartMethod::Honour,Restart::Honour);
PP_WIRE_ENUM(RestartMethod::Grid,Restart::Grid);
PP_WIRE_ENUM(StartPresentationPhase::None,Phase::None);
PP_WIRE_ENUM(StartPresentationPhase::Countdown,Phase::Countdown);
PP_WIRE_ENUM(StartPresentationPhase::ResumeCountdown,Phase::ResumeCountdown);
PP_WIRE_ENUM(StartPresentationPhase::Go,Phase::Go);
PP_WIRE_ENUM(RaceEngineModule::LiveGapKind::None,Gap::None);
PP_WIRE_ENUM(RaceEngineModule::LiveGapKind::Laps,Gap::Laps);
PP_WIRE_ENUM(RaceEngineModule::LiveGapKind::Time,Gap::Time);
PP_WIRE_ENUM(TasterCarGraphic::GreenMgb,Graphic::GreenMgb);
PP_WIRE_ENUM(TasterCarGraphic::RedEtype,Graphic::RedEtype);
#undef PP_WIRE_ENUM
static_assert(TasterPresentationPreferences::LabelBytes+1==LabelSize,"update explicit label mapping");

inline bool projectDisplay(const TasterSnapshot& sample,
                           const TasterPresentationPreferences* preferences,
                           uint64_t preferencesRevision,
                           const Context& context, uint64_t coreTimeUs,
                           DisplaySnapshot& out) {
  const auto& n=sample.state;
  const auto& p=sample.proposal;
  if(n.entryCount>2 || n.entryCount>PP_MAX_ENTRIES || p.activeLanes>2 ||
     sample.proposalRevision!=n.proposalRevision ||
     sample.proposalRevision!=context.proposalRevision) return false;
  DisplaySnapshot s{};
  s.session=context.session; s.authorityGeneration=context.authorityGeneration;
  s.lifecycleRevision=context.lifecycleRevision; s.raceId=context.raceId;
  s.proposalRevision=context.proposalRevision; s.coreTimeUs=coreTimeUs;
  s.lifecycle=static_cast<DisplayLifecycle>(n.lifecycle);
  if(gateLifecycle(s.lifecycle)!=context.lifecycle) return false;
  s.tasterAllowed=context.tasterAllowed;
  s.activeMode=static_cast<DisplayMode>(n.sessionMode);
  s.proposalMode=static_cast<DisplayMode>(p.mode);
  s.readiness=static_cast<Readiness>(sample.readiness);
  s.activeFinish=static_cast<FinishPolicy>(n.finishBehaviour);
  s.proposalFinish=static_cast<FinishPolicy>(p.finish);
  s.restart=static_cast<Restart>(n.restartMethod);
  s.phase=static_cast<Phase>(n.startPhase);
  s.redCount=n.redLightCount; s.redLit=n.redLightsLit; s.startSignal=n.startSignal;
  s.entryCount=n.entryCount; s.proposedLanes=p.activeLanes;
  s.proposalLaps=p.lapTarget; s.activeLaps=n.lapTarget;
  s.proposalMinutes=p.durationMinutes; s.activeMinutes=n.durationMinutes;
  auto flag=[&](bool value,DisplayFlags f){if(value) s.flags|=f;};
  flag(n.durationExpired,DurationExpired); flag(n.simulatedSession,Simulated);
  flag(n.startLightsGreen,Green); flag(n.resultSealed,ResultSealed);
  flag(n.resultValid,ResultValid); flag(n.deadHeat,DeadHeat);
  flag(n.raceIntegrityFaulted,IntegrityFault); flag(n.finishDisplayActive,FinishDisplay);
  flag(n.persistencePending,PersistencePending); flag(n.persistenceFault,PersistenceFault);
  s.elapsedUs=n.raceElapsed; s.remainingUs=n.remainingDuration; s.overtimeUs=n.overtime;
  s.goUs=n.scheduledGo; s.phaseUntilUs=n.startPhaseUntil; s.pauseUs=n.pauseEffectiveAt;
  s.restartUs=n.scheduledRestartAt; s.expiryUs=n.durationExpiryAt; s.finishUntilUs=n.finishDisplayUntil;
  if(preferences) {
    // Fixed arrays are checked for termination before passing them to any
    // string validator. Copy only the meaningful prefix; padding is zero.
    const char* names[]={preferences->lane1Label,preferences->lane2Label};
    for(size_t i=0;i<2;++i) {
      const auto* end=static_cast<const char*>(std::memchr(names[i],0,LabelSize));
      if(!end) return false;
      std::memcpy(s.labels[i].data(),names[i],static_cast<size_t>(end-names[i]));
    }
    s.graphics[0]=static_cast<Graphic>(preferences->lane1Graphic);
    s.graphics[1]=static_cast<Graphic>(preferences->lane2Graphic);
    s.preferencesRevision=preferencesRevision;
    s.flags|=PreferencesPresent;
  } else if(preferencesRevision) return false;
  for(size_t i=0;i<s.entryCount;++i) {
    const auto& a=n.entries[i]; auto& b=s.lanes[i];
    b.lane=a.lane; b.gap=static_cast<Gap>(a.gapKind);
    if(a.hasLap) b.flags|=HasLap;
    if(a.completed) b.flags|=Completed;
    if(a.hasOvertimeOffset) b.flags|=HasOvertime;
    if(a.waitingForTimingOrigin) b.flags|=WaitingOrigin;
    b.laps=a.laps; b.classified=a.classifiedLaps; b.penalty=a.lapPenalty;
    b.position=a.position; b.lapsBehind=a.lapsBehind;
    b.lastLapUs=a.lastLapTime; b.bestLapUs=a.bestLapTime;
    b.finishUs=a.finishDuration; b.overtimeUs=a.overtimeOffset; b.gapUs=a.gapTime;
  }
  if(!validDisplay(s)) return false;
  out=s; return true;
}

} // namespace pp::taster_link::wire
