#pragma once

// The ESP32-8048S070 RGB panel is a board surface, not a second source of
// product authority.  This small adapter only acknowledges boot readiness;
// P&P state remains authoritative in the existing controller/browser paths.

#if defined(PP_PHASE8B_REAL_SD) || defined(PP_WP7_PHYSICAL_RACING_TEST)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <cstring>

#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
#include <esp_cache.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace pp {

#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)

// Arduino_GFX's stock RGB wrapper exposes one framebuffer and only performs a
// cache write-back on flush().  The physical racing surface needs the ESP-IDF
// direct-mode double-buffer contract instead: render into the offline buffer,
// ask the RGB driver to switch at VSYNC, then wait until the old buffer is safe
// to reuse.  The drawing API remains Arduino_GFX; only its framebuffer/present
// boundary is adapted here.
class Wp7DoubleBufferedDisplay final : public Arduino_RGB_Display {
  static constexpr int kBacklight = 2;

public:
  Wp7DoubleBufferedDisplay()
      : Arduino_RGB_Display(kDisplayWidth, kDisplayHeight, nullptr, 0, false,
                            nullptr, GFX_NOT_DEFINED) {}

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    (void)speed;
    delay(300);

    // Preserve the exact backlight sequence used by the known-good
    // Arduino_GFX path. The direct ESP-IDF panel does not receive the old
    // reset/backlight pin argument, so GPIO2 must be driven explicitly.
    pinMode(kBacklight, OUTPUT);
    digitalWrite(kBacklight, HIGH);
    delay(100);
    digitalWrite(kBacklight, LOW);
    delay(120);
    digitalWrite(kBacklight, HIGH);
    delay(120);
    Serial.printf("[DISPLAY] backlight gpio=%d level=%d\n",
                  kBacklight, digitalRead(kBacklight));

    esp_lcd_rgb_panel_config_t config{};
#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
    config.clk_src = LCD_CLK_SRC_PLL160M;
#else
    config.clk_src = LCD_CLK_SRC_DEFAULT;
#endif
    config.timings.pclk_hz = 12000000;
    config.timings.h_res = kDisplayWidth;
    config.timings.v_res = kDisplayHeight;
    config.timings.hsync_pulse_width = 30;
    config.timings.hsync_back_porch = 16;
    config.timings.hsync_front_porch = 210;
    config.timings.vsync_pulse_width = 13;
    config.timings.vsync_back_porch = 10;
    config.timings.vsync_front_porch = 22;
    config.timings.flags.hsync_idle_low = 1;
    config.timings.flags.vsync_idle_low = 1;
    config.timings.flags.de_idle_high = 0;
    config.timings.flags.pclk_active_neg = 1;
    config.timings.flags.pclk_idle_high = 0;
    config.data_width = 16;
    config.bits_per_pixel = 16;
    config.num_fbs = 2;
    // Direct framebuffer mode is required for ESP-IDF's frame-buffer link
    // switching.  When bounce buffers are enabled, the RGB driver keeps the
    // scanout DMA on its bounce path and draw_bitmap(back_) does not select the
    // offline framebuffer.  The two PSRAM framebuffers remain the only scanout
    // buffers; presentation is synchronized by on_frame_buf_complete below.
    config.bounce_buffer_size_px = 0;
    config.sram_trans_align = 8;
    config.psram_trans_align = 64;
    config.hsync_gpio_num = kTftHsync;
    config.vsync_gpio_num = kTftVsync;
    config.de_gpio_num = kTftDe;
    config.pclk_gpio_num = kTftPclk;
    config.disp_gpio_num = GPIO_NUM_NC;
    config.data_gpio_nums[0] = kTftB0;
    config.data_gpio_nums[1] = kTftB1;
    config.data_gpio_nums[2] = kTftB2;
    config.data_gpio_nums[3] = kTftB3;
    config.data_gpio_nums[4] = kTftB4;
    config.data_gpio_nums[5] = kTftG0;
    config.data_gpio_nums[6] = kTftG1;
    config.data_gpio_nums[7] = kTftG2;
    config.data_gpio_nums[8] = kTftG3;
    config.data_gpio_nums[9] = kTftG4;
    config.data_gpio_nums[10] = kTftG5;
    config.data_gpio_nums[11] = kTftR0;
    config.data_gpio_nums[12] = kTftR1;
    config.data_gpio_nums[13] = kTftR2;
    config.data_gpio_nums[14] = kTftR3;
    config.data_gpio_nums[15] = kTftR4;
    config.flags.disp_active_low = 1;
    config.flags.refresh_on_demand = 0;
    config.flags.fb_in_psram = 1;
    config.flags.double_fb = 0;
    config.flags.no_fb = 0;
    config.flags.bb_invalidate_cache = 0;

