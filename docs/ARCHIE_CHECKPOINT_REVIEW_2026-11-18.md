# Proposed checkpoint and regression evidence

Retain firmware `3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08` as the accepted physical milestone. WP8 remains in progress and WP7 real hardware remains open.

| Group | Actual host result | Remaining evidence |
|---|---|---|
| Shared configuration | All 13 storage host targets passed after the migration correction, including all seven explicit legacy migration cases. The previous runs exposed the 1,200-lap defect. | Physical ESP32 NVS migration and live Browser/Taster agreement remain unverified. |
| Race Engine | Lap-target guard and simulated racing journey passed. | Prove frozen standalone eligibility and later SMUG attachment, including unchanged simulated classification. Real detectors and physical Endurance remain open. |
| HTTP socket limit | Source/configuration contract passed for both SDK configurations. | Live saturation, reconnect and Browser/Taster coexistence remain open. |
| WP6 adapter | Update API, platform, state machine and validation diagnostics host tests all passed. | Physical signed validation and automatic rollback remain open. |

## Proposed file list for review

### Legacy migration evidence

`acceptance/persistent_storage/configuration_migration_test.cpp` uses independent
legacy record fixtures and a host A/B backend. Before the correction, a
historically valid 1,200-lap CPP2 record was rejected and unrelated settings
fell back to defaults. The retained regression now requires the exact target
and unrelated settings to survive migration and reload. CPP1 defaults and
upgrade, CPP2 update and restart, failed writes, corrupt-slot fallback, the
1/999/1000/UINT32_MAX boundaries, and zero/corruption rejection all passed.

The shared configuration keeps positive lap targets. The Taster retains and
displays the authoritative target without rewriting it. A standalone Taster
now rejects START above 999 and requires an explicit local distance adjustment;
that first adjustment enters the 1–999 range at 999. Changing mode or Endurance
duration leaves the stored lap target intact. Browser-controlled lap editing
and START retain the shared range, with the existing Endurance cap retained.
The focused Taster and Browser setup regressions passed. Physical Browser/Taster
coexistence acceptance remains open.

The Stage 7.3 racer-selection contract and all nine focused Taster checks
passed. The Taster checks cover setup, rendering invalidation, dirty regions,
GT911, lap guard, presentation, structure and both disabled-feature contracts.
The warning-disabled contract used its dedicated compile flag. ESP32-S3
syntax checking passed with existing warnings. On-device migration and the
compact high-target display still require physical acceptance.

Browser setup synchronisation, Browser authority, and the Stage 7.3 Racer
Browser contract passed. The setup harness covered unchanged blur at 1,200
laps and explicit submissions of 999, 1,000, 1,201 and 4,294,967,295. Its fake
DOM now supplies the existing racer section and response so the actual
embedded Browser setup flow can complete. The broader Stage 14C integration
review still reports four image-artifact checks without the expected
historical demo and acceptance images: `demo_image_detector_labels`,
`acceptance_image_detector_labels`, `demo_image_stage14c_controls` and
`browser_start_diagnostic_demo_image`.

The UINT32_MAX checks exercise storage and Browser setup. Race Engine
finish calculations still add penalty laps to the target in 32-bit
arithmetic, which can overflow at the extreme boundary. That existing
Race Engine issue remains outside this migration correction.

### Proposed migration correction file list

- Shared storage validator: `firmware/include/pp/core.h`.
- Taster boundary and numeric rendering: `firmware/include/pp/taster_surface.h`,
  `firmware/include/pp/wp7_physical_racing_ui.h`.
- Browser source and embedded payload: `firmware/browser/normal_browser.html`,
  `firmware/include/pp/normal_browser_page.inc`.
- Regression sources: `acceptance/persistent_storage/configuration_migration_test.cpp`,
  `acceptance/stage8a_taster_surface_test.cpp`, and
  `acceptance/stage14c/browser_setup_sync_runtime.js`.
- Governing documentation: `docs/CONFIGURATION_AND_IDENTITY_SPEC.md`,
  `docs/PP_TASTER_SPEC.md`, `docs/MASTER_PROJECT_PLAN.md`, and this file.

