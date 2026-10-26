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

## Follow-up physical evidence

Two photographs now classify the symptom: the backlight and black background
are active, and the white READY text is approximately in the expected position
and orientation, but glyphs are horizontally displaced/repeated and the exact
corruption changes between photographs. This is unstable RGB pixel scanout,
not a wrong READY string, blank panel, rotation choice or simple backlight
failure.

## Compatibility boundary assessment

The old and new P&P adapters still match. The meaningful execution boundary is
the Arduino-ESP32/IDF RGB driver beneath Arduino_GFX:

* Arduino 2.x/IDF 4.x takes the legacy RGB-panel path, with the old
  `LCD_CLK_SRC_PLL160M` selection and the older `esp_lcd` implementation.
* Arduino 3.3.12/IDF 5.5.5 takes the newer `esp_lcd` RGB path. Arduino_GFX
  1.6.5 supplies the Arduino-3 configuration fields (`bits_per_pixel`, one
  framebuffer, PSRAM framebuffer and cache/bounce-buffer flags) and uses the
  Arduino-3 cache/header branch. The requested panel timings remain unchanged;
  the framework owns the DMA/PSRAM scanout implementation.
* In the installed S3 IDF headers `LCD_CLK_SRC_DEFAULT` resolves to the
  160 MHz PLL source, so the source-name change alone does not prove a clock
  frequency mismatch. The evidence does, however, identify the IDF-5 RGB DMA,
  PSRAM and cache-coherency path as the most likely compatibility boundary.
* Arduino_GFX 1.6.5 also reorganises framebuffer pixel writes and cache
  writeback. P&P uses rotation 0, so the rotation rewrite is not by itself an
  explanation, but it remains part of the changed library path.

Display4 confirms the board wiring and porch values and uses the same general
Arduino_GFX/ESP32 RGB construction, but its project is an LVGL application on
Espressif32 6.1.0 and uses a 14 MHz experiment plus a separate LVGL draw
buffer. It is therefore useful hardware/configuration evidence, not proof that
the migration's IDF-5 single-framebuffer path is correct.

The smallest discriminating next test is a display-only migration probe using
the exact P&P pins, porches, 12 MHz clock, rotation and single static pattern,
but no P&P SD/network/race startup allocations. If that probe corrupts in the
same way, the defect is isolated to the Arduino-3/IDF-5/GFX RGB scanout path;
if it is stable, the remaining boundary is P&P PSRAM allocation/lifecycle
interaction. This is a diagnostic choice, not an authorised firmware change.

No production display correction is justified from source inspection alone.

## Post-probe startup/resource lifecycle investigation

The isolated probe was physically PASS on `9f511bc7e39a7c099b3fa924aed4d927c023e625`.
It showed six colour squares, clean grid geometry and three steady text lines;
the pattern had no horizontal repetition or displacement. The probe therefore
does not reproduce the migrated RGB corruption. The remaining boundary is the
full P&P startup/resource lifecycle, not the panel pins, porch values, PCLK,
rotation or the low-level migrated RGB path by itself.

### Construction and allocation order

The production display adapter is `firmware/include/pp/board_display_esp32s3.h`.
Its global `pp::Esp32S3BoardDisplay boardDisplay` construction allocates only
the `Arduino_ESP32RGBPanel` and `Arduino_RGB_Display` objects. It does not call
`begin()` and therefore does not enable RGB scanout or allocate the full
framebuffer at global-construction time.

`main.cpp` calls `boardDisplay.begin()` only after the first valid
`ConfigurationLoaded` message, after these startup boundaries have already run:

1. Serial/build identity and system-time checks;
2. NVS `storage.begin()`, authority-store startup and persisted-authority
   restoration;
3. `PhysicalSdOwner::begin()` and the History/detail storage boundary;
4. the bounded diagnostics recorder and its SD-backed owner;
5. message-bus subscriptions, race/control/presentation objects and the bus
   self-test;
6. `BrowserInterface::begin()`, which starts the AP/STA manager; and
7. early `browser.tick()` work, which starts the HTTP server once the P&P AP is
   available. The server is configured with an 8 KiB task stack, 13 maximum
   open sockets and LRU purge.

Only then does the configuration response make the application `ready`, after
which `boardDisplay.begin()` performs the 300 ms settle delay, calls the GFX
`begin()` path and enables scanout, followed by the single READY draw. The
display is consequently the first large RGB/PSRAM allocation in the full
startup path, whereas the isolated probe performs that allocation before SD,
Wi-Fi, HTTP, Browser or persistence startup.

The one normal RGB framebuffer is 800 x 480 x 2 bytes = 768,000 bytes. Both
the probe and production adapter use the same one-framebuffer Arduino_GFX
construction, so the material difference established by source inspection is
when that allocation occurs, not its size or panel configuration.

### Configuration comparison

The probe and production adapter use the same Arduino_GFX constructor values:
the DE/VSYNC/HSYNC/PCLK and RGB pins, 800 x 480 geometry, HSYNC
`0/210/30/16`, VSYNC `0/22/13/10`, active-negative PCLK, 12 MHz, rotation 0,
null data bus and automatic flush. No remaining source difference in pins,
porches, polarity, geometry, rotation or framebuffer count was found.

In the migrated Arduino_GFX/IDF-5 path the framebuffer is requested in
`Arduino_RGB_Display::begin()` through the RGB panel's one-framebuffer
PSRAM/DMA configuration. The probe exercises the same library path. The
production source contains no later `boardDisplay` draw, delete/free, direct
RGB-peripheral reconfiguration, LVGL setup, touch setup or alternate display
object. `showReady()` writes once and `loop()` does not write to the display.
Thus source inspection finds no post-`begin()` owner that intentionally
modifies or frees the framebuffer.

