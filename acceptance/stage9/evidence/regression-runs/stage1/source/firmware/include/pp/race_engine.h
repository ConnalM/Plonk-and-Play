#pragma once
#include "core.h"
#include "session_definition.h"
namespace pp {
class RaceEngineModule {
public:
  struct EntryState { uint32_t raceEntryId=0,laps=0;Time lastCrossing=0,lastLapTime=0;bool hasLap=false; };
  RaceEngineModule(Bus& b,Bus::Endpoint e,ActiveSessionDefinition& a):bus_(b),endpoint_(e),active_(a){}
  void begin(const SessionDefinition& d,Time go){definition_=&d;observedRevision_=active_.revision();reset(go);}
  void prepare(const SessionDefinition& d){definition_=&d;observedRevision_=active_.revision();reset(0);}
  void tick(){synchronise();Message m;while(bus_.receive(endpoint_,m)){if(m.type==Type::GoScheduled)scheduled(m);else if(m.type==Type::InputEvent)queue(m);else if(m.type==Type::RaceIntegrityFault)faulted_=true;}process();}
  uint32_t laps()const{return entries_[0].laps;}Time lastLapTime()const{return entries_[0].lastLapTime;}bool complete()const{return complete_;}Time go()const{return go_;}
  uint8_t entryCount()const{return definition_?definition_->entryCount():0;} const EntryState& entryState(uint8_t i)const{return entries_[i];} bool deadHeat()const{return deadHeat_;} bool faulted()const{return faulted_;}
private:
  void reset(Time go){go_=go;complete_=deadHeat_=faulted_=false;pendingCount_=0;for(auto& e:entries_)e={};if(definition_)for(uint8_t i=0;i<definition_->entryCount();++i)entries_[i].raceEntryId=definition_->entry(i).raceEntryId;}
  void synchronise(){if(active_.revision()!=observedRevision_){definition_=active_.current();observedRevision_=active_.revision();reset(0);}}
  void scheduled(const Message&m){if(definition_&&m.relevantTime){go_=m.relevantTime;for(uint8_t i=0;i<definition_->entryCount();++i)entries_[i].lastCrossing=go_;}}
  bool seen(uint32_t id)const{for(auto x:seen_)if(x==id)return true;return false;} void remember(uint32_t id){seen_[seenAt_++%16]=id;}
  void queue(const Message&m){if(!definition_||complete_||faulted_||!go_||!m.eventId||seen(m.eventId)||pendingCount_==16)return;seen_[seenAt_++%16]=m.eventId;pending_[pendingCount_++]=m;}
  void process(){if(!definition_||complete_||faulted_)return;for(uint8_t i=1;i<pendingCount_;++i){Message x=pending_[i];int j=i-1;while(j>=0&&pending_[j].relevantTime>x.relevantTime){pending_[j+1]=pending_[j];--j;}pending_[j+1]=x;}for(uint8_t i=0;i<pendingCount_;++i)interpret(pending_[i]);pendingCount_=0;if(pendingFinish_){complete_=true;Message c{};c.type=Type::CompetitionComplete;c.relevantTime=pendingFinishTime_;c.raceEntryId=winner_;c.probe=deadHeat_?1:0;bus_.publish(endpoint_,c);pendingFinish_=false;}}
  void interpret(const Message&m){if(pendingFinish_&&m.relevantTime>pendingFinishTime_)return;SessionInputRole role;RaceEntryDefinition entry;if(!definition_->resolve(m.input,role,entry)||role.role!=InputRole::StartFinish)return;EntryState* state=nullptr;for(auto& e:entries_)if(e.raceEntryId==entry.raceEntryId)state=&e;if(!state||m.relevantTime<=state->lastCrossing)return;const Time lap=m.relevantTime-state->lastCrossing;state->lastCrossing=m.relevantTime;state->lastLapTime=lap;state->hasLap=true;++state->laps;Message f{};f.type=Type::LapCompleted;f.relevantTime=m.relevantTime;f.raceEntryId=entry.raceEntryId;f.lapNumber=state->laps;f.lapTime=lap;f.eventId=m.eventId;bus_.publish(endpoint_,f);Message n{};n.type=Type::NoticeboardChanged;bus_.publish(endpoint_,n);if(state->laps>=definition_->lapTarget()){if(!pendingFinish_||m.relevantTime<pendingFinishTime_){pendingFinish_=true;pendingFinishTime_=m.relevantTime;winner_=entry.raceEntryId;deadHeat_=false;}else if(m.relevantTime==pendingFinishTime_)deadHeat_=true;}}
  Bus& bus_;Bus::Endpoint endpoint_;ActiveSessionDefinition& active_;const SessionDefinition* definition_=nullptr;uint32_t observedRevision_=0;Time go_=0;EntryState entries_[2]{};bool complete_=false,deadHeat_=false,faulted_=false,pendingFinish_=false;Time pendingFinishTime_=0;uint32_t winner_=0;Message pending_[16]{};uint8_t pendingCount_=0;uint32_t seen_[16]{};uint8_t seenAt_=0;
};
}
