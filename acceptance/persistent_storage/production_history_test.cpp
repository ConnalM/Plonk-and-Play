#include <cstdio>
#include <cstring>
#include <vector>
#include "pp/persistent_storage.h"

using namespace pp;

class TestStore final : public Store {
public:
  bool online=true; int failWriteSlot=-1;
  std::vector<std::vector<uint8_t>> slots=std::vector<std::vector<uint8_t>>(256);
  bool available()const override{return online;}
  bool read(unsigned slot,uint8_t*out,size_t n)override{if(!online||!out||slot>=slots.size()||slots[slot].size()!=n)return false;memcpy(out,slots[slot].data(),n);return true;}
  bool write(unsigned slot,const uint8_t*in,size_t n)override{if(!online||!in||slot>=slots.size())return false;if(static_cast<int>(slot)==failWriteSlot){failWriteSlot=-1;return false;}slots[slot].assign(in,in+n);return true;}
};

static int failures=0;
static void check(const char*id,bool ok,const char*detail){std::printf("PSF %s %s %s\n",id,ok?"PASS":"FAIL",detail);if(!ok)++failures;}

static bool loadText(HistoryStore&history,uint8_t index,const char*expected,uint32_t wanted=0){
  uint8_t bytes[128]{};size_t length=0;uint32_t sequence=0;
  return history.loadNewest(index,bytes,sizeof(bytes),length,sequence)&&length==strlen(expected)&&memcmp(bytes,expected,length)==0&&(!wanted||sequence==wanted);
}

