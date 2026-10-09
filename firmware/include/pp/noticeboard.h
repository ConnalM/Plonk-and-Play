#pragma once
#include "race_control.h"
#include "race_engine.h"
namespace pp {
struct NoticeboardEntryState { uint32_t raceEntryId,laps,classifiedLaps,lapPenalty,position,lapsBehind;uint8_t lane;Time lastLapTime,bestLapTime;bool hasLap,waitingForTimingOrigin; constexpr NoticeboardEntryState(uint32_t id=0,uint32_t lap=0,Time time=0,bool present=false,Time best=0,bool waiting=false,uint32_t classified=0,uint32_t penalty=0,uint8_t laneValue=0,uint32_t positionValue=0,uint32_t behind=0):raceEntryId(id),laps(lap),classifiedLaps(classified),lapPenalty(penalty),position(positionValue),lapsBehind(behind),lane(laneValue),lastLapTime(time),bestLapTime(best),hasLap(present),waitingForTimingOrigin(waiting){} };
struct NoticeboardState {
  SessionLifecycle lifecycle=SessionLifecycle::Ready;SessionMode sessionMode=SessionMode::None;uint32_t proposalRevision=0;uint32_t raceEntryId=0,laps=0,lapTarget=0;Time lastLapTime=0,scheduledGo=0,pauseEffectiveAt=0,scheduledRestartAt=0,sessionFastestLap=0,durationExpiryAt=0,remainingDuration=0,overtime=0;uint16_t durationMinutes=0;bool durationExpired=false;LapFinishBehaviour finishBehaviour=LapFinishBehaviour::Immediate;RestartMethod restartMethod=RestartMethod::None;bool hasLap=false;NoticeboardEntryState entries[PP_MAX_ENTRIES]{};uint8_t entryCount=0;bool deadHeat=false,raceIntegrityFaulted=false,resultValid=true;RaceIntegrityReason raceIntegrityReason=RaceIntegrityReason::None;bool resultSealed=false;Time winningTime=0,finishTime=0,fastestLap=0;uint32_t historySequence=0;bool persistencePending=false,persistenceFault=false;uint8_t redLightCount=0,redLightsLit=0,startSignal=0;Time redIntervalUs=0,finalDelayUs=0;StartPresentationPhase startPhase=StartPresentationPhase::None;Time startPhaseUntil=0,finishDisplayUntil=0,finishDisplayRemaining=0;uint8_t finishDisplayDurationSeconds=5;bool finishDisplayActive=false,practiceSummaryAvailable=false;
};
class Noticeboard {
public:
  Noticeboard(const RaceControlModule& c,const RaceEngineModule&e,const ActiveSessionDefinition&a):control_(c),engine_(e),active_(a){}
  void setSession(const SessionDefinition& d){definition_=&d;}
  NoticeboardState current()const{return currentAt(systemTime());}
  NoticeboardState currentAt(Time now)const{
    NoticeboardState s{};s.lifecycle=control_.state();s.sessionMode=control_.mode();s.proposalRevision=control_.proposalRevision();s.scheduledGo=control_.scheduledGo();s.pauseEffectiveAt=control_.pauseEffectiveAt();s.scheduledRestartAt=control_.scheduledRestartAt();s.restartMethod=control_.restartMethod();s.redLightCount=control_.redLightCount();s.redLightsLit=control_.redLightsLit(now);s.startSignal=control_.startSignal();s.redIntervalUs=control_.definition()?control_.definition()->redIntervalUs():0;s.finalDelayUs=control_.finalDelay();s.lapTarget=control_.definition()?control_.definition()->lapTarget():(control_.proposedRaceSetup()?control_.proposedRaceSetup()->lapTarget:0);s.entryCount=engine_.entryCount();s.durationMinutes=control_.definition()?control_.definition()->durationMinutes():0;s.durationExpiryAt=control_.durationExpiryAt();s.remainingDuration=control_.durationRemaining(now);s.durationExpired=control_.durationExpired();s.finishBehaviour=control_.definition()?control_.definition()->finishBehaviour():LapFinishBehaviour::Immediate;
    for(uint8_t i=0;i<s.entryCount;i++){const auto&e=engine_.entryState(i);const uint8_t lane=control_.definition()?control_.definition()->entry(i).lane:uint8_t(i+1);s.entries[i]=NoticeboardEntryState(e.raceEntryId,e.laps,e.lastLapTime,e.hasLap,e.bestLapTime,e.waitingForTimingOrigin,e.laps>e.lapPenalty?e.laps-e.lapPenalty:0,e.lapPenalty,lane,engine_.livePosition(i),engine_.liveLapsBehind(i));if(e.bestLapTime&&(!s.sessionFastestLap||e.bestLapTime<s.sessionFastestLap))s.sessionFastestLap=e.bestLapTime;}
    if(s.entryCount){s.raceEntryId=s.entries[0].raceEntryId;s.laps=s.entries[0].laps;s.lastLapTime=s.entries[0].lastLapTime;s.hasLap=s.entries[0].hasLap;}
    selectLatestLap(s,engine_);
    s.deadHeat=engine_.deadHeat();s.raceIntegrityFaulted=control_.integrityFaulted();s.raceIntegrityReason=control_.integrityReason();s.resultValid=!s.raceIntegrityFaulted;
    const auto&r=engine_.completedResult();s.resultSealed=r.sealed;s.winningTime=r.winningTime;s.finishTime=r.finishTime;s.fastestLap=r.fastestLap;s.overtime=r.overtime&&r.finishTime>r.expiryTime?r.finishTime-r.expiryTime:0;
    // During Finish Current Lap the authoritative expiry boundary is already
    // settled, but the result is not sealed until the eligible lap arrives.
    // Expose live overtime from the same P&P clock so Browser presentation can
    // show it without creating a second timing authority.
    if(s.sessionMode==SessionMode::Endurance&&s.finishBehaviour==LapFinishBehaviour::CompleteCurrentLap&&s.durationExpired&&!s.resultSealed&&s.durationExpiryAt){s.overtime=now>s.durationExpiryAt?now-s.durationExpiryAt:0;}
    s.historySequence=engine_.historySequence();s.persistencePending=engine_.persistencePending();s.persistenceFault=engine_.persistenceFault();s.startPhase=control_.startPresentationPhase(now);s.startPhaseUntil=control_.scheduledGo();s.finishDisplayActive=control_.finishDisplayActive(now);s.finishDisplayUntil=control_.finishDisplayUntil();s.finishDisplayRemaining=control_.finishDisplayRemaining(now);s.finishDisplayDurationSeconds=control_.finishDisplayDurationSeconds();s.practiceSummaryAvailable=engine_.practiceSummary().available;return s;
  }
  const RaceEngineModule::CompletedRaceResult& completedResult() const{return engine_.completedResult();}
  const ProposedRaceSetup* proposedRaceSetup() const{return control_.proposedRaceSetup();}
  StartReadiness readiness() const{return control_.readiness();}
  uint32_t proposalRevision() const{return control_.proposalRevision();}
  const RaceEngineModule::PracticeSummary& practiceSummary() const{return engine_.practiceSummary();}
private:
  static void selectLatestLap(NoticeboardState&s,const RaceEngineModule&e) __attribute__((noinline)){Time latest=0;for(uint8_t i=0;i<s.entryCount;i++){const auto&entry=e.entryState(i);if(entry.hasLap&&entry.recordCount&&entry.records[entry.recordCount-1].finishTime>=latest){latest=entry.records[entry.recordCount-1].finishTime;s.raceEntryId=entry.raceEntryId;s.laps=entry.laps;s.lastLapTime=entry.lastLapTime;s.hasLap=true;}}}
  const RaceControlModule&control_;const RaceEngineModule&engine_;const ActiveSessionDefinition&active_;const SessionDefinition*definition_=nullptr;
};
}
