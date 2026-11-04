#include "../../firmware/include/pp/core.h"
#include <iostream>

namespace {
bool require(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL: " << message << "\n";
  return condition;
}
}

int main() {
  using namespace pp;
  bool ok = true;
  Bus bus;
  const auto inputEndpoint = bus.attach(Role::Input);
  const auto raceEndpoint = bus.attach(Role::RaceEngine);
  ok &= require(bus.subscribe(raceEndpoint, Type::InputEvent), "race endpoint subscribes to input events");

  SimulatedDetectorProvider provider;
  InputModule input(bus, inputEndpoint, provider);
  const auto laneA = input.detectorStatus(0);
  const auto laneB = input.detectorStatus(1);
  ok &= require(input.detectorCount() >= 2, "provider exposes two independent detector sources");
  ok &= require(laneA.healthy() && laneB.healthy(), "both simulated detectors start available and healthy");
  ok &= require(laneA.identity != laneB.identity, "detector identities are distinct");

  input.sampleSource(0, false, 0);
  input.sampleSource(0, true, 100);
  input.sampleSource(0, true, 100 + SimulatedDetector::DetectionStableUs);
  Message event{};
  ok &= require(bus.receive(raceEndpoint, event), "lane A produces one clean event");
  ok &= require(event.input == laneA.identity && event.relevantTime == 100 + SimulatedDetector::DetectionStableUs,
                "lane A event preserves stable identity and source timestamp");
  ok &= require(event.raceEntryId == 0 && event.lapNumber == 0, "input event remains role-neutral");

  input.sampleSourceB(false, 30000);
  input.sampleSourceB(true, 30100);
  input.sampleSourceB(true, 30100 + SimulatedDetector::DetectionStableUs);
  Message eventB{};
  ok &= require(bus.receive(raceEndpoint, eventB), "lane B produces one independent clean event");
  ok &= require(eventB.input == laneB.identity && eventB.relevantTime == 30100 + SimulatedDetector::DetectionStableUs,
                "lane B event preserves its own identity and timestamp");
  ok &= require(eventB.eventId != event.eventId, "events have independent event identities");

  provider.setAvailable(0, false);
  auto missing = input.detectorStatus(0);
  ok &= require(missing.availability == DetectorAvailability::Missing && missing.health == DetectorHealth::Unknown,
                "missing detector is reported authoritatively");
  input.sampleSource(0, false, 60000);
  input.sampleSource(0, true, 60100);
  input.sampleSource(0, true, 60100 + SimulatedDetector::DetectionStableUs);
  ok &= require(!bus.receive(raceEndpoint, event), "missing detector cannot emit an event");

  provider.setAvailable(0, true);
  ok &= require(input.detectorStatus(0).healthy(), "detector becomes healthy after availability is restored");
  provider.setFault(0, true);
  auto fault = input.detectorStatus(0);
  ok &= require(fault.availability == DetectorAvailability::Available && fault.health == DetectorHealth::Fault,
                "detector fault is distinct from physical absence");
  input.sampleSource(0, false, 90000);
  input.sampleSource(0, true, 90100);
  input.sampleSource(0, true, 90100 + SimulatedDetector::DetectionStableUs);
  ok &= require(!bus.receive(raceEndpoint, event), "faulted detector cannot emit an event");
  provider.setFault(0, false);
  ok &= require(input.detectorStatus(0).healthy(), "cleared fault restores healthy state");

  const InputIdentity replacement{0x50500099u, 7};
  provider.markReplacement(1, replacement);
  const auto replaced = input.detectorStatus(1);
  ok &= require(replaced.identity == laneB.identity, "replacement does not mutate configured identity");
  ok &= require(replaced.observedIdentity == replacement && replaced.replacementRequired,
                "replacement identity is exposed for explicit reconciliation");
  ok &= require(replaced.health == DetectorHealth::ReplacementRequired,
                "replacement is a distinct health state");
  input.sampleSourceB(false, 120000);
  input.sampleSourceB(true, 120100);
  input.sampleSourceB(true, 120100 + SimulatedDetector::DetectionStableUs);
  ok &= require(!bus.receive(raceEndpoint, event), "replacement cannot silently emit or reassign a lane");

  std::cout << (ok ? "WP7.1 detector provider contract PASS\n" : "WP7.1 detector provider contract FAIL\n");
  return ok ? 0 : 1;
}
