# Browser reboot-recovery acceptance precondition failure — 5997624

Date: 2026-10-27

## Scope

The planned controller power-cycle was **not performed**. The two-Browser synchronized precondition failed before restart.

## Roles established

Controller-side `/context` evidence established:

- Browser B: `client_tag=2`, `is_master=true`, `result=ACCEPTED`.
- Browser A: `client_tag=3`, `is_master=false`, `result=NONE`.

The tablet then displayed intermittent `Controller unavailable`, so the required visible synchronized state was not stable enough to power-cycle.

## First complete failure boundary

The tablet/browser recovery attempts repeatedly reached `/events` but were rejected by the bounded admission guard:

```
[HTTP_TRACE] phase=enter route=/events fd=57 ... clients=7 active=1 ...
[HTTP_ADMISSION] route=/events fd=57 outcome=checked clients=7 sse_slots=1 page_spaces=0 ...
[HTTP_ADMISSION] route=/events fd=57 outcome=ordinary_capacity_exhausted clients=7 sse_slots=1 page_spaces=0 ...
[DEV] HTTP ERROR route=/events duration_ms=24 result=257 ...
[HTTP_TRACE] phase=exit route=/events fd=57 ... result=257 ...
```

The same failure recurred with client counts 7, 8, 9, 10 and 11. `/context`, `/state` and `/proposal` continued to complete successfully while these SSE reconnects were rejected. The tablet therefore recovered briefly through ordinary state acquisition, then lost liveness again when its EventSource could not be admitted.

## Interpretation

- This is not a controller reboot result; no power-cycle, refresh, retry or takeover was performed.
- The existing Browser B SSE remained active (`generation=11`, fd 52, keepalive HWM 5784 bytes), while reconnect attempts accumulated ordinary HTTP clients and were rejected.
- The first proven boundary is SSE admission exhaustion caused by the reconnect/client population, not authority reconstruction. The controller continued serving ordinary state/proposal/context requests and remained alive.
- The acceptance test is stopped as required. No firmware change is made by this evidence commit.

Capture: `firmware/logs/device-monitor-http-sse-5997624-recovery.log`.
