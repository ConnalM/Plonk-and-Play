#pragma once

#include "core.h"
#include <mutex>

namespace pp {

// These are presentation choices only.  They never identify a racer, input,
// lane assignment, or race entry.  The enum deliberately exposes only artwork
// already compiled into the accepted two-lane Taster source.
enum class TasterCarGraphic : uint8_t { GreenMgb = 0, RedEtype = 1 };

inline uint32_t tasterPresentationIdentityHash(const char* label,
                                                TasterCarGraphic graphic) {
  uint32_t hash = 2166136261U;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(label);
       p && *p; ++p) {
    hash ^= *p;
    hash *= 16777619U;
  }
  hash ^= static_cast<uint8_t>(graphic);
  return hash * 16777619U;
}

struct TasterPresentationPreferences {
  static constexpr size_t LabelBytes = 24;
  static constexpr size_t LabelCharacters = 16;

  char lane1Label[LabelBytes + 1] = "LANE 1";
  char lane2Label[LabelBytes + 1] = "LANE 2";
  TasterCarGraphic lane1Graphic = TasterCarGraphic::GreenMgb;
  TasterCarGraphic lane2Graphic = TasterCarGraphic::RedEtype;

  static TasterPresentationPreferences factoryDefaults() {
    return TasterPresentationPreferences{};
  }

  static bool validLabel(const char* value) {
    if (!value) return false;
    size_t bytes = 0, characters = 0;
    while (value[bytes]) {
      if (bytes == LabelBytes || characters == LabelCharacters) return false;
      const uint8_t first = static_cast<uint8_t>(value[bytes]);
      if (first < 0x20 || first == 0x7f || first == '"' || first == '\\') return false;
      size_t width = 1;
      if (first >= 0x80) {
        if ((first & 0xe0) == 0xc0) width = 2;
        else if ((first & 0xf0) == 0xe0) width = 3;
        else if ((first & 0xf8) == 0xf0) width = 4;
        else return false;
        if (bytes + width > LabelBytes) return false;
        for (size_t index = 1; index < width; ++index) {
          const uint8_t continuation = static_cast<uint8_t>(value[bytes + index]);
          if ((continuation & 0xc0) != 0x80) return false;
        }
      }
      bytes += width;
      ++characters;
    }
    return true;
  }

  bool valid() const {
    return validLabel(lane1Label) && validLabel(lane2Label) &&
           static_cast<uint8_t>(lane1Graphic) <= static_cast<uint8_t>(TasterCarGraphic::RedEtype) &&
           static_cast<uint8_t>(lane2Graphic) <= static_cast<uint8_t>(TasterCarGraphic::RedEtype);
  }
};

// Presentation preferences intentionally use their own namespace.  Existing
// storage allocations are: race setup 0/1/5, track setup 2/3/4, history and
// records 6..209, and MUG catalog 210/211/212.  These records therefore cannot
// be mistaken for, or overwrite, historical customer data.
class TasterPresentationPreferencesMemory {
public:
  static constexpr unsigned FirstSlot = 213;
  static constexpr unsigned SecondSlot = 214;
  static constexpr unsigned StagingSlot = 215;
  static constexpr uint32_t Magic = 0x31505054u; // TPP1
  static constexpr uint8_t FormatVersion = 1;
  static constexpr size_t RecordSize = 68;

  explicit TasterPresentationPreferencesMemory(Store& store) : store_(store) {}

  LoadStatus load(TasterPresentationPreferences& value) {
    value = TasterPresentationPreferences::factoryDefaults();
    if (!store_.available()) return LoadStatus::DefaultsStorageError;
    uint8_t first[RecordSize]{}, second[RecordSize]{};
    TasterPresentationPreferences firstValue{}, secondValue{};
    uint32_t firstGeneration = 0, secondGeneration = 0;
    const bool firstPresent = store_.read(FirstSlot, first, sizeof(first));
    const bool secondPresent = store_.read(SecondSlot, second, sizeof(second));
    const bool firstValid = firstPresent && decode(first, firstValue, firstGeneration);
    const bool secondValid = secondPresent && decode(second, secondValue, secondGeneration);
    if (!firstValid && !secondValid) {
      current_ = value;
      loaded_ = true;
      return firstPresent || secondPresent ? LoadStatus::DefaultsInvalid : LoadStatus::DefaultsMissing;
    }
    const bool useSecond = secondValid && (!firstValid || secondGeneration > firstGeneration);
    value = useSecond ? secondValue : firstValue;
    current_ = value;
    generation_ = useSecond ? secondGeneration : firstGeneration;
    activeSlot_ = useSecond ? SecondSlot : FirstSlot;
    loaded_ = true;
    return LoadStatus::Remembered;
  }

  bool save(const TasterPresentationPreferences& value) {
    if (!loaded_) {
      TasterPresentationPreferences ignored{};
      if (load(ignored) == LoadStatus::DefaultsStorageError) return false;
    }
    if (!store_.available() || !value.valid() || generation_ == UINT32_MAX) return false;
    uint8_t encoded[RecordSize]{};
    encode(value, generation_ + 1, encoded);
    const unsigned target = activeSlot_ == FirstSlot ? SecondSlot : FirstSlot;
    if (!store_.writeUnreferenced(StagingSlot, encoded, sizeof(encoded))) return false;
    uint8_t verified[RecordSize]{};
    if (!store_.read(StagingSlot, verified, sizeof(verified)) ||
        memcmp(encoded, verified, sizeof(encoded)) != 0) {
      (void)store_.reclaim(StagingSlot);
      return false;
    }
    if (!store_.writeMetadata(target, encoded, sizeof(encoded))) {
      (void)store_.reclaim(StagingSlot);
      return false;
    }
    (void)store_.reclaim(StagingSlot);
    current_ = value;
    activeSlot_ = target;
    ++generation_;
    return true;
  }

