# Browser recovery physical-validation artifact

Implementation: `88515b88f4f972da52d3b6a89888483189ff1b3f`

## Preserved artifact

Path:

`_approved-artifacts/88515b88f4f972da52d3b6a89888483189ff1b3f/stage8bp1s3sd/firmware.bin`

- environment: `stage8bp1s3sd`
- size: `1,465,840` bytes
- SHA-256: `597BD42EE40A90B77BE18449C9811A0BDED64039ACDB33C08E8C57B7794B9E23`
- ELF SHA-256: `7D8148CE9A58119DA4BCAD16293CF1BA58E5DAA03F25C97029E1CEA837F562DB`
- flashed: no

The first artifact built from the evidence-commit checkout was rejected before
acceptance because `/health` reported the wrong embedded source commit. The
artifact was then rebuilt from detached implementation commit `88515b8` and
the preserved copy was hashed after copying and matched this manifest.

## Build identity/resources

- PlatformIO Espressif32: `8.0.0`
- Arduino-ESP32: `3.3.12`
- ESP-IDF: `5.5.5`
- esptool: `5.4.0`
- GFX Library for Arduino: `1.6.5+sha.4c1d0a6`
- static RAM: `88,884` bytes
- linked program flash: `1,437,547` bytes
- SSE workers: `4 × 8,192` byte diagnostic stacks

## Artifact-dependent checks

`browser_binary_provenance.py` passed against the preserved binary and
reported the expected embedded Browser payload and SHA-256.

The Stage 14C integration review was run against the preserved production
binary. The production-agnostic checks passed. Two checks are intentionally
stage-specific and do not apply to a `stage8bp1s3sd` production image:
`browser_start_diagnostic_demo_image` expects a Stage 14C demo-only marker,
and `browser_route_trace` retains an older source-marker expectation. They do
not indicate a failure of the sequential recovery implementation or of this
production artifact.

All deterministic Browser, HTTP/SSE, Stage 10, Stage 15, WP3/WP4A, and
Persistent Storage suites were run before artifact preservation and passed;
the Stage 14C runtime suite includes the reboot-shaped sequential recovery
regression.
