# HTTP/SSE isolation architecture and conformance review

**Review type:** read-only design/conformance review  
**Baseline:** `1ea8786141e64fcd36abcbd85e19bf54212a52b3`  
**Scope:** Browser HTTP/SSE isolation only. No firmware, framework, race, persistence or authority changes are included in this review.

## Executive finding

The reported failure is consistent with a real server-task isolation defect. The current P&P server uses one ESP-IDF HTTPD task for URI handlers and for queued SSE work. A normal page handler sends its response synchronously in several chunks. SSE notification work is queued with `httpd_queue_work()`, but that callback also runs in the HTTPD task and calls `httpd_socket_send()`. A slow or stalled socket can therefore hold the shared HTTPD task until the configured send timeout (and a page consists of several such sends). While that task is blocked, unrelated `/health`, `/context`, page and SSE work cannot be serviced even though the controller and race system continue to run.

This is not fixed by increasing socket counts, send timeouts or retries. It requires moving potentially blocking response sends out of the HTTPD task, with explicit finite admission and ownership rules.

## Governing requirements

The review uses the following frozen requirements as the design boundary:

* `docs/SYSTEM_ARCHITECTURE.md` §§7.7, 11.2 and 11.4: Browser/Taster obtain current authoritative state through the Noticeboard; reconnect reconstructs current state rather than replaying events; a slow or disconnected remote participant must not block timing-critical local processing; stale presentation is not authoritative after synchronisation is lost.
* `docs/MESSAGE_CONTRACT.md` §§10 and 14: `NOTICEBOARD_CHANGED` tells a consumer to look again at current truth and may be coalesced; slow/non-critical Browser work must not block timing-critical processing.
* `docs/ACCEPTANCE_TESTS_STAGE_10.md` §§10.10, 10.11 and closure criteria: slow/sleeping/disconnected viewers may not block or alter authoritative timing, input, race engine/control or Noticeboard state; reconnect uses current Noticeboard state.
* `docs/ACCEPTANCE_TESTS_STAGE_14C.md` 14C.20, 14C.21 and 14C.26: reconnect reconstructs live state without Fact replay; presentation pressure/Browser absence cannot alter authoritative duration, crossings, classification or expiry; normal Browser Interface boundaries remain in force.
* `docs/ACCEPTANCE_TESTS_STAGE_15_LOGGING_DIAGNOSTICS_FOUNDATION.md` NET15.4, NET15.5, PS15.44, PS15.47–PS15.49 and PS15.51: AP/HTTP remain available independently of STA; Browser tracing is bounded/non-blocking; reconnect evidence records state acquisition/application/render; clients remain individually attributable; no new diagnostic transport or authority semantics are introduced.
* `docs/MASTER_PROJECT_PLAN.md` WP3/G3: normal Browser lifecycle/reconstruction, lease, spectators, multi-client SSE, reconnect/reacquisition and AP+STA/LAN access are product behaviour, not a new authority owner.

The proposed change is therefore infrastructure-only. It must preserve the current endpoints, Noticeboard/state/proposal/context reconstruction, identity cookies, lease/takeover rules, race/persistence semantics and AP+STA behaviour.

## Current implementation and proven conformance gap

The relevant implementation is `firmware/include/pp/browser_interface.h`.

### HTTP server configuration

`startServer()` starts ESP-IDF HTTPD with the following P&P overrides:

* `stack_size = 8192`
* `recv_wait_timeout = 5`
* `send_wait_timeout = 5`
* `max_open_sockets = 13`
* `lru_purge_enable = true`
* a route table containing `/`, `/normal`, `/events`, `/health`, `/context`, `/state`, `/proposal` and the product operations.

ESP-IDF 5.5.5's `HTTPD_DEFAULT_CONFIG()` documents three internally reserved sockets. Thus `13` is a finite global bound, not thirteen ordinary application clients. LRU purge can reclaim an eligible idle session when admission is exhausted; it does not make an active blocking send pre-emptible and it does not reserve capacity for `/health` or `/context`.

