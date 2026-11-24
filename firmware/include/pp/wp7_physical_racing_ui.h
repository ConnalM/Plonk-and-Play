#pragma once

#include <Arduino.h>
#include <cstring>
#include <Wire.h>
#include <TAMC_GT911.h>

#include "board_display_esp32s3.h"
#include "core.h"
#include "system_time.h"
#include "physical_taster_authority.h"
#include "ui_dirty_regions.h"
#include "wp7_presentation_contract.h"
#include "wp7_barlow_bold_28.h"
#include "wp7_barlow_bold_40.h"
#include "wp7_barlow_semibold_24.h"
#include "wp7_barlow_semibold_30.h"
#include "wp7_ready_artwork.h"

namespace pp {

#if defined(PP_WP7_STATIC_WELCOME_RASTER)
// The XIP build embeds the approved 800x480 RGB565 raster in flash.  The
// symbols are supplied by target_add_binary_data() in the opt-in IDF target;
// no second framebuffer or PSRAM asset copy is created.
extern const uint8_t wp7WelcomeRasterStart[]
    asm("_binary_wp7_welcome_static_rgb565_bin_start");
extern const uint8_t wp7WelcomeRasterEnd[]
    asm("_binary_wp7_welcome_static_rgb565_bin_end");
extern const uint8_t wp7LampRedStart[]
    asm("_binary_lamp_red_rgb565_start");
extern const uint8_t wp7LampGreenStart[]
    asm("_binary_lamp_green_rgb565_start");
extern const uint8_t wp7LampOffStart[]
    asm("_binary_lamp_off_rgb565_start");
extern const uint8_t wp7LampYellowStart[]
    asm("_binary_lamp_yellow_rgb565_start");
extern const uint8_t wp7HeaderSurroundStart[]
    asm("_binary_header_trapezoid_surround_rgb565_start");
extern const uint8_t wp7HeaderLogoStart[]
    asm("_binary_pp_header_logo_rgb565_start");
#endif

// Disposable physical WP7 surface. It is a presentation/control client only:
// Race Control, InputModule and Race Engine remain the authorities.
class Wp7PhysicalRacingUi final {
public:
  Wp7PhysicalRacingUi(Esp32S3BoardDisplay& display, PhysicalTasterAuthority& authority)
      : display_(display), authority_(authority), surface_(authority, 0x544153544552ULL, 0x80000000U),
        touch_(kTouchSda, kTouchScl, kTouchInt, kTouchRst,
               kPanelWidth, kPanelHeight) {}

  uint32_t touchReadErrors() const { return touch_.readErrorCount(); }
  uint32_t touchInvalidSamples() const { return touch_.invalidSampleCount(); }

  void begin() {
    surface_.sync();
    welcomeActive_ = true;
    startupComplete_ = false;
    startupFailed_ = false;
    welcomeFirstPresented_ = false;
    readyPresentedLogged_ = false;
    welcomeDrawn_ = false;
    welcomeFailureDrawn_ = false;
    welcomeProgress_ = 0;
    startupProgress_ = 0;
    pausedWarningStartedMs_ = 0;
    startupStatus_ = "INITIALISING DISPLAY";
    welcomeStatus_ = welcomeCustomerStatus(false);
    drawWelcome(welcomeProgress_, welcomeStatus_);
    Wire.begin(kTouchSda, kTouchScl);
    Wire.setClock(100000);
    detectedAddress_ = 0;
    const uint8_t addresses[] = {0x5D, 0x14};
    for (const uint8_t address : addresses) {
      Wire.beginTransmission(address);
      const bool ack = Wire.endTransmission() == 0;
      Serial.printf("[WP7 PHYS] I2C address=0x%02X ack=%s\n",
                    address, ack ? "YES" : "NO");
      if (ack && detectedAddress_ == 0) detectedAddress_ = address;
    }
    if (detectedAddress_ != 0 && touch_.begin(detectedAddress_)) {
      touch_.setRotation(ROTATION_INVERTED);
      touchReady_ = true;
      Serial.printf("[WP7 PHYS] GT911 READY SDA=%d SCL=%d INT=%d RST=%d address=0x%02X\n",
                    kTouchSda, kTouchScl, kTouchInt, kTouchRst, detectedAddress_);
      Serial.println("[WP7 PHYS] touch polling=20ms hold-release=700ms");
      reportStartupStage(20, "TOUCHSCREEN READY");
    } else {
      startupFailed_ = true;
      Serial.println("[WP7 PHYS] GT911 NOT READY; touchscreen controls disabled");
      reportStartupStage(20, "STARTUP FAILED");
    }
    nextPollMs_ = millis() + kPollIntervalMs;
    nextDrawMs_ = millis();
    nextTouchDiagnosticMs_ = millis() + 1000U;
  }

  void reportStartupStage(uint8_t progress, const char* status) {
    if (!welcomeActive_) return;
    startupProgress_ = progress > 100U ? 100U : progress;
    startupStatus_ = status ? status : "STARTING P&P...";
    Serial.printf("[WP7 PHYS] startup stage t_ms=%lu progress=%u status=%s\n",
                  static_cast<unsigned long>(millis()),
                  static_cast<unsigned>(startupProgress_), startupStatus_);
    const bool failed = std::strcmp(startupStatus_, "STARTUP FAILED") == 0;
    const char* displayStatus = welcomeCustomerStatus(failed);
    const bool statusChanged = !welcomeStatus_ ||
                               std::strcmp(welcomeStatus_, displayStatus) != 0;
    if (welcomeProgress_ != startupProgress_ || statusChanged) {
      welcomeProgress_ = startupProgress_;
      welcomeStatus_ = displayStatus;
      drawWelcome(welcomeProgress_, welcomeStatus_);
    }
  }

  void completeStartup(bool operational) {
    if (!welcomeActive_) return;
    startupComplete_ = operational;
    startupFailed_ = !operational;
    if (!operational) {
      reportStartupStage(startupProgress_, "STARTUP FAILED");
      Serial.printf("[WP7 PHYS] local controller startup FAILED t_ms=%lu\n",
                    static_cast<unsigned long>(millis()));
      return;
    }
    reportStartupStage(100, "STARTING P&P...");
    Serial.printf("[WP7 PHYS] local controller ready t_ms=%lu\n",
                  static_cast<unsigned long>(millis()));
  }

  void tick(Time now) {
    authority_.pollResults();
    surface_.service();
    const uint32_t currentMs = millis();
    if (welcomeActive_) {
      updateWelcome(now, currentMs, false);
      return;
    }
    pollTouch(currentMs);
    if (currentMs >= nextTouchDiagnosticMs_) {
      Serial.printf("[WP8 DIAG] touch polls=%lu i2c_errors=%lu invalid_samples=%lu "
                    "last_status=0x%02X reported_touches=%u last_read=%s\n",
                    static_cast<unsigned long>(touch_.pollCount()),
                    static_cast<unsigned long>(touch_.readErrorCount()),
                    static_cast<unsigned long>(touch_.invalidSampleCount()),
                    static_cast<unsigned>(touch_.lastPointInfo()),
                    static_cast<unsigned>(touch_.lastReportedTouches()),
                    touch_.lastReadOk() ? "OK" : "INVALID");
      nextTouchDiagnosticMs_ = currentMs + 1000U;
    }
    if (static_cast<int32_t>(currentMs - nextDrawMs_) >= 0) {
      drawCurrent(now);
      const uint32_t afterDrawMs = millis();
      const bool animating = kAnimationsEnabled &&
                             (laneAnimationActive(1, afterDrawMs) ||
                              laneAnimationActive(2, afterDrawMs));
      nextDrawMs_ = afterDrawMs +
                    (animating ? kAnimationFrameMs : kDrawIntervalMs);
    }
  }

private:
  enum class FullFrameReason : uint8_t {
    None,
    InitialDraw,
    LifecycleTransition,
    WelcomeTransition,
    DirtyRegionOverflow,
    UnmappedSignatureChange,
  };

  static const char* fullFrameReasonText(FullFrameReason reason) {
    switch (reason) {
      case FullFrameReason::InitialDraw: return "initial-draw";
      case FullFrameReason::LifecycleTransition:
        return "lifecycle-transition";
      case FullFrameReason::WelcomeTransition:
        return "welcome-transition";
      case FullFrameReason::DirtyRegionOverflow:
        return "dirty-region-overflow";
      case FullFrameReason::UnmappedSignatureChange:
        return "unmapped-signature-change";
      case FullFrameReason::None: break;
    }
    return "partial";
  }

#if defined(PP_WP7_DISABLE_ANIMATIONS)
  // Keep the original animation implementation and artwork compiled for
  // future use, but make the current physical target a stable static
  // presentation while animation work is suspended.
  static constexpr bool kAnimationsEnabled = false;
#else
  static constexpr bool kAnimationsEnabled = true;
#endif

