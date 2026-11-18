#pragma once

#if defined(PP_STAGE8A_TASTER_WOKWI)

#include <Arduino.h>
#include <HardwareSerial.h>
#include <stdio.h>
#include <string.h>
#include "taster_browser_adapter.h"

namespace pp {

// Wokwi's custom display chip is deliberately a presentation-only device.
// These momentary inputs are the simulation's touch-zone proxy; all resulting
// actions still cross the normal Taster -> Browser Interface -> Race Control
// authority boundary.
class TasterRuntime {
public:
  using LaneStimulus = void (*)(uint8_t lane, bool active);

  explicit TasterRuntime(BrowserInterface& browser, LaneStimulus laneStimulus = nullptr)
      : authority_(browser, 0x544153544552ULL), surface_(authority_), laneStimulus_(laneStimulus) {}

  void begin() {
    for (const auto& button : buttons_) pinMode(button.pin, INPUT_PULLUP);
    Serial1.begin(115200, SERIAL_8N1, -1, 17);
    surface_.sync();
    render(true);
  }

  void tick() {
    serviceButtons();
    serviceLaneStimulus();
    surface_.service();
    render(false);
  }

private:
  enum class TouchAction : uint8_t {
    Mode,
    Decrease,
    Increase,
    Start,
    Interrupt,
    Resume,
    Abandon,
    RaceAgain,
    Lane1,
    Lane2,
  };

  struct TouchButton {
    uint8_t pin;
    TouchAction action;
    bool down;
    uint32_t pressedAt;
  };

  // The diagram maps these to labelled Wokwi pushbuttons. The input names
  // deliberately describe touch zones rather than rotary-encoder mechanics.
  TouchButton buttons_[10]{
      {5, TouchAction::Mode, false, 0},
      {6, TouchAction::Decrease, false, 0},
      {7, TouchAction::Increase, false, 0},
      {8, TouchAction::Start, false, 0},
      {9, TouchAction::Interrupt, false, 0},
      {10, TouchAction::Resume, false, 0},
      {11, TouchAction::Abandon, false, 0},
      {12, TouchAction::RaceAgain, false, 0},
      {13, TouchAction::Lane1, false, 0},
      {14, TouchAction::Lane2, false, 0},
  };

  static const char* modeName(SessionMode mode) {
    return mode == SessionMode::Endurance ? "TIMED" : "LAPS";
  }

  static void copyDisplayName(char* out, size_t capacity,
                              const char* source, uint8_t lane,
                              uint32_t mugId) {
    if (!out || capacity == 0) return;
    // Built-in identities are intentionally presented as lane defaults. A
    // saved MUG's catalog name remains authoritative and replaces the default
    // rather than being shown alongside it.
    if (mugId == 1 || mugId == 2 || !source || !source[0]) {
      snprintf(out, capacity, "Lane %u", static_cast<unsigned>(lane));
    } else {
      strncpy(out, source, capacity - 1);
      out[capacity - 1] = '\0';
    }
    // The UART protocol is pipe-delimited. Keep the display safe even if a
    // saved profile contains a separator or line break.
    for (size_t i = 0; out[i]; ++i) {
      if (out[i] == '|' || out[i] == '\r' || out[i] == '\n') out[i] = ' ';
    }
  }

  static void formatTime(Time value, char* out, size_t capacity) {
    if (!value) {
      snprintf(out, capacity, "--");
      return;
    }
    snprintf(out, capacity, "%.2f", static_cast<double>(value) / 1000000.0);
  }

  static void formatGap(const NoticeboardEntryState& entry, char* out, size_t capacity) {
    if (!entry.gapTime) {
      snprintf(out, capacity, "-");
      return;
    }
    snprintf(out, capacity, "+%.2f", static_cast<double>(entry.gapTime) / 1000000.0);
  }

  void serviceButtons() {
    const uint32_t now = millis();
    for (auto& button : buttons_) {
      const bool pressed = digitalRead(button.pin) == LOW;
      if (pressed && !button.down) {
        button.down = true;
        button.pressedAt = now;
        if (button.action == TouchAction::Lane1) triggerLane(1);
        else if (button.action == TouchAction::Lane2) triggerLane(2);
        continue;
      }
      if (!pressed && button.down) {
        button.down = false;
        handleRelease(button.action, static_cast<uint32_t>(now - button.pressedAt));
      }
    }
  }

