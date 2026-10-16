#include <cstdio>
#include "pp/race_engine.h"

using namespace pp;

int main() {
  Bus bus;
  ActiveSessionDefinition active;
  auto engineEndpoint = bus.attach(Role::RaceEngine);
  auto sourceEndpoint = bus.attach(Role::Input);
  ProposedRaceSetup setup{};
  setup.activeLanes = 1;
  setup.lapTarget = 1000;
  setup.mode = SessionMode::LapRace;
  setup.entries[0].startFinish = {InputModule::simulatedDetectorIdentity(), 1, InputRole::StartFinish};
  setup.entries[0].mugId = 901;
  if (!active.commit(setup, 1, 1)) return 1;
  SessionDefinition definition(setup, 1, 1);
  RaceEngineModule engine(bus, engineEndpoint, active);
  bus.subscribe(engineEndpoint, Type::GoScheduled);
  bus.subscribe(engineEndpoint, Type::InputEvent);
  engine.begin(definition, 1000000);
  Message go{}; go.type = Type::GoScheduled; go.relevantTime = 1000000; bus.publish(sourceEndpoint, go);
  engine.tick();
  const auto input = InputModule::simulatedDetectorIdentity();
  bool milestone100 = false;
  for (uint32_t lap = 1; lap <= 300; ++lap) {
    Message passage{}; passage.type = Type::InputEvent; passage.eventId = lap;
    passage.input = input; passage.relevantTime = 1000000 + Time(lap) * 100000;
    bus.publish(sourceEndpoint, passage); engine.tick();
    if (lap == 100) milestone100 = engine.entryState(0).laps == 100 && engine.entryState(0).recordCount == 100;
  }
  const auto& state = engine.entryState(0);
  const bool pass = milestone100 && state.laps == 300 && state.recordCount == 300 && state.detail.complete &&
                    state.detail.records[299].lapNumber == 300 && state.bestLapTime == 100000;
  std::printf("WP2C active_detail_100_and_over_255 %s milestone100=%u laps=%lu records=%lu last=%lu\n",
              pass ? "PASS" : "FAIL", milestone100 ? 1u : 0u, (unsigned long)state.laps,
              (unsigned long)state.recordCount,
              (unsigned long)(state.recordCount ? state.detail.records[state.recordCount - 1].lapNumber : 0));
  return pass ? 0 : 1;
}
