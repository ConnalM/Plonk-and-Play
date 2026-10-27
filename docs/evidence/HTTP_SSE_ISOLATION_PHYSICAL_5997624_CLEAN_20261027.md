# HTTP/SSE isolation clean-topology diagnostic — 5997624

Date: 2026-10-27

## Starting-state reconciliation

The earlier inconclusive attempt left three local Python transport-test process pairs alive. Host TCP inspection identified three process-owned groups with three connections each to `192.168.68.102:80`; these were the diagnostic harness processes, not ordinary Browser windows. Only the three proven TCP-owning Python processes were terminated. No firmware process, Browser window or controller restart was touched.

After termination, the host had no established connections to the controller. A controlled `/events` probe then showed `sse_slots=0` before admission, and its disconnect produced the expected fault-only cleanup marker. A second probe again showed `sse_slots=0` before admission. This established a clean baseline without rebooting.

## Candidate and provenance

- Source commit: `5997624ea336d495ad2725c79bdf33d3b2d25857`
- Environment: `stage8bp1s3sd`
- Preserved artifact: `_approved-artifacts/5997624ea336d495ad2725c79bdf33d3b2d25857/stage8bp1s3sd/firmware.bin`
- Artifact size: `1464112` bytes
- Artifact SHA-256: `94301567B7D7FA9AECAE43083D16077B491204B001045B6300A6C35808856EF7`
- Runtime `/health`: source commit and environment matched; ELF prefix `d8564b686`
- SD self-test: `BOOT_SELF_TEST=PASS`
- No panic, reset or reboot occurred.

Capture: `firmware/logs/device-monitor-http-sse-5997624-clean.log`.

## Controlled topology and results

The same running firmware was used. The probe held one page request and one `/events` request without reading them, then used a separate ordinary client for each framed route and opened one healthy SSE client. No other host TCP sessions were present.

Client-side completion timings (response framing, not TCP close):

| route | result | elapsed |
| --- | --- | ---: |
| `/health` | HTTP 200, 193 bytes | 202.7 ms |
| `/context` | HTTP 200, 179 bytes | 200.1 ms |
| `/state` | HTTP 200, 889 bytes | 70.1 ms |
| `/proposal` | HTTP 200, 685 bytes | 108.4 ms |

The healthy SSE received HTTP 200 headers and an initial event while both held clients remained open. It stayed admitted for the 12-second hold; the harness then closed all three held/SSE sockets deliberately.

Representative controller traces:

```
[HTTP_ADMISSION] route=/ fd=51 outcome=admitted clients=1 sse_slots=0 page_spaces=1
[HTTP_ADMISSION] route=/events fd=52 outcome=checked clients=2 sse_slots=0 page_spaces=0
[HTTP_ADMISSION] route=/events fd=52 outcome=admitted clients=2 sse_slots=1 page_spaces=0
[HTTP_TRACE] phase=send_complete route=/health ... elapsed_ms=26
[HTTP_TRACE] phase=send_complete route=/context ... elapsed_ms=38
[HTTP_TRACE] phase=send_complete route=/state ... elapsed_ms=28
[HTTP_TRACE] phase=send_complete route=/proposal ... elapsed_ms=27
[HTTP_ADMISSION] route=/events fd=53 outcome=checked clients=3 sse_slots=1 page_spaces=0
[HTTP_ADMISSION] route=/events fd=53 outcome=admitted clients=3 sse_slots=2 page_spaces=0
```

After release, a post-release `/health` returned the correct provenance payload; its controller trace completed in 26 ms. The host had no remaining established controller TCP sessions. A final controlled `/events` admission again reported `sse_slots=0` before its own admission, confirming reclamation.

## SSE stack telemetry

Minimum free bytes observed on the 8192-byte diagnostic stacks:

| path | minimum free bytes | maximum observed use |
| --- | ---: | ---: |
| initial | 5984 | 2208 |
| keepalive | 5984 | 2208 |
| send_failure | 5984 | 2208 |
| disconnect cleanup | 5984 | 2208 |

Notification, stalled-send, replacement and shutdown were not naturally exercised and were not manufactured. The measured 2208-byte use is therefore the worst observed, not a complete final-stack sizing basis. No final production stack size is selected by this run.

## Result

The clean-topology transport isolation test passed for the exercised scope:

- stalled page and stalled SSE were admitted without consuming ordinary route service;
- `/health`, `/context`, `/state` and `/proposal` completed with framed responses;
- a healthy SSE was admitted and remained responsive during the hold;
- release reclaimed the SSE sessions and ordinary service remained responsive;
- no stale host sockets, panic, reset, authority mutation, race or persistence activity was observed.

The unexercised notification/stalled-send/replacement/shutdown stack paths remain outside this run's measured evidence.
