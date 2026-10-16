#include <cstdio>
#include <vector>
#include "pp/persistent_storage.h"
using namespace pp;

struct RecoveryStream final : ActiveDetailStore {
  std::vector<LapRecord> records;
  bool available=false,lost=false,finalized=false;
  size_t queueLimit=1000;
  bool begin(uint32_t,uint8_t) override {records.clear();lost=false;finalized=false;return true;}
  bool append(uint8_t,const LapRecord&r) override {if(lost)return false;if(records.size()>=queueLimit){lost=true;return false;}records.push_back(r);return true;}
  void drain() override {}
  bool finalize() override {if(!available||lost)return false;finalized=true;return true;}
  void abort() override {records.clear();finalized=false;}
  uint32_t dropped()const override{return lost?1:0;}
  bool storagePending()const override{return !available&&!lost;}
  bool detailLost()const override{return lost;}
  bool recover()override{available=true;return true;}
  uint8_t detailEntryCount()const override{return 1;}
  uint32_t detailRecordCount(uint8_t e)const override{return e?0:uint32_t(records.size());}
  bool readDetail(uint8_t e,uint32_t i,LapRecord&o)const override{if(e||i>=records.size())return false;o=records[i];return true;}
  bool detailComplete()const override{return available&&finalized&&!lost;}
};

struct History final : HistoryStore {
  unsigned appendCalls=0; std::vector<uint8_t> committed;
  bool append(const uint8_t*,size_t,uint32_t&s)override{++appendCalls;s=appendCalls;return true;}
  bool appendDetailed(const void*,size_t,const DetailSource&source,uint32_t&s)override{++appendCalls;committed.clear();LapRecord r{};for(uint32_t i=0;i<source.detailRecordCount(0);++i){if(!source.readDetail(0,i,r))return false;committed.push_back(uint8_t(r.lapNumber));}s=appendCalls;return true;}
  bool loadNewest(uint8_t,uint8_t*,size_t,size_t&,uint32_t&)override{return false;}
  uint8_t count()override{return appendCalls?1:0;}
  bool clear()override{appendCalls=0;committed.clear();return true;}
};

static bool runRace(RecoveryStream&stream,History&history) {
  Bus bus; ActiveSessionDefinition active;
  auto ep=bus.attach(Role::RaceEngine),src=bus.attach(Role::Input),ctl=bus.attach(Role::RaceControl);
  bus.subscribe(ep,Type::GoScheduled);bus.subscribe(ep,Type::InputEvent);bus.subscribe(ep,Type::FinishSettled);
  bus.subscribe(src,Type::FinishSettlement);bus.subscribe(ctl,Type::CompetitionComplete);
  ProposedRaceSetup setup{};setup.activeLanes=1;setup.lapTarget=12;setup.mode=SessionMode::LapRace;setup.entries[0].mugId=904;
  setup.entries[0].startFinish={InputModule::simulatedDetectorIdentity(),1,InputRole::StartFinish};
  if(!active.commit(setup,1,1))return false;SessionDefinition d(setup,1,1);
  RaceEngineModule engine(bus,ep,active,&history,nullptr,&stream);engine.begin(d,1000000);
  Message go{};go.type=Type::GoScheduled;go.relevantTime=1000000;bus.publish(src,go);engine.tick();
  auto in=InputModule::simulatedDetectorIdentity();for(uint32_t i=1;i<=12;++i){Message p{};p.type=Type::InputEvent;p.eventId=i;p.input=in;p.relevantTime=1000000+Time(i)*100000;bus.publish(src,p);engine.tick();}
  Message settled{};settled.type=Type::FinishSettled;settled.relevantTime=2200000;bus.publish(src,settled);engine.tick();
  for(unsigned i=0;i<3;++i)engine.tick();
  const auto&pending=engine.completedResult();
  const bool pendingOk=pending.sealed&&!pending.entries[0].detailComplete&&history.appendCalls==0&&engine.persistencePending();
  const bool recovered=engine.recoverPersistence();
  const auto&result=engine.completedResult();
  return pendingOk&&recovered&&history.appendCalls==1&&result.entries[0].detailComplete&&history.committed.size()==12&&history.committed.back()==12;
}

int main(){
  RecoveryStream beforeLoss;History h1;const bool before=runRace(beforeLoss,h1);
  RecoveryStream afterLoss;afterLoss.queueLimit=4;History h2;
  // The race remains sportingly valid, but actual queue overflow is permanent.
  Bus bus; ActiveSessionDefinition active;auto ep=bus.attach(Role::RaceEngine),src=bus.attach(Role::Input),ctl=bus.attach(Role::RaceControl);
  bus.subscribe(ep,Type::GoScheduled);bus.subscribe(ep,Type::InputEvent);bus.subscribe(ep,Type::FinishSettled);bus.subscribe(src,Type::FinishSettlement);bus.subscribe(ctl,Type::CompetitionComplete);
  ProposedRaceSetup setup{};setup.activeLanes=1;setup.lapTarget=12;setup.mode=SessionMode::LapRace;setup.entries[0].startFinish={InputModule::simulatedDetectorIdentity(),1,InputRole::StartFinish};active.commit(setup,1,1);SessionDefinition d(setup,1,1);RaceEngineModule engine(bus,ep,active,&h2,nullptr,&afterLoss);engine.begin(d,1000000);Message go{};go.type=Type::GoScheduled;go.relevantTime=1000000;bus.publish(src,go);engine.tick();auto in=InputModule::simulatedDetectorIdentity();for(uint32_t i=1;i<=12;++i){Message p{};p.type=Type::InputEvent;p.eventId=i;p.input=in;p.relevantTime=1000000+Time(i)*100000;bus.publish(src,p);engine.tick();}Message settled{};settled.type=Type::FinishSettled;settled.relevantTime=2200000;bus.publish(src,settled);engine.tick();for(unsigned i=0;i<3;++i)engine.tick();const bool lost=engine.completedResult().sealed&&!engine.completedResult().entries[0].detailComplete&&engine.persistenceFault()&&h2.appendCalls==0;afterLoss.recover();engine.recoverPersistence();const bool staysLost=!engine.completedResult().entries[0].detailComplete&&h2.appendCalls==0;
  const bool pass=before&&lost&&staysLost;std::printf("WP2D active_detail_recovery %s before_loss=%u after_loss=%u latched=%u\n",pass?"PASS":"FAIL",before?1:0,lost?1:0,staysLost?1:0);return pass?0:1;
}
