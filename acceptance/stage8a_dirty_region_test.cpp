#include <cassert>
#include <iostream>

#include "../../firmware/include/pp/ui_dirty_regions.h"
#include "../../firmware/include/pp/wp7_presentation_contract.h"

using namespace pp;

int main() {
  DirtyRegionSet unchanged;
  assert(unchanged.empty());
  assert(unchanged.count() == 0);

  DirtyRegionSet overlap;
  assert(overlap.mark(UiRect{10, 10, 20, 20},
                      uiComponentBit(Wp7UiComponent::ReadyValue)));
  assert(overlap.mark(UiRect{20, 20, 20, 20},
                      uiComponentBit(Wp7UiComponent::ReadyUnit)));
  assert(overlap.count() == 1);
  assert(overlap.at(0).rect.x == 10);
  assert(overlap.at(0).rect.y == 10);
  assert(overlap.at(0).rect.width == 30);
  assert(overlap.at(0).rect.height == 30);
  assert(overlap.has(Wp7UiComponent::ReadyValue));
  assert(overlap.has(Wp7UiComponent::ReadyUnit));

  DirtyRegionSet adjacent;
  adjacent.mark(UiRect{0, 0, 10, 10}, 1U);
  adjacent.mark(UiRect{10, 0, 10, 10}, 2U);
  assert(adjacent.count() == 2);

  DirtyRegionSet lights;
  lights.mark(Wp7UiComponent::Light2);
  assert(lights.count() == 1);
  assert(lights.has(Wp7UiComponent::Light2));
  assert(!lights.has(Wp7UiComponent::Light1));
  assert(!lights.has(Wp7UiComponent::Light3));
  const UiRect light2 = Wp7DirtyLayout::rect(Wp7UiComponent::Light2);
  assert(light2.x == 399 && light2.y == 4);
  assert(light2.width == 60 && light2.height == 60);
  assert(lights.at(0).rect.x == light2.x);
  const UiRect light0 = Wp7DirtyLayout::rect(Wp7UiComponent::Light0);
  const UiRect light1 = Wp7DirtyLayout::rect(Wp7UiComponent::Light1);
  const UiRect light3 = Wp7DirtyLayout::rect(Wp7UiComponent::Light3);
  const UiRect light4 = Wp7DirtyLayout::rect(Wp7UiComponent::Light4);
  assert(light0.x == 243 && light4.x == 555);
  assert(light1.x - light0.right() == 18);
  assert(light2.x - light1.right() == 18);
  assert(light3.x - light2.right() == 18);
  assert(light4.x - light3.right() == 18);
  // The new header ends at y=69; the READY controls begin at y=72, so lamp
  // updates cannot touch any READY controls.
  assert(light2.bottom() < Wp7DirtyLayout::rect(
      Wp7UiComponent::ReadyModeLaps).y);

  DirtyRegionSet numericTransition;
  numericTransition.mark(Wp7UiComponent::ReadyValue);
  assert(numericTransition.count() == 1);
  assert(numericTransition.has(Wp7UiComponent::ReadyValue));
  assert(!numericTransition.has(Wp7UiComponent::Lane1Static));
  assert(!numericTransition.has(Wp7UiComponent::Lane2Static));
  assert(!numericTransition.has(Wp7UiComponent::Lane1Metrics));
  assert(!numericTransition.has(Wp7UiComponent::Lane2Metrics));
  assert(numericTransition.at(0).rect.width ==
         Wp7DirtyLayout::rect(Wp7UiComponent::ReadyValue).width);

  DirtyRegionSet readyTarget;
  readyTarget.mark(Wp7UiComponent::Lane1Target);
  readyTarget.mark(Wp7UiComponent::Lane2Target);
  assert(readyTarget.count() == 2);
  assert(readyTarget.has(Wp7UiComponent::Lane1Target));
  assert(readyTarget.has(Wp7UiComponent::Lane2Target));
  assert(!readyTarget.has(Wp7UiComponent::Lane1Metrics));
  assert(!readyTarget.has(Wp7UiComponent::Lane2Metrics));
  assert(Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Target).width <
         Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Metrics).width);
  assert(!Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Target).overlaps(
      Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Metrics)));
  assert(!Wp7DirtyLayout::rect(Wp7UiComponent::Lane2Target).overlaps(
      Wp7DirtyLayout::rect(Wp7UiComponent::Lane2Metrics)));
  assert(Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Target).right() ==
         Wp7DirtyLayout::rect(Wp7UiComponent::Lane1Metrics).x);

  // A paused warning phase changes only the five lamp cells.  Keeping the
  // regions separate prevents a 500 ms phase flip from repainting the screen.
  DirtyRegionSet pausedWarning;
  pausedWarning.mark(Wp7UiComponent::Light0);
  pausedWarning.mark(Wp7UiComponent::Light1);
  pausedWarning.mark(Wp7UiComponent::Light2);
  pausedWarning.mark(Wp7UiComponent::Light3);
  pausedWarning.mark(Wp7UiComponent::Light4);
  assert(pausedWarning.count() == 5);
  assert(!pausedWarning.has(Wp7UiComponent::FullFrame));

  // GO changes the lamps, begins the visible lap/timer fields and replaces the
  // starting message with the pause control.  It must stay a collection of
  // local regions rather than reverting to a framebuffer redraw.
  Wp7PartialInvalidationInput go{};
  for (uint8_t index = 0; index < 5; ++index) {
    go.lightColors[index] = 0x07E0;
    go.previousLightColors[index] = 0xF800;
  }
  go.lane1MetricsChanged = true;
  go.lane1TargetChanged = true;
  go.lane2MetricsChanged = true;
  go.lane2TargetChanged = true;
  go.bottomMessageChanged = true;
  go.mainButtonChanged = true;
  const DirtyRegionSet goTransition = planWp7PartialInvalidation(go);
  assert(goTransition.count() == 11);
  assert(!goTransition.has(Wp7UiComponent::FullFrame));
  assert(goTransition.has(Wp7UiComponent::Light0));
  assert(goTransition.has(Wp7UiComponent::Light4));
  assert(goTransition.has(Wp7UiComponent::Lane1Metrics));
  assert(goTransition.has(Wp7UiComponent::Lane1Target));
  assert(goTransition.has(Wp7UiComponent::Lane2Metrics));
  assert(goTransition.has(Wp7UiComponent::Lane2Target));
  assert(goTransition.has(Wp7UiComponent::BottomMessage));
  assert(goTransition.has(Wp7UiComponent::MainButton));

  // PAUSED warnings retain the approved 500 ms alternating Y/O sequence and
  // remain five independent lamp cells.
  assert(pausedWarningLightsEnabled());
  assert(pausedWarningPhase(0) == 0);
  assert(pausedWarningPhase(499) == 0);
  assert(pausedWarningPhase(500) == 1);
  assert(pausedWarningLampLit(0, 0));
  assert(!pausedWarningLampLit(1, 0));
  assert(!pausedWarningLampLit(0, 1));
  assert(pausedWarningLampLit(1, 1));

  DirtyRegionSet modeChange;
  modeChange.mark(Wp7UiComponent::ReadyModeLaps);
  modeChange.mark(Wp7UiComponent::ReadyModeTimed);
  modeChange.mark(Wp7UiComponent::ReadyUnit);
  assert(modeChange.count() == 3);

  DirtyRegionSet full;
  full.mark(Wp7UiComponent::ReadyValue);
  full.mark(Wp7UiComponent::FullFrame);
  assert(full.count() == 1);
  assert(full.has(Wp7UiComponent::FullFrame));
  assert(full.at(0).rect.x == 0 && full.at(0).rect.y == 0);

  // The safety fallback is observable and must be distinguishable from an
  // ordinary explicit full-frame transition.
  DirtyRegionSet overflow;
  for (int i = 0; i < 17; ++i) {
    assert(overflow.mark(UiRect{static_cast<int16_t>((i % 6) * 120),
                               static_cast<int16_t>((i / 6) * 120), 20, 20},
                         uint64_t{1} << (i + 1)) ||
           overflow.overflowed());
  }
  assert(overflow.overflowed());
  assert(overflow.count() == 1);
  assert(overflow.has(Wp7UiComponent::FullFrame));

  std::cout << "DIRTY_REGION_TEST_PASS\n";
  return 0;
}
