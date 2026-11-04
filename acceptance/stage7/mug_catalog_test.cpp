#include "pp/mug_catalog.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool require(bool value, const char* message) {
  if (!value) std::cerr << "FAIL: " << message << "\n";
  return value;
}

class CatalogStore final : public pp::Store {
public:
  static constexpr unsigned SlotCount = 213;
  bool failStagingWrite = false;
  bool failStagingRead = false;
  bool failPublication = false;
  bool failCleanup = false;

  bool available() const override { return available_; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!available_ || slot >= SlotCount || !data || slots_[slot].size() != size) return false;
    std::memcpy(data, slots_[slot].data(), size);
    if (slot == pp::MugCatalog::StagingSlot && failStagingRead) {
      data[0] ^= 0xffu;
      failStagingRead = false;
    }
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (!available_ || slot >= SlotCount || !data) return false;
    if (slot == pp::MugCatalog::StagingSlot && failStagingWrite) return false;
    slots_[slot].assign(data, data + size);
    return true;
  }
  bool writeMetadata(unsigned slot, const uint8_t* data, size_t size) override {
    if (failPublication) return false;
    return write(slot, data, size);
  }
  bool reclaim(unsigned slot) override {
    if (failCleanup) return false;
    if (slot < SlotCount) slots_[slot].clear();
    return true;
  }
  void setAvailable(bool value) { available_ = value; }
  void seed(unsigned slot, const std::vector<uint8_t>& value) { slots_[slot] = value; }
  const std::vector<uint8_t>& raw(unsigned slot) const { return slots_[slot]; }

private:
  bool available_ = true;
  std::array<std::vector<uint8_t>, SlotCount> slots_{};
};

bool hasName(const pp::MugCatalog& catalog, uint32_t id, const char* expected,
             bool includeArchived = true) {
  pp::MugCatalog::Entry value{};
  return catalog.find(id, value, includeArchived) && std::strcmp(value.name, expected) == 0;
}

bool restartHas(CatalogStore& store, uint32_t id, const char* expected,
                bool includeArchived = true) {
  pp::MugCatalog restarted(store);
  return restarted.load() == pp::MugCatalog::LoadResult::Remembered &&
         hasName(restarted, id, expected, includeArchived);
}

} // namespace

