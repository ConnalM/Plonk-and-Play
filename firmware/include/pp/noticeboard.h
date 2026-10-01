#pragma once
#include "race_control.h"
#include "race_engine.h"

namespace pp {
// The Noticeboard is an authoritative externally presentable view. It reads
// state from the owners; it does not hold a competing race-state model.
struct NoticeboardState {
  SessionLifecycle lifecycle=SessionLifecycle::Ready;
  uint32_t raceEntryId=0,laps=0;
  Time lastLapTime=0,scheduledGo=0;
  bool hasLap=false;
};
class Noticeboard {
public:
  Noticeboard(const RaceControlModule& control,const RaceEngineModule& engine,const ActiveSessionDefinition& active):control_(control),engine_(engine),active_(active) {}
  void setSession(const SessionDefinition& definition){definition_=&definition;}
  NoticeboardState current() const {
    NoticeboardState state{};state.lifecycle=control_.state();state.scheduledGo=control_.scheduledGo();
    state.laps=engine_.laps();state.lastLapTime=engine_.lastLapTime();state.hasLap=state.laps!=0;
    const SessionDefinition* definition=definition_?definition_:active_.current();
    if(definition)state.raceEntryId=definition->raceEntryId();
    return state;
  }
private:
  const RaceControlModule& control_;const RaceEngineModule& engine_;const ActiveSessionDefinition& active_;const SessionDefinition* definition_=nullptr;
};
} // namespace pp