`MaxSseClients` is `PP_MAX_SSE_CLIENTS` (the current default is four), but there is no separate ordinary-request reservation or admission class. Four SSE sessions plus ordinary requests can consume the same global pool.

### Page responses

`normalPage()` sends the complete page synchronously with repeated `httpd_resp_send_chunk()` calls and a terminating chunk. Other JSON/detail/history routes use the same synchronous HTTPD handler model. The complete response must be preserved; truncating a page is not an acceptable isolation fix.

### SSE lifecycle and blocking point

`events()` synchronously sends the initial chunk, allocates an `SseSession`, stores the fd/generation in the HTTPD session context and leaves the chunked response open. Subsequent notifications are coalesced and queued with `httpd_queue_work()`.

`eventWork()`/`sendEventNotification()` validates the session pointer, fd context and generation, builds a bounded SSE frame, then calls `httpd_socket_send()` from the queued work callback. The callback is therefore running on the shared HTTPD task. A stalled socket send can occupy that task until the socket timeout. The same path calls `httpd_sess_trigger_close()` on failure but currently does not check its return value. Replacement and stale-generation checks exist, but the blocking send and close-failure ownership boundary are not sufficient to guarantee prompt reclamation.

The current comments describe queued SSE work as asynchronous relative to the request handler, but `httpd_queue_work()` means asynchronous *queueing into HTTPD context*, not an independent send worker. That comment and the Stage 10 slow-viewer requirement need reconciliation when the implementation is corrected.

### Why the observed failure fits

The observed 10–64 second `/`/`/normal` responses, `ESP_ERR_HTTPD_RESP_SEND`, and contemporaneous `/health` timeout while the controller remains alive are consistent with one or more blocking sends serialising the HTTPD task. Several five-second page chunks can accumulate. This evidence does not prove whether the initiating client was a page socket or SSE socket, nor whether all four SSE slots were exhausted; it does prove that the current shared-task send architecture permits either case to affect unrelated HTTP service.

The slower tablet/PC observations are also not automatically explained by the same defect. Serial service of SSE sends can delay one client behind another, but Browser scheduling/rendering/coalescing remains a separate possible contributor. The isolation change must remove cross-client blocking first, then the two-client perceptual-synchronisation requirement can be measured without conflating transport starvation with presentation behaviour.

## ESP-IDF 5.5 APIs and their implications

The installed framework header is:

`C:\Users\conna\.platformio\packages\framework-arduinoespressif32-esp32s3-libs\include\esp_http_server\include\esp_http_server.h`

The supported asynchronous request APIs are:

* `httpd_req_async_handler_begin(httpd_req_t *r, httpd_req_t **out)`: called by a URI handler to obtain a request copy usable on an asynchronous thread. The header explicitly says this is necessary for simultaneous request handling.
* `httpd_req_async_handler_complete(httpd_req_t *r)`: must be called on every success and error path. It frees the request and relinquishes the socket; failure to complete eventually prevents new connections from being accepted.
* `httpd_sess_trigger_close(handle, fd)`: queues a bounded close request and returns a status. `ESP_FAIL`, `ESP_ERR_NOT_FOUND` and invalid-argument outcomes must be handled rather than ignored.
* `httpd_queue_work()`: runs the callback in HTTPD context. It is appropriate for short non-blocking control/close work, not for a potentially blocking page or SSE send.
* `httpd_socket_send()`: the low-level send primitive. ESP-IDF documents it for advanced asynchronous communication; calling it from the HTTPD work queue still blocks the HTTPD task, so it is not an isolation boundary by itself.

The implementation must use request copies and a dedicated worker/task context for sends. It must not call a raw socket API from an arbitrary task unless the chosen ESP-IDF async-request pattern establishes the required ownership and thread-safety. The implementation review must follow the framework's `async_requests` example and the exact 5.5.5 headers/source.

Reference: [ESP-IDF 5.5 HTTP server API](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/protocols/esp_http_server.html).

## Options considered

