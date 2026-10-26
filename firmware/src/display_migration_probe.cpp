#if defined(PP_DISPLAY_MIGRATION_PROBE)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_idf_version.h>

#include "pp/build_identity_generated.h"

namespace {

constexpr int kTftDe = 41;
constexpr int kTftVsync = 40;
constexpr int kTftHsync = 39;
constexpr int kTftPclk = 42;
constexpr int kTftR0 = 14;
constexpr int kTftR1 = 21;
constexpr int kTftR2 = 47;
constexpr int kTftR3 = 48;
constexpr int kTftR4 = 45;
constexpr int kTftG0 = 9;
constexpr int kTftG1 = 46;
constexpr int kTftG2 = 3;
constexpr int kTftG3 = 8;
constexpr int kTftG4 = 16;
constexpr int kTftG5 = 1;
constexpr int kTftB0 = 15;
constexpr int kTftB1 = 7;
constexpr int kTftB2 = 6;
constexpr int kTftB3 = 5;
constexpr int kTftB4 = 4;
constexpr int kDisplayWidth = 800;
constexpr int kDisplayHeight = 480;
constexpr int kBacklight = 2;

Arduino_ESP32RGBPanel* displayBus = nullptr;
Arduino_RGB_Display* display = nullptr;

void drawProbePattern() {
  display->fillScreen(0x0000);

  // Full-height and full-width blocks make horizontal duplication or phase
  // displacement visible without animation or repeated redraws.
  display->fillRect(24, 24, 96, 96, 0xF800);
  display->fillRect(144, 24, 96, 96, 0x07E0);
  display->fillRect(264, 24, 96, 96, 0x001F);
  display->fillRect(384, 24, 96, 96, 0xFFE0);
  display->fillRect(504, 24, 96, 96, 0xF81F);
  display->fillRect(624, 24, 96, 96, 0x07FF);

  display->drawFastVLine(80, 150, 250, 0xFFFF);
  display->drawFastVLine(200, 150, 250, 0xFFFF);
  display->drawFastVLine(320, 150, 250, 0xFFFF);
  display->drawFastVLine(440, 150, 250, 0xFFFF);
  display->drawFastVLine(560, 150, 250, 0xFFFF);
  display->drawFastVLine(680, 150, 250, 0xFFFF);
  display->drawFastHLine(24, 150, 696, 0xFFFF);
  display->drawFastHLine(24, 275, 696, 0xFFFF);
  display->drawFastHLine(24, 400, 696, 0xFFFF);

  display->setTextColor(0xFFFF);
  display->setTextSize(3);
  display->setCursor(48, 180);
  display->println("P&P DISPLAY PROBE");
  display->setTextSize(2);
  display->setCursor(48, 315);
  display->println("S3 / Arduino_GFX / RGB");
  display->setCursor(48, 345);
  display->println("STATIC 800x480 12MHz");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 3000U) {
    delay(10);
  }
  delay(300);

  Serial.printf("[DISPLAY_PROBE] source_commit=%s environment=%s\n",
                PP_BUILD_SOURCE_COMMIT, PP_BUILD_ENVIRONMENT);
  Serial.printf("[DISPLAY_PROBE] framework=Arduino-ESP32-3.3.12 idf=%s gfx=1.6.5\n",
                esp_get_idf_version());
  Serial.println("[DISPLAY_PROBE] init begin; no P&P subsystems, LVGL or repeated redraw");

  displayBus = new Arduino_ESP32RGBPanel(
      kTftDe, kTftVsync, kTftHsync, kTftPclk,
      kTftR0, kTftR1, kTftR2, kTftR3, kTftR4,
      kTftG0, kTftG1, kTftG2, kTftG3, kTftG4, kTftG5,
      kTftB0, kTftB1, kTftB2, kTftB3, kTftB4,
      0, 210, 30, 16,
      0, 22, 13, 10,
      1, 12000000);
  display = new Arduino_RGB_Display(
      kDisplayWidth, kDisplayHeight, displayBus, 0, true, nullptr, kBacklight);

  if (display == nullptr || !display->begin()) {
    Serial.println("[DISPLAY_PROBE] init FAILED");
    return;
  }
  display->setRotation(0);
  Serial.println("[DISPLAY_PROBE] init OK; drawing one static pattern");
  drawProbePattern();
  Serial.println("[DISPLAY_PROBE] draw complete; no further display writes");
}

void loop() {
  delay(1000);
}

#endif
