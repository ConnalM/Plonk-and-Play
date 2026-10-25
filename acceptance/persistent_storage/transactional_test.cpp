#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#include "pp/persistent_storage.h"

using namespace pp;

class TestStore final : public Store {
public:
  bool online = true;
  int failWriteSlot = -1;
  int failReadSlot = -1;
  int corruptWriteSlot = -1;
  int failMetadataRemoveSlot = -1;
  int failMetadataPublishSlot = -1;
  std::vector<unsigned> metadataRemoved;
  std::vector<unsigned> unreferencedWrites;
  std::vector<std::vector<uint8_t>> slots = std::vector<std::vector<uint8_t>>(256);

  bool available() const override { return online; }

  bool read(unsigned slot, uint8_t* out, size_t length) override {
    if (!online || !out || slot >= slots.size() || static_cast<int>(slot) == failReadSlot) {
      if (static_cast<int>(slot) == failReadSlot) failReadSlot = -1;
      return false;
    }
    if (slots[slot].size() != length) return false;
    memcpy(out, slots[slot].data(), length);
    return true;
  }

  bool write(unsigned slot, const uint8_t* data, size_t length) override {
    if (!online || !data || slot >= slots.size()) return false;
    if (static_cast<int>(slot) == failWriteSlot) {
      failWriteSlot = -1;
      return false;
    }
    slots[slot].assign(data, data + length);
    if (static_cast<int>(slot) == corruptWriteSlot) {
      corruptWriteSlot = -1;
      if (!slots[slot].empty()) slots[slot][0] ^= 0x5a;
    }
    return true;
  }

  bool writeUnreferenced(unsigned slot, const uint8_t* data, size_t length) override {
    unreferencedWrites.push_back(slot);
    return write(slot, data, length);
  }

