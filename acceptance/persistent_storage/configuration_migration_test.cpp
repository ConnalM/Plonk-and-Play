#include "pp/core.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using pp::Configuration;
using pp::LoadStatus;
using pp::Memory;
using pp::SessionMode;
using Bytes = std::vector<uint8_t>;

// Historical layout from firmware/include/pp/core.h at 6178158. Encode
// independently of Memory::encode so the fixtures really are 32-byte blobs.
constexpr uint32_t kCpp1 = 0x31505043u;
constexpr uint32_t kCpp2 = 0x32505043u;
constexpr uint32_t kCpp3 = 0x33505043u;

void put32(Bytes& bytes, size_t offset, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) {
    bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
  }
}

uint32_t read32(const Bytes& bytes, size_t offset) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) {
    value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
  }
  return value;
}

Bytes legacyRecord(uint32_t format, uint32_t generation,
                   const Configuration& settings) {
  Bytes bytes(32);
  put32(bytes, 0, format);
  put32(bytes, 4, generation);
  if (format == kCpp1) {
    put32(bytes, 8, settings.lanes);
    put32(bytes, 12, settings.laps);
    put32(bytes, 16, settings.optionalFeatures);
    bytes[20] = settings.soundIfAvailable;
    bytes[21] = settings.powerIfAvailable;
    bytes[22] = settings.mode;
  } else {
    bytes[8] = static_cast<uint8_t>(settings.lanes);
    put32(bytes, 9, settings.lane1MugId);
    put32(bytes, 13, settings.laps);
    put32(bytes, 17, settings.optionalFeatures);
    bytes[21] = settings.soundIfAvailable;
    bytes[22] = settings.powerIfAvailable;
    bytes[23] = settings.mode;
    put32(bytes, 24, settings.lane2MugId);
  }
  uint32_t checksum = 2166136261u;
  for (size_t i = 0; i < 28; ++i) {
    checksum = (checksum ^ bytes[i]) * 16777619u;
  }
  put32(bytes, 28, checksum);
  return bytes;
}

class NvsContractStore final : public pp::Store {
public:
  std::array<Bytes, 6> slots{};
  bool failPublication = false;
  unsigned writes = 0;

  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= slots.size() || slots[slot].empty()) return false;
    // NvsStore in firmware/src/main.cpp reports a present blob of the wrong
    // size as readable zeroes. It does not return a truncated legacy blob.
    if (slots[slot].size() != size) {
      std::fill_n(data, size, 0);
      return true;
    }
    std::copy(slots[slot].begin(), slots[slot].end(), data);
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (slot >= slots.size()) return false;
    ++writes;
    slots[slot].assign(data, data + size);
    return true;
  }
  bool writeMetadata(unsigned slot, const uint8_t* data, size_t size) override {
    return !failPublication && write(slot, data, size);
  }
};

Configuration customerSettings() {
  Configuration config{};
  config.mode = static_cast<uint8_t>(SessionMode::Endurance);
  config.laps = 137;
  config.soundIfAvailable = false;
  config.powerIfAvailable = false;
  config.lane1MugId = 74;
  config.lane2MugId = 89;
  return config;
}

bool sameSettings(const Configuration& actual, const Configuration& expected) {
  return actual.lanes == expected.lanes && actual.laps == expected.laps &&
         actual.durationMinutes == expected.durationMinutes &&
         actual.optionalFeatures == expected.optionalFeatures &&
         actual.soundIfAvailable == expected.soundIfAvailable &&
         actual.powerIfAvailable == expected.powerIfAvailable &&
         actual.mode == expected.mode && actual.lane1MugId == expected.lane1MugId &&
         actual.lane2MugId == expected.lane2MugId;
}

bool remembers(NvsContractStore& store, const Configuration& expected) {
  Memory restarted(store);
  Configuration actual{};
  return restarted.load(actual) == LoadStatus::Remembered &&
         sameSettings(actual, expected);
}

bool carriesForwardMissingV1Fields() {
  NvsContractStore store;
  Configuration expected = customerSettings();
  expected.lanes = 1;
  const auto legacy = legacyRecord(kCpp1, 41, expected);
  store.slots[0] = legacy;
  expected.lane1MugId = 1;
  expected.lane2MugId = 2;
  expected.durationMinutes = 10;
  if (!remembers(store, expected) || store.writes != 0) return false;
  Memory memory(store);
  if (!memory.save(19, 25)) return false;
  expected.lane1MugId = 19;
  expected.lane2MugId = 25;
  const Bytes& published = store.slots[1];
  return store.slots[0] == legacy && published.size() == 36 &&
         read32(published, 0) == kCpp3 && read32(published, 4) == 42 &&
         remembers(store, expected);
}

bool upgradesV2AndPreservesCustomerSettings() {
  NvsContractStore store;
  Configuration expected = customerSettings();
  const auto legacy = legacyRecord(kCpp2, 55, expected);
  Configuration older{};
  store.slots[0].resize(36);
  Memory::encode(older, 53, store.slots[0].data());
  store.slots[1] = legacy;
  if (!remembers(store, expected) || store.writes != 0) return false;
  Memory memory(store);
  if (!memory.saveRaceSettings(SessionMode::Endurance, 0, 73)) return false;
  expected.durationMinutes = 73;
  const Bytes& published = store.slots[0];
  if (store.slots[1] != legacy || published.size() != 36 ||
      read32(published, 0) != kCpp3 || read32(published, 4) != 56 ||
      !remembers(store, expected)) return false;
  // A racer-only save through Memory must retain the remembered race settings.
  Memory racerSettings(store);
  if (!racerSettings.save(101, 102)) return false;
  expected.lane1MugId = 101;
  expected.lane2MugId = 102;
  return remembers(store, expected);
}

