// Archie review candidate: host-only acceptance matrix, NOT executed when authored.
// Compile with C++17, -Wall -Wextra -Werror and firmware/include on include path.
// This intentionally exercises the approved behaviour, not merely current behaviour.
// Run independently of the existing focused tests; no hardware/flash dependencies.
#include "pp/race_control.h"
#include "pp/race_engine.h"
#include "pp/control_lease.h"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
using namespace pp;
struct SeedHistory final : HistoryStore {
  unsigned writes=0;
  uint32_t sequence=19;
  uint8_t count() override { return uint8_t(1+writes); }
  bool append(const uint8_t*,size_t,uint32_t& s) override {
    s=++sequence; ++writes; return true;
  }
  bool appendDetailed(const void*,size_t,const DetailSource& d,uint32_t& s) override {
    assert(d.detailComplete());
    assert(d.detailEntryCount()==2);
    assert(d.detailRecordCount(0)>0 && d.detailRecordCount(1)>0);
    s=++sequence; ++writes; return true;
  }
  bool loadNewest(uint8_t,uint8_t*,size_t,size_t& n,uint32_t& s) override {
    n=0; s=0; return false;
  }
  bool clear() override { assert(false); return false; }
};
struct SeedRecords final : TrackRecordStore {
  unsigned writes=0;
  Time pb[2]{10'000'000,10'000'000};
  Time track=10'000'000;
  bool observe(uint8_t lane,Time t) override {
    assert(lane>=1 && lane<=2); ++writes;
    if(t<pb[lane-1]) pb[lane-1]=t;
    if(t<track) track=t;
    return true;
  }
  Time lanePb(uint8_t lane) override { return pb[lane-1]; }
  Time trackRecord() override { return track; }
  uint32_t era() override { return 1; }
  bool clearLane(uint8_t) override { assert(false); return false; }
  bool clearTrack() override { assert(false); return false; }
  bool clearAll() override { assert(false); return false; }
};
struct ProbeDetail final : ActiveDetailStore {
  unsigned begins=0,appends=0,drains=0,finalizes=0,aborts=0,recovers=0;
  uint8_t entryCount=0;
  std::vector<LapRecord> rows[2];
  bool begin(uint32_t,uint8_t count) override {
    assert(count==2); ++begins; entryCount=count;
    for(auto& r:rows) r.clear();
    return true;
  }
  bool append(uint8_t entry,const LapRecord& r) override {
    assert(entry<entryCount); ++appends; rows[entry].push_back(r); return true;
  }
  void drain() override { ++drains; }
  bool finalize() override { ++finalizes; return true; }
  void abort() override { ++aborts; entryCount=0; for(auto& r:rows) r.clear(); }
  uint32_t dropped() const override { return 0; }
  bool recover() override { ++recovers; return true; }
  uint8_t detailEntryCount() const override { return entryCount; }
  uint32_t detailRecordCount(uint8_t entry) const override {
    return entry<entryCount?static_cast<uint32_t>(rows[entry].size()):0;
  }
  bool readDetail(uint8_t entry,uint32_t i,LapRecord& r) const override {
    if(i>=detailRecordCount(entry)) return false;
    r=rows[entry][i]; return true;
  }
  bool detailComplete() const override { return true; }
  unsigned calls() const { return begins+appends+drains+finalizes+aborts+recovers; }
};
struct LeaseAdapter final : StartAuthority {
  ControlLease lease;
  bool authorizesStart(uint64_t owner) const override {
    return lease.isOwner(owner) && lease.ownerConnected();
  }
  bool hasController() const override { return lease.ownerConnected(); }
};
struct Fixture {
  Bus bus;
  ActiveSessionDefinition active;
  SeedHistory history;
  SeedRecords records;
  ProbeDetail detail;
  LeaseAdapter authority;
  Bus::Endpoint controlEp{bus.attach(Role::RaceControl)};
  Bus::Endpoint engineEp{bus.attach(Role::RaceEngine)};
  Bus::Endpoint inputEp{bus.attach(Role::Input)};
  Bus::Endpoint tasterEp{bus.attach(Role::Presentation)};
  Bus::Endpoint browserEp{bus.attach(Role::Presentation)};
  Bus::Endpoint unknownEp{bus.attach(Role::Presentation)};
  RaceControlModule control{bus,controlEp,active,&history,&records};
  RaceEngineModule engine{bus,engineEp,active,&history,&records,&detail};
  uint32_t eventId=1, corr=1;
  unsigned storedFacts=0, faultFacts=0;
  Time now=1'000'000;

