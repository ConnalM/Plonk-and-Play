# Isolated migrated RGB display probe

This is a disposable diagnostic environment, not a P&P production display
implementation. It uses Espressif32 8.0.0, Arduino-ESP32 3.3.12,
ESP-IDF 5.5.5 and Arduino_GFX 1.6.5. It includes no LVGL and excludes the
P&P application, SD, Wi-Fi, HTTP/SSE, Browser, race, persistence and touch
paths.

The probe uses the exact current P&P RGB configuration: 800x480; pins DE 41,
VSYNC 40, HSYNC 39, PCLK 42; RGB pins 14/21/47/48/45,
9/46/3/8/16/1 and 15/7/6/5/4; HSYNC 0/210/30/16; VSYNC 0/22/13/10;
active-negative PCLK; 12 MHz; rotation 0; one normal Arduino_GFX RGB
framebuffer. It constructs `Arduino_ESP32RGBPanel` and
`Arduino_RGB_Display`, draws one static black/colour-block/line/text pattern,
then performs no further display writes.

Serial identifies the generated source commit, `stage` environment, Arduino
framework/IDF/GFX versions and display initialization result.

Physical interpretation:

* Pattern displaced/repeated/corrupt: the migrated low-level
  Arduino-ESP32/ESP-IDF/Arduino_GFX RGB scanout path is implicated.
* Pattern completely stable/correct: the low-level path works; the next
  boundary is interaction with P&P startup/resource/PSRAM lifecycle.

Software scope check: the probe is compile/build-only in this commit. It has
not been flashed or physically exercised.

## Software build evidence

The `display-migration-probe` environment built successfully with PlatformIO
Espressif32 8.0.0, Arduino-ESP32 3.3.12 and Arduino_GFX 1.6.5. The generated
provenance header identifies the probe source commit and environment. The
candidate artifact before this evidence-only note update was 378,656 bytes;
the final artifact/hash must be regenerated after the evidence commit so the
embedded source commit remains exact. No upload or physical test was run.
