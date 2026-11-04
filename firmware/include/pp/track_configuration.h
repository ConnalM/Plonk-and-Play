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
// 0 and 1.  Publication is the same write-then-read-verify pattern used by
// Memory; the old record remains intact until its replacement is verified.
class TrackConfigurationMemory final : public TrackConfigurationPersistence {
public:
  static constexpr size_t RecordSize = 32;
  static constexpr unsigned FirstSlot = 2;
  static constexpr unsigned SecondSlot = 3;

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
    // Track Setup is installation metadata. Use the Store's metadata boundary
    // so an existing inactive copy can be replaced atomically by backends that
    // require that distinction (NVS retains source compatibility).
    if (!store_.writeMetadata(target, encoded, sizeof(encoded))) return false;
    uint8_t verify[RecordSize]{};
    if (!store_.read(target, verify, sizeof(verify)) || memcmp(encoded, verify, sizeof(encoded)) != 0) {
      // A successful write followed by an unverified read must never become
      // the remembered record after restart. Invalidate only the inactive
      // target; the previous committed copy remains untouched.
      uint8_t invalid[RecordSize]{};
      (void)store_.writeMetadata(target, invalid, sizeof(invalid));
      return false;
    }
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