  bool restoreFactory() { return save(TasterPresentationPreferences::factoryDefaults()); }
  const TasterPresentationPreferences& current() const { return current_; }
  uint32_t generation() const { return generation_; }

  static void encode(const TasterPresentationPreferences& value, uint32_t generation,
                     uint8_t* bytes) {
    memset(bytes, 0, RecordSize);
    put(bytes, Magic);
    bytes[4] = FormatVersion;
    put(bytes + 8, generation);
    memcpy(bytes + 12, value.lane1Label, TasterPresentationPreferences::LabelBytes + 1);
    memcpy(bytes + 37, value.lane2Label, TasterPresentationPreferences::LabelBytes + 1);
    bytes[62] = static_cast<uint8_t>(value.lane1Graphic);
    bytes[63] = static_cast<uint8_t>(value.lane2Graphic);
    put(bytes + 64, checksum(bytes, 64));
  }

private:
  Store& store_;
  TasterPresentationPreferences current_ = TasterPresentationPreferences::factoryDefaults();
  uint32_t generation_ = 0;
  unsigned activeSlot_ = SecondSlot;
  bool loaded_ = false;

  static void put(uint8_t* bytes, uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) bytes[index] = uint8_t(value >> (8 * index));
  }
  static uint32_t get(const uint8_t* bytes) {
    uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) value |= uint32_t(bytes[index]) << (8 * index);
    return value;
  }
  static uint32_t checksum(const uint8_t* bytes, size_t length) {
    uint32_t value = 2166136261u;
    while (length--) value = (value ^ *bytes++) * 16777619u;
    return value;
  }
  static bool decode(const uint8_t* bytes, TasterPresentationPreferences& value,
                     uint32_t& generation) {
    if (get(bytes) != Magic || bytes[4] != FormatVersion ||
        get(bytes + 64) != checksum(bytes, 64)) return false;
    TasterPresentationPreferences candidate{};
    memcpy(candidate.lane1Label, bytes + 12, TasterPresentationPreferences::LabelBytes + 1);
    memcpy(candidate.lane2Label, bytes + 37, TasterPresentationPreferences::LabelBytes + 1);
    candidate.lane1Label[TasterPresentationPreferences::LabelBytes] = '\0';
    candidate.lane2Label[TasterPresentationPreferences::LabelBytes] = '\0';
    candidate.lane1Graphic = static_cast<TasterCarGraphic>(bytes[62]);
    candidate.lane2Graphic = static_cast<TasterCarGraphic>(bytes[63]);
    generation = get(bytes + 8);
    if (generation == 0 || !candidate.valid()) return false;
    value = candidate;
    return true;
  }
};

enum class TasterPresentationPreferencesResult : uint8_t {
  Accepted, PermissionDenied, LifecycleNotReady, StorageUnavailable, InvalidPreferences
};

// This narrow controller service makes policy explicit and keeps Browser HTTP
// code out of the persistence boundary.  The caller supplies the committed
// authority/lifecycle facts; the service never receives a filesystem handle.
class TasterPresentationPreferencesController {
public:
  explicit TasterPresentationPreferencesController(TasterPresentationPreferencesMemory& memory)
      : memory_(memory) {}

  struct Committed {
    TasterPresentationPreferences value{};
    uint64_t revision=0;
    bool loaded=false;
  };
  LoadStatus load() {
    TasterPresentationPreferences ignored{};
    const auto result=memory_.load(ignored);
    if(result!=LoadStatus::DefaultsStorageError) publish();
    return result;
  }
  // Remote Core owner reads one committed value/revision under a short
  // lock. The existing HTTP writer owns storage operations; no lock held
  // over persistence. Startup load precedes HTTP startup.
  Committed committed() const {
    std::lock_guard<std::mutex> lock(publicationLock_);return publication_;
  }
  const TasterPresentationPreferences& current() const { return memory_.current(); }

  TasterPresentationPreferencesResult update(bool controllingBrowser, bool ready,
                                             const TasterPresentationPreferences& value) {
    if (!controllingBrowser) return TasterPresentationPreferencesResult::PermissionDenied;
    if (!ready) return TasterPresentationPreferencesResult::LifecycleNotReady;
    if (!value.valid()) return TasterPresentationPreferencesResult::InvalidPreferences;
    if(!memory_.save(value))return TasterPresentationPreferencesResult::StorageUnavailable;
    publish();return TasterPresentationPreferencesResult::Accepted;
  }

  TasterPresentationPreferencesResult restoreFactory(bool controllingBrowser, bool ready) {
    if (!controllingBrowser) return TasterPresentationPreferencesResult::PermissionDenied;
    if (!ready) return TasterPresentationPreferencesResult::LifecycleNotReady;
    if(!memory_.restoreFactory())return TasterPresentationPreferencesResult::StorageUnavailable;
    publish();return TasterPresentationPreferencesResult::Accepted;
  }

private:
  void publish(){
    Committed next{memory_.current(),memory_.generation(),true};
    std::lock_guard<std::mutex> lock(publicationLock_);publication_=next;
  }
  TasterPresentationPreferencesMemory& memory_;
  mutable std::mutex publicationLock_;
  Committed publication_{};
};

} // namespace pp