int main() {
  using namespace pp;
  bool ok = true;

  CatalogStore store;
  MugCatalog catalog(store);
  ok &= require(catalog.load() == MugCatalog::LoadResult::Empty && catalog.count() == 0 &&
                catalog.nextId() == MugCatalog::FirstSavedId,
                "empty catalog is a normal first-use state");

  uint32_t id = 0;
  ok &= require(catalog.create("Connal", id) == MugCatalog::Result::Accepted && id == 3,
                "first saved MUG receives permanent ID 3");
  uint32_t secondId = 0;
  ok &= require(catalog.create("Connal", secondId) == MugCatalog::Result::Accepted && secondId == 4,
                "duplicate display names are permitted and IDs remain unique");
  ok &= require(catalog.count() == 2 && catalog.activeCount() == 2 && catalog.nextId() == 5,
                "catalog tracks saved entries and next monotonic ID");
  ok &= require(restartHas(store, 3, "Connal") && restartHas(store, 4, "Connal"),
                "committed catalog reconstructs after restart");

  MugCatalog::Entry found{};
  ok &= require(catalog.rename(3, "Connal Renamed") == MugCatalog::Result::Accepted &&
                catalog.find(3, found), "rename is accepted without changing identity");
  ok &= require(hasName(catalog, 3, "Connal Renamed") && restartHas(store, 3, "Connal Renamed"),
                "renamed display name persists while ID remains 3");

  ok &= require(catalog.archive(3) == MugCatalog::Result::Accepted &&
                catalog.activeCount() == 1 && hasName(catalog, 3, "Connal Renamed"),
                "archive retains the historical entry but removes it from active count");
  MugCatalog::Entry archived{};
  ok &= require(!catalog.find(3, archived, false) &&
                catalog.rename(3, "Nope") == MugCatalog::Result::Archived &&
                catalog.archive(3) == MugCatalog::Result::AlreadyArchived,
                "archived identities cannot be selected, renamed or archived twice");
  uint32_t thirdId = 0;
  ok &= require(catalog.create("Freya", thirdId) == MugCatalog::Result::Accepted && thirdId == 5,
                "archived IDs are never reused");
  ok &= require(catalog.archive(1) == MugCatalog::Result::UnknownId &&
                catalog.rename(999, "Unknown") == MugCatalog::Result::UnknownId,
                "built-in/default and unknown IDs are not catalog entries");

  std::string forty(40, 'x');
  std::string fortyOne(41, 'x');
  std::string fortyUtf8;
  for (unsigned i = 0; i < 40; ++i) fortyUtf8 += "\xC3\xA9";
  std::string fortyOneUtf8 = fortyUtf8 + "\xC3\xA9";
  ok &= require(catalog.create("", id) == MugCatalog::Result::InvalidName &&
                catalog.create(fortyOne.c_str(), id) == MugCatalog::Result::InvalidName &&
                catalog.create("\xC0\x80", id) == MugCatalog::Result::InvalidName &&
                catalog.create(fortyOneUtf8.c_str(), id) == MugCatalog::Result::InvalidName &&
                catalog.create(fortyUtf8.c_str(), id) == MugCatalog::Result::Accepted &&
                catalog.create(forty.c_str(), id) == MugCatalog::Result::Accepted,
                "names require strict valid UTF-8 with one to forty characters");

  CatalogStore isolated;
  const std::vector<uint8_t> sentinel = {0x31, 0x32, 0x33, 0x34};
  for (unsigned slot : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 207u, 208u, 209u}) isolated.seed(slot, sentinel);
  MugCatalog isolatedCatalog(isolated);
  ok &= require(isolatedCatalog.load() == MugCatalog::LoadResult::Empty &&
                isolatedCatalog.create("Isolated", id) == MugCatalog::Result::Accepted,
                "catalog can publish in its dedicated namespace");
  bool unchanged = true;
  for (unsigned slot : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 207u, 208u, 209u})
    unchanged = unchanged && isolated.raw(slot) == sentinel;
  ok &= require(unchanged && !isolated.raw(MugCatalog::CatalogSlotA).empty(),
                "catalog slots do not collide with Memory, Track, History or Record slots");

  CatalogStore failureStore;
  MugCatalog failureCatalog(failureStore);
  ok &= require(failureCatalog.load() == MugCatalog::LoadResult::Empty &&
                failureCatalog.create("Original", id) == MugCatalog::Result::Accepted,
                "failure matrix has a committed baseline");
  auto rejectedRenamePreserves = [&](const char* message) {
    MugCatalog restarted(failureStore);
    return require(restarted.load() == MugCatalog::LoadResult::Remembered &&
                   hasName(restarted, 3, "Original"), message);
  };

  failureStore.failStagingWrite = true;
  ok &= require(failureCatalog.rename(3, "WriteFail") == MugCatalog::Result::StorageUnavailable,
                "staging write failure rejects rename");
  ok &= rejectedRenamePreserves("staging write failure preserves restart catalog");
  failureStore.failStagingWrite = false;

  failureStore.failStagingRead = true;
  ok &= require(failureCatalog.rename(3, "ReadFail") == MugCatalog::Result::StorageUnavailable,
                "staging read-back failure rejects rename");
  ok &= rejectedRenamePreserves("staging read-back failure preserves restart catalog");

  failureStore.failPublication = true;
  ok &= require(failureCatalog.rename(3, "PublishFail") == MugCatalog::Result::StorageUnavailable,
                "publication failure rejects rename");
  ok &= rejectedRenamePreserves("publication failure preserves restart catalog");
  failureStore.failPublication = false;

  failureStore.failCleanup = true;
  ok &= require(failureCatalog.rename(3, "CleanupFail") == MugCatalog::Result::Accepted &&
                restartHas(failureStore, 3, "CleanupFail"),
                "cleanup failure is non-authoritative housekeeping after publication");
  failureStore.failCleanup = false;

  ok &= require(failureCatalog.rename(3, "Recovered") == MugCatalog::Result::Accepted &&
                restartHas(failureStore, 3, "Recovered"),
                "catalog recovers and accepts a later operation after failed saves");

  CatalogStore capacityStore;
  MugCatalog capacity(capacityStore);
  ok &= require(capacity.load() == MugCatalog::LoadResult::Empty,
                "capacity test starts from an empty catalog");
  for (uint16_t i = 0; i < MugCatalog::MaxProfiles; ++i) {
    char name[16]{};
    std::snprintf(name, sizeof(name), "MUG-%u", unsigned(i));
    uint32_t created = 0;
    ok &= require(capacity.create(name, created) == MugCatalog::Result::Accepted &&
                  created == uint32_t(MugCatalog::FirstSavedId + i),
                  "profile capacity entries allocate monotonically");
    if (!ok) break;
  }
  ok &= require(capacity.count() == MugCatalog::MaxProfiles &&
                capacity.create("Overflow", id) == MugCatalog::Result::CapacityFull,
                "101st saved profile is rejected at the bounded capacity");

  if (!ok) return 1;
  std::cout << "WP7 saved MUG catalog foundation PASS\n";
  return 0;
}
