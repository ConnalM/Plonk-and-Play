#pragma once

// Real Core runtime safety boundaries.  These types deliberately do not
// emulate a detector or a writable race-record store while the Core board and
// its SD/detector hardware have not been qualified.

#include "core.h"
#include "history_store.h"
#include "record_store.h"

namespace pp::core_runtime {

class UnavailableDetectorProvider final : public DetectorProvider {
 public:
  static constexpr InputIdentity laneIdentity(uint8_t lane) {
    // A Core-owned installation identity, not a simulated-detector identity.
    return InputIdentity(0x43505245u, lane);
  }

  uint8_t detectorCount() const override { return 0; }
  bool sample(uint8_t, Time, Time&) override { return false; }
  DetectorStatus status(uint8_t) const override { return DetectorStatus{}; }
};

class UnavailableHistoryStore final : public HistoryStore {
 public:
  bool append(const uint8_t*, size_t, uint32_t& sequence) override {
    sequence = 0;
    return false;
  }
  bool loadNewest(uint8_t, uint8_t*, size_t, size_t& length,
                  uint32_t& sequence) override {
    length = 0;
    sequence = 0;
    return false;
  }
  uint8_t count() override { return 0; }
  bool clear() override { return false; }
  StoreHealth health() const override { return StoreHealth::Unavailable; }
};

class UnavailableTrackRecordStore final : public TrackRecordStore {
 public:
  bool observe(uint8_t, Time) override { return false; }
  Time lanePb(uint8_t) override { return 0; }
  Time trackRecord() override { return 0; }
  uint32_t era() override { return 0; }
  bool clearLane(uint8_t) override { return false; }
  bool clearTrack() override { return false; }
  bool clearAll() override { return false; }
};

struct BenchSafetyState {
  bool detectorsQualified = false;
  bool raceStorageQualified = false;

  constexpr bool startPermitted() const {
    return detectorsQualified && raceStorageQualified;
  }
};

inline constexpr BenchSafetyState kUnqualifiedBenchSafety{};

}  // namespace pp::core_runtime
