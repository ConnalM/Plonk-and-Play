#include <cstdio>
#include <vector>
#include "pp/persistent_storage.h"
using namespace pp;
struct FailingStream final : ActiveDetailStore {
  std::vector<LapRecord> records; bool complete_=true,opened_=true,failFinalize=false; int failAppend=-1;
  bool begin(uint32_t,uint8_t) override { records.clear();complete_=opened_;return opened_; }
  bool append(uint8_t,const LapRecord&r) override { if(!opened_||(failAppend>=0&&int(records.size())>=failAppend)){complete_=false;return false;}records.push_back(r);return true; }
  void drain() override {}
  bool finalize() override { if(failFinalize)complete_=false;return complete_; }
  void abort() override { records.clear(); }
  uint32_t dropped()const override{return complete_?0:1;}
  uint8_t detailEntryCount()const override{return 1;}
  uint32_t detailRecordCount(uint8_t e)const override{return e?0:static_cast<uint32_t>(records.size());}
  bool readDetail(uint8_t e,uint32_t i,LapRecord&o)const override{if(e||i>=records.size())return false;o=records[i];return true;}
  bool detailComplete()const override{return complete_;}
};
struct History final : HistoryStore {
  unsigned appendCalls=0;
  bool append(const uint8_t*,size_t,uint32_t&s)override{++appendCalls;s=0;return false;}
  bool loadNewest(uint8_t,uint8_t*,size_t,size_t&,uint32_t&)override{return false;}
  uint8_t count()override{return 0;}
  bool clear()override{return true;}
};
static bool run(FailingStream&stream,History&history,int failAppend,bool failFinalize) {
  stream.failAppend=failAppend;stream.failFinalize=failFinalize;Bus bus;ActiveSessionDefinition active;auto ep=bus.attach(Role::RaceEngine),src=bus.attach(Role::Input),ctl=bus.attach(Role::RaceControl);bus.subscribe(ep,Type::GoScheduled);bus.subscribe(ep,Type::InputEvent);bus.subscribe(ep,Type::FinishSettled);bus.subscribe(src,Type::FinishSettlement);bus.subscribe(ctl,Type::CompetitionComplete);
  ProposedRaceSetup setup{};setup.activeLanes=1;setup.lapTarget=12;setup.mode=SessionMode::LapRace;setup.entries[0].mugId=904;setup.entries[0].startFinish={InputModule::simulatedDetectorIdentity(),1,InputRole::StartFinish};if(!active.commit(setup,1,1))return false;SessionDefinition d(setup,1,1);RaceEngineModule engine(bus,ep,active,&history,nullptr,&stream);engine.begin(d,1000000);Message go{};go.type=Type::GoScheduled;go.relevantTime=1000000;bus.publish(src,go);engine.tick();auto in=InputModule::simulatedDetectorIdentity();for(uint32_t i=1;i<=12;++i){Message p{};p.type=Type::InputEvent;p.eventId=i;p.input=in;p.relevantTime=1000000+Time(i)*100000;bus.publish(src,p);engine.tick();}Message settled{};settled.type=Type::FinishSettled;settled.relevantTime=2200000;bus.publish(src,settled);engine.tick();for(unsigned i=0;i<4;++i)engine.tick();const auto&result=engine.completedResult();const bool ok=engine.complete()&&result.entries[0].laps==12&&!result.entries[0].detailComplete&&history.appendCalls==0&&engine.persistenceFault();if(!ok)std::printf("debug append=%d finalize=%u complete=%u sealed=%u detail=%u calls=%u fault=%u records=%lu\\n",failAppend,failFinalize?1:0,engine.complete()?1:0,result.sealed?1:0,result.entries[0].detailComplete?1:0,history.appendCalls,engine.persistenceFault()?1:0,(unsigned long)stream.records.size());return ok;
}
int main(){FailingStream open;open.opened_=false;History h1;const bool openFail=run(open,h1,-1,false);FailingStream write;History h2;const bool writeFail=run(write,h2,4,false);FailingStream finalize;History h3;const bool finalizeFail=run(finalize,h3,-1,true);const bool pass=openFail&&writeFail&&finalizeFail;std::printf("WP2D active_detail_failure %s open=%u write=%u finalize=%u\n",pass?"PASS":"FAIL",openFail?1:0,writeFail?1:0,finalizeFail?1:0);return pass?0:1;}
