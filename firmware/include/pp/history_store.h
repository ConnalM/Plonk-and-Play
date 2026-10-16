#pragma once
#include "core.h"
#include <stdlib.h>

namespace pp {

class HistoryStore {
public:
  // V1 product retention.  Payload size is deliberately not a sporting or
  // product limit; the backend accepts the complete encoded race that the
  // available storage can commit.
  static constexpr uint8_t Capacity = 100;
  struct Entry { uint32_t sequence=0; uint32_t bytes=0; uint8_t slot=0; };
  virtual ~HistoryStore() = default;
  virtual bool append(const uint8_t* bytes, size_t length, uint32_t& sequence) = 0;
  virtual bool appendDetailed(const void*, size_t, const DetailSource&, uint32_t& sequence) { sequence=0; return false; }
  virtual bool loadNewest(uint8_t index, uint8_t* bytes, size_t capacity, size_t& length, uint32_t& sequence) = 0;
  virtual uint8_t count() = 0;
  virtual bool loadSequence(uint32_t wanted, uint8_t* bytes, size_t capacity, size_t& length, uint32_t& sequence) {
    if (!wanted) return false;
    const uint8_t total = count();
    for (uint8_t i = 0; i < total; ++i) {
      size_t candidateLength = 0; uint32_t candidateSequence = 0;
      if (loadNewest(i, bytes, capacity, candidateLength, candidateSequence) && candidateSequence == wanted) {
        length = candidateLength; sequence = candidateSequence; return true;
      }
    }
    length = 0; sequence = 0; return false;
  }
  virtual bool clear() = 0;
};

// Transactional per-race HistoryStore.  Metadata is bounded while each race
// remains an independently committed payload, so History enumeration does not
// require loading all retained detail into RAM.
class SlotHistoryStore final : public HistoryStore {
public:
  explicit SlotHistoryStore(Store& store):store_(store){}

  bool append(const uint8_t* bytes, size_t length, uint32_t& sequence) override {
    sequence = 0;
    if (!bytes || length == 0 || length > UINT32_MAX || !ensure()) return false;
    // Never overwrite a slot referenced by committed metadata. The spare half
    // of the bounded slot set is the staging area for a new transaction.
    const uint8_t slot = freeSlot();
    if (slot == InvalidSlot) return false;
    uint8_t* verify = static_cast<uint8_t*>(malloc(length));
    if (!verify) return false;
    const bool payloadWritten = store_.write(DataSlot + slot, bytes, length);
    const bool payloadVerified = payloadWritten && store_.read(DataSlot + slot, verify, length) &&
                                 memcmp(bytes, verify, length) == 0;
    free(verify);
    if (!payloadVerified) return false;

    MetadataState candidate = meta_;
    if (candidate.count == Capacity) {
      for (uint8_t i = 1; i < candidate.count; ++i) candidate.entries[i - 1] = candidate.entries[i];
      --candidate.count;
    }
    sequence = ++candidate.nextSequence;
    Entry& entry = candidate.entries[candidate.count++];
    entry.sequence = sequence; entry.bytes = static_cast<uint32_t>(length); entry.slot = slot;
    candidate.generation = meta_.generation + 1;
    if (!publishMetadata(candidate)) { sequence = 0; return false; }
    return true;
  }

  bool loadNewest(uint8_t index, uint8_t* bytes, size_t capacity, size_t& length, uint32_t& sequence) override {
    length = 0; sequence = 0;
    if (!bytes || !ensure() || index >= meta_.count) return false;
    const Entry& entry = meta_.entries[meta_.count - 1 - index];
    if (!entry.bytes || entry.bytes > capacity || entry.slot >= PhysicalSlots) return false;
    if (!store_.read(DataSlot + entry.slot, bytes, entry.bytes)) return false;
    length = entry.bytes; sequence = entry.sequence; return true;
  }

  uint8_t count() override { return ensure() ? meta_.count : 0; }

  bool clear() override {
    if (!ensure()) return false;
    MetadataState empty{}; empty.generation = meta_.generation + 1; empty.nextSequence = meta_.nextSequence;
    return publishMetadata(empty);
  }

private:
  static constexpr unsigned MetaSlotA = 6;
  // Keep the legacy metadata slot and development payload slots readable while
  // moving the second metadata copy beyond the 200 production data slots.
  static constexpr unsigned MetaSlotB = 208;
  static constexpr unsigned DataSlot = 8;
  static constexpr uint8_t PhysicalSlots = static_cast<uint8_t>(Capacity * 2);
  static constexpr uint8_t InvalidSlot = 0xff;
  static constexpr uint32_t Magic = 0x54524831u; // TRH1
  static constexpr uint16_t Version = 1;
  // header (20) + 100 entries (sequence u32, bytes u32, slot u8) + checksum.
  static constexpr size_t EncodedMetadataBytes = 924;
  static constexpr size_t LegacyMetadataBytes = 48;

