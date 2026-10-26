# S3 framework migration baseline — software evidence

Migration commit: `c7a7bb43d8dc20cf2f73666321a50ab2953cfcce`; accepted comparison source: `ec899f93c080a433371b9aeb85c154a811e64ca3`. This is an Arduino application migration, not the later HTTP/SSE isolation or START architecture work. No controller was flashed and no NVS was erased or migrated.

## Resolved toolchain and compatibility

- PlatformIO Espressif32 8.0.0; Arduino-ESP32 3.3.12 (`framework-arduinoespressif32` 4.30312.261024); S3 libraries 3.3.12 report ESP-IDF v5.5.5 b774170ff46; Xtensa/RISC-V compiler 14.2.0+20260121; esptool 5.4.0.
- GFX Library for Arduino changes from v1.5.6 (58ac1d4) to v1.6.5 (4c1d0a6), the first upstream release fixing the Arduino 3.3.6+ `spiFrequencyToClockDiv` API signature. The original v1.5.6 actually failed the S3 build in its ESP32 SPI and SPI-DMA sources. This is a library compatibility change, not an intentional display redesign. The newer library also changes RGB/display implementation: display behaviour and timing still require physical validation.
- The existing UART0 Serial selection is retained as `ARDUINO_USB_CDC_ON_BOOT=0`, using `build_unflags` to remove the board's competing `=1` definition. The generated compilation database shows exactly one definition, `=0`; the first 8.0.0 build exposed repeated macro-redefinition warnings and an order-sensitive undefinition.
- No race, persistence, network, Browser, HTTP/SSE or START source was changed.

## Build, layout and resource evidence

Clean `pio run -e stage8bp1s3sd` PASS. The generated build-identity contract passes for the migration commit and environment; the Browser binary-provenance test finds the current generated 60,170-byte Browser payload in the application. The S3 240 MHz target and 16 MB board setting remain. The board retains `qio_opi` PSRAM selection, 80 MHz flash setting and the P1 `dio` flash-mode override; the selected qio_opi S3 configuration declares octal 80 MHz PSRAM. No hardware execution has confirmed PSRAM or peripheral operation on this candidate.

The generated 3,072-byte partition binary is byte-identical to the accepted build: SHA-256 `BD0F7954ACA2EF7D925EE21AAA1F3DC8822D1D6CE5CBBD26A135E5886BFFF6CE`. The PlatformIO upload plan is bootloader 0x0000, partition table 0x8000, boot_app0 0xe000, application 0x10000. The NVS partition layout is unchanged; framework/NVS credential compatibility on a real upgrade remains unproven.

| Measure | Accepted ec899f9 image | Migration c7a7bb4 image |
| --- | ---: | ---: |
| Application binary | 1,044,800 bytes | 1,452,880 bytes |
| Static RAM (PlatformIO section convention) | 84,776 bytes | 86,828 bytes |
| Flash usage (PlatformIO section convention) | 1,044,441 bytes | 1,425,163 bytes |

The migration adds 408,080 bytes to the application file, 380,722 bytes to reported flash sections and 2,052 bytes of static RAM. The application partition is 6,553,600 bytes. ELF-derived baseline uses the accepted build's section-matching convention, not a run of the old build wrapper. Migration application SHA-256: `787973D2095C95560AE43083F708DF46ABFE68F417E3A19DD4DBD545FF43FDD2`; ELF SHA-256: `B6525970DA615BACAD798EA9466C495F4322B9486FF79E83C3208150AA9BC742`. Runtime heap and stack high-water marks are not measurable without running the candidate. S3 SDK configuration for both frameworks declares Arduino loop stack 8,192 bytes, system event task stack 2,048 bytes and main task stack 4,096 bytes; this does not prove runtime headroom or other task equivalence.

## Software regression evidence and limits

PASS: Persistent Storage Foundation suite, including PhysicalSdOwner and A/B Track Record contracts; Stage 15 network, control lease, multi-client structural contract, WP3 Browser/recorder/sequence/export/observation, WP4A diagnostics, phases 1–7, resource/performance host model and diagnostics retrieval tooling contract. Stage 14C Browser runtime tests passed for proposal, recovery, presentation, resume, polling, SSE liveness/notification, current START presentation, setup and Results including physical-payload reproduction. The current START test only establishes the current implementation and does not resolve the separately documented START architecture discrepancy.

The Stage 14C full runner did NOT execute: `firmware/build.ps1` explicitly requires the globally active PlatformIO Espressif32 6.9.0, while this migration installed 8.0.0 alongside it. The Stage 15 multi-client script's real HTTP/SSE choreography was NOT executed (no `PP_MULTI_CLIENT_BASE_URL` supplied). Host/contract tests do not exercise the new ESP32 framework on hardware. These are not physical or full Stage 14C passes.

Warnings include the now-deprecated `esp_ota_get_app_elf_sha256`, volatile increment deprecation, existing misleading-indentation and unused variables/functions, and upstream GFX overloaded-virtual warnings. No warning was suppressed as a substitute for verification. The old platform/framework packages remain installed side-by-side, but legacy `build.ps1` assumes 6.9.0 is the globally active default: select the old platform explicitly or isolate the old tool environment when running that legacy runner.

## Review and follow-up boundaries

Code review should assess the newer GFX RGB implementation, actual USB/upload flags, IDF/NVS and Wi-Fi compatibility, heap/stack headroom and SD recovery on the actual S3. No physical acceptance is claimed. The prior shared HTTP server blocking and two-client synchronization concerns remain open; this commit does not implement async request handling, SSE changes or START/GO synchronization. Controlled physical validation requires Archie review and a separate decision before flashing.
