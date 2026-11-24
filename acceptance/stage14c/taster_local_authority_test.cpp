// Host-only local adapter test. Engine-originated settlement/completion
// messages below are fixtures, not claims of detector or hardware acceptance.
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
#include "pp/local_taster_authority.h"

namespace pp {
static Time testClock = 1000000;
Time systemTime() { return testClock; }
}
using namespace pp;
namespace {
class TestStore final : public Store {
public:
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* out, size_t n) override {
    auto it = data.find(slot);
    if (it == data.end() || it->second.size() != n) return false;
    memcpy(out, it->second.data(), n);
    return true;
  }
  bool write(unsigned slot, const uint8_t* in, size_t n) override {
    data[slot] = std::vector<uint8_t>(in, in+n);
    return true;
  }
private:
  std::map<unsigned, std::vector<uint8_t>> data;
};
class BrowserOwner final : public StartAuthority {
public:
  bool connected = false;
  bool authorizesStart(uint64_t owner) const override { return connected && owner == 7; }
  bool hasController() const override { return connected; }
};
}
int main() {
  Bus bus;
  const auto controller = bus.attach(Role::RaceControl);
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto browserEndpoint = bus.attach(Role::Presentation);
  const auto tasterEndpoint = bus.attach(Role::Presentation);
  for (auto t : {Type::StartRequest, Type::SetupRequest, Type::SessionOperationRequest,
                 Type::PauseSettled, Type::CompetitionComplete})
    assert(bus.subscribe(controller,t));
  for (auto t : {Type::GoScheduled, Type::SessionOperation})
    assert(bus.subscribe(engineEndpoint,t));
  assert(bus.subscribe(tasterEndpoint,Type::RequestResult));
  TestStore store;
  Memory remembered(store);
  TasterPresentationPreferencesMemory prefsMemory(store);
  TasterPresentationPreferencesController prefs(prefsMemory);
  prefs.load();
  ActiveSessionDefinition active;
  RaceControlModule control(bus,controller,active,nullptr,nullptr,nullptr,nullptr,&remembered);
  RaceEngineModule engine(bus,engineEndpoint,active);
  Noticeboard notice(control,engine,active);
  BrowserOwner owner;
  control.setControllerStartAuthority(browserEndpoint,owner);
  control.setStandaloneTasterEndpoint(tasterEndpoint);
  LocalTasterAuthority local(control,notice,bus,tasterEndpoint,&prefs);
  PhysicalTasterAuthority& authority = local;
  ProposedRaceSetup setup{};
  setup.activeLanes=2;
  setup.mode=SessionMode::LapRace;
  setup.lapTarget=10;
  setup.durationMinutes=5;
  setup.finish=LapFinishBehaviour::CompleteFullRaceDistance;
  setup.entries[0].startFinish={InputIdentity(1,1),1,InputRole::StartFinish};
  setup.entries[1].startFinish={InputIdentity(2,1),2,InputRole::StartFinish};
  setup.entries[0].mugId=1;
  setup.entries[1].mugId=2;
  control.setProposedRaceSetup(setup);
  control.setRequiredCapabilityAvailable(true);
  assert(authority.snapshot().readiness==control.readiness());
  assert(authority.stateAt(testClock).proposalRevision==notice.currentAt(testClock).proposalRevision);
  TasterSurfaceModel model(authority,1,0x80000000U);
  model.sync();
  assert(model.selectedValue()==10);
  auto resolve=[&](uint32_t id, RequestResult expected, RequestRejection reason) {
    control.tick(testClock);
    // Consume engine messages without simulating detector physics.
    Message discarded;
    while(bus.receive(engineEndpoint,discarded)) {}
    authority.pollResults();
    RequestResult value=RequestResult::Rejected;
    RequestRejection rejection=RequestRejection::None;
    assert(authority.result(id,value,rejection));
    assert(value==expected && rejection==reason);
    model.service();
  };
  auto oldRevision=authority.snapshot().proposalRevision;
  assert(model.adjustPreview(1));
  assert(authority.snapshot().proposal.lapTarget==10);
  assert(model.commitAdjustment());
  resolve(0x80000000U,RequestResult::Accepted,RequestRejection::None);
  assert(authority.snapshot().proposal.lapTarget==11);
  assert(!model.setupPending());
  uint32_t id=model.allocateCorrelation();
  assert(id==0x80000001U);
  assert(authority.submitSetup(id,SessionMode::LapRace,12,5,LapFinishBehaviour::CompleteFullRaceDistance,oldRevision));
  resolve(id,RequestResult::Rejected,RequestRejection::ProposalRevisionConflict);
  owner.connected=true;
  id=model.allocateCorrelation();
  assert(authority.submitStart(id));
  resolve(id,RequestResult::Rejected,RequestRejection::PermissionDenied);
  owner.connected=false;
  auto p=*authority.committedPreferences();
  strcpy(p.lane1Label,"GREEN TEAM");
  p.lane1Graphic=TasterCarGraphic::RedEtype;
  assert(prefs.update(true,true,p)==TasterPresentationPreferencesResult::Accepted);
  assert(strcmp(authority.committedPreferences()->lane1Label,"GREEN TEAM")==0);
  assert(authority.committedPreferences()->lane1Graphic==TasterCarGraphic::RedEtype);
  assert(prefs.restoreFactory(true,true)==TasterPresentationPreferencesResult::Accepted);
  assert(strcmp(authority.committedPreferences()->lane1Label,"LANE 1")==0);
  assert(authority.snapshot().proposal.lapTarget==11);
  id=model.allocateCorrelation();
  assert(authority.submitStart(id));
  resolve(id,RequestResult::Accepted,RequestRejection::None);
  assert(active.current() && !active.current()->officialRecordEligible());
  assert(authority.lifecycle()==SessionLifecycle::Starting);
  testClock=control.scheduledGo();
  control.tick(testClock);
  assert(authority.stateAt(testClock).lifecycle==SessionLifecycle::Racing);
  auto operation=[&](SessionOperation op,bool confirmed,RequestResult result,RequestRejection rejection) {
    const auto correlation=model.allocateCorrelation();
    assert(authority.submitOperation(correlation,op,confirmed));
    resolve(correlation,result,rejection);
  };
  operation(SessionOperation::Pause,false,RequestResult::Accepted,RequestRejection::None);
  assert(authority.lifecycle()==SessionLifecycle::Paused);
  operation(SessionOperation::Resume,false,RequestResult::Rejected,RequestRejection::PauseSettlementPending);
  Message fact{};
  fact.type=Type::PauseSettled; fact.relevantTime=testClock;
  assert(bus.publish(engineEndpoint,fact)==Delivery::Delivered);
  control.tick(testClock);
  operation(SessionOperation::Resume,false,RequestResult::Accepted,RequestRejection::None);
  testClock=control.scheduledGo();
  control.tick(testClock);
  operation(SessionOperation::Pause,false,RequestResult::Accepted,RequestRejection::None);
  fact.relevantTime=testClock;
  assert(bus.publish(engineEndpoint,fact)==Delivery::Delivered);
  control.tick(testClock);
  operation(SessionOperation::EndRace,false,RequestResult::Rejected,RequestRejection::ConfirmationRequired);
  operation(SessionOperation::EndRace,true,RequestResult::Accepted,RequestRejection::None);
  assert(authority.lifecycle()==SessionLifecycle::Ready);
  control.setRequiredCapabilityAvailable(true);
  model.sync();
  assert(model.start());
  // Model used the next allocated identifier; verify via authoritative state.
  control.tick(testClock); authority.pollResults(); model.service();
  assert(authority.lifecycle()==SessionLifecycle::Starting);
  testClock=control.scheduledGo(); control.tick(testClock);
  fact.type=Type::CompetitionComplete; fact.relevantTime=testClock;
  assert(bus.publish(engineEndpoint,fact)==Delivery::Delivered);
  control.tick(testClock);
  operation(SessionOperation::RaceAgain,false,RequestResult::Accepted,RequestRejection::None);
  assert(authority.lifecycle()==SessionLifecycle::Ready);
  assert(authority.snapshot().proposal.lapTarget==11);
  std::cout << "S2 local Taster adapter host contract PASS\n";
}
