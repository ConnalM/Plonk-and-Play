#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pp {

struct UiRect {
  int16_t x = 0;
  int16_t y = 0;
  int16_t width = 0;
  int16_t height = 0;

  int right() const { return static_cast<int>(x) + width; }
  int bottom() const { return static_cast<int>(y) + height; }
  bool valid() const { return width > 0 && height > 0; }

  bool overlaps(const UiRect& other) const {
    return valid() && other.valid() && x < other.right() &&
           other.x < right() && y < other.bottom() &&
           other.y < bottom();
  }

  UiRect united(const UiRect& other) const {
    const int left = x < other.x ? x : other.x;
    const int top = y < other.y ? y : other.y;
    const int rightEdge = right() > other.right() ? right() : other.right();
    const int bottomEdge = bottom() > other.bottom() ? bottom() : other.bottom();
    return {static_cast<int16_t>(left), static_cast<int16_t>(top),
            static_cast<int16_t>(rightEdge - left),
            static_cast<int16_t>(bottomEdge - top)};
  }
};

enum class Wp7UiComponent : uint8_t {
  FullFrame = 0,
  Light0,
  Light1,
  Light2,
  Light3,
  Light4,
  ReadyModeLaps,
  ReadyModeTimed,
  ReadyMinus,
  ReadyValue,
  ReadyUnit,
  ReadyPlus,
  ActiveTop,
  Lane1Static,
  Lane1Metrics,
  Lane1Target,
  Lane2Static,
  Lane2Metrics,
  Lane2Target,
  BottomMessage,
  MainButton,
  PauseOverlay,
  FinishFlag,
  Count,
};

constexpr uint64_t uiComponentBit(Wp7UiComponent component) {
  return uint64_t{1} << static_cast<uint8_t>(component);
}

struct Wp7DirtyLayout {
  static constexpr UiRect fullFrame() { return {0, 0, 800, 480}; }
  static constexpr UiRect rect(Wp7UiComponent component) {
    switch (component) {
      case Wp7UiComponent::Light0: return {243, 4, 60, 60};
      case Wp7UiComponent::Light1: return {321, 4, 60, 60};
      case Wp7UiComponent::Light2: return {399, 4, 60, 60};
      case Wp7UiComponent::Light3: return {477, 4, 60, 60};
      case Wp7UiComponent::Light4: return {555, 4, 60, 60};
      case Wp7UiComponent::ReadyModeLaps: return {156, 72, 165, 54};
      case Wp7UiComponent::ReadyModeTimed: return {332, 72, 165, 54};
      case Wp7UiComponent::ReadyMinus: return {516, 72, 62, 54};
      case Wp7UiComponent::ReadyValue: return {584, 72, 82, 54};
      case Wp7UiComponent::ReadyUnit: return {664, 72, 53, 54};
      case Wp7UiComponent::ReadyPlus: return {719, 72, 62, 54};
      case Wp7UiComponent::ActiveTop: return {150, 70, 540, 110};
      case Wp7UiComponent::Lane1Static: return {20, 183, 760, 103};
      case Wp7UiComponent::Lane1Metrics: return {468, 199, 312, 71};
      case Wp7UiComponent::Lane1Target: return {372, 199, 96, 71};
      case Wp7UiComponent::Lane2Static: return {20, 299, 760, 104};
      case Wp7UiComponent::Lane2Metrics: return {468, 315, 312, 71};
      case Wp7UiComponent::Lane2Target: return {372, 315, 96, 71};
      case Wp7UiComponent::BottomMessage: return {0, 406, 244, 74};
      case Wp7UiComponent::MainButton: return {244, 416, 313, 62};
      case Wp7UiComponent::PauseOverlay: return {0, 0, 800, 480};
      case Wp7UiComponent::FinishFlag: return {650, 70, 130, 60};
      case Wp7UiComponent::FullFrame:
      case Wp7UiComponent::Count:
      default: return fullFrame();
    }
  }

