#include "pp/race_control.h"

#include <cassert>
#include <cstdio>

namespace {
using namespace pp;

ProposedRaceSetup validLapRace() {
  ProposedRaceSetup setup{};
  setup.mode = SessionMode::LapRace;
  setup.activeLanes = 1;
  setup.lapTarget = 5;
  setup.entries[0].startFinish =
      SessionInputRole{InputIdentity{50500001, 1}, 1, InputRole::StartFinish};
  setup.entries[0].mugId = 1;
  return setup;
}

struct StartFixture {
  struct Authority final : StartAuthority {
    bool controller = false;
    bool authorizesStart(uint64_t owner) const override { return controller && owner == 7; }
    bool hasController() const override { return controller; }
  } authority;
  Bus bus;
  ActiveSessionDefinition active;
  Bus::Endpoint controlEndpoint{bus.attach(Role::RaceControl)};
  Bus::Endpoint engineEndpoint{bus.attach(Role::RaceEngine)};
  Bus::Endpoint tasterEndpoint{bus.attach(Role::Presentation)};
  Bus::Endpoint browserEndpoint{bus.attach(Role::Presentation)};
  RaceControlModule control{bus, controlEndpoint, active};

  StartFixture() {
    assert(bus.subscribe(controlEndpoint, Type::StartRequest));
    assert(bus.subscribe(engineEndpoint, Type::GoScheduled));
    assert(bus.subscribe(tasterEndpoint, Type::RequestResult));
    assert(bus.subscribe(browserEndpoint, Type::RequestResult));
    control.setStandaloneTasterEndpoint(tasterEndpoint);
    control.setControllerStartAuthority(browserEndpoint, authority);
    control.setProposedRaceSetup(validLapRace());
    control.setRequiredCapabilityAvailable(true);
  }

  void start(Bus::Endpoint endpoint, ClientContext context, uint32_t correlation) {
    Message request{};
    request.type = Type::StartRequest;
    request.correlation = correlation;
    request.sessionMode = SessionMode::None;
    request.clientContext = context;
    request.controlOwner = endpoint.id == browserEndpoint.id ? 7 : 0;
    assert(bus.publish(endpoint, request) == Delivery::Delivered);
    control.tick(1'000'000);
  }

  Message response(Bus::Endpoint endpoint, uint32_t correlation) {
    Message message{};
    while (bus.receive(endpoint, message)) {
      if (message.type == Type::RequestResult && message.correlation == correlation) return message;
    }
    assert(false && "missing request result");
    return {};
  }
};

void standaloneStartFreezesIneligible() {
  StartFixture fixture;
  fixture.start(fixture.tasterEndpoint, ClientContext::StandaloneTaster, 1);
  assert(fixture.response(fixture.tasterEndpoint, 1).requestResult == RequestResult::Accepted);
  assert(fixture.active.current());
  assert(!fixture.active.current()->officialRecordEligible());

  // A later controller connection cannot reclassify the active definition.
  fixture.authority.controller = true;
  fixture.start(fixture.browserEndpoint, ClientContext::RaceDirectorSmug, 2);
  assert(fixture.response(fixture.browserEndpoint, 2).rejection == RequestRejection::LifecycleNotStartable);
  assert(!fixture.active.current()->officialRecordEligible());
}

void controllerStartRemainsEligible() {
  StartFixture fixture;
  fixture.authority.controller = true;
  fixture.start(fixture.browserEndpoint, ClientContext::RaceDirectorSmug, 3);
  assert(fixture.response(fixture.browserEndpoint, 3).requestResult == RequestResult::Accepted);
  assert(fixture.active.current()->officialRecordEligible());
}

void untrustedStandaloneLabelIsRejected() {
  StartFixture fixture;
  fixture.start(fixture.browserEndpoint, ClientContext::StandaloneTaster, 4);
  const Message result = fixture.response(fixture.browserEndpoint, 4);
  assert(result.requestResult == RequestResult::Rejected);
  assert(result.rejection == RequestRejection::PermissionDenied);
  assert(!fixture.active.current());
}
}  // namespace

int main() {
  standaloneStartFreezesIneligible();
  controllerStartRemainsEligible();
  untrustedStandaloneLabelIsRejected();
  std::puts("STANDALONE_TASTER_RECORD_ELIGIBILITY_PASS");
  return 0;
}
