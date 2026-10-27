# SSE reconnect/client accumulation analysis — 5997624

Date: 2026-10-27  
Implementation reviewed: `5997624ea336d495ad2725c79bdf33d3b2d25857`  
Physical capture: `firmware/logs/device-monitor-http-sse-5997624-recovery.log`  
Precondition evidence: `432d05b68e88088aada5abe2167df241e4fd93a6`

## Scope and outcome

This was a read-only analysis of the stopped two-Browser recovery precondition. No
firmware, Browser product semantics, authority state, race state or persistence
state was changed.

Browser B remained the one admitted SSE (`generation=11`, `fd=52`,
`sse_slots=1`). Browser A repeatedly tried `/events`; those attempts were
rejected at the ordinary-capacity guard while `/context`, `/state` and
`/proposal` continued to complete. The controller stayed alive and did not
reboot.

The first proven cause is a reconnect/client-population feedback loop, not a
second admitted SSE or an unreleased P&P `SseSession`:

1. Browser A's `/events` attempt is refused when the HTTPD client list reaches
   the ordinary-capacity threshold.
2. The handler does not send a 503 body; it only sets the status and returns
   `ESP_FAIL` with trace result `257` (`ESP_ERR_NO_MEM`, not an HTTP status).
3. The ESP-IDF URI-handler contract says a non-`ESP_OK` handler result closes
   the underlying socket. The repeated `fd=57`/`fd=58`/`fd=59`/`fd=60`/`fd=61`
   reuse and later fall-back from client counts 11 to 7 are consistent with
   those rejected sockets being closed/reclaimed, not permanently leaked.
4. While SSE admission is failing, Browser A continues its normal recovery
   acquisition (`/noticeboard`, `/context`, `/state`, `/proposal`) and keeps
   retrying EventSource. These ordinary HTTP/1.1 sessions remain visible to
   `httpd_get_client_list` after their handlers have exited. The replacement
   requests therefore refill the client population faster than the HTTPD/LRU
   close/reap path drains it.

The current defect is therefore a combination of Browser retry/poll pressure and
an insufficiently explicit server rejection lifecycle. A permanent socket leak
is not proven by this capture.

## A. Reconstructed connection lifecycle

The relevant capture sequence is:

| Capture lines | fd / clients | Result |
|---|---:|---|
| 630–634 | `fd=57`, 7 clients | `/events` enters, is checked, rejected as `ordinary_capacity_exhausted`, exits with result `257` in 24 ms |
| 635–643 | `fd=57`, 7 clients | Two more rejected attempts; no SSE slot is allocated |
| 644–657 | `fd=58`, then `59`, then `60`; 8→10 clients | `/state`, `/proposal`, `/context` all enter/send/exit successfully; no handler remains active |
| 659–675 | `fd=60`, `59`, `57`, `58`; 10→7 clients | Four more rejected `/events` attempts |
| 676–712 | `fd=57`, 7 clients | Repeated rejected attempts with the same fd reused |
| 713–724 | `fd=58`, `59`, `60`; 8→10 clients | Another successful state/proposal/context acquisition sequence |
| 725–742 | `fd=60`, `59`, `57`, `58`; 10→7 clients | Rejected `/events` attempts; fd reuse continues |
| 775–791 | `fd=58`, `59`, `60`, `61`; 8→11 clients | Another successful acquisition sequence followed by rejection at 11 clients |
| 794–810 | `fd=60`, `59`, `57`, `58`; 10→7 clients | Rejected attempts and subsequent fd reuse |

For a rejected request, `events()` performs no `allocateSseSession()`, no
`httpd_req_async_handler_begin()`, no `httpd_sess_set_ctx()`, and no worker
notification. The trace has `session_ctx=0`, `sse_slots=1`, and no
`send_begin`/`send_complete` pair. The source only calls:

```cpp
httpd_resp_set_status(request, "503 Service Unavailable");
trace.complete(ESP_ERR_NO_MEM);
return ESP_FAIL;
```

Thus P&P does not transmit an explicit error response from this rejection path.
The installed ESP-IDF 5.5.5 header documents that a URI handler must return
`ESP_OK`; any other return value closes the underlying socket. This is the
server-side close mechanism used here. A `Connection: close` header and an
explicit bounded error body are nevertheless absent, so the Browser receives a
transport failure rather than a deliberate, retry-bounded admission response.

The fd reuse and falling client counts prove eventual release of rejected
connections in this run. They do not prove that every close is prompt, and they
do not exclude HTTPD/LRU timing pressure while the sockets are being retired.

## B. Browser EventSource and recovery behaviour

The generated Normal Browser (`firmware/browser/embed_normal.py`, embedded into
`firmware/include/pp/normal_browser_page.inc`) has these relevant paths:

