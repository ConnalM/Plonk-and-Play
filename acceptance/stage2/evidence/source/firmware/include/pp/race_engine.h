#pragma once
#include "core.h"
#include "session_definition.h"
#include "history_store.h"
#include "record_store.h"
namespace pp {
class RaceEngineModule {
public:
  static constexpr uint8_t MaxEntries=PP_MAX_ENTRIES,MaxLaps=16;
  static constexpr uint16_t ResultFormatVersion=2;
  struct LapRecord{uint32_t lapNumber=0;Time startTime=0,finishTime=0,lapTime=0;bool valid=false;};
  struct EntryState{uint32_t raceEntryId=0,laps=0;Time lastCrossing=0,lastLapTime=0;bool hasLap=false;LapRecord records[MaxLaps]{};uint8_t recordCount=0;};
  struct ResultEntry{uint32_t raceEntryId=0,laps=0,rank=0,lapsBehind=0;uint8_t lane=0;bool tied=false,completed=false;Time completionTime=0,bestLap=0;LapRecord records[MaxLaps]{};uint8_t recordCount=0;};
  struct CompletedRaceResult{uint16_t formatVersion=ResultFormatVersion;bool sealed=false,valid=false,deadHeat=false,fastestLapTied=false;Time winningTime=0,finishTime=0;LapFinishBehaviour behaviour=LapFinishBehaviour::Immediate;ResultEntry entries[MaxEntries]{};uint8_t entryCount=0;Time fastestLap=0;uint32_t fastestEntryId=0;uint32_t lapTarget=0;};
  RaceEngineModule(Bus&b,Bus::Endpoint e,ActiveSessionDefinition&a,HistoryStore* history=nullptr,TrackRecordStore* records=nullptr):bus_(b),endpoint_(e),active_(a),history_(history),records_(records){}
  void resetForFixture(){definition_=nullptr;reset(0);} void begin(const SessionDefinition&d,Time go){definition_=&d;observedRevision_=active_.revision();reset(go);} void prepare(const SessionDefinition&d){definition_=&d;observedRevision_=active_.revision();reset(0);}
  void tick(){synchronise();Message m;while(bus_.receive(endpoint_,m)){if(m.type==Type::GoScheduled)scheduled(m);else if(m.type==Type::InputEvent)queue(m);else if(m.type==Type::SessionOperation)operation(m);else if(m.type==Type::InputSettlement){settlementSeen_=true;settlementAt_=m.relevantTime;}else if(m.type==Type::FinishSettled)finishSettled_=m.relevantTime==candidateF_;else if(m.type==Type::RaceIntegrityFault)faulted_=true;}process();publishPauseSettlement();publishFinishSettlement();completeAfterSettlement();persistResult();}
  uint32_t laps()const{return entries_[0].laps;}Time lastLapTime()const{return entries_[0].lastLapTime;}bool complete()const{return complete_;}Time go()const{return go_;}uint8_t entryCount()const{return definition_?definition_->entryCount():0;}const EntryState&entryState(uint8_t i)const{return entries_[i];}bool deadHeat()const{return deadHeat_;}bool faulted()const{return faulted_;}const CompletedRaceResult&completedResult()const{return result_;} uint32_t historySequence()const{return historySequence_;} bool persistencePending()const{return result_.sealed&&!persisted_;}bool persistenceFault()const{return persistenceFault_;}
private:
  void reset(Time go){go_=go;pauseAt_=restartAt_=settlementAt_=winningTime_=candidateF_=0;winner_=0;complete_=completionPublished_=deadHeat_=faulted_=paused_=restartScheduled_=gridRestart_=false;settlementSeen_=settlementPublished_=finishReady_=finishSettlementSent_=finishSettled_=false;persisted_=persistenceFault_=false;pendingCount_=seenAt_=0;for(auto&x:seen_)x=0;for(auto&e:entries_)e={};for(auto& p:penalties_)p=0;for(auto& o:observed_)o=false;result_={};finishBehaviour_=definition_?definition_->finishBehaviour():LapFinishBehaviour::Immediate;recordEligible_=true;
#if defined(PP_STAGE13_DEMO) || defined(PP_STAGE13_ACCEPTANCE)
    recordEligible_=false;
#endif
    if(definition_)for(uint8_t i=0;i<entryCount();++i)entries_[i].raceEntryId=definition_->entry(i).raceEntryId;}
  void synchronise(){if(active_.revision()!=observedRevision_){definition_=active_.current();observedRevision_=active_.revision();reset(0);}}
  void scheduled(const Message&m){if(definition_&&m.relevantTime){go_=m.relevantTime;for(uint8_t i=0;i<entryCount();++i)entries_[i].lastCrossing=go_;}}
  void operation(const Message&m){if(m.operation==SessionOperation::Pause){pauseAt_=m.relevantTime;paused_=true;return;}settlementSeen_=settlementPublished_=false;settlementAt_=0;restartScheduled_=true;restartAt_=m.relevantTime;gridRestart_=m.restartMethod==RestartMethod::Grid;paused_=false;if(gridRestart_)for(uint8_t i=0;i<entryCount();++i)entries_[i].lastCrossing=restartAt_;}
  void queue(const Message&m){if(!definition_||complete_||faulted_||!go_||!m.eventId||seen(m.eventId)||pendingCount_==16)return;seen_[seenAt_++%16]=m.eventId;pending_[pendingCount_++]=m;}
  bool seen(uint32_t id)const{for(auto x:seen_)if(x==id)return true;return false;}
  void process(){if(!definition_||complete_||faulted_)return;for(uint8_t i=1;i<pendingCount_;++i){Message x=pending_[i];int j=i-1;while(j>=0&&pending_[j].relevantTime>x.relevantTime){pending_[j+1]=pending_[j];--j;}pending_[j+1]=x;}for(uint8_t i=0;i<pendingCount_;++i)interpret(pending_[i]);pendingCount_=0;evaluateFinish();}
  uint8_t indexOf(uint32_t id)const{for(uint8_t i=0;i<entryCount();++i)if(entries_[i].raceEntryId==id)return i;return MaxEntries;}
  void interpret(const Message&m){if(finishReady_&&m.relevantTime>candidateF_)return;if(restartScheduled_&&m.relevantTime<restartAt_)return;if(definition_&&definition_->falseStartCapability()&&m.relevantTime<go_){SessionInputRole falseStartRole;RaceEntryDefinition falseStartEntry;if(!definition_->resolve(m.input,falseStartRole,falseStartEntry)||falseStartRole.role!=InputRole::StartFinish)return;Message fs{};fs.type=Type::FalseStart;fs.relevantTime=m.relevantTime;fs.input=m.input;fs.raceEntryId=falseStartEntry.raceEntryId;fs.probe=definition_->falseStartPolicy();bus_.publish(endpoint_,fs);if(definition_->falseStartPolicy()==2){const uint8_t penaltyIndex=indexOf(fs.raceEntryId);if(penaltyIndex<entryCount())++penalties_[penaltyIndex];}publishNoticeboardChanged();return;}SessionInputRole role;RaceEntryDefinition def;if(!definition_->resolve(m.input,role,def)||role.role!=InputRole::StartFinish)return;const uint8_t i=indexOf(def.raceEntryId);if(i>=entryCount())return;EntryState&s=entries_[i];if(m.relevantTime<s.lastCrossing||(paused_&&m.relevantTime>=pauseAt_))return;const Time lapStart=s.lastCrossing;Time lap=m.relevantTime-lapStart;if(restartScheduled_&&!gridRestart_&&m.relevantTime>=restartAt_&&s.lastCrossing<pauseAt_){const Time stopped=restartAt_-pauseAt_;lap=lap>stopped?lap-stopped:0;}s.lastCrossing=m.relevantTime;s.lastLapTime=lap;s.hasLap=true;++s.laps;if(s.recordCount<MaxLaps){LapRecord&r=s.records[s.recordCount++];r.lapNumber=s.laps;r.startTime=lapStart;r.finishTime=m.relevantTime;r.lapTime=lap;r.valid=true;}if(recordEligible_&&records_)records_->observe(def.lane,lap);Message f{};f.type=Type::LapCompleted;f.relevantTime=m.relevantTime;f.raceEntryId=def.raceEntryId;f.lapNumber=s.laps;f.lapTime=lap;f.eventId=m.eventId;bus_.publish(endpoint_,f);publishNoticeboardChanged();applyFinish(i,m.relevantTime);}
  bool allObserved()const{for(uint8_t i=0;i<entryCount();++i)if(!observed_[i])return false;return true;}
  void applyFinish(uint8_t i,Time at){EntryState&s=entries_[i];const bool target=s.laps>=definition_->lapTarget()+penalties_[i];if(!winningTime_&&target){winningTime_=at;winner_=s.raceEntryId;observed_[i]=true;if(finishBehaviour_==LapFinishBehaviour::Immediate){candidateF_=at;finishReady_=true;}return;}if(!winningTime_)return;if(at==winningTime_&&target){observed_[i]=true;deadHeat_=true;return;}if(at<=winningTime_||observed_[i])return;if(finishBehaviour_==LapFinishBehaviour::CompleteCurrentLap||(finishBehaviour_==LapFinishBehaviour::CompleteFullRaceDistance&&target)){observed_[i]=true;candidateF_=at;if(allObserved())finishReady_=true;}}
  void evaluateFinish(){if(!winningTime_||finishReady_)return;if(finishBehaviour_==LapFinishBehaviour::Immediate){candidateF_=winningTime_;finishReady_=true;}else if(allObserved()){if(candidateF_<winningTime_)candidateF_=winningTime_;finishReady_=true;}}
  void publishPauseSettlement(){if(!settlementSeen_||settlementPublished_||pendingCount_)return;Message m{};m.type=Type::PauseSettled;m.relevantTime=settlementAt_;if(bus_.publish(endpoint_,m)==Delivery::Delivered)settlementPublished_=true;}
  void publishFinishSettlement(){if(!finishReady_||finishSettlementSent_||pendingCount_)return;Message m{};m.type=Type::FinishSettlement;m.relevantTime=candidateF_;if(bus_.publish(endpoint_,m)==Delivery::Delivered)finishSettlementSent_=true;}
  void completeAfterSettlement(){
#if defined(PP_STAGE6_ACCEPTANCE)
    // Stage 6 predates the finish-settlement round trip. Preserve its frozen
    // direct completion contract; Stage 7+ use the settled path below.
    if(finishReady_&&!completionPublished_&&!finishSettlementSent_){if(!result_.sealed)seal();Message m{};m.type=Type::CompetitionComplete;m.relevantTime=result_.finishTime;m.raceEntryId=winner_;m.probe=deadHeat_?1:0;if(bus_.publish(endpoint_,m)==Delivery::Delivered){completionPublished_=true;complete_=true;}return;}
#endif
    if(!finishSettled_||completionPublished_||!finishSettlementSent_||!winningTime_||!definition_)return;
    const uint8_t winnerIndex=indexOf(winner_);
    if(winnerIndex>=entryCount() || entries_[winnerIndex].laps<definition_->lapTarget()+penalties_[winnerIndex])return;
    if(!result_.sealed)seal();
    Message m{};m.type=Type::CompetitionComplete;m.relevantTime=result_.finishTime;m.raceEntryId=winner_;m.probe=deadHeat_?1:0;
    if(bus_.publish(endpoint_,m)==Delivery::Delivered){completionPublished_=true;complete_=true;}
  }
  void persistResult(){
    if(!result_.sealed||persisted_||!history_)return;
    static_assert(sizeof(CompletedRaceResult)<=HistoryStore::MaxBytes,"history result exceeds bounded store");
    uint32_t sequence=0;
    if(history_->append(reinterpret_cast<const uint8_t*>(&result_),sizeof(result_),sequence)){
      historySequence_=sequence;persisted_=true;persistenceFault_=false;
      Message stored{};stored.type=Type::HistoryStored;stored.historySequence=sequence;stored.relevantTime=result_.finishTime;bus_.publish(endpoint_,stored);publishNoticeboardChanged();
    } else if(!persistenceFault_) {
      persistenceFault_=true;Message fault{};fault.type=Type::StorageFault;fault.relevantTime=result_.finishTime;bus_.publish(endpoint_,fault);publishNoticeboardChanged();
    }
  }
  static bool before(const ResultEntry&a,const ResultEntry&b,LapFinishBehaviour f){if(a.laps!=b.laps)return a.laps>b.laps;if(f==LapFinishBehaviour::Immediate)return false;if(!a.completionTime)return false;if(!b.completionTime)return true;return a.completionTime<b.completionTime;}
  void publishNoticeboardChanged(){Message n{};n.type=Type::NoticeboardChanged;bus_.publish(endpoint_,n);}
  static bool samePlace(const ResultEntry&a,const ResultEntry&b,LapFinishBehaviour f){return a.laps==b.laps&&(f==LapFinishBehaviour::Immediate||a.completionTime==b.completionTime);}
  void seal(){result_={};result_.formatVersion=ResultFormatVersion;result_.sealed=true;result_.valid=!faulted_;result_.deadHeat=deadHeat_;result_.winningTime=winningTime_;result_.finishTime=candidateF_;result_.behaviour=finishBehaviour_;result_.entryCount=entryCount();result_.lapTarget=definition_?definition_->lapTarget():0;uint32_t leaderLaps=0;for(uint8_t i=0;i<entryCount();++i){auto&t=result_.entries[i];const auto&s=entries_[i];t.raceEntryId=s.raceEntryId;t.lane=definition_->entry(i).lane;t.laps=s.laps;t.completed=observed_[i];t.completionTime=t.completed?s.lastCrossing:0;t.recordCount=s.recordCount;if(t.laps>leaderLaps)leaderLaps=t.laps;for(uint8_t j=0;j<s.recordCount;++j){t.records[j]=s.records[j];if(s.records[j].valid&&(!t.bestLap||s.records[j].lapTime<t.bestLap))t.bestLap=s.records[j].lapTime;}}
    for(uint8_t i=1;i<entryCount();++i){ResultEntry value=result_.entries[i];int j=int(i)-1;while(j>=0&&before(value,result_.entries[j],finishBehaviour_)){result_.entries[j+1]=result_.entries[j];--j;}result_.entries[j+1]=value;}
    uint32_t rank=1;for(uint8_t i=0;i<entryCount();++i){if(i&&!samePlace(result_.entries[i-1],result_.entries[i],finishBehaviour_))rank=i+1;auto&t=result_.entries[i];t.rank=rank;t.tied=i&&result_.entries[i-1].rank==rank;if(t.tied)result_.entries[i-1].tied=true;t.lapsBehind=leaderLaps-t.laps;for(uint8_t j=0;j<t.recordCount;++j){const auto&lap=t.records[j];if(!lap.valid)continue;if(!result_.fastestLap||lap.lapTime<result_.fastestLap){result_.fastestLap=lap.lapTime;result_.fastestEntryId=t.raceEntryId;result_.fastestLapTied=false;}else if(lap.lapTime==result_.fastestLap&&result_.fastestEntryId!=t.raceEntryId)result_.fastestLapTied=true;}}}
  Bus&bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition&active_;HistoryStore*history_=nullptr;TrackRecordStore*records_=nullptr;const SessionDefinition*definition_=nullptr; SessionInputRole fsRole_{}; RaceEntryDefinition fsDef_{}; uint8_t penalties_[MaxEntries]{};bool observed_[MaxEntries]{};uint32_t observedRevision_=0;Time go_=0,pauseAt_=0,restartAt_=0,settlementAt_=0,winningTime_=0,candidateF_=0;EntryState entries_[MaxEntries]{};LapFinishBehaviour finishBehaviour_=LapFinishBehaviour::Immediate;bool complete_=false,completionPublished_=false,deadHeat_=false,faulted_=false,paused_=false,restartScheduled_=false,gridRestart_=false,settlementSeen_=false,settlementPublished_=false,finishReady_=false,finishSettlementSent_=false,finishSettled_=false,persisted_=false,persistenceFault_=false,recordEligible_=true;CompletedRaceResult result_{};uint32_t winner_=0,historySequence_=0;Message pending_[16]{};uint8_t pendingCount_=0;uint32_t seen_[16]{};uint8_t seenAt_=0;
};
} // namespace pp