int main(){
  TestStore store;SlotHistoryStore history(store);uint32_t sequence=0;bool first100=true;
  for(unsigned i=1;i<=100;++i){char text[32];snprintf(text,sizeof(text),"race-%u",i);if(!history.append(reinterpret_cast<const uint8_t*>(text),strlen(text),sequence)||sequence!=i)first100=false;}
  check("WP2B.1",first100&&history.count()==100,"one_to_100_committed=1");
  check("WP2B.2",loadText(history,0,"race-100",100)&&loadText(history,99,"race-1",1),"newest_first_100=1");

  store.failWriteSlot=208;uint32_t failed=0;const char replacement[]="race-101";
  bool failed101=!history.append(reinterpret_cast<const uint8_t*>(replacement),strlen(replacement),failed)&&failed==0&&history.count()==100&&loadText(history,0,"race-100",100)&&loadText(history,99,"race-1",1);
  check("WP2B.3",failed101,"failed_101_preserves_1_to_100=1");
  bool retry=history.append(reinterpret_cast<const uint8_t*>(replacement),strlen(replacement),sequence)&&sequence==101&&history.count()==100&&loadText(history,0,"race-101",101)&&loadText(history,99,"race-2",2)&&!loadText(history,99,"race-1",1);
  check("WP2B.4",retry,"retry_evicts_only_after_commit=1");

  TestStore detailStore;SlotHistoryStore raw(detailStore);VersionedHistoryStore historyWithDetail(raw);
  RaceEngineModule::CompletedRaceResult result{};result.sealed=true;result.valid=true;result.formatVersion=RaceEngineModule::ResultFormatVersion;result.mode=SessionMode::Endurance;result.behaviour=LapFinishBehaviour::CompleteCurrentLap;result.entryCount=1;result.entries[0].lane=1;result.entries[0].completed=true;result.entries[0].detail.prepare(220);result.entries[0].laps=220;result.entries[0].classifiedLaps=220;
  for(uint32_t i=0;i<220;++i){RaceEngineModule::LapRecord lap{};lap.lapNumber=i+1;lap.startTime=i*1000000;lap.finishTime=(i+1)*1000000;lap.lapTime=1000000;lap.racingFinishTime=lap.finishTime;lap.valid=true;result.entries[0].detail.append(lap);}result.entries[0].records=result.entries[0].detail.records;result.entries[0].recordCount=result.entries[0].detail.recordCount;
  uint8_t* encodedBytes=new uint8_t[VersionedResultCodec::MaxBytes]{};size_t encodedLength=0;bool encoded=VersionedResultCodec::encode(result,encodedBytes,VersionedResultCodec::MaxBytes,encodedLength);check("WP2B.5",encoded&&encodedLength>8192,"detail_payload_exceeds_old_8192=1");delete[] encodedBytes;
  uint32_t detailSequence=0;bool detailStored=historyWithDetail.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),detailSequence);RaceEngineModule::CompletedRaceResult recovered{};size_t recoveredBytes=0;uint32_t recoveredSequence=0;bool detailRecovered=detailStored&&historyWithDetail.loadNewest(0,reinterpret_cast<uint8_t*>(&recovered),sizeof(recovered),recoveredBytes,recoveredSequence)&&recovered.entries[0].recordCount==220&&recovered.entries[0].records[219].lapNumber==220;check("WP2B.6",detailRecovered,"more_than_24_detail_records_reconstruct=1");

  // PSF.18/19: one invalid committed payload is excluded while its valid
  // sibling remains readable. The committed bytes are never repaired.
  uint32_t secondSequence=0;
  const bool secondStored=historyWithDetail.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),secondSequence);
  const size_t originalBytes=detailStore.slots[9].size();
  detailStore.slots[9].resize(1);
  SlotHistoryStore wrongSizeRaw(detailStore); VersionedHistoryStore wrongSizeHistory(wrongSizeRaw);
  const bool wrongSizeReconstructed=wrongSizeHistory.reconstruct();
  RaceEngineModule::CompletedRaceResult sibling{};size_t siblingBytes=0;uint32_t siblingSequence=0;
  const bool siblingSurvives=wrongSizeReconstructed&&wrongSizeHistory.health()==StoreHealth::Corrupt&&wrongSizeHistory.invalidCount()==1&&
    !wrongSizeHistory.loadNewest(0,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&
    wrongSizeHistory.loadNewest(1,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&siblingSequence==detailSequence&&
    detailStore.slots[9].size()==1;
  check("PSF18_wrong_size_sibling",secondStored&&siblingSurvives,"invalid_record_excluded_valid_sibling_retained_no_repair=1");

  TestStore corruptStore; SlotHistoryStore corruptRaw(corruptStore); VersionedHistoryStore corruptHistory(corruptRaw); uint32_t corruptFirst=0,corruptSecond=0;
  const bool corruptStored=corruptHistory.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),corruptFirst)&&corruptHistory.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),corruptSecond);
  corruptStore.slots[9].back()^=0x01; SlotHistoryStore corruptRawReload(corruptStore); VersionedHistoryStore corruptReload(corruptRawReload);
  const bool checksumSibling=corruptStored&&corruptReload.reconstruct()&&corruptReload.health()==StoreHealth::Corrupt&&corruptReload.invalidCount()==1&&
    !corruptReload.loadNewest(0,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&
    corruptReload.loadNewest(1,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&siblingSequence==corruptFirst;
  check("PSF18_checksum_sibling",checksumSibling,"checksum_corrupt_record_excluded_valid_sibling_retained=1");

  TestStore unsupportedStore; SlotHistoryStore unsupportedRaw(unsupportedStore); VersionedHistoryStore unsupportedHistory(unsupportedRaw); uint32_t unsupportedFirst=0,unsupportedSecond=0;
  const bool unsupportedStored=unsupportedHistory.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),unsupportedFirst)&&unsupportedHistory.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),unsupportedSecond);
  unsupportedStore.slots[9][4]=0x09; unsupportedStore.slots[9][5]=0x00; SlotHistoryStore unsupportedRawReload(unsupportedStore); VersionedHistoryStore unsupportedReload(unsupportedRawReload);
  const bool unsupportedSibling=unsupportedStored&&unsupportedReload.reconstruct()&&unsupportedReload.health()==StoreHealth::Unsupported&&unsupportedReload.invalidCount()==1&&
    !unsupportedReload.loadNewest(0,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&
    unsupportedReload.loadNewest(1,reinterpret_cast<uint8_t*>(&sibling),sizeof(sibling),siblingBytes,siblingSequence)&&siblingSequence==unsupportedFirst;
  check("PSF19_unsupported_sibling",unsupportedSibling,"unsupported_record_excluded_valid_sibling_retained=1");
  (void)originalBytes;
  return failures?1:0;
}