  static constexpr int kTouchSda = 19;
  static constexpr int kTouchScl = 20;
  static constexpr int kTouchInt = 17;
  static constexpr int kTouchRst = 18;
  static constexpr int kPanelWidth = 800;
  static constexpr int kPanelHeight = 480;
  static constexpr int kMapX1 = 3;
  static constexpr int kMapX2 = 797;
  static constexpr int kMapY1 = 1;
  static constexpr int kMapY2 = 480;
  static constexpr uint32_t kPollIntervalMs = 20;
  // Non-adjustment controls retain their established release debounce.
  static constexpr uint32_t kReleaseDebounceMs = 80;
  static constexpr uint32_t kDrawIntervalMs = 80;
  static constexpr uint32_t kAnimationFrameMs = 50;
  static constexpr uint32_t kLapAnimationFrames = 14;
  static constexpr uint32_t kFinishAnimationFrames = 18;
  static constexpr int kCarStartX = 52;
  static constexpr int kCarTravelPixels = 500;
  static constexpr uint32_t kAdjustRepeatDelayMs = 450;
  static constexpr uint32_t kAdjustInitialIntervalMs = 250;
  static constexpr uint32_t kWelcomeMinimumMs = 3000;
  static constexpr int kMainButtonX = 246;
  static constexpr int kMainButtonY = 418;
  static constexpr int kMainButtonWidth = 309;
  static constexpr int kMainButtonHeight = 58;
  static constexpr int kReadyModeY = 72;
  static constexpr int kReadyModeWidth = 165;
  static constexpr int kReadyModeHeight = 54;
  static constexpr int kReadyLapsX = 156;
  static constexpr int kReadyTimedX = 332;
  static constexpr int kReadyMinusX = 516;
  static constexpr int kReadyPlusX = 719;
  static constexpr int kReadyStepWidth = 62;
  static constexpr int kReadyStepHeight = 54;
  // The selected value gets its own fixed invalidation cell.  Keeping this
  // separate from the mode buttons prevents a one/two/three digit change from
  // clearing and repainting the whole READY header.
  static constexpr int kReadyValueX = 584;
  static constexpr int kReadyValueY = 72;
  static constexpr int kReadyValueWidth = 82;
  static constexpr int kReadyValueHeight = 54;
  // Bold40's widest digit is 22 px.  These fixed slot centres keep the
  // rightmost glyph inside the 584..665 value field and clear of the unit
  // label, whose restore begins at x=664.
  static constexpr int kReadyValueCenterX = 624;
  static constexpr int kReadyUnitX = 666;
  static constexpr int kReadyUnitY = 72;
  static constexpr int kReadyUnitWidth = 53;
  static constexpr int kReadyUnitHeight = 54;
  static constexpr int kPausePanelX = 112;
  static constexpr int kPausePanelY = 152;
  static constexpr int kPausePanelWidth = 576;
  static constexpr int kPausePanelHeight = 196;
  static constexpr int kPauseLeftButtonX = 144;
  static constexpr int kPauseRightButtonX = 413;
  static constexpr int kPauseButtonY = 244;
  static constexpr int kPauseButtonWidth = 244;
  static constexpr int kPauseButtonHeight = 73;
  static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8U) << 8) |
                                 ((g & 0xFCU) << 3) | (b >> 3));
  }

  static constexpr uint16_t kBackground = 0x0063;
  // The static welcome raster and its failure overlay use the same navy as
  // the established READY/racing dashboard: RGB565 0x0063 (#000C19).
  static constexpr uint16_t kWelcomeBackground = kBackground;
  static constexpr uint16_t kWelcomeTrack = 0x4311;       // #476389
  static constexpr uint16_t kWelcomeText = 0xFFFF;
  static constexpr uint16_t kPod = 0x08C4;
  static constexpr uint16_t kPanel = 0x10E5;
  static constexpr uint16_t kBorder = 0x326C;
  static constexpr uint16_t kMuted = 0x9537;
  static constexpr uint16_t kText = 0xE77E;
  static constexpr uint16_t kYellow = 0xFE60;
  static constexpr uint16_t kGreen = 0x260F;
  static constexpr uint16_t kRed = 0xF9EA;
  static constexpr uint16_t kDarkRed = 0x5105;
  static constexpr uint16_t kDarkGreen = 0x1A26;
  static constexpr uint16_t kPressed = 0xFF4E;

  // Keep the right-hand timing field inside the panel even for unbounded
  // minute values such as 60:01:23 or +00:18:42.
  static constexpr int kLapsColumnX = 420;
  static constexpr int kLastColumnX = 515;
  static constexpr int kBestColumnX = 605;
  static constexpr int kRaceColumnX = 700;
  static constexpr int kLaneMetricsX = 468;
  static constexpr int kLaneMetricsWidth = 312;
  static constexpr int kLaneMetricsOffsetY = 16;
  static constexpr int kLaneMetricsHeight = 71;
  // The lap target is the only part of a READY lane row that changes when
  // the +/- value is adjusted. Keep it in its own fixed cell so the other
  // lane metrics and the static artwork are never repainted.
  static constexpr int kLaneTargetX = 372;
  static constexpr int kLaneTargetWidth = 96;

  struct TouchState {
    bool down = false;
    bool mainTarget = false;
    bool adjustmentTarget = false;
    bool adjustmentRepeated = false;
    uint8_t target = 0;
    SessionLifecycle lifecycle = SessionLifecycle::Ready;
    uint32_t downAt = 0;
    uint32_t missingSince = 0;
    uint32_t nextAdjustmentAt = 0;
    uint32_t adjustmentIntervalMs = kAdjustInitialIntervalMs;
    uint32_t adjustmentRepeats = 0;
    int adjustmentDelta = 0;
    int mappedX = 0;
    int mappedY = 0;
  };

  struct LaneAnimation {
    bool active = false;
    bool finishing = false;
    uint32_t startedAtMs = 0;
  };

  enum TouchTarget : uint8_t {
    TouchNone = 0,
    TouchMain = 1,
    TouchResume = 2,
    TouchAbandon = 3,
    TouchCancel = 4,
    TouchConfirm = 5,
    TouchModeLap = 6,
    TouchModeTimed = 7,
    TouchDecrease = 8,
    TouchIncrease = 9,
  };


  static int mapClamped(uint16_t value, int inMin, int inMax,
                       int outMin, int outMax) {
    const long mapped = map(static_cast<long>(value), inMin, inMax,
                            outMin, outMax);
    return static_cast<int>(constrain(mapped, outMin, outMax));
  }

  struct AaTextBounds {
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    bool hasPixels = false;
  };

  static uint16_t blend565(uint16_t background, uint16_t foreground,
                           uint8_t coverage) {
    if (!coverage) return background;
    if (coverage >= 15) return foreground;
    const int bgR = (background >> 11) & 0x1F;
    const int bgG = (background >> 5) & 0x3F;
    const int bgB = background & 0x1F;
    const int fgR = (foreground >> 11) & 0x1F;
    const int fgG = (foreground >> 5) & 0x3F;
    const int fgB = foreground & 0x1F;
    const auto mix = [coverage](int bg, int fg) {
      return (bg * (15 - coverage) + fg * coverage + 7) / 15;
    };
    return static_cast<uint16_t>((mix(bgR, fgR) << 11) |
                                 (mix(bgG, fgG) << 5) | mix(bgB, fgB));
  }

  static const Wp7AaGlyph* aaGlyph(const Wp7AaFont* font, unsigned char c) {
    if (!font) return nullptr;
    const uint16_t first = pgm_read_word(&font->first);
    const uint16_t last = pgm_read_word(&font->last);
    if (c < first || c > last) return nullptr;
    return font->glyph + (c - first);
  }

  static void measureAaText(const char* text, const Wp7AaFont* font,
                            AaTextBounds& bounds) {
    bounds = {};
    if (!text || !font) return;
    int cursor = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
         *p; ++p) {
      const Wp7AaGlyph* glyph = aaGlyph(font, *p);
      if (!glyph) continue;
      const int width = pgm_read_byte(&glyph->width);
      const int height = pgm_read_byte(&glyph->height);
      const int xOffset = static_cast<int8_t>(pgm_read_byte(&glyph->xOffset));
      const int yOffset = static_cast<int8_t>(pgm_read_byte(&glyph->yOffset));
      if (width && height) {
        const int left = cursor + xOffset;
        const int top = yOffset;
        const int right = left + width;
        const int bottom = top + height;
        if (!bounds.hasPixels) {
          bounds.minX = left;
          bounds.minY = top;
          bounds.maxX = right;
          bounds.maxY = bottom;
          bounds.hasPixels = true;
        } else {
          bounds.minX = min(bounds.minX, left);
          bounds.minY = min(bounds.minY, top);
          bounds.maxX = max(bounds.maxX, right);
          bounds.maxY = max(bounds.maxY, bottom);
        }
      }
      cursor += pgm_read_byte(&glyph->xAdvance);
    }
  }

  static void drawAaText(Arduino_RGB_Display* gfx, const char* text,
                         int originX, int baselineY, const Wp7AaFont* font,
                         uint16_t color, uint16_t background) {
    if (!gfx || !text || !font) return;
    gfx->startWrite();
    int cursor = originX;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
         *p; ++p) {
      const Wp7AaGlyph* glyph = aaGlyph(font, *p);
      if (!glyph) continue;
      const uint32_t offset = pgm_read_dword(&glyph->bitmapOffset);
      const uint8_t width = pgm_read_byte(&glyph->width);
      const uint8_t height = pgm_read_byte(&glyph->height);
      const int xOffset = static_cast<int8_t>(pgm_read_byte(&glyph->xOffset));
      const int yOffset = static_cast<int8_t>(pgm_read_byte(&glyph->yOffset));
      uint32_t pixelIndex = 0;
      for (uint8_t row = 0; row < height; ++row) {
        for (uint8_t column = 0; column < width; ++column, ++pixelIndex) {
          const uint8_t packed = pgm_read_byte(&font->bitmap[offset +
                                       (pixelIndex >> 1)]);
          const uint8_t coverage = (pixelIndex & 1U) ? (packed & 0x0FU)
                                                     : (packed >> 4);
          if (coverage) {
            gfx->writePixel(cursor + xOffset + column,
                            baselineY + yOffset + row,
                            blend565(background, color, coverage));
          }
        }
      }
      cursor += pgm_read_byte(&glyph->xAdvance);
    }
    gfx->endWrite();
  }

  // Arduino_GFX's RGB fillRect implementation writes the requested pixels but
  // flushes an entire framebuffer-width span for every row.  READY values and
  // lane target cells are deliberately narrow, so clear them a row at a time
  // through writeFastHLine().  The RGB implementation synchronises only the
  // written span for each row (using the same supported cache helper as the
  // rest of Arduino_GFX), avoiding the 800-pixel write-back caused by fillRect.
  static void fillNarrowRect(Arduino_RGB_Display* gfx, int x, int y, int width,
                             int height, uint16_t color) {
    if (!gfx || width <= 0 || height <= 0) return;
    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = (x + width) > 800 ? 800 : (x + width);
    const int bottom = (y + height) > 480 ? 480 : (y + height);
    if (left >= right || top >= bottom) return;
    gfx->startWrite();
    for (int row = top; row < bottom; ++row) {
      gfx->writeFastHLine(left, row, right - left, color);
    }
    gfx->endWrite();
  }

  static void drawTextTop(Arduino_RGB_Display* gfx, const char* text, int x,
                          int top, const Wp7AaFont* font, uint16_t color,
                          uint16_t background = kBackground) {
    AaTextBounds bounds;
    measureAaText(text, font, bounds);
    if (!bounds.hasPixels) return;
    drawAaText(gfx, text, x - bounds.minX, top - bounds.minY, font, color,
               background);
  }

  static void drawCentered(Arduino_RGB_Display* gfx, const char* text,
                           int centerX, int top, const Wp7AaFont* font,
                           uint16_t color,
                           uint16_t background = kBackground) {
    AaTextBounds bounds;
    measureAaText(text, font, bounds);
    if (!bounds.hasPixels) return;
    const int originX = centerX - (bounds.minX + bounds.maxX) / 2;
    drawAaText(gfx, text, originX, top - bounds.minY, font, color, background);
  }

  static void drawCompactCentered(Arduino_RGB_Display* gfx, const char* text,
                                  int centerX, int top, uint16_t background) {
    constexpr int kGlyphStep = 6;
    int x = centerX - static_cast<int>(std::strlen(text)) * kGlyphStep / 2;
    gfx->setFont(static_cast<const GFXfont*>(nullptr));
    gfx->setTextSize(1);
    for (; *text; ++text, x += kGlyphStep) {
      gfx->drawChar(x, top, *text, kText, background);
    }
  }

  static void drawRightAligned(Arduino_RGB_Display* gfx, const char* text,
                               int rightX, int top, const Wp7AaFont* font,
                               uint16_t color,
                               uint16_t background = kBackground) {
    AaTextBounds bounds;
    measureAaText(text, font, bounds);
    if (!bounds.hasPixels) return;
    const int originX = rightX - bounds.maxX;
    drawAaText(gfx, text, originX, top - bounds.minY, font, color, background);
  }

  static bool isFixedClock(const char* text) {
    return text && std::strlen(text) == 5 && text[2] == ':';
  }

  static void drawFixedClock(Arduino_RGB_Display* gfx, const char* text,
                             int centerX, int top, const Wp7AaFont* font,
                             uint16_t color, uint16_t background = kBackground) {
    if (!isFixedClock(text)) {
      drawCentered(gfx, text, centerX, top, font, color, background);
      return;
    }
    constexpr int kDigitAdvance = 16;
    constexpr int kColonAdvance = 8;
    constexpr int kTotalWidth = kDigitAdvance * 4 + kColonAdvance;
    int x = centerX - kTotalWidth / 2;
    for (int index = 0; index < 5; ++index) {
      const int width = text[index] == ':' ? kColonAdvance : kDigitAdvance;
      char glyph[2] = {text[index], '\0'};
      drawCentered(gfx, glyph, x + width / 2, top, font, color, background);
      x += width;
    }
  }

  static void drawAaTextItalic(Arduino_RGB_Display* gfx, const char* text,
                               int originX, int baselineY, const Wp7AaFont* font,
                               uint16_t color, uint16_t background,
                               int shearPixels = 6) {
    if (!gfx || !text || !font) return;
    gfx->startWrite();
    int cursor = originX;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
         *p; ++p) {
      const Wp7AaGlyph* glyph = aaGlyph(font, *p);
      if (!glyph) continue;
      const uint32_t offset = pgm_read_dword(&glyph->bitmapOffset);
      const uint8_t width = pgm_read_byte(&glyph->width);
      const uint8_t height = pgm_read_byte(&glyph->height);
      const int xOffset = static_cast<int8_t>(pgm_read_byte(&glyph->xOffset));
      const int yOffset = static_cast<int8_t>(pgm_read_byte(&glyph->yOffset));
      uint32_t pixelIndex = 0;
      for (uint8_t row = 0; row < height; ++row) {
        const int shear = height
                              ? ((static_cast<int>(height - row) * shearPixels) /
                                 static_cast<int>(height))
                              : 0;
        for (uint8_t column = 0; column < width; ++column, ++pixelIndex) {
          const uint8_t packed = pgm_read_byte(&font->bitmap[offset +
                                       (pixelIndex >> 1)]);
          const uint8_t coverage = (pixelIndex & 1U) ? (packed & 0x0FU)
                                                     : (packed >> 4);
          if (coverage) {
            gfx->writePixel(cursor + xOffset + column + shear,
                            baselineY + yOffset + row,
                            blend565(background, color, coverage));
          }
        }
      }
      cursor += pgm_read_byte(&glyph->xAdvance);
    }
    gfx->endWrite();
  }

  static void drawCenteredItalic(Arduino_RGB_Display* gfx, const char* text,
                                 int centerX, int top, const Wp7AaFont* font,
                                 uint16_t color, uint16_t background,
                                 int shearPixels = 6) {
    AaTextBounds bounds;
    measureAaText(text, font, bounds);
    if (!bounds.hasPixels) return;
    const int slant = shearPixels > 0 ? shearPixels : 0;
    const int originX = centerX - (bounds.minX + bounds.maxX + slant) / 2;
    drawAaTextItalic(gfx, text, originX, top - bounds.minY, font, color,
                     background, shearPixels);
  }

  bool drawWelcome(uint8_t progress, const char* status) {
#if defined(PP_WP7_STATIC_WELCOME_RASTER)
    (void)progress;
    if (status && std::strcmp(status, "STARTUP FAILED") == 0) {
      return drawWelcomeFailure();
    }
    if (welcomeDrawn_) return true;
    Arduino_RGB_Display* gfx = display_.touchTestCanvas();
    if (!gfx || !display_.beginFrame()) return false;
    if (!copyWelcomeRaster(gfx)) {
      display_.setBacklight(true);
      return false;
    }
    // The framebuffer is complete and cache-synchronised before the panel is
    // made visible.  The static raster is never redrawn during normal startup.
    display_.setBacklight(true);
    const bool presented = display_.presentFrame();
    if (presented && !welcomeFirstPresented_) {
      welcomeDrawn_ = true;
      welcomeFirstPresented_ = true;
      welcomeFirstPresentedMs_ = millis();
      Serial.printf("[WP7 PHYS] welcome static raster presented t_ms=%lu bytes=%lu\n",
                    static_cast<unsigned long>(welcomeFirstPresentedMs_),
                    static_cast<unsigned long>(kWelcomeRasterBytes));
    } else if (presented) {
      welcomeDrawn_ = true;
    }
    return presented;
#else
    Arduino_RGB_Display* gfx = display_.touchTestCanvas();
    if (!gfx || !display_.beginFrame()) return false;
    const bool fullRedraw = !welcomeDrawn_;
    if (fullRedraw) gfx->fillScreen(kWelcomeBackground);

    // Match Gina's two-tone lettering.  The physical image uses the
    // project-embedded Barlow face (the browser preview uses Arial Black as
    // its closest available reference), but retains the exact colour
    // hierarchy: white ampersand in P&P and yellow ampersand in the name.
    if (fullRedraw) {
      drawCenteredItalic(gfx, "P", 350, 58, &Wp7BarlowSemiCondensedBold40,
                         kYellow, kWelcomeBackground, 7);
      drawCenteredItalic(gfx, "&", 400, 62, &Wp7BarlowSemiCondensedBold40,
                         kWelcomeText, kWelcomeBackground, 5);
      drawCenteredItalic(gfx, "P", 450, 58, &Wp7BarlowSemiCondensedBold40,
                         kYellow, kWelcomeBackground, 7);
      drawCentered(gfx, "PLONK ", 330, 164,
                   &Wp7BarlowSemiCondensedSemiBold30, kWelcomeText,
                   kWelcomeBackground);
      drawCentered(gfx, "&", 400, 164,
                   &Wp7BarlowSemiCondensedSemiBold30, kYellow,
                   kWelcomeBackground);
      drawCentered(gfx, " PLAY", 470, 164,
                   &Wp7BarlowSemiCondensedSemiBold30, kWelcomeText,
                   kWelcomeBackground);
      drawCentered(gfx, "Simple Setup. Serious Slotting.", 400, 218,
                   &Wp7BarlowSemiCondensedSemiBold24, kWelcomeText,
                   kWelcomeBackground);
    }

    constexpr int barX = 150;
    constexpr int barY = 294;
    constexpr int barWidth = 500;
    constexpr int barHeight = 14;
    // Progress and status are the only changing welcome regions.  Restoring
    // just these rectangles avoids exposing a partially rebuilt splash while
    // NVS/SD/network startup continues in the other services.
    gfx->fillRect(barX, barY, barWidth, barHeight, kWelcomeBackground);
    gfx->fillRoundRect(barX, barY, barWidth, barHeight, 7, kWelcomeTrack);
    const int filled = (barWidth * (progress > 100 ? 100 : progress)) / 100;
    if (filled > 0) {
      gfx->fillRoundRect(barX, barY, filled, barHeight, 7, kYellow);
    }

    constexpr int statusX = 100;
    constexpr int statusY = 342;
    constexpr int statusWidth = 600;
    constexpr int statusHeight = 62;
    gfx->fillRect(statusX, statusY, statusWidth, statusHeight,
                  kWelcomeBackground);
    const char* displayStatus = status ? status : welcomeCustomerStatus(false);
    drawCenteredItalic(gfx, displayStatus, 400, 354,
                       &Wp7BarlowSemiCondensedBold40, kYellow,
                       kWelcomeBackground, 5);
    gfx->fillRect(100, 418, 600, 45, kWelcomeBackground);
    if (std::strcmp(displayStatus, "STARTUP FAILED") == 0) {
      drawCentered(gfx, touchReady_ ? "P&P STARTUP FAILED"
                                    : "TOUCHSCREEN UNAVAILABLE",
                   400, 430, &Wp7BarlowSemiCondensedSemiBold24, kMuted,
                   kWelcomeBackground);
    }
    const bool presented = display_.presentFrame();
    if (presented && !welcomeFirstPresented_) {
      welcomeDrawn_ = true;
      welcomeFirstPresented_ = true;
      welcomeFirstPresentedMs_ = millis();
      Serial.printf("[WP7 PHYS] welcome first presented t_ms=%lu progress=%u status=%s\n",
                    static_cast<unsigned long>(welcomeFirstPresentedMs_),
                    static_cast<unsigned>(progress), status ? status : "");
    } else if (presented) {
      welcomeDrawn_ = true;
    }
    return presented;
#endif
  }

