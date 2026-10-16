#include <cstdio>
#include <cstring>
#include <vector>
#include "pp/persistent_storage.h"

using namespace pp;

struct MemoryStore final : Store {
  bool online = true;
  std::vector<std::vector<uint8_t>> slots = std::vector<std::vector<uint8_t>>(16);

  bool available() const override { return online; }
  bool read(unsigned slot, uint8_t* out, size_t size) override {
    if (!online || !out || slot >= slots.size() || slots[slot].size() != size) return false;
    std::memcpy(out, slots[slot].data(), size);
    return true;
  }
  bool write(unsigned slot, const uint8_t* in, size_t size) override {
    if (!online || !in || slot >= slots.size()) return false;
    slots[slot].assign(in, in + size);
    return true;
  }
};

struct MemoryHistory final : HistoryStore {
  std::vector<uint8_t> payload;
  uint32_t sequence = 0;

  bool append(const uint8_t* bytes, size_t size, uint32_t& outSequence) override {
    if (!bytes || !size) return false;
    payload.assign(bytes, bytes + size);
    outSequence = ++sequence;
    return true;
  }
  bool loadNewest(uint8_t index, uint8_t* out, size_t capacity, size_t& size, uint32_t& outSequence) override {
    if (index || !out || payload.size() > capacity) return false;
    std::memcpy(out, payload.data(), payload.size());
    size = payload.size();
    outSequence = sequence;
    return true;
  }
  uint8_t count() override { return payload.empty() ? 0 : 1; }
  bool clear() override { payload.clear(); sequence = 0; return true; }
  size_t payloadSize(uint8_t index) override { return index ? 0 : payload.size(); }
  size_t maximumPayloadBytes() override { return payload.size(); }
};

struct RecoverableDetail final : ActiveDetailStore {
  std::vector<LapRecord> records;
  bool available = false;
  bool finalized = false;

  bool begin(uint32_t, uint8_t) override { records.clear(); available = false; finalized = false; return true; }
  bool append(uint8_t, const LapRecord& record) override { records.push_back(record); return true; }
  void drain() override {}
  bool finalize() override { if (!available) return false; finalized = true; return true; }
  void abort() override { records.clear(); finalized = false; }
  uint32_t dropped() const override { return 0; }
  bool storagePending() const override { return !available; }
  bool recover() override { available = true; return true; }
  uint8_t detailEntryCount() const override { return 1; }
  uint32_t detailRecordCount(uint8_t entry) const override { return entry ? 0 : static_cast<uint32_t>(records.size()); }
  bool readDetail(uint8_t entry, uint32_t index, LapRecord& out) const override {
    if (entry || index >= records.size()) return false;
    out = records[index];
    return true;
  }
  bool detailComplete() const override { return available && finalized; }
};

static int failCount = 0;
static void check(const char* name, bool ok) {
  std::printf("WP2F semantic %s %s\n", name, ok ? "PASS" : "FAIL");
  if (!ok) ++failCount;
}

int main() {
  Bus bus;
  ActiveSessionDefinition active;
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto inputEndpoint = bus.attach(Role::Input);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  bus.subscribe(engineEndpoint, Type::GoScheduled);
  bus.subscribe(engineEndpoint, Type::InputEvent);
  bus.subscribe(engineEndpoint, Type::FinishSettled);
  bus.subscribe(inputEndpoint, Type::FinishSettlement);
  bus.subscribe(controlEndpoint, Type::CompetitionComplete);

  ProposedRaceSetup setup{};
  setup.activeLanes = 1;
  setup.lapTarget = 3;
  setup.mode = SessionMode::LapRace;
  setup.entries[0].mugId = 1501;
  setup.entries[0].startFinish = {InputModule::simulatedDetectorIdentity(), 1, InputRole::StartFinish};
  check("setup_committed", active.commit(setup, 1, 1));

  MemoryHistory rawHistory;
  VersionedHistoryStore history(rawHistory);
  MemoryStore rawRecords;
  VersionedTrackRecordStore records(rawRecords);
  RecoverableDetail detail;
  SessionDefinition definition(setup, 1, 1);
  RaceEngineModule engine(bus, engineEndpoint, active, &history, &records, &detail);
  engine.begin(definition, 1000000);

  Message go{}; go.type = Type::GoScheduled; go.relevantTime = 1000000;
  bus.publish(inputEndpoint, go); engine.tick();
  const auto input = InputModule::simulatedDetectorIdentity();
  for (uint32_t lap = 1; lap <= 3; ++lap) {
    Message passage{}; passage.type = Type::InputEvent; passage.eventId = lap;
    passage.input = input; passage.relevantTime = 1000000 + Time(lap) * 100000;
    bus.publish(inputEndpoint, passage); engine.tick();
  }
  Message settled{}; settled.type = Type::FinishSettled; settled.relevantTime = 1300000;
  bus.publish(inputEndpoint, settled); engine.tick();

  const auto& pending = engine.completedResult();
  check("recoverable_interruption_keeps_race_truth", pending.sealed && pending.entries[0].laps == 3 && engine.persistencePending());
  check("temporary_detail_not_published", rawHistory.count() == 0);

  check("explicit_recovery", engine.recoverPersistence());
  const auto& committed = engine.completedResult();
  check("commit_after_recovery", committed.entries[0].detailComplete && rawHistory.count() == 1 && committed.entries[0].laps == 3);

  RaceEngineModule::CompletedRaceResult reconstructed{};
  size_t length = 0; uint32_t sequence = 0;
  const bool loaded = history.loadNewest(0, reinterpret_cast<uint8_t*>(&reconstructed), sizeof(reconstructed), length, sequence);
  check("restart_reconstructs_result_and_details", loaded && sequence == 1 && reconstructed.entries[0].laps == 3 && reconstructed.entries[0].recordCount == 3 && reconstructed.entries[0].records[2].lapNumber == 3);

  VersionedTrackRecordStore recordsAfterRestart(rawRecords);
  check("pb_track_record_reconstruct", recordsAfterRestart.lanePb(1) == 100000 && recordsAfterRestart.trackRecord() == 100000);

  RaceEngineModule::clearResult(reconstructed);
  std::printf("WP2F semantic END_TO_END %s\n", failCount ? "FAIL" : "PASS");
  return failCount ? 1 : 0;
}