  struct MetadataState { uint32_t generation=0, nextSequence=0; uint8_t count=0; Entry entries[Capacity]{}; };
  Store& store_; MetadataState meta_{}; unsigned activeMetaSlot_ = MetaSlotA; bool loaded_ = false;

  static uint32_t checksum(const uint8_t* p, size_t n) { uint32_t h=2166136261u; while(n--) h=(h^*p++)*16777619u; return h; }
  static uint32_t get32(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
  static uint16_t get16(const uint8_t* p) { return uint16_t(p[0])|(uint16_t(p[1])<<8); }
  static void put32(uint8_t* p,uint32_t v) { for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i)); }
  static void put16(uint8_t* p,uint16_t v) { p[0]=uint8_t(v);p[1]=uint8_t(v>>8); }
  static bool valid(const MetadataState& value) {
    if(value.count>Capacity)return false;
    for(uint8_t i=0;i<value.count;++i)if(!value.entries[i].sequence||!value.entries[i].bytes||value.entries[i].slot>=PhysicalSlots)return false;
    return true;
  }
  static void encode(const MetadataState& value,uint8_t* out) {
    memset(out,0,EncodedMetadataBytes);put32(out,Magic);put16(out+4,Version);put32(out+8,value.generation);put32(out+12,value.nextSequence);out[16]=value.count;
    size_t p=20;for(uint8_t i=0;i<Capacity;++i){put32(out+p,value.entries[i].sequence);p+=4;put32(out+p,value.entries[i].bytes);p+=4;out[p++]=value.entries[i].slot;}put32(out+p,checksum(out,p));
  }
  static bool decode(const uint8_t* bytes,MetadataState& value) {
    if(!bytes||get32(bytes)!=Magic||get16(bytes+4)!=Version||get32(bytes+EncodedMetadataBytes-4)!=checksum(bytes,EncodedMetadataBytes-4))return false;
    value=MetadataState{};value.generation=get32(bytes+8);value.nextSequence=get32(bytes+12);value.count=bytes[16];size_t p=20;
    for(uint8_t i=0;i<Capacity;++i){value.entries[i].sequence=get32(bytes+p);p+=4;value.entries[i].bytes=get32(bytes+p);p+=4;value.entries[i].slot=bytes[p++];}
    return value.generation!=0&&valid(value);
  }
  static bool decodeLegacy(const uint8_t* bytes,MetadataState& value) {
    if(!bytes||get32(bytes)!=0x50504831u||get32(bytes+44)!=checksum(bytes,44)||bytes[8]>Capacity)return false;
    value=MetadataState{};value.generation=1;value.nextSequence=get32(bytes+4);value.count=bytes[8];size_t p=12;
    for(uint8_t i=0;i<Capacity;++i){value.entries[i].sequence=get32(bytes+p);p+=8;value.entries[i].bytes=get16(bytes+p-4);value.entries[i].slot=bytes[p-2];}
    return valid(value);
  }
  bool ensure() {
    if (loaded_) return store_.available();
    loaded_ = true;
    if (!store_.available()) return false;
    uint8_t a[EncodedMetadataBytes]{},b[EncodedMetadataBytes]{};MetadataState va{},vb{};const bool hasA=store_.read(MetaSlotA,a,sizeof(a))&&decode(a,va);const bool hasB=store_.read(MetaSlotB,b,sizeof(b))&&decode(b,vb);
    if(hasA||hasB){if(hasB&&(!hasA||vb.generation>va.generation)){meta_=vb;activeMetaSlot_=MetaSlotB;}else{meta_=va;activeMetaSlot_=MetaSlotA;}return true;}
    uint8_t legacy[LegacyMetadataBytes]{};MetadataState old{};if(store_.read(MetaSlotA,legacy,sizeof(legacy))&&decodeLegacy(legacy,old)){meta_=old;activeMetaSlot_=MetaSlotA;}return true;
  }
  uint8_t freeSlot()const {for(uint8_t slot=0;slot<PhysicalSlots;++slot){bool used=false;for(uint8_t i=0;i<meta_.count;++i)if(meta_.entries[i].slot==slot){used=true;break;}if(!used)return slot;}return InvalidSlot;}
  bool publishMetadata(const MetadataState& candidate) {
    if (!valid(candidate)) return false;
    uint8_t bytes[EncodedMetadataBytes]{};
    encode(candidate,bytes);
    const unsigned target=activeMetaSlot_==MetaSlotA?MetaSlotB:MetaSlotA;
    if (!store_.write(target,bytes,sizeof(bytes))) return false;
    uint8_t verify[EncodedMetadataBytes]{};MetadataState checked{};
    if(!store_.read(target,verify,sizeof(verify))||!decode(verify,checked)||checked.generation!=candidate.generation)return false;
    meta_=candidate;activeMetaSlot_=target;return true;
  }
};

}
