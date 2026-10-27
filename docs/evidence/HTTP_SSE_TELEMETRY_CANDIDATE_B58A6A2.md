# SSE telemetry candidate — b58a6a2

## Implementation

Implementation commit: `b58a6a27f35da600c8381e03fb5a10fbff46132c`.

The SSE worker now samples `uxTaskGetStackHighWaterMark()` and writes only bounded
raw records to a fixed 32-entry telemetry ring. Worker code performs no
`Serial.printf`, `snprintf` or `vsnprintf`. The loop-side drain formats the existing
`[SSE_STACK]` records outside worker context. Coverage remains for idle, initial,
notification, keepalive, stalled-send, send-failure, disconnect, replacement and
shutdown paths.

The session frame buffers and 8192-byte worker stack are unchanged; this candidate
does not select a final stack size.

## Exact preserved artifact

Artifact directory:

`_approved-artifacts/b58a6a27f35da600c8381e03fb5a10fbff46132c/stage8bp1s3sd/`

Manifest and binary are preserved there. The copy was verified immediately after the
clean build and must be flashed directly without invoking a PlatformIO build/upload.

- environment: `stage8bp1s3sd`
- `firmware.bin`: 1,479,536 bytes
- firmware SHA-256: `07CDAF954D23B2DEB4069444FC61D155189A761DA3A182421645E0DF7A260BD6`
- ELF SHA-256: `F518CBA9EB0FD7588E6C9A84E86621316D07F201A707AC945DD3705E2557C722`

## Resource/build result

Clean PlatformIO build succeeded with Espressif32 8.0.0, Arduino-ESP32 3.3.12,
ESP-IDF 5.5.5 and Arduino_GFX 1.6.5.

- static RAM: 88,884 / 327,680 bytes (27.1%)
- application flash: 1,451,287 / 6,553,600 bytes (22.1%)
- four SSE worker stacks: 32,768 bytes total at the diagnostic 8192-byte setting
- telemetry ring: 32 bounded records (observation-only)

## Software checks

- `python acceptance/stage10/http_sse_isolation_contract.py` — PASS
- `python acceptance/stage10/http_sse_isolation_deterministic.py` — PASS
- `python acceptance/stage10/structural_review.py` — PASS
- clean `pio run -e stage8bp1s3sd` — PASS

No hardware was flashed and no race was run.