### Resource evidence and limits

The production startup creates the NVS/authority objects, SD owner and
filesystem handles, diagnostics recorder, bus/race/browser state, AP/STA
manager and the HTTPD task/socket resources before the framebuffer is
allocated. Browser route handlers also use bounded request buffers and SSE
session/work allocations once clients arrive. These are materially absent from
the probe. The repository has no accepted runtime heap snapshot taken at the
display allocation boundary, so the exact PSRAM free-space/fragmentation delta
is not proven by this read-only review. Likewise, no source path proves a
post-`begin()` framebuffer overwrite. The evidence supports a resource/order
interaction hypothesis, but does not yet establish whether the cause is PSRAM
fragmentation, internal-RAM/DMA pressure, cache-coherency interaction after
other startup activity, or another full-application side effect.

### Smallest discriminating next test

Keep every display setting and production subsystem unchanged, but create a
diagnostic-only full-P&P candidate that moves the existing single
`boardDisplay.begin(); boardDisplay.showReady();` boundary to immediately after
Serial/build startup and before NVS, SD, Browser/AP/STA and HTTP startup. Do
not add a second display object, change timings or draw more than the existing
READY image. A stable screen would identify startup allocation/resource order
as the compatibility boundary; corruption in that placement would leave a
full-application framework/resource interaction or framebuffer ownership issue
as the next boundary. This is a proposed discriminator only; no production
code or firmware was changed in this investigation.

## Startup-order probe result and pre-setup comparison

The full-P&P startup-order probe remained corrupt even though the display was
initialized before SD, Browser/AP/STA and HTTP startup. This is diagnostically
valid because the firmware source was unchanged; it is not evidence that
ordinary late SD/network/HTTP allocation is the sole cause.

The captured heap snapshots were:

| boundary | internal free | largest internal block | PSRAM free | largest PSRAM block |
|---|---:|---:|---:|---:|
| before display begin | 270,552 | 212,980 | 8,384,788 | 8,257,524 |
| after READY draw | 267,720 | 212,980 | 7,616,400 | 7,602,164 |
| after SD startup | 266,684 | 212,980 | 7,586,624 | 7,471,092 |
| after Browser/HTTP startup | 197,524 | 155,636 | 7,573,732 | 7,471,092 |

The display allocation consumes approximately 768 KB of PSRAM, as expected
for one 800 x 480 RGB565 framebuffer. At the early draw there was still more
than 8 MB of free PSRAM and a contiguous block over 8 MB; internal free heap
was over 270 KB. The corruption is therefore not explained by an immediately
exhausted heap or inability to obtain a contiguous framebuffer.

### Probe versus full-P&P build configuration

Both environments use the same ESP32-S3 N16R8 board definition, Espressif32
8.0.0, Arduino-ESP32 3.3.12/IDF 5.5.5, Arduino_GFX 1.6.5, DIO flash mode,
USB CDC disabled, and the same partition/PSRAM board configuration. The
project has no separate `sdkconfig` or `sdkconfig.defaults`; no distinct
RGB/DMA/cache sdkconfig setting was found. The display constructor arguments
are identical.

The full environment additionally compiles the P&P application with
`PP_PHASE8B_REAL_SD`, `PP_PHASE8B_P1_TEST`, `PP_MAX_ENTRIES=2`,
`PP_MAX_SSE_CLIENTS=4`, scan instrumentation and the linker scan wrapper. It
also runs the Browser embed prebuild; the probe only defines
`PP_DISPLAY_MIGRATION_PROBE` and compiles the display-probe translation unit.
None of these flags explicitly changes the RGB panel or PSRAM DMA settings.

The link maps show a materially larger pre-setup/static footprint in the full
image. The full probe has `.dram0.data` 0xCACC (51,916 bytes) and
`.dram0.bss` 0x8868 (34,920 bytes), compared with 0x3B35 (15,157 bytes) and
0x1D40 (7,488 bytes) for the isolated probe. The full map also reserves
0x14FFE0 bytes of linker PSRAM dummy space versus 0x5FFE0 in the probe. The
dummy-space figure is a linker/heap-layout indicator rather than proof of a
runtime allocation, but it confirms that the full image has a materially
different memory layout before `setup()`.

Before `setup()` the full image has global P&P objects for NVS storage,
PhysicalSdOwner, SD persistence/detail/history/track-record stores, authority,
race control/engine, noticeboard, Browser, diagnostics and the display adapter.
Their constructors account for the object/static footprint visible in the map;
the display adapter itself allocates only its two small GFX objects at this
stage. The framebuffer is still allocated by `display.begin()`. Arduino core
startup and PSRAM initialization are otherwise the same board/framework path
in both environments. No source or map evidence shows a pre-`setup()` RGB
peripheral reconfiguration in the full image.

### Remaining discriminator

The smallest next discriminator is a disposable full-P&P build that retains
the same global P&P object set and migrated build flags but exits setup after
the early display draw, before SD/network/HTTP startup. A clean image there
would implicate global/static construction or the full link/memory layout;
corruption there would rule out later subsystem startup and narrow the cause
to the full application image/framework context around RGB DMA/PSRAM. This is
only a proposed diagnostic build. No production source, timing, pin mapping,
LVGL or runtime behaviour was changed here.

