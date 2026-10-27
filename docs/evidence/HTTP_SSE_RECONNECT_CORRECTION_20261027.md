# HTTP/SSE reconnect and admission correction

This change closes the reconnect accumulation boundary identified in the
`5997624` recovery capture. It is infrastructure-only: authority, race,
control-lease, persistence, AP+STA and Browser product semantics are unchanged.

## Server boundary

Every `/events` admission rejection now uses one terminal helper. It returns a
minimal `503 Service Unavailable` response, sets `Connection: close`, sends the
bounded text body, and returns `ESP_FAIL` so ESP-IDF closes the URI-handler
socket. The helper is reached before an `SseSession` is activated and before
`httpd_req_async_handler_begin()` creates an async request copy. The four-SSE
limit and ordinary-request reserve remain unchanged.

## Browser boundary

The generated Normal Browser owns one EventSource recovery schedule. On an
error, connect watchdog expiry or liveness expiry it marks the page offline,
detaches and closes the current EventSource, and schedules one exponential
retry (2 s, 4 s, 8 s, then capped at 16 s). Native EventSource retry is
effectively disabled because the failed object is retired immediately. A timer
or recovery-in-flight guard prevents duplicate attempts.

While offline, ordinary notification polling is ignored. A successful
replacement stream performs a fresh `/state`, `/proposal` and `/context`
acquisition before controls are restored; `/context` remains the sole source of
authority. Healthy operation remains SSE-driven and does not depend on polling.

## Evidence

- `acceptance/stage10/http_sse_isolation_contract.py` checks the single 503/close
  path, pre-async rejection ordering, bounded resources, and no legacy session
  allocation.
- `acceptance/stage14c/browser_sse_admission_recovery_runtime.js` proves one
  EventSource owner, deterministic retirement, bounded backoff, no offline
  acquisition storm, fresh reconstruction, and control restoration only after
  synchronisation.
- Existing liveness, notification, Stage 15 networking, WP3/WP4 and Persistent
  Storage Foundation contracts remain in the suite.

The live HTTP tests that bind local sockets require the host test environment;
the software contracts and Browser runtime regressions are deterministic and
do not require a device or network service.