- `openEventStream()` refuses a second JavaScript stream while the current one is
  `OPEN` or `CONNECTING`; `retireEventStream()` calls `EventSource.close()` before
  replacement. Therefore the source does not intentionally maintain two live
  Browser EventSource objects at once.
- A rejected request is delivered to `stream.onerror`; the code marks the page
  offline, but only retires/schedules its own replacement when the browser has
  already moved the stream to `CLOSED`. While it remains `CONNECTING`, native
  EventSource retry behaviour remains in control.
- A five-second connection watchdog retires a stream that is not `OPEN`, calls
  `poll('recovery')`, and schedules another open after two seconds. The
  fifteen-second recovery interval also calls `openEventStream()` and
  `poll('recovery')` when the stream is not open.
- The page’s normal 250 ms poll interval continues while unsynchronised. In the
  authoritative Stage/authority page, `poll()` is `authorityPoll()` and fetches
  `/noticeboard`, `/context`, `/state` and `/proposal` in sequence. A polling
  guard prevents overlap inside one page, but it does not provide exponential
  backoff or stop the acquisition loop while SSE admission is unavailable.
- These `fetch()` calls do not request `Connection: close`; successful responses
  therefore leave HTTP/1.1 client sessions eligible for reuse/idle retention.

The combination can create a reconnect storm faster than the server’s socket
close/LRU path drains it: native EventSource retry plus the five-second watchdog
replacement are accompanied by continuous authoritative reacquisition. The
current source bounds the JavaScript object count to one reference, but it does
not bound transport-attempt rate or transient HTTPD client population.

## C. Server rejection behaviour

The installed HTTPD configuration in `browser_interface.h` is:

- `max_open_sockets = 13` (three are reserved internally; P&P treats ten as
  client capacity);
- `recv_wait_timeout = 5` seconds;
- `send_wait_timeout = 5` seconds;
- `lru_purge_enable = true`;
- `PP_MAX_SSE_CLIENTS = 4`;
- `PP_HTTP_ORDINARY_REQUEST_RESERVE = 4`.

`events()` rejects when `httpd_get_client_list()` reports at least
`MaxHttpClientSockets - OrdinaryRequestReserve` (six client entries), so the
ordinary reserve is active. It leaves the existing B stream alone and leaves
`sse_slots` at one. The rejection is therefore doing the intended admission
protection for ordinary requests, but the protocol response is incomplete: the
status is set but never sent, no retry hint is emitted, and the path relies on
handler-failure socket closure.

The capture shows no rejected-request async object or P&P session to clean up.
The count reaching 11 is a snapshot of the HTTPD client list under pressure;
the exact accounting of the three internal sockets is an HTTPD detail, not
evidence that P&P admitted eleven SSE clients. The observed return to seven and
fd reuse are against a permanent server/session leak.

## D. Required correction boundary

The narrowest correction supported by this evidence is not a socket-count or
timeout increase and not an authority change. It has two bounded parts:

1. **Server rejection:** make ordinary-capacity `/events` rejection explicit
   and deterministic: send a minimal 503 response with `Connection: close`
   (or the equivalent supported HTTPD close response) and retain exactly one
   close/error path. Do not allocate an SSE session or async request on this
   path.
2. **Browser retry control:** when `/events` is rejected or fails, retire the
   current EventSource deterministically, suppress native/custom duplicate
   retries, and use one bounded backoff/retry schedule. While unsynchronised,
   suspend the 250 ms full state/proposal/context polling loop and perform one
   bounded reacquisition attempt per retry cycle. Controls remain disabled until
   fresh `/state`, `/proposal` and `/context` succeed; authority remains solely
   server-derived.

This preserves the current four-SSE limit, ordinary-request reserve, fd/
generation protection, authority semantics and product state. It also keeps
the already accepted HTTP/SSE worker isolation design unchanged.

## E. Focused software regression before another physical test

Add a deterministic HTTPD/browser harness which:

1. Holds one healthy SSE and drives a second Browser’s `/events` admission to
   the ordinary-capacity rejection boundary.
2. Repeats the Browser’s rejection/reconnect sequence, including the native
   EventSource error state and watchdog timing, while the page performs its
   normal recovery acquisition.
3. Asserts each rejected `/events` attempt receives the explicit bounded error
   and closes/reclaims its session; no `SseSession` slot is allocated.
4. Asserts at most one EventSource attempt is outstanding and retry/backoff is
   bounded; unsynchronised polling does not create an unbounded request loop.
5. Asserts `/health`, `/context`, `/state` and `/proposal` remain within their
   budget and a healthy SSE remains responsive.
6. Runs the fd-reuse/replacement path and verifies stale work cannot target the
   replacement fd/session.
7. Releases the pressure and asserts client/session counts return to baseline,
   with no authority, race or persistence mutation.

This regression should precede any further physical recovery acceptance. No
firmware change is made by this report.
