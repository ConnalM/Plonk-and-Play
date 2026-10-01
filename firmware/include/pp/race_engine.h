#pragma once
#include "core.h"
#include "session_definition.h"
namespace pp {
class RaceEngineModule {
public:
  RaceEngineModule(Bus& bus,Bus::Endpoint endpoint):bus_(bus),endpoint_(endpoint) {}
  // Stage 5 regression setup only. Stage 6 uses GO_SCHEDULED from Race Control.
  void begin(const SessionDefinition& definition,Time go){definition_=&definition;go_=go;lastCrossing_=go;lastLapTime_=0;laps_=0;complete_=false;}
  // The Stage 6 preparation boundary supplies immutable working session data;
  // it does not establish GO or change Race Control lifecycle state.
  void prepare(const SessionDefinition& definition){definition_=&definition;go_=0;lastCrossing_=0;lastLapTime_=0;laps_=0;complete_=false;}
  void tick(){Message event;while(bus_.receive(endpoint_,event)){if(event.type==Type::GoScheduled)scheduledGo(event);else if(event.type==Type::InputEvent)interpret(event);}}
  uint32_t laps()const{return laps_;} Time lastLapTime()const{return lastLapTime_;}
  bool complete()const{return complete_;} Time go()const{return go_;}
private:
  void scheduledGo(const Message& event){if(definition_&&event.relevantTime>0){go_=event.relevantTime;lastCrossing_=go_;}}
  void interpret(const Message& event){
    if(!definition_||complete_||event.eventId==0||go_==0||event.relevantTime<=lastCrossing_)return;
    SessionInputRole role;if(!definition_->resolve(event.input,role)||role.role!=InputRole::StartFinish)return;
    const Time lap=event.relevantTime-lastCrossing_;lastCrossing_=event.relevantTime;lastLapTime_=lap;++laps_;
    Message fact{};fact.type=Type::LapCompleted;fact.relevantTime=event.relevantTime;fact.raceEntryId=definition_->raceEntryId();fact.lapNumber=laps_;fact.lapTime=lap;fact.eventId=event.eventId;bus_.publish(endpoint_,fact);
    Message notice{};notice.type=Type::NoticeboardChanged;bus_.publish(endpoint_,notice);
    if(definition_->lapTarget()&&laps_>=definition_->lapTarget()&&definition_->finishBehaviour()==LapFinishBehaviour::Immediate){complete_=true;Message complete{};complete.type=Type::CompetitionComplete;complete.relevantTime=event.relevantTime;complete.eventId=event.eventId;bus_.publish(endpoint_,complete);}
  }
  Bus& bus_;Bus::Endpoint endpoint_;const SessionDefinition* definition_=nullptr;Time go_=0,lastCrossing_=0,lastLapTime_=0;uint32_t laps_=0;bool complete_=false;
};
}
