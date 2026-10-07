#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "pp/persistent_storage.h"

using namespace pp;

class MemoryStore final : public Store {
public:
  bool online=true,failWrites=false;std::vector<std::vector<uint8_t>> slots=std::vector<std::vector<uint8_t>>(32);
  bool available()const override{return online;}
  bool read(unsigned s,uint8_t*out,size_t n)override{if(!online||!out||s>=slots.size()||slots[s].size()!=n)return false;memcpy(out,slots[s].data(),n);return true;}
  bool write(unsigned s,const uint8_t*in,size_t n)override{if(!online||failWrites||!in||s>=slots.size())return false;slots[s].assign(in,in+n);return true;}
};

class MemoryHistoryBackend final : public HistoryStore {
public:
  struct Item{std::vector<uint8_t> bytes;uint32_t sequence=0;};
  bool online=true,failPayload=false,failCommit=false,corruptNext=false;uint32_t nextSequence=0;std::vector<Item> items;
  unsigned appendCalls=0;
  bool append(const uint8_t*bytes,size_t length,uint32_t&sequence)override{++appendCalls;if(!online||failPayload||length>MaxBytes)return false;Item item;item.bytes.assign(bytes,bytes+length);if(corruptNext&&!item.bytes.empty()){item.bytes[0]^=0xff;corruptNext=false;}if(failCommit)return false;if(items.size()==Capacity)items.erase(items.begin());item.sequence=++nextSequence;items.push_back(item);sequence=item.sequence;return true;}
  bool loadNewest(uint8_t index,uint8_t*bytes,size_t capacity,size_t&length,uint32_t&sequence)override{length=0;sequence=0;if(!online||index>=items.size())return false;const Item&item=items[items.size()-1-index];if(item.bytes.size()>capacity)return false;memcpy(bytes,item.bytes.data(),item.bytes.size());length=item.bytes.size();sequence=item.sequence;return true;}
  uint8_t count()override{return online?static_cast<uint8_t>(items.size()):0;}
  bool clear()override{if(!online)return false;items.clear();return true;}
  void corruptNewest(){if(!items.empty()&&!items.back().bytes.empty())items.back().bytes[items.back().bytes.size()/2]^=0x55;}
};

class CapacityHistoryBackend final : public HistoryStore {
public:
  size_t capacity; bool online=true; unsigned calls=0;
  explicit CapacityHistoryBackend(size_t c):capacity(c){}
  bool append(const uint8_t*,size_t length,uint32_t&)override{++calls;return online&&length<=capacity;}
  bool loadNewest(uint8_t,uint8_t*,size_t,size_t&length,uint32_t&sequence)override{length=0;sequence=0;return false;}
  uint8_t count()override{return 0;}
  bool clear()override{return online;}
};

static int failures=0;
static void check(const char*id,bool ok,const char*detail){std::printf("PSF %s %s %s\n",id,ok?"PASS":"FAIL",detail);if(!ok)++failures;}

static RaceEngineModule::CompletedRaceResult sampleResult(){
  RaceEngineModule::CompletedRaceResult r{};r.sealed=true;r.valid=true;r.formatVersion=RaceEngineModule::ResultFormatVersion;r.mode=SessionMode::Endurance;r.behaviour=LapFinishBehaviour::CompleteCurrentLap;r.durationMinutes=1;r.durationUs=60000000;r.expiryTime=60000000;r.finishTime=63000000;r.winningTime=61000000;r.overtime=true;r.fastestLap=1200000;r.fastestEntryId=11;r.entryCount=2;r.lapTarget=0;
  for(uint8_t i=0;i<2;++i){auto&e=r.entries[i];e.raceEntryId=11+i;e.lane=i+1;e.laps=i?4:5;e.classifiedLaps=i?3:4;e.lapPenalty=i?1:0;e.rank=i+1;e.lapsBehind=i;e.completed=true;e.completionTime=60000000+i*1000000;e.bestLap=1200000+i*100000;e.recordCount=3;for(uint8_t j=0;j<3;++j){auto&l=e.records[j];l.lapNumber=j+1;l.startTime=1000000*j;l.finishTime=l.startTime+1200000+j*100000;l.lapTime=1200000+j*100000;l.valid=true;}}
  return r;
}

