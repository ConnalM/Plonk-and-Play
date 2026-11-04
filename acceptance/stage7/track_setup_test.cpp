#include "pp/race_control.h"
#include "pp/track_configuration.h"
#include <cstring>
#include <iostream>

namespace {
bool require(bool value, const char* message) {
  if (!value) std::cerr << "FAIL: " << message << "\n";
  return value;
}

class MemoryStore final : public pp::Store {
public:
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= 4 || !present[slot] || size != sizeof(bytes[slot])) return false;
    std::memcpy(data, bytes[slot], size);
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (slot >= 4 || size != sizeof(bytes[slot])) return false;
    std::memcpy(bytes[slot], data, size);
    present[slot] = true;
    return true;
  }
private:
  bool present[4]{};
  uint8_t bytes[4][pp::TrackConfigurationMemory::RecordSize]{};
};

pp::ProposedRaceSetup twoLaneSetup() {
  pp::ProposedRaceSetup setup{};
  setup.activeLanes = 2;
  setup.lapTarget = 2;
  setup.mode = pp::SessionMode::LapRace;
  setup.entries[0].startFinish = {pp::InputModule::simulatedDetectorIdentity(0), 1, pp::InputRole::StartFinish};
  setup.entries[0].mugId = 91;
  setup.entries[1].startFinish = {pp::InputModule::simulatedDetectorIdentity(1), 2, pp::InputRole::StartFinish};
  setup.entries[1].mugId = 92;
  setup.startFinish = setup.entries[0].startFinish;
  setup.secondStartFinish = setup.entries[1].startFinish;
  return setup;
}

pp::Message setupRequest(uint32_t correlation, uint32_t revision,
                         pp::InputIdentity lane1, pp::InputIdentity lane2) {
  pp::Message request{};
  request.type = pp::Type::SetupRequest;
  request.correlation = correlation;
  request.clientContext = pp::ClientContext::RaceDirectorSmug;
  request.sessionMode = pp::SessionMode::LapRace;
  request.setupLapTarget = 2;
  request.setupFinishPolicy = uint8_t(pp::LapFinishBehaviour::Immediate);
  request.proposalRevision = revision;
  request.setupAssignmentMask = 0x03;
  request.setupLane1 = lane1;
  request.setupLane2 = lane2;
  return request;
}

bool receiveResult(pp::Bus& bus, pp::Bus::Endpoint presentation,
                   pp::RequestResult result, pp::RequestRejection rejection) {
  pp::Message response{};
  if (!bus.receive(presentation, response)) return false;
  return response.type == pp::Type::RequestResult && response.requestResult == result &&
         response.rejection == rejection;
}

bool submit(pp::Bus& bus, pp::Bus::Endpoint presentation, pp::RaceControlModule& control,
            pp::InputIdentity lane1, pp::InputIdentity lane2,
            pp::RequestRejection expected, uint32_t correlation = 1) {
  const pp::Message request = setupRequest(correlation, control.proposalRevision(), lane1, lane2);
  if (bus.publish(presentation, request) != pp::Delivery::Delivered) return false;
  control.tick(1000);
  return receiveResult(bus, presentation,
                       expected == pp::RequestRejection::None ? pp::RequestResult::Accepted : pp::RequestResult::Rejected,
                       expected);
}
}

int main() {
  using namespace pp;
  bool ok = true;
  SimulatedDetectorProvider provider;
  const InputIdentity detectorC = InputModule::simulatedDetectorIdentity(2);
  const InputIdentity detectorD = InputModule::simulatedDetectorIdentity(3);

  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engine = bus.attach(Role::RaceEngine);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr, &provider);
  ok &= require(bus.subscribe(controlEndpoint, Type::SetupRequest), "Race Control subscribes to Track Setup requests");
  ok &= require(bus.subscribe(controlEndpoint, Type::StartRequest), "Race Control subscribes to START requests");
  ok &= require(bus.subscribe(presentation, Type::RequestResult), "presentation receives setup results");
  ok &= require(bus.subscribe(engine, Type::GoScheduled), "start schedule has a Race Engine consumer");

  const ProposedRaceSetup initial = twoLaneSetup();
  control.setProposedRaceSetup(initial);
  control.setRequiredCapabilityAvailable(true);
  ok &= require(control.readiness() == StartReadiness::Ready, "healthy two-lane setup is startable");
  ok &= require(submit(bus, presentation, control, detectorC, detectorD, RequestRejection::None),
                "healthy detector reassignment is accepted");
  ok &= require(control.proposedRaceSetup()->entries[0].startFinish.input == detectorC &&
                control.proposedRaceSetup()->entries[1].startFinish.input == detectorD,
                "accepted assignments feed the authoritative proposal");
  ok &= require(requiredDetectorCapabilitiesHealthy(*control.proposedRaceSetup(), provider),
                "accepted assignments feed readiness evaluation");

  ok &= require(submit(bus, presentation, control, detectorC, detectorC,
                       RequestRejection::InvalidRaceSetup, 2),
                "duplicate detector assignment is rejected");
  provider.setAvailable(2, false);
  ok &= require(!requiredDetectorCapabilitiesHealthy(*control.proposedRaceSetup(), provider),
                "missing required detector blocks readiness");
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::RequiredCapabilityUnavailable, 3),
                "missing detector assignment is rejected");
  provider.setAvailable(2, true);
  provider.setFault(3, true);
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::RequiredCapabilityUnavailable, 4),
                "faulted detector assignment is rejected");
  provider.setFault(3, false);
  provider.markReplacement(2, InputIdentity(0x50500099u, 7));
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::RequiredCapabilityUnavailable, 5),
                "unreconciled replacement is rejected");
  provider.markReplacement(2, detectorC);
  ok &= require(requiredDetectorCapabilitiesHealthy(*control.proposedRaceSetup(), provider),
                "reconciled detector restores readiness");

  pp::Message start{};
  start.type = Type::StartRequest;
  start.correlation = 6;
  start.clientContext = ClientContext::RaceDirectorSmug;
  start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "START request reaches Race Control");
  control.tick(1000);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "healthy configured lanes permit START through the existing path");

  MemoryStore store;
  TrackConfigurationMemory remembered(store);
  TrackConfiguration saved{};
  saved.lane1 = detectorC;
  saved.lane2 = detectorD;
  ok &= require(remembered.save(saved), "Track Setup persists through the Store boundary");
  TrackConfiguration loaded{};
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered && loaded == saved,
                "Track Setup reconstructs its latest remembered assignments");

  std::cout << (ok ? "WP7.3 Track Setup contract PASS\n" : "WP7.3 Track Setup contract FAIL\n");
  return ok ? 0 : 1;
}
