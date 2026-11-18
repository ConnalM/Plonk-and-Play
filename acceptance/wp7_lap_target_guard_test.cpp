#include "pp/race_engine.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace {
using namespace pp;

struct Fixture {
  Bus bus;
  ActiveSessionDefinition active;
  Bus::Endpoint controlEndpoint;
  Bus::Endpoint engineEndpoint;
  Bus::Endpoint inputEndpoint;
  Bus::Endpoint presentationEndpoint;
  RaceEngineModule engine;
  std::optional<SessionDefinition> definition;
  uint32_t nextEventId = 1;

  Fixture(SessionMode mode, LapFinishBehaviour finish, uint32_t target = 45,
          uint16_t durationMinutes = 1)
      : controlEndpoint(bus.attach(Role::RaceControl)),
        engineEndpoint(bus.attach(Role::RaceEngine)),
        inputEndpoint(bus.attach(Role::Input)),
        presentationEndpoint(bus.attach(Role::Presentation)),
        engine(bus, engineEndpoint, active) {
    assert(bus.subscribe(engineEndpoint, Type::GoScheduled));
    assert(bus.subscribe(engineEndpoint, Type::InputEvent));
    assert(bus.subscribe(engineEndpoint, Type::EnduranceExpired));
    assert(bus.subscribe(engineEndpoint, Type::FinishSettled));
    assert(bus.subscribe(controlEndpoint, Type::CompetitionComplete));
    assert(bus.subscribe(inputEndpoint, Type::FinishSettlement));
    assert(bus.subscribe(presentationEndpoint, Type::LapCompleted));

    ProposedRaceSetup setup{};
    setup.mode = mode;
    setup.activeLanes = 2;
    setup.lapTarget = mode == SessionMode::LapRace ? target : 0;
    setup.durationMinutes = durationMinutes;
    setup.finish = finish;
    setup.entries[0].startFinish =
        SessionInputRole{InputIdentity{50500001, 1}, 1, InputRole::StartFinish};
    setup.entries[1].startFinish =
        SessionInputRole{InputIdentity{50500001, 2}, 2, InputRole::StartFinish};
    setup.entries[0].mugId = 1;
    setup.entries[1].mugId = 2;
    assert(active.commit(setup, 1, 1));
    definition.emplace(setup, 1, 1);
    engine.begin(*definition, 1'000'000);

    Message go{};
    go.type = Type::GoScheduled;
    go.relevantTime = 1'000'000;
    assert(bus.publish(controlEndpoint, go) == Delivery::Delivered);
    engine.tick();
  }

  uint8_t laneIndex(uint8_t lane) const { return static_cast<uint8_t>(lane - 1); }

  uint32_t passage(uint8_t lane, Time at) {
    Message event{};
    event.type = Type::InputEvent;
    event.eventId = nextEventId++;
    event.relevantTime = at;
    event.raceEntryId = lane;
    event.input = InputIdentity{50500001, lane};
    assert(bus.publish(inputEndpoint, event) == Delivery::Delivered);
    engine.tick();

    uint32_t lapFacts = 0;
    Message fact{};
    while (bus.receive(presentationEndpoint, fact)) {
      if (fact.type == Type::LapCompleted) ++lapFacts;
    }
    return lapFacts;
  }

  bool settle() {
    Message settlement{};
    bool found = false;
    while (bus.receive(inputEndpoint, settlement)) {
      if (settlement.type != Type::FinishSettlement) continue;
      Message settled{};
      settled.type = Type::FinishSettled;
      settled.relevantTime = settlement.relevantTime;
      assert(bus.publish(inputEndpoint, settled) == Delivery::Delivered);
      found = true;
    }
    if (found) engine.tick();
    return found;
  }

  const RaceEngineModule::ResultEntry* resultForLane(uint8_t lane) const {
    const auto& result = engine.completedResult();
    for (uint8_t i = 0; i < result.entryCount; ++i) {
      if (result.entries[i].lane == lane) return &result.entries[i];
    }
    return nullptr;
  }
};