    if (esp_lcd_new_rgb_panel(&config, &panel_) != ESP_OK ||
        esp_lcd_panel_reset(panel_) != ESP_OK ||
        esp_lcd_panel_init(panel_) != ESP_OK) {
      Serial.println("[DISPLAY] ESP32-8048S070 init FAILED");
      panel_ = nullptr;
      return false;
    }

    void* first = nullptr;
    void* second = nullptr;
    if (esp_lcd_rgb_panel_get_frame_buffer(panel_, 2, &first, &second) != ESP_OK ||
        !first || !second) {
      Serial.println("[DISPLAY] double framebuffer allocation FAILED");
      return false;
    }
    front_ = static_cast<uint16_t*>(first);
    back_ = static_cast<uint16_t*>(second);
    _framebuffer = front_;
    Serial.printf("[DISPLAY] framebuffer front=%p back=%p bytes_each=%u\n",
                  static_cast<void*>(front_), static_cast<void*>(back_),
                  static_cast<unsigned>(kFrameBytes));

    frameDone_ = xSemaphoreCreateBinary();
    if (!frameDone_) {
      Serial.println("[DISPLAY] VSYNC completion semaphore FAILED");
      return false;
    }
    esp_lcd_rgb_panel_event_callbacks_t callbacks{};
    callbacks.on_frame_buf_complete = frameCompleteIsr;
    if (esp_lcd_rgb_panel_register_event_callbacks(panel_, &callbacks, this) != ESP_OK) {
      Serial.println("[DISPLAY] VSYNC callback registration FAILED");
      return false;
    }
    healthy_ = true;
    return true;
  }

  bool beginFrame() {
    if (!healthy_ || !front_ || !back_) return false;
    // The UI retains its region invalidation logic.  Copying the last complete
    // frame first gives the offline buffer the same stationary pixels before
    // the changed rows/regions are redrawn.
    memcpy(back_, front_, kFrameBytes);
    _framebuffer = back_;
    return true;
  }

  bool presentFrame() {
    if (!healthy_ || !panel_ || !back_) return false;
    // Arduino_GFX 1.6.5 uses the legacy Cache_WriteBack_Addr() helper.  That
    // helper is not the source-built ESP-IDF 5.5 cache boundary used by the
    // proven diagnostic, so keep the adapter's presentation contract on the
    // supported esp_cache_msync() API instead.
    const esp_err_t cacheResult = esp_cache_msync(
        back_, kFrameBytes,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (cacheResult != ESP_OK) {
      Serial.printf("[DISPLAY] frame cache sync FAILED err=%s\n",
                    esp_err_to_name(cacheResult));
      healthy_ = false;
      return false;
    }
    while (xSemaphoreTake(frameDone_, 0) == pdTRUE) {
    }
    if (esp_lcd_panel_draw_bitmap(panel_, 0, 0, kDisplayWidth, kDisplayHeight,
                                  back_) != ESP_OK) {
      Serial.println("[DISPLAY] frame switch request FAILED");
      healthy_ = false;
      return false;
    }
    if (xSemaphoreTake(frameDone_, pdMS_TO_TICKS(250)) != pdTRUE) {
      Serial.println("[DISPLAY] frame switch VSYNC timeout; freezing surface");
      healthy_ = false;
      return false;
    }
    uint16_t* oldFront = front_;
    front_ = back_;
    back_ = oldFront;
    _framebuffer = back_;
    ++framePresentCount_;
    if (framePresentCount_ <= 3) {
      Serial.printf("[DISPLAY] frame presentation OK count=%lu front=%p back=%p\n",
                    static_cast<unsigned long>(framePresentCount_),
                    static_cast<void*>(front_), static_cast<void*>(back_));
    }
    return true;
  }

  bool healthy() const { return healthy_; }

  static constexpr size_t frameBytes() {
    return kFrameBytes;
  }

private:
  static constexpr int kDisplayWidth = 800;
  static constexpr int kDisplayHeight = 480;
  static constexpr size_t kBounceBufferSizePx = 20 * kDisplayWidth;
  static constexpr size_t kFrameBytes =
      static_cast<size_t>(kDisplayWidth) * kDisplayHeight * sizeof(uint16_t);

  static bool IRAM_ATTR frameCompleteIsr(
      esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void* user) {
    auto* self = static_cast<Wp7DoubleBufferedDisplay*>(user);
    if (!self || !self->frameDone_) return false;
    BaseType_t highPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->frameDone_, &highPriorityTaskWoken);
    return highPriorityTaskWoken == pdTRUE;
  }

  esp_lcd_panel_handle_t panel_ = nullptr;
  SemaphoreHandle_t frameDone_ = nullptr;
  uint16_t* front_ = nullptr;
  uint16_t* back_ = nullptr;
  uint32_t framePresentCount_ = 0;
  bool healthy_ = false;

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
};

#endif  // PP_WP7_PHYSICAL_RACING_TEST && PP_WP7_IDF_DOUBLE_BUFFER && !PP_WP7_SINGLE_FB_RECOVERY

