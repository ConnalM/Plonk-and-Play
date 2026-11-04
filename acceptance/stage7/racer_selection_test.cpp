#include "pp/race_control.h"
#include "pp/race_engine.h"
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
  bool failStagingWrite = false;
  bool failStagingRead = false;
  bool failPublication = false;
  bool failCleanup = false;
  uint32_t writeCalls = 0;
  uint32_t reclaimCalls = 0;
  bool available() const override { return true; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (slot >= SlotCount || !present[slot] || size != sizeof(bytes[slot])) return false;
    std::memcpy(data, bytes[slot], size);
    if (slot == pp::Memory::StagingSlot && failStagingRead) {
      data[0] ^= 0xff;
      failStagingRead = false;
    }
    return true;
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (slot == pp::Memory::StagingSlot && failStagingWrite) return false;
    if (slot >= SlotCount || size != sizeof(bytes[slot])) return false;
    ++writeCalls;
    std::memcpy(bytes[slot], data, size); present[slot] = true; return true;
  }
  bool writeMetadata(unsigned slot, const uint8_t* data, size_t size) override {
    if (failPublication) return false;
    return write(slot, data, size);
  }
  bool reclaim(unsigned slot) override {
    ++reclaimCalls;
    if (failCleanup) return false;
    if (slot < SlotCount) present[slot] = false;
    return true;
  }
  void seed(unsigned slot, const uint8_t* data, size_t size) {
    if (slot >= SlotCount || size != sizeof(bytes[slot])) return;
    std::memcpy(bytes[slot], data, size); present[slot] = true;
  }
  void seedOldRecord(uint32_t generation, uint16_t lanes, uint32_t laps,
                    uint32_t optionalFeatures, bool sound, bool power, uint8_t mode) {
    uint8_t record[pp::Memory::RecordSize]{};
    put(record, 0x31505043u);
    put(record + 4, generation);
    put(record + 8, lanes);
    put(record + 12, laps);
    put(record + 16, optionalFeatures);
    record[20] = sound ? 1 : 0;
    record[21] = power ? 1 : 0;
    record[22] = mode;
    put(record + 28, checksum(record, 28));
    seed(0, record, sizeof(record));
  }
private:
  static constexpr unsigned SlotCount = 6;
  bool present[SlotCount]{};
  uint8_t bytes[SlotCount][pp::Memory::RecordSize]{};
  static void put(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(value >> (8 * i));
  }
  static uint32_t checksum(const uint8_t* p, size_t size) {
    uint32_t value = 2166136261u;
    while (size--) value = (value ^ *p++) * 16777619u;
    return value;
  }
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

pp::Message combinedSetupRequest(uint32_t correlation, uint32_t revision,
                                 pp::InputIdentity lane1, pp::InputIdentity lane2,
                                 uint32_t lane1MugId, uint32_t lane2MugId) {
  pp::Message request = setupRequest(correlation, revision, lane1MugId, lane2MugId);
  request.setupAssignmentMask = 0x03;
  request.setupLane1 = lane1;
  request.setupLane2 = lane2;
  return request;
}

