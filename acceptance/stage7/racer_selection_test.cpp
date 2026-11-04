#include "pp/race_control.h"
#include "pp/race_engine.h"
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
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= 2 || !present[slot] || size != sizeof(bytes[slot])) return false;
    std::memcpy(data, bytes[slot], size);
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (failWrite) return false;
    if (slot >= 2 || size != sizeof(bytes[slot])) return false;
    std::memcpy(bytes[slot], data, size); present[slot] = true; return true;
  }
private:
  bool present[2]{};
  uint8_t bytes[2][pp::Memory::RecordSize]{};
};

pp::ProposedRaceSetup initialSetup() {
  pp::ProposedRaceSetup setup{};
  setup.activeLanes = 2; setup.lapTarget = 2; setup.mode = pp::SessionMode::LapRace;
  setup.entries[0].startFinish = {pp::InputModule::simulatedDetectorIdentity(0), 1, pp::InputRole::StartFinish};
  setup.entries[0].mugId = 1;
  setup.entries[1].startFinish = {pp::InputModule::simulatedDetectorIdentity(1), 2, pp::InputRole::StartFinish};
  setup.entries[1].mugId = 2;
  setup.startFinish = setup.entries[0].startFinish; setup.secondStartFinish = setup.entries[1].startFinish;
  setup.selectedMugId = 1; setup.secondMugId = 2;
  return setup;
}

pp::Message setupRequest(uint32_t correlation, uint32_t revision,
                         uint32_t lane1MugId, uint32_t lane2MugId,
                         uint8_t racerMask = 3, uint32_t lapTarget = 2) {
  pp::Message request{}; request.type = pp::Type::SetupRequest;
  request.correlation = correlation; request.clientContext = pp::ClientContext::RaceDirectorSmug;
  request.sessionMode = pp::SessionMode::LapRace; request.setupLapTarget = lapTarget;
  request.setupFinishPolicy = uint8_t(pp::LapFinishBehaviour::Immediate);
  request.proposalRevision = revision; request.setupRacerAssignmentMask = racerMask;
  request.setupLane1MugId = lane1MugId; request.setupLane2MugId = lane2MugId;
  return request;
}

bool receiveResult(pp::Bus& bus, pp::Bus::Endpoint presentation,
                   pp::RequestResult result, pp::RequestRejection rejection) {
  pp::Message response{};
  if (!bus.receive(presentation, response)) return false;
  return response.type == pp::Type::RequestResult && response.requestResult == result &&
         response.rejection == rejection;
}
}

