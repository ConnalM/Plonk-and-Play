#pragma once
#include "core.h"
namespace pp {
// Default standalone record context. It is intentionally separate from
// History: clearing one never reconstructs or removes the other.
class TrackRecordStore {
public:
  virtual ~TrackRecordStore() = default;
  virtual bool observe(uint8_t lane, Time lapTime) = 0;
  virtual Time lanePb(uint8_t lane) = 0;
  virtual Time trackRecord() = 0;
  virtual uint32_t era() = 0;
  virtual bool clearLane(uint8_t lane) = 0;
  virtual bool clearTrack() = 0;
  virtual bool clearAll() = 0;
};
class SlotTrackRecordStore final : public TrackRecordStore {
public:
  static constexpr uint8_t MaxEntries=PP_MAX_ENTRIES;
  explicit SlotTrackRecordStore(Store& store):store_(store){}
  bool observe(uint8_t lane,Time lapTime) override {if(lane<1||lane>MaxEntries||!lapTime||!ensure())return false;bool changed=false;if(!data_.pb[lane-1]||lapTime<data_.pb[lane-1]){data_.pb[lane-1]=lapTime;changed=true;}if(!data_.track||lapTime<data_.track){data_.track=lapTime;data_.trackLane=lane;changed=true;}return !changed||write();}
  Time lanePb(uint8_t lane) override{return lane>=1&&lane<=MaxEntries&&ensure()?data_.pb[lane-1]:0;}
  Time trackRecord() override{return ensure()?data_.track:0;}
  uint32_t era() override{return ensure()?data_.era:0;}
  bool clearLane(uint8_t lane) override{if(lane<1||lane>MaxEntries||!ensure())return false;data_.pb[lane-1]=0;recomputeTrack();++data_.era;return write();}
  bool clearTrack() override{if(!ensure())return false;data_.track=0;data_.trackLane=0;++data_.era;return write();}
  bool clearAll() override{if(!ensure())return false;for(uint8_t i=0;i<MaxEntries;++i)data_.pb[i]=0;data_.track=0;data_.trackLane=0;++data_.era;return write();}
private:
  static constexpr unsigned Slot=7;
  static constexpr uint32_t FormatVersion=2;
  struct Data{uint32_t magic=0x50505231u,format=FormatVersion,era=1;Time pb[MaxEntries]{};Time track=0;uint8_t trackLane=0;uint8_t reserved[7]{};uint32_t checksum=0;};
  Store&store_;Data data_{};bool loaded_=false;
  static uint32_t sum(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
  bool ensure(){if(loaded_)return store_.available();loaded_=true;if(!store_.available())return false;Data candidate{};if(!store_.read(Slot,reinterpret_cast<uint8_t*>(&candidate),sizeof(candidate)))return true;const auto expected=sum(reinterpret_cast<const uint8_t*>(&candidate),offsetof(Data,checksum));if(candidate.magic==0x50505231u&&candidate.format==FormatVersion&&candidate.era&&candidate.checksum==expected)data_=candidate;return true;}
  bool write(){data_.checksum=sum(reinterpret_cast<const uint8_t*>(&data_),offsetof(Data,checksum));return store_.write(Slot,reinterpret_cast<const uint8_t*>(&data_),sizeof(data_));}
  void recomputeTrack(){data_.track=0;data_.trackLane=0;for(uint8_t i=0;i<MaxEntries;++i)if(data_.pb[i]&&(!data_.track||data_.pb[i]<data_.track)){data_.track=data_.pb[i];data_.trackLane=i+1;}}
};
}