  Fixture(SessionMode mode=SessionMode::LapRace,
          LapFinishBehaviour finish=LapFinishBehaviour::CompleteFullRaceDistance,
          uint32_t target=2,bool simulated=false) {
    for(auto t:{Type::StartRequest,Type::SessionOperationRequest,Type::SetupRequest,
                Type::CompetitionComplete,Type::PauseSettled})
      assert(bus.subscribe(controlEp,t));
    for(auto t:{Type::GoScheduled,Type::InputEvent,Type::EnduranceExpired,
                Type::FinishSettled,Type::SessionOperation,Type::InputSettlement})
      assert(bus.subscribe(engineEp,t));
    assert(bus.subscribe(inputEp,Type::FinishSettlement));
    for(auto t:{Type::RequestResult,Type::HistoryStored,Type::StorageFault})
      assert(bus.subscribe(browserEp,t));
    control.setStandaloneTasterEndpoint(tasterEp);
    control.setControllerStartAuthority(browserEp,authority);
    ProposedRaceSetup setup{};
    setup.mode=mode; setup.activeLanes=2; setup.finish=finish;
    setup.lapTarget=mode==SessionMode::LapRace?target:0;
    setup.durationMinutes=1;
    setup.simulatedSession=simulated;
    for(uint8_t i=0;i<2;++i) {
      setup.entries[i].startFinish={InputIdentity{50500001,uint16_t(i+1)},
                                       uint8_t(i+1),InputRole::StartFinish};
      setup.entries[i].mugId=i+1;
    }
    control.setProposedRaceSetup(setup);
    control.setRequiredCapabilityAvailable(true);
  }
  ~Fixture() { engine.resetForFixture(); }
  uint32_t queueStart(Bus::Endpoint e,ClientContext context,uint64_t owner=0) {
    Message m{}; m.type=Type::StartRequest; m.correlation=corr++;
    m.clientContext=context; m.controlOwner=owner; m.sessionMode=SessionMode::None;
    assert(bus.publish(e,m)==Delivery::Delivered); return m.correlation;
  }
  uint32_t queueOperation(Bus::Endpoint e,SessionOperation op) {
    Message m{}; m.type=Type::SessionOperationRequest; m.correlation=corr++;
    m.clientContext=ClientContext::StandaloneTaster; m.operation=op; m.probe=1;
    assert(bus.publish(e,m)==Delivery::Delivered); return m.correlation;
  }
  void pump(Time at) { now=at; control.tick(now); engine.tick(); }
  Message response(uint32_t id) {
    Message m{};
    while(bus.receive(browserEp,m)){
      if(m.type==Type::HistoryStored) ++storedFacts;
      if(m.type==Type::StorageFault) ++faultFacts;
      if(m.type==Type::RequestResult&&m.correlation==id) return m;
    }
    assert(false && "missing request result"); return {};
  }
  void drainFacts() {
    Message m{}; while(bus.receive(browserEp,m)){
      if(m.type==Type::HistoryStored) ++storedFacts;
      if(m.type==Type::StorageFault) ++faultFacts;
    }
  }
  void start(bool official) {
    if(official) {
      assert(authority.lease.restore(7)); authority.lease.connected(7);
    }
    const auto id=queueStart(official?browserEp:tasterEp,
       official?ClientContext::RaceDirectorSmug:ClientContext::StandaloneTaster,
       official?7:0);
    pump(now);
    assert(response(id).requestResult==RequestResult::Accepted);
    assert(active.current() && active.current()->officialRecordEligible()==official);
  }
  Time go() { auto at=control.scheduledGo(); assert(at>=now); pump(at);
    assert(control.state()==SessionLifecycle::Racing); return at; }
  void lap(uint8_t lane,Time at) {
    assert(at>=now);
    control.tick(at); now=at;
    Message m{};m.type=Type::InputEvent;m.eventId=eventId++;
    m.relevantTime=at;m.input={50500001,lane};
    assert(bus.publish(inputEp,m)==Delivery::Delivered); engine.tick();
    drainFacts();
  }
  void settle() {
    Message m{}; bool found=false;
    while(bus.receive(inputEp,m)) {
      if(m.type!=Type::FinishSettlement) continue;
      Message s{};s.type=Type::FinishSettled;s.relevantTime=m.relevantTime;
      assert(bus.publish(inputEp,s)==Delivery::Delivered); found=true;
    }
    assert(found); engine.tick(); control.tick(now); drainFacts();
    assert(control.state()==SessionLifecycle::Finished);
    assert(engine.completedResult().sealed && engine.completedResult().valid);
  }
  void check(bool official,unsigned expectedLaps=4) {
    assert(!engine.persistencePending()&&!engine.persistenceFault()&&!faultFacts);
    if(official) {
      assert(history.writes==1&&history.sequence==20&&history.count()==2);
      assert(records.writes==expectedLaps&&records.pb[0]<10'000'000&&records.pb[1]<10'000'000);
      assert(records.track<10'000'000);
      assert(detail.begins==1&&detail.appends==expectedLaps&&detail.finalizes==1);
      assert(engine.historySequence()==20&&storedFacts==1);
    } else {
      assert(history.writes==0&&history.sequence==19&&history.count()==1);
      assert(records.writes==0&&records.pb[0]==10'000'000&&records.pb[1]==10'000'000);
      assert(records.track==10'000'000&&detail.calls()==0);
      assert(engine.historySequence()==0&&storedFacts==0);
      assert(!engine.recoverPersistence()&&detail.calls()==0);
    }
  }
};

