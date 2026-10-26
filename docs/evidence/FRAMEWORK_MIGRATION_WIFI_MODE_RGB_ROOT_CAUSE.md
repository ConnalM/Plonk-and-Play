# Framework migration: WiFi-mode/RGB scanout root-cause analysis

Date: 2026-10-26  
Branch: `codex/history-read-diagnostic`  
Base: `f8b92dc4779e2a163b380af2530b0f0537a4d7c2`

Status: read-only source/framework analysis. No firmware, display settings,
build configuration, or hardware state was changed by this investigation.

## Proven boundary

The accepted physical evidence is now:

* the isolated migrated Arduino_GFX RGB probe is clean and stable;
* the full P&P image with storage/diagnostics/Message Bus startup, but before
  Browser/network startup, is clean;
* the full image becomes intermittently corrupted after the WiFi-mode-only
  boundary, using `WiFi.persistent(true)` followed by
  `WiFi.mode(WIFI_AP_STA)`;
* AP creation, remembered-STA `WiFi.begin()`, mDNS, Browser, HTTP and SSE are
  not required;
* no panic, reset, or heap exhaustion was observed.

The current capture is
`firmware/logs/device-monitor-261026-213113.log`. Its relevant resource
figures are:

```
before display:       internal 275128 / 212980, PSRAM 8384788 / 8257524
after READY draw:     internal 272296 / 212980, PSRAM 7616400 / 7602164
after WiFi.mode:      internal 226376 / 180212, PSRAM 7575072 / 7471092
```

The loss is material but leaves large contiguous regions; this is not evidence
of simple allocation exhaustion.

## What `WiFi.mode(WIFI_AP_STA)` actually starts

The installed Arduino-ESP32 3.3.12 source is under
`C:/Users/conna/.platformio/packages/framework-arduinoespressif32`.

In `libraries/WiFi/src/WiFiGeneric.cpp`:

* `WiFiGenericClass::mode()` first calls `wifiLowLevelInit()` when changing
  from `WIFI_MODE_NULL` (lines 602-612).
* `wifiLowLevelInit()` calls `Network.begin()` (258-270), which calls
  `esp_netif_init()` and `initNetworkEvents()` in
  `libraries/Network/src/NetworkManager.cpp` (19-37).
* `NetworkEvents::initNetworkEvents()` allocates an event group, a 32-entry
  event queue, a mutex, creates the default ESP event loop, and starts the
  `arduino_events` task with a 4096-byte stack (the installed
  `libraries/Network/src/NetworkEvents.cpp`, lines 14-87).
* `wifiLowLevelInit()` creates `WIFI_INIT_CONFIG_DEFAULT()`, then the Arduino
  path uses dynamic TX/RX settings (`dynamic_tx_buf_num=32`,
  `cache_tx_buf_num=4`, `static_rx_buf_num=4`) when static-buffer mode is not
  selected (278-289), calls `esp_wifi_init()` (289), installs WiFi event
  handlers, and creates the default AP and STA netifs (301-309).
* `mode()` enables the STA/AP interface objects (617-621), sets the STA
  hostname when needed (643), calls `esp_wifi_set_mode()` (649), and finally
  calls `espWiFiStart()` (697-704), which invokes `esp_wifi_start()` (347-355).

Thus “mode-only” is not a passive enum assignment. It starts the IDF WiFi
driver/netif/event infrastructure, allocates WiFi buffers and event/task state,
installs interrupt/driver machinery, and starts the WiFi task. The physical
probe did not need an association or an HTTP client for the display failure.

The current S3 framework SDK configuration also includes
`CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=1`, WiFi static RX 8, dynamic RX 32,
cache TX 16, WiFi task pinned to core 0, and LCD RGB restart-in-VSYNC enabled.
The corresponding generated header is
`framework-arduinoespressif32-esp32s3-libs/dio_qspi/include/sdkconfig.h`.

## RGB path actually used by P&P

P&P's unchanged adapter is
`firmware/include/pp/board_display_esp32s3.h:18-25`. It uses the proven pins,
800x480 geometry, the existing porches/polarity, rotation 0 and a 12 MHz PCLK.
The Arduino_GFX 1.6.5 path is
`firmware/.pio/libdeps/stage8bp1s3sd-display-wifi-mode-only-stop/GFX Library for Arduino/src/databus/Arduino_ESP32RGBPanel.cpp`:

* `Arduino_RGB_Display::begin()` obtains one framebuffer via
  `getFrameBuffer()` (lines 28-71 of `display/Arduino_RGB_Display.cpp`).
* `esp_lcd_rgb_panel_config_t` requests `bits_per_pixel=16`, `num_fbs=1`,
  `fb_in_psram=true`, `double_fb=false`, `no_fb=false`,
  `refresh_on_demand=false`, `bb_invalidate_cache=false`, and
  `bounce_buffer_size_px=0` (RGB panel source lines 42-97).
* The framebuffer is therefore a full 800x480x2 = 768,000-byte PSRAM buffer
  read directly by RGB DMA. No internal SRAM bounce buffers are requested.
* `Arduino_RGB_Display` uses the cache writeback wrapper after drawing and on
  `flush()` (lines 105-107, 239-241, 292-294 and 553-557). It does not change
  the panel configuration after startup.

