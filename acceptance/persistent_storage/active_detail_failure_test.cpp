#define PP_ACTIVE_DETAIL_BUDGET_BYTES 512u
#include <cstdio>
#include "pp/race_engine.h"

using namespace pp;

struct RejectingHistory final : HistoryStore {
  uint32_t appendCalls=0;
  bool append(const uint8_t*,size_t,uint32_t& sequence) override { ++appendCalls; sequence=0; return false; }
  bool loadNewest(uint8_t,uint8_t*,size_t,size_t&,uint32_t&) override { return false; }
  uint8_t count() override { return 0; }
  bool clear() override { return true; }
};

int main() {
  Bus bus;
  ActiveSessionDefinition active;
  const auto engineEndpoint=bus.attach(Role::RaceEngine);
  const auto sourceEndpoint=bus.attach(Role::Input);
  const auto controlEndpoint=bus.attach(Role::RaceControl);
  bus.subscribe(engineEndpoint,Type::GoScheduled);
  bus.subscribe(engineEndpoint,Type::InputEvent);
  bus.subscribe(engineEndpoint,Type::FinishSettled);
  bus.subscribe(sourceEndpoint,Type::FinishSettlement);
  bus.subscribe(controlEndpoint,Type::CompetitionComplete);

  ProposedRaceSetup setup{};
  setup.activeLanes=1;
  setup.lapTarget=12;
  setup.mode=SessionMode::LapRace;
  setup.finish=LapFinishBehaviour::Immediate;
  setup.entries[0].startFinish={InputModule::simulatedDetectorIdentity(),1,InputRole::StartFinish};
  setup.entries[0].mugId=902;
  if(!active.commit(setup,1,1)){std::printf("commit_fail\\n");return 1;}
  SessionDefinition definition(setup,1,1);
  RejectingHistory history;
  RaceEngineModule engine(bus,engineEndpoint,active,&history,nullptr);
  engine.begin(definition,1000000);
  Message go{};go.type=Type::GoScheduled;go.relevantTime=1000000;bus.publish(sourceEndpoint,go);engine.tick();
  const auto input=InputModule::simulatedDetectorIdentity();
  for(uint32_t lap=1;lap<=12;++lap){Message passage{};passage.type=Type::InputEvent;passage.eventId=lap;passage.input=input;passage.relevantTime=1000000+Time(lap)*100000;bus.publish(sourceEndpoint,passage);engine.tick();}
  Message settled{};settled.type=Type::FinishSettled;settled.relevantTime=2200000;bus.publish(sourceEndpoint,settled);engine.tick();
  const auto& result=engine.completedResult();
  const bool pass=engine.complete()&&result.sealed&&result.entries[0].laps==12&&result.entries[0].recordCount<12&&!result.entries[0].detailComplete&&history.appendCalls==0&&engine.persistenceFault();
  std::printf("WP2C detail_exhaustion %s laps=%lu records=%lu complete=%u history_append_calls=%lu persistence_fault=%u\n",pass?"PASS":"FAIL",(unsigned long)result.entries[0].laps,(unsigned long)result.entries[0].recordCount,engine.complete()?1:0,(unsigned long)history.appendCalls,engine.persistenceFault()?1:0);
  return pass?0:1;
}