bool restartHasRacers(MemoryStore& store, uint32_t lane1, uint32_t lane2) {
  pp::Memory restarted(store);
  pp::Configuration configuration{};
  return restarted.load(configuration) == pp::LoadStatus::Remembered &&
         configuration.lane1MugId == lane1 && configuration.lane2MugId == lane2;
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
  ok &= require(memory.save(defaults), "default configuration can be published before combined-request checks");

  TrackConfigurationMemory trackMemory(store);
  TrackConfiguration rememberedTrack{};
  rememberedTrack.laneCount = 2;
  rememberedTrack.lane1 = InputModule::simulatedDetectorIdentity(0);
  rememberedTrack.lane2 = InputModule::simulatedDetectorIdentity(1);
  ok &= require(trackMemory.save(rememberedTrack), "baseline Track Setup can be remembered");

  SimulatedDetectorProvider provider; Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto inputEndpoint = bus.attach(Role::Input);
  ActiveSessionDefinition active;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr, &provider, &trackMemory, &memory);
  RaceEngineModule engine(bus, engineEndpoint, active);
  ok &= require(bus.subscribe(controlEndpoint, Type::SetupRequest), "Race Control subscribes to setup");
  ok &= require(bus.subscribe(controlEndpoint, Type::StartRequest), "Race Control subscribes to start");
  ok &= require(bus.subscribe(presentation, Type::RequestResult), "presentation receives results");
  ok &= require(bus.subscribe(engineEndpoint, Type::GoScheduled) &&
                bus.subscribe(engineEndpoint, Type::InputEvent),
                "normal Race Engine path has consumers");
  control.setProposedRaceSetup(initialSetup()); control.setRequiredCapabilityAvailable(true);

  const uint32_t combinedRevision = control.proposalRevision();
  const uint32_t combinedWrites = store.writeCalls;
  const uint32_t combinedTrackGeneration = trackMemory.generation();
  const auto combinedBefore = *control.proposedRaceSetup();
  const auto combined = combinedSetupRequest(
      1, combinedRevision, InputModule::simulatedDetectorIdentity(2),
      InputModule::simulatedDetectorIdentity(3), 5, 6);
  ok &= require(bus.publish(presentation, combined) == Delivery::Delivered,
                "combined detector+racer request reaches Race Control");
  control.tick(1000);
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected,
                              RequestRejection::InvalidRaceSetup),
                "combined detector+racer request is rejected before persistence");
  ok &= require(store.writeCalls == combinedWrites &&
                trackMemory.generation() == combinedTrackGeneration &&
                control.proposalRevision() == combinedRevision &&
                control.proposedRaceSetup()->entries[0].mugId == combinedBefore.entries[0].mugId &&
                control.proposedRaceSetup()->entries[1].mugId == combinedBefore.entries[1].mugId,
                "combined rejection preserves proposal, revision and both persistence namespaces");
  TrackConfiguration reconstructedTrack{};
  TrackConfigurationMemory restartedTrack(store);
  ok &= require(restartedTrack.load(reconstructedTrack) == LoadStatus::Remembered &&
                reconstructedTrack.lane1 == rememberedTrack.lane1 &&
                reconstructedTrack.lane2 == rememberedTrack.lane2 &&
                restartHasRacers(store, 1, 2),
                "combined rejection preserves both remembered configurations after restart");

  const uint32_t initialRevision = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(2, initialRevision, 3, 4)) == Delivery::Delivered,
                "racer assignment reaches Race Control");
  control.tick(1001);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "distinct existing racer identities are accepted");
  ok &= require(control.proposedRaceSetup()->entries[0].mugId == 3 &&
                control.proposedRaceSetup()->entries[1].mugId == 4,
                "accepted racers feed the authoritative proposal");
  Configuration remembered{};
  ok &= require(memory.load(remembered) == LoadStatus::Remembered &&
                remembered.lane1MugId == 3 && remembered.lane2MugId == 4,
                "accepted racer selection is persisted and verified");

  auto rejectedRacerChange = [&](uint32_t correlation, const char* message) {
    const uint32_t revision = control.proposalRevision();
    const auto before = *control.proposedRaceSetup();
    const bool delivered = bus.publish(presentation, setupRequest(correlation, revision, 5, 6)) == Delivery::Delivered;
    control.tick(1100 + correlation);
    const bool rejected = receiveResult(bus, presentation, RequestResult::Rejected,
                                        RequestRejection::StorageUnavailable);
    const bool proposalPreserved = control.proposalRevision() == revision &&
      control.proposedRaceSetup()->entries[0].mugId == before.entries[0].mugId &&
      control.proposedRaceSetup()->entries[1].mugId == before.entries[1].mugId;
    const bool restartPreserved = restartHasRacers(store, 3, 4);
    return require(delivered && rejected && proposalPreserved && restartPreserved, message);
  };

  store.failStagingWrite = true;
  ok &= rejectedRacerChange(3, "failed racer staging write rejects and preserves restart state");
  store.failStagingWrite = false;

  store.failStagingRead = true;
  ok &= rejectedRacerChange(4, "failed racer staging read-back rejects and preserves restart state");

  store.failStagingRead = true;
  store.failCleanup = true;
  ok &= rejectedRacerChange(5, "failed racer staging cleanup cannot make a rejected change authoritative");
  store.failCleanup = false;

  store.failPublication = true;
  ok &= rejectedRacerChange(6, "failed racer publication rejects and preserves restart state");
  store.failPublication = false;

  store.failPublication = true;
  store.failCleanup = true;
  ok &= rejectedRacerChange(7, "failed publication cleanup cannot make a rejected change authoritative");
  store.failPublication = false;
  store.failCleanup = false;

  const uint32_t successfulRevision = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(8, successfulRevision, 7, 8)) == Delivery::Delivered,
                "successful racer change reaches Race Control after rejected attempts");
  control.tick(1200);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None) &&
                restartHasRacers(store, 7, 8),
                "successful racer change and restart reconstruct the newest IDs");

  const uint32_t revisionAfterRacers = control.proposalRevision();
  const uint32_t writesBeforeOrdinary = store.writeCalls;
  ok &= require(bus.publish(presentation, setupRequest(9, revisionAfterRacers, 0, 0, 0, 5)) == Delivery::Delivered,
                "ordinary race-setting update reaches Race Control");
  control.tick(1201);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "ordinary race-setting update remains accepted");
  ok &= require(control.proposedRaceSetup()->entries[0].mugId == 7 &&
                control.proposedRaceSetup()->entries[1].mugId == 8,
                "ordinary race settings preserve selected racers");
  Configuration afterOrdinary{}; memory.load(afterOrdinary);
  ok &= require(afterOrdinary.lane1MugId == 7 && afterOrdinary.lane2MugId == 8 &&
                store.writeCalls == writesBeforeOrdinary,
                "ordinary race settings do not overwrite remembered racers");

  const uint32_t revisionBeforeDuplicate = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(10, revisionBeforeDuplicate, 5, 5)) == Delivery::Delivered,
                "duplicate racer request reaches Race Control");
  control.tick(1202);
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup),
                "duplicate racer identities are rejected");
  ok &= require(control.proposalRevision() == revisionBeforeDuplicate &&
                control.proposedRaceSetup()->entries[0].mugId == 7 &&
                control.proposedRaceSetup()->entries[1].mugId == 8,
                "rejected racer assignment preserves proposal and revision");

  const uint32_t revisionBeforeInvalid = control.proposalRevision();
  ok &= require(bus.publish(presentation, setupRequest(11, revisionBeforeInvalid, 0, 6)) == Delivery::Delivered,
                "invalid racer request reaches Race Control");
  control.tick(1203);
  ok &= require(receiveResult(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup) &&
                control.proposalRevision() == revisionBeforeInvalid,
                "zero racer identity is rejected without changing the proposal");

  MemoryStore oldStore;
  oldStore.seedOldRecord(7, 2, 17, 0, true, true, uint8_t(SessionMode::LapRace));
  Memory oldMemory(oldStore); Configuration oldConfiguration{};
  ok &= require(oldMemory.load(oldConfiguration) == LoadStatus::Remembered &&
                oldConfiguration.lanes == 2 && oldConfiguration.laps == 17 &&
                oldConfiguration.mode == uint8_t(SessionMode::LapRace) &&
                oldConfiguration.lane1MugId == 1 && oldConfiguration.lane2MugId == 2,
                "valid old-format configuration reconstructs settings and default MUG identities");

  Message start{}; start.type = Type::StartRequest; start.correlation = 12;
  start.clientContext = ClientContext::RaceDirectorSmug; start.sessionMode = SessionMode::None;
  ok &= require(bus.publish(presentation, start) == Delivery::Delivered, "START reaches Race Control");
  control.tick(1204);
  ok &= require(receiveResult(bus, presentation, RequestResult::Accepted, RequestRejection::None),
                "selected racers permit the normal START path");
  ok &= require(control.definition() && control.definition()->entry(0).mugId == 7 &&
                control.definition()->entry(1).mugId == 8,
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
  ok &= require(engine.entryState(0).mugId == 7 && engine.entryState(1).mugId == 8 &&
                engine.entryState(0).laps == 1 && engine.entryState(1).laps == 1,
                "normal racing preserves selected racer identities for both lanes");

  Memory reconstructedMemory(store); Configuration reconstructed{};
  ok &= require(reconstructedMemory.load(reconstructed) == LoadStatus::Remembered &&
                reconstructed.lane1MugId == 7 && reconstructed.lane2MugId == 8,
                "restart reconstruction restores newest racer selection");

  std::cout << (ok ? "WP7.3 Racer selection contract PASS\n" : "WP7.3 Racer selection contract FAIL\n");
  return ok ? 0 : 1;
}
