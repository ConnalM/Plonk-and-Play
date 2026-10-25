#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "pp/persistent_storage.h"

using namespace pp;

class TestStore final : public Store {
public:
  bool online=true; int failWriteSlot=-1;
  int failUnreferencedSlot=-1;
  std::vector<bool> stale=std::vector<bool>(256,false);
  std::vector<unsigned> removed;
  std::vector<std::vector<uint8_t>> slots=std::vector<std::vector<uint8_t>>(256);
  bool available()const override{return online;}
  bool read(unsigned slot,uint8_t*out,size_t n)override{if(!online||!out||slot>=slots.size()||slots[slot].size()!=n)return false;memcpy(out,slots[slot].data(),n);return true;}
  bool write(unsigned slot,const uint8_t*in,size_t n)override{if(!online||!in||slot>=slots.size())return false;if(static_cast<int>(slot)==failWriteSlot){failWriteSlot=-1;return false;}slots[slot].assign(in,in+n);return true;}
  bool writeUnreferenced(unsigned slot,const uint8_t*in,size_t n)override{
    if(!online||!in||slot>=slots.size()||static_cast<int>(slot)==failUnreferencedSlot)return false;
    if(stale[slot]){stale[slot]=false;slots[slot].clear();removed.push_back(slot);}
    return write(slot,in,n);
  }
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

  TestStore orphanStore; SlotHistoryStore orphanHistory(orphanStore); uint32_t orphanSequence=0;
  const char first[]="orphan-first"; const char second[]="orphan-second";
  const bool firstStored=orphanHistory.append(reinterpret_cast<const uint8_t*>(first),strlen(first),orphanSequence);
  orphanStore.stale[8]=true; orphanStore.slots[8]={'r','e','f'};
  orphanStore.stale[9]=true; orphanStore.slots[9]={'o','r','p'};
  const bool replaced=orphanHistory.append(reinterpret_cast<const uint8_t*>(second),strlen(second),orphanSequence);
  const bool removedOnlyFree=std::find(orphanStore.removed.begin(),orphanStore.removed.end(),9u)!=orphanStore.removed.end()&&std::find(orphanStore.removed.begin(),orphanStore.removed.end(),8u)==orphanStore.removed.end()&&orphanStore.stale[8];
  check("WP2B.7",firstStored&&replaced&&removedOnlyFree,"stale_unreferenced_payload_replaced_referenced_payload_preserved=1");

  TestStore blockedStore; SlotHistoryStore blockedHistory(blockedStore); uint32_t blockedSequence=0;
  const bool blockedFirst=blockedHistory.append(reinterpret_cast<const uint8_t*>(first),strlen(first),blockedSequence);
  blockedStore.stale[9]=true; blockedStore.slots[9]={'o','r','p'}; blockedStore.failUnreferencedSlot=9; blockedSequence=0;
  const bool blockedAppend=!blockedHistory.append(reinterpret_cast<const uint8_t*>(second),strlen(second),blockedSequence)&&blockedSequence==0&&blockedHistory.count()==1&&blockedStore.stale[9]&&blockedStore.removed.empty()&&loadText(blockedHistory,0,"orphan-first",1);
  check("WP2B.8",blockedFirst&&blockedAppend,"stale_payload_clear_failure_preserves_metadata=1");

  TestStore detailStore;SlotHistoryStore raw(detailStore);VersionedHistoryStore historyWithDetail(raw);
  RaceEngineModule::CompletedRaceResult result{};result.sealed=true;result.valid=true;result.formatVersion=RaceEngineModule::ResultFormatVersion;result.mode=SessionMode::Endurance;result.behaviour=LapFinishBehaviour::CompleteCurrentLap;result.entryCount=1;result.entries[0].lane=1;result.entries[0].completed=true;result.entries[0].detail.prepare(220);result.entries[0].laps=220;result.entries[0].classifiedLaps=220;
  for(uint32_t i=0;i<220;++i){RaceEngineModule::LapRecord lap{};lap.lapNumber=i+1;lap.startTime=i*1000000;lap.finishTime=(i+1)*1000000;lap.lapTime=1000000;lap.racingFinishTime=lap.finishTime;lap.valid=true;result.entries[0].detail.append(lap);}result.entries[0].records=result.entries[0].detail.records;result.entries[0].recordCount=result.entries[0].detail.recordCount;
  uint8_t* encodedBytes=new uint8_t[VersionedResultCodec::MaxBytes]{};size_t encodedLength=0;bool encoded=VersionedResultCodec::encode(result,encodedBytes,VersionedResultCodec::MaxBytes,encodedLength);check("WP2B.5",encoded&&encodedLength>8192,"detail_payload_exceeds_old_8192=1");delete[] encodedBytes;
  uint32_t detailSequence=0;bool detailStored=historyWithDetail.append(reinterpret_cast<const uint8_t*>(&result),sizeof(result),detailSequence);RaceEngineModule::CompletedRaceResult recovered{};size_t recoveredBytes=0;uint32_t recoveredSequence=0;bool detailRecovered=detailStored&&historyWithDetail.loadNewest(0,reinterpret_cast<uint8_t*>(&recovered),sizeof(recovered),recoveredBytes,recoveredSequence)&&recovered.entries[0].recordCount==220&&recovered.entries[0].records[219].lapNumber==220;check("WP2B.6",detailRecovered,"more_than_24_detail_records_reconstruct=1");
  return failures?1:0;
}
