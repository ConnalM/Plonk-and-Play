#include "pp/race_control.h"
#include "pp/race_engine.h"
#include "pp/mug_catalog.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
using namespace pp;

struct CatalogStore final : Store {
  static constexpr unsigned SlotCount = MugCatalog::StagingSlot + 1;
  std::vector<std::array<uint8_t, MugCatalog::RecordSize>> slots =
      std::vector<std::array<uint8_t, MugCatalog::RecordSize>>(SlotCount);
  std::array<bool, SlotCount> present{};
  bool online = true;
  bool available() const override { return online; }
  bool read(unsigned slot, uint8_t* out, size_t size) override {
    if (!online || slot >= SlotCount || !present[slot] || size > slots[slot].size()) return false;
    std::memcpy(out, slots[slot].data(), size);
    return true;
  }
  ReadResult readResult(unsigned slot, uint8_t* out, size_t size) override {
    if (!online) return ReadResult::Failed;
    if (slot >= SlotCount || !present[slot]) return ReadResult::Missing;
    return read(slot, out, size) ? ReadResult::Readable : ReadResult::Failed;
  }
  bool write(unsigned slot, const uint8_t* in, size_t size) override {
    if (!online || slot >= SlotCount || size > slots[slot].size()) return false;
    std::memcpy(slots[slot].data(), in, size); present[slot] = true; return true;
  }
  bool writeUnreferenced(unsigned slot, const uint8_t* in, size_t size) override { return write(slot, in, size); }
  bool writeMetadata(unsigned slot, const uint8_t* in, size_t size) override { return write(slot, in, size); }
  bool reclaim(unsigned slot) override { if (slot < SlotCount) present[slot] = false; return true; }
  StoreHealth health() const override { return online ? StoreHealth::Healthy : StoreHealth::Unavailable; }
};

struct HistoryFake final : HistoryStore {
  uint32_t appendCalls = 0;
  uint32_t nextSequence = 0;
  uint8_t countValue = 0;
  bool append(const uint8_t*, size_t, uint32_t& sequence) override {
    ++appendCalls; sequence = ++nextSequence; ++countValue; return true;
  }
  bool loadNewest(uint8_t, uint8_t*, size_t, size_t&, uint32_t&) override { return false; }
  uint8_t count() override { return countValue; }
  bool clear() override { countValue = 0; return true; }
  StoreHealth health() const override { return StoreHealth::Healthy; }
};

struct RecordsFake final : TrackRecordStore {
  uint32_t observeCalls = 0;
  Time best = 0;
  bool observe(uint8_t, Time lap) override { ++observeCalls; if (!best || lap < best) best = lap; return true; }
  Time lanePb(uint8_t) override { return best; }
  Time trackRecord() override { return best; }
  uint32_t era() override { return 1; }
  bool clearLane(uint8_t) override { return true; }
  bool clearTrack() override { return true; }
  bool clearAll() override { return true; }
};

bool require(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL: " << message << "\n";
  return condition;
}

} // namespace

