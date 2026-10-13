#pragma once

#if defined(PP_STAGE8A_TASTER_WOKWI)

#include <Arduino.h>
#include <HardwareSerial.h>
#include "taster_browser_adapter.h"

namespace pp {

// Phase 8A Wokwi runtime: a thin physical-control/display client over the
// existing Browser Interface. No race state is cached here.
class TasterRuntime {
public:
  explicit TasterRuntime(BrowserInterface& browser)
      : authority_(browser, 0x544153544552ULL), surface_(authority_) {}

  void begin() {
    surface_.sync();
    pinMode(5, INPUT_PULLUP);
    pinMode(6, INPUT_PULLUP);
    pinMode(7, INPUT_PULLUP);
    lastClock_ = digitalRead(5);
    Serial1.begin(115200, SERIAL_8N1, -1, 17);
    render(true);
  }

  void tick() {
    serviceEncoder();
    serviceButton();
    surface_.service();
    render(false);
  }

private:
  static const char* modeName(SessionMode mode) {
    return mode == SessionMode::Endurance ? "TIMED" : "LAPS";
  }

  static const char* lifecycleName(SessionLifecycle value) {
    switch (value) {
      case SessionLifecycle::Ready: return "READY";
      case SessionLifecycle::Starting: return "STARTING";
      case SessionLifecycle::Racing: return "RACING";
      case SessionLifecycle::Paused: return "PAUSED";
      case SessionLifecycle::Finished: return "FINISHED";
      default: return "READY";
    }
  }

  void serviceEncoder() {
    const int clock = digitalRead(5);
    if (clock == lastClock_) return;
    if (clock == HIGH) {
      const int delta = digitalRead(6) == LOW ? 1 : -1;
      if (decisionPrompt_) decisionAbandon_ = delta > 0;
      else surface_.adjust(delta);
    }
    lastClock_ = clock;
  }

  void serviceButton() {
    const bool pressed = digitalRead(7) == LOW;
    const uint32_t now = millis();
    if (pressed && !buttonDown_) { buttonDown_ = true; buttonAt_ = now; return; }
    if (pressed || !buttonDown_) return;
    buttonDown_ = false;
    const bool longPress = static_cast<uint32_t>(now - buttonAt_) >= 700U;
    const SessionLifecycle lifecycle = surface_.snapshot().state.lifecycle;
    if (decisionPrompt_) {
      if (longPress) surface_.restart(true);
      else if (decisionAbandon_) surface_.abandon(true);
      else surface_.operation(SessionOperation::Resume);
      decisionPrompt_ = false;
      return;
    }
    if (lifecycle == SessionLifecycle::Ready && !longPress) surface_.start();
    else if (lifecycle == SessionLifecycle::Ready && longPress) surface_.toggleMode();
    else if (lifecycle == SessionLifecycle::Finished && !longPress) surface_.operation(SessionOperation::RaceAgain);
    else if (lifecycle == SessionLifecycle::Paused && !longPress) surface_.operation(SessionOperation::Resume);
    else if (lifecycle == SessionLifecycle::Paused && longPress) { decisionPrompt_ = true; }
    else if (lifecycle == SessionLifecycle::Racing && longPress) {
      if (surface_.operation(SessionOperation::Pause)) decisionPrompt_ = true;
    }
  }

  void render(bool force) {
    const TasterSnapshot& snapshot = surface_.snapshot();
    uint32_t liveMarker = snapshot.state.entryCount;
    for (uint8_t i = 0; i < snapshot.state.entryCount; ++i) liveMarker += snapshot.state.entries[i].laps * 31U;
    const uint32_t marker = static_cast<uint32_t>(snapshot.state.lifecycle) * 100000U +
                            snapshot.proposalRevision * 10U + liveMarker;
    if (!force && marker == lastMarker_) return;
    lastMarker_ = marker;
    const uint32_t value = surface_.selectedValue();
    const char* mode = modeName(surface_.selectedMode());
    const char* state = lifecycleName(snapshot.state.lifecycle);
    char line[160]{};
    if (decisionPrompt_) {
      snprintf(line, sizeof(line), "PROMPT|%s|%s|%lu|LANE 1|LANE 2\n",
               decisionAbandon_ ? "ABANDON" : "RESUME", mode,
               static_cast<unsigned long>(value));
    } else if (snapshot.state.lifecycle == SessionLifecycle::Starting) {
      snprintf(line, sizeof(line), "START|%u|%s|%lu|LANE 1|LANE 2\n",
               unsigned(snapshot.state.redLightCount ? snapshot.state.redLightCount : 5), mode,
               static_cast<unsigned long>(value));
    } else if (snapshot.state.lifecycle == SessionLifecycle::Racing ||
               snapshot.state.lifecycle == SessionLifecycle::Paused ||
               snapshot.state.lifecycle == SessionLifecycle::Finished) {
      snprintf(line, sizeof(line), "%s|%s|%lu|LANE 1|LANE 2\n", state, mode,
               static_cast<unsigned long>(value));
    } else {
      snprintf(line, sizeof(line), "READY|%s|%lu|LANE 1|LANE 2\n", mode,
               static_cast<unsigned long>(value));
    }
    Serial1.print(line);
  }

  BrowserTasterAuthority authority_;
  TasterSurfaceModel surface_;
  int lastClock_ = HIGH;
  bool buttonDown_ = false;
  uint32_t buttonAt_ = 0;
  bool decisionPrompt_ = false;
  bool decisionAbandon_ = false;
  uint32_t lastMarker_ = UINT32_MAX;
};

} // namespace pp

#endif
