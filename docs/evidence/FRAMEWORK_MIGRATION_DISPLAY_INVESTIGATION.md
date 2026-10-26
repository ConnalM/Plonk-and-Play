# Framework-migration display investigation

Status: read-only investigation; no firmware or display source changed.

## Scope and candidate

This investigation compares the current migration baseline (PlatformIO
`espressif32@8.0.0`, Arduino-ESP32 `3.3.12`, ESP-IDF `5.5.5`, Arduino_GFX
`1.6.5`) with the accepted pre-migration implementation (PlatformIO
`espressif32@6.9.0`, Arduino-ESP32 2.x/IDF 4.x, Arduino_GFX `1.5.6`). The
current branch is `codex/history-read-diagnostic`, at the History-validation
candidate associated with commit `234bdd84da14be8c07737e04e8a3ebba6204a556`.

## Intended boot output and owner

The only P&P boot acknowledgement owned by the product firmware is the
`Esp32S3BoardDisplay` adapter in
`firmware/include/pp/board_display_esp32s3.h`:

* the global `pp::Esp32S3BoardDisplay boardDisplay` is constructed in
  `firmware/src/main.cpp`;
* construction allocates `Arduino_ESP32RGBPanel` and `Arduino_RGB_Display`,
  but does not start RGB scanout;
* after the normal P&P startup checks reach `ready`, `boardDisplay.begin()`
  starts the panel and `showReady()` performs one static draw;
* the intended text is two lines: `P&P READY` and `READY FOR WORK`.

`begin()` logs `[DISPLAY] ESP32-8048S070 init OK` and `showReady()` logs
`[DISPLAY] P&P READY FOR WORK`. The physical migration capture
`firmware/logs/device-monitor-261026-162836.log` contains both lines, proving
that the firmware reached the intended initialization and draw path. Serial
evidence cannot prove that the pixels were visibly correct.

## P&P source comparison

`git show c7a7bb4^:firmware/include/pp/board_display_esp32s3.h` is identical
to the current header for the display-relevant implementation. The following
are unchanged between the accepted pre-migration source and the migration
candidate:

* DE/VSYNC/HSYNC/PCLK pins `41/40/39/42`;
* RGB pins `14,21,47,48,45 / 9,46,3,8,16,1 / 15,7,6,5,4`;
* 800x480 geometry;
* HSYNC `0,210,30,16` and VSYNC `0,22,13,10`;
* active-negative PCLK and 12 MHz preferred clock;
* backlight pin 2;
* global object allocation, 300 ms settle delay, `begin()`, rotation 0,
  framebuffer ownership, and the single READY draw.

The migration did not modify `main.cpp` display call order or this adapter.

## Framework/library differences that can affect RGB behaviour

The migration-only `firmware/platformio.ini` change is:

* Espressif32 `6.9.0` -> `8.0.0`;
* Arduino-ESP32/ESP-IDF -> Arduino `3.3.12` / IDF `5.5.5`;
* Arduino_GFX `1.5.6` -> `1.6.5`.

The GFX change was required because 1.5.6 does not build with the newer
Arduino SPI API (`spiFrequencyToClockDiv`); it was not a deliberate panel
retune. Nevertheless, the RGB implementation is compiled through different
framework/library branches:

* `Arduino_ESP32RGBPanel.cpp` changes the target guard to include ESP32-P4
  and, under Arduino major 3, uses the IDF-5 RGB configuration fields
  (`bits_per_pixel`, `num_fbs`, bounce-buffer and framebuffer flags). The
  P&P values remain one 16-bit framebuffer in PSRAM, no double framebuffer,
  no refresh-on-demand and the same timings/pins.
* The IDF clock-source branch changes from the old
  `LCD_CLK_SRC_PLL160M` path to the Arduino-3/IDF-5
  `LCD_CLK_SRC_DEFAULT` path. The requested PCLK remains 12 MHz.
* `Arduino_RGB_Display.cpp` has the corresponding Arduino-3 target guard and
  a new `GFX_SKIP_DATABUS_BEGIN` option. Its pixel-write implementation was
  reorganised (notably rotation handling and cache-writeback); P&P uses
  rotation 0, a null data bus and `auto_flush=true`, so this is still a
  library/runtime difference even though P&P source is unchanged.

The migration also retains the same S3 board, 16 MB flash, PSRAM selection,
partition table and display object lifetime. No P&P display timing, pin,
backlight or startup change is evidenced by the source comparison.

## Proven local display references

The local `Display3` project contains an independently proven
ESP32-8048S070 RGB implementation with the same wiring and porch values, but
uses a 16 MHz preferred clock and therefore is not evidence that P&P's 12 MHz
candidate is visually correct. `Display4` is an LVGL display project and uses
its own display lifecycle/PSRAM buffers (and a 14 MHz experiment); it is not an
authoritative apples-to-apples P&P baseline. Neither project identifies the
current migration symptom.

## Evidence gap and minimum physical observation

No repository capture records whether the migration display is blank, partial,
miscoloured, geometrically shifted, unstable, backlight-only, or otherwise
wrong. The Serial lines prove initialization and the intended READY draw only.

The single required observation is therefore: **take one photo of the entire
physical display immediately after the migration candidate reaches READY,
showing the panel and backlight, without changing settings or flashing/rebooting
for this observation.** That photo is sufficient to classify the visible
failure before any compatibility change is proposed.

