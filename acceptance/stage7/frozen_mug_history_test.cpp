#include "pp/persistent_storage.h"
#include "pp/race_control.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
struct Writer {
  std::vector<uint8_t> bytes;
  void u8(uint8_t value) { bytes.push_back(value); }
  void u16(uint16_t value) { u8(uint8_t(value)); u8(uint8_t(value >> 8)); }
  void u32(uint32_t value) { for (unsigned i = 0; i < 4; ++i) u8(uint8_t(value >> (i * 8))); }
  void u64(uint64_t value) { for (unsigned i = 0; i < 8; ++i) u8(uint8_t(value >> (i * 8))); }
};
uint32_t checksum(const uint8_t* data, size_t length) {
  uint32_t value = 2166136261u;
  while (length--) value = (value ^ *data++) * 16777619u;
  return value;
}
bool require(bool value, const char* message) {
  if (!value) std::cerr << "FAIL: " << message << "\n";
  return value;
}

class CatalogStore final : public pp::Store {
public:
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= slots.size() || slots[slot].size() != size) return false;
    std::memcpy(data, slots[slot].data(), size); return true;
  }
  ReadResult readResult(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= slots.size() || slots[slot].empty()) return ReadResult::Missing;
    return read(slot, data, size) ? ReadResult::Readable : ReadResult::Failed;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (slot >= slots.size()) return false;
    slots[slot].assign(data, data + size); return true;
  }
  bool writeMetadata(unsigned slot, const uint8_t* data, size_t size) override { return write(slot, data, size); }
  bool reclaim(unsigned slot) override { if (slot < slots.size()) slots[slot].clear(); return true; }
private:
  std::array<std::vector<uint8_t>, 300> slots{};
};

std::vector<uint8_t> legacyV3() {
  using namespace pp;
  Writer w;
  w.u32(VersionedResultCodec::Magic); w.u16(VersionedResultCodec::LegacyVersion); w.u16(0);
  w.u32(0); w.u16(RaceEngineModule::ResultFormatVersion);
  w.u8(3); w.u8(static_cast<uint8_t>(SessionMode::LapRace));
  w.u8(static_cast<uint8_t>(LapFinishBehaviour::Immediate)); w.u8(2);
  w.u16(0); w.u16(0); w.u32(2);
  w.u64(0); w.u64(2000); w.u64(0); w.u64(0); w.u64(1000); w.u32(1); w.u32(0);
  for (uint32_t i = 0; i < 2; ++i) {
    w.u32(i + 1); w.u32(11 + i); w.u32(1); w.u32(1); w.u32(i + 1);
    w.u32(0); w.u32(0); w.u8(uint8_t(i + 1)); w.u8(2); w.u32(0);
    w.u64(1000 + i); w.u64(1000 + i);
  }
  const uint32_t total = static_cast<uint32_t>(w.bytes.size() + 4);
  for (unsigned i = 0; i < 4; ++i) w.bytes[8 + i] = uint8_t(total >> (i * 8));
  const uint32_t sum = checksum(w.bytes.data(), w.bytes.size());
  w.u32(sum);
  return w.bytes;
}
}

