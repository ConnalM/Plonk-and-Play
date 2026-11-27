#include "pp/core_runtime_contract.h"
#include "pp/core_runtime_wiring.h"
#include "pp/firmware_update_api.h"
#include "pp/race_control.h"
#include "pp/session_definition.h"

#include <cassert>
#include <iostream>

int main() {
  using namespace pp;
  using namespace pp::core_runtime;

  UnavailableDetectorProvider detectors;
  assert(detectors.detectorCount() == 0);
  Time trigger = 0;
  assert(!detectors.sample(0, 1, trigger));

  ProposedRaceSetup setup{};
  setup.activeLanes = 2;
  setup.lapTarget = 10;
  setup.entries[0].startFinish = {UnavailableDetectorProvider::laneIdentity(1),
                                  1, InputRole::StartFinish};
  setup.entries[1].startFinish = {UnavailableDetectorProvider::laneIdentity(2),
                                  2, InputRole::StartFinish};
  setup.entries[0].mugId = 1;
  setup.entries[1].mugId = 2;
  setup.startFinish = setup.entries[0].startFinish;
  setup.secondStartFinish = setup.entries[1].startFinish;
  setup.selectedMugId = 1;
  setup.secondMugId = 2;
  assert(valid(setup));
  assert(!requiredDetectorCapabilitiesHealthy(setup, detectors));

  UnavailableHistoryStore history;
  UnavailableTrackRecordStore records;
  uint32_t sequence = 99;
  const uint8_t payload[] = {1, 2, 3};
  assert(!history.append(payload, sizeof(payload), sequence));
  assert(sequence == 0);
  assert(history.count() == 0);
  assert(history.health() == StoreHealth::Unavailable);
  assert(!records.observe(1, 1000));
  assert(records.lanePb(1) == 0);
  assert(records.trackRecord() == 0);
  assert(!kUnqualifiedBenchSafety.startPermitted());

  // The Core bench has no update endpoint and maintenance remains explicitly
  // unavailable. The Browser-facing API compiles independently of the
  // signed-image verifier and exposes the existing failure response.
  assert(static_cast<unsigned>(MaintenanceApiResult::Failed) == 5u);

  // Exercise the production Race Control path, rather than merely checking
  // the detector boundary in isolation.  A valid proposal and an authorised
  // Browser START must still be refused before a session definition exists.
  class Controller final : public StartAuthority {
  public:
    bool authorizesStart(uint64_t owner) const override { return owner == 7; }
    bool hasController() const override { return true; }
  } controller;

  Bus bus;
  const core_runtime::RuntimeEndpoints endpoints{
      bus.attach(Role::Lifecycle), bus.attach(Role::Memory), bus.attach(Role::Input),
      bus.attach(Role::RaceControl), bus.attach(Role::RaceEngine),
      bus.attach(Role::Presentation)};
  assert(endpoints.lifecycle.id && endpoints.memory.id && endpoints.input.id &&
         endpoints.raceControl.id && endpoints.raceEngine.id &&
         endpoints.presentation.id);
  // This is the actual subscription list used by CoreRuntime, not a local
  // approximation of the Race Control and Browser portions of that wiring.
  assert(core_runtime::subscribeRuntime(bus, endpoints));
  ActiveSessionDefinition active;
  RaceControlModule raceControl{bus, endpoints.raceControl, active,
                                &history, &records, &detectors};
  raceControl.setControllerStartAuthority(endpoints.presentation, controller);
  raceControl.setProposedRaceSetup(setup);
  raceControl.setRequiredCapabilityAvailable(false);
  assert(raceControl.readiness() == StartReadiness::CapabilityUnavailable);

  Message start{};
  start.type = Type::StartRequest;
  start.correlation = 41;
  start.clientContext = ClientContext::RaceDirectorSmug;
  start.controlOwner = 7;
  start.sessionMode = SessionMode::None;
  assert(bus.publish(endpoints.presentation, start) == Delivery::Delivered);
  raceControl.tick(1000000);
  Message result{};
  assert(bus.receive(endpoints.presentation, result));
  assert(result.type == Type::RequestResult);
  assert(result.correlation == start.correlation);
  assert(result.requestResult == RequestResult::Rejected);
  assert(result.rejection == RequestRejection::RequiredCapabilityUnavailable);
  assert(!active.current());
  assert(history.count() == 0);
  assert(records.trackRecord() == 0);

  // The same shared list must also retain the production Input -> Race Engine
  // integrity route; it is deliberately separate from Browser presentation.
  Message integrity{};
  integrity.type = Type::RaceIntegrityFault;
  assert(bus.publish(endpoints.input, integrity) == Delivery::Delivered);
  assert(bus.queued(endpoints.raceEngine) == 1);

  std::cout << "S4 Core unavailable-hardware safety contract PASS\n";
}
