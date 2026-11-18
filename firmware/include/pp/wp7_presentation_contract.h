#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "core.h"
#include "race_control.h"

namespace pp {

// Pure presentation helpers shared by the physical renderer and host-side
// contract tests.  They do not own race state or persistence.
inline void formatVisibleLapCell(char* out, size_t capacity,
                                 SessionMode mode, bool hasEntry,
                                 uint32_t laps, uint32_t target) {
  if (!out || capacity == 0) return;
  if (!hasEntry) {
    out[0] = '\0';
    return;
  }
  const uint32_t safeTarget = target ? target : 10U;
  if (mode == SessionMode::Endurance) {
    snprintf(out, capacity, "%lu", static_cast<unsigned long>(laps));
  } else {
    snprintf(out, capacity, "%lu/%lu",
             static_cast<unsigned long>(laps),
             static_cast<unsigned long>(safeTarget));
  }
}

inline bool welcomeCanLeave(bool displayReady, bool touchReady,
                            bool controllerReady) {
  // The splash gate is an operational gate, not the race-start gate.  The
  // Taster must reach READY so that local setup can be edited even when the
  // selected setup is not yet startable (for example, a required detector is
  // missing).  Race Control remains the authority for whether START may be
  // accepted.
  return displayReady && touchReady && controllerReady;
}

inline bool welcomeCanLeaveAfterStartup(bool startupComplete,
                                        bool displayReady,
                                        bool touchReady,
                                        bool controllerReady) {
  // Startup completion is deliberately separate from the START guard.  This
  // only prevents the splash from being replaced before the local controller
  // has completed its bootstrap work; Race Control still decides whether a
  // race may be started.
  return startupComplete &&
         welcomeCanLeave(displayReady, touchReady, controllerReady);
}

inline bool welcomeCanLeaveAfterMinimum(bool startupComplete,
                                        bool displayReady,
                                        bool touchReady,
                                        bool controllerReady,
                                        bool firstFramePresented,
                                        uint32_t firstFrameMs,
                                        uint32_t nowMs,
                                        uint32_t minimumMs = 3000U) {
  if (!welcomeCanLeaveAfterStartup(startupComplete, displayReady, touchReady,
                                   controllerReady) ||
      !firstFramePresented) {
    return false;
  }
  return static_cast<uint32_t>(nowMs - firstFrameMs) >= minimumMs;
}

inline const char* welcomeCustomerStatus(bool startupFailed) {
  return startupFailed ? "STARTUP FAILED" : "STARTING P&P...";
}

// READY renders the locally selected controls while a setup request is
// pending. Once Race Control acknowledges the same values, the visible setup
// key is unchanged and must not trigger another presentation. Active screens
// render the authoritative session values.
inline SessionMode visibleSetupMode(SessionLifecycle lifecycle,
                                    SessionMode authoritativeMode,
                                    SessionMode selectedMode) {
  return lifecycle == SessionLifecycle::Ready ? selectedMode
                                               : authoritativeMode;
}

inline uint32_t visibleSetupValue(SessionLifecycle lifecycle,
                                  SessionMode authoritativeMode,
                                  uint32_t authoritativeLapTarget,
                                  uint16_t authoritativeDurationMinutes,
                                  uint32_t selectedValue) {
  if (lifecycle == SessionLifecycle::Ready) return selectedValue;
  return authoritativeMode == SessionMode::Endurance
             ? static_cast<uint32_t>(authoritativeDurationMinutes)
             : authoritativeLapTarget;
}

inline bool startButtonVisualChanged(bool pressed, bool previousPressed,
                                     bool enabled, bool previousEnabled) {
  return pressed != previousPressed || enabled != previousEnabled;
}

// The physical Taster uses these fixed presentation conventions for live
// clocks.  The value is still authoritative microseconds; this helper only
// formats it for the client and deliberately lets the minute field grow past
// two digits when required.
inline void formatClockMinutesSeconds(Time value, char* out, size_t capacity) {
  if (!out || capacity == 0) return;
  const uint64_t totalSeconds = value / 1000000ULL;
  const uint64_t minutes = totalSeconds / 60ULL;
  const uint64_t seconds = totalSeconds % 60ULL;
  snprintf(out, capacity, "%02llu:%02llu",
           static_cast<unsigned long long>(minutes),
           static_cast<unsigned long long>(seconds));
}

inline void formatDurationMinutesSeconds(uint32_t minutes, char* out,
                                         size_t capacity) {
  if (!out || capacity == 0) return;
  const uint64_t totalSeconds = static_cast<uint64_t>(minutes) * 60ULL;
  const uint64_t wholeMinutes = totalSeconds / 60ULL;
  const uint64_t seconds = totalSeconds % 60ULL;
  snprintf(out, capacity, "%02llu:%02llu",
           static_cast<unsigned long long>(wholeMinutes),
           static_cast<unsigned long long>(seconds));
}

// The active race heading occupies the far-left heading position; configured
// targets belong only on the READY setup controls and lane counters.
constexpr int kTasterHeadingLabelX = 35;

inline const char* tasterRaceHeading(SessionMode mode) {
  return mode == SessionMode::Endurance ? "ENDURANCE" : "LAPS";
}

// Base RGB565 colours for the physical racing signal lamps.  Keep these
// separate from the dashboard lane colours so red/green remain unambiguous.
constexpr uint16_t kRacingSignalRed = 0xF800U;
constexpr uint16_t kRacingSignalGreen = 0x07E0U;
constexpr uint16_t kRacingSignalOff = 0x18C3U;

constexpr uint16_t racingSignalLampColor(bool green, uint8_t redLightsLit,
                                         int index) {
  return green ? kRacingSignalGreen
               : (index >= 0 && index < redLightsLit ? kRacingSignalRed
                                                      : kRacingSignalOff);
}

// Four changes per second after the initial hold delay, accelerating in
// bounded steps to ten changes per second.  Keeping this policy pure makes it
// deterministic in host-side contract tests and independent of GT911 timing.
inline uint32_t adjustmentRepeatIntervalMs(uint32_t completedRepeats) {
  constexpr uint32_t initial = 250U;
  constexpr uint32_t minimum = 100U;
  constexpr uint32_t acceleration = 25U;
  const uint32_t reduction = completedRepeats * acceleration;
  return reduction >= initial - minimum ? minimum : initial - reduction;
}

// The GT911 occasionally reports a complete but empty sample while a finger
// remains on the glass.  The physical trace recorded a 201 ms worst loop;
// require 250 ms of consecutive empty samples before treating an adjustment
// hold as released.  During that window adjustments are intentionally paused.
constexpr uint32_t kAdjustmentReleaseConfirmationMs = 250U;
constexpr uint32_t kAdjustmentTenStepAfterMs = 3000U;
constexpr uint32_t kAdjustmentTenStepIntervalMs = 100U;
constexpr uint32_t kTouchHoldThresholdMs = 700U;

constexpr bool touchHoldThresholdReached(uint32_t heldMs) {
  return heldMs >= kTouchHoldThresholdMs;
}

constexpr bool pauseHoldFeedbackActive(SessionLifecycle lifecycle,
                                       bool mainTarget, uint32_t heldMs) {
  return lifecycle == SessionLifecycle::Racing && mainTarget &&
         touchHoldThresholdReached(heldMs);
}

constexpr bool adjustmentReleaseConfirmed(uint32_t missingSinceMs,
                                         uint32_t nowMs) {
  return nowMs - missingSinceMs >= kAdjustmentReleaseConfirmationMs;
}

constexpr int adjustmentHoldDelta(int direction, uint32_t heldMs) {
  return heldMs >= kAdjustmentTenStepAfterMs ? direction * 10 : direction;
}

// Advance a periodic deadline past now without emitting the missed repeats.
// This keeps the cadence anchored to its original schedule while ensuring a
// contact gap can never produce a catch-up burst when contact returns.
constexpr uint32_t adjustmentDeadlineAfter(uint32_t deadlineMs,
                                           uint32_t intervalMs,
                                           uint32_t nowMs) {
  if (static_cast<int32_t>(nowMs - deadlineMs) < 0) return deadlineMs;
  return deadlineMs + (((nowMs - deadlineMs) / intervalMs) + 1U) * intervalMs;
}

#if defined(PP_WP7_DISABLE_PAUSED_WARNING_LIGHTS)
constexpr bool pausedWarningLightsEnabled() { return false; }
#else
constexpr bool pausedWarningLightsEnabled() { return true; }
#endif

inline uint8_t pausedWarningPhase(uint32_t elapsedMs) {
#if defined(PP_WP7_DISABLE_PAUSED_WARNING_LIGHTS)
  (void)elapsedMs;
  return 0;
#else
  return static_cast<uint8_t>((elapsedMs / 500U) & 1U);
#endif
}

constexpr bool pausedWarningPhaseChanged(uint8_t previousPhase,
                                          uint8_t currentPhase) {
  return pausedWarningLightsEnabled() && previousPhase != currentPhase;
}

inline bool pausedWarningLampLit(uint8_t lampIndex, uint8_t phase) {
  // Phase 0: Y O Y O Y; phase 1: O Y O Y O.
  return (lampIndex & 1U) == (phase & 1U);
}

}  // namespace pp