void lapRace(bool official,bool simulated=false) {
  Fixture f(SessionMode::LapRace,LapFinishBehaviour::CompleteFullRaceDistance,2,simulated);
  f.start(official);
  const auto session=f.active.current()->sessionId();
  if(official) f.authority.lease.disconnected(7);
  else { assert(f.authority.lease.restore(7)); f.authority.lease.connected(7); }
  const auto go=f.go();
  f.lap(1,go+1'000'000); f.lap(2,go+1'100'000);
  f.lap(1,go+2'000'000); f.lap(2,go+2'100'000);
  f.settle();
  assert(f.active.current()->sessionId()==session);
  assert(f.active.current()->officialRecordEligible()==official);
  assert(f.active.current()->simulatedSession()==simulated);
  f.check(official&&!simulated);
}
void endurance(bool official,LapFinishBehaviour policy) {
  Fixture f(SessionMode::Endurance,policy);
  f.start(official);
  const auto go=f.go();
  f.lap(1,go+1'000'000); f.lap(2,go+1'100'000);
  if(official) f.authority.lease.disconnected(7);
  else { assert(f.authority.lease.restore(7)); f.authority.lease.connected(7); }
  const auto end=f.control.durationExpiryAt();
  assert(end==go+60'000'000);
  f.pump(end);
  if(policy==LapFinishBehaviour::CompleteCurrentLap) {
    f.lap(1,end+1'000'000); f.lap(2,end+1'100'000);
  }
  f.settle();
  assert(f.active.current()->officialRecordEligible()==official);
  f.check(official,policy==LapFinishBehaviour::CompleteCurrentLap?4:2);
}
void leaseOrdering() {
  {
    Fixture f;
    assert(f.authority.lease.restore(7));
    auto id=f.queueStart(f.tasterEp,ClientContext::StandaloneTaster);
    f.authority.lease.connected(7); // connect before START acceptance
    f.pump(f.now);
    assert(f.response(id).rejection==RequestRejection::PermissionDenied);
    assert(!f.active.current());
    f.authority.lease.disconnected(7);
    id=f.queueStart(f.tasterEp,ClientContext::StandaloneTaster);
    f.pump(f.now);
    assert(f.response(id).requestResult==RequestResult::Accepted);
    assert(!f.active.current()->officialRecordEligible());
  }
  {
    Fixture f; assert(f.authority.lease.restore(7)); f.authority.lease.connected(7);
    auto id=f.queueStart(f.unknownEp,ClientContext::RaceDirectorSmug,7);
    f.pump(f.now); assert(f.response(id).rejection==RequestRejection::PermissionDenied);
    id=f.queueStart(f.browserEp,ClientContext::RaceDirectorSmug,8);
    f.pump(f.now); assert(f.response(id).rejection==RequestRejection::PermissionDenied);
    id=f.queueStart(f.browserEp,ClientContext::RaceDirectorSmug,7);
    uint32_t takeover=0;
    assert(f.authority.lease.request(8,takeover)==ControlLeaseResult::Pending);
    assert(f.authority.lease.respond(7,true,takeover)==ControlLeaseResult::Accepted);
    f.authority.lease.connected(8);
    f.pump(f.now); assert(f.response(id).rejection==RequestRejection::PermissionDenied);
    id=f.queueStart(f.browserEp,ClientContext::RaceDirectorSmug,8);
    f.pump(f.now); assert(f.response(id).requestResult==RequestResult::Accepted);
    assert(f.active.current()->officialRecordEligible());
  }
}
void disconnectedBrowserMustNotStart() {
  Fixture f; assert(f.authority.lease.restore(7)); f.authority.lease.connected(7);
  const auto id=f.queueStart(f.browserEp,ClientContext::RaceDirectorSmug,7);
  f.authority.lease.disconnected(7);
  f.pump(f.now);
  // Requirement: disconnected SMUG cannot win a queued START over Taster.
  // Expected failure with 411d659 owner-only authorizesStart(). Update this
  // adapter alongside the production policy; test the real Browser separately.
  assert(f.response(id).rejection==RequestRejection::PermissionDenied);
  assert(!f.active.current());
}
void subordinatePauseMustNotExecute() {
  Fixture f; f.start(false); const auto go=f.go();
  assert(f.authority.lease.restore(7)); f.authority.lease.connected(7);
  auto id=f.queueOperation(f.tasterEp,SessionOperation::Pause);
  f.pump(go+1);
  assert(f.response(id).rejection==RequestRejection::PermissionDenied);
  assert(f.control.state()==SessionLifecycle::Racing);
}
}
int main() {
  lapRace(false); lapRace(true); lapRace(true,true);
  for(bool official:{false,true}) {
    endurance(official,LapFinishBehaviour::Immediate);
    endurance(official,LapFinishBehaviour::CompleteCurrentLap);
  }
  leaseOrdering();
  std::puts("ARCHIE_RECORD_MATRIX_CORE_PASS");
  disconnectedBrowserMustNotStart();
  subordinatePauseMustNotExecute();
  std::puts("ARCHIE_RECORD_MATRIX_PASS");
}