  void handleRelease(TouchAction action, uint32_t heldMs) {
    const SessionLifecycle lifecycle = surface_.snapshot().state.lifecycle;
    if (action == TouchAction::Lane1 || action == TouchAction::Lane2) return;
    if (action == TouchAction::Interrupt) {
      // The interruption control is intentionally inert for short touches and
      // is recognised only on release after the agreed 700 ms hold.
      if (heldMs >= 700U && lifecycle == SessionLifecycle::Racing) {
        surface_.operation(SessionOperation::Pause);
      }
      return;
    }
    if (action == TouchAction::Mode) {
      if (lifecycle == SessionLifecycle::Ready) surface_.toggleMode();
      return;
    }
    if (action == TouchAction::Decrease) {
      if (lifecycle == SessionLifecycle::Ready) surface_.adjust(-1);
      return;
    }
    if (action == TouchAction::Increase) {
      if (lifecycle == SessionLifecycle::Ready) surface_.adjust(1);
      return;
    }
    if (action == TouchAction::Start) {
      if (lifecycle == SessionLifecycle::Ready) surface_.start();
      return;
    }
    if (action == TouchAction::Resume) {
      if (abandonPrompt_) {
        abandonPrompt_ = false;
      } else if (lifecycle == SessionLifecycle::Paused) {
        surface_.operation(SessionOperation::Resume);
      }
      return;
    }
    if (action == TouchAction::Abandon) {
      if (lifecycle != SessionLifecycle::Paused && !abandonPrompt_) return;
      if (!abandonPrompt_) {
        abandonPrompt_ = true;
      } else {
        surface_.abandon(true);
        abandonPrompt_ = false;
      }
      return;
    }
    if (action == TouchAction::RaceAgain && lifecycle == SessionLifecycle::Finished) {
      surface_.operation(SessionOperation::RaceAgain);
    }
  }

  void triggerLane(uint8_t lane) {
    if (!laneStimulus_ || lane < 1 || lane > 2 ||
        surface_.snapshot().state.lifecycle != SessionLifecycle::Racing) return;
    laneStimulus_(lane, true);
    laneReleaseAt_[lane - 1] = millis() + 60U;
  }

  void serviceLaneStimulus() {
    if (!laneStimulus_) return;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 2; ++i) {
      if (laneReleaseAt_[i] != 0 && static_cast<int32_t>(now - laneReleaseAt_[i]) >= 0) {
        laneStimulus_(static_cast<uint8_t>(i + 1), false);
        laneReleaseAt_[i] = 0;
      }
    }
  }

  static uint32_t markerFor(const TasterSnapshot& snapshot) {
    uint32_t marker = static_cast<uint32_t>(snapshot.state.lifecycle) * 100000U;
    marker ^= snapshot.proposalRevision * 31U;
    marker ^= static_cast<uint32_t>(snapshot.state.redLightsLit) << 24;
    marker ^= snapshot.state.startLightsGreen ? 0x00800000U : 0U;
    for (uint8_t i = 0; i < snapshot.state.entryCount; ++i) {
      marker ^= snapshot.state.entries[i].laps * (31U + i * 7U);
      marker ^= static_cast<uint32_t>(snapshot.state.entries[i].lastLapTime);
    }
    return marker;
  }