| Option | HTTPD task isolation | Resource cost/bounds | SSE effect | Assessment |
|---|---|---|---|---|
| Keep synchronous sends and tune timeouts/counts | None. A blocked send still owns HTTPD. | Low code cost, no real reservation. | Unchanged shared-task starvation. | Reject. |
| Move sends to `httpd_queue_work()` | None. Queue callbacks execute in HTTPD context. | Queue is finite but the wrong task remains blocked. | Current failure, only relabelled. | Reject. |
| One generic worker for all pages and SSE | HTTPD is released, but one stalled worker can delay every other send. | Small task cost, easy bound. | Fails the requirement that one stalled SSE cannot delay another healthy SSE. | Reject as the final design. |
| Async request + bounded page worker(s), and one worker per active SSE stream | HTTPD only accepts/admit/enqueues; blocking send belongs to the owning worker. Ordinary requests remain serviceable. | Fixed page queue/worker count plus at most `MaxSseClients` stream workers; explicit stack/queue memory. | Preserves chunked pages and long-lived SSE while isolating each stream. | Recommended minimum. |
| Replace HTTPD with a different server/framework | Potentially good isolation but changes framework/transport surface. | Large migration and compatibility risk. | Product behaviour and APIs must be re-proven. | Out of scope for this correction. |

## Recommended implementation envelope

Use ESP-IDF's async request ownership with fixed, explicit application bounds.

### Capacity

Keep the current global HTTPD limit and explicitly reserve ordinary request capacity. Treat the current four SSE sessions and a small fixed page-job capacity as separate application budgets, not as an unbounded heap queue. The exact constants must be selected from the measured S3 memory budget in implementation review, but the invariant is:

* at least four ordinary request slots remain available for `/health`, `/context`, `/state`, `/proposal` and a healthy Browser;
* no more than four SSE streams are admitted (the existing product bound);
* page jobs have a fixed queue and at least two worker slots, so one stalled page cannot hold the only page worker;
* queue-full/admission failure is explicit (for example 503) and never silently drops a valid page response;
* every allocated job/request/worker has one owner and one terminal release path.

Because HTTPD's socket pool is global, app-level SSE admission must account for current client sessions and refuse a new SSE before consuming the ordinary reserve. LRU remains a last-resort idle-session policy, not the reservation mechanism.

### Page lifecycle

1. URI handler validates/adopts only request metadata and calls `httpd_req_async_handler_begin()`.
2. It enqueues a fixed-size `PageJob` containing the async request, fd, client identity metadata and a generation token. If no page slot is available, it completes the async request and returns an explicit bounded failure.
3. A page worker builds the complete response using immutable authoritative snapshots and sends the existing chunks plus terminating chunk. It must not hold a Race Engine, Race Control, persistence, authority or Noticeboard mutation lock while sending.
4. On success, send failure, disconnect or stale generation, the worker calls `httpd_req_async_handler_complete()` exactly once and releases the job slot.
5. The complete response remains intact; isolation is achieved by worker ownership, not by truncating or inventing content.

### SSE lifecycle

1. `/events` admits only if the existing client identity replacement and bounded SSE/ordinary capacity rules permit it.
2. The handler creates an async request ownership record, assigns a monotonically increasing generation, and enqueues the initial event to that session's dedicated stream worker. The HTTPD handler returns promptly; it does not perform a potentially blocking initial send.
3. Each active SSE stream has one bounded worker/slot (at most `MaxSseClients`). The worker snapshots the small current notification payload before sending and performs the send without a shared P&P lock.
4. Notification coalescing remains legal: a notification means “look again at current authoritative state”, not replay of every event. It must not become a cross-client queue that makes one stream wait behind another.
5. A send failure, transport disconnect, replacement/reconnect or server stop marks the generation invalid, prevents future sends, and causes exactly one close/completion path. Close-request return values are recorded/handled; a failed close request must not leave the session counted as live.
6. fd reuse is protected by `(fd, generation, session identity)` checks. A worker must not send to a newly assigned client that inherited the same fd.
7. On Browser liveness loss, the existing Browser watchdog/reconnect path remains authoritative: mark unsynchronised, retire the stream, reacquire `/state`, `/proposal` and `/context`, and only then restore controls. No `isMaster`, race state or network state is persisted or manufactured in Browser storage.

