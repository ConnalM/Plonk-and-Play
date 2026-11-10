# Plonk & Play™ — Product Specification Index

**Status:** Architecture baseline accepted — ready for Stage 1 implementation  
**Baseline accepted:** 30 September 2026

The governing P&P architecture has completed its pre-implementation consistency review. No unresolved architecture blockers remain. Deferred implementation choices and future product detail remain deliberately deferred and do not prevent Stage 1 from beginning.

Changes to the accepted architectural boundaries should now be made deliberately and recorded as architecture changes, rather than emerging accidentally during implementation.

This file identifies where agreed P&P decisions live so later design work does not rely on conversation memory alone.

## Governing architecture

- `DESIGN_CONSTITUTION.md` — governing design principles.
- `SYSTEM_REQUIREMENTS.md` — platform requirements and future capability boundaries.
- `SYSTEM_ARCHITECTURE.md` — system responsibilities and architectural boundaries.
- `SYSTEM_LIFECYCLE.md` — lifecycle/recovery responsibilities.
- `RACE_CONTROL_ENGINE_DESIGN.md` — Race Control / Race Engine ownership and boundaries.
- `FIRST_IMPLEMENTATION_BEHAVIOUR.md` — deliberately narrow first useful implementation, including the three Lap Race finish behaviours.
- `MESSAGE_CONTRACT.md` — common P&P Message Bus rules, first concrete Message Types, Session Definition v1 information and authoritative Noticeboard-change behaviour.
- `DEVELOPMENT_WORKFLOW.md` — proven local Work/PlatformIO/Wokwi development and test-bench workflow; development infrastructure only, not product architecture.

## Product behaviour specifications

- `RACE_MODES_PRODUCT_SPEC.md` — agreed Lap, Practice, Endurance, Timed Stage and Drag behaviour.
- `COMMON_DISPLAY_SPEC.md` — common Lap/Endurance/Timed Stage display, personal MUG displays, Results and Yellow/Red display principles.
- `PRESENTATION_DESIGN_SPEC.md` — shared P&P visual language, reusable presentation components, responsive hierarchy and consistency rules across the integrated display and browser clients.
- `PRODUCT_FACILITIES_SPEC.md` — Power Module, audio/SD, track setup, speed calculations, browser/SC split, solo/multi-user behaviour and option philosophy.
- `CONFIGURATION_AND_IDENTITY_SPEC.md` — v1 MUG identity, Guest promotion, optional independent cars, lane assignment, Swap Lanes and remembered setup.

## Deliberately deferred

The following are not yet fully specified and should not be silently invented during implementation:

- remaining Drag/REU detail beyond the committed base Drag staging, tree, timing and results behaviour;
- detailed RMS Yellow/Red state machine and resume sequencing;
- exact Power Module electrical behaviour;
- exact audio/SD hardware implementation;
- exact dual-beam Drag/REU implementation;
- detailed competition/championship management implied by `CONTINUE`;
- exact presentation tokens, fonts, icon set, dimensions and animation timings within the accepted `PRESENTATION_DESIGN_SPEC.md` visual direction;
- detailed persistence/history schema.

When one of these areas is designed, update the appropriate specification or add a new specification and link it here.

## Deferred implementation checkpoint — WP7 Taster animations (10 November 2026)

**Status: parked until core P&P functionality is complete.** This is deferred presentation implementation, not a change to the agreed racing rules or Gina's animation design.

- **Proven working baseline:** physical ESP32-S3 WP7 Taster uses the single-framebuffer Arduino_GFX path (800 × 480 RGB565, validated 12 MHz pixel clock, bounce-buffer configuration). START, five lights, LAPS, lap counting/finishing, Barlow anti-aliased typography, PAUSE/RESUME/ABANDON have worked on the hardware. The existing animated row redraw flickers/breaks up the lane.
- **Approved presentation to retain:** Gina's Wokwi animation source `firmware/taster_wokwi/pp-taster-display.chip.c`; original MGB/E-Type artwork, independent 500-pixel quadratic car passes on each lap, streaks and waving chequered finish flag. Do not invent a replacement animation, delete its source, or change Race Control/Engine.
- **What was proved:** isolated ESP-IDF 5.5.5 diagnostic with Octal PSRAM at 80 MHz, `CONFIG_LCD_RGB_RESTART_IN_VSYNC=n`, two driver-owned 768,000-byte PSRAM framebuffers, `esp_cache_msync()` and `esp_lcd_panel_draw_bitmap()` **visibly alternated red and blue**. Earlier Arduino precompiled-driver diagnostic had the restart setting enabled and remained red. An earlier source-built diagnostic failed before app startup due to incorrect Quad-vs-Octal PSRAM configuration.
- **Where integration stopped:** isolated opt-in `firmware/wp7_idf_arduino_dbuf` (Arduino as ESP-IDF component) built and flashed, but its READY interface **rolled horizontally on the physical LCD** despite successful framebuffer/callback logs. It was rejected and the single-buffer firmware restored; red/blue diagnostic success does not establish stable combined P&P scanout. No more graphics-driver experiments until the rest of P&P works.
- **Verified recovery:** `C:\Users\conna\Documents\Plonk-and-Play\WP7-recovery-readback-20261109\firmware-recovery.bin` — SHA-256 `C8906CD37C6C1D796C89AFD41C9FDD9A1B9F830D3EBA6DBD44B32FF3B1F31CDD`, app0 flash at `0x10000`. Keep this file and single-buffer build path safe and unchanged.

**To do (in order):**

- [ ] In the next stable single-framebuffer firmware build, **disable the moving-car lap animations and animated chequered flags** (do not delete code). Keep the *static* MGB and E-Type, Barlow font and fixed region-based timing updates. This disable-and-flash step was agreed but **has not yet been performed**.
- [ ] Finish Gina's READY LAPS/TIMED selection and 1–999 lap/minute setting, then test TIMED (Endurance), Browser/SMUG handover, real sensors, records/history and fault/recovery flows.
- [ ] **After functional completion**, resume graphics work from the successful isolated red/blue diagnostic and failed combined display test. First investigate why the opt-in full P&P ESP-IDF/Arduino RGB scanout rolled horizontally; do not assume callbacks mean valid pixels. Preserve validated RGB pins, pixel clock and timing. If stable direct double buffering is not practical, consider buffering only car/flag regions on the proven Arduino_GFX path.
- [ ] Visually acceptance-test Gina's complete car/flag motion on real hardware, with no flickering, corruption or interruption to race timing.

## Working rule

A settled product decision should be written into GitHub before the design conversation moves far beyond it.

> **GitHub, not chat memory, is the durable source of truth.**

- `PP_TASTER_SPEC.md` — built-in two-sensor/two-lane standalone Taster, SMUG configuration, MINIMUG operation and browser boundary.

- `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md` — browser Home/mode setup flows, immediate results, rolling History and minimal v1 records.
