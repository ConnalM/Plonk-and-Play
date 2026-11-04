#include "pp/race_control.h"
#include <iostream>

namespace {
bool require(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL: " << message << "\n";
  return condition;
}

pp::ProposedRaceSetup twoLaneSetup() {
  pp::ProposedRaceSetup setup{};
  setup.activeLanes = 2;
  setup.lapTarget = 2;
  setup.mode = pp::SessionMode::LapRace;
  setup.entries[0].startFinish = {pp::InputModule::simulatedDetectorIdentity(0), 1, pp::InputRole::StartFinish};
  setup.entries[0].mugId = 91;
  setup.entries[1].startFinish = {pp::InputModule::simulatedDetectorIdentity(1), 2, pp::InputRole::StartFinish};
  setup.entries[1].mugId = 92;
  return setup;
}

bool raceControlSees(const pp::ProposedRaceSetup& setup,
                     pp::DetectorProvider& provider,
                     pp::StartReadiness expected) {
  pp::Bus bus;
  const auto controlEndpoint = bus.attach(pp::Role::RaceControl);
  pp::ActiveSessionDefinition active;
  pp::RaceControlModule control(bus, controlEndpoint, active);
  control.setProposedRaceSetup(setup);
  control.setRequiredCapabilityAvailable(pp::requiredDetectorCapabilitiesHealthy(setup, provider));
  return control.readiness() == expected;
}

bool startRequestRejectedForCapability(pp::SimulatedDetectorProvider& provider,
                                       const pp::ProposedRaceSetup& setup) {
  pp::Bus bus;
  const auto presentation = bus.attach(pp::Role::Presentation);
  const auto controlEndpoint = bus.attach(pp::Role::RaceControl);
  const auto engine = bus.attach(pp::Role::RaceEngine);
  pp::ActiveSessionDefinition active;
  pp::RaceControlModule control(bus, controlEndpoint, active);
  if (!bus.subscribe(controlEndpoint, pp::Type::StartRequest) ||
      !bus.subscribe(presentation, pp::Type::RequestResult) ||
      !bus.subscribe(engine, pp::Type::GoScheduled)) return false;
  control.setProposedRaceSetup(setup);
  control.setRequiredCapabilityAvailable(pp::requiredDetectorCapabilitiesHealthy(setup, provider));

  pp::Message request{};
  request.type = pp::Type::StartRequest;
  request.correlation = 1;
  request.clientContext = pp::ClientContext::RaceDirectorSmug;
  request.sessionMode = pp::SessionMode::None;
  if (bus.publish(presentation, request) != pp::Delivery::Delivered) return false;
  control.tick(1000);
  pp::Message response{};
  if (!bus.receive(presentation, response)) return false;
  return response.requestResult == pp::RequestResult::Rejected &&
         response.rejection == pp::RequestRejection::RequiredCapabilityUnavailable;
}
}

int main() {
  using namespace pp;
  bool ok = true;
  SimulatedDetectorProvider provider;
  const ProposedRaceSetup twoLane = twoLaneSetup();

  ok &= require(requiredDetectorCapabilitiesHealthy(twoLane, provider),
                "both required detectors healthy makes Race Control ready");
  ok &= require(raceControlSees(twoLane, provider, StartReadiness::Ready),
                "Race Control readiness is READY for healthy required detectors");

  provider.setAvailable(0, false);
  ok &= require(!requiredDetectorCapabilitiesHealthy(twoLane, provider),
                "missing required detector blocks readiness");
  ok &= require(raceControlSees(twoLane, provider, StartReadiness::CapabilityUnavailable),
                "missing required detector exposes the authoritative capability reason");
  ok &= require(startRequestRejectedForCapability(provider, twoLane),
                "START is rejected with RequiredCapabilityUnavailable");

  provider.setAvailable(0, true);
  provider.setFault(0, true);
  ok &= require(!requiredDetectorCapabilitiesHealthy(twoLane, provider),
                "faulted required detector blocks readiness");

  provider.setFault(0, false);
  const InputIdentity replacement{0x50500099u, 7};
  provider.markReplacement(1, replacement);
  ok &= require(!requiredDetectorCapabilitiesHealthy(twoLane, provider),
                "unreconciled replacement blocks readiness without lane reassignment");
  ok &= require(provider.status(1).identity == InputModule::simulatedDetectorBIdentity(),
                "replacement does not change the configured lane identity");

  provider.markReplacement(1, InputModule::simulatedDetectorBIdentity());
  ok &= require(requiredDetectorCapabilitiesHealthy(twoLane, provider),
                "explicit replacement reconciliation restores readiness");

  ProposedRaceSetup oneLane = twoLane;
  oneLane.activeLanes = 1;
  provider.setAvailable(1, false);
  ok &= require(requiredDetectorCapabilitiesHealthy(oneLane, provider),
                "unavailable optional/unassigned detector does not block a one-lane session");

  provider.setAvailable(1, true);
  ActiveSessionDefinition active;
  ok &= require(active.commit(twoLane, 1, 1), "two-lane setup commits to Session Definition");
  SessionDefinition definition(twoLane, 1, 1);
  ok &= require(requiredDetectorCapabilitiesHealthy(definition, provider),
                "active Session Definition uses the same required detector assignments");

  std::cout << (ok ? "WP7.2 detector readiness contract PASS\n"
                   : "WP7.2 detector readiness contract FAIL\n");
  return ok ? 0 : 1;
}