### Synchronisation and locking

Workers may copy an immutable presentation snapshot or a bounded event payload under a short-lived read/copy section. No shared P&P/race-critical lock may be held during socket sends, queue waits or worker blocking. The worker must not call product operations as a side effect of transport failure; only the existing disconnect/reconstruction path may update Browser connection state.

## Required regression and acceptance

Add a focused transport-isolation fixture before physical validation. It should use the real route table/HTTPD profile with deterministic test sockets or an ESP-IDF-supported test harness:

1. Open a page request whose client deliberately stops reading. Assert the HTTPD task remains able to accept/service other requests.
2. Open an SSE connection whose client deliberately stops reading. Assert this does not delay a second healthy SSE stream.
3. While both are stalled, issue `/health` and `/context` from a healthy Browser and assert bounded completion below the agreed service budget.
4. Verify the healthy Browser receives fresh authoritative state and that controls are disabled until its reconnect sequence has acquired `/state`, `/proposal` and `/context`.
5. Verify page chunks are complete and the terminating chunk is sent when the worker succeeds; a deliberate worker/socket failure produces an explicit failure and releases the request.
6. Replace/reconnect an SSE identity and exercise fd reuse. Assert stale queued work cannot send to the replacement generation.
7. Fill every page/SSE budget. Assert bounded 503/admission failure, no heap growth/socket leak, and recovery after clients disconnect.
8. During the test, assert no Race Engine timing, Race Control request, authority lease, Noticeboard authoritative state or persistence record changes except those explicitly caused by separate test actions.

The smallest later physical regression is one stalled page client plus one stalled SSE client, with a second healthy Browser repeatedly requesting `/health` and `/context` while the controller is idle (then once during a harmless authoritative state change). Release the stalled clients, verify explicit completion/reclamation, and inspect Serial/health for no panic, socket leak, authority mutation, race mutation or persistence fault. A separate two-client race-latency acceptance should measure start-light stages and lap delivery after transport isolation; the framework change alone must not be credited as fixing Browser presentation/coalescing until that evidence exists.

## Contradictions/reconciliation required before implementation

1. The Stage 10 slow-viewer requirement and the architecture/message contracts require isolation, while the current SSE comment implies that `httpd_queue_work()` makes sends non-blocking. The implementation must remove/reword that implication and add the stalled-page/SSE contract above.
2. Stage 14C.21 protects authoritative Endurance semantics under presentation pressure but does not by itself prove prompt `/health`/`/context` service. Keep it and add the transport-isolation test; do not weaken 14C.21.
3. PS15.51 prohibits adding a *diagnostic* state channel or changing Browser authority semantics. This Browser transport correction is product infrastructure and must continue to use the existing notification/state/proposal/context paths; it is not a new diagnostic transport.
4. Current `MaxSseClients=4`, `max_open_sockets=13` and LRU=true are finite but do not constitute an ordinary-request reservation. Record the explicit reserve and admission rule in the HTTP/SSE contract when the implementation lands.
5. The observed PC/tablet lag and grouped start lights remain a separate acceptance concern. Isolation is necessary to prevent a slow client delaying a healthy client, but it does not prove that Browser rendering/coalescing is correct. Add the two-client perceptual-synchronisation criterion to the eventual acceptance evidence.

## Decision

Proceed to implementation only after Archie reviews this envelope. The minimum robust design is **ESP-IDF async request ownership plus fixed page workers and one bounded worker per active SSE stream**, with ordinary-request capacity reserved and all completion/failure/disconnect/replacement paths explicit. Do not implement by raising timeouts/socket counts, by using `httpd_queue_work()` for sends, by truncating pages, or by changing authority/race/persistence semantics.

No source, build, firmware or hardware changes were made by this review.
