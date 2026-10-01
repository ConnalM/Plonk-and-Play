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
  Noticeboard(const RaceControlModule& control,const RaceEngineModule& engine):control_(control),engine_(engine) {}
  void setSession(const SessionDefinition& definition){definition_=&definition;}
  NoticeboardState current() const {
    NoticeboardState state{};state.lifecycle=control_.state();state.scheduledGo=control_.scheduledGo();
    state.laps=engine_.laps();state.lastLapTime=engine_.lastLapTime();state.hasLap=state.laps!=0;
    if(definition_)state.raceEntryId=definition_->raceEntryId();
    return state;
  }
private:
  const RaceControlModule& control_;const RaceEngineModule& engine_;const SessionDefinition* definition_=nullptr;
};
} // namespace pp
