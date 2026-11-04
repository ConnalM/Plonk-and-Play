#pragma once

#include "core.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

namespace pp {

// Saved MUG identity is a persistent SD-backed product facility.  These slots
// are deliberately outside every existing configuration, History and Record
// slot used by the production Store implementations:
//   Memory                 0, 1, 5 (staging)
//   Track Configuration    2, 3, 4 (staging)
//   History                6, 8..207, 208
//   Track Records          7, 209
// The catalog therefore has its own logical namespace without introducing a
// second storage technology or giving the Browser direct filesystem access.
class MugCatalog final {
public:
  static constexpr uint32_t FirstSavedId = 3;
  static constexpr uint16_t MaxProfiles = 100;
  static constexpr uint8_t MaxNameCharacters = 40;
  // UTF-8 code points may occupy at most four bytes.  The fixed record keeps
  // the storage transaction bounded while accepting every valid 1..40-code
  // point name.
  static constexpr uint16_t MaxNameBytes = MaxNameCharacters * 4;

  static constexpr unsigned CatalogSlotA = 210;
  static constexpr unsigned CatalogSlotB = 211;
  static constexpr unsigned StagingSlot = 212;

  static constexpr uint32_t Magic = 0x3147554Du; // MUG1
  static constexpr uint16_t FormatVersion = 1;
  static constexpr size_t HeaderBytes = 20;
  static constexpr size_t EntryBytes = 4 + 1 + 1 + 2 + MaxNameBytes;
  static constexpr size_t RecordSize = HeaderBytes + MaxProfiles * EntryBytes + 4;

  struct Entry {
    uint32_t id = 0;
    bool archived = false;
    uint16_t nameBytes = 0;
    char name[MaxNameBytes + 1]{};
  };

  enum class Result : uint8_t {
    Accepted,
    NotLoaded,
    StorageUnavailable,
    InvalidName,
    CapacityFull,
    UnknownId,
    Archived,
    AlreadyArchived,
    IdExhausted
  };

  enum class LoadResult : uint8_t {
    Empty,
    Remembered,
    Corrupt,
    StorageUnavailable,
    MemoryUnavailable
  };

  explicit MugCatalog(Store& store) : store_(store) {}

  // Reconstruct only the newest checksum-valid A/B record.  An absent pair is
  // a normal first-use empty catalog; a malformed present pair is not silently
  // treated as an accepted catalog.
  LoadResult load() {
    if (loaded_) {
      return store_.available() ? (count_ ? LoadResult::Remembered : LoadResult::Empty)
                                : LoadResult::StorageUnavailable;
    }
    if (!store_.available()) return LoadResult::StorageUnavailable;

    uint8_t* first = static_cast<uint8_t*>(malloc(RecordSize));
    uint8_t* second = static_cast<uint8_t*>(malloc(RecordSize));
    if (!first || !second) {
      free(first); free(second);
      return LoadResult::MemoryUnavailable;
    }
    Entry* firstEntries = static_cast<Entry*>(calloc(MaxProfiles, sizeof(Entry)));
    Entry* secondEntries = static_cast<Entry*>(calloc(MaxProfiles, sizeof(Entry)));
    if (!firstEntries || !secondEntries) {
      free(first); free(second); free(firstEntries); free(secondEntries);
      return LoadResult::MemoryUnavailable;
    }
    uint16_t firstCount = 0, secondCount = 0;
    uint32_t firstGeneration = 0, secondGeneration = 0;
    uint32_t firstNextId = FirstSavedId, secondNextId = FirstSavedId;
    const Store::ReadResult firstRead = store_.readResult(CatalogSlotA, first, RecordSize);
    const Store::ReadResult secondRead = store_.readResult(CatalogSlotB, second, RecordSize);
    const bool firstReadable = firstRead == Store::ReadResult::Readable;
    const bool secondReadable = secondRead == Store::ReadResult::Readable;
    const bool firstValid = firstReadable && decode(first, firstEntries, firstCount,
                                                    firstGeneration, firstNextId);
    const bool secondValid = secondReadable && decode(second, secondEntries, secondCount,
                                                      secondGeneration, secondNextId);
    const bool anyRecord = firstReadable || secondReadable;
    const bool readFailure = firstRead == Store::ReadResult::Failed ||
                             secondRead == Store::ReadResult::Failed;
    if (!firstValid && !secondValid) {
      if (readFailure) {
        // Do not commission an empty in-memory catalog after an existing
        // namespace could not be read. The caller must retain the previous
        // authority and wait for storage to recover.
        free(first); free(second); free(firstEntries); free(secondEntries);
        return LoadResult::StorageUnavailable;
      }
      if (anyRecord) {
        free(first); free(second); free(firstEntries); free(secondEntries);
        return LoadResult::Corrupt;
      }
      loaded_ = true;
      clearState();
      free(first); free(second); free(firstEntries); free(secondEntries);
      return LoadResult::Empty;
    }

    const bool useSecond = secondValid && (!firstValid || secondGeneration > firstGeneration);
    const Entry* source = useSecond ? secondEntries : firstEntries;
    count_ = useSecond ? secondCount : firstCount;
    generation_ = useSecond ? secondGeneration : firstGeneration;
    nextId_ = useSecond ? secondNextId : firstNextId;
    activeSlot_ = useSecond ? CatalogSlotB : CatalogSlotA;
    for (uint16_t i = 0; i < count_; ++i) entries_[i] = source[i];
    loaded_ = true;
    free(first); free(second); free(firstEntries); free(secondEntries);
    return LoadResult::Remembered;
  }

