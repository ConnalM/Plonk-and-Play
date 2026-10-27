# 3d1c524 reconnect-accumulation physical failure analysis

Date: 2026-10-27  
Firmware: `3d1c524f34294df6669d650f50b2b098db4c4fd3`  
Environment: `stage8bp1s3sd`  
Artifact: `_approved-artifacts/3d1c524f34294df6669d650f50b2b098db4c4fd3/stage8bp1s3sd/firmware.bin`  
Artifact SHA-256: `8DA5000384C37D9C6A3C3E8FEFFC7ACDA8EDB0A836830E29EC944449C29D9451`  
Capture: [`HTTP_SSE_RECONNECT_FAILURE_3D1C524_20261027.log`](HTTP_SSE_RECONNECT_FAILURE_3D1C524_20261027.log)

This is a read-only analysis of the stopped physical reconnect test. No firmware,
browser product code, storage, authority or network behaviour was changed.

## Executive result

The failure is an admission-accounting defect, not a proven P&P SSE-session leak.
P&P compares the count returned by `httpd_get_client_list()` with a value of ten,
after subtracting three internal sockets a second time. The configured
`httpd_config_t::max_open_sockets` is thirteen; the installed ESP-IDF 5.5.5
header documents that the three internal sockets are already reserved by HTTPD.
`httpd_get_client_list()` returns active client sessions and its input capacity is
the configured `max_open_sockets` value. Consequently the current ordinary reserve
guard rejects `/events` at six clients (`10 - 4`) instead of at nine clients
(`13 - 4`). During recovery, ordinary keep-alive sessions and the bounded fresh
state/proposal/context acquisition occupy six or more client entries, so Browser A
cannot reacquire its second SSE slot even though ordinary routes remain responsive.

The smallest correction boundary is to make SSE/page admission use the configured
client-session capacity (thirteen) from one shared constant, while retaining the
ordinary reserve of four, the four-SSE limit, explicit 503/close rejection and all
Browser/authority semantics. This report does not implement that correction.

## A. Reconstructed connection lifecycle

The capture began with two intended clients:

| Phase | Evidence | Result |
|---|---|---|
| Initial | `fd=51`, `/events`, generation 1 | Laptop Browser A, spectator; admitted |
| Initial | `fd=58`, `/events`, generation 2 | Tablet Browser B, Master; admitted |
| Tablet Wi-Fi off | `STA_EVENT DISCONNECTED reason=8 ASSOC_LEAVE`; generation 1 later reports `send_failure`; generation 2 remains alive | The STA event is concurrent evidence, not proof that the tablet action caused the STA loss |
| STA recovery | `STA_EVENT CONNECTED`, then `GOT_IP`; `fd=52`, `/events`, generation 3, clients=3, `sse_slots=2` | A reconnect is admitted |
| Reconnect pressure | `fd=53`, `/events`, clients=6, `sse_slots=1`, `ordinary_capacity_exhausted`, result 257 | A reconnect is rejected |
| Replacement | generation 3 reports `cleanup=replacement`; ordinary `/context`, `/state`, `/proposal` continue returning 200 | No P&P SSE slot remains for that rejected attempt |
| Further recovery | `fd=57`, `/events`, clients=5, generation 4 admitted; it then reports `send_failure cleanup=disconnect` | A second candidate fails and is cleaned up |
| Sole survivor | `fd=56`, generation 5 admitted at clients=4; `sse_slots=1` thereafter | Browser B/one SSE remains healthy |
| Repeated attempts | fds 53, 55, 57, 58, 59 recur; counts fluctuate approximately 6–10; repeated `ordinary_capacity_exhausted` | Browser A remains without SSE; ordinary routes remain responsive |

The live capture was left running. The durable snapshot contains 2,123 lines at
the time of this report, including the complete stopped-test sequence and the
subsequent rejection/keep-alive evidence. The fd-to-browser identity of every
post-recovery `/events` attempt is not logged, so generation 3/4/5 cannot be
attributed to A or B beyond the authoritative context evidence; no identity is
invented here.

## B. Rejection path and socket reclamation

The source path is `BrowserInterface::events()` and
`BrowserInterface::rejectSseAdmission()` in
`firmware/include/pp/browser_interface.h` (approximately lines 1612–1623 and
1816–1860 at this commit). Admission rejection:

* sends `503 Service Unavailable`;
* sets `Connection: close`;
* sends a bounded text body;
* records the result (`result=257` in the physical trace); and
* returns `ESP_FAIL`, which the source comment and ESP-IDF contract identify as
  the HTTPD close path for the URI-handler socket.

The capture does not contain packet bytes, so the literal 503 wire response is
not independently packet-captured. It does contain the handler exit with result
257, the same fd values being reused by later requests, and client counts falling
after rejected attempts. This proves handler return and eventual fd/session
reclamation strongly enough for this diagnosis; it does not show an accumulating
P&P `SseSession` or async-request object. Rejected fds are not the population that
stays resident: they reappear as later ordinary requests or later rejected
attempts, while `sse_slots` remains one.

## C. What occupies the HTTPD client list

