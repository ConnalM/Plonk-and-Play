#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "pp/persistent_storage.h"

using namespace pp;

class TrackStore final : public Store {
public:
  bool online = true;
  int failRemoveSlot = -1;
  int failPublishSlot = -1;
  int corruptPublishSlot = -1;
  unsigned genericWrites = 0;
  std::vector<unsigned> trackWrites;
  std::vector<unsigned> removals;
  std::vector<std::vector<uint8_t>> slots = std::vector<std::vector<uint8_t>>(256);

  bool available() const override { return online; }
  bool read(unsigned slot, uint8_t* out, size_t size) override {
    if (!online || !out || slot >= slots.size() || slots[slot].size() != size) return false;
    memcpy(out, slots[slot].data(), size); return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    ++genericWrites;
    if (!online || !data || slot >= slots.size()) return false;
    slots[slot].assign(data, data + size); return true;
  }
  bool writeTrackRecord(unsigned slot, const uint8_t* data, size_t size) override {
    trackWrites.push_back(slot);
    if (!online || !data || slot >= slots.size()) return false;
    if (!slots[slot].empty()) {
      if (static_cast<int>(slot) == failRemoveSlot) { failRemoveSlot = -1; return false; }
      removals.push_back(slot); slots[slot].clear();
    }
    if (static_cast<int>(slot) == failPublishSlot) { failPublishSlot = -1; return false; }
    slots[slot].assign(data, data + size);
    if (static_cast<int>(slot) == corruptPublishSlot) {
      corruptPublishSlot = -1;
      if (!slots[slot].empty()) slots[slot][0] ^= 0x5a;
    }
    return true;
  }
};

static int failures = 0;
static void check(const char* id, bool ok, const char* detail) {
  std::printf("PSF %s %s %s\n", id, ok ? "PASS" : "FAIL", detail);
  if (!ok) ++failures;
}

static uint32_t sum(const uint8_t* p, size_t n) {
  uint32_t h = 2166136261u; while (n--) h = (h ^ *p++) * 16777619u; return h;
}

static void put32(uint8_t*& p, uint32_t v) {
  *p++ = uint8_t(v); *p++ = uint8_t(v >> 8); *p++ = uint8_t(v >> 16); *p++ = uint8_t(v >> 24);
}
static void put64(uint8_t*& p, uint64_t v) {
  for (unsigned i = 0; i < 8; ++i) *p++ = uint8_t(v >> (i * 8));
}

static std::vector<uint8_t> legacyRecord(uint32_t era, Time lane1, Time lane2, Time track, uint8_t trackLane) {
  const size_t size = 4 + 4 + 4 + PP_MAX_ENTRIES * 8 + 8 + 1 + 4;
  std::vector<uint8_t> bytes(size); uint8_t* p = bytes.data();
  put32(p, VersionedTrackRecordStore::Magic); put32(p, VersionedTrackRecordStore::LegacyVersion); put32(p, era);
  for (uint8_t i = 0; i < PP_MAX_ENTRIES; ++i) put64(p, i == 0 ? lane1 : (i == 1 ? lane2 : 0));
  put64(p, track); *p++ = trackLane;
  put32(p, sum(bytes.data(), size - 4)); return bytes;
}

static bool sameRecord(VersionedTrackRecordStore& records, Time lane1, Time lane2, Time track, uint32_t era) {
  return records.lanePb(1) == lane1 && records.lanePb(2) == lane2 &&
         records.trackRecord() == track && records.era() == era;
}