  bool writeMetadata(unsigned slot, const uint8_t* data, size_t length) override {
    if (!online || !data || slot >= slots.size()) return false;
    if (static_cast<int>(slot) == failMetadataPublishSlot) {
      failMetadataPublishSlot = -1;
      return false;
    }
    if (static_cast<int>(slot) == failWriteSlot) {
      failWriteSlot = -1;
      return false;
    }
    if (!slots[slot].empty()) {
      if (static_cast<int>(slot) == failMetadataRemoveSlot) {
        failMetadataRemoveSlot = -1;
        return false;
      }
      metadataRemoved.push_back(slot);
      slots[slot].clear();
    }
    slots[slot].assign(data, data + length);
    if (static_cast<int>(slot) == corruptWriteSlot) {
      corruptWriteSlot = -1;
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

static bool loadPayload(HistoryStore& history, uint8_t newest, const char* expected) {
  uint8_t bytes[64]{};
  size_t length = 0;
  uint32_t sequence = 0;
  return history.loadNewest(newest, bytes, sizeof(bytes), length, sequence) &&
         length == strlen(expected) && memcmp(bytes, expected, length) == 0;
}

int main() {
  TestStore store;
  SlotHistoryStore history(store);
  uint32_t sequence = 0;
  const char first[] = "first-committed";
  const char second[] = "second-committed";
  const char orphan[] = "orphan-must-stay-hidden";

  bool firstOk = history.append(reinterpret_cast<const uint8_t*>(first), strlen(first), sequence) &&
                 sequence == 1 && history.count() == 1 && loadPayload(history, 0, first);
  check("PSF.15", firstOk, "initial_payload_and_commit=1");

  store.failWriteSlot = 9; // first spare payload slot
  uint32_t failed = 0;
  bool payloadFailure = !history.append(reinterpret_cast<const uint8_t*>(second), strlen(second), failed) &&
                        failed == 0 && history.count() == 1 && loadPayload(history, 0, first);
  check("PSF.15", payloadFailure, "payload_write_failure_hides_uncommitted=1");

  store.corruptWriteSlot = 9;
  bool verifyFailure = !history.append(reinterpret_cast<const uint8_t*>(second), strlen(second), failed) &&
                       failed == 0 && history.count() == 1 && loadPayload(history, 0, first);
  check("PSF.17", verifyFailure, "payload_verify_failure_hides_uncommitted=1");

  bool secondOk = history.append(reinterpret_cast<const uint8_t*>(second), strlen(second), sequence) &&
                  sequence == 2 && history.count() == 2 && loadPayload(history, 0, second) && loadPayload(history, 1, first);
  check("PSF.18", secondOk, "committed_order_and_reconstruction=1");

  // The next publication targets the inactive slot 208, which already holds
  // generation 1. It is replaced only through the metadata boundary; the
  // active slot 6 remains untouched until the new copy verifies.
  const size_t removedBefore = store.metadataRemoved.size();
  bool replacement = history.append(reinterpret_cast<const uint8_t*>(orphan), strlen(orphan), sequence) &&
                     sequence == 3 && history.count() == 3 &&
                     store.metadataRemoved.size() == removedBefore + 1 &&
                     store.metadataRemoved.back() == 208 &&
                     std::find(store.unreferencedWrites.begin(), store.unreferencedWrites.end(), 6u) == store.unreferencedWrites.end() &&
                     loadPayload(history, 0, orphan) && loadPayload(history, 1, second);
  check("PSF.16", replacement, "existing_inactive_metadata_replaced_active_preserved=1");

  // A failed inactive-target removal leaves the current authoritative copy
  // intact and does not publish a new generation.
  store.failMetadataRemoveSlot = 6;
  const size_t removedAtFailure = store.metadataRemoved.size();
  bool removalFailure = !history.append(reinterpret_cast<const uint8_t*>(first), strlen(first), failed) &&
                        failed == 0 && history.count() == 3 &&
                        store.metadataRemoved.size() == removedAtFailure &&
                        loadPayload(history, 0, orphan) && loadPayload(history, 2, first);
  check("PSF.21", removalFailure, "inactive_metadata_remove_failure_preserves_active=1");

  // A failure after the inactive copy was removed but before publication is
  // also safe: the old active copy remains the only valid generation.
  store.failMetadataPublishSlot = 6;
  bool publishFailure = !history.append(reinterpret_cast<const uint8_t*>(first), strlen(first), failed) &&
                        failed == 0 && history.count() == 3 &&
                        loadPayload(history, 0, orphan) && loadPayload(history, 2, first);
  check("PSF.22", publishFailure, "post_remove_metadata_failure_preserves_active=1");

  bool metadataBoundary = !store.unreferencedWrites.empty() &&
                          std::find(store.unreferencedWrites.begin(), store.unreferencedWrites.end(), 6u) == store.unreferencedWrites.end() &&
                          std::find(store.unreferencedWrites.begin(), store.unreferencedWrites.end(), 208u) == store.unreferencedWrites.end();
  check("PSF.23", metadataBoundary, "metadata_never_uses_writeUnreferenced=1");

  TestStore verifyStore;
  SlotHistoryStore verifyHistory(verifyStore);
  uint32_t verifySequence = 0;
  const bool verifyFirst = verifyHistory.append(reinterpret_cast<const uint8_t*>(first), strlen(first), verifySequence);
  verifyStore.corruptWriteSlot = 6; // inactive metadata target on generation 2
  uint32_t verifyFailedSequence = 0;
  const bool metadataVerifyFailure = !verifyHistory.append(reinterpret_cast<const uint8_t*>(second), strlen(second), verifyFailedSequence) &&
                             verifyFailedSequence == 0 && verifyHistory.count() == 1 &&
                             loadPayload(verifyHistory, 0, first);
  SlotHistoryStore verifyReload(verifyStore);
  const bool verifyRecovery = verifyReload.count() == 1 && loadPayload(verifyReload, 0, first);
  check("PSF.24", verifyFirst && metadataVerifyFailure && verifyRecovery,
        "metadata_readback_verification_failure_preserves_old_generation=1");

  SlotHistoryStore reloaded(store);
  bool reloadedCommitted = reloaded.count() == 3 && loadPayload(reloaded, 0, orphan) && loadPayload(reloaded, 1, second);
  check("PSF.20", reloadedCommitted, "stale_orphan_hidden_after_reload=1");

  TestStore corrupted;
  SlotHistoryStore raw(corrupted);
  VersionedHistoryStore versioned(raw);
  RaceEngineModule::CompletedRaceResult result{};
  result.sealed = true;
  result.valid = true;
  result.formatVersion = RaceEngineModule::ResultFormatVersion;
  result.mode = SessionMode::Endurance;
  result.entryCount = 1;
  result.entries[0].lane = 1;
  result.entries[0].completed = true;
  uint32_t resultSequence = 0;
  bool encodedCommit = versioned.append(reinterpret_cast<const uint8_t*>(&result), sizeof(result), resultSequence);
  // The first committed payload is physical slot 8. Versioned decoding must
  // reject corruption even though the slot-level backend can still read it.
  if (corrupted.slots[8].size() > 4) corrupted.slots[8][4] ^= 0x33;
  RaceEngineModule::CompletedRaceResult recovered{};
  size_t resultLength = 0;
  uint32_t recoveredSequence = 0;
  bool corruptionRejected = !versioned.loadNewest(0, reinterpret_cast<uint8_t*>(&recovered), sizeof(recovered),
                                                   resultLength, recoveredSequence);
  check("PSF.18", encodedCommit && corruptionRejected, "versioned_checksum_rejects_corrupt_payload=1");

  TestStore fallback;
  SlotHistoryStore fallbackHistory(fallback);
  uint32_t ignored = 0;
  bool f1 = fallbackHistory.append(reinterpret_cast<const uint8_t*>(first), strlen(first), ignored);
  bool f2 = fallbackHistory.append(reinterpret_cast<const uint8_t*>(second), strlen(second), ignored);
  // Generation 2 is in metadata slot 6; corrupting it must fall back to the
  // prior committed generation in slot 208 rather than exposing partial data.
  if (fallback.slots[6].size() > 1) fallback.slots[6][1] ^= 0x7f;
  SlotHistoryStore fallbackReload(fallback);
  bool fallbackOk = f1 && f2 && fallbackReload.count() == 1 && loadPayload(fallbackReload, 0, first);
  check("PSF.17", fallbackOk, "older_valid_metadata_generation_survives=1");

  return failures ? 1 : 0;
}
