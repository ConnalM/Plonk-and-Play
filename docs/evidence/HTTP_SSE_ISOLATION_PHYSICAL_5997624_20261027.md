# HTTP/SSE isolation physical diagnostic — 5997624

Date: 2026-10-27

## Candidate and provenance

- Source commit: `5997624ea336d495ad2725c79bdf33d3b2d25857`
- Environment: `stage8bp1s3sd`
- Preserved artifact: `_approved-artifacts/5997624ea336d495ad2725c79bdf33d3b2d25857/stage8bp1s3sd/firmware.bin`
- Artifact size: `1464112` bytes
- Artifact SHA-256: `94301567B7D7FA9AECAE43083D16077B491204B001045B6300A6C35808856EF7`
- Runtime `/health`: `sourceCommit=5997624ea336d495ad2725c79bdf33d3b2d25857`, `buildEnvironment=stage8bp1s3sd`, `appElfSha256=d8564b686`
- Direct flash: application written at `0x10000`; esptool reported `Hash of data verified` and hard reset.

## Capture

- Serial capture: `firmware/logs/device-monitor-http-sse-5997624.log`
- Boot remained alive for the capture interval. The log contains no panic, reset, stack-canary or reboot marker.
- SD self-test reported `BOOT_SELF_TEST=PASS`. The `/health` pre-test request completed with HTTP trace `send_complete` and `exit` in 26 ms.

## First complete transport failure

The intended topology could not be established cleanly because SSE sessions were already resident before the probe reached its healthy-SSE admission:

1. Existing sessions occupied `sse_slots=2`.
2. The probe admitted another `/events` session as generation 3, fd 55.
3. The next `/events` request (the intended healthy SSE) reached the HTTP handler but was rejected explicitly:

```
[HTTP_TRACE] phase=enter route=/events fd=56 ... clients=6 active=2 ...
[HTTP_ADMISSION] route=/events fd=56 outcome=checked clients=6 sse_slots=3 page_spaces=0 ...
[HTTP_ADMISSION] route=/events fd=56 outcome=ordinary_capacity_exhausted clients=6 sse_slots=3 page_spaces=0 ...
[HTTP_TRACE] phase=exit route=/events fd=56 ... result=257 elapsed_ms=29 ...
```

This is the first complete failure boundary. Per the approved procedure the run was stopped; no retry, reboot, reflash or race was performed. Because the required healthy-SSE topology was not established, this run is **INCONCLUSIVE for NET15.14 isolation**, not a pass.

## Observed SSE telemetry

The exercised initial/keepalive paths remained well clear of the 8192-byte diagnostic allocation:

| worker generation | path | minimum free HWM | observed use |
| --- | --- | ---: | ---: |
| 1 | initial/keepalive | 5992 bytes | 2200 bytes |
| 2 | initial/keepalive | 5984 bytes | 2208 bytes |
| 3 | initial/keepalive | 5992 bytes | 2200 bytes |

No notification, stalled-send, send-failure, disconnect, replacement or shutdown path was reached in this attempt. Therefore these values do not select the final production stack size.

## Interpretation

- The bounded admission guard is active and rejects excess SSE work instead of admitting an unbounded fourth stream.
- This evidence does not prove that ordinary `/health`, `/context`, `/state` or `/proposal` remain responsive while the intended two stalled clients and one healthy SSE are held; those route timings were not reached after the fourth SSE admission failed.
- The resident sessions must be cleared or a clean controller state established before the focused topology can be evaluated. No firmware correction is justified by this incomplete run.
