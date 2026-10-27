# HTTP/SSE isolation diagnostic — b58a6a2 physical run

Date: 2026-10-27

## Candidate and capture

- Source commit: `b58a6a27f35da600c8381e03fb5a10fbff46132c`
- Environment: `stage8bp1s3sd`
- Preserved artifact: `_approved-artifacts/b58a6a27f35da600c8381e03fb5a10fbff46132c/stage8bp1s3sd/firmware.bin`
- Artifact size: `1,479,536` bytes
- Artifact SHA-256: `07CDAF954D23B2DEB4069444FC61D155189A761DA3A182421645E0DF7A260BD6`
- Runtime build line: `source_commit=b58a6a27f35da600c8381e03fb5a10fbff46132c environment=stage8bp1s3sd app_elf_sha256=f518cba9e`
- Serial capture: `firmware/logs/device-monitor-261027-143835.log`

The artifact was written directly at `0x10000` with esptool; the tool reported `Hash of data verified`. No PlatformIO build or upload was invoked.

## Boot result

Boot was clean before the test: SD owner mount OK, AP `192.168.4.1` started, remembered STA detected and subsequently connected, mDNS started, HTTP server started with error 0, RGB display initialised with the production 16,000-pixel bounce buffers, and READY was displayed. No panic/reset occurred during idle startup.

Idle SSE telemetry before admission was:

| sample | HWM words | remaining bytes |
|---:|---:|---:|
| 1 | 1524 | 6096 |
| 2 | 1532 | 6128 |
| 3 | 1440 | 5760 |
| 4 | 1532 | 6128 |

The lowest idle HWM was therefore 1,440 words (5,760 bytes). No non-idle stage marker was emitted before the failure.

## Controlled test and first failure

The test opened a stalled page socket (`GET /`) and a stalled SSE socket (`GET /events`) against the house-LAN address `192.168.68.102`, without reading either response. A separate healthy SSE and ordinary route matrix was then attempted. The first complete failure occurred immediately after the first SSE admission:

```text
[HTTP_TRACE] phase=enter route=/events fd=51 session_ctx=0 clients=1 active=1 result=0 elapsed_ms=0 server_generation=1
[HTTP_ADMISSION] route=/events fd=51 outcome=checked clients=1 sse_slots=0 page_spaces=0 server_generation=1
[HTTP_ADMISSION] route=/events fd=51 outcome=admitted clients=1 sse_slots=1 page_spaces=0 server_generation=1
Guru Meditation Error: Core  0 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (pp_sse_0)
...
Rebooting...
```

The panic occurred before the route matrix could complete. The direct client timings captured before the process stopped were:

- `/health`: 12.110 s, no response bytes, `TimeoutError`.
- `/context`: HTTP 200 headers/body (367 bytes) were received, but the client-side read timed out waiting for connection close at 12.562 s.
- `/state`, `/proposal`, healthy SSE and post-release checks were not reached because the controller rebooted on the first SSE worker failure.

The log also contains the pre-existing/independent routine trace:

```text
[HTTP ERROR] route=/normal duration_ms=10736 result=45062 active=0 heap=116080 min_heap=114800
```

## Result

**FAIL — diagnostic run stopped at the first complete failure.**

The approved 8,192-byte worker configuration (2,048 words) still tripped the `pp_sse_0` stack canary on initial SSE admission. This run does not establish final HWM values for initial/notification/keepalive/stalled-send/failure/cleanup, nor does it establish the complete isolation matrix. It does establish that the current telemetry-safe candidate remains unsafe at the first worker startup path and that no production acceptance pass can be claimed.

No race, persistence operation or authority mutation was performed. The full capture is retained verbatim for Archie review.