  void render(bool force) {
    const TasterSnapshot& snapshot = surface_.snapshot();
    const uint32_t marker = markerFor(snapshot);
    const uint32_t now = millis();
    const auto lifecycle = snapshot.state.lifecycle;
    const uint32_t cadence = lifecycle == SessionLifecycle::Paused ? 1000U :
                             lifecycle == SessionLifecycle::Finished ? 500U :
                             lifecycle == SessionLifecycle::Racing ? 250U :
                             lifecycle == SessionLifecycle::Starting ? 100U : 0U;
    if (!force && marker == lastMarker_ && abandonPrompt_ == lastPrompt_ &&
        (!cadence || static_cast<uint32_t>(now - lastRenderAt_) < cadence)) return;
    lastMarker_ = marker;
    lastPrompt_ = abandonPrompt_;
    lastRenderAt_ = now;

    char racer1[48]{}, racer2[48]{};
    copyDisplayName(racer1, sizeof(racer1), snapshot.proposal.entries[0].mugName, 1,
                    snapshot.proposal.entries[0].mugId);
    copyDisplayName(racer2, sizeof(racer2), snapshot.proposal.entries[1].mugName, 2,
                    snapshot.proposal.entries[1].mugId);

    const auto& entry1 = snapshot.state.entries[0];
    const auto& entry2 = snapshot.state.entries[1];
    char last1[16]{}, last2[16]{}, best1[16]{}, best2[16]{}, gap1[16]{}, gap2[16]{};
    formatTime(entry1.lastLapTime, last1, sizeof(last1));
    formatTime(entry2.lastLapTime, last2, sizeof(last2));
    formatTime(entry1.bestLapTime, best1, sizeof(best1));
    formatTime(entry2.bestLapTime, best2, sizeof(best2));
    formatGap(entry1, gap1, sizeof(gap1));
    formatGap(entry2, gap2, sizeof(gap2));

    const char* mode = modeName(surface_.selectedMode());
    const uint32_t value = surface_.selectedValue();
    const char* state = lifecycle == SessionLifecycle::Starting ? "START" :
                        lifecycle == SessionLifecycle::Racing ? "RACING" :
                        lifecycle == SessionLifecycle::Paused ? "PAUSED" :
                        lifecycle == SessionLifecycle::Finished ? "FINISHED" : "READY";
    const char* message = lifecycle == SessionLifecycle::Ready ? "TOUCH START TO BEGIN" :
                          lifecycle == SessionLifecycle::Starting ? "COUNTDOWN" :
                          lifecycle == SessionLifecycle::Racing ? "HOLD PAUSE 700MS" :
                          lifecycle == SessionLifecycle::Paused ? "TOUCH RESUME OR ABANDON" :
                          lifecycle == SessionLifecycle::Finished ? "TOUCH RACE AGAIN FOR READY" : "";

    char line[512]{};
    if (abandonPrompt_) {
      snprintf(line, sizeof(line),
               "PROMPT|ABANDON|%s|%lu|%s|%s|%lu|%lu|%s|%s|%s|%s|%s|%s|%lu|%lu|%u|%u|%u|TOUCH ABANDON AGAIN TO CONFIRM\n",
               mode, static_cast<unsigned long>(value), racer1, racer2,
               static_cast<unsigned long>(entry1.laps), static_cast<unsigned long>(entry2.laps),
               last1, last2, best1, best2, gap1, gap2,
               static_cast<unsigned long>(entry1.position),
               static_cast<unsigned long>(entry2.position),
               0U, 0U, 0U);
    } else {
      snprintf(line, sizeof(line),
               "%s|%s|%lu|%s|%s|%lu|%lu|%s|%s|%s|%s|%s|%s|%lu|%lu|%u|%u|%u|%s\n",
               state, mode, static_cast<unsigned long>(value), racer1, racer2,
               static_cast<unsigned long>(entry1.laps), static_cast<unsigned long>(entry2.laps),
               last1, last2, best1, best2, gap1, gap2,
               static_cast<unsigned long>(entry1.position),
               static_cast<unsigned long>(entry2.position),
               snapshot.state.resultSealed ? 1U : 0U,
               static_cast<unsigned>(snapshot.state.redLightsLit),
               snapshot.state.startLightsGreen ? 1U : 0U, message);
    }
    Serial1.print(line);
  }

  BrowserTasterAuthority authority_;
  TasterSurfaceModel surface_;
  LaneStimulus laneStimulus_ = nullptr;
  uint32_t laneReleaseAt_[2]{};
  uint32_t lastMarker_ = UINT32_MAX;
  uint32_t lastRenderAt_ = 0;
  bool abandonPrompt_ = false;
  bool lastPrompt_ = false;
};

} // namespace pp

#endif
