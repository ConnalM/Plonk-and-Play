#pragma once

#include "core.h"
#include "session_definition.h"

namespace pp {

// Installation-level two-lane detector assignment.  This is deliberately
// separate from Memory's customer race settings record: Track Setup is a
// remembered installation setting, while the proposed Session Definition
// remains the sole authority for the next race.
struct TrackConfiguration {
  uint8_t laneCount = 2;
  InputIdentity lane1 = InputModule::simulatedDetectorIdentity(0);
  InputIdentity lane2 = InputModule::simulatedDetectorIdentity(1);

  bool valid() const {
    return laneCount == 2 && lane1.device != 0 && lane1.capability != 0 &&
           lane2.device != 0 && lane2.capability != 0 && lane1 != lane2;
  }

  bool operator==(const TrackConfiguration& other) const {
    return laneCount == other.laneCount && lane1 == other.lane1 && lane2 == other.lane2;
  }
  bool operator!=(const TrackConfiguration& other) const { return !(*this == other); }

  static TrackConfiguration fromSetup(const ProposedRaceSetup& original) {
    const ProposedRaceSetup setup = normalised(original);
    TrackConfiguration value{};
    value.laneCount = setup.activeLanes;
    if (setup.activeLanes > 0) value.lane1 = setup.entries[0].startFinish.input;
    if (setup.activeLanes > 1) value.lane2 = setup.entries[1].startFinish.input;
    return value;
  }
};

// Race Control uses this narrow seam to make an installation assignment
// durable before publishing the corresponding proposal. The production
// implementation remains backed by the existing Store boundary; tests can
// inject deterministic failures without involving Browser or hardware.
class TrackConfigurationPersistence {
public:
  virtual ~TrackConfigurationPersistence() = default;
  virtual bool save(const TrackConfiguration& configuration) = 0;
};

// A/B remembered Track Setup records.  Slots 2 and 3 are reserved for this
// installation namespace and never overlap Memory's customer settings slots
// 0 and 1. Slot 4 is an unreferenced staging slot: load() deliberately never
// considers it authoritative. A candidate is read-back verified there before
// the Store metadata boundary publishes it to the inactive A/B record. This
// makes cleanup best-effort without allowing an unverified candidate to win
// reconstruction after a restart.
class TrackConfigurationMemory final : public TrackConfigurationPersistence {
public:
  static constexpr size_t RecordSize = 32;
  static constexpr unsigned FirstSlot = 2;
  static constexpr unsigned SecondSlot = 3;
  static constexpr unsigned StagingSlot = 4;

  explicit TrackConfigurationMemory(Store& store) : store_(store) {}

  LoadStatus load(TrackConfiguration& configuration) {
    configuration = TrackConfiguration{};
    if (!store_.available()) return LoadStatus::DefaultsStorageError;
    uint8_t first[RecordSize]{}, second[RecordSize]{};
    const bool hasFirst = store_.read(FirstSlot, first, sizeof(first));
    const bool hasSecond = store_.read(SecondSlot, second, sizeof(second));
    TrackConfiguration firstValue{}, secondValue{};
    uint32_t firstGeneration = 0, secondGeneration = 0;
    const bool firstValid = hasFirst && decode(first, firstValue, firstGeneration);
    const bool secondValid = hasSecond && decode(second, secondValue, secondGeneration);
    if (!firstValid && !secondValid) {
      return hasFirst || hasSecond ? LoadStatus::DefaultsInvalid : LoadStatus::DefaultsMissing;
    }
    const bool useSecond = secondValid && (!firstValid || secondGeneration > firstGeneration);
    configuration = useSecond ? secondValue : firstValue;
    generation_ = useSecond ? secondGeneration : firstGeneration;
    activeSlot_ = useSecond ? SecondSlot : FirstSlot;
    return LoadStatus::Remembered;
  }

  bool save(const TrackConfiguration& configuration) override {
    if (!store_.available() || !configuration.valid() || generation_ == UINT32_MAX) return false;
    uint8_t encoded[RecordSize]{};
    encode(configuration, generation_ + 1, encoded);
    const unsigned target = activeSlot_ == FirstSlot ? SecondSlot : FirstSlot;
    // Stage outside the remembered A/B namespace. Even if cleanup later fails,
    // load() cannot mistake this object for an accepted installation change.
    if (!store_.writeUnreferenced(StagingSlot, encoded, sizeof(encoded))) return false;
    uint8_t verify[RecordSize]{};
    if (!store_.read(StagingSlot, verify, sizeof(verify)) || memcmp(encoded, verify, sizeof(encoded)) != 0) {
      // Cleanup is deliberately best-effort. The staged object is outside the
      // load() namespace, so even a failed reclaim cannot become authoritative.
      (void)store_.reclaim(StagingSlot);
      return false;
    }
    // This is the sole publication boundary. Store::writeMetadata() is
    // required to leave the prior inactive copy authoritative when it returns
    // false; NVS putBytes() provides that key-level commit boundary and the SD
    // backend stages/verifies before rename. No post-publication read-back is
    // used: a failed read-back after publication cannot be safely rolled back
    // through the generic Store contract.
    if (!store_.writeMetadata(target, encoded, sizeof(encoded))) {
      (void)store_.reclaim(StagingSlot);
      return false;
    }
    // Reclamation is housekeeping only. A stale staging record is ignored by
    // load() and therefore cannot change the accepted configuration.
    (void)store_.reclaim(StagingSlot);
    activeSlot_ = target;
    ++generation_;
    return true;
  }

  uint32_t generation() const { return generation_; }
  unsigned activeSlot() const { return activeSlot_; }

  static void encode(const TrackConfiguration& configuration, uint32_t generation, uint8_t* bytes) {
    memset(bytes, 0, RecordSize);
    put(bytes, 0x31545043u); // CPT1: Configuration/Track, version 1.
    put(bytes + 4, generation);
    bytes[8] = configuration.laneCount;
    put(bytes + 12, configuration.lane1.device);
    put(bytes + 16, configuration.lane1.capability);
    put(bytes + 20, configuration.lane2.device);
    put(bytes + 24, configuration.lane2.capability);
    put(bytes + 28, checksum(bytes, 28));
  }

private:
  Store& store_;
  uint32_t generation_ = 0;
  unsigned activeSlot_ = SecondSlot;

  static void put(uint8_t* bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (8 * i));
  }
  static uint32_t get(const uint8_t* bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[i]) << (8 * i);
    return value;
  }
  static uint32_t checksum(const uint8_t* bytes, size_t length) {
    uint32_t value = 2166136261u;
    while (length--) value = (value ^ *bytes++) * 16777619u;
    return value;
  }
  static bool decode(const uint8_t* bytes, TrackConfiguration& configuration, uint32_t& generation) {
    if (get(bytes) != 0x31545043u || get(bytes + 28) != checksum(bytes, 28)) return false;
    configuration.laneCount = bytes[8];
    configuration.lane1 = InputIdentity(get(bytes + 12), uint16_t(get(bytes + 16)));
    configuration.lane2 = InputIdentity(get(bytes + 20), uint16_t(get(bytes + 24)));
    generation = get(bytes + 4);
    return generation > 0 && configuration.valid();
  }
};

} // namespace pp
