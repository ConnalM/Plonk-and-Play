#include "pp/race_control.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
bool require(bool value, const char* message) {
  if (!value) std::cerr << "FAIL: " << message << "\n";
  return value;
}

class IntegrationStore final : public pp::Store {
public:
  static constexpr unsigned SlotCount = 213;
  bool failA = false;
  bool failB = false;
  bool available() const override { return available_; }
  bool read(unsigned slot, uint8_t* data, size_t size) override {
    if (!available_ || slot >= SlotCount || !data || slots_[slot].size() != size) return false;
    std::memcpy(data, slots_[slot].data(), size);
    return true;
  }
  ReadResult readResult(unsigned slot, uint8_t* data, size_t size) override {
    if ((slot == pp::MugCatalog::CatalogSlotA && failA) ||
        (slot == pp::MugCatalog::CatalogSlotB && failB)) return ReadResult::Failed;
    return Store::readResult(slot, data, size);
  }
  bool write(unsigned slot, const uint8_t* data, size_t size) override {
    if (!available_ || slot >= SlotCount || !data) return false;
    slots_[slot].assign(data, data + size);
    return true;
  }
  bool writeMetadata(unsigned slot, const uint8_t* data, size_t size) override {
    return write(slot, data, size);
  }
  bool reclaim(unsigned slot) override {
    if (slot < SlotCount) slots_[slot].clear();
    return true;
  }
  void setAvailable(bool value) { available_ = value; }
private:
  bool available_ = true;
  std::array<std::vector<uint8_t>, SlotCount> slots_{};
};

pp::ProposedRaceSetup setup() {
  pp::ProposedRaceSetup value{};
  value.activeLanes = 2;
  value.mode = pp::SessionMode::LapRace;
  value.lapTarget = 2;
  value.entries[0].startFinish = {pp::InputModule::simulatedDetectorIdentity(0), 1, pp::InputRole::StartFinish};
  value.entries[1].startFinish = {pp::InputModule::simulatedDetectorIdentity(1), 2, pp::InputRole::StartFinish};
  value.entries[0].mugId = 1;
  value.entries[1].mugId = 2;
  value.startFinish = value.entries[0].startFinish;
  value.secondStartFinish = value.entries[1].startFinish;
  value.selectedMugId = 1;
  value.secondMugId = 2;
  return value;
}

pp::Message racerRequest(uint32_t correlation, uint32_t revision, uint32_t lane1, uint32_t lane2) {
  pp::Message value{};
  value.type = pp::Type::SetupRequest;
  value.correlation = correlation;
  value.clientContext = pp::ClientContext::RaceDirectorSmug;
  value.sessionMode = pp::SessionMode::LapRace;
  value.setupLapTarget = 2;
  value.setupFinishPolicy = uint8_t(pp::LapFinishBehaviour::Immediate);
  value.proposalRevision = revision;
  value.setupRacerAssignmentMask = 0x03;
  value.setupLane1MugId = lane1;
  value.setupLane2MugId = lane2;
  return value;
}

bool result(pp::Bus& bus, pp::Bus::Endpoint presentation,
            pp::RequestResult expected, pp::RequestRejection rejection) {
  pp::Message value{};
  return bus.receive(presentation, value) && value.type == pp::Type::RequestResult &&
         value.requestResult == expected && value.rejection == rejection;
}
}