  Result create(const char* name, uint32_t& id) {
    id = 0;
    if (!loaded_) return Result::NotLoaded;
    if (!store_.available()) return Result::StorageUnavailable;
    uint16_t nameBytes = 0; uint8_t nameCharacters = 0;
    if (!validName(name, nameBytes, nameCharacters)) return Result::InvalidName;
    (void)nameCharacters;
    if (count_ >= MaxProfiles) return Result::CapacityFull;
    if (nextId_ < FirstSavedId || nextId_ == UINT32_MAX) return Result::IdExhausted;
    State* candidate = allocateCandidate();
    if (!candidate) return Result::StorageUnavailable;
    Entry& entry = candidate->entries[candidate->count++];
    entry.id = candidate->nextId;
    entry.archived = false;
    entry.nameBytes = nameBytes;
    copyName(entry, name, nameBytes);
    ++candidate->nextId;
    const bool published = publish(*candidate);
    const uint32_t createdId = entry.id;
    free(candidate);
    if (!published) return Result::StorageUnavailable;
    id = createdId;
    return Result::Accepted;
  }

  Result rename(uint32_t id, const char* name) {
    if (!loaded_) return Result::NotLoaded;
    if (!store_.available()) return Result::StorageUnavailable;
    uint16_t nameBytes = 0; uint8_t nameCharacters = 0;
    if (!validName(name, nameBytes, nameCharacters)) return Result::InvalidName;
    (void)nameCharacters;
    const int index = findIndex(id);
    if (index < 0) return Result::UnknownId;
    if (entries_[index].archived) return Result::Archived;
    State* candidate = allocateCandidate();
    if (!candidate) return Result::StorageUnavailable;
    candidate->entries[index].nameBytes = nameBytes;
    copyName(candidate->entries[index], name, nameBytes);
    const bool published = publish(*candidate);
    free(candidate);
    return published ? Result::Accepted : Result::StorageUnavailable;
  }

  Result archive(uint32_t id) {
    if (!loaded_) return Result::NotLoaded;
    if (!store_.available()) return Result::StorageUnavailable;
    const int index = findIndex(id);
    if (index < 0) return Result::UnknownId;
    if (entries_[index].archived) return Result::AlreadyArchived;
    State* candidate = allocateCandidate();
    if (!candidate) return Result::StorageUnavailable;
    candidate->entries[index].archived = true;
    const bool published = publish(*candidate);
    free(candidate);
    return published ? Result::Accepted : Result::StorageUnavailable;
  }

  bool find(uint32_t id, Entry& out, bool includeArchived = true) const {
    const int index = findIndex(id);
    if (index < 0 || (!includeArchived && entries_[index].archived)) return false;
    out = entries_[index];
    return true;
  }

  const Entry* entry(uint16_t index) const { return index < count_ ? &entries_[index] : nullptr; }
  uint16_t count() const { return count_; }
  uint16_t activeCount() const {
    uint16_t result = 0;
    for (uint16_t i = 0; i < count_; ++i) if (!entries_[i].archived) ++result;
    return result;
  }
  uint32_t generation() const { return generation_; }
  uint32_t nextId() const { return nextId_; }
  unsigned activeSlot() const { return activeSlot_; }