#if defined(PP_WP7_STATIC_WELCOME_RASTER)
  static constexpr size_t kWelcomeRasterBytes =
      static_cast<size_t>(kPanelWidth) * kPanelHeight * sizeof(uint16_t);

  bool copyWelcomeRaster(Arduino_RGB_Display* gfx) {
    if (!gfx) return false;
    uint16_t* framebuffer = gfx->getFramebuffer();
    const size_t embeddedBytes =
        static_cast<size_t>(wp7WelcomeRasterEnd - wp7WelcomeRasterStart);
    if (!framebuffer || embeddedBytes != kWelcomeRasterBytes) {
      Serial.printf("[WP7 PHYS] welcome raster invalid framebuffer=%p bytes=%lu expected=%lu\n",
                    static_cast<void*>(framebuffer),
                    static_cast<unsigned long>(embeddedBytes),
                    static_cast<unsigned long>(kWelcomeRasterBytes));
      return false;
    }
    display_.setBacklight(false);
    std::memcpy(framebuffer, wp7WelcomeRasterStart, kWelcomeRasterBytes);
    // Arduino_GFX's proven single-framebuffer path owns this cache boundary;
    // force one complete write-back while the backlight is off.
    gfx->flush(true);
    return true;
  }

  bool drawWelcomeFailure() {
    if (welcomeFailureDrawn_) return true;
    Arduino_RGB_Display* gfx = display_.touchTestCanvas();
    if (!gfx || !display_.beginFrame()) return false;
    if (!copyWelcomeRaster(gfx)) {
      display_.setBacklight(true);
      return false;
    }
    gfx->fillRect(80, 326, 640, 108, kWelcomeBackground);
    drawCentered(gfx, touchReady_ ? "P&P STARTUP FAILED"
                                 : "TOUCHSCREEN UNAVAILABLE",
                 400, 350, &Wp7BarlowSemiCondensedSemiBold24, kMuted,
                 kWelcomeBackground);
    gfx->flush(true);
    display_.setBacklight(true);
    const bool presented = display_.presentFrame();
    if (presented) {
      welcomeFailureDrawn_ = true;
      welcomeDrawn_ = true;
      if (!welcomeFirstPresented_) {
        welcomeFirstPresented_ = true;
        welcomeFirstPresentedMs_ = millis();
      }
      Serial.printf("[WP7 PHYS] welcome failure presented t_ms=%lu\n",
                    static_cast<unsigned long>(millis()));
    }
    return presented;
  }