  static const char* name(Wp7UiComponent component) {
    switch (component) {
      case Wp7UiComponent::FullFrame: return "full";
      case Wp7UiComponent::Light0: return "light0";
      case Wp7UiComponent::Light1: return "light1";
      case Wp7UiComponent::Light2: return "light2";
      case Wp7UiComponent::Light3: return "light3";
      case Wp7UiComponent::Light4: return "light4";
      case Wp7UiComponent::ReadyModeLaps: return "ready-mode-laps";
      case Wp7UiComponent::ReadyModeTimed: return "ready-mode-timed";
      case Wp7UiComponent::ReadyMinus: return "ready-minus";
      case Wp7UiComponent::ReadyValue: return "ready-value";
      case Wp7UiComponent::ReadyUnit: return "ready-unit";
      case Wp7UiComponent::ReadyPlus: return "ready-plus";
      case Wp7UiComponent::ActiveTop: return "active-top";
      case Wp7UiComponent::Lane1Static: return "lane1-static";
      case Wp7UiComponent::Lane1Metrics: return "lane1-metrics";
      case Wp7UiComponent::Lane1Target: return "lane1-target";
      case Wp7UiComponent::Lane2Static: return "lane2-static";
      case Wp7UiComponent::Lane2Metrics: return "lane2-metrics";
      case Wp7UiComponent::Lane2Target: return "lane2-target";
      case Wp7UiComponent::BottomMessage: return "bottom-message";
      case Wp7UiComponent::MainButton: return "main-button";
      case Wp7UiComponent::PauseOverlay: return "pause-overlay";
      case Wp7UiComponent::FinishFlag: return "finish-flag";
      case Wp7UiComponent::Count: break;
    }
    return "unknown";
  }
};

struct UiDirtyRegion {
  UiRect rect{};
  uint64_t components = 0;
};

class DirtyRegionSet {
public:
  static constexpr size_t kMaxRegions = 16;

  void clear() {
    count_ = 0;
    mask_ = 0;
    overflowed_ = false;
  }

  bool empty() const { return count_ == 0; }
  size_t count() const { return count_; }
  uint64_t mask() const { return mask_; }
  bool overflowed() const { return overflowed_; }
  const UiDirtyRegion& at(size_t index) const { return regions_[index]; }

  bool has(Wp7UiComponent component) const {
    return (mask_ & uiComponentBit(component)) != 0;
  }

  bool mark(Wp7UiComponent component) {
    const uint64_t bit = uiComponentBit(component);
    if (component == Wp7UiComponent::FullFrame) {
      clear();
      regions_[0] = {Wp7DirtyLayout::fullFrame(), bit};
      count_ = 1;
      mask_ = bit;
      return true;
    }
    return mark(Wp7DirtyLayout::rect(component), bit);
  }

  bool mark(UiRect rect, uint64_t components) {
    if (!rect.valid() || components == 0) return false;
    if (rect.x < 0) {
      rect.width = static_cast<int16_t>(rect.right());
      rect.x = 0;
    }
    if (rect.y < 0) {
      rect.height = static_cast<int16_t>(rect.bottom());
      rect.y = 0;
    }
    if (rect.right() > 800) rect.width = static_cast<int16_t>(800 - rect.x);
    if (rect.bottom() > 480) rect.height = static_cast<int16_t>(480 - rect.y);
    if (!rect.valid()) return false;

    for (size_t i = 0; i < count_; ++i) {
      if (!regions_[i].rect.overlaps(rect)) continue;
      regions_[i].rect = regions_[i].rect.united(rect);
      regions_[i].components |= components;
      mergeOverlapping(i);
      mask_ |= components;
      return true;
    }

    if (count_ >= kMaxRegions) {
      clear();
      overflowed_ = true;
      regions_[0] = {Wp7DirtyLayout::fullFrame(),
                    uiComponentBit(Wp7UiComponent::FullFrame)};
      count_ = 1;
      mask_ = uiComponentBit(Wp7UiComponent::FullFrame);
      return false;
    }
    regions_[count_++] = {rect, components};
    mask_ |= components;
    return true;
  }

private:
  void mergeOverlapping(size_t index) {
    for (size_t i = 0; i < count_;) {
      if (i == index || !regions_[index].rect.overlaps(regions_[i].rect)) {
        ++i;
        continue;
      }
      regions_[index].rect = regions_[index].rect.united(regions_[i].rect);
      regions_[index].components |= regions_[i].components;
      regions_[i] = regions_[count_ - 1];
      --count_;
      if (i < index) --index;
    }
  }