int main() {
  using namespace pp;
  bool ok = true;
  IntegrationStore store;
  Memory memory(store);
  Configuration defaults{};
  ok &= require(memory.load(defaults) == LoadStatus::DefaultsMissing && memory.save(defaults),
                "controller memory starts with built-in MUG defaults");
  MugCatalog catalog(store);
  ok &= require(catalog.load() == MugCatalog::LoadResult::Empty,
                "controller-owned catalog starts empty");

  Bus bus;
  const auto presentation = bus.attach(Role::Presentation);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  ok &= require(bus.subscribe(controlEndpoint, Type::SetupRequest) &&
                bus.subscribe(controlEndpoint, Type::StartRequest) &&
                bus.subscribe(presentation, Type::RequestResult),
                "controller authority subscriptions are available");
  ActiveSessionDefinition active;
  SimulatedDetectorProvider provider;
  RaceControlModule control(bus, controlEndpoint, active, nullptr, nullptr, &provider,
                            nullptr, &memory, &catalog);
  control.setProposedRaceSetup(setup());
  control.setRequiredCapabilityAvailable(true);

  uint32_t alice = 0, bob = 0;
  ok &= require(control.createMug(ClientContext::Spectator, "Alice", alice) == MugCreateResult::PermissionDenied,
                "spectator cannot create a saved MUG");
  ok &= require(control.createMug(ClientContext::RaceDirectorSmug, "Alice", alice) == MugCreateResult::Accepted && alice == 3,
                "controlling Race Director creates the first saved MUG through controller authority");
  ok &= require(control.createMug(ClientContext::RaceDirectorSmug, "Bob", bob) == MugCreateResult::Accepted && bob == 4,
                "second saved MUG receives its permanent ID");
  MugCatalog::Entry listed{};
  ok &= require(catalog.find(alice, listed, false) && std::strcmp(listed.name, "Alice") == 0 &&
                catalog.find(bob, listed, false) && std::strcmp(listed.name, "Bob") == 0,
                "catalog exposes actual saved names and permanent IDs");

  const uint32_t before = control.proposalRevision();
  ok &= require(bus.publish(presentation, racerRequest(1, before, alice, bob)) == Delivery::Delivered,
                "saved racer assignment reaches Race Control");
  control.tick(1000);
  ok &= require(result(bus, presentation, RequestResult::Accepted, RequestRejection::None) &&
                control.proposedRaceSetup()->entries[0].mugId == alice &&
                control.proposedRaceSetup()->entries[1].mugId == bob,
                "saved IDs become the authoritative proposal");

  const uint32_t rejectedRevision = control.proposalRevision();
  ok &= require(bus.publish(presentation, racerRequest(2, rejectedRevision, 99, bob)) == Delivery::Delivered,
                "unknown saved identity reaches Race Control");
  control.tick(1001);
  ok &= require(result(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup) &&
                control.proposalRevision() == rejectedRevision &&
                control.proposedRaceSetup()->entries[0].mugId == alice,
                "unknown ID is rejected without changing the proposal");

  ok &= require(catalog.archive(alice) == MugCatalog::Result::Accepted,
                "catalog can archive a saved identity outside the selection path");
  const uint32_t archivedRevision = control.proposalRevision();
  ok &= require(bus.publish(presentation, racerRequest(3, archivedRevision, alice, bob)) == Delivery::Delivered,
                "archived identity reaches Race Control");
  control.tick(1002);
  ok &= require(result(bus, presentation, RequestResult::Rejected, RequestRejection::InvalidRaceSetup) &&
                control.proposalRevision() == archivedRevision,
                "archived identity is rejected without publication");

  ok &= require(catalog.rename(bob, "Bob Renamed") == MugCatalog::Result::Accepted,
                "catalog update remains independent of lane assignment");
  const unsigned newestSlot = catalog.activeSlot();
  if (newestSlot == MugCatalog::CatalogSlotA) store.failA = true; else store.failB = true;
  MugCatalog failed(store);
  ok &= require(failed.load() == MugCatalog::LoadResult::StorageUnavailable,
                "partial A/B catalog read is unavailable rather than stale");
  store.failA = false; store.failB = false;
  ok &= require(failed.reload() == MugCatalog::LoadResult::Remembered &&
                failed.find(bob, listed, false) && std::strcmp(listed.name, "Bob Renamed") == 0,
                "catalog reconstructs after the read failure clears");

  std::cout << (ok ? "WP7 saved MUG controller integration PASS\n" :
                       "WP7 saved MUG controller integration FAIL\n");
  return ok ? 0 : 1;
}
