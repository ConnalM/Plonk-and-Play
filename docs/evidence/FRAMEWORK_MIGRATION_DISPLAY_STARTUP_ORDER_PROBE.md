# Full P&P display startup-order probe

This is a disposable diagnostic candidate. It does not change the production
display configuration or product startup architecture. The build inherits the
full `stage8bp1s3sd` P&P environment and adds only
`PP_DISPLAY_STARTUP_ORDER_PROBE`.

The existing `Esp32S3BoardDisplay` object, Arduino_GFX construction, RGB pins,
porches, active-negative PCLK, 12 MHz clock, 800 x 480 geometry, rotation 0,
single PSRAM framebuffer and READY draw are unchanged. The probe moves the
existing `boardDisplay.begin()`/`showReady()` call to immediately after the
system-time check and before NVS, SD, diagnostics, Browser/AP/STA and HTTP
startup. The normal startup then continues; the later production display call
is suppressed so the panel is initialized exactly once.

The diagnostic emits four direct Serial heap snapshots:

* `before_display_begin`;
* `after_display_ready` (or `after_display_failed`);
* `after_sd_startup`;
* `after_browser_http_startup`, once the normal HTTP server reports ready.

Each snapshot reports free and largest internal heap blocks and free and
largest PSRAM blocks. No subsystem is disabled, no storage is written by the
probe itself, and no HTTP/SSE or race behaviour is changed.

Interpretation:

* corruption immediately after the early draw implicates full-P&P static or
  pre-display allocation/context;
* a clean early image that becomes corrupt later implicates a later startup
  transition;
* a clean image after complete startup implicates late framebuffer allocation
  and its resource state, but does not by itself authorize a production
  ordering change.

## Build evidence

The candidate was clean-built from source commit
`473fbb71b36c694e928f735372dad2ac687b145a` using `stage8bp1s3sd-display-order-probe`:

* PlatformIO Espressif32 8.0.0;
* Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5;
* Arduino_GFX 1.6.5;
* ESP32-S3 N16R8, 16 MB flash, existing P&P partition configuration;
* firmware size 1,454,912 bytes;
* firmware SHA-256
  `D8D54F93540BE2929F7CEED181B7C6AE903CC8FAD7698D2A3E7C3D03B8819DB3`.

The generated build-identity header embeds the same source commit and the
diagnostic environment. Compilation and link succeeded; only the existing
warning set was reported. No physical flash or validation has been performed.
This artifact is a diagnostic candidate and must not be treated as a
production image.