  UiDirtyRegion regions_[kMaxRegions]{};
  size_t count_ = 0;
  uint64_t mask_ = 0;
  bool overflowed_ = false;
};

// Inputs for the ordinary (non-transition) WP7 presentation pass.  This is
// deliberately a small, pure planning boundary: drawCurrent() supplies the
// already-authoritative state/signature comparisons and uses the returned
// DirtyRegionSet to choose the existing drawing helpers.  Keeping this exact
// decision in a host-testable function prevents the renderer from silently
// regressing to broad fallback redraws.
struct Wp7PartialInvalidationInput {
  uint16_t lightColors[5]{};
  uint16_t previousLightColors[5]{};

  bool readyScreen = false;
  bool modeChanged = false;
  bool readyValueChanged = false;
  bool activeTopChanged = false;
  bool bottomMessageChanged = false;

  bool lane1AnimationNow = false;
  bool lane1AnimationPreviouslyActive = false;
  bool lane1StaticChanged = false;
  bool lane1MetricsChanged = false;
  bool lane1TargetChanged = false;

  bool lane2AnimationNow = false;
  bool lane2AnimationPreviouslyActive = false;
  bool lane2StaticChanged = false;
  bool lane2MetricsChanged = false;
  bool lane2TargetChanged = false;

  bool mainButtonChanged = false;
  bool pauseOverlayChanged = false;
  bool finishFlagVisible = false;
};

inline DirtyRegionSet planWp7PartialInvalidation(
    const Wp7PartialInvalidationInput& input) {
  DirtyRegionSet dirty;

  for (uint8_t index = 0; index < 5; ++index) {
    if (input.lightColors[index] != input.previousLightColors[index]) {
      dirty.mark(static_cast<Wp7UiComponent>(
          static_cast<uint8_t>(Wp7UiComponent::Light0) + index));
    }
  }

  if (input.readyScreen) {
    if (input.modeChanged) {
      dirty.mark(Wp7UiComponent::ReadyModeLaps);
      dirty.mark(Wp7UiComponent::ReadyModeTimed);
      dirty.mark(Wp7UiComponent::ReadyUnit);
    }
    if (input.modeChanged || input.readyValueChanged) {
      dirty.mark(Wp7UiComponent::ReadyValue);
    }
  } else if (input.activeTopChanged) {
    dirty.mark(Wp7UiComponent::ActiveTop);
  }

  if (input.lane1AnimationNow || input.lane1AnimationPreviouslyActive ||
      input.lane1StaticChanged) {
    dirty.mark(Wp7UiComponent::Lane1Static);
    dirty.mark(Wp7UiComponent::Lane1Metrics);
    dirty.mark(Wp7UiComponent::Lane1Target);
  } else {
    // The lap/progress cell and LAST/BEST/RACE TIME are independent visible
    // fields.  A lap event may change both, but must never repaint the whole
    // metric band while leaving the lap cell stale.
    if (input.lane1TargetChanged) {
      dirty.mark(Wp7UiComponent::Lane1Target);
    }
    if (input.lane1MetricsChanged) {
      dirty.mark(Wp7UiComponent::Lane1Metrics);
    }
  }

  if (input.lane2AnimationNow || input.lane2AnimationPreviouslyActive ||
      input.lane2StaticChanged) {
    dirty.mark(Wp7UiComponent::Lane2Static);
    dirty.mark(Wp7UiComponent::Lane2Metrics);
    dirty.mark(Wp7UiComponent::Lane2Target);
  } else {
    if (input.lane2TargetChanged) {
      dirty.mark(Wp7UiComponent::Lane2Target);
    }
    if (input.lane2MetricsChanged) {
      dirty.mark(Wp7UiComponent::Lane2Metrics);
    }
  }

  if (input.bottomMessageChanged) dirty.mark(Wp7UiComponent::BottomMessage);
  if (input.mainButtonChanged) dirty.mark(Wp7UiComponent::MainButton);
  if (input.pauseOverlayChanged) dirty.mark(Wp7UiComponent::PauseOverlay);
  if (input.finishFlagVisible) dirty.mark(Wp7UiComponent::FinishFlag);
  return dirty;
}

}  // namespace pp