int main() {
  using namespace pp;
  bool ok = true;
  RaceEngineModule::CompletedRaceResult original{};
  RaceEngineModule::clearResult(original);
  original.sealed = true; original.valid = true; original.entryCount = 2;
  original.mode = SessionMode::LapRace; original.lapTarget = 2;
  original.entries[0].raceEntryId = 1; original.entries[0].mugId = 11; original.entries[0].lane = 1;
  original.entries[1].raceEntryId = 2; original.entries[1].mugId = 12; original.entries[1].lane = 2;
  std::strcpy(original.entries[0].mugName, "Twin");
  std::strcpy(original.entries[1].mugName, "Twin");
  uint8_t encoded[VersionedResultCodec::MaxBytes]{}; size_t length = 0;
  RaceEngineModule::CompletedRaceResult decoded{};
  ok &= require(VersionedResultCodec::encode(original, encoded, sizeof(encoded), length),
                "current named result encodes");
  ok &= require(VersionedResultCodec::decode(encoded, length, decoded) &&
                std::strcmp(decoded.entries[0].mugName, "Twin") == 0 &&
                std::strcmp(decoded.entries[1].mugName, "Twin") == 0,
                "current result round-trip preserves frozen names and distinct IDs");
  auto legacy = legacyV3();
  RaceEngineModule::CompletedRaceResult legacyDecoded{};
  ok &= require(VersionedResultCodec::decode(legacy.data(), legacy.size(), legacyDecoded) &&
                std::strcmp(legacyDecoded.entries[0].mugName, "MUG 11") == 0 &&
                std::strcmp(legacyDecoded.entries[1].mugName, "MUG 12") == 0,
                "old ID-only results remain readable without catalog lookup");
  ProposedRaceSetup setup{}; setup.activeLanes = 2; setup.lapTarget = 2;
  setup.entries[0].mugId = 11; setup.entries[1].mugId = 12;
  std::strcpy(setup.entries[0].mugName, "Twin"); std::strcpy(setup.entries[1].mugName, "Twin");
  SessionDefinition definition(setup, 5, 20);
  ok &= require(std::strcmp(definition.entry(0).mugName, "Twin") == 0 &&
                std::strcmp(definition.entry(1).mugName, "Twin") == 0 &&
                definition.entry(0).mugId != definition.entry(1).mugId,
                "Session Definition retains frozen names and permanent IDs");

  CatalogStore catalogStore;
  MugCatalog catalog(catalogStore);
  uint32_t firstId = 0, secondId = 0;
  ok &= require(catalog.load() == MugCatalog::LoadResult::Empty &&
                catalog.create("Alice", firstId) == MugCatalog::Result::Accepted &&
                catalog.create("Alice", secondId) == MugCatalog::Result::Accepted &&
                firstId != secondId,
                "saved catalog provides distinct permanent IDs for duplicate names");
  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  ActiveSessionDefinition active;
  SimulatedDetectorProvider provider;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr,
                            &provider, nullptr, nullptr, &catalog);
  bus.subscribe(controlEndpoint, Type::StartRequest);
  bus.subscribe(presentation, Type::RequestResult);
  bus.subscribe(engineEndpoint, Type::GoScheduled);
  ProposedRaceSetup startSetup{};
  startSetup.activeLanes = 2; startSetup.lapTarget = 2;
  startSetup.entries[0].startFinish = {InputModule::simulatedDetectorIdentity(0), 1, InputRole::StartFinish};
  startSetup.entries[1].startFinish = {InputModule::simulatedDetectorIdentity(1), 2, InputRole::StartFinish};
  startSetup.entries[0].mugId = firstId; startSetup.entries[1].mugId = secondId;
  control.setProposedRaceSetup(startSetup); control.setRequiredCapabilityAvailable(true);
  Message start{}; start.type = Type::StartRequest; start.correlation = 1;
  start.clientContext = ClientContext::RaceDirectorSmug; start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "saved-name START reaches Race Control");
  control.tick(1000);
  Message response{}; bus.receive(presentation, response);
  ok &= require(response.requestResult == RequestResult::Accepted && control.definition() &&
                std::strcmp(control.definition()->entry(0).mugName, "Alice") == 0 &&
                std::strcmp(control.definition()->entry(1).mugName, "Alice") == 0,
                "START freezes current saved names into the authoritative Session Definition");
  ok &= require(catalog.rename(firstId, "Renamed") == MugCatalog::Result::Accepted &&
                catalog.archive(secondId) == MugCatalog::Result::Accepted &&
                std::strcmp(control.definition()->entry(0).mugName, "Alice") == 0 &&
                std::strcmp(control.definition()->entry(1).mugName, "Alice") == 0,
                "catalog rename/archive after START cannot change frozen Session Definition names");
  std::cout << (ok ? "WP7 frozen racer history contract PASS\n" : "WP7 frozen racer history contract FAIL\n");
  return ok ? 0 : 1;
}
