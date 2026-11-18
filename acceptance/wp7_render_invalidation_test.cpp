#include <cassert>
#include <cstdio>

#include "pp/race_engine.h"
#include "pp/ui_dirty_regions.h"

using namespace pp;

static Wp7PartialInvalidationInput baseInput() {
  Wp7PartialInvalidationInput input{};
  for (uint8_t i = 0; i < 5; ++i) {
    input.lightColors[i] = 0x1234;
    input.previousLightColors[i] = 0x1234;
  }
  return input;
}

static bool acceptedLaneLap(uint8_t lane, uint32_t& laps) {
  Bus bus;
  ActiveSessionDefinition active;
  const auto engineEndpoint = bus.attach(Role::RaceEngine);
  const auto inputEndpoint = bus.attach(Role::Input);
  const auto controlEndpoint = bus.attach(Role::RaceControl);
  bus.subscribe(engineEndpoint, Type::GoScheduled);
  bus.subscribe(engineEndpoint, Type::InputEvent);

  ProposedRaceSetup setup{};
  setup.activeLanes = 2;
  setup.lapTarget = 8;
  setup.mode = SessionMode::LapRace;
  setup.entries[0].mugId = 91;
  setup.entries[0].startFinish = {
      InputModule::simulatedDetectorIdentity(0), 1, InputRole::StartFinish};
  setup.entries[1].mugId = 92;
  setup.entries[1].startFinish = {
      InputModule::simulatedDetectorIdentity(1), 2, InputRole::StartFinish};
  setup.startFinish = setup.entries[0].startFinish;
  setup.secondStartFinish = setup.entries[1].startFinish;
  if (!active.commit(setup, 1, 1)) return false;

  SessionDefinition definition(setup, 1, 1);
  RaceEngineModule engine(bus, engineEndpoint, active);
  engine.begin(definition, 1000000);

  Message go{};
  go.type = Type::GoScheduled;
  go.relevantTime = 1000000;
  if (bus.publish(controlEndpoint, go) != Delivery::Delivered) return false;
  engine.tick();

  Message passage{};
  passage.type = Type::InputEvent;
  passage.eventId = 1;
  passage.input = InputModule::simulatedDetectorIdentity(lane - 1);
  passage.relevantTime = 1100000;
  if (bus.publish(inputEndpoint, passage) != Delivery::Delivered) return false;
  engine.tick();

  const uint8_t index = static_cast<uint8_t>(lane - 1);
  laps = engine.entryState(index).laps;
  return laps == 1;
}

int main() {
  // An accepted Lane 1 passage changes the authoritative lane total.  The
  // production drawCurrent() path supplies that signature change to the same
  // planner used here, which must select only Lane 1's target cell.
  uint32_t lane1Laps = 0;
  assert(acceptedLaneLap(1, lane1Laps) && lane1Laps == 1);
  uint32_t lane2Laps = 0;
  assert(acceptedLaneLap(2, lane2Laps) && lane2Laps == 1);
  auto lane1 = baseInput();
  lane1.lane1TargetChanged = true;
  DirtyRegionSet lane1Dirty = planWp7PartialInvalidation(lane1);
  assert(lane1Dirty.has(Wp7UiComponent::Lane1Target));
  assert(!lane1Dirty.has(Wp7UiComponent::Lane2Target));
  assert(!lane1Dirty.has(Wp7UiComponent::Lane1Metrics));
  assert(!lane1Dirty.has(Wp7UiComponent::FullFrame));

  auto lane1LapWithMetrics = baseInput();
  lane1LapWithMetrics.lane1TargetChanged = true;
  lane1LapWithMetrics.lane1MetricsChanged = true;
  DirtyRegionSet lane1LapDirty =
      planWp7PartialInvalidation(lane1LapWithMetrics);
  assert(lane1LapDirty.has(Wp7UiComponent::Lane1Target));
  assert(lane1LapDirty.has(Wp7UiComponent::Lane1Metrics));
  assert(!lane1LapDirty.has(Wp7UiComponent::Lane2Target));
  assert(!lane1LapDirty.has(Wp7UiComponent::Lane2Metrics));
  assert(!lane1LapDirty.has(Wp7UiComponent::FullFrame));

  // Lane 2 is independent, and a crossing that changes both the progress cell
  // and LAST/BEST/RACE TIME repaints both fixed regions, never the whole row.
  auto lane2 = baseInput();
  lane2.lane2TargetChanged = true;
  lane2.lane2MetricsChanged = true;
  DirtyRegionSet lane2Dirty = planWp7PartialInvalidation(lane2);
  assert(lane2Dirty.has(Wp7UiComponent::Lane2Target));
  assert(lane2Dirty.has(Wp7UiComponent::Lane2Metrics));
  assert(!lane2Dirty.has(Wp7UiComponent::Lane1Target));
  assert(!lane2Dirty.has(Wp7UiComponent::FullFrame));

  // A normal countdown step changes exactly one lamp.  The GO transition
  // changes all five lamps, but still never falls back to FullFrame.
  auto oneLamp = baseInput();
  oneLamp.lightColors[2] = 0xF800;
  DirtyRegionSet oneLampDirty = planWp7PartialInvalidation(oneLamp);
  assert(oneLampDirty.count() == 1);
  assert(oneLampDirty.has(Wp7UiComponent::Light2));
  assert(!oneLampDirty.has(Wp7UiComponent::Light1));
  assert(!oneLampDirty.has(Wp7UiComponent::Light3));
  assert(!oneLampDirty.has(Wp7UiComponent::FullFrame));

  auto go = baseInput();
  for (uint8_t i = 0; i < 5; ++i) go.lightColors[i] = 0x07E0;
  DirtyRegionSet goDirty = planWp7PartialInvalidation(go);
  assert(goDirty.count() == 5);
  for (uint8_t i = 0; i < 5; ++i) {
    assert(goDirty.has(static_cast<Wp7UiComponent>(
        static_cast<uint8_t>(Wp7UiComponent::Light0) + i)));
  }
  assert(!goDirty.has(Wp7UiComponent::FullFrame));

  auto unchanged = baseInput();
  DirtyRegionSet unchangedDirty = planWp7PartialInvalidation(unchanged);
  assert(unchangedDirty.empty());

  // The current production flag supplies false animation inputs.  A lap or a
  // Finished state therefore cannot add car or finish-flag redraw regions.
  auto disabledAnimations = baseInput();
  disabledAnimations.lane1TargetChanged = true;
  disabledAnimations.finishFlagVisible = false;
  DirtyRegionSet disabledDirty = planWp7PartialInvalidation(disabledAnimations);
  assert(!disabledDirty.has(Wp7UiComponent::Lane1Static));
  assert(!disabledDirty.has(Wp7UiComponent::FinishFlag));

  auto finished = baseInput();
  finished.finishFlagVisible = false;
  DirtyRegionSet finishedDirty = planWp7PartialInvalidation(finished);
  assert(finishedDirty.empty());

  std::puts("WP7_RENDER_INVALIDATION_PASS");
  return 0;
}
