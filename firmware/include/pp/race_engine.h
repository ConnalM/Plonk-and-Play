#pragma once
#include "core.h"
#include "session_definition.h"
#include "history_store.h"
#include "record_store.h"

namespace pp {

// Race Engine owns factual crossings and lap timing for every active entry.
// Endurance adds a duration boundary; it does not create a second timing path.
class RaceEngineModule {
public:
  static constexpr uint8_t MaxEntries=PP_MAX_ENTRIES, MaxLaps=16;
  static constexpr uint16_t ResultFormatVersion=3;
  struct LapRecord { uint32_t lapNumber=0; Time startTime=0,finishTime=0,lapTime=0; Time racingFinishTime=0; bool valid=false; };
  struct EntryState {
    uint32_t raceEntryId=0,mugId=0,laps=0; Time lastCrossing=0,lastLapTime=0,bestLapTime=0;
    bool hasLap=false,waitingForTimingOrigin=false,timingOriginEstablished=false,postExpiryCompleted=false;
    uint32_t lapPenalty=0; LapRecord records[MaxLaps]{}; uint8_t recordCount=0;
  };
  struct ResultEntry {
    uint32_t raceEntryId=0,mugId=0,laps=0,classifiedLaps=0,rank=0,lapsBehind=0,lapPenalty=0;
    uint8_t lane=0; bool tied=false,completed=false; Time completionTime=0,bestLap=0;
    LapRecord records[MaxLaps]{}; uint8_t recordCount=0;
  };
  struct PracticeSummaryEntry { uint32_t raceEntryId=0,mugId=0; uint8_t lane=0; uint32_t laps=0; Time lastLap=0,bestLap=0; };
  struct PracticeSummary { uint16_t formatVersion=1; bool available=false; uint32_t sessionId=0; Time endedAt=0; PracticeSummaryEntry entries[MaxEntries]{}; uint8_t entryCount=0; Time sessionFastestLap=0; uint32_t sessionFastestEntryId=0; };
  struct CompletedRaceResult {
    uint16_t formatVersion=ResultFormatVersion; bool sealed=false,valid=false,deadHeat=false,fastestLapTied=false;
    Time winningTime=0,finishTime=0,expiryTime=0,durationUs=0; bool overtime=false;
    LapFinishBehaviour behaviour=LapFinishBehaviour::Immediate; SessionMode mode=SessionMode::LapRace;
    uint16_t durationMinutes=0; ResultEntry entries[MaxEntries]{}; uint8_t entryCount=0;
    Time fastestLap=0; uint32_t fastestEntryId=0,lapTarget=0;
  };
  RaceEngineModule(Bus&b,Bus::Endpoint e,ActiveSessionDefinition&a,HistoryStore*h=nullptr,TrackRecordStore*r=nullptr):bus_(b),endpoint_(e),active_(a),history_(h),records_(r){}
  void resetForFixture(){definition_=nullptr;reset(0);}
  void begin(const SessionDefinition&d,Time go){definition_=&d;observedRevision_=active_.revision();reset(go);}
  void prepare(const SessionDefinition&d){definition_=&d;observedRevision_=active_.revision();reset(0);}
  void tick(){
    // Race Control clears the active definition as part of accepting
    // Practice END SESSION.  Keep the old definition for this one delivery
    // boundary so the authoritative operation can capture its session-local
    // summary before the engine is cleared.  Other messages from the retired
    // session are discarded; they must not mutate a new/empty session.
    const bool retiredDefinition=active_.revision()!=observedRevision_&&active_.current()==nullptr&&definition_!=nullptr;
    if(!retiredDefinition)synchronise();
    Message m;
    while(bus_.receive(endpoint_,m)) {
      if(retiredDefinition&&m.type!=Type::SessionOperation)continue;
      if(m.type==Type::GoScheduled) scheduled(m);
      else if(m.type==Type::InputEvent) queue(m);
      else if(m.type==Type::SessionOperation) operation(m);
      else if(m.type==Type::EnduranceExpired) enduranceExpired(m);
      else if(m.type==Type::InputSettlement){settlementSeen_=true;settlementAt_=m.relevantTime;}
      else if(m.type==Type::FinishSettled) finishSettled_=m.relevantTime==candidateF_;
      else if(m.type==Type::RaceIntegrityFault) faulted_=true;
    }
    if(retiredDefinition){definition_=nullptr;observedRevision_=active_.revision();reset(0);}
    process(); publishPauseSettlement(); publishFinishSettlement(); completeAfterSettlement(); persistResult();
  }
  uint32_t laps()const{return entries_[0].laps;} Time lastLapTime()const{return entries_[0].lastLapTime;}
  bool complete()const{return complete_;} Time go()const{return go_;} uint8_t entryCount()const{return definition_?definition_->entryCount():0;}
  const EntryState&entryState(uint8_t i)const{return entries_[i];} bool deadHeat()const{return deadHeat_;} bool faulted()const{return faulted_;}
  const CompletedRaceResult&completedResult()const{return result_;} uint32_t historySequence()const{return historySequence_;}
  const PracticeSummary& practiceSummary()const{return practiceSummary_;}
  // Competitive live ordering is an authoritative Race Engine view.  It uses
  // the same completed-progress and finish-time comparison as sealed results;
  // Practice deliberately has no competitive position or gap.
  uint32_t livePosition(uint8_t index)const{
    if(!definition_||definition_->mode()==SessionMode::OpenPractice||index>=entryCount())return 0;
    uint32_t position=1;for(uint8_t i=0;i<entryCount();++i)if(i!=index&&beforeLive(entries_[i],entries_[index],finishBehaviour_))++position;return position;
  }
  uint32_t liveLapsBehind(uint8_t index)const{
    if(!definition_||definition_->mode()==SessionMode::OpenPractice||index>=entryCount())return 0;
    uint32_t leader=0;for(uint8_t i=0;i<entryCount();++i){const auto&e=entries_[i];const uint32_t classified=e.laps>e.lapPenalty?e.laps-e.lapPenalty:0;if(classified>leader)leader=classified;}
    const auto&e=entries_[index];const uint32_t classified=e.laps>e.lapPenalty?e.laps-e.lapPenalty:0;return leader>classified?leader-classified:0;
  }
  enum class LiveGapKind : uint8_t { None=0, Laps=1, Time=2 };
  LiveGapKind liveGapKind(uint8_t index)const {
    if(!definition_||definition_->mode()==SessionMode::OpenPractice||index>=entryCount())return LiveGapKind::None;
    uint32_t leader=0;for(uint8_t i=0;i<entryCount();++i){const auto&e=entries_[i];const uint32_t classified=e.laps>e.lapPenalty?e.laps-e.lapPenalty:0;if(classified>leader)leader=classified;}
    const auto&entry=entries_[index];const uint32_t classified=entry.laps>entry.lapPenalty?entry.laps-entry.lapPenalty:0;if(classified<leader)return LiveGapKind::Laps;
    if(classified==0)return LiveGapKind::None;
    uint8_t leaders=0,leaderIndex=MaxEntries;for(uint8_t i=0;i<entryCount();++i)if(livePosition(i)==1){++leaders;leaderIndex=i;}
    if(leaders!=1||leaderIndex==index)return LiveGapKind::None;
    return hasCrossingForClassified(leaderIndex,leader)&&hasCrossingForClassified(index,leader)?LiveGapKind::Time:LiveGapKind::None;
  }
  Time liveGapValue(uint8_t index)const {
    if(liveGapKind(index)!=LiveGapKind::Time)return 0;const uint32_t level=entries_[index].laps>entries_[index].lapPenalty?entries_[index].laps-entries_[index].lapPenalty:0;const Time leader=crossingForClassified(leaderForGap(),level),current=crossingForClassified(index,level);return current>=leader?current-leader:0;
  }
  bool persistencePending()const{return result_.sealed&&!persisted_;} bool persistenceFault()const{return persistenceFault_||recordPersistenceFault_;}
  bool settlementSeen()const{return settlementSeen_;} bool settlementPublished()const{return settlementPublished_;}
#if defined(PP_STAGE14C_ACCEPTANCE)
  // Acceptance-only boundary hook: model an entry with no legitimate timing
  // origin at expiry without adding a production operation or timing path.
  void acceptanceClearTimingOrigin(uint8_t i){if(i<entryCount())entries_[i].timingOriginEstablished=false;}
#endif

private:
  void reset(Time go){
    go_=go; pauseAt_=restartAt_=settlementAt_=practiceResumeAt_=winningTime_=candidateF_=expiryTime_=resumeAt_=0;
    winner_=0;complete_=completionPublished_=deadHeat_=faulted_=paused_=restartScheduled_=gridRestart_=false;pausedDuration_=0;pauseOpen_=false;
    enduranceExpired_=finishCurrentPending_=settlementSeen_=settlementPublished_=finishReady_=finishSettlementSent_=finishSettled_=false;
    persisted_=persistenceFault_=recordPersistenceFault_=storageFaultPublished_=false;historyPersistFailed_=false;pendingCount_=seenAt_=0;for(auto&x:seen_)x=0;for(auto&o:observed_)o=false;for(auto&e:entries_)e={};
    result_={}; finishBehaviour_=definition_?definition_->finishBehaviour():LapFinishBehaviour::Immediate;
    recordEligible_=definition_&&(definition_->mode()==SessionMode::LapRace||definition_->mode()==SessionMode::Endurance);
#if defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE)
    recordEligible_=false;
#endif
    if(definition_)for(uint8_t i=0;i<entryCount();++i){entries_[i].raceEntryId=definition_->entry(i).raceEntryId;entries_[i].mugId=definition_->entry(i).mugId;entries_[i].waitingForTimingOrigin=definition_->mode()==SessionMode::OpenPractice;}
  }
  void synchronise(){if(active_.revision()!=observedRevision_){definition_=active_.current();observedRevision_=active_.revision();if(definition_&&definition_->mode()==SessionMode::OpenPractice)practiceSummary_={};reset(0);}}
  void scheduled(const Message&m){if(!definition_||!m.relevantTime)return;go_=m.relevantTime;for(uint8_t i=0;i<entryCount();++i){entries_[i].lastCrossing=go_;if(definition_->mode()==SessionMode::Endurance)entries_[i].timingOriginEstablished=true;}}
  void enduranceExpired(const Message&m){if(!definition_||definition_->mode()!=SessionMode::Endurance)return;enduranceExpired_=true;expiryTime_=m.relevantTime;for(uint8_t i=0;i<entryCount();++i){if(!entries_[i].timingOriginEstablished||entries_[i].postExpiryCompleted)observed_[i]=true;}if(finishBehaviour_==LapFinishBehaviour::Immediate||finishBehaviour_==LapFinishBehaviour::CompleteFullRaceDistance){candidateF_=expiryTime_;finishReady_=true;}else finishCurrentPending_=true;publishNoticeboardChanged();}
  void operation(const Message&m){
    if(m.operation==SessionOperation::Pause){pauseAt_=m.relevantTime;pauseOpen_=true;paused_=true;settlementSeen_=settlementPublished_=false;settlementAt_=m.relevantTime;return;}
    if(definition_&&definition_->mode()==SessionMode::OpenPractice&&m.operation==SessionOperation::Resume){closePause(m.relevantTime);paused_=false;practiceResumeAt_=m.relevantTime;for(uint8_t i=0;i<entryCount();++i)entries_[i].waitingForTimingOrigin=true;return;}
    if(definition_&&definition_->mode()==SessionMode::OpenPractice&&m.operation==SessionOperation::EndSession){capturePracticeSummary(m.relevantTime);paused_=true;return;}
    if(definition_&&definition_->mode()==SessionMode::Endurance&&m.operation==SessionOperation::Resume){closePause(m.relevantTime);paused_=false;resumeAt_=m.relevantTime;return;}
    closePause(m.relevantTime);
    settlementSeen_=settlementPublished_=false;settlementAt_=0;restartScheduled_=true;restartAt_=m.relevantTime;gridRestart_=m.restartMethod==RestartMethod::Grid;paused_=false;if(gridRestart_)for(uint8_t i=0;i<entryCount();++i)entries_[i].lastCrossing=restartAt_;
  }
  void queue(const Message&m){if(!definition_||complete_||faulted_||(!go_&&definition_->mode()!=SessionMode::OpenPractice)||!m.eventId||seen(m.eventId)||pendingCount_==16)return;seen_[seenAt_++%16]=m.eventId;pending_[pendingCount_++]=m;}
  bool seen(uint32_t id)const{for(auto x:seen_)if(x==id)return true;return false;}
  void process(){if(!definition_||complete_||faulted_)return;for(uint8_t i=1;i<pendingCount_;++i){Message x=pending_[i];int j=i-1;while(j>=0&&pending_[j].relevantTime>x.relevantTime){pending_[j+1]=pending_[j];--j;}pending_[j+1]=x;}for(uint8_t i=0;i<pendingCount_;++i)interpret(pending_[i]);pendingCount_=0;evaluateFinish();}
  uint8_t indexOf(uint32_t id)const{for(uint8_t i=0;i<entryCount();++i)if(entries_[i].raceEntryId==id)return i;return MaxEntries;}
  void interpret(const Message&m){
    if(finishReady_&&m.relevantTime>candidateF_)return;if(restartScheduled_&&m.relevantTime<restartAt_)return;
    SessionInputRole role;RaceEntryDefinition def;if(!definition_||!definition_->resolve(m.input,role,def)||role.role!=InputRole::StartFinish)return;const uint8_t i=indexOf(def.raceEntryId);if(i>=entryCount())return;EntryState&s=entries_[i];
    if(definition_->mode()==SessionMode::OpenPractice){if(paused_&&m.relevantTime>=pauseAt_)return;if(practiceResumeAt_&&m.relevantTime<practiceResumeAt_)return;if(s.waitingForTimingOrigin){s.lastCrossing=m.relevantTime;s.waitingForTimingOrigin=false;return;}if(m.relevantTime<s.lastCrossing)return;const Time lap=m.relevantTime-s.lastCrossing;s.lastCrossing=m.relevantTime;s.lastLapTime=lap;s.hasLap=true;if(!s.bestLapTime||lap<s.bestLapTime)s.bestLapTime=lap;++s.laps;storeLap(s,lap,m.relevantTime);publishLap(def,s,lap,m);return;}
    if(definition_->falseStartCapability()&&m.relevantTime<go_){Message fs{};fs.type=Type::FalseStart;fs.relevantTime=m.relevantTime;fs.input=m.input;fs.raceEntryId=def.raceEntryId;fs.probe=definition_->falseStartPolicy();bus_.publish(endpoint_,fs);if(definition_->falseStartPolicy()==2|| (definition_->mode()==SessionMode::Endurance&&definition_->falseStartPolicy()==3)){++s.lapPenalty;penalties_[i]=s.lapPenalty;}publishNoticeboardChanged();return;}
    if(definition_->mode()==SessionMode::Endurance){interpretEndurance(m,def,s);return;}
    if(m.relevantTime<s.lastCrossing||(paused_&&m.relevantTime>=pauseAt_))return;const Time lapStart=s.lastCrossing;Time lap=m.relevantTime-lapStart;if(restartScheduled_&&!gridRestart_&&m.relevantTime>=restartAt_&&s.lastCrossing<pauseAt_){const Time stopped=restartAt_-pauseAt_;lap=lap>stopped?lap-stopped:0;}s.lastCrossing=m.relevantTime;s.lastLapTime=lap;s.hasLap=true;++s.laps;storeLap(s,lap,m.relevantTime);if(recordEligible_&&records_&&!records_->observe(def.lane,lap))latchPersistenceFault(m.relevantTime,true);publishLap(def,s,lap,m);applyFinish(i,m.relevantTime);
  }
  void interpretEndurance(const Message&m,const RaceEntryDefinition&def,EntryState&s){
    if(paused_&&m.relevantTime>=pauseAt_)return;if(m.relevantTime<s.lastCrossing)return;
    if(resumeAt_&&m.relevantTime<resumeAt_)return;
    const bool after=expiryTime_&&m.relevantTime>expiryTime_, at=expiryTime_&&m.relevantTime==expiryTime_;
    if(enduranceExpired_&&after){if(finishBehaviour_!=LapFinishBehaviour::CompleteCurrentLap||!s.timingOriginEstablished||s.postExpiryCompleted)return;s.postExpiryCompleted=true;completeEnduranceEntry(s,m,def);return;}
    if(enduranceExpired_&&!at)return;if(after&&finishBehaviour_!=LapFinishBehaviour::CompleteCurrentLap)return;
    Time lap=m.relevantTime-s.lastCrossing;if(resumeAt_&&s.lastCrossing<pauseAt_){const Time stopped=resumeAt_-pauseAt_;lap=lap>stopped?lap-stopped:0;}
    s.lastCrossing=m.relevantTime;s.lastLapTime=lap;s.hasLap=true;++s.laps;storeLap(s,lap,m.relevantTime);if(recordEligible_&&records_&&!records_->observe(def.lane,lap))latchPersistenceFault(m.relevantTime,true);publishLap(def,s,lap,m);
    if(after||at){if(after)s.postExpiryCompleted=true;if(enduranceExpired_&&finishBehaviour_==LapFinishBehaviour::CompleteCurrentLap){observed_[indexOf(s.raceEntryId)]=true;candidateF_=m.relevantTime;finishReady_=allObserved();}return;}
  }
  // The first eligible crossing after expiry completes the lap that was in
  // progress.  It is a real factual lap, so it must update the same counters,
  // immutable detail record, PB/record path and Browser fact as an ordinary
  // lap.  The per-entry postExpiryCompleted fence prevents any later crossing
  // from starting or completing another lap.
  void completeEnduranceEntry(EntryState&s,const Message&m,const RaceEntryDefinition&def){
    if(m.relevantTime<s.lastCrossing)return;
    const Time lap=m.relevantTime-s.lastCrossing;
    s.lastCrossing=m.relevantTime;s.lastLapTime=lap;s.hasLap=true;++s.laps;
    storeLap(s,lap,m.relevantTime);
    if(recordEligible_&&records_&&lap&&!records_->observe(def.lane,lap))latchPersistenceFault(m.relevantTime,true);
    publishLap(def,s,lap,m);
    uint8_t i=indexOf(s.raceEntryId);if(i<entryCount())observed_[i]=true;
    candidateF_=m.relevantTime;finishReady_=allObserved();
  }
  void storeLap(EntryState&s,Time lap,Time at){if(s.recordCount<MaxLaps){LapRecord&r=s.records[s.recordCount++];r.lapNumber=s.laps;r.startTime=at-lap;r.finishTime=at;r.lapTime=lap;r.racingFinishTime=racingTime(at);r.valid=true;}if(!s.bestLapTime||lap<s.bestLapTime)s.bestLapTime=lap;}
  void capturePracticeSummary(Time at){practiceSummary_={};practiceSummary_.formatVersion=1;practiceSummary_.available=true;practiceSummary_.sessionId=definition_?definition_->sessionId():0;practiceSummary_.endedAt=at;practiceSummary_.entryCount=entryCount();for(uint8_t i=0;i<entryCount();++i){const auto&s=entries_[i];auto&out=practiceSummary_.entries[i];out.raceEntryId=s.raceEntryId;out.mugId=definition_->entry(i).mugId;out.lane=definition_->entry(i).lane;out.laps=s.laps;out.lastLap=s.lastLapTime;out.bestLap=s.bestLapTime;if(s.bestLapTime&&(!practiceSummary_.sessionFastestLap||s.bestLapTime<practiceSummary_.sessionFastestLap)){practiceSummary_.sessionFastestLap=s.bestLapTime;practiceSummary_.sessionFastestEntryId=s.raceEntryId;}}}
  void publishLap(const RaceEntryDefinition&def,const EntryState&s,Time lap,const Message&m){Message f{};f.type=Type::LapCompleted;f.relevantTime=m.relevantTime;f.raceEntryId=def.raceEntryId;f.lapNumber=s.laps;f.lapTime=lap;f.eventId=m.eventId;bus_.publish(endpoint_,f);publishNoticeboardChanged();}
  bool allObserved()const{for(uint8_t i=0;i<entryCount();++i)if(!observed_[i])return false;return true;}
  void applyFinish(uint8_t i,Time at){EntryState&s=entries_[i];const bool target=s.laps>=definition_->lapTarget()+penalties_[i];if(!winningTime_&&target){winningTime_=at;winner_=s.raceEntryId;observed_[i]=true;if(finishBehaviour_==LapFinishBehaviour::Immediate){candidateF_=at;finishReady_=true;}return;}if(!winningTime_)return;if(at==winningTime_&&target){observed_[i]=true;deadHeat_=true;return;}if(at<=winningTime_||observed_[i])return;if(finishBehaviour_==LapFinishBehaviour::CompleteCurrentLap||(finishBehaviour_==LapFinishBehaviour::CompleteFullRaceDistance&&target)){observed_[i]=true;candidateF_=at;if(allObserved())finishReady_=true;}}
  void evaluateFinish(){if(definition_&&definition_->mode()==SessionMode::Endurance){if(enduranceExpired_&&finishBehaviour_==LapFinishBehaviour::CompleteCurrentLap&&allObserved()){finishReady_=true;if(!candidateF_)candidateF_=expiryTime_;}return;}if(!winningTime_||finishReady_)return;if(finishBehaviour_==LapFinishBehaviour::Immediate){candidateF_=winningTime_;finishReady_=true;}else if(allObserved()){if(candidateF_<winningTime_)candidateF_=winningTime_;finishReady_=true;}}
  void publishPauseSettlement(){if(!settlementSeen_||settlementPublished_||pendingCount_)return;if(definition_&&definition_->mode()==SessionMode::OpenPractice){for(uint8_t i=0;i<entryCount();++i){entries_[i].waitingForTimingOrigin=true;entries_[i].lastCrossing=0;}}Message m{};m.type=Type::PauseSettled;m.relevantTime=settlementAt_;if(bus_.publish(endpoint_,m)==Delivery::Delivered)settlementPublished_=true;}
  void publishFinishSettlement(){if(!finishReady_||finishSettlementSent_||pendingCount_)return;Message m{};m.type=Type::FinishSettlement;m.relevantTime=candidateF_;if(bus_.publish(endpoint_,m)==Delivery::Delivered)finishSettlementSent_=true;}
  void completeAfterSettlement(){
#if defined(PP_STAGE6_ACCEPTANCE)
    if(finishReady_&&!completionPublished_&&!finishSettlementSent_){if(!result_.sealed)seal();Message m{};m.type=Type::CompetitionComplete;m.relevantTime=result_.finishTime;m.raceEntryId=winner_;m.probe=deadHeat_?1:0;if(bus_.publish(endpoint_,m)==Delivery::Delivered){completionPublished_=true;complete_=true;}return;}
#endif
    if(!finishSettled_||completionPublished_||!finishSettlementSent_||!definition_)return;
    if(definition_->mode()!=SessionMode::Endurance){const uint8_t w=indexOf(winner_);if(w>=entryCount()||entries_[w].laps<definition_->lapTarget()+penalties_[w])return;}
    if(!result_.sealed)seal();Message m{};m.type=Type::CompetitionComplete;m.relevantTime=result_.finishTime;m.raceEntryId=winner_;m.probe=deadHeat_?1:0;if(bus_.publish(endpoint_,m)==Delivery::Delivered){completionPublished_=true;complete_=true;}
  }
  void latchPersistenceFault(Time at,bool record){if(record)recordPersistenceFault_=true;else historyPersistFailed_=true;persistenceFault_=true;if(storageFaultPublished_)return;storageFaultPublished_=true;Message fault{};fault.type=Type::StorageFault;fault.relevantTime=at;bus_.publish(endpoint_,fault);publishNoticeboardChanged();}
  void persistResult(){if(!result_.sealed||persisted_||historyPersistFailed_||!history_)return;static_assert(sizeof(CompletedRaceResult)<=HistoryStore::MaxBytes,"history result exceeds bounded store");uint32_t sequence=0;if(history_->append(reinterpret_cast<const uint8_t*>(&result_),sizeof(result_),sequence)){historySequence_=sequence;persisted_=true;Message stored{};stored.type=Type::HistoryStored;stored.historySequence=sequence;stored.relevantTime=result_.finishTime;bus_.publish(endpoint_,stored);publishNoticeboardChanged();}else{latchPersistenceFault(result_.finishTime,false);}}
  static bool before(const ResultEntry&a,const ResultEntry&b,LapFinishBehaviour f){if(a.classifiedLaps!=b.classifiedLaps)return a.classifiedLaps>b.classifiedLaps;if(f==LapFinishBehaviour::Immediate)return false;if(!a.completionTime)return false;if(!b.completionTime)return true;return a.completionTime<b.completionTime;}
  static bool beforeLive(const EntryState&a,const EntryState&b,LapFinishBehaviour f){const uint32_t ac=a.laps>a.lapPenalty?a.laps-a.lapPenalty:0;const uint32_t bc=b.laps>b.lapPenalty?b.laps-b.lapPenalty:0;if(ac!=bc)return ac>bc;if(f==LapFinishBehaviour::Immediate)return false;if(!a.lastCrossing)return false;if(!b.lastCrossing)return true;return a.lastCrossing<b.lastCrossing;}
  static bool samePlace(const ResultEntry&a,const ResultEntry&b,LapFinishBehaviour f){return a.classifiedLaps==b.classifiedLaps&&(f==LapFinishBehaviour::Immediate||a.completionTime==b.completionTime);}
  void publishNoticeboardChanged(){Message n{};n.type=Type::NoticeboardChanged;bus_.publish(endpoint_,n);}
  void seal(){result_={};result_.formatVersion=ResultFormatVersion;result_.sealed=true;result_.valid=!faulted_;result_.deadHeat=deadHeat_;result_.winningTime=winningTime_;result_.finishTime=candidateF_;result_.expiryTime=expiryTime_;result_.overtime=expiryTime_&&candidateF_>expiryTime_;result_.durationUs=definition_?definition_->durationUs():0;result_.durationMinutes=definition_?definition_->durationMinutes():0;result_.mode=definition_?definition_->mode():SessionMode::LapRace;result_.behaviour=finishBehaviour_;result_.entryCount=entryCount();result_.lapTarget=definition_?definition_->lapTarget():0;uint32_t leader=0;
    for(uint8_t i=0;i<entryCount();++i){auto&t=result_.entries[i];const auto&s=entries_[i];t.raceEntryId=s.raceEntryId;t.mugId=definition_->entry(i).mugId;t.lane=definition_->entry(i).lane;t.laps=s.laps;t.lapPenalty=s.lapPenalty;t.classifiedLaps=s.laps>s.lapPenalty?s.laps-s.lapPenalty:0;t.completed=observed_[i];t.completionTime=t.completed?s.lastCrossing:0;t.recordCount=s.recordCount;if(t.classifiedLaps>leader)leader=t.classifiedLaps;for(uint8_t j=0;j<s.recordCount;++j){t.records[j]=s.records[j];if(s.records[j].valid&&(!t.bestLap||s.records[j].lapTime<t.bestLap))t.bestLap=s.records[j].lapTime;}}
    for(uint8_t i=1;i<entryCount();++i){ResultEntry v=result_.entries[i];int j=int(i)-1;while(j>=0&&before(v,result_.entries[j],finishBehaviour_)){result_.entries[j+1]=result_.entries[j];--j;}result_.entries[j+1]=v;}
    uint32_t rank=1;for(uint8_t i=0;i<entryCount();++i){if(i&&!samePlace(result_.entries[i-1],result_.entries[i],finishBehaviour_))rank=i+1;auto&t=result_.entries[i];t.rank=rank;t.tied=i&&result_.entries[i-1].rank==rank;if(t.tied)result_.entries[i-1].tied=true;t.lapsBehind=leader-t.classifiedLaps;for(uint8_t j=0;j<t.recordCount;++j){const auto&lap=t.records[j];if(!lap.valid)continue;if(!result_.fastestLap||lap.lapTime<result_.fastestLap){result_.fastestLap=lap.lapTime;result_.fastestEntryId=t.raceEntryId;result_.fastestLapTied=false;}else if(lap.lapTime==result_.fastestLap&&result_.fastestEntryId!=t.raceEntryId)result_.fastestLapTied=true;}}
  }
  Time racingTime(Time raw)const{return raw>=pausedDuration_?raw-pausedDuration_:0;}
  void closePause(Time at){if(pauseOpen_&&at>=pauseAt_){pausedDuration_+=at-pauseAt_;pauseOpen_=false;}}
  bool hasCrossingForClassified(uint8_t index,uint32_t classified)const{if(index>=entryCount()||!classified)return false;const auto&s=entries_[index];const uint32_t factual=classified+s.lapPenalty;for(uint8_t j=0;j<s.recordCount;++j)if(s.records[j].valid&&s.records[j].lapNumber==factual)return true;return false;}
  Time crossingForClassified(uint8_t index,uint32_t classified)const{if(!hasCrossingForClassified(index,classified))return 0;const auto&s=entries_[index];const uint32_t factual=classified+s.lapPenalty;for(uint8_t j=0;j<s.recordCount;++j)if(s.records[j].valid&&s.records[j].lapNumber==factual)return s.records[j].racingFinishTime;return 0;}
  uint8_t leaderForGap()const{for(uint8_t i=0;i<entryCount();++i)if(livePosition(i)==1)return i;return MaxEntries;}
  Bus&bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition&active_;HistoryStore*history_=nullptr;TrackRecordStore*records_=nullptr;const SessionDefinition*definition_=nullptr;
  uint8_t penalties_[MaxEntries]{},observed_[MaxEntries]{};uint32_t observedRevision_=0;Time go_=0,pauseAt_=0,restartAt_=0,settlementAt_=0,practiceResumeAt_=0,winningTime_=0,candidateF_=0,expiryTime_=0,resumeAt_=0,pausedDuration_=0;bool pauseOpen_=false;
  EntryState entries_[MaxEntries]{};LapFinishBehaviour finishBehaviour_=LapFinishBehaviour::Immediate;bool complete_=false,completionPublished_=false,deadHeat_=false,faulted_=false,paused_=false,restartScheduled_=false,gridRestart_=false,enduranceExpired_=false,finishCurrentPending_=false,settlementSeen_=false,settlementPublished_=false,finishReady_=false,finishSettlementSent_=false,finishSettled_=false,persisted_=false,persistenceFault_=false,recordPersistenceFault_=false,historyPersistFailed_=false,storageFaultPublished_=false,recordEligible_=true;
  CompletedRaceResult result_{};PracticeSummary practiceSummary_{};uint32_t winner_=0,historySequence_=0;Message pending_[16]{};uint8_t pendingCount_=0;uint32_t seen_[16]{};uint8_t seenAt_=0;
};
} // namespace pp
