#pragma once
#include "core.h"
#include "session_definition.h"
namespace pp {
class RaceEngineModule {
public:
  RaceEngineModule(Bus& bus,Bus::Endpoint endpoint):bus_(bus),endpoint_(endpoint) {}
  // Race Control will establish this condition in Stage 6. Stage 5 scaffolding
  // supplies predetermined values solely to prove Race Engine interpretation.
  void begin(const SessionDefinition& definition,Time go){definition_=&definition;go_=go;lastCrossing_=go;laps_=0;}
  void tick(){Message event;while(bus_.receive(endpoint_,event))interpret(event);}
  uint32_t laps()const{return laps_;} Time lastLapTime()const{return lastLapTime_;}
private:
  void interpret(const Message& event){
    if(!definition_||event.type!=Type::InputEvent||event.eventId==0||event.relevantTime<=lastCrossing_)return;
    SessionInputRole role;if(!definition_->resolve(event.input,role)||role.role!=InputRole::StartFinish)return;
    const Time lap=event.relevantTime-lastCrossing_;lastCrossing_=event.relevantTime;lastLapTime_=lap;++laps_;
    Message fact{};fact.type=Type::LapCompleted;fact.relevantTime=event.relevantTime;fact.raceEntryId=definition_->raceEntryId();fact.lapNumber=laps_;fact.lapTime=lap;fact.eventId=event.eventId;bus_.publish(endpoint_,fact);
  }
  Bus& bus_;Bus::Endpoint endpoint_;const SessionDefinition* definition_=nullptr;Time go_=0,lastCrossing_=0,lastLapTime_=0;uint32_t laps_=0;
};
}
