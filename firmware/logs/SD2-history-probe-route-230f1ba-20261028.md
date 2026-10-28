# SD-2 missing-medium `/history` probe via proven route — 230f1ba

Date: 2026-10-28  
Firmware: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Proven route: `192.168.68.102`  
Capture: `device-monitor-sd2-history-probe-route-230f1ba-20261028.log`  
Capture SHA-256: `F374BE91D8DD824A6C775B1FD762FB3FBCCB4D26834DF21992752DF30BC70F51`

## Result

**FAIL — missing-medium History read is explicit but the owner did not transition degraded.** The route was first proven with `/health` returning HTTP 200 and a matching Serial handler trace. Exactly one `/history` request was then issued.

## Decisive evidence

- `/health` on `192.168.68.102` returned HTTP 200 and Serial recorded the request on this controller.
- `/history` returned HTTP headers, then terminated after approximately 2.17 seconds with result `260`.
- Fault-boundary diagnostic:
  - `history_payload_read slot=18 expected=298 open=0 file_size=0 bytes_read=0 result=open`
  - `HTTP ERROR route=/history duration_ms=2172 result=260`
- No autonomous `SD.begin()`, remount, recovery, retry storm, panic, reset or watchdog event appeared.
- Browser/SSE keepalives and `/health` remained responsive; authority and race state were unaffected.

## Classification

The missing medium is now proven to fail at the selected History payload open, with a bounded explicit HTTP failure. However, no owner-unavailable/degraded or durable-health transition was reported after the failed read; that observability/recovery behavior remains open. The SD card remains out and no further storage request or recovery was attempted.