#endif

  void updateWelcome(Time now, uint32_t nowMs, bool force) {
    if (!startupComplete_) {
      if (startupFailed_) return;
      const char* displayStatus = welcomeCustomerStatus(false);
      const bool statusChanged = !welcomeStatus_ ||
                                 std::strcmp(welcomeStatus_, displayStatus) != 0;
      if (force || welcomeProgress_ != startupProgress_ || statusChanged) {
        welcomeProgress_ = startupProgress_;
        welcomeStatus_ = displayStatus;
        drawWelcome(welcomeProgress_, welcomeStatus_);
      }
      return;
    }
    const TasterSnapshot current = authority_.snapshot();
    uint8_t progress = 70U;
    bool failed = !touchReady_;
    if (!touchReady_) {
      // A missing touch controller is a real startup failure for this
      // touchscreen client.  Keep the failure visible instead of leaving a
      // plausible-looking partial progress bar on screen indefinitely.
      progress = 100U;
    } else if (current.state.lifecycle == SessionLifecycle::Faulted) {
      progress = 100U;
      failed = true;
    } else if (current.readiness == StartReadiness::Ready &&
               current.state.lifecycle == SessionLifecycle::Ready) {
      progress = 100U;
    }
    const char* status = welcomeCustomerStatus(failed);
    const bool statusChanged = !welcomeStatus_ ||
                               std::strcmp(welcomeStatus_, status) != 0;

    if (force || progress != welcomeProgress_ || statusChanged) {
      welcomeProgress_ = progress;
      welcomeStatus_ = status;
      drawWelcome(progress, status);
    }

    // Leaving the welcome screen means the controller is operational and the
    // local Taster can accept setup input.  It deliberately does not require
    // StartReadiness::Ready: detector capability and setup validity remain
    // independent Race Control guards on START.
    const bool controllerReady = current.state.lifecycle == SessionLifecycle::Ready;
    if (welcomeCanLeaveAfterMinimum(startupComplete_, true, touchReady_,
                                    controllerReady, welcomeFirstPresented_,
                                    welcomeFirstPresentedMs_, nowMs,
                                    kWelcomeMinimumMs)) {
      welcomeActive_ = false;
      renderSignatureValid_ = false;
      welcomeTransitionPending_ = true;
      nextDrawMs_ = nowMs;
      drawCurrent(now);
      if (!readyPresentedLogged_) {
        readyPresentedLogged_ = true;
        const uint32_t exitedMs = millis();
        const uint32_t visibleMs = welcomeFirstPresented_
                                       ? exitedMs - welcomeFirstPresentedMs_
                                       : 0U;
        Serial.printf("[WP7 PHYS] welcome exited t_ms=%lu visible_ms=%lu; READY presented\n",
                      static_cast<unsigned long>(exitedMs),
                      static_cast<unsigned long>(visibleMs));
      }
    }
  }

  static void drawButton(Arduino_RGB_Display* gfx, int x, int y, int width,
                         int height, const char* label, uint16_t fill,
                         uint16_t ink, const Wp7AaFont* font,
                         bool pressed = false) {
    if (!gfx || !label) return;
    if (pressed) fill = kPressed;
    gfx->fillRoundRect(x, y, width, height, 8, fill);
    gfx->drawRoundRect(x, y, width, height, 8, kBorder);
    AaTextBounds bounds;
    measureAaText(label, font, bounds);
    if (!bounds.hasPixels) return;
    const int textHeight = bounds.maxY - bounds.minY;
    drawCentered(gfx, label, x + width / 2,
                 y + (height - textHeight) / 2, font, ink, fill);
  }

  static void drawRleAsset(Arduino_RGB_Display* gfx, const Wp7RleAsset& asset,
                           int x, int y, int width, int height) {
    if (!gfx) return;
    for (uint16_t r = 0; r < asset.runCount; ++r) {
      const Wp7RleRun& run = asset.runs[r];
      for (uint16_t i = 0; i < run.length; ++i) {
        const int sourceX = static_cast<int>(run.x) + i;
        const int destX = x + (sourceX * width) / asset.width;
        const int destY = y + (static_cast<int>(run.y) * height) / asset.height;
        gfx->drawPixel(destX, destY, asset.pixels[run.pixelOffset + i]);
      }
    }
  }

  static void formatTime(Time value, char* out, size_t capacity) {
    formatClockMinutesSeconds(value, out, capacity);
  }

  static uint64_t wholeSeconds(Time value) {
    return value / 1000000ULL;
  }

  static uint64_t remainingWholeSeconds(Time value) {
    if (!value) return 0;
    return (value + 999999ULL) / 1000000ULL;
  }

  static void formatRaceDuration(Time value, char* out, size_t capacity) {
    if (!out || !capacity) return;
    const uint64_t hundredths = (value + 5000ULL) / 10000ULL;
    const uint64_t totalSeconds = hundredths / 100ULL;
    const uint64_t minutes = totalSeconds / 60ULL;
    const uint64_t seconds = totalSeconds % 60ULL;
    const uint64_t fraction = hundredths % 100ULL;
    snprintf(out, capacity, "%02llu:%02llu:%02llu",
             static_cast<unsigned long long>(minutes),
             static_cast<unsigned long long>(seconds),
             static_cast<unsigned long long>(fraction));
  }

  static void formatWholeSeconds(Time value, char* out, size_t capacity) {
    formatClockMinutesSeconds(value, out, capacity);
  }

  static void formatRemainingSeconds(Time value, char* out, size_t capacity) {
    if (!out || !capacity) return;
    const uint64_t totalSeconds = remainingWholeSeconds(value);
    const uint64_t minutes = totalSeconds / 60ULL;
    const uint64_t seconds = totalSeconds % 60ULL;
    snprintf(out, capacity, "%02llu:%02llu",
             static_cast<unsigned long long>(minutes),
             static_cast<unsigned long long>(seconds));
  }

  static void formatOffset(Time value, char* out, size_t capacity) {
    if (!out || !capacity) return;
    char duration[24]{};
    formatRaceDuration(value, duration, sizeof(duration));
    snprintf(out, capacity, "+%s", duration);
  }

  static void formatRaceColumn(const NoticeboardState& state,
                               const NoticeboardEntryState* entry,
                               char* out, size_t capacity) {
    if (!out || !capacity) return;
    out[0] = '\0';
    if (state.sessionMode == SessionMode::Endurance) {
      if (entry && entry->hasOvertimeOffset) {
        formatOffset(entry->overtimeOffset, out, capacity);
      } else if (entry && entry->completed) {
        // A settled entry without a legitimate expiry crossing has no offset.
        snprintf(out, capacity, "--");
      } else if (state.durationExpired) {
        snprintf(out, capacity, "+%llu",
                 static_cast<unsigned long long>(wholeSeconds(state.overtime)));
      } else if (state.remainingDuration) {
        formatRemainingSeconds(state.remainingDuration, out, capacity);
      } else {
        snprintf(out, capacity, "--");
      }
      return;
    }
    if (entry && entry->completed && entry->finishDuration) {
      // The Taster's Race Time column uses the same compact MM:SS clock at
      // completion as it does while racing.  The authoritative completion
      // timestamp remains unchanged; the Race Engine supplies the elapsed
      // duration (without pauses), and this client formats whole seconds.
      formatClockMinutesSeconds(entry->finishDuration, out, capacity);
    } else if (state.raceElapsed) {
      formatWholeSeconds(state.raceElapsed, out, capacity);
    } else {
      snprintf(out, capacity, "--");
    }
  }

  uint32_t renderSignature(const NoticeboardState& state,
                           uint32_t nowMs) const {
    uint32_t hash = 2166136261U;
    const auto add = [&hash](uint64_t value) {
      hash ^= static_cast<uint32_t>(value);
      hash *= 16777619U;
      hash ^= static_cast<uint32_t>(value >> 32);
      hash *= 16777619U;
    };
    add(static_cast<uint32_t>(state.lifecycle));
    // READY displays the local setup controls. Do not hash the later
    // authoritative acknowledgement separately: it is the same visible
    // mode/value and should not fall through to a full-frame redraw.
    const SessionMode visibleMode = visibleSetupMode(
        state.lifecycle, state.sessionMode, surface_.selectedMode());
    const uint32_t visibleValue = visibleSetupValue(
        state.lifecycle, state.sessionMode, state.lapTarget,
        state.durationMinutes, surface_.selectedValue());
    add(static_cast<uint32_t>(visibleMode));
    add(visibleValue);
    if (state.lifecycle != SessionLifecycle::Ready) {
      add(state.lapTarget);
      add(state.durationMinutes);
    }
    add(state.durationExpired ? 1U : 0U);
    add(wholeSeconds(state.raceElapsed));
    add(remainingWholeSeconds(state.remainingDuration));
    add(wholeSeconds(state.overtime));
    add(state.redLightsLit);
    add(state.startLightsGreen ? 1U : 0U);
    if (pausedWarningLightsEnabled() &&
        state.lifecycle == SessionLifecycle::Paused) {
      add(lightPhaseFor(nowMs));
    }
    add(state.entryCount);
    add(static_cast<uint32_t>(surface_.selectedMode()));
    add(surface_.selectedValue());
    // Setup acknowledgement is not itself a visible state.  The presentation
    // latch keeps the button stable while a requested value is being accepted,
    // and only changes it when its visible enabled/disabled state changes.
    add(displayedStartEnabled() ? 1U : 0U);
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      const auto& entry = state.entries[i];
      add(entry.lane);
      add(entry.laps);
      add(entry.hasLap ? 1U : 0U);
      add(entry.lastLapTime);
      add(entry.bestLapTime);
      add(entry.completed ? 1U : 0U);
      add(entry.finishDuration);
      add(entry.hasOvertimeOffset ? 1U : 0U);
      add(entry.overtimeOffset);
      add(static_cast<uint32_t>(entry.gapKind));
      add(entry.lapsBehind);
      add(entry.gapTime);
    }
    add(mainPressed_ ? 1U : 0U);
    add(pauseHoldFeedback_ ? 1U : 0U);
    add(abandonConfirm_ ? 1U : 0U);
    return hash;
  }

  bool displayedStartEnabled() const {
    if (!renderSignatureValid_) return surface_.startEnabled();
    if (surface_.setupPending()) return lastStartEnabled_;
    return surface_.startEnabled();
  }

  static const char* lifecycleText(SessionLifecycle lifecycle) {
    switch (lifecycle) {
      case SessionLifecycle::Starting: return "STARTING";
      case SessionLifecycle::Racing: return "RACING";
      case SessionLifecycle::Paused: return "PAUSED";
      case SessionLifecycle::Finished: return "FINISHED";
      default: return "READY";
    }
  }

  const char* laneName(uint8_t lane, char* fallback, size_t capacity) const {
    // A committed presentation label owns this text. It is deliberately
    // separate from MUG identity and does not change when a race entry or
    // its frozen racer name changes. Blank labels use the generic lane name.
    const auto* committed = authority_.committedPreferences();
    if (committed && lane >= 1 && lane <= 2) {
      const auto& preferences = *committed;
      const char* label = lane == 1 ? preferences.lane1Label : preferences.lane2Label;
      if (label[0]) return label;
    }
    snprintf(fallback, capacity, "Lane %u", unsigned(lane));
    return fallback;
  }

  TasterCarGraphic laneGraphic(uint8_t lane) const {
    const auto* committed = authority_.committedPreferences();
    if (committed && lane >= 1 && lane <= 2) {
      const auto& preferences = *committed;
      return lane == 1 ? preferences.lane1Graphic : preferences.lane2Graphic;
    }
    return lane == 2 ? TasterCarGraphic::RedEtype : TasterCarGraphic::GreenMgb;
  }

  const Wp7RleAsset& laneCar(uint8_t lane) const {
    return laneGraphic(lane) == TasterCarGraphic::RedEtype ? kWp7Etype : kWp7Mgb;
  }

  static uint16_t startingLightColorForPhase(const NoticeboardState& state,
                                             int index, uint8_t warningPhase) {
    if (pausedWarningLightsEnabled() &&
        state.lifecycle == SessionLifecycle::Paused) {
      return pausedWarningLampLit(static_cast<uint8_t>(index), warningPhase)
                 ? kYellow
                 : rgb565(20, 32, 45);
    }
    return startingLightColor(state, index);
  }

  uint8_t lightPhaseFor(uint32_t nowMs) const {
    // The warning cadence starts when the authoritative PAUSED lifecycle is
    // first observed, rather than being tied to boot uptime.  A RESUME or
    // ABANDON transition resets the anchor and therefore stops the warning
    // sequence immediately.
    return pausedWarningPhase(nowMs - pausedWarningStartedMs_);
  }

  void drawStartingLights(Arduino_RGB_Display* gfx,
                          const NoticeboardState& state,
                          uint32_t nowMs) const {
    const int centers[] = {273, 351, 429, 507, 585};
    const uint8_t warningPhase = pausedWarningLightsEnabled()
                                     ? lightPhaseFor(nowMs)
                                     : 0;
    for (int i = 0; i < 5; ++i) {
      drawStartingLight(gfx, centers[i],
                        startingLightColorForPhase(state, i, warningPhase));
    }
  }

  static uint16_t startingLightColor(const NoticeboardState& state, int index) {
    return racingSignalLampColor(state.startLightsGreen, state.redLightsLit,
                                 index);
  }

#if defined(PP_WP7_STATIC_WELCOME_RASTER)
  static void drawApprovedRgb565(Arduino_RGB_Display* gfx,
                                 const uint8_t* bytes, int x, int y,
                                 int width, int height) {
    if (!gfx || !bytes) return;
    // ESP32-S3 is little-endian and Arduino_GFX's RGB bitmap method consumes
    // host-order RGB565 words, matching the approved file format directly.
    gfx->draw16bitRGBBitmap(
        x, y,
        const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(bytes)),
        width, height);
  }