bool restoresLegacyAfterFailedPublication() {
  NvsContractStore store;
  const Configuration expected = customerSettings();
  const auto legacy = legacyRecord(kCpp2, 72, expected);
  store.slots[0] = legacy;
  store.failPublication = true;
  Memory memory(store);
  if (memory.saveRaceSettings(SessionMode::Endurance, 200, 38)) return false;
  // The model leaves the staging blob present. Reconstruction still selects
  // only the committed A/B slots, including after a restart.
  return store.slots[0] == legacy && store.slots[1].empty() &&
         store.slots[Memory::StagingSlot].size() == 36 &&
         remembers(store, expected);
}

bool fallsBackToLegacyAfterDamagedV3() {
  NvsContractStore store;
  const Configuration expected = customerSettings();
  store.slots[1] = legacyRecord(kCpp2, 72, expected);
  store.slots[0].resize(36);
  Memory::encode(Configuration{}, 73, store.slots[0].data());
  store.slots[0][32] ^= 1;
  return remembers(store, expected) && store.writes == 0;
}

bool retainsOtherSettingsForLegacyLapTargetOver999() {
  NvsContractStore store;
  Configuration expected = customerSettings();
  expected.laps = 1200;
  const auto legacy = legacyRecord(kCpp2, 92, expected);
  store.slots[0] = legacy;
  Memory memory(store);
  Configuration loaded{};
  const LoadStatus status = memory.load(loaded);
  if (status == LoadStatus::DefaultsInvalid) {
    std::cerr << "  1200-lap legacy blob rejected; loader returned defaults\n";
  }
  if (status != LoadStatus::Remembered || !sameSettings(loaded, expected) ||
      store.writes != 0 || store.slots[0] != legacy) return false;
  if (!memory.saveRaceSettings(SessionMode::Endurance, 0, 73)) return false;
  expected.durationMinutes = 73;
  return store.slots[0] == legacy && read32(store.slots[1], 0) == kCpp3 &&
         remembers(store, expected);
}

bool sharedLapBoundariesSurviveRestart() {
  for (uint32_t laps : {1U, 999U, 1000U, std::numeric_limits<uint32_t>::max()}) {
    NvsContractStore store;
    Configuration expected = customerSettings();
    store.slots[0] = legacyRecord(kCpp2, 4, expected);
    Memory memory(store);
    if (!memory.saveRaceSettings(SessionMode::LapRace, laps, 0)) return false;
    expected.mode = static_cast<uint8_t>(SessionMode::LapRace);
    expected.laps = laps;
    if (!remembers(store, expected)) return false;
  }
  return true;
}

bool rejectsCorruptRecordsWithoutRewriting() {
  NvsContractStore store;
  Configuration bad = customerSettings();
  bad.laps = 0;
  // Even with a correct checksum, zero is still invalid shared configuration.
  store.slots[0] = legacyRecord(kCpp2, 44, bad);
  const auto zeroLegacy = store.slots[0];
  store.slots[1].resize(Memory::RecordSize);
  Memory::encode(customerSettings(), 45, store.slots[1].data());
  store.slots[1][13] ^= 1;
  const auto corruptCurrent = store.slots[1];
  for (unsigned boot = 0; boot < 2; ++boot) {
    Memory memory(store);
    Configuration loaded{};
    if (memory.load(loaded) != LoadStatus::DefaultsInvalid ||
        !sameSettings(loaded, Configuration{})) return false;
  }
  return store.writes == 0 && store.slots[0] == zeroLegacy &&
         store.slots[1] == corruptCurrent;
}
}  // namespace

int main() {
  const struct {
    const char* name;
    bool (*check)();
  } cases[] = {
      {"CPP1 load and upgrade with legacy defaults", carriesForwardMissingV1Fields},
      {"CPP2 upgrade, cross-setting saves and restart", upgradesV2AndPreservesCustomerSettings},
      {"failed CPP3 publication retains CPP2", restoresLegacyAfterFailedPublication},
      {"damaged newer CPP3 falls back to CPP2", fallsBackToLegacyAfterDamagedV3},
      {"historically valid target above 999 retains unrelated settings",
       retainsOtherSettingsForLegacyLapTargetOver999},
      {"shared lap targets 1, 999, 1000 and UINT32_MAX persist after restart",
       sharedLapBoundariesSurviveRestart},
      {"invalid legacy and checksum-damaged current records remain invalid",
       rejectsCorruptRecordsWithoutRewriting},
  };
  bool allPassed = true;
  for (const auto& test : cases) {
    const bool passed = test.check();
    std::cout << (passed ? "PASS " : "FAIL ") << test.name << '\n';
    allPassed &= passed;
  }
  return allPassed ? 0 : 1;
}