The ordinary requests are successful, short responses on keep-alive sessions:
`/state`, `/proposal` and `/context` repeatedly enter, send, complete and exit in
roughly 26–49 ms. Their fds are reused across routes. They are counted by
`httpd_get_client_list()` while idle between requests. A reconnect cycle also
performs the bounded fresh state/proposal/context acquisition, so two Browser
pages plus existing keep-alive and transient dead/idle peers can reach six or
more active client entries.

The controller remains alive, ordinary routes continue to return 200, and the
client count drains and reuses fds over time. The evidence therefore supports
active keep-alive/peer sessions plus admission pressure, not a permanent socket
leak. The tablet's abrupt Wi-Fi disappearance can leave a peer to be reaped by
normal send/timeout processing; it is a contributing transient, not a proven
separate lifecycle defect.

## D. Admission accounting defect

At `firmware/include/pp/browser_interface.h`:

```text
MaxHttpClientSockets = 10; // max_open_sockets(13) minus ESP-IDF's three internal sockets
OrdinaryRequestReserve = PP_HTTP_ORDINARY_REQUEST_RESERVE (4)
```

The `/events` and page guards reject when:

```text
fdsCapacity >= MaxHttpClientSockets - OrdinaryRequestReserve
```

The same class starts HTTPD with `config.max_open_sockets = 13`. In the installed
ESP-IDF 5.5.5 header,
`C:\Users\conna\.platformio\packages\framework-arduinoespressif32-esp32s3-libs\include\esp_http_server\include\esp_http_server.h`,
the `max_open_sockets` field is documented as the maximum number of connected
sockets/clients, with three sockets reserved for HTTPD's internal work. The
`httpd_get_client_list()` documentation says the output is the number of active
session fds and that the caller's array must be at least the configured
`max_open_sockets` size. The three internal sockets are therefore already
accounted for by HTTPD; subtracting them in P&P is incorrect.

This explains the physical boundary directly:

* current guard: reject at `6` (`10 - 4`);
* intended guard with the configured client capacity: reject at `9` (`13 - 4`);
* the trace shows rejection at clients `6`, `7`, `8`, `9` and `10` while
  ordinary requests continue to complete.

The ordinary reserve is not being weakened by this conclusion. It remains four
client slots; only the capacity arithmetic is corrected to match the HTTPD API.

## E. Browser recovery behaviour

The generated product code is `firmware/include/pp/normal_browser_page.inc`,
generated from `firmware/browser/embed_normal.py`. It has one `eventStream`, one
retry timer, one recovery-in-flight flag and one bounded backoff schedule
(2, 4, 8, 16 seconds). `retireEventStream()` clears handlers and calls
`EventSource.close()`. `scheduleEventStreamRecovery()` is the single entry point
for errors, liveness expiry and the interval watchdog. Offline normal polling is
gated; recovery performs one fresh acquisition cycle and does not run the former
250 ms request storm. Controls are restored only after fresh state/proposal/context
acquisition, with authority still derived from `/context`.

The existing software recovery/admission contract exercises these ownership and
backoff invariants. The physical capture's repeated `/events` attempts are
consistent with two Browser pages undergoing bounded recovery and with ordinary
acquisition pressure. This capture has no Browser console trace, so it cannot
prove that two independent EventSource objects existed or that native retry
escaped the single-owner schedule. No Browser code defect is established by this
run.

## F. Alternatives considered

* **P&P SSE-session leak:** not proven. `sse_slots` changes with admission,
  replacement and send-failure cleanup; rejected fds are reused.
* **HTTPD close failure for 503:** not proven. Source requests close and the
  physical fd/count behaviour is consistent with eventual close. A packet-level
  503/FIN trace is absent, so this remains a useful regression assertion, not the
  primary diagnosis.
* **Unbounded Browser reconnect storm:** not proven. The source has one bounded
  retry schedule and no ordinary offline poll storm. Browser console capture would
  be needed to distinguish that hypothesis from the server-side admission error.
* **Authority or race/persistence mutation:** not observed. Context, state and
  proposal remained authoritative and responsive; no race was run and no
  persistence change occurred.
* **Degraded SD mount:** present in boot evidence as a separate observation; it
  is not implicated in this HTTP/SSE admission failure.

## G. Smallest correction and deterministic proof required

After Archie review, make the capacity source single and explicit: use the
configured HTTPD client capacity (13 for this build) for `httpd_get_client_list`
and admission thresholds, retaining `OrdinaryRequestReserve=4`, four SSE slots,
503/`Connection: close`, existing async ownership and all Browser semantics.
Do not increase socket limits or reduce the reserve.

Before another physical attempt, add a deterministic software regression that:

1. keeps one healthy SSE admitted;
2. creates the bounded ordinary keep-alive population and a recovering second
   Browser;
3. verifies SSE admission at client counts 6–8 and rejection at 9+;
4. verifies rejected `/events` returns 503/close without allocating an
   `SseSession` or async request;
5. verifies fd reuse and session-count return to baseline;
6. verifies `/health`, `/context`, `/state`, `/proposal` and the healthy SSE
   remain responsive;
7. verifies abrupt peer disappearance/idle keep-alive reaping without a P&P
   session leak; and
8. verifies no authority, race or persistence state mutation.

The browser source recovery contract should remain unchanged unless a later
browser-console trace proves overlapping retry schedules. No physical retry is
justified until the capacity correction and deterministic proof are reviewed.