int main() {
  static_assert(VersionedTrackRecordStore::AlternateSlot != VersionedTrackRecordStore::Slot, "A/B slots must differ");
  static_assert(VersionedTrackRecordStore::AlternateSlot > 208, "alternate must not collide with History metadata");

  // Existing v1 slot-7 data remains readable without a destructive migration.
  TrackStore legacy;
  legacy.slots[VersionedTrackRecordStore::Slot] = legacyRecord(7, 2300000, 1800000, 1800000, 2);
  VersionedTrackRecordStore legacyReload(legacy);
  check("PSF.TR.1", sameRecord(legacyReload, 2300000, 1800000, 1800000, 7), "legacy_slot7_recovery=1");

  // First new publication goes to the dedicated alternate, not generic write.
  bool first = legacyReload.observe(1, 1700000);
  const bool firstTarget = !legacy.trackWrites.empty() && legacy.trackWrites.back() == VersionedTrackRecordStore::AlternateSlot && legacy.genericWrites == 0;
  check("PSF.TR.2", first && firstTarget && legacy.slots[VersionedTrackRecordStore::Slot].size() > 0, "first_alternate_publication=1");

  // A subsequent generation alternates to slot 7 and safely replaces the
  // inactive legacy copy while preserving the active alternate until verify.
  const size_t removalsBefore = legacy.removals.size();
  bool second = legacyReload.observe(2, 1500000);
  const bool alternated = second && legacy.trackWrites.back() == VersionedTrackRecordStore::Slot &&
                         legacy.removals.size() == removalsBefore + 1 && legacy.removals.back() == VersionedTrackRecordStore::Slot;
  VersionedTrackRecordStore secondReload(legacy);
  check("PSF.TR.3", alternated && sameRecord(secondReload, 1700000, 1500000, 1500000, 7), "subsequent_ab_alternation_and_recovery=1");

  // Failure to remove the inactive target leaves RAM and the active copy intact.
  legacy.failRemoveSlot = VersionedTrackRecordStore::AlternateSlot;
  const bool removeFailed = !secondReload.observe(2, 1400000) &&
                            sameRecord(secondReload, 1700000, 1500000, 1500000, 7);
  VersionedTrackRecordStore afterRemoveFailure(legacy);
  check("PSF.TR.4", removeFailed && sameRecord(afterRemoveFailure, 1700000, 1500000, 1500000, 7), "inactive_remove_failure_preserves_active_and_ram=1");

  // Failure after removal but before publication is also recoverable from the
  // still-authoritative copy.
  legacy.failPublishSlot = VersionedTrackRecordStore::AlternateSlot;
  const bool publishFailed = !secondReload.observe(2, 1400000) &&
                             sameRecord(secondReload, 1700000, 1500000, 1500000, 7);
  VersionedTrackRecordStore afterPublishFailure(legacy);
  check("PSF.TR.5", publishFailed && sameRecord(afterPublishFailure, 1700000, 1500000, 1500000, 7), "post_remove_failure_preserves_active=1");

  // A torn/corrupt newer copy is rejected and recovery selects the older one.
  legacy.corruptPublishSlot = VersionedTrackRecordStore::AlternateSlot;
  const bool verifyFailed = !secondReload.observe(2, 1400000);
  VersionedTrackRecordStore afterVerifyFailure(legacy);
  check("PSF.TR.6", verifyFailed && sameRecord(afterVerifyFailure, 1700000, 1500000, 1500000, 7), "torn_newer_copy_falls_back=1");

  // Highest valid generation wins when both copies are valid.
  TrackStore highest;
  VersionedTrackRecordStore writer(highest);
  const bool g1 = writer.observe(1, 3000000);
  const bool g2 = writer.observe(1, 2500000);
  const bool g3 = writer.observe(2, 2000000);
  VersionedTrackRecordStore highestReload(highest);
  check("PSF.TR.7", g1 && g2 && g3 && sameRecord(highestReload, 2500000, 2000000, 2000000, 1), "highest_valid_generation_selected=1");

  // Unchanged observations do not rewrite either copy, and success updates RAM
  // only after the read-back verification has completed.
  const size_t writesBefore = highest.trackWrites.size();
  const bool unchanged = highestReload.observe(1, 2500000) && highest.trackWrites.size() == writesBefore;
  check("PSF.TR.8", unchanged, "unchanged_observation_no_rewrite=1");

  TrackStore failed;
  VersionedTrackRecordStore failedRecords(failed);
  failedRecords.observe(1, 4000000);
  const Time before = failedRecords.lanePb(1);
  failed.failPublishSlot = static_cast<int>(VersionedTrackRecordStore::Slot);
  const bool failedRam = !failedRecords.observe(1, 1000000) && failedRecords.lanePb(1) == before;
  check("PSF.TR.9", failedRam, "ram_updates_only_after_durable_verification=1");

  // Slot 209 is the dedicated record namespace and cannot overlap History's
  // payload (8..207) or metadata (6/208) slots.
  const bool noCollision = VersionedTrackRecordStore::AlternateSlot > 8 + 200 - 1 &&
                           VersionedTrackRecordStore::AlternateSlot != 208;
  check("PSF.TR.10", noCollision, "track_record_namespace_does_not_collide_with_history=1");
  return failures ? 1 : 0;
}