class Esp32S3BoardDisplay final {
public:
  Esp32S3BoardDisplay() {
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
    display_ = new Wp7DoubleBufferedDisplay();
#else
    // Match the known-working project: allocate the panel objects during
    // global board-display construction, but defer begin() and scanout.
    displayBus_ = new Arduino_ESP32RGBPanel(
        kTftDe, kTftVsync, kTftHsync, kTftPclk,
        kTftR0, kTftR1, kTftR2, kTftR3, kTftR4,
        kTftG0, kTftG1, kTftG2, kTftG3, kTftG4, kTftG5,
        kTftB0, kTftB1, kTftB2, kTftB3, kTftB4,
        0, 210, 30, 16, 0, 22, 13, 10, 1, 12000000,
        false, 0, 0, kBounceBufferSizePx);
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
    // The racing surface publishes one completed framebuffer after its
    // changed regions are drawn.  Keep primitive-level cache write-back off
    // so the panel never observes the intermediate clear/draw sequence.
    const bool autoFlush = false;
#else
    const bool autoFlush = true;
#endif
    display_ = new Arduino_RGB_Display(
        kDisplayWidth, kDisplayHeight, displayBus_, 0, autoFlush, nullptr,
        kBacklight);
#endif
  }

  bool begin() {
    // Match the stable Phase 8B probe: allow the board's display power and
    // RGB panel signals to settle before enabling the continuous pixel clock.
    delay(300);
    if (display_ == nullptr || !display_->begin()) {
      Serial.println("[DISPLAY] ESP32-8048S070 init FAILED");
      return false;
    }
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
    Serial.println("[DISPLAY] RGB direct framebuffer mode: bounce_buffer_size_px=0");
#else
    Serial.printf("[DISPLAY] RGB bounce buffers configured: bounce_buffer_size_px=%u bytes_each=%u total_bytes=%u\n",
                  static_cast<unsigned>(kBounceBufferSizePx),
                  static_cast<unsigned>(kBounceBufferBytesEach),
                  static_cast<unsigned>(kBounceBufferBytesTotal));
#endif
    display_->setRotation(0);
    initialized_ = true;
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
    Serial.printf("[DISPLAY] VSYNC double framebuffer enabled: frame_bytes=%u total_psram_bytes=%u\n",
                  static_cast<unsigned>(Wp7DoubleBufferedDisplay::frameBytes()),
                  static_cast<unsigned>(Wp7DoubleBufferedDisplay::frameBytes() * 2U));
#endif
    Serial.println("[DISPLAY] ESP32-8048S070 init OK");
    return true;
  }

  void showReady() {
    if (!initialized_) return;
    showMessage("P&P READY", "READY FOR WORK");
    Serial.println("[DISPLAY] P&P READY FOR WORK");
  }

#if defined(PP_WP7_TOUCH_TEST) || defined(PP_WP7_READY_TEST) || defined(PP_WP7_PHYSICAL_RACING_TEST)
  // Test-only access for the disposable WP7 touch probe. Production builds do
  // not expose the framebuffer and retain the existing display boundary.
  Arduino_RGB_Display* touchTestCanvas() { return initialized_ ? display_ : nullptr; }
  void setBacklight(bool enabled) {
    pinMode(kBacklight, OUTPUT);
    digitalWrite(kBacklight, enabled ? HIGH : LOW);
  }
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
  bool beginFrame() { return initialized_ && display_->beginFrame(); }
  bool presentFrame() { return initialized_ && display_->presentFrame(); }
#else
  bool beginFrame() { return initialized_; }
  bool presentFrame() { return initialized_; }
#endif
#endif

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
  // The validated IDF-5 WiFi/RGB configuration uses Espressif's 20-line
  // bounce-buffer pattern. Two RGB565 buffers therefore consume
  // 20 * 800 * 2 * 2 = 64,000 bytes of internal DMA-capable SRAM while the
  // full framebuffer remains in PSRAM.
  static constexpr size_t kBounceBufferSizePx = 20 * kDisplayWidth;
  static constexpr size_t kBounceBufferBytesEach = kBounceBufferSizePx * sizeof(uint16_t);
  static constexpr size_t kBounceBufferBytesTotal = 2 * kBounceBufferBytesEach;

#if !defined(PP_WP7_PHYSICAL_RACING_TEST) || !defined(PP_WP7_IDF_DOUBLE_BUFFER) || defined(PP_WP7_SINGLE_FB_RECOVERY)
  Arduino_ESP32RGBPanel* displayBus_ = nullptr;
#endif
#if defined(PP_WP7_PHYSICAL_RACING_TEST) && defined(PP_WP7_IDF_DOUBLE_BUFFER) && !defined(PP_WP7_SINGLE_FB_RECOVERY)
  Wp7DoubleBufferedDisplay* display_ = nullptr;
#else
  Arduino_RGB_Display* display_ = nullptr;
#endif
  bool initialized_ = false;
};

}  // namespace pp

#endif
