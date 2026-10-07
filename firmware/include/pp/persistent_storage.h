#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "race_engine.h"

namespace pp {

// Stable wire/storage schema. This is deliberately independent of C++ object
// layout and remains separate from CompletedRaceResult::formatVersion.
class VersionedResultCodec {
public:
  static constexpr uint32_t Magic = 0x50534631u; // PSF1
  static constexpr uint16_t Version = 1;
  static constexpr size_t MaxBytes = HistoryStore::MaxBytes;

  static bool encode(const RaceEngineModule::CompletedRaceResult& value,
                     uint8_t* out, size_t capacity, size_t& length) {
    Writer w(out, capacity);
    w.u32(Magic); w.u16(Version); w.u16(0); size_t lengthOffset=w.position(); w.u32(0);
    w.u16(value.formatVersion);
    uint8_t flags=0;
    if(value.sealed)flags|=1u; if(value.valid)flags|=2u; if(value.deadHeat)flags|=4u;
    if(value.fastestLapTied)flags|=8u; if(value.overtime)flags|=16u;
    w.u8(flags); w.u8(static_cast<uint8_t>(value.mode)); w.u8(static_cast<uint8_t>(value.behaviour)); w.u8(value.entryCount);
    w.u16(value.durationMinutes); w.u16(0); w.u32(value.lapTarget);
    w.u64(value.winningTime); w.u64(value.finishTime); w.u64(value.expiryTime); w.u64(value.durationUs);
    w.u64(value.fastestLap); w.u32(value.fastestEntryId); w.u32(0);
    if(value.entryCount>RaceEngineModule::MaxEntries) return false;
    for(uint8_t i=0;i<value.entryCount;++i) {
      const auto&e=value.entries[i];
      w.u32(e.raceEntryId); w.u32(e.laps); w.u32(e.classifiedLaps); w.u32(e.rank);
      w.u32(e.lapsBehind); w.u32(e.lapPenalty); w.u8(e.lane);
      uint8_t entryFlags=(e.tied?1u:0u)|(e.completed?2u:0u); w.u8(entryFlags); w.u8(e.recordCount); w.u8(0);
      w.u64(e.completionTime); w.u64(e.bestLap);
      if(e.recordCount>RaceEngineModule::MaxLaps) return false;
      for(uint8_t j=0;j<e.recordCount;++j) {
        const auto&lap=e.records[j];
        w.u32(lap.lapNumber); w.u64(lap.startTime); w.u64(lap.finishTime); w.u64(lap.lapTime); w.u8(lap.valid?1u:0u); w.u8(0); w.u16(0);
      }
    }
    if(!w.ok()) return false;
    const size_t totalWithoutChecksum=w.position()+4;
    if(totalWithoutChecksum>capacity) return false;
    w.u32(0); if(!w.ok()) return false;
    const uint32_t total=static_cast<uint32_t>(totalWithoutChecksum);
    out[lengthOffset]=uint8_t(total); out[lengthOffset+1]=uint8_t(total>>8); out[lengthOffset+2]=uint8_t(total>>16); out[lengthOffset+3]=uint8_t(total>>24);
    const uint32_t checksum=sum(out,totalWithoutChecksum-4);
    out[totalWithoutChecksum-4]=uint8_t(checksum); out[totalWithoutChecksum-3]=uint8_t(checksum>>8); out[totalWithoutChecksum-2]=uint8_t(checksum>>16); out[totalWithoutChecksum-1]=uint8_t(checksum>>24);
    length=totalWithoutChecksum; return true;
  }