  static const char* resultName(Result value) {
    switch (value) {
      case Result::Accepted: return "ACCEPTED";
      case Result::NotLoaded: return "NOT_LOADED";
      case Result::StorageUnavailable: return "STORAGE_UNAVAILABLE";
      case Result::InvalidName: return "INVALID_NAME";
      case Result::CapacityFull: return "CAPACITY_FULL";
      case Result::UnknownId: return "UNKNOWN_ID";
      case Result::Archived: return "ARCHIVED";
      case Result::AlreadyArchived: return "ALREADY_ARCHIVED";
      case Result::IdExhausted: return "ID_EXHAUSTED";
    }
    return "UNKNOWN";
  }

private:
  struct State {
    uint16_t count = 0;
    uint32_t nextId = FirstSavedId;
    Entry entries[MaxProfiles]{};
  };

  Store& store_;
  Entry entries_[MaxProfiles]{};
  uint16_t count_ = 0;
  uint32_t generation_ = 0;
  uint32_t nextId_ = FirstSavedId;
  unsigned activeSlot_ = CatalogSlotB;
  bool loaded_ = false;

  static uint32_t get32(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
  }
  static uint16_t get16(const uint8_t* bytes) {
    return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
  }
  static void put32(uint8_t* bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (8 * i));
  }
  static void put16(uint8_t* bytes, uint16_t value) {
    bytes[0] = uint8_t(value); bytes[1] = uint8_t(value >> 8);
  }
  static uint32_t checksum(const uint8_t* bytes, size_t length) {
    uint32_t value = 2166136261u;
    while (length--) value = (value ^ *bytes++) * 16777619u;
    return value;
  }
  static void clearEntry(Entry& entry) { entry = Entry{}; }
  void clearState() {
    for (uint16_t i = 0; i < MaxProfiles; ++i) clearEntry(entries_[i]);
    count_ = 0; generation_ = 0; nextId_ = FirstSavedId; activeSlot_ = CatalogSlotB;
  }
  State* allocateCandidate() const {
    State* value = static_cast<State*>(calloc(1, sizeof(State)));
    if (!value) return nullptr;
    value->count = count_; value->nextId = nextId_;
    for (uint16_t i = 0; i < count_; ++i) value->entries[i] = entries_[i];
    return value;
  }
  int findIndex(uint32_t id) const {
    if (id < FirstSavedId) return -1;
    for (uint16_t i = 0; i < count_; ++i) if (entries_[i].id == id) return int(i);
    return -1;
  }
  static void copyName(Entry& entry, const char* name, uint16_t bytes) {
    memset(entry.name, 0, sizeof(entry.name));
    memcpy(entry.name, name, bytes);
    entry.name[bytes] = '\0';
  }
  static bool continuation(uint8_t value) { return (value & 0xc0u) == 0x80u; }

  // Strict UTF-8 validation: rejects overlong encodings, surrogate code
  // points and code points above U+10FFFF while counting Unicode characters.
  static bool validName(const char* name, uint16_t& bytes, uint8_t& characters) {
    bytes = 0; characters = 0;
    if (!name) return false;
    const size_t length = strlen(name);
    if (!length || length > MaxNameBytes) return false;
    const uint8_t* value = reinterpret_cast<const uint8_t*>(name);
    size_t index = 0;
    while (index < length) {
      const uint8_t first = value[index++];
      uint32_t codePoint = 0;
      size_t continuationCount = 0;
      if (first <= 0x7f) {
        codePoint = first;
      } else if (first >= 0xc2 && first <= 0xdf) {
        codePoint = first & 0x1fu; continuationCount = 1;
      } else if (first >= 0xe0 && first <= 0xef) {
        codePoint = first & 0x0fu; continuationCount = 2;
      } else if (first >= 0xf0 && first <= 0xf4) {
        codePoint = first & 0x07u; continuationCount = 3;
      } else {
        return false;
      }
      if (index + continuationCount > length) return false;
      for (size_t i = 0; i < continuationCount; ++i) {
        if (!continuation(value[index])) return false;
        codePoint = (codePoint << 6) | (value[index++] & 0x3fu);
      }
      if ((continuationCount == 2 && codePoint < 0x800u) ||
          (continuationCount == 3 && codePoint < 0x10000u) ||
          codePoint > 0x10ffffu || (codePoint >= 0xd800u && codePoint <= 0xdfffu)) return false;
      // Enforce the boundary-specific first continuation restrictions.
      const uint8_t lead = value[index - continuationCount - 1];
      if ((lead == 0xe0 && value[index - 2] < 0xa0) ||
          (lead == 0xed && value[index - 2] > 0x9f) ||
          (lead == 0xf0 && value[index - 3] < 0x90) ||
          (lead == 0xf4 && value[index - 3] > 0x8f)) return false;
      ++characters;
      if (characters > MaxNameCharacters) return false;
    }
    bytes = static_cast<uint16_t>(length);
    return characters > 0;
  }

