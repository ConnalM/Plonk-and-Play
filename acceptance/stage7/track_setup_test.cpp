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
  bool failWrite = false;
  bool corruptNextRead = false;
  uint32_t writeCalls = 0;
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= 4 || !present[slot] || size != sizeof(bytes[slot])) return false;
    std::memcpy(data, bytes[slot], size);
    if (corruptNextRead) {
      data[0] ^= 0xff;
      corruptNextRead = false;
    }
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (failWrite || slot >= 4 || size != sizeof(bytes[slot])) return false;
    ++writeCalls;
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
                         pp::InputIdentity lane1, pp::InputIdentity lane2,
                         bool includeAssignments = true, uint32_t lapTarget = 2) {
  pp::Message request{};
  request.type = pp::Type::SetupRequest;
  request.correlation = correlation;
  request.clientContext = pp::ClientContext::RaceDirectorSmug;
  request.sessionMode = pp::SessionMode::LapRace;
  request.setupLapTarget = lapTarget;
  request.setupFinishPolicy = uint8_t(pp::LapFinishBehaviour::Immediate);
  request.proposalRevision = revision;
  if (includeAssignments) {
    request.setupAssignmentMask = 0x03;
    request.setupLane1 = lane1;
    request.setupLane2 = lane2;
  }
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
            pp::RequestRejection expected, uint32_t correlation = 1,
            bool includeAssignments = true, uint32_t lapTarget = 2) {
  const pp::Message request = setupRequest(correlation, control.proposalRevision(), lane1, lane2,
                                           includeAssignments, lapTarget);
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
  const InputIdentity detectorE = InputModule::simulatedDetectorIdentity(4);
  const InputIdentity detectorF = InputModule::simulatedDetectorIdentity(5);

  MemoryStore store;
  TrackConfigurationMemory remembered(store);

  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engine = bus.attach(Role::RaceEngine);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr, &provider, &remembered);
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
  TrackConfiguration loaded{};
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorC && loaded.lane2 == detectorD,
                "accepted assignment is persisted and verified before acceptance");

  const uint32_t revisionBeforeOrdinary = control.proposalRevision();
  const uint32_t writesBeforeOrdinary = store.writeCalls;
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::None, 2, false, 3),
                "ordinary race-setting change is accepted");
  ok &= require(control.proposalRevision() == revisionBeforeOrdinary + 1,
                "ordinary race-setting change advances the proposal revision");
  ok &= require(store.writeCalls == writesBeforeOrdinary,
                "ordinary race-setting change does not invoke Track Setup persistence");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorC && loaded.lane2 == detectorD,
                "ordinary race-setting change does not overwrite Track Setup");

  ok &= require(submit(bus, presentation, control, detectorE, detectorF,
                       RequestRejection::None, 3),
                "successive Track Setup assignment is accepted");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorE && loaded.lane2 == detectorF,
                "newest successive assignment is remembered");

  ok &= require(submit(bus, presentation, control, detectorC, detectorC,
                       RequestRejection::InvalidRaceSetup, 4),
                "duplicate detector assignment is rejected");
  provider.setAvailable(4, false);
  ok &= require(!requiredDetectorCapabilitiesHealthy(*control.proposedRaceSetup(), provider),
                "missing required detector blocks readiness");
  ok &= require(submit(bus, presentation, control, detectorE, detectorF,
                       RequestRejection::RequiredCapabilityUnavailable, 5),
                "missing detector assignment is rejected");
  provider.setAvailable(4, true);
  provider.setFault(5, true);
  ok &= require(submit(bus, presentation, control, detectorE, detectorF,
                       RequestRejection::RequiredCapabilityUnavailable, 6),
                "faulted detector assignment is rejected");
  provider.setFault(5, false);
  provider.markReplacement(4, InputIdentity(0x50500099u, 7));
  ok &= require(submit(bus, presentation, control, detectorE, detectorF,
                       RequestRejection::RequiredCapabilityUnavailable, 7),
                "unreconciled replacement is rejected");
  provider.markReplacement(4, detectorE);
  ok &= require(requiredDetectorCapabilitiesHealthy(*control.proposedRaceSetup(), provider),
                "reconciled detector restores readiness");

  const ProposedRaceSetup proposalBeforeWriteFailure = *control.proposedRaceSetup();
  const uint32_t revisionBeforeWriteFailure = control.proposalRevision();
  store.failWrite = true;
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::StorageUnavailable, 8),
                "failed Track Setup write is rejected");
  store.failWrite = false;
  ok &= require(control.proposalRevision() == revisionBeforeWriteFailure &&
                control.proposedRaceSetup()->entries[0].startFinish.input == proposalBeforeWriteFailure.entries[0].startFinish.input &&
                control.proposedRaceSetup()->entries[1].startFinish.input == proposalBeforeWriteFailure.entries[1].startFinish.input,
                "failed write preserves authoritative proposal and revision");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorE && loaded.lane2 == detectorF,
                "failed write preserves previous remembered configuration");

  const uint32_t revisionBeforeReadbackFailure = control.proposalRevision();
  store.corruptNextRead = true;
  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::StorageUnavailable, 9),
                "failed Track Setup read-back verification is rejected");
  ok &= require(control.proposalRevision() == revisionBeforeReadbackFailure &&
                control.proposedRaceSetup()->entries[0].startFinish.input == proposalBeforeWriteFailure.entries[0].startFinish.input &&
                control.proposedRaceSetup()->entries[1].startFinish.input == proposalBeforeWriteFailure.entries[1].startFinish.input,
                "failed read-back preserves authoritative proposal and revision");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorE && loaded.lane2 == detectorF,
                "failed read-back preserves previous remembered configuration");

  ok &= require(submit(bus, presentation, control, detectorC, detectorD,
                       RequestRejection::None, 10),
                "Track Setup recovers after a failed save");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorC && loaded.lane2 == detectorD,
                "recovered save becomes the newest remembered configuration");

  ok &= require(submit(bus, presentation, control, detectorE, detectorF,
                       RequestRejection::None, 11),
                "another successive assignment is accepted");
  ok &= require(remembered.load(loaded) == LoadStatus::Remembered &&
                loaded.lane1 == detectorE && loaded.lane2 == detectorF,
                "latest assignment remains after recovery");
  TrackConfigurationMemory restarted(store);
  TrackConfiguration reconstructed{};
  ok &= require(restarted.load(reconstructed) == LoadStatus::Remembered &&
                reconstructed.lane1 == detectorE && reconstructed.lane2 == detectorF,
                "restart reconstructs the newest valid installation configuration");

  pp::Message start{};
  start.type = Type::StartRequest;
  start.correlation = 6;
  start.clientContext = ClientContext::RaceDirectorSmug;
  start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "START request reaches Race Control");
  control.tick(1000);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "healthy configured lanes permit START through the existing path");

  std::cout << (ok ? "WP7.3 Track Setup contract PASS\n" : "WP7.3 Track Setup contract FAIL\n");
  return ok ? 0 : 1;
}