int main() {
  using namespace pp;
  bool ok = true;
  MemoryStore store; Memory memory(store); Configuration defaults{};
  ok &= require(memory.load(defaults) == LoadStatus::DefaultsMissing &&
                defaults.lane1MugId == 1 && defaults.lane2MugId == 2,
                "default racer identities are available through Memory");

  SimulatedDetectorProvider provider; Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto inputEndpoint = bus.attach(Role::Input);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr, &provider, nullptr, &memory);
  RaceEngineModule engine(bus, engineEndpoint, active);
  ok &= require(bus.subscribe(controlEndpoint, Type::SetupRequest), "Race Control subscribes to setup");
  ok &= require(bus.subscribe(controlEndpoint, Type::StartRequest), "Race Control subscribes to start");
  ok &= require(bus.subscribe(presentation, Type::RequestResult), "presentation receives results");
  ok &= require(bus.subscribe(engineEndpoint, Type::GoScheduled) &&
                bus.subscribe(engineEndpoint, Type::InputEvent),
                "normal Race Engine path has consumers");
  control.setProposedRaceSetup(initialSetup()); control.setRequiredCapabilityAvailable(true);

  const uint32_t initialRevision = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(1, initialRevision, 3, 4)) == Delivery::Delivered,
                "racer assignment reaches Race Control");
  control.tick(1000);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "distinct existing racer identities are accepted");
  ok &= require(control.proposedRaceSetup()->entries[0].mugId == 3 &&
                control.proposedRaceSetup()->entries[1].mugId == 4,
                "accepted racers feed the authoritative proposal");
  Configuration remembered{};
  ok &= require(memory.load(remembered) == LoadStatus::Remembered &&
                remembered.lane1MugId == 3 && remembered.lane2MugId == 4,
                "accepted racer selection is persisted and verified");

  const uint32_t revisionAfterRacers = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(2, revisionAfterRacers, 0, 0, 0, 5)) == Delivery::Delivered,
                "ordinary race-setting update reaches Race Control");
  control.tick(1001);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "ordinary race-setting update remains accepted");
  ok &= require(control.proposedRaceSetup()->entries[0].mugId == 3 &&
                control.proposedRaceSetup()->entries[1].mugId == 4,
                "ordinary race settings preserve selected racers");
  Configuration afterOrdinary{}; memory.load(afterOrdinary);
  ok &= require(afterOrdinary.lane1MugId == 3 && afterOrdinary.lane2MugId == 4,
                "ordinary race settings do not overwrite remembered racers");

  const uint32_t revisionBeforeDuplicate = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(3, revisionBeforeDuplicate, 5, 5)) == Delivery::Delivered,
                "duplicate racer request reaches Race Control");
  control.tick(1002);
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup),
                "duplicate racer identities are rejected");
  ok &= require(control.proposalRevision() == revisionBeforeDuplicate &&
                control.proposedRaceSetup()->entries[0].mugId == 3 &&
                control.proposedRaceSetup()->entries[1].mugId == 4,
                "rejected racer assignment preserves proposal and revision");

  const uint32_t revisionBeforeInvalid = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(4, revisionBeforeInvalid, 0, 6)) == Delivery::Delivered,
                "invalid racer request reaches Race Control");
  control.tick(1003);
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup) &&
                control.proposalRevision() == revisionBeforeInvalid,
                "zero racer identity is rejected without changing the proposal");

  const uint32_t revisionBeforeStorageFailure = control.proposalRevision();
  store.failWrite = true;
  ok &= require(bus.publish(presentation, setupRequest(5, revisionBeforeStorageFailure, 5, 6)) == Delivery::Delivered,
                "racer persistence failure request reaches Race Control");
  control.tick(1004);
  store.failWrite = false;
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected, RequestRejection::StorageUnavailable) &&
                control.proposalRevision() == revisionBeforeStorageFailure &&
                control.proposedRaceSetup()->entries[0].mugId == 3 &&
                control.proposedRaceSetup()->entries[1].mugId == 4,
                "racer persistence failure preserves authoritative selection");

  Message start{}; start.type = Type::StartRequest; start.correlation = 4;
  start.clientContext = ClientContext::RaceDirectorSmug; start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "START reaches Race Control");
  control.tick(1003);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "selected racers permit the normal START path");
  ok &= require(control.definition() && control.definition()->entry(0).mugId == 3 &&
                control.definition()->entry(1).mugId == 4,
                "Session Definition freezes selected racer identities");

  engine.tick();
  const Time go = control.scheduledGo();
  Message lane1Passage{}; lane1Passage.type = Type::InputEvent; lane1Passage.eventId = 1;
  lane1Passage.relevantTime = go + 1000000;
  lane1Passage.input = InputModule::simulatedDetectorIdentity(0);
  Message lane2Passage{}; lane2Passage.type = Type::InputEvent; lane2Passage.eventId = 2;
  lane2Passage.relevantTime = go + 2000000;
  lane2Passage.input = InputModule::simulatedDetectorIdentity(1);
  ok &= require(bus.publish(inputEndpoint, lane1Passage) == Delivery::Delivered &&
                bus.publish(inputEndpoint, lane2Passage) == Delivery::Delivered,
                "keyboard/simulated detector passages enter the normal Race Engine path");
  engine.tick();
  ok &= require(engine.entryState(0).mugId == 3 && engine.entryState(1).mugId == 4 &&
                engine.entryState(0).laps == 1 && engine.entryState(1).laps == 1,
                "normal racing preserves selected racer identities for both lanes");

  Memory reconstructedMemory(store); Configuration reconstructed{};
  ok &= require(reconstructedMemory.load(reconstructed) == LoadStatus::Remembered &&
                reconstructed.lane1MugId == 3 && reconstructed.lane2MugId == 4,
                "restart reconstruction restores newest racer selection");

  std::cout << (ok ? "WP7.3 Racer selection contract PASS\n" : "WP7.3 Racer selection contract FAIL\n");
  return ok ? 0 : 1;
}