  static bool decode(const uint8_t* bytes, Entry* entries, uint16_t& count,
                     uint32_t& generation, uint32_t& nextId) {
    if (!bytes || get32(bytes) != Magic || get16(bytes + 4) != FormatVersion ||
        get32(bytes + RecordSize - 4) != checksum(bytes, RecordSize - 4)) return false;
    count = get16(bytes + 8);
    if (get16(bytes + 10) != 0) return false;
    generation = get32(bytes + 12); nextId = get32(bytes + 16);
    if (count > MaxProfiles || !generation || nextId < FirstSavedId) return false;
    size_t offset = HeaderBytes;
    uint32_t previousId = 0;
    for (uint16_t i = 0; i < MaxProfiles; ++i) {
      Entry value{};
      value.id = get32(bytes + offset); offset += 4;
      value.archived = bytes[offset++] != 0;
      if (bytes[offset++] != 0) return false;
      value.nameBytes = get16(bytes + offset); offset += 2;
      memcpy(value.name, bytes + offset, MaxNameBytes); offset += MaxNameBytes;
      if (i >= count) {
        if (value.id || value.archived || value.nameBytes || value.name[0]) return false;
        continue;
      }
      if (value.id < FirstSavedId || (i && value.id <= previousId) || value.nameBytes == 0 ||
          value.nameBytes > MaxNameBytes || value.name[value.nameBytes] != '\0') return false;
      uint8_t characters = 0; uint16_t checkedBytes = 0;
      if (!validName(value.name, checkedBytes, characters) || checkedBytes != value.nameBytes) return false;
      entries[i] = value; previousId = value.id;
      if (value.id >= nextId && value.id != UINT32_MAX) return false;
    }
    return true;
  }
  static void encode(const State& state, uint32_t generation, uint8_t* bytes) {
    memset(bytes, 0, RecordSize);
    put32(bytes, Magic); put16(bytes + 4, FormatVersion); put16(bytes + 6, 0);
    put16(bytes + 8, state.count); put16(bytes + 10, 0);
    put32(bytes + 12, generation); put32(bytes + 16, state.nextId);
    size_t offset = HeaderBytes;
    for (uint16_t i = 0; i < MaxProfiles; ++i) {
      const Entry& value = state.entries[i];
      put32(bytes + offset, value.id); offset += 4;
      bytes[offset++] = value.archived ? 1 : 0;
      bytes[offset++] = 0;
      put16(bytes + offset, value.nameBytes); offset += 2;
      memcpy(bytes + offset, value.name, MaxNameBytes); offset += MaxNameBytes;
    }
    put32(bytes + RecordSize - 4, checksum(bytes, RecordSize - 4));
  }
  bool publish(const State& candidate) {
    if (generation_ == UINT32_MAX) return false;
    uint8_t* encoded = static_cast<uint8_t*>(malloc(RecordSize));
    uint8_t* verify = static_cast<uint8_t*>(malloc(RecordSize));
    if (!encoded || !verify) { free(encoded); free(verify); return false; }
    const uint32_t nextGeneration = generation_ + 1;
    encode(candidate, nextGeneration, encoded);
    const unsigned target = activeSlot_ == CatalogSlotA ? CatalogSlotB : CatalogSlotA;
    bool ok = store_.writeUnreferenced(StagingSlot, encoded, RecordSize);
    if (ok) ok = store_.read(StagingSlot, verify, RecordSize) &&
                 memcmp(encoded, verify, RecordSize) == 0;
    if (ok) ok = store_.writeMetadata(target, encoded, RecordSize);
    if (!ok) (void)store_.reclaim(StagingSlot);
    else {
      (void)store_.reclaim(StagingSlot);
      count_ = candidate.count; nextId_ = candidate.nextId; generation_ = nextGeneration;
      activeSlot_ = target;
      for (uint16_t i = 0; i < count_; ++i) entries_[i] = candidate.entries[i];
      for (uint16_t i = count_; i < MaxProfiles; ++i) clearEntry(entries_[i]);
    }
    free(encoded); free(verify);
    return ok;
  }
};

} // namespace pp
