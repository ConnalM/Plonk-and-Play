#pragma once
#include "core.h"

namespace pp {
class HistoryStore {
public:
  static constexpr uint8_t Capacity = 4;
  static constexpr size_t MaxBytes = 8192;
  struct Entry { uint32_t sequence=0; uint16_t bytes=0; uint8_t slot=0; };
  virtual ~HistoryStore() = default;
  virtual bool append(const uint8_t* bytes, size_t length, uint32_t& sequence) = 0;
  virtual bool loadNewest(uint8_t index, uint8_t* bytes, size_t capacity, size_t& length, uint32_t& sequence) = 0;
  virtual uint8_t count() = 0;
  virtual bool clear() = 0;
};
class SlotHistoryStore final : public HistoryStore {
public:
  explicit SlotHistoryStore(Store& store):store_(store){}
  bool append(const uint8_t* bytes,size_t length,uint32_t& sequence) override {
    if(!bytes||length==0||length>MaxBytes||!ensure()) return false;
    const uint8_t slot=meta_.count<Capacity?meta_.count:meta_.entries[0].slot;
    if(!store_.write(DataSlot+slot,bytes,length)) return false;
    if(meta_.count==Capacity){for(uint8_t i=1;i<meta_.count;++i)meta_.entries[i-1]=meta_.entries[i];--meta_.count;}
    sequence=++meta_.nextSequence;
    Entry& entry=meta_.entries[meta_.count++];entry.sequence=sequence;entry.bytes=uint16_t(length);entry.slot=slot;
    return writeMeta();
  }
  bool loadNewest(uint8_t index,uint8_t* bytes,size_t capacity,size_t& length,uint32_t& sequence) override {
    length=0;sequence=0;if(!bytes||!ensure()||index>=meta_.count)return false;
    const Entry&e=meta_.entries[meta_.count-1-index];
    if(!e.bytes||e.bytes>capacity||!store_.read(DataSlot+e.slot,bytes,e.bytes))return false;
    length=e.bytes;sequence=e.sequence;return true;
  }
  uint8_t count() override{return ensure()?meta_.count:0;}
  bool clear() override {if(!ensure())return false;meta_.count=0;return writeMeta();}
private:
  static constexpr unsigned MetaSlot=6,DataSlot=8;
  struct Metadata{uint32_t magic=0x50504831u,nextSequence=0;uint8_t count=0;uint8_t reserved[3]{};Entry entries[Capacity]{};uint32_t checksum=0;};
  Store& store_;Metadata meta_{};bool loaded_=false;
  static uint32_t checksum(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
  bool ensure(){if(loaded_)return true;loaded_=true;Metadata candidate{};if(!store_.available()||!store_.read(MetaSlot,reinterpret_cast<uint8_t*>(&candidate),sizeof(candidate)))return store_.available();const uint32_t sum=checksum(reinterpret_cast<const uint8_t*>(&candidate),sizeof(candidate)-sizeof(candidate.checksum));if(candidate.magic==meta_.magic&&candidate.count<=Capacity&&candidate.checksum==sum)meta_=candidate;return true;}
  bool writeMeta(){meta_.checksum=checksum(reinterpret_cast<const uint8_t*>(&meta_),sizeof(meta_)-sizeof(meta_.checksum));return store_.write(MetaSlot,reinterpret_cast<const uint8_t*>(&meta_),sizeof(meta_));}
};
}