The 1.5.6 and 1.6.5 Arduino_GFX `Arduino_ESP32RGBPanel.cpp` files are
effectively the same source for this path; the important change is the
Arduino-3/IDF-5 branch selected by `ESP_ARDUINO_VERSION_MAJOR`, and the IDF
implementation underneath it. The old Arduino-2/IDF-4 branch selected the
legacy `esp_lcd` RGB implementation and `LCD_CLK_SRC_PLL160M`; Arduino-3 uses
the IDF-5 `LCD_CLK_SRC_DEFAULT` branch and the new explicit framebuffer/bounce
fields. The requested PCLK remains 12 MHz, and the probe proves the constructor
arguments alone are not the problem.

## Known IDF 5.x mechanism

ESP-IDF 5.5's RGB panel API explicitly provides
`bounce_buffer_size_px`. A non-zero value allocates two internal DMA bounce
buffers; the LCD DMA reads SRAM while an ISR copies from the PSRAM framebuffer.
The ESP-IDF 5.5 RGB documentation states that this mode is more robust against
short bandwidth spikes, at the cost of CPU and internal RAM, and specifically
documents PSRAM framebuffer bandwidth limitations. The official RGB example
also describes bounce buffers as the mechanism to tolerate WiFi/other traffic
when direct PSRAM scanout is insufficient.

Primary references:

* [ESP-IDF 5.5 RGB LCD guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html)
* [ESP-IDF RGB example](https://github.com/espressif/esp-idf/blob/master/examples/peripherals/lcd/rgb_panel/README.md)
* [Arduino_GFX 1.6.5 RGB implementation](https://raw.githubusercontent.com/moononournation/Arduino_GFX/v1.6.5/src/databus/Arduino_ESP32RGBPanel.cpp)
* [ESP-IDF RGB driver source](https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/rgb/esp_lcd_panel_rgb.c)

ESP-IDF's FAQ is even more direct: for an application using WiFi/BLE with an
RGB panel, it recommends the `XIP on PSRAM + RGB Bounce buffer` combination.
P&P's current generated SDK configuration does not define the XIP-from-PSRAM
options, and Arduino_GFX passes a zero bounce-buffer size.

## Ranked causes

1. **Most likely, and directly supported by the boundary and IDF guidance:**
   IDF-5 direct RGB DMA from PSRAM is exposed to a short PSRAM/SPI0/cache
   bandwidth or arbitration disturbance when WiFi driver startup begins. A
   continuous RGB stream can lose synchronization when DMA cannot keep up;
   intermittent horizontal repetition/displacement is consistent with that
   failure mode. WiFi-mode-only is the first point at which the competing
   driver/resource activity exists.
2. **Contributing mechanism:** WiFi mode startup creates internal DMA/driver
   buffers, event/netif allocations and a WiFi task. `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`
   also permits selected LWIP buffers in PSRAM. The captured heap remains ample,
   so this is a contention/latency hypothesis, not a simple out-of-memory
   explanation.
3. **Possible but not established:** IDF 5 power-management or WiFi clock/RF
   activity changes arbitration timing. No source or capture proves that it
   changes the RGB PCLK or LCD peripheral configuration.
4. **Not supported by current evidence:** P&P framebuffer lifetime, later
   Browser/HTTP activity, softAP/STA association, mDNS, pin/porch/polarity or
   PCLK error. The isolated probe and early/full/pre-Browser physical passes
   clear those alternatives at their respective boundaries.

## Smallest technically justified next step

The first controlled correction/discriminator should be the existing IDF-5
supported RGB bounce-buffer mode, not another timing or pin experiment: retain
the single PSRAM framebuffer and configure a modest non-zero
`bounce_buffer_size_px` so two internal DMA buffers carry the scanout. This is
the smallest change that directly tests the documented PSRAM/WiFi contention
mechanism while preserving pins, porches, polarity, geometry, rotation, PCLK,
authority and all P&P product behaviour.

It must remain an explicitly reviewed diagnostic first: build a disposable
full-P&P candidate, record internal-RAM impact, and physically observe the
screen after WiFi-mode startup. A clean result would validate the mechanism;
failure would leave IDF-5 cache/driver interaction or another WiFi-induced
resource effect for further analysis. No source change, build, flash, timing
experiment or production decision is made by this report.

## Production decision (2bf3cf1)

The full-P&P bounce-buffer candidate was physically validated under the normal
startup path. With the migrated ESP32-S3 stack, SD, AP, remembered STA, mDNS and
HTTP all running, the READY screen remained steady and clean for the observation
period. The runtime marker reported `bounce_buffer_size_px=16000`, two 32,000-byte
RGB565 buffers, and 64,000 bytes total internal DMA SRAM. The single full
800x480 framebuffer remains in PSRAM; no pins, porch/polarity values, rotation,
12 MHz PCLK, Arduino_GFX version, or product subsystem semantics changed.

That result is the accepted correction for the IDF-5 WiFi/RGB contention boundary.
The setting is now part of the normal `stage8bp1s3sd` display adapter rather than
a probe-only build flag. The disposable
`stage8bp1s3sd-display-rgb-bounce-probe` environment and probe-only marker were
removed. The normal runtime keeps only a compact configuration line identifying
the validated bounce-buffer allocation.