void lane1FinishesFirst() {
  Fixture fixture(SessionMode::LapRace,
                  LapFinishBehaviour::CompleteFullRaceDistance);
  const Time go = fixture.engine.go();
  for (uint32_t lap = 1; lap <= 45; ++lap) {
    assert(fixture.passage(1, go + Time(lap) * 1'000'000) == 1);
  }
  for (uint32_t lap = 1; lap <= 44; ++lap) {
    assert(fixture.passage(2, go + Time(lap) * 1'000'000 + 100'000) == 1);
  }

  const auto completed = fixture.engine.entryState(0);
  const Time finishCrossing = completed.lastCrossing;
  const Time finishLap = completed.lastLapTime;
  const uint32_t detailCount = completed.detail.recordCount;
  assert(fixture.engine.entryCompleted(0));

  for (uint32_t late = 46; late <= 48; ++late) {
    assert(fixture.passage(1, go + Time(late) * 1'000'000) == 0);
  }
  const auto& after = fixture.engine.entryState(0);
  assert(after.laps == 45 && after.detail.recordCount == detailCount);
  assert(after.lastCrossing == finishCrossing && after.lastLapTime == finishLap);

  assert(fixture.passage(2, go + 45'100'000'000ULL) == 1);
  assert(fixture.settle());
  const auto* lane1 = fixture.resultForLane(1);
  const auto* lane2 = fixture.resultForLane(2);
  assert(lane1 && lane2 && lane1->laps == 45 && lane2->laps == 45);
  assert(lane1->completionTime == finishCrossing);
  assert(fixture.engine.complete());
  assert(fixture.engine.completedResult().lapTarget == 45);
}

void lane2FinishesFirst() {
  Fixture fixture(SessionMode::LapRace,
                  LapFinishBehaviour::CompleteFullRaceDistance);
  const Time go = fixture.engine.go();
  for (uint32_t lap = 1; lap <= 45; ++lap) {
    assert(fixture.passage(2, go + Time(lap) * 1'000'000) == 1);
  }
  for (uint32_t lap = 1; lap <= 44; ++lap) {
    assert(fixture.passage(1, go + Time(lap) * 1'000'000 + 100'000) == 1);
  }

  const auto completed = fixture.engine.entryState(1);
  const Time finishCrossing = completed.lastCrossing;
  const Time finishLap = completed.lastLapTime;
  const uint32_t detailCount = completed.detail.recordCount;
  assert(fixture.engine.entryCompleted(1));

  for (uint32_t late = 46; late <= 48; ++late) {
    assert(fixture.passage(2, go + Time(late) * 1'000'000) == 0);
  }
  const auto& after = fixture.engine.entryState(1);
  assert(after.laps == 45 && after.detail.recordCount == detailCount);
  assert(after.lastCrossing == finishCrossing && after.lastLapTime == finishLap);

  assert(fixture.passage(1, go + 45'100'000'000ULL) == 1);
  assert(fixture.settle());
  const auto* lane1 = fixture.resultForLane(1);
  const auto* lane2 = fixture.resultForLane(2);
  assert(lane1 && lane2 && lane1->laps == 45 && lane2->laps == 45);
  assert(lane2->completionTime == finishCrossing);
  assert(fixture.engine.complete());
  assert(fixture.engine.completedResult().lapTarget == 45);
}

void interleavedExactCompletion() {
  Fixture fixture(SessionMode::LapRace,
                  LapFinishBehaviour::CompleteFullRaceDistance);
  const Time go = fixture.engine.go();
  for (uint32_t lap = 1; lap <= 45; ++lap) {
    assert(fixture.passage(1, go + Time(lap) * 1'000'000) == 1);
    assert(fixture.passage(2, go + Time(lap) * 1'000'000 + 100'000) == 1);
  }
  assert(fixture.engine.entryState(0).laps == 45);
  assert(fixture.engine.entryState(1).laps == 45);
  assert(fixture.engine.complete() == false);
  assert(fixture.passage(1, go + 46'000'000'000ULL) == 0);
  assert(fixture.passage(2, go + 46'100'000'000ULL) == 0);
  assert(fixture.settle());
  const auto& result = fixture.engine.completedResult();
  assert(result.sealed && result.entryCount == 2 && result.lapTarget == 45);
  assert(result.entries[0].laps == 45 && result.entries[1].laps == 45);
  assert(fixture.engine.complete());
}

void enduranceFinishCurrentLapRemainsUnchanged() {
  Fixture fixture(SessionMode::Endurance,
                  LapFinishBehaviour::CompleteCurrentLap, 0, 1);
  const Time go = fixture.engine.go();
  assert(fixture.passage(1, go + 500'000) == 1);
  assert(fixture.passage(2, go + 600'000) == 1);

  Message expiry{};
  expiry.type = Type::EnduranceExpired;
  expiry.relevantTime = go + 1'000'000;
  assert(fixture.bus.publish(fixture.controlEndpoint, expiry) == Delivery::Delivered);
  fixture.engine.tick();

  assert(fixture.passage(1, go + 1'100'000) == 1);
  const auto completed = fixture.engine.entryState(0);
  assert(completed.laps == 2 && completed.detail.recordCount == 2);
  assert(fixture.passage(1, go + 1'200'000) == 0);
  const auto& after = fixture.engine.entryState(0);
  assert(after.laps == 2 && after.detail.recordCount == 2);

  assert(fixture.passage(2, go + 1'150'000) == 1);
  assert(fixture.settle());
  const auto& result = fixture.engine.completedResult();
  assert(result.sealed && result.mode == SessionMode::Endurance);
  assert(result.entries[0].laps == 2 && result.entries[1].laps == 2);
  assert(fixture.engine.complete());
}
} // namespace

int main() {
  lane1FinishesFirst();
  lane2FinishesFirst();
  interleavedExactCompletion();
  enduranceFinishCurrentLapRemainsUnchanged();
  std::puts("WP7_LAP_TARGET_GUARD_PASS");
  return 0;
}
