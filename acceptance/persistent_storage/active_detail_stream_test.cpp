#include <cstdio>
#include <vector>
#include "pp/persistent_storage.h"

using namespace pp;

struct Stream final : ActiveDetailStore {
  std::vector<LapRecord> records;
  bool complete_=true,finalized_=false;
  bool begin(uint32_t,uint8_t) override { records.clear(); complete_=true; finalized_=false; return true; }
  bool append(uint8_t,const LapRecord& r) override { records.push_back(r); return true; }
  void drain() override {}
  bool finalize() override { finalized_=true; return true; }
  void abort() override { records.clear(); finalized_=false; complete_=true; }
  uint32_t dropped() const override { return 0; }
  uint8_t detailEntryCount() const override { return 1; }
  uint32_t detailRecordCount(uint8_t e) const override { return e==0 ? static_cast<uint32_t>(records.size()) : 0; }
  bool readDetail(uint8_t e,uint32_t i,LapRecord& out) const override { if(e||i>=records.size())return false; out=records[i]; return true; }
  bool detailComplete() const override { return complete_&&finalized_; }
};

struct Backend final : HistoryStore {
  std::vector<uint8_t> payload; uint32_t sequence=0;
  bool append(const uint8_t* b,size_t n,uint32_t& s) override { payload.assign(b,b+n); s=++sequence; return true; }
  bool loadNewest(uint8_t index,uint8_t*b,size_t cap,size_t&n,uint32_t&s) override { if(index||payload.size()>cap)return false; memcpy(b,payload.data(),payload.size());n=payload.size();s=sequence;return true; }
  uint8_t count() override { return payload.empty()?0:1; }
  size_t payloadSize(uint8_t index) override { return index==0?payload.size():0; }
  size_t maximumPayloadBytes() override { return payload.size(); }
  bool clear() override { payload.clear(); return true; }
};

int main() {
  Bus bus; ActiveSessionDefinition active;
  const auto engineEndpoint=bus.attach(Role::RaceEngine), sourceEndpoint=bus.attach(Role::Input), controlEndpoint=bus.attach(Role::RaceControl);
  bus.subscribe(engineEndpoint,Type::GoScheduled);bus.subscribe(engineEndpoint,Type::InputEvent);bus.subscribe(engineEndpoint,Type::FinishSettled);bus.subscribe(sourceEndpoint,Type::FinishSettlement);bus.subscribe(controlEndpoint,Type::CompetitionComplete);
  ProposedRaceSetup setup{};setup.activeLanes=1;setup.lapTarget=3500;setup.mode=SessionMode::LapRace;setup.entries[0].startFinish={InputModule::simulatedDetectorIdentity(),1,InputRole::StartFinish};setup.entries[0].mugId=903;
  if(!active.commit(setup,1,1))return 1; SessionDefinition definition(setup,1,1); Stream stream; Backend backend; VersionedHistoryStore history(backend);
  RaceEngineModule engine(bus,engineEndpoint,active,&history,nullptr,&stream); engine.begin(definition,1000000);
  Message go{};go.type=Type::GoScheduled;go.relevantTime=1000000;bus.publish(sourceEndpoint,go);engine.tick();
  const auto input=InputModule::simulatedDetectorIdentity();for(uint32_t lap=1;lap<=3500;++lap){Message p{};p.type=Type::InputEvent;p.eventId=lap;p.input=input;p.relevantTime=1000000+Time(lap)*100000;bus.publish(sourceEndpoint,p);engine.tick();}
  Message settled{};settled.type=Type::FinishSettled;settled.relevantTime=351000000;bus.publish(sourceEndpoint,settled);engine.tick();
  const auto& result=engine.completedResult();const bool boundedActive=result.entries[0].recordCount==3500&&result.entries[0].detail.records==nullptr&&stream.records.size()==3500;
  const bool committed=backend.count()==1&&backend.payload.size()>128u*1024u;
  RaceEngineModule::CompletedRaceResult recovered{};size_t recoveredBytes=0;uint32_t recoveredSequence=0;const bool reconstructed=history.loadNewest(0,reinterpret_cast<uint8_t*>(&recovered),sizeof(recovered),recoveredBytes,recoveredSequence)&&recovered.entries[0].recordCount==3500&&recovered.entries[0].records[3499].lapNumber==3500;
  const bool pass=boundedActive&&committed&&reconstructed&&result.entries[0].laps==3500&&result.entries[0].detailComplete;RaceEngineModule::clearResult(recovered);
  std::printf("WP2D active_detail_stream %s laps=%lu streamed=%lu payload_bytes=%lu resident_records=%lu\n",pass?"PASS":"FAIL",(unsigned long)result.entries[0].laps,(unsigned long)stream.records.size(),(unsigned long)backend.payload.size(),(unsigned long)result.entries[0].detail.recordCount);
  return pass?0:1;
}
