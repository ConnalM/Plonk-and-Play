#pragma once

// The ESP32-8048S070 RGB panel is a board surface, not a second source of
// product authority.  This small adapter only acknowledges boot readiness;
// P&P state remains authoritative in the existing controller/browser paths.

#if defined(PP_PHASE8B_REAL_SD)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

namespace pp {

class Esp32S3BoardDisplay final {
public:
  bool begin() {
    // Match the stable Phase 8B probe: allow the board's display power and
    // RGB panel signals to settle before enabling the continuous pixel clock.
    delay(300);
    displayBus_ = new Arduino_ESP32RGBPanel(
        kTftDe, kTftVsync, kTftHsync, kTftPclk,
        kTftR0, kTftR1, kTftR2, kTftR3, kTftR4,
        kTftG0, kTftG1, kTftG2, kTftG3, kTftG4, kTftG5,
        kTftB0, kTftB1, kTftB2, kTftB3, kTftB4,
        0, 210, 30, 16, 0, 22, 13, 10, 1, 16000000);
    display_ = new Arduino_RGB_Display(
        kDisplayWidth, kDisplayHeight, displayBus_, 0, true, nullptr,
        kBacklight);
    if (display_ == nullptr || !display_->begin()) {
      Serial.println("[DISPLAY] ESP32-8048S070 init FAILED");
      return false;
    }
    display_->setRotation(0);
    initialized_ = true;
    showMessage("P&P BOOTING...", "PLEASE WAIT");
    Serial.println("[DISPLAY] ESP32-8048S070 init OK");
    return true;
  }

  void showReady() {
    if (!initialized_) return;
    showMessage("P&P READY", "READY FOR WORK");
    Serial.println("[DISPLAY] P&P READY FOR WORK");
  }

private:
  void showMessage(const char* first, const char* second) {
    display_->fillScreen(0x0000);
    display_->setTextColor(0xFFFF);
    display_->setTextSize(4);
    display_->setCursor(80, 175);
    display_->println(first);
    display_->setTextSize(3);
    display_->setCursor(80, 245);
    display_->println(second);
  }

  // ESP32-8048S070C RGB panel wiring proven by the Phase 8B display probe.
  static constexpr int kTftDe = 41;
  static constexpr int kTftVsync = 40;
  static constexpr int kTftHsync = 39;
  static constexpr int kTftPclk = 42;
  static constexpr int kTftR0 = 14;
  static constexpr int kTftR1 = 21;
  static constexpr int kTftR2 = 47;
  static constexpr int kTftR3 = 48;
  static constexpr int kTftR4 = 45;
  static constexpr int kTftG0 = 9;
  static constexpr int kTftG1 = 46;
  static constexpr int kTftG2 = 3;
  static constexpr int kTftG3 = 8;
  static constexpr int kTftG4 = 16;
  static constexpr int kTftG5 = 1;
  static constexpr int kTftB0 = 15;
  static constexpr int kTftB1 = 7;
  static constexpr int kTftB2 = 6;
  static constexpr int kTftB3 = 5;
  static constexpr int kTftB4 = 4;
  static constexpr int kBacklight = 2;
  static constexpr int kDisplayWidth = 800;
  static constexpr int kDisplayHeight = 480;

  Arduino_ESP32RGBPanel* displayBus_ = nullptr;
  Arduino_RGB_Display* display_ = nullptr;
  bool initialized_ = false;
};

}  // namespace pp

#endif