int main() {
  using namespace pp;
  bool ok = true;

  CatalogStore catalogStorage;
  MugCatalog catalog(catalogStorage);
  ok &= require(catalog.load() == MugCatalog::LoadResult::Empty, "first-use catalog is empty");
  uint32_t alice = 0, bob = 0;
  ok &= require(catalog.create("Alice", alice) == MugCatalog::Result::Accepted && alice == 3, "Alice receives first saved ID");
  ok &= require(catalog.create("Bob", bob) == MugCatalog::Result::Accepted && bob == 4, "Bob receives second saved ID");

  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto inputEndpoint = bus.attach(Role::Input);
  ActiveSessionDefinition active;
  SimulatedDetectorProvider provider;
  HistoryFake history;
  RecordsFake records;
  RaceControlModule control(bus, controlEndpoint, active, &history, &records, &provider,
                            nullptr, nullptr, &catalog);
  RaceEngineModule engine(bus, engineEndpoint, active, &history, &records);
  InputModule input(bus, inputEndpoint, provider);

  ok &= require(bus.subscribe(controlEndpoint, Type::StartRequest), "Race Control receives START");
  ok &= require(bus.subscribe(controlEndpoint, Type::SessionOperationRequest), "Race Control receives operations");
  ok &= require(bus.subscribe(controlEndpoint, Type::CompetitionComplete), "Race Control receives completion");
  ok &= require(bus.subscribe(presentation, Type::RequestResult), "Browser receives request results");
  ok &= require(bus.subscribe(engineEndpoint, Type::GoScheduled), "Race Engine receives GO");
  ok &= require(bus.subscribe(engineEndpoint, Type::InputEvent), "Race Engine receives passages");
  ok &= require(bus.subscribe(engineEndpoint, Type::SessionOperation), "Race Engine receives Race Again");
  ok &= require(bus.subscribe(engineEndpoint, Type::FinishSettled), "Race Engine receives finish settlement");
  ok &= require(bus.subscribe(engineEndpoint, Type::RaceIntegrityFault), "Race Engine receives integrity faults");
  ok &= require(bus.subscribe(inputEndpoint, Type::SessionOperation), "Input receives session operations");
  ok &= require(bus.subscribe(inputEndpoint, Type::FinishSettlement), "Input receives finish settlement");

  ProposedRaceSetup setup{};
  setup.mode = SessionMode::LapRace;
  setup.activeLanes = 2;
  setup.lapTarget = 5;
  setup.finish = LapFinishBehaviour::CompleteCurrentLap;
  setup.entries[0].startFinish = SessionInputRole{provider.status(0).identity, 1, InputRole::StartFinish};
  setup.entries[1].startFinish = SessionInputRole{provider.status(1).identity, 2, InputRole::StartFinish};
  setup.entries[0].mugId = alice;
  setup.entries[1].mugId = bob;
  control.setProposedRaceSetup(setup);
  control.setRequiredCapabilityAvailable(true);
  ok &= require(control.armSimulatedSession(), "controller arms the test session in READY");
  ok &= require(control.simulatedSessionArmed(), "simulation arm is visible in the proposal");
  ok &= require(control.readiness() == StartReadiness::Ready, "armed five-lap proposal remains startable");

  Message start{};
  start.type = Type::StartRequest;
  start.correlation = 1;
  start.clientContext = ClientContext::RaceDirectorSmug;
  start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "normal Browser START request enters the bus");
  control.tick(1'000'000);
  ok &= require(control.state() == SessionLifecycle::Starting && control.definition(), "START commits a Session Definition");
  ok &= require(control.definition()->simulatedSession(), "Session Definition freezes the test boundary");
  ok &= require(std::strcmp(control.definition()->entry(0).mugName, "Alice") == 0 &&
                std::strcmp(control.definition()->entry(1).mugName, "Bob") == 0,
                "START freezes the selected saved MUG names");
  engine.tick();
  const Time go = control.scheduledGo();
  control.tick(go);
  ok &= require(control.state() == SessionLifecycle::Racing, "countdown reaches normal racing state");

  auto passage = [&](uint8_t lane, Time at) {
    input.sampleSource(lane, false, at - SimulatedDetector::ClearStableUs - SimulatedDetector::DetectionStableUs - 1);
    input.sampleSource(lane, false, at - SimulatedDetector::DetectionStableUs - 1);
    input.sampleSource(lane, true, at - SimulatedDetector::DetectionStableUs);
    input.sampleSource(lane, true, at);
    engine.tick();
  };

  passage(0, go + 1'000'000);
  passage(1, go + 1'100'000);
  ok &= require(engine.livePosition(0) == 1 && engine.livePosition(1) == 2, "live positions follow simulated passages");
  ok &= require(engine.liveGapKind(1) == RaceEngineModule::LiveGapKind::Time && engine.liveGapValue(1) > 0,
                "live timing exposes a lane gap");
  for (uint32_t lap = 2; lap <= 5; ++lap) {
    passage(0, go + Time(lap) * 1'000'000);
    passage(1, go + Time(lap) * 1'000'000 + 100'000);
  }
  input.tick(go + 6'100'000);
  engine.tick();
  control.tick(go + 6'100'001);
  const auto& result = engine.completedResult();
  ok &= require(control.state() == SessionLifecycle::Finished && result.sealed && result.valid,
                "five-lap race seals through the normal completion path");
  bool sawAlice = false, sawBob = false;
  for (uint8_t i = 0; i < result.entryCount; ++i) {
    sawAlice = sawAlice || std::strcmp(result.entries[i].mugName, "Alice") == 0;
    sawBob = sawBob || std::strcmp(result.entries[i].mugName, "Bob") == 0;
  }
  ok &= require(sawAlice && sawBob && result.entryCount == 2, "sealed result retains both frozen racer names");
  ok &= require(history.appendCalls == 0 && engine.historySequence() == 0, "simulated result never writes official History");
  ok &= require(records.observeCalls == 0 && !engine.recordsEligible(), "simulated result never writes PB or Track Record");
  ok &= require(!engine.persistencePending() && !engine.persistenceFault(), "simulated result has no persistence side effect");

  Message again{};
  again.type = Type::SessionOperationRequest;
  again.correlation = 2;
  again.operation = SessionOperation::RaceAgain;
  again.clientContext = ClientContext::RaceDirectorSmug;
  ok &= require(bus.publish(presentation, again) == Delivery::Delivered, "Race Again request enters the normal operation path");
  control.tick(go + 6'200'000);
  ok &= require(control.state() == SessionLifecycle::Ready && control.proposedRaceSetup() &&
                control.proposedRaceSetup()->simulatedSession && !control.definition(),
                "Race Again preserves the armed test boundary without reboot");
  Message secondStart = start; secondStart.correlation = 3;
  ok &= require(bus.publish(presentation, secondStart) == Delivery::Delivered, "second START is accepted without reboot");
  control.tick(go + 6'300'000);
  ok &= require(control.state() == SessionLifecycle::Starting && control.definition() &&
                control.definition()->simulatedSession(), "second session starts with the same authoritative test boundary");

  std::cout << (ok ? "Simulated racing journey PASS\n" : "Simulated racing journey FAIL\n");
  return ok ? 0 : 1;
}