Select only correction hunks in files with pre-existing work. Apply the
correction on top of the reviewed WP8 source baseline, since the physical
renderer and shared record migration already contain work absent from HEAD.
The supplied `Archie_Configuration_Migration_Source_2026-11-18.patch` compares
the correction against the earlier Archie review snapshot for overlapping
files and HEAD `6178158` for the Browser HTML. It includes the complete
untracked migration test as a new file for checkpoint review.
The full WP8 candidate set below requires separate checkpoint approval. The accepted
application and recovery image remain the 3649CEE0 checkpoint. Standalone
record eligibility, audio implementation, and physical migration acceptance
remain separate obligations.

### Candidate files

Paths are relative to the existing worktree. Approval and regression closure are required for a firmware-source checkpoint.

- Documentation slice: `docs/MASTER_PROJECT_PLAN.md`, `docs/ARCHIE_COMPLIANCE_RECONCILIATION_2026-11-18.md`, `docs/RACE_CONTROL_ENGINE_DESIGN.md`, `docs/PRODUCT_FACILITIES_SPEC.md`, `docs/CONFIGURATION_AND_IDENTITY_SPEC.md`, `docs/COMMON_DISPLAY_SPEC.md`, `docs/STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md`, `docs/WP7_XIP_APPLICATION_BUILD_REFERENCE.md`, and this file.
- Presentation slice: `firmware/include/pp/ui_dirty_regions.h`, `wp7_physical_racing_ui.h`, `wp7_presentation_contract.h`, `wp7_ready_artwork.h`, `wp7_barlow_bold_28.h`, `wp7_barlow_bold_40.h`, `wp7_barlow_semibold_24.h`, and `wp7_barlow_semibold_30.h` in that same directory.
- Embedded artwork: `firmware/assets/wp7_welcome_static_rgb565.bin` (source raster), and `firmware/assets/wp7_approved/` files `pp_header_logo.rgb565`, `header_trapezoid_surround.rgb565`, `lamp_red.rgb565`, `lamp_green.rgb565`, `lamp_yellow.rgb565`, `lamp_off.rgb565`.
- Build inputs: `firmware/wp7_idf_arduino_dbuf/` project, main and component CMake files, SDK configs, partition CSV, dependency manifest and lock, and README. Review the required local GT911 changes under `firmware/lib/TAMC_GT911/src/` and pin the external ESP-IDF, Arduino and GFX inputs before claiming reproducibility.
- Focused tests: `acceptance/stage8a_dirty_region_test.cpp`, `stage8a_taster_surface_test.cpp`, `wp7_render_invalidation_test.cpp`, `wp7_gt911_safety_test.cpp`, `wp7_lap_target_guard_test.cpp`, `wp8_presentation_contract_test.cpp` and `wp8_http_socket_limit_test.py` under `acceptance/`.
- Separate review slices: `firmware/include/pp/core.h`, `race_control.h`, `race_engine.h`, `noticeboard.h`, `taster_runtime.h`, `taster_surface.h`, `browser_interface.h`, `esp32_firmware_update_adapter.h`, `board_display_esp32s3.h`, plus `firmware/src/main.cpp` and `firmware/platformio.ini`. Confirm cross-slice dependencies before staging any subset.

- Shared-configuration regression source: `acceptance/persistent_storage/configuration_migration_test.cpp`.

- Standalone START-gate correction: `firmware/include/pp/taster_surface.h`,
  `acceptance/stage8a_taster_surface_test.cpp` and
  `acceptance/stage14c/browser_setup_sync_runtime.js`. The host regressions
  prove the standalone-only gate, explicit re-entry at 999 and an unaffected
  Browser 1,200-lap START. Physical coexistence evidence remains open.

Exclude build output, caches, credentials, recovery images and temporary diagnostics. Standalone record-policy implementation is a separate future change.

The master plan also records two additional open controls: Race Engine
penalty-lap addition can overflow at an extreme valid shared target, and
Gina-owned SMUGLAND Browser visual design/acceptance is an explicit workstream.
