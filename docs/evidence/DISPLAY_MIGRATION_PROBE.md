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

## Physical validation

The reviewed probe from source commit
`9f511bc7e39a7c099b3fa924aed4d927c023e625` was flashed once on the target
ESP32-S3. Serial reported the expected probe commit, `stage8bp1s3sd`
framework stack, and `init OK`; no P&P subsystem was enabled. The physical
pattern was PASS: six colour squares, the grid geometry, and all three fixed
text lines were clean and steady. No horizontal repetition, displacement or
corruption was observed. This proves the migrated low-level
ESP32-S3 + Arduino-ESP32 3.3.12 + ESP-IDF 5.5.5 + Arduino_GFX 1.6.5 RGB
path with the current P&P panel configuration is stable in isolation.

The probe artifact used for that validation was 378,656 bytes with SHA-256
`3ECAB4F2BD71A395F35AE955752C5B5EE3A8F758563C7E43D53C0D45C3C9B271`.

This does not prove the full P&P startup/resource lifecycle is safe for the
same framebuffer. That is the next investigation boundary.

## Software build evidence

The `display-migration-probe` environment built successfully with PlatformIO
Espressif32 8.0.0, Arduino-ESP32 3.3.12 and Arduino_GFX 1.6.5. The generated
provenance header identifies the probe source commit and environment. The
candidate artifact before this evidence-only note update was 378,656 bytes;
the final artifact/hash must be regenerated after the evidence commit so the
embedded source commit remains exact. No upload or physical test was run.