  static bool decode(const uint8_t* bytes, size_t length, RaceEngineModule::CompletedRaceResult& out) {
    if(!bytes||length<32)return false;
    const uint32_t expected=sum(bytes,length-4);
    const uint32_t supplied=read32(bytes+length-4);
    if(expected!=supplied)return false;
    Reader r(bytes,length-4); uint32_t magic=0,total=0;uint16_t version=0,reserved=0;
    if(!r.u32(magic)||!r.u16(version)||!r.u16(reserved)||!r.u32(total)||magic!=Magic||version!=Version||total!=length)return false;
    uint16_t resultVersion=0;uint8_t flags=0,mode=0,behaviour=0,count=0;uint16_t duration=0,unused=0;uint32_t lapTarget=0;
    if(!r.u16(resultVersion)||!r.u8(flags)||!r.u8(mode)||!r.u8(behaviour)||!r.u8(count)||!r.u16(duration)||!r.u16(unused)||!r.u32(lapTarget)||count>RaceEngineModule::MaxEntries)return false;
    out={};out.formatVersion=resultVersion;out.sealed=flags&1u;out.valid=flags&2u;out.deadHeat=flags&4u;out.fastestLapTied=flags&8u;out.overtime=flags&16u;
    if(mode<static_cast<uint8_t>(SessionMode::LapRace)||mode>static_cast<uint8_t>(SessionMode::Endurance)||behaviour>static_cast<uint8_t>(LapFinishBehaviour::CompleteCurrentLap))return false;
    out.mode=static_cast<SessionMode>(mode);out.behaviour=static_cast<LapFinishBehaviour>(behaviour);out.entryCount=count;out.durationMinutes=duration;out.lapTarget=lapTarget;
    uint32_t unused32=0;if(!r.u64(out.winningTime)||!r.u64(out.finishTime)||!r.u64(out.expiryTime)||!r.u64(out.durationUs)||!r.u64(out.fastestLap)||!r.u32(out.fastestEntryId)||!r.u32(unused32))return false;
    for(uint8_t i=0;i<count;++i){auto&e=out.entries[i];uint8_t entryFlags=0,recordCount=0,lane=0,entryUnused=0;if(!r.u32(e.raceEntryId)||!r.u32(e.laps)||!r.u32(e.classifiedLaps)||!r.u32(e.rank)||!r.u32(e.lapsBehind)||!r.u32(e.lapPenalty)||!r.u8(lane)||!r.u8(entryFlags)||!r.u8(recordCount)||!r.u8(entryUnused)||!r.u64(e.completionTime)||!r.u64(e.bestLap)||recordCount>RaceEngineModule::MaxLaps)return false;e.lane=lane;e.tied=entryFlags&1u;e.completed=entryFlags&2u;e.recordCount=recordCount;for(uint8_t j=0;j<recordCount;++j){auto&lap=e.records[j];uint8_t valid=0;uint8_t pad8=0;uint16_t pad16=0;if(!r.u32(lap.lapNumber)||!r.u64(lap.startTime)||!r.u64(lap.finishTime)||!r.u64(lap.lapTime)||!r.u8(valid)||!r.u8(pad8)||!r.u16(pad16))return false;lap.valid=valid!=0;}}
    return r.position()==length-4;
  }

private:
  struct Writer { uint8_t*data;size_t cap,pos=0;bool good=true;Writer(uint8_t*d,size_t c):data(d),cap(c){}void put(const uint8_t*p,size_t n){if(!good||pos+n>cap){good=false;return;}memcpy(data+pos,p,n);pos+=n;}void u8(uint8_t v){put(&v,1);}void u16(uint16_t v){uint8_t p[2]={uint8_t(v),uint8_t(v>>8)};put(p,2);}void u32(uint32_t v){uint8_t p[4]={uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};put(p,4);}void u64(uint64_t v){uint8_t p[8];for(unsigned i=0;i<8;++i)p[i]=uint8_t(v>>(i*8));put(p,8);}bool ok()const{return good;}size_t position()const{return pos;}};
  struct Reader {const uint8_t*data;size_t cap,pos=0;bool good=true;Reader(const uint8_t*d,size_t c):data(d),cap(c){}bool take(uint8_t*p,size_t n){if(!good||pos+n>cap){good=false;return false;}memcpy(p,data+pos,n);pos+=n;return true;}bool u8(uint8_t&v){return take(&v,1);}bool u16(uint16_t&v){uint8_t p[2];if(!take(p,2))return false;v=uint16_t(p[0])|(uint16_t(p[1])<<8);return true;}bool u32(uint32_t&v){uint8_t p[4];if(!take(p,4))return false;v=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);return true;}bool u64(uint64_t&v){uint8_t p[8];if(!take(p,8))return false;v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(p[i])<<(i*8);return true;}size_t position()const{return pos;}};
  static uint32_t read32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
  static uint32_t sum(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
};

// Decorates any bounded HistoryStore with the stable serialized result format.
class VersionedHistoryStore final : public HistoryStore {
public:
  explicit VersionedHistoryStore(HistoryStore& backend):backend_(backend){}
  bool append(const uint8_t*bytes,size_t length,uint32_t&sequence) override {
    if(!bytes||length!=sizeof(RaceEngineModule::CompletedRaceResult))return false;
    auto* encoded=static_cast<uint8_t*>(malloc(MaxBytes));if(!encoded)return false;size_t encodedLength=0;
    const bool encodedOk=VersionedResultCodec::encode(*reinterpret_cast<const RaceEngineModule::CompletedRaceResult*>(bytes),encoded,MaxBytes,encodedLength);
    const bool ok=encodedOk&&backend_.append(encoded,encodedLength,sequence);free(encoded);return ok;
  }
  bool loadNewest(uint8_t index,uint8_t*bytes,size_t capacity,size_t&length,uint32_t&sequence) override {
    length=0;sequence=0;if(!bytes||capacity<sizeof(RaceEngineModule::CompletedRaceResult))return false;auto* encoded=static_cast<uint8_t*>(malloc(MaxBytes));if(!encoded)return false;size_t encodedLength=0;uint32_t storedSequence=0;const bool loaded=backend_.loadNewest(index,encoded,MaxBytes,encodedLength,storedSequence);RaceEngineModule::CompletedRaceResult decoded{};const bool decodedOk=loaded&&VersionedResultCodec::decode(encoded,encodedLength,decoded);if(decodedOk){memcpy(bytes,&decoded,sizeof(decoded));length=sizeof(decoded);sequence=storedSequence;}free(encoded);return decodedOk;
  }
  uint8_t count() override{return backend_.count();}
  bool clear() override{return backend_.clear();}
private: HistoryStore&backend_;
};

// Versioned Track Record persistence for the development backend. It retains
// the existing lane-based product API without making PP_MAX_ENTRIES a MUG
// database limit.
class VersionedTrackRecordStore final : public TrackRecordStore {
public:
  static constexpr uint32_t Magic=0x50535231u,Version=1;static constexpr unsigned Slot=7;
  explicit VersionedTrackRecordStore(Store&store):store_(store){}
  bool observe(uint8_t lane,Time lap) override {if(lane<1||lane>PP_MAX_ENTRIES||!lap||!ensure())return false;bool changed=false;if(!data_.pb[lane-1]||lap<data_.pb[lane-1]){data_.pb[lane-1]=lap;changed=true;}if(!data_.track||lap<data_.track){data_.track=lap;data_.trackLane=lane;changed=true;}return !changed||write();}
  Time lanePb(uint8_t lane)override{return lane>=1&&lane<=PP_MAX_ENTRIES&&ensure()?data_.pb[lane-1]:0;}
  Time trackRecord()override{return ensure()?data_.track:0;}
  uint32_t era()override{return ensure()?data_.era:0;}
  bool clearLane(uint8_t lane)override{if(lane<1||lane>PP_MAX_ENTRIES||!ensure())return false;data_.pb[lane-1]=0;recompute();++data_.era;return write();}
  bool clearTrack()override{if(!ensure())return false;data_.track=0;data_.trackLane=0;++data_.era;return write();}
  bool clearAll()override{if(!ensure())return false;for(uint8_t i=0;i<PP_MAX_ENTRIES;++i)data_.pb[i]=0;data_.track=0;data_.trackLane=0;++data_.era;return write();}
private:
  struct Data{uint32_t era=1;Time pb[PP_MAX_ENTRIES]{};Time track=0;uint8_t trackLane=0;};
  Store&store_;Data data_{};bool loaded_=false;
  static uint32_t sum(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
  static constexpr size_t EncodedBytes=4+4+4+PP_MAX_ENTRIES*8+8+1+4;
    bool ensure(){if(loaded_)return store_.available();loaded_=true;if(!store_.available())return false;uint8_t bytes[EncodedBytes]{};if(!store_.read(Slot,bytes,sizeof(bytes)))return true;size_t p=0;auto u32=[&](uint32_t&v){v=uint32_t(bytes[p])|(uint32_t(bytes[p+1])<<8)|(uint32_t(bytes[p+2])<<16)|(uint32_t(bytes[p+3])<<24);p+=4;};auto u64=[&](uint64_t&v){v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(bytes[p+i])<<(i*8);p+=8;};uint32_t magic=0,version=0,checksum=0;uint8_t trackLane=0;u32(magic);u32(version);if(magic!=Magic||version!=Version)return true;u32(data_.era);for(uint8_t i=0;i<PP_MAX_ENTRIES;++i)u64(data_.pb[i]);u64(data_.track);trackLane=bytes[p++];u32(checksum);if(checksum!=sum(bytes,sizeof(bytes)-4)||data_.era==0)data_={};else data_.trackLane=trackLane;return true;}
  bool write(){const size_t size=EncodedBytes;uint8_t bytes[size]{};size_t p=0;auto put32=[&](uint32_t v){bytes[p++]=uint8_t(v);bytes[p++]=uint8_t(v>>8);bytes[p++]=uint8_t(v>>16);bytes[p++]=uint8_t(v>>24);};auto put64=[&](uint64_t v){for(unsigned i=0;i<8;++i)bytes[p++]=uint8_t(v>>(i*8));};put32(Magic);put32(Version);put32(data_.era);for(uint8_t i=0;i<PP_MAX_ENTRIES;++i)put64(data_.pb[i]);put64(data_.track);bytes[p++]=data_.trackLane;const uint32_t checksum=sum(bytes,size-4);put32(checksum);return store_.write(Slot,bytes,size);}
  void recompute(){data_.track=0;data_.trackLane=0;for(uint8_t i=0;i<PP_MAX_ENTRIES;++i)if(data_.pb[i]&&(!data_.track||data_.pb[i]<data_.track)){data_.track=data_.pb[i];data_.trackLane=i+1;}}
};

}