#endif

  static void drawStartingLight(Arduino_RGB_Display* gfx, int centerX,
                                uint16_t fill) {
    if (!gfx) return;
#if defined(PP_WP7_STATIC_WELCOME_RASTER)
    // All state artwork is supplied as opaque, native-size, little-endian
    // RGB565. Each bitmap exactly fills one 60x60 lamp dirty cell.
    const uint8_t* asset = fill == kRacingSignalRed
                               ? wp7LampRedStart
                               : fill == kRacingSignalGreen
                                     ? wp7LampGreenStart
                                     : fill == kYellow ? wp7LampYellowStart
                                                       : wp7LampOffStart;
    drawApprovedRgb565(gfx, asset, centerX - 30, 4, 60, 60);
#else
    gfx->fillCircle(centerX, 34, 17, fill);
#endif
  }

  static void drawStartingLightDelta(Arduino_RGB_Display* gfx,
                                     const NoticeboardState& state,
                                     const NoticeboardState& previous) {
    if (!gfx) return;
    const int centers[] = {273, 351, 429, 507, 585};
    for (int i = 0; i < 5; ++i) {
      if (startingLightColor(state, i) != startingLightColor(previous, i)) {
        drawStartingLight(gfx, centers[i], startingLightColor(state, i));
      }
    }
  }

  void drawLaneLapsField(Arduino_RGB_Display* gfx, int y, uint8_t lane,
                         const NoticeboardState& state, bool clear) const {
    if (clear) {
      fillNarrowRect(gfx, kLaneTargetX, y + kLaneMetricsOffsetY,
                     kLaneTargetWidth, kLaneMetricsHeight, kPanel);
    }
    const NoticeboardEntryState* entry = nullptr;
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      if (state.entries[i].lane == lane) {
        entry = &state.entries[i];
        break;
      }
    }
    char laps[24]{};
    const SessionMode mode = visibleSetupMode(
        state.lifecycle, state.sessionMode, surface_.selectedMode());
    const uint32_t target = visibleSetupValue(
        state.lifecycle, state.sessionMode, state.lapTarget,
        state.durationMinutes, surface_.selectedValue());
    const bool raceHasStarted = state.lifecycle == SessionLifecycle::Racing ||
                                state.lifecycle == SessionLifecycle::Paused ||
                                state.lifecycle == SessionLifecycle::Finished;
    formatVisibleLapCell(laps, sizeof(laps), mode, entry != nullptr && raceHasStarted,
                         entry ? entry->laps : 0U, target);
    if (*laps && mode == SessionMode::LapRace && target > 999) {
      // Long shared race distances still fit the lane's existing dirty cell.
      if (std::strlen(laps) > kLaneTargetWidth / 6) {
        char* separator = std::strchr(laps, '/');
        if (separator) {
          *separator = '\0';
          drawCompactCentered(gfx, laps, kLapsColumnX, y + 32, kPanel);
          *separator = '/';
          drawCompactCentered(gfx, separator, kLapsColumnX, y + 42, kPanel);
          return;
        }
      }
      drawCompactCentered(gfx, laps, kLapsColumnX, y + 36, kPanel);
      return;
    }
    drawCentered(gfx, laps, kLapsColumnX, y + 32, &Wp7BarlowSemiCondensedBold28,
                 kText, kPanel);
  }

  void drawLaneMetrics(Arduino_RGB_Display* gfx, int y,
                       uint8_t lane, const NoticeboardState& state) const {
    // Timer/lap fields are the only moving pixels in a row. Restore just this
    // fixed metrics band so the car, lane identity and racer name never flash.
    gfx->fillRect(kLaneMetricsX, y + kLaneMetricsOffsetY,
                  kLaneMetricsWidth, kLaneMetricsHeight, kPanel);
    const NoticeboardEntryState* entry = nullptr;
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      if (state.entries[i].lane == lane) {
        entry = &state.entries[i];
        break;
      }
    }
    char last[24] = "--";
    char best[24] = "--";
    char raceColumn[32] = "--";
    if (entry) {
      if (entry->hasLap) formatTime(entry->lastLapTime, last, sizeof(last));
      if (entry->bestLapTime) formatTime(entry->bestLapTime, best, sizeof(best));
      formatRaceColumn(state, entry, raceColumn, sizeof(raceColumn));
    }
    drawFixedClock(gfx, last, kLastColumnX, y + 32,
                   &Wp7BarlowSemiCondensedBold28, kText, kPanel);
    drawFixedClock(gfx, best, kBestColumnX, y + 32,
                   &Wp7BarlowSemiCondensedBold28, kText, kPanel);
    drawFixedClock(gfx, raceColumn, kRaceColumnX, y + 32,
                   &Wp7BarlowSemiCondensedBold28, kText, kPanel);
  }

  // Direct port of the Wokwi chip's draw_flag_fabric().  The physical panel
  // uses RGB565 pixels instead of the chip's RGBA framebuffer, but retains
  // the same two triangular wave components, checker cell size and phase.
  static void drawFlagFabric(Arduino_RGB_Display* gfx, int x, int y,
                             int width, int height, int phase,
                             int amplitude) {
    if (!gfx || width <= 0 || height <= 0) return;
    constexpr int kCell = 7;
    const uint16_t light = rgb565(238, 243, 248);
    const uint16_t dark = rgb565(31, 43, 55);
    for (int xx = 0; xx < width; ++xx) {
      const int q = (xx + phase) % 56;
      const int tri = q < 28 ? q : 56 - q;
      int dy = ((tri - 14) * amplitude) / 14;
      const int q2 = (xx * 3 + phase * 2) % 92;
      const int tri2 = q2 < 46 ? q2 : 92 - q2;
      dy += ((tri2 - 23) * amplitude) / 69;
      for (int yy = 0; yy < height; ++yy) {
        const int py = y + dy + yy;
        if (py < 0 || py >= kPanelHeight) continue;
        const bool isLight = (((xx / kCell) + (yy / kCell)) & 1) == 0;
        gfx->drawPixel(x + xx, py, isLight ? light : dark);
      }
    }
  }

  static void drawFinishFlag(Arduino_RGB_Display* gfx, uint32_t phase) {
    if (!gfx) return;
    // This is the same finished-screen flag footprint as the Wokwi renderer.
    // Clear the small animated region first so pixels from the previous wave
    // cannot remain behind the new phase.
    gfx->fillRect(650, 70, 130, 60, kBackground);
    drawFlagFabric(gfx, 650, 76, 130, 48, static_cast<int>(phase), 3);
  }

  static void drawAnimatedCar(Arduino_RGB_Display* gfx,
                              const Wp7RleAsset& car, int y,
                              uint32_t frame, bool finishing) {
    if (!gfx) return;
    if (!frame) {
      drawRleAsset(gfx, car, kCarStartX, y, 135, 60);
      return;
    }
    const uint32_t limit = finishing ? kFinishAnimationFrames
                                    : kLapAnimationFrames;
    const uint32_t step = min(frame, limit);
    const int f = static_cast<int>(step) - 1;
    const int denominator = finishing
                                ? static_cast<int>(kFinishAnimationFrames - 1U)
                                : static_cast<int>(kLapAnimationFrames - 1U);
    const int motionX = kCarStartX +
                        (f * f * kCarTravelPixels) /
                            (denominator * denominator);
    if (finishing && f >= 2) {
      // Preserve the original Wokwi streak and waving fabric geometry.
      int streakX = motionX - 55;
      if (streakX < 28) streakX = 28;
      gfx->drawLine(streakX, y + 30, motionX - 8, y + 30,
                    rgb565(58, 86, 105));
      gfx->drawLine(streakX + 15, y + 38, motionX - 5, y + 38,
                    rgb565(45, 70, 88));
      if (motionX > kCarStartX + 12) {
        drawFlagFabric(gfx, kCarStartX, y + 10,
                       motionX - kCarStartX - 8, 40,
                       static_cast<int>(frame * 9U), 3);
      }
    }
    drawRleAsset(gfx, car, motionX, y, 135, 60);
  }

  bool laneAnimationActive(uint8_t lane, uint32_t nowMs) const {
    if (!kAnimationsEnabled) return false;
    if (lane < 1 || lane > 2) return false;
    const LaneAnimation& animation = laneAnimations_[lane - 1];
    if (!animation.active) return false;
    const uint32_t frames = animation.finishing ? kFinishAnimationFrames
                                                : kLapAnimationFrames;
    return static_cast<uint32_t>(nowMs - animation.startedAtMs) <
           frames * kAnimationFrameMs;
  }

  uint32_t laneAnimationFrame(uint8_t lane, uint32_t nowMs) const {
    if (!laneAnimationActive(lane, nowMs)) return 0;
    const LaneAnimation& animation = laneAnimations_[lane - 1];
    const uint32_t frames = animation.finishing ? kFinishAnimationFrames
                                                : kLapAnimationFrames;
    const uint32_t elapsed = nowMs - animation.startedAtMs;
    return min(frames, elapsed / kAnimationFrameMs + 1U);
  }

  const NoticeboardEntryState* entryForLane(const NoticeboardState& state,
                                            uint8_t lane) const {
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      if (state.entries[i].lane == lane) return &state.entries[i];
    }
    return nullptr;
  }

  void observeAnimationEvents(const NoticeboardState& state,
                              uint32_t nowMs) {
    if (!kAnimationsEnabled) {
      for (auto& animation : laneAnimations_) animation.active = false;
      return;
    }
    if (!renderSignatureValid_ || state.lifecycle == SessionLifecycle::Ready ||
        state.lifecycle == SessionLifecycle::Starting ||
        state.lifecycle == SessionLifecycle::Paused) {
      for (auto& animation : laneAnimations_) animation.active = false;
      return;
    }
    for (uint8_t lane = 1; lane <= 2; ++lane) {
      const NoticeboardEntryState* current = entryForLane(state, lane);
      const NoticeboardEntryState* previous = entryForLane(lastState_, lane);
      if (!current || !previous) continue;
      const bool newLap = current->laps > previous->laps;
      const bool newlyFinished = current->completed && !previous->completed;
      if (newLap || newlyFinished) {
        LaneAnimation& animation = laneAnimations_[lane - 1];
        animation.active = true;
        animation.finishing = current->completed || newlyFinished;
        animation.startedAtMs = nowMs;
      }
    }
  }

  void drawLaneStatic(Arduino_RGB_Display* gfx, int y, uint16_t stripColor,
                      uint8_t lane, const char* name,
                      const Wp7RleAsset& car, uint32_t nowMs = 0) const {
    gfx->fillRoundRect(20, y, 760, 103, 4, kPanel);
    gfx->drawRoundRect(20, y, 760, 103, 4, kBorder);
    gfx->fillRect(20, y, 7, 103, stripColor);
    char laneNumber[4]{};
    snprintf(laneNumber, sizeof(laneNumber), "%u", unsigned(lane));
    drawTextTop(gfx, laneNumber, 38, y + 39,
                &Wp7BarlowSemiCondensedSemiBold24, kText, kPanel);
    const LaneAnimation& animation = laneAnimations_[lane - 1];
    drawAnimatedCar(gfx, car, y + 18,
                    laneAnimationFrame(lane, nowMs), animation.finishing);
    drawTextTop(gfx, name, 200, y + 23,
                &Wp7BarlowSemiCondensedSemiBold30, kText, kPanel);
  }

  void drawLaneRow(Arduino_RGB_Display* gfx, int y, uint16_t stripColor,
                   uint8_t lane, const NoticeboardState& state,
                   const char* name, const Wp7RleAsset& car,
                   uint32_t nowMs = 0) const {
    drawLaneStatic(gfx, y, stripColor, lane, name, car, nowMs);
    drawLaneLapsField(gfx, y, lane, state, true);
    drawLaneMetrics(gfx, y, lane, state);
  }

  void drawTopSummary(Arduino_RGB_Display* gfx,
                      const NoticeboardState& state) const {
    if (state.lifecycle == SessionLifecycle::Ready) {
      readyValueSlotsValid_ = false;
      drawReadyModeControls(gfx);
      drawReadyValue(gfx);
      drawReadyModeLabel(gfx);
    } else {
    // Clear both the former centred target and the left-aligned heading.
    // Active screens deliberately show no configured target in this row.
    gfx->fillRect(28, 70, 662, 70, kBackground);
    const bool endurance = state.sessionMode == SessionMode::Endurance;
    drawTextTop(gfx, tasterRaceHeading(state.sessionMode),
                kTasterHeadingLabelX, 96,
                &Wp7BarlowSemiCondensedSemiBold24, kYellow);
    drawTextTop(gfx, "RACER", 35, 151, &Wp7BarlowSemiCondensedSemiBold24,
                kMuted);
    drawCentered(gfx, "LAPS", kLapsColumnX, 151,
                 &Wp7BarlowSemiCondensedSemiBold24, kMuted);
    drawCentered(gfx, "LAST", kLastColumnX, 151,
                 &Wp7BarlowSemiCondensedSemiBold24, kMuted);
    drawCentered(gfx, "BEST", kBestColumnX, 151,
                 &Wp7BarlowSemiCondensedSemiBold24, kMuted);
    drawCentered(gfx, endurance ? "TIME LEFT" : "RACE TIME", kRaceColumnX, 151,
                 &Wp7BarlowSemiCondensedSemiBold24, kMuted);
    }
    if (state.lifecycle == SessionLifecycle::Ready) {
      drawTextTop(gfx, "RACER", 35, 151, &Wp7BarlowSemiCondensedSemiBold24,
                  kMuted);
      drawCentered(gfx, "LAPS", kLapsColumnX, 151,
                   &Wp7BarlowSemiCondensedSemiBold24, kMuted);
      drawCentered(gfx, "LAST", kLastColumnX, 151,
                   &Wp7BarlowSemiCondensedSemiBold24, kMuted);
      drawCentered(gfx, "BEST", kBestColumnX, 151,
                   &Wp7BarlowSemiCondensedSemiBold24, kMuted);
      drawCentered(gfx, "RACE TIME", kRaceColumnX, 151,
                   &Wp7BarlowSemiCondensedSemiBold24, kMuted);
    }
  }

  void drawReadyModeControls(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    drawReadyModeButtons(gfx);
    drawReadyStepButtons(gfx);
  }

  void drawReadyModeButtons(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    const bool timed = surface_.selectedMode() == SessionMode::Endurance;
    drawButton(gfx, kReadyLapsX, kReadyModeY, kReadyModeWidth,
               kReadyModeHeight, "LAPS", timed ? kPanel : kYellow,
               timed ? kText : kBackground,
               &Wp7BarlowSemiCondensedSemiBold30);
    drawButton(gfx, kReadyTimedX, kReadyModeY, kReadyModeWidth,
               kReadyModeHeight, "TIMED", timed ? kYellow : kPanel,
               timed ? kBackground : kText,
               &Wp7BarlowSemiCondensedSemiBold30);
  }

  void drawReadyStepButtons(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    drawButton(gfx, kReadyMinusX, kReadyModeY, kReadyStepWidth,
               kReadyStepHeight, "-", kPanel, kText,
               &Wp7BarlowSemiCondensedBold40);
    drawButton(gfx, kReadyPlusX, kReadyModeY, kReadyStepWidth,
               kReadyStepHeight, "+", kPanel, kText,
               &Wp7BarlowSemiCondensedBold40);
  }

  void drawReadyValue(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    const uint32_t selected = surface_.selectedValue();
    if (selected > 999) {
      char value[11]{};
      snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(selected));
      fillNarrowRect(gfx, kReadyValueX, kReadyValueY,
                     kReadyValueWidth, kReadyValueHeight, kBackground);
      drawCompactCentered(gfx, value, kReadyValueCenterX, 95, kBackground);
      readyValueSlotsValid_ = false;
      return;
    }
    // A whole-cell erase was visible as a flicker on the single RGB
    // framebuffer during 100 ms repeats. Keep three fixed digit cells and
    // repaint only cells whose character changed.
    char value[4]{};
    snprintf(value, sizeof(value), "%3lu",
             static_cast<unsigned long>(selected));
    // Digit bounds are at most 22 px wide, entirely inside each 27 px clear
    // cell: [584,611), [611,638), [638,665).  The rightmost glyph ends at
    // x=662, before drawReadyModeLabel restores its field from x=664.
    constexpr int kSlotWidth = 27;
    for (int index = 0; index < 3; ++index) {
      if (readyValueSlotsValid_ && readyValueSlots_[index] == value[index]) continue;
      fillNarrowRect(gfx, kReadyValueX + index * kSlotWidth, kReadyValueY,
                     kSlotWidth, kReadyValueHeight, kBackground);
      if (value[index] != ' ') {
        char glyph[2] = {value[index], '\0'};
        drawCentered(gfx, glyph, kReadyValueCenterX - kSlotWidth +
                         index * kSlotWidth, 82,
                     &Wp7BarlowSemiCondensedBold40, kText, kBackground);
      }
      readyValueSlots_[index] = value[index];
    }
    readyValueSlotsValid_ = true;
  }

  void drawReadyModeLabel(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    const bool timed = surface_.selectedMode() == SessionMode::Endurance;
    // The mode unit is tied to the selected mode and is redrawn only with the
    // mode change, not for every +/- adjustment.
    fillNarrowRect(gfx, kReadyUnitX - 2, kReadyUnitY, kReadyUnitWidth,
                   kReadyUnitHeight, kBackground);
    drawTextTop(gfx, timed ? "MINS" : "LAPS", kReadyUnitX, 96,
                &Wp7BarlowSemiCondensedSemiBold24, kYellow, kBackground);
  }

  void drawReadyTopDelta(Arduino_RGB_Display* gfx, bool modeChanged,
                         bool valueChanged) const {
    if (!gfx) return;
    if (modeChanged) {
      // A mode switch changes only the two mode cells, not the fixed +/-
      // controls.  The numeric cell and its unit label are redrawn below.
      drawReadyModeButtons(gfx);
      drawReadyModeLabel(gfx);
    }
    if (modeChanged || valueChanged) drawReadyValue(gfx);
  }

  void drawBottomMainButton(Arduino_RGB_Display* gfx,
                            const NoticeboardState& state) const {
    if (!gfx) return;
    // The button can change enabled/pressed state while the READY setup
    // request is acknowledged. Restore only its own rectangle; the message
    // strip and all other bottom graphics remain untouched.
    gfx->fillRect(kMainButtonX - 2, kMainButtonY - 2,
                  kMainButtonWidth + 4, kMainButtonHeight + 4,
                  kBackground);
    if (state.lifecycle == SessionLifecycle::Ready) {
      const bool enabled = displayedStartEnabled();
      drawButton(gfx, kMainButtonX, kMainButtonY, kMainButtonWidth,
                 kMainButtonHeight, "START", enabled ? kYellow : kPanel,
                 enabled ? kBackground : kMuted,
                 &Wp7BarlowSemiCondensedSemiBold30,
                 enabled && mainPressed_);
    } else if (state.lifecycle == SessionLifecycle::Finished) {
      drawButton(gfx, kMainButtonX, kMainButtonY, kMainButtonWidth,
                 kMainButtonHeight, "RACE AGAIN", kYellow, kBackground,
                 &Wp7BarlowSemiCondensedSemiBold30,
                 mainPressed_);
    } else if (state.lifecycle == SessionLifecycle::Racing) {
      drawButton(gfx, kMainButtonX, kMainButtonY, kMainButtonWidth,
                 kMainButtonHeight,
                 pauseHoldFeedback_ ? "RELEASE" : "HOLD TO PAUSE",
                 pauseHoldFeedback_ ? kPressed : kPanel,
                 pauseHoldFeedback_ ? kBackground : kText,
                 &Wp7BarlowSemiCondensedSemiBold30,
                 mainPressed_);
    }
  }

  void drawBottomMessage(Arduino_RGB_Display* gfx,
                         const NoticeboardState& state) const {
    // The message occupies the left side of the bottom strip.  Keeping its
    // invalidation cell clear of the main button means a message change cannot
    // erase a button that did not change.
    gfx->fillRect(0, 406, 244, 74, kBackground);
    gfx->drawLine(20, 408, 780, 408, rgb565(20, 36, 50));
    const char* message = state.lifecycle == SessionLifecycle::Ready
                              ? "READY TO START"
                              : state.lifecycle == SessionLifecycle::Finished
                                    ? "RACE FINISHED"
                                    : lifecycleText(state.lifecycle);
    drawTextTop(gfx, message, 28, 426,
                &Wp7BarlowSemiCondensedSemiBold24, kYellow);
  }

  void drawBottom(Arduino_RGB_Display* gfx,
                  const NoticeboardState& state) const {
    drawBottomMessage(gfx, state);
    drawBottomMainButton(gfx, state);
  }

  void drawPausedOverlay(Arduino_RGB_Display* gfx) const {
    if (!gfx) return;
    gfx->fillRoundRect(kPausePanelX, kPausePanelY, kPausePanelWidth,
                       kPausePanelHeight, 12, kPanel);
    gfx->drawRoundRect(kPausePanelX, kPausePanelY, kPausePanelWidth,
                       kPausePanelHeight, 12, kText);
    if (abandonConfirm_) {
      drawCentered(gfx, "ABANDON RACE?", 400, 180,
                   &Wp7BarlowSemiCondensedSemiBold30, kText, kPanel);
      drawCentered(gfx, "This will end the current race.", 400, 218,
                   &Wp7BarlowSemiCondensedSemiBold24, kMuted, kPanel);
      drawButton(gfx, kPauseLeftButtonX, 254, kPauseButtonWidth,
                 kPauseButtonHeight, "CANCEL", kPanel, kText,
                 &Wp7BarlowSemiCondensedSemiBold30);
      drawButton(gfx, kPauseRightButtonX, 254, kPauseButtonWidth,
                 kPauseButtonHeight, "CONFIRM", kYellow, kBackground,
                 &Wp7BarlowSemiCondensedSemiBold30);
    } else {
      drawCentered(gfx, "RACE PAUSED", 400, 180,
                   &Wp7BarlowSemiCondensedSemiBold30, kText, kPanel);
      drawButton(gfx, kPauseLeftButtonX, kPauseButtonY, kPauseButtonWidth,
                 kPauseButtonHeight, "RESUME", kYellow, kBackground,
                 &Wp7BarlowSemiCondensedSemiBold30);
      drawButton(gfx, kPauseRightButtonX, kPauseButtonY, kPauseButtonWidth,
                 kPauseButtonHeight, "ABANDON", kPanel, kText,
                 &Wp7BarlowSemiCondensedSemiBold30);
    }
  }

  uint32_t laneStaticSignature(const NoticeboardState& state,
                               uint8_t lane) const {
    // Mode, target and timing values are rendered by drawLaneMetrics().  The
    // static signature covers only panel identity, so changing READY values
    // cannot promote both lanes to a full-row redraw.
    (void)state;
    char fallback[16]{};
    const char* name = laneName(lane, fallback, sizeof(fallback));
    return tasterPresentationIdentityHash(name, laneGraphic(lane));
  }

  uint32_t laneRenderSignature(const NoticeboardState& state,
                               uint8_t lane) const {
    uint32_t hash = 2166136261U;
    const auto add = [&hash](uint64_t value) {
      hash ^= static_cast<uint32_t>(value);
      hash *= 16777619U;
      hash ^= static_cast<uint32_t>(value >> 32);
      hash *= 16777619U;
    };
    // READY target/unit rendering has its own narrow signature and dirty
    // region.  Keeping it out of this signature prevents a +/- adjustment
    // from repainting the complete lane metrics band.
    add(state.raceElapsed / 1000000ULL);
    add(state.remainingDuration / 1000000ULL);
    add(state.durationExpired ? 1U : 0U);
    add(state.overtime / 1000000ULL);
    add(state.entryCount);
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      const auto& entry = state.entries[i];
      if (entry.lane != lane) continue;
      add(entry.lane);
      add(entry.laps);
      add(entry.hasLap ? 1U : 0U);
      add(entry.lastLapTime);
      add(entry.bestLapTime);
      add(entry.completed ? 1U : 0U);
      add(entry.finishDuration);
      add(entry.hasOvertimeOffset ? 1U : 0U);
      add(entry.overtimeOffset);
      add(static_cast<uint32_t>(entry.gapKind));
      add(entry.lapsBehind);
      add(entry.gapTime);
      return hash;
    }
    add(0);
    return hash;
  }

  void drawFullFrame(Arduino_RGB_Display* gfx,
                     const NoticeboardState& state,
                     uint32_t nowMs) const {
    gfx->fillScreen(kBackground);
#if defined(PP_WP7_STATIC_WELCOME_RASTER)
    // The approved header assets include their native clearance pixels.
    // Draw this static layer once; subsequent countdown/GO passes touch only
    // the independent 60x60 lamp cells.
    drawApprovedRgb565(gfx, wp7HeaderSurroundStart, 155, 0, 538, 70);
    drawApprovedRgb565(gfx, wp7HeaderLogoStart, 13, 6, 145, 55);
#else
    drawCenteredItalic(gfx, "P", 47, 18, &Wp7BarlowSemiCondensedBold40,
                       kYellow, kBackground, 7);
    drawCenteredItalic(gfx, "&", 75, 22, &Wp7BarlowSemiCondensedBold40,
                       kText, kBackground, 5);
    drawCenteredItalic(gfx, "P", 103, 18, &Wp7BarlowSemiCondensedBold40,
                       kYellow, kBackground, 7);
#endif
    drawStartingLights(gfx, state, nowMs);
    drawTopSummary(gfx, state);
    if (kAnimationsEnabled && state.lifecycle == SessionLifecycle::Finished) {
      drawFinishFlag(gfx, static_cast<uint32_t>((nowMs / kAnimationFrameMs) * 5U));
    }
    char lane1Fallback[16]{}, lane2Fallback[16]{};
    drawLaneRow(gfx, 183, kGreen, 1, state,
                laneName(1, lane1Fallback, sizeof(lane1Fallback)), laneCar(1),
                nowMs);
    drawLaneRow(gfx, 299, kRed, 2, state,
                laneName(2, lane2Fallback, sizeof(lane2Fallback)), laneCar(2),
                nowMs);
    drawBottom(gfx, state);
  }

  static Wp7UiComponent lightComponent(uint8_t index) {
    switch (index) {
      case 0: return Wp7UiComponent::Light0;
      case 1: return Wp7UiComponent::Light1;
      case 2: return Wp7UiComponent::Light2;
      case 3: return Wp7UiComponent::Light3;
      default: return Wp7UiComponent::Light4;
    }
  }

  static Wp7UiComponent laneTargetComponent(uint8_t lane) {
    return lane == 1 ? Wp7UiComponent::Lane1Target
                     : Wp7UiComponent::Lane2Target;
  }

  uint32_t laneTargetSignature(const NoticeboardState& state,
                               uint8_t lane) const {
    // Only the READY denominator/unit belongs to this signature.  Lap times,
    // elapsed time and finish fields remain in laneRenderSignature().
    uint32_t hash = 2166136261U;
    const auto add = [&hash](uint64_t value) {
      hash ^= static_cast<uint32_t>(value);
      hash *= 16777619U;
      hash ^= static_cast<uint32_t>(value >> 32);
      hash *= 16777619U;
    };
    add(lane);
    const SessionMode mode = visibleSetupMode(
        state.lifecycle, state.sessionMode, surface_.selectedMode());
    const uint32_t value = visibleSetupValue(
        state.lifecycle, state.sessionMode, state.lapTarget,
        state.durationMinutes, surface_.selectedValue());
    add(static_cast<uint32_t>(mode));
    add(value);
    const NoticeboardEntryState* entry = nullptr;
    for (uint8_t i = 0; i < state.entryCount; ++i) {
      if (state.entries[i].lane == lane) {
        entry = &state.entries[i];
        break;
      }
    }
    add(entry ? 1U : 0U);
    add(entry ? entry->laps : 0U);
    return hash;
  }

  static uint32_t estimatedCacheWriteBytes(const DirtyRegionSet& dirty) {
    // Match the two cache paths used by this renderer: fillNarrowRect() uses
    // narrow writeFastHLine() spans, while legacy fillRect-based components
    // still write back complete framebuffer-width scanline spans.  Glyph
    // writePixel calls and rounded primitives add small extra writes, so this
    // remains a conservative region-level estimate rather than a claim about
    // every individual cache line.
    uint64_t bytes = 0;
    for (size_t i = 0; i < dirty.count(); ++i) {
      const UiDirtyRegion& region = dirty.at(i);
      const UiRect& rect = region.rect;
      const uint64_t components = region.components;
      bool accountedNarrow = false;
      bool hasBroadComponent = false;
      uint64_t narrow = 0;
      const auto addNarrow = [&](Wp7UiComponent component) {
        if ((components & uiComponentBit(component)) == 0) return;
        const UiRect componentRect = Wp7DirtyLayout::rect(component);
        narrow += static_cast<uint64_t>(componentRect.width) *
                  static_cast<uint64_t>(componentRect.height) *
                  sizeof(uint16_t);
        accountedNarrow = true;
      };
      addNarrow(Wp7UiComponent::ReadyValue);
      addNarrow(Wp7UiComponent::ReadyUnit);
      addNarrow(Wp7UiComponent::Lane1Target);
      addNarrow(Wp7UiComponent::Lane2Target);
      for (uint8_t component = 0;
           component < static_cast<uint8_t>(Wp7UiComponent::Count);
           ++component) {
        const auto value = static_cast<Wp7UiComponent>(component);
        if ((components & uiComponentBit(value)) == 0) continue;
        if (value != Wp7UiComponent::ReadyValue &&
            value != Wp7UiComponent::ReadyUnit &&
            value != Wp7UiComponent::Lane1Target &&
            value != Wp7UiComponent::Lane2Target) {
          hasBroadComponent = true;
          break;
        }
      }
      if (accountedNarrow && !hasBroadComponent) {
        bytes += narrow;
      } else {
        bytes += static_cast<uint64_t>(kPanelWidth) *
                 static_cast<uint64_t>(rect.height) * sizeof(uint16_t);
      }
    }
    return bytes > 0xFFFFFFFFULL ? 0xFFFFFFFFU
                                 : static_cast<uint32_t>(bytes);
  }

  static void reportDirtyRegions(const DirtyRegionSet& dirty,
                                 FullFrameReason fullFrameReason) {
    Serial.printf("[WP7 PHYS] redraw regions=%u mask=0x%llX reason=%s",
                  static_cast<unsigned>(dirty.count()),
                  static_cast<unsigned long long>(dirty.mask()),
                  fullFrameReasonText(fullFrameReason));
    for (size_t i = 0; i < dirty.count(); ++i) {
      const UiDirtyRegion& region = dirty.at(i);
      Serial.printf(" [%d,%d %dx%d:", region.rect.x, region.rect.y,
                    region.rect.width, region.rect.height);
      for (uint8_t component = 0;
           component < static_cast<uint8_t>(Wp7UiComponent::Count);
           ++component) {
        const auto value = static_cast<Wp7UiComponent>(component);
        if (region.components & uiComponentBit(value)) {
          Serial.printf("%s,", Wp7DirtyLayout::name(value));
        }
      }
      Serial.print("]");
    }
    Serial.println();
  }

  void drawCurrent(Time now) {
    Arduino_RGB_Display* gfx = display_.touchTestCanvas();
    if (!gfx) return;
    const uint32_t currentMs = millis();
    const NoticeboardState state = authority_.stateAt(now);
    if (state.lifecycle == SessionLifecycle::Paused &&
        lastState_.lifecycle != SessionLifecycle::Paused) {
      pausedWarningStartedMs_ = currentMs;
    } else if (state.lifecycle != SessionLifecycle::Paused) {
      pausedWarningStartedMs_ = currentMs;
    }
    if (state.lifecycle != SessionLifecycle::Paused) abandonConfirm_ = false;
    observeAnimationEvents(state, currentMs);
    const bool lane1AnimationNow = laneAnimationActive(1, currentMs);
    const bool lane2AnimationNow = laneAnimationActive(2, currentMs);
    const bool finishFlagVisible = kAnimationsEnabled &&
                                   state.lifecycle == SessionLifecycle::Finished;
    const uint32_t signature = renderSignature(state, currentMs);
    if (renderSignatureValid_ && signature == lastRenderSignature_ &&
        !lane1AnimationNow && !lane2AnimationNow &&
        !lastLane1AnimationActive_ && !lastLane2AnimationActive_ &&
        !finishFlagVisible) return;
    if (!display_.beginFrame()) return;

    const bool initial = !renderSignatureValid_;
    const bool lifecycleChanged = initial || state.lifecycle != lastState_.lifecycle;
    // GO does not alter the screen's structure.  Redrawing the full framebuffer
    // here made the otherwise-local red-to-green change visibly refresh the
    // panel.  Its individual changing fields are invalidated below instead.
    const bool goTransition =
        lastState_.lifecycle == SessionLifecycle::Starting &&
        state.lifecycle == SessionLifecycle::Racing;
    const SessionMode visibleMode = visibleSetupMode(
        state.lifecycle, state.sessionMode, surface_.selectedMode());
    const SessionMode lastVisibleMode = visibleSetupMode(
        lastState_.lifecycle, lastState_.sessionMode, lastSelectedMode_);
    const bool modeChanged = visibleMode != lastVisibleMode;
    const bool readyValueChanged =
        surface_.selectedValue() != lastSelectedValue_;
    const bool lane1StaticChanged =
        laneStaticSignature(state, 1) != laneStaticSignature(lastState_, 1);
    const bool lane2StaticChanged =
        laneStaticSignature(state, 2) != laneStaticSignature(lastState_, 2);
    const bool lane1MetricsChanged =
        laneRenderSignature(state, 1) != laneRenderSignature(lastState_, 1);
    const bool lane2MetricsChanged =
        laneRenderSignature(state, 2) != laneRenderSignature(lastState_, 2);
    const bool lane1TargetChanged =
        laneTargetSignature(state, 1) != laneTargetSignature(lastState_, 1);
    const bool lane2TargetChanged =
        laneTargetSignature(state, 2) != laneTargetSignature(lastState_, 2);
    // Pending setup is represented by startEnabled().  Do not repaint the
    // button a second time merely because the authority has acknowledged the
    // same value; repaint only when its visible enabled/pressed state changes.
    const bool mainButtonChanged = startButtonVisualChanged(
        mainPressed_, lastMainPressed_, displayedStartEnabled(),
        lastStartEnabled_) ||
        pauseHoldFeedback_ != lastPauseHoldFeedback_;

    DirtyRegionSet dirty;
    FullFrameReason fullFrameReason = FullFrameReason::None;
    const uint8_t warningPhase = pausedWarningLightsEnabled()
                                     ? lightPhaseFor(currentMs)
                                     : 0;
    if (welcomeTransitionPending_) {
      dirty.mark(Wp7UiComponent::FullFrame);
      fullFrameReason = FullFrameReason::WelcomeTransition;
      welcomeTransitionPending_ = false;
    } else if (initial) {
      dirty.mark(Wp7UiComponent::FullFrame);
      fullFrameReason = FullFrameReason::InitialDraw;
    } else if (lifecycleChanged && !goTransition) {
      dirty.mark(Wp7UiComponent::FullFrame);
      fullFrameReason = FullFrameReason::LifecycleTransition;
    } else {
      const uint8_t previousWarningPhase = lastPausedWarningPhase_;
      Wp7PartialInvalidationInput invalidation{};
      invalidation.readyScreen = state.lifecycle == SessionLifecycle::Ready;
      invalidation.modeChanged = modeChanged;
      invalidation.readyValueChanged = readyValueChanged;
      invalidation.activeTopChanged =
          state.sessionMode != lastState_.sessionMode ||
          state.lapTarget != lastState_.lapTarget ||
          state.durationMinutes != lastState_.durationMinutes;
      for (uint8_t index = 0; index < 5; ++index) {
        invalidation.lightColors[index] =
            startingLightColorForPhase(state, index, warningPhase);
        invalidation.previousLightColors[index] =
            startingLightColorForPhase(lastState_, index,
                                       previousWarningPhase);
      }
      invalidation.lane1AnimationNow = lane1AnimationNow;
      invalidation.lane1AnimationPreviouslyActive =
          lastLane1AnimationActive_;
      invalidation.lane1StaticChanged = lane1StaticChanged;
      invalidation.lane1MetricsChanged = lane1MetricsChanged || goTransition;
      invalidation.lane1TargetChanged = lane1TargetChanged || goTransition;
      invalidation.lane2AnimationNow = lane2AnimationNow;
      invalidation.lane2AnimationPreviouslyActive =
          lastLane2AnimationActive_;
      invalidation.lane2StaticChanged = lane2StaticChanged;
      invalidation.lane2MetricsChanged = lane2MetricsChanged || goTransition;
      invalidation.lane2TargetChanged = lane2TargetChanged || goTransition;
      invalidation.bottomMessageChanged = goTransition;
      invalidation.mainButtonChanged = mainButtonChanged || goTransition;
      invalidation.pauseOverlayChanged =
          state.lifecycle == SessionLifecycle::Paused &&
          abandonConfirm_ != lastAbandonConfirm_;
      invalidation.finishFlagVisible = finishFlagVisible;
      dirty = planWp7PartialInvalidation(invalidation);
    }
    if (dirty.overflowed()) {
      fullFrameReason = FullFrameReason::DirtyRegionOverflow;
    } else if (dirty.empty()) {
      dirty.mark(Wp7UiComponent::FullFrame);
      fullFrameReason = FullFrameReason::UnmappedSignatureChange;
      Serial.printf(
          "[WP7 PHYS] full-frame signature-change current=0x%08lX "
          "previous=0x%08lX state_mode=%u visible_mode=%u state_laps=%lu "
          "state_duration=%u visible_value=%lu setup_pending=%u "
          "proposal_revision=%lu\n",
          static_cast<unsigned long>(signature),
          static_cast<unsigned long>(lastRenderSignature_),
          static_cast<unsigned>(state.sessionMode),
          static_cast<unsigned>(visibleMode),
          static_cast<unsigned long>(state.lapTarget),
          static_cast<unsigned>(state.durationMinutes),
          static_cast<unsigned long>(visibleSetupValue(
              state.lifecycle, state.sessionMode, state.lapTarget,
              state.durationMinutes, surface_.selectedValue())),
          surface_.setupPending() ? 1U : 0U,
          static_cast<unsigned long>(state.proposalRevision));
    }
    reportDirtyRegions(dirty, fullFrameReason);
    const uint32_t renderStartedUs = micros();
    const uint32_t cacheWritebackEstimate = estimatedCacheWriteBytes(dirty);

    if (dirty.has(Wp7UiComponent::FullFrame)) {
      drawFullFrame(gfx, state, currentMs);
      if (state.lifecycle == SessionLifecycle::Paused) drawPausedOverlay(gfx);
    } else {
      const int lightCenters[] = {273, 351, 429, 507, 585};
      for (uint8_t index = 0; index < 5; ++index) {
        if (dirty.has(lightComponent(index))) {
          drawStartingLight(gfx, lightCenters[index],
                            startingLightColorForPhase(state, index,
                                                       warningPhase));
        }
      }
      if (dirty.has(Wp7UiComponent::ReadyModeLaps) ||
          dirty.has(Wp7UiComponent::ReadyModeTimed)) {
        drawReadyModeButtons(gfx);
      }
      if (dirty.has(Wp7UiComponent::ReadyValue)) drawReadyValue(gfx);
      if (dirty.has(Wp7UiComponent::ReadyUnit)) drawReadyModeLabel(gfx);
      if (dirty.has(Wp7UiComponent::ActiveTop)) drawTopSummary(gfx, state);
      if (dirty.has(Wp7UiComponent::FinishFlag)) {
        drawFinishFlag(gfx,
                       static_cast<uint32_t>((currentMs / kAnimationFrameMs) * 5U));
      }
      if (dirty.has(Wp7UiComponent::Lane1Static)) {
        char fallback[16]{};
        drawLaneStatic(gfx, 183, kGreen, 1,
                       laneName(1, fallback, sizeof(fallback)), laneCar(1),
                       currentMs);
      }
      if (dirty.has(Wp7UiComponent::Lane1Metrics)) {
        drawLaneMetrics(gfx, 183, 1, state);
      }
      if (dirty.has(Wp7UiComponent::Lane1Target)) {
        drawLaneLapsField(gfx, 183, 1, state, true);
      }
      if (dirty.has(Wp7UiComponent::Lane2Static)) {
        char fallback[16]{};
        drawLaneStatic(gfx, 299, kRed, 2,
                       laneName(2, fallback, sizeof(fallback)), laneCar(2),
                       currentMs);
      }
      if (dirty.has(Wp7UiComponent::Lane2Metrics)) {
        drawLaneMetrics(gfx, 299, 2, state);
      }
      if (dirty.has(Wp7UiComponent::Lane2Target)) {
        drawLaneLapsField(gfx, 299, 2, state, true);
      }
      if (dirty.has(Wp7UiComponent::BottomMessage)) {
        drawBottomMessage(gfx, state);
      }
      if (dirty.has(Wp7UiComponent::MainButton)) {
        drawBottomMainButton(gfx, state);
      }
      if (dirty.has(Wp7UiComponent::PauseOverlay)) drawPausedOverlay(gfx);
    }
    if (!display_.presentFrame()) return;
    Serial.printf("[WP7 PHYS] render complete duration_us=%lu "
                  "cache_row_writeback_estimate=%lu adjustment_seq=%lu\n",
                  static_cast<unsigned long>(micros() - renderStartedUs),
                  static_cast<unsigned long>(cacheWritebackEstimate),
                  static_cast<unsigned long>(adjustmentSequence_));
    lastState_ = state;
    lastMainPressed_ = mainPressed_;
    lastPauseHoldFeedback_ = pauseHoldFeedback_;
    lastSelectedMode_ = surface_.selectedMode();
    lastSelectedValue_ = surface_.selectedValue();
    lastStartEnabled_ = displayedStartEnabled();
    lastAbandonConfirm_ = abandonConfirm_;
    lastRenderSignature_ = signature;
    lastLane1AnimationActive_ = lane1AnimationNow;
    lastLane2AnimationActive_ = lane2AnimationNow;
    lastPausedWarningPhase_ = lightPhaseFor(currentMs);
    if (!lane1AnimationNow) laneAnimations_[0].active = false;
    if (!lane2AnimationNow) laneAnimations_[1].active = false;
    renderSignatureValid_ = true;
  }

  static bool inRect(int x, int y, int left, int top, int width, int height) {
    return x >= left && x < left + width && y >= top && y < top + height;
  }

  uint8_t touchTarget(int x, int y, SessionLifecycle lifecycle) const {
    if (lifecycle == SessionLifecycle::Paused) {
      if (abandonConfirm_) {
        if (inRect(x, y, kPauseLeftButtonX, 254, kPauseButtonWidth,
                   kPauseButtonHeight)) return TouchCancel;
        if (inRect(x, y, kPauseRightButtonX, 254, kPauseButtonWidth,
                   kPauseButtonHeight)) return TouchConfirm;
      } else {
        if (inRect(x, y, kPauseLeftButtonX, kPauseButtonY,
                   kPauseButtonWidth, kPauseButtonHeight)) return TouchResume;
        if (inRect(x, y, kPauseRightButtonX, kPauseButtonY,
                   kPauseButtonWidth, kPauseButtonHeight)) return TouchAbandon;
      }
      return TouchNone;
    }
    if (lifecycle == SessionLifecycle::Ready) {
      if (inRect(x, y, kReadyLapsX, kReadyModeY, kReadyModeWidth, kReadyModeHeight)) return TouchModeLap;
      if (inRect(x, y, kReadyTimedX, kReadyModeY, kReadyModeWidth, kReadyModeHeight)) return TouchModeTimed;
      if (inRect(x, y, kReadyMinusX, kReadyModeY, kReadyStepWidth, kReadyStepHeight)) return TouchDecrease;
      if (inRect(x, y, kReadyPlusX, kReadyModeY, kReadyStepWidth, kReadyStepHeight)) return TouchIncrease;
    }
    if (inRect(x, y, kMainButtonX, kMainButtonY, kMainButtonWidth,
               kMainButtonHeight)) return TouchMain;
    return TouchNone;
  }

  // Share the model's correlation allocator while preserving the existing
  // direct physical operation submission behavior.
  void publishOperation(SessionOperation operation, bool confirmed, const char* name) {
    const uint32_t correlation = surface_.allocateCorrelation();
    const bool delivered = authority_.submitOperation(correlation, operation, confirmed);
    Serial.printf("[WP7 PHYS] %s request=%s correlation=%lu\n", name,
                  delivered ? "PUBLISHED" : "REJECTED",
                  static_cast<unsigned long>(correlation));
  }

  void publishResume() { publishOperation(SessionOperation::Resume, false, "RESUME"); }
  void publishAbandon() { publishOperation(SessionOperation::EndRace, true, "ABANDON"); }
  void publishPause() { publishOperation(SessionOperation::Pause, false, "PAUSE"); }
  void publishRaceAgain() { publishOperation(SessionOperation::RaceAgain, false, "RACE_AGAIN"); }

  void pollTouch(uint32_t now) {
    if (!touchReady_ || static_cast<int32_t>(now - nextPollMs_) < 0) return;
    nextPollMs_ = now + kPollIntervalMs;
    // Invalid/short samples are not releases.  A valid 0x00 response likewise
    // retains the adapter's last contact state, so existing +/- repeats keep
    // their cadence between fresh GT911 reports.
    if (!touch_.read()) return;
    const bool isDown = touch_.isTouched;
    if (isDown) {
      const uint16_t rawX = touch_.points[0].x;
      const uint16_t rawY = touch_.points[0].y;
      const int mappedX = mapClamped(rawX, kMapX1, kMapX2, 0, kPanelWidth - 1);
      const int mappedY = mapClamped(rawY, kMapY1, kMapY2, 0, kPanelHeight - 1);
      if (!touchState_.down) {
        touchState_.down = true;
        touchState_.downAt = now;
        touchState_.missingSince = 0;
        touchState_.lifecycle = authority_.lifecycle();
        touchState_.target = touchTarget(mappedX, mappedY, touchState_.lifecycle);
        touchState_.mainTarget = touchState_.target == TouchMain;
        touchState_.adjustmentTarget =
            touchState_.target == TouchDecrease ||
            touchState_.target == TouchIncrease;
        if (touchState_.adjustmentTarget) {
          touchState_.adjustmentDelta =
              touchState_.target == TouchDecrease ? -1 : 1;
          touchState_.nextAdjustmentAt = now + kAdjustRepeatDelayMs;
          touchState_.adjustmentIntervalMs =
              adjustmentRepeatIntervalMs(touchState_.adjustmentRepeats);
        }
        mainPressed_ = touchState_.mainTarget;
        pauseHoldFeedback_ = false;
        const char* target = touchState_.target == TouchMain ? "MAIN" :
                             touchState_.target == TouchResume ? "RESUME" :
                             touchState_.target == TouchAbandon ? "ABANDON" :
                             touchState_.target == TouchCancel ? "CANCEL" :
                             touchState_.target == TouchConfirm ? "CONFIRM" : "OTHER";
        Serial.printf("[WP7 PHYS] TOUCH DOWN mapped=(%d,%d) target=%s state=%s\n",
                      mappedX, mappedY, target,
                      lifecycleText(touchState_.lifecycle));
      }
      // A confirmed contact return preserves the original hold origin and
      // repeat phase.  Move a missed deadline forward, rather than replaying
      // it, so an interrupted hold cannot overshoot on contact recovery.
      const bool adjustmentContactRecovered =
          touchState_.adjustmentTarget && touchState_.missingSince != 0;
      if (adjustmentContactRecovered) {
        touchState_.nextAdjustmentAt = adjustmentDeadlineAfter(
            touchState_.nextAdjustmentAt, touchState_.adjustmentIntervalMs, now);
        touchState_.missingSince = 0;
      }
      touchState_.mappedX = mappedX;
      touchState_.mappedY = mappedY;
      if (pauseHoldFeedbackActive(touchState_.lifecycle,
                                  touchState_.mainTarget,
                                  now - touchState_.downAt)) {
        pauseHoldFeedback_ = true;
      }
      if (touchState_.adjustmentTarget &&
          touchState_.lifecycle == SessionLifecycle::Ready &&
          static_cast<int32_t>(now - touchState_.nextAdjustmentAt) >= 0) {
        const uint32_t heldMs = now - touchState_.downAt;
        const bool tenStep = heldMs >= kAdjustmentTenStepAfterMs;
        const int delta = adjustmentHoldDelta(touchState_.adjustmentDelta, heldMs);
        const uint32_t before = surface_.selectedValue();
        const bool changed = surface_.adjustPreview(delta);
        const uint32_t after = surface_.selectedValue();
        if (changed) logAdjustment(delta, before, after);
        // Once the hold has crossed the repeat boundary, release must not add
        // a second tap-sized adjustment.  At 1/999 the held gesture remains
        // consumed even though the bounded value cannot change further.
        touchState_.adjustmentRepeated = true;
        ++touchState_.adjustmentRepeats;
        touchState_.adjustmentIntervalMs = tenStep
            ? kAdjustmentTenStepIntervalMs
            : adjustmentRepeatIntervalMs(touchState_.adjustmentRepeats);
        // Keep the periodic schedule anchored to its preceding deadline.
        // A late loop produces at most one adjustment; missed slots are not
        // replayed on this or a later loop.
        touchState_.nextAdjustmentAt = adjustmentDeadlineAfter(
            touchState_.nextAdjustmentAt, touchState_.adjustmentIntervalMs, now);
      }
    } else if (touchState_.down) {
      if (!touchState_.missingSince) touchState_.missingSince = now;
      // +/- controls need a longer confirmation because the physical trace
      // showed complete empty GT911 samples during a ten-second press.  Do
      // not adjust while this contact is uncertain.  All other controls keep
      // their established 80 ms release behaviour.
      const bool releaseConfirmed = touchState_.adjustmentTarget
          ? adjustmentReleaseConfirmed(touchState_.missingSince, now)
          : now - touchState_.missingSince >= kReleaseDebounceMs;
      if (!releaseConfirmed) return;
      const uint32_t heldMs = now - touchState_.downAt;
      const bool hold = touchHoldThresholdReached(heldMs);
      Serial.printf("[WP7 PHYS] TOUCH UP mapped=(%d,%d) held_ms=%lu classification=%s\n",
                    touchState_.mappedX, touchState_.mappedY,
                    static_cast<unsigned long>(heldMs), hold ? "HOLD" : "SHORT");
      if (touchState_.mainTarget) {
        if (touchState_.lifecycle == SessionLifecycle::Ready) surface_.start();
        else if (touchState_.lifecycle == SessionLifecycle::Racing && hold) publishPause();
        else if (touchState_.lifecycle == SessionLifecycle::Finished && !hold) publishRaceAgain();
      } else if (touchState_.target == TouchModeLap && !hold) {
        surface_.selectMode(SessionMode::LapRace);
      } else if (touchState_.target == TouchModeTimed && !hold) {
        surface_.selectMode(SessionMode::Endurance);
      } else if (touchState_.adjustmentTarget) {
        if (!touchState_.adjustmentRepeated) {
          const uint32_t before = surface_.selectedValue();
          const bool changed =
              surface_.adjustPreview(touchState_.adjustmentDelta);
          const uint32_t after = surface_.selectedValue();
          if (changed) logAdjustment(touchState_.adjustmentDelta, before, after);
        }
        // A complete touch gesture is committed once, after all local repeats
        // have settled.  This keeps the existing A/B storage contract while
        // avoiding one NVS transaction for every repeat tick.
        surface_.commitAdjustment();
      } else if (touchState_.target == TouchResume && !hold) {
        publishResume();
      } else if (touchState_.target == TouchAbandon && !hold) {
        abandonConfirm_ = true;
      } else if (touchState_.target == TouchCancel && !hold) {
        abandonConfirm_ = false;
      } else if (touchState_.target == TouchConfirm && !hold) {
        publishAbandon();
      }
      touchState_ = TouchState{};
      mainPressed_ = false;
      pauseHoldFeedback_ = false;
    }
  }

  void logAdjustment(int delta, uint32_t before, uint32_t after) {
    ++adjustmentSequence_;
    Serial.printf("[WP8 DIAG] adjust seq=%lu t_ms=%lu mode=%u delta=%d "
                  "value_before=%lu value_after=%lu\n",
                  static_cast<unsigned long>(adjustmentSequence_),
                  static_cast<unsigned long>(millis()),
                  static_cast<unsigned>(surface_.selectedMode()), delta,
                  static_cast<unsigned long>(before),
                  static_cast<unsigned long>(after));
  }

  Esp32S3BoardDisplay& display_;
  PhysicalTasterAuthority& authority_;
  TasterSurfaceModel surface_;
  TAMC_GT911 touch_;
  TouchState touchState_{};
  uint8_t detectedAddress_ = 0;
  uint32_t nextPollMs_ = 0;
  uint32_t nextDrawMs_ = 0;
  uint32_t nextTouchDiagnosticMs_ = 0;
  uint32_t adjustmentSequence_ = 0;
  mutable char readyValueSlots_[3]{};
  mutable bool readyValueSlotsValid_ = false;
  bool touchReady_ = false;
  bool welcomeActive_ = false;
  bool startupComplete_ = false;
  bool startupFailed_ = false;
  bool welcomeFirstPresented_ = false;
  bool welcomeDrawn_ = false;
  bool welcomeFailureDrawn_ = false;
  bool readyPresentedLogged_ = false;
  bool welcomeTransitionPending_ = false;
  uint8_t welcomeProgress_ = 255;
  const char* welcomeStatus_ = nullptr;
  uint8_t startupProgress_ = 0;
  const char* startupStatus_ = "INITIALISING DISPLAY";
  uint32_t welcomeFirstPresentedMs_ = 0;
  bool mainPressed_ = false;
  bool pauseHoldFeedback_ = false;
  bool abandonConfirm_ = false;
  bool lastAbandonConfirm_ = false;
  NoticeboardState lastState_{};
  bool lastMainPressed_ = false;
  bool lastPauseHoldFeedback_ = false;
  SessionMode lastSelectedMode_ = SessionMode::LapRace;
  uint32_t lastSelectedValue_ = 10;
  bool lastStartEnabled_ = false;
  uint32_t lastRenderSignature_ = 0;
  bool renderSignatureValid_ = false;
  uint32_t pausedWarningStartedMs_ = 0;
  uint8_t lastPausedWarningPhase_ = 0;
  LaneAnimation laneAnimations_[2]{};
  bool lastLane1AnimationActive_ = false;
  bool lastLane2AnimationActive_ = false;
};

}  // namespace pp