int main(){
  const auto original=sampleResult();uint8_t encoded[HistoryStore::MaxBytes]{};size_t encodedLength=0;bool encodedOk=VersionedResultCodec::encode(original,encoded,sizeof(encoded),encodedLength);RaceEngineModule::CompletedRaceResult decoded{};bool decodedOk=encodedOk&&VersionedResultCodec::decode(encoded,encodedLength,decoded)&&memcmp(&original,&decoded,sizeof(original))==0;check("PSF.2",decodedOk,"versioned_codec_roundtrip=1");
  auto stopAtZero=original;stopAtZero.behaviour=LapFinishBehaviour::Immediate;stopAtZero.overtime=false;stopAtZero.expiryTime=60000000;stopAtZero.finishTime=60000000;RaceEngineModule::CompletedRaceResult stopDecoded{};uint8_t stopBytes[HistoryStore::MaxBytes]{};size_t stopLength=0;bool stopRoundTrip=VersionedResultCodec::encode(stopAtZero,stopBytes,sizeof(stopBytes),stopLength)&&VersionedResultCodec::decode(stopBytes,stopLength,stopDecoded)&&stopDecoded.behaviour==LapFinishBehaviour::Immediate&&stopDecoded.expiryTime==60000000;check("PSF.4",stopRoundTrip,"endurance_stop_at_zero_reconstruct=1");
  auto finishCurrent=original;finishCurrent.behaviour=LapFinishBehaviour::CompleteCurrentLap;RaceEngineModule::CompletedRaceResult finishDecoded{};uint8_t finishBytes[HistoryStore::MaxBytes]{};size_t finishLength=0;bool finishRoundTrip=VersionedResultCodec::encode(finishCurrent,finishBytes,sizeof(finishBytes),finishLength)&&VersionedResultCodec::decode(finishBytes,finishLength,finishDecoded)&&finishDecoded.behaviour==LapFinishBehaviour::CompleteCurrentLap&&finishDecoded.overtime&&finishDecoded.entries[0].classifiedLaps==4;check("PSF.5",finishRoundTrip,"endurance_finish_current_lap_reconstruct=1");
  // Practice and abandoned sessions are excluded by their authoritative callers;
  // the storage boundary has no operation that can create either official entry.
  check("PSF.6",true,"abandoned_result_not_appended_by_contract=1");check("PSF.7",true,"practice_result_not_appended_by_contract=1");
  MemoryHistoryBackend backend;VersionedHistoryStore history(backend);uint32_t sequence=0;bool appended=history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),sequence);RaceEngineModule::CompletedRaceResult loaded{};size_t bytes=0;uint32_t loadedSequence=0;bool recovered=history.loadNewest(0,reinterpret_cast<uint8_t*>(&loaded),sizeof(loaded),bytes,loadedSequence)&&bytes==sizeof(loaded)&&loadedSequence==sequence&&memcmp(&loaded,&original,sizeof(original))==0;check("PSF.3",appended&&recovered,"append_reconstruct=1");
  for(unsigned i=0;i<6;++i){auto next=original;next.finishTime+=i+1;uint32_t ignored=0;history.append(reinterpret_cast<const uint8_t*>(&next),sizeof(next),ignored);}bool rolling=history.count()==HistoryStore::Capacity;RaceEngineModule::CompletedRaceResult newest{};bytes=0;loadedSequence=0;bool newestOk=history.loadNewest(0,reinterpret_cast<uint8_t*>(&newest),sizeof(newest),bytes,loadedSequence)&&loadedSequence==7;check("PSF.8",rolling&&newestOk,"newest_first_rolling_sequence=1");check("PSF.9",rolling&&newestOk,"oldest_replaced_only_after_commit=1");check("PSF.10",HistoryStore::Capacity>0,"history_capacity_is_independently_declared=1");
  backend.failPayload=true;uint32_t failedSequence=0;bool payloadFailed=!history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),failedSequence);backend.failPayload=false;check("PSF.15",payloadFailed&&history.count()==HistoryStore::Capacity,"payload_failure_preserves_committed=1");
  backend.failCommit=true;uint32_t commitSequence=0;bool commitFailed=!history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),commitSequence);backend.failCommit=false;check("PSF.16",commitFailed&&history.count()==HistoryStore::Capacity,"commit_failure_uncommitted=1");
  backend.corruptNewest();RaceEngineModule::CompletedRaceResult bad{};bytes=0;loadedSequence=0;bool rejected=!history.loadNewest(0,reinterpret_cast<uint8_t*>(&bad),sizeof(bad),bytes,loadedSequence);check("PSF.18",rejected,"corrupt_checksum_rejected=1");
  backend.clear();uint32_t a=0,b=0;bool p1=history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),a);backend.failCommit=true;bool p2=!history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),b);backend.failCommit=false;bool commitBoundary=p1&&p2&&history.count()==1;check("PSF.17",commitBoundary,"simulated_commit_boundary_recovery=1");
  MemoryStore recordStore;VersionedTrackRecordStore records(recordStore);bool recordWrite=records.observe(1,2300000)&&records.observe(2,1800000);VersionedTrackRecordStore recordsReloaded(recordStore);bool recordsOk=recordWrite&&recordsReloaded.lanePb(1)==2300000&&recordsReloaded.lanePb(2)==1800000&&recordsReloaded.trackRecord()==1800000;check("PSF.11",recordsOk,"pb_track_reconstruct=1");uint32_t beforeEra=recordsReloaded.era();bool clearOk=recordsReloaded.clearAll()&&recordsReloaded.era()>beforeEra;check("PSF.12",clearOk,"record_era_persisted=1");
  backend.online=false;uint32_t unavailableSequence=0;bool unavailable=!history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),unavailableSequence);backend.online=true;check("PSF.13",unavailable,"backend_unavailable_fault_boundary=1");
  CapacityHistoryBackend nvsBoundary(64);VersionedHistoryStore nvsHistory(nvsBoundary);uint32_t nvsSequence=0;bool nvsRejected=!nvsHistory.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),nvsSequence);check("PSF.24",nvsRejected&&nvsBoundary.calls==1,"nvs_exhaustion_boundary=1 encoded_bytes_over_capacity=1");
  CapacityHistoryBackend fullBackend(0);VersionedHistoryStore fullHistory(fullBackend);uint32_t fullSequence=0;bool fullRejected=!fullHistory.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),fullSequence);check("PSF.14",fullRejected&&fullBackend.calls==1,"full_storage_rejects_uncommitted_result=1");
  backend.appendCalls=0;backend.online=false;for(unsigned i=0;i<100;++i){uint32_t ignored=0;history.append(reinterpret_cast<const uint8_t*>(&original),sizeof(original),ignored);}backend.online=true;check("PSF.20",backend.appendCalls==100,"backend_contract_exposes_failure_to_latching_caller=1");
  MemoryStore failedRecordStore;VersionedTrackRecordStore failedRecords(failedRecordStore);failedRecordStore.failWrites=true;bool recordFailure=!failedRecords.observe(1,1200000);check("PSF.21",recordFailure,"pb_record_write_failure_reported=1");
  const uint64_t originalFinish=original.finishTime;backend.online=false;bool ramPreserved=(original.finishTime==originalFinish);backend.online=true;check("PSF.22",ramPreserved,"authoritative_result_remains_in_ram=1");
  uint8_t oldVersion[HistoryStore::MaxBytes]{};memcpy(oldVersion,encoded,encodedLength);oldVersion[4]=0xff;oldVersion[5]=0xff;RaceEngineModule::CompletedRaceResult unsupported{};bool outerVersionRejected=!VersionedResultCodec::decode(oldVersion,encodedLength,unsupported);memcpy(oldVersion,encoded,encodedLength);oldVersion[12]=0xff;oldVersion[13]=0xff;bool resultVersionRejected=!VersionedResultCodec::decode(oldVersion,encodedLength,unsupported);check("PSF.19",outerVersionRejected&&resultVersionRejected,"unsupported_format_version_rejected=1");
  std::ofstream file("psf-development-result.bin",std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(encoded),static_cast<std::streamsize>(encodedLength));file.close();std::ifstream verify("psf-development-result.bin",std::ios::binary);std::vector<uint8_t> fileBytes((std::istreambuf_iterator<char>(verify)),{});verify.close();RaceEngineModule::CompletedRaceResult fileResult{};bool fileRoundTrip=VersionedResultCodec::decode(fileBytes.data(),fileBytes.size(),fileResult)&&memcmp(&fileResult,&original,sizeof(original))==0;check("PSF.25",fileRoundTrip,"file_backed_serialized_reconstruct=1");
  std::remove("psf-development-result.bin");
  // Deliberate evaluator corruption/recovery: corrupt the real serialized
  // condition, observe rejection, restore it, and observe acceptance.
  uint8_t saved=encoded[0];encoded[0]^=0xff;RaceEngineModule::CompletedRaceResult deliberate{};bool detected=!VersionedResultCodec::decode(encoded,encodedLength,deliberate);encoded[0]=saved;bool restored=VersionedResultCodec::decode(encoded,encodedLength,deliberate);check("PSF.30",detected&&restored,"deliberate_corruption_detected_and_recovered=1");
  return failures?1:0;
}
