#include <cstdio>
#include <vector>
#include "pp/system_time.h"
#include "pp/noticeboard.h"

using namespace pp;

struct Stream final : ActiveDetailStore {
  std::vector<LapRecord> records;
  bool finalised = false;
  bool begin(uint32_t, uint8_t) override { records.clear(); finalised = false; return true; }
  bool append(uint8_t, const LapRecord& record) override { records.push_back(record); return true; }
  void drain() override {}
  bool finalize() override { finalised = true; return true; }
  void abort() override { records.clear(); finalised = false; }
  uint32_t dropped() const override { return 0; }
  uint8_t detailEntryCount() const override { return 1; }
  uint32_t detailRecordCount(uint8_t entry) const override {
    return entry == 0 ? static_cast<uint32_t>(records.size()) : 0;
  }
  bool readDetail(uint8_t entry, uint32_t index, LapRecord& out) const override {
    if (entry != 0 || index >= records.size()) return false;
    out = records[index];
    return true;
  }
  bool detailComplete() const override { return finalised; }
};

int main() {
  Bus bus;
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto sourceEndpoint = bus.attach(Role::Input);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  bus.subscribe(engineEndpoint, Type::GoScheduled);
  bus.subscribe(engineEndpoint, Type::InputEvent);

  ActiveSessionDefinition active;
  ProposedRaceSetup setup{};
  setup.activeLanes = 1;
  setup.lapTarget = 3;
  setup.mode = SessionMode::LapRace;
  setup.entries[0].mugId = 901;
  setup.entries[0].startFinish = {InputModule::simulatedDetectorIdentity(), 1, InputRole::StartFinish};
  if (!active.commit(setup, 1, 1)) return 1;

  SessionDefinition definition(setup, 1, 1);
  RaceControlModule control(bus, controlEndpoint, active);
  control.prepare(definition);
  Stream streamedDetail;
  RaceEngineModule engine(bus, engineEndpoint, active, nullptr, nullptr, &streamedDetail);
  if (!control.start(1'000'000)) return 2;
  const Time go = control.scheduledGo();
  engine.begin(definition, go);
  engine.tick();
  control.tick(go);
  engine.tick();

  Message passage{};
  passage.type = Type::InputEvent;
  passage.eventId = 1;
  passage.input = InputModule::simulatedDetectorIdentity();
  passage.relevantTime = go + 1'000'000;
  if (bus.publish(sourceEndpoint, passage) != Delivery::Delivered) return 3;
  engine.tick();

  // The streamed-detail path intentionally has no in-RAM records pointer.
  // Noticeboard must still reconstruct the latest live lap without crashing.
  Noticeboard noticeboard(control, engine, active);
  const auto state = noticeboard.currentAt(passage.relevantTime);
  const bool pass = state.hasLap && state.laps == 1 && state.lastLapTime == 1'000'000 &&
                    state.raceEntryId == definition.entry(0).raceEntryId &&
                    streamedDetail.records.size() == 1 && engine.entryState(0).records == nullptr;
  std::printf("P1 streamed-detail Noticeboard state %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 4;
}
