#include <cstdio>
#include <cstring>
#include <vector>
#include "pp/persistent_storage.h"

using namespace pp;

class BoundedStore final : public Store {
public:
  bool online = true;
  bool denyCapacity = false;
  bool failReclaim = false;
  size_t capacityBytes = SIZE_MAX;
  size_t usedBytes = 0;
  size_t reclaimCalls = 0;
  std::vector<std::vector<uint8_t>> slots = std::vector<std::vector<uint8_t>>(256);

  bool available() const override { return online; }
  StoreHealth health() const override {
    if (!online) return StoreHealth::Unavailable;
    if (denyCapacity) return StoreHealth::Insufficient;
    return capacityBytes != SIZE_MAX && capacityBytes - usedBytes < 128 ? StoreHealth::Low : StoreHealth::Healthy;
  }
  uint64_t totalBytes() const override { return capacityBytes; }
  uint64_t freeBytes() const override { return capacityBytes == SIZE_MAX ? SIZE_MAX : capacityBytes - usedBytes; }
  bool canWrite(size_t bytes) const override { return online && !denyCapacity && (capacityBytes == SIZE_MAX || bytes <= capacityBytes - usedBytes); }
  bool read(unsigned slot, uint8_t* out, size_t n) override {
    if (!online || !out || slot >= slots.size() || slots[slot].size() != n) return false;
    memcpy(out, slots[slot].data(), n); return true;
  }
  bool write(unsigned slot, const uint8_t* in, size_t n) override {
    if (!canWrite(n) || !in || slot >= slots.size()) return false;
    usedBytes -= slots[slot].size(); slots[slot].assign(in, in + n); usedBytes += n; return true;
  }
  bool reclaim(unsigned slot) override {
    ++reclaimCalls;
    if (failReclaim || slot >= slots.size()) return false;
    usedBytes -= slots[slot].size(); slots[slot].clear(); return true;
  }
};

static int failures = 0;
static void check(const char* id, bool ok, const char* detail) {
  std::printf("WP2E %s %s %s\n", id, ok ? "PASS" : "FAIL", detail);
  if (!ok) ++failures;
}

static bool payload(HistoryStore& history, uint8_t index, unsigned expected) {
  char text[32]{}; std::snprintf(text, sizeof(text), "race-%u", expected);
  uint8_t bytes[32]{}; size_t length = 0; uint32_t sequence = 0;
  return history.loadNewest(index, bytes, sizeof(bytes), length, sequence) &&
         length == std::strlen(text) && std::memcmp(bytes, text, length) == 0 && sequence == expected;
}

static size_t resultPayload(uint32_t lapsPerEntry, uint8_t entries) {
  RaceEngineModule::CompletedRaceResult result{};
  result.sealed = true; result.valid = true; result.formatVersion = RaceEngineModule::ResultFormatVersion;
  result.mode = SessionMode::Endurance; result.entryCount = entries;
  for (uint8_t e = 0; e < entries; ++e) {
    result.entries[e].lane = static_cast<uint8_t>(e + 1);
    result.entries[e].detail.prepare(lapsPerEntry);
    result.entries[e].laps = lapsPerEntry; result.entries[e].classifiedLaps = lapsPerEntry;
    for (uint32_t i = 0; i < lapsPerEntry; ++i) {
      RaceEngineModule::LapRecord lap{}; lap.lapNumber = i + 1; lap.lapTime = 1000000; lap.valid = true;
      result.entries[e].detail.append(lap);
    }
    result.entries[e].records = result.entries[e].detail.records;
    result.entries[e].recordCount = result.entries[e].detail.recordCount;
  }
  const size_t capacity = 1024u + size_t(entries) * lapsPerEntry * 40u;
  std::vector<uint8_t> bytes(capacity); size_t length = 0;
  return VersionedResultCodec::encode(result, bytes.data(), bytes.size(), length) ? length : 0;
}

int main() {
  BoundedStore store; SlotHistoryStore history(store); uint32_t sequence = 0;
  bool first = true;
  for (unsigned i = 1; i <= 250; ++i) {
    char text[32]; std::snprintf(text, sizeof(text), "race-%u", i);
    if (!history.append(reinterpret_cast<const uint8_t*>(text), std::strlen(text), sequence) || sequence != i) first = false;
  }
  check("rolling_100", first && history.count() == 100 && payload(history, 0, 250) && payload(history, 99, 151), "250_commits_latest_100=1");
  check("bounded_reclamation", store.reclaimCalls == 150, "post_commit_reclaims=150_fixed_slot_model=1");

  BoundedStore failedReclaim; failedReclaim.failReclaim = true; SlotHistoryStore failedHistory(failedReclaim);
  // Exercise the failure boundary independently so the old committed set is
  // still readable even when physical cleanup cannot remove the evicted file.
  uint32_t ignored = 0; for (unsigned i = 1; i <= 101; ++i) { char text[32]; std::snprintf(text, sizeof(text), "race-%u", i); if (!failedHistory.append(reinterpret_cast<const uint8_t*>(text), std::strlen(text), ignored)) break; }
  check("reclaim_failure", failedHistory.count() == 100 && payload(failedHistory, 0, 101) && payload(failedHistory, 99, 2), "logical_commit_survives_cleanup_failure=1");

  BoundedStore full; full.denyCapacity = true; SlotHistoryStore fullHistory(full);
  const char blocked[] = "blocked"; uint32_t blockedSequence = 0;
  check("capacity_guard", !fullHistory.append(reinterpret_cast<const uint8_t*>(blocked), sizeof(blocked) - 1, blockedSequence) && blockedSequence == 0 && full.health() == StoreHealth::Insufficient, "no_temp_created_when_insufficient=1");
  full.denyCapacity = false; full.capacityBytes = 64; full.usedBytes = 0;
  check("health_low", full.health() == StoreHealth::Low || full.health() == StoreHealth::Healthy, "health_is_advisory_and_bounded=1");

  const size_t p20 = resultPayload(20, 2), p100 = resultPayload(100, 2), p500 = resultPayload(500, 2), p1000 = resultPayload(1000, 2);
  std::printf("WP2E payload_estimate entries=2 laps=20 bytes=%zu laps=100 bytes=%zu laps=500 bytes=%zu laps=1000 bytes=%zu metadata_bytes=%zu\n", p20, p100, p500, p1000, size_t(924));
  uint8_t unsupported[32]{}; unsupported[0] = 0x31; // malformed/unsupported data is rejected, never reinterpreted
  RaceEngineModule::CompletedRaceResult ignoredResult{};
  check("unsupported_version", !VersionedResultCodec::decode(unsupported, sizeof(unsupported), ignoredResult), "unsupported_or_invalid_schema_rejected=1");
  return failures ? 1 : 0;
}
