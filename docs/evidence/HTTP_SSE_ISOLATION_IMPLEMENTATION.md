# HTTP/SSE isolation implementation evidence

This record covers the implementation of the approved bounded HTTP/SSE
transport boundary. It does not change Browser product, authority, race,
persistence, AP+STA or reconnect semantics.

## Runtime boundary

On the migrated ESP-IDF 5.x image, page handlers take ownership with
`httpd_req_async_handler_begin()` and return promptly. Two fixed page-worker
tasks drain a depth-two `PageJob` queue; each worker sends the complete existing
page, including its terminating chunk, and releases the async request exactly
once on success, failure, stale generation or shutdown. Admission rejects
explicitly when the queue is full or the ordinary-request reserve would be
consumed.

Each admitted SSE connection has one static session record and one dedicated
worker task. At most the existing four sessions are admitted. Sends use the
owned async request, never `httpd_queue_work()` or a raw socket API. Session
identity is checked by server generation, fd, generation and registered
session pointer before every send. Replacement, disconnect, close failure and
server shutdown all invalidate the session and converge on one completion path.

The legacy IDF 4.x acceptance path retains its synchronous compatibility
fallback because the async request ownership API is unavailable there; the
production migrated environment uses the async path.

## Fixed resource bounds

| Resource | Bound |
|---|---:|
| HTTPD configured sockets | 13 (10 client sockets after the framework's internal allocation) |
| ordinary-request reserve | 4 client sockets |
| page workers | 2 tasks, 8,192-byte stack each |
| page queue | 2 jobs (two active plus two queued maximum) |
| SSE workers | 4 tasks, 8,192-byte diagnostic stack each |
| SSE sessions | 4 (unchanged product limit) |

The dedicated SSE worker stack reservation is 32,768 bytes total, before
FreeRTOS TCBs and the small queue/session records. This is a diagnostic starting
allocation; the final production value remains evidence-led and must be based
on measured worst-case free bytes plus an explicit margin.

## Thread-safety boundary

The installed ESP-IDF 5.5.5 header defines `httpd_req_async_handler_begin()` as
the supported way to obtain a request copy usable on an asynchronous thread and
requires one matching `httpd_req_async_handler_complete()` on every terminal
path. The same HTTPD API documentation warns that the general server APIs are
not thread-safe. P&P therefore gives each async request copy to one exclusive
per-client worker: no request copy or fd is sent by two workers, and every send
and release checks server generation, fd, generation and registered session
identity. There is no global send mutex, because that would recreate
cross-client blocking. Session mutation and completion remain lifecycle
operations owned by the generation-bound session path; arbitrary concurrent use
of one request/session is not permitted.

## Product routes

The generated Normal Browser routes both `/` and `/normal` to the bounded `page`
admission handler. The page worker sends the complete embedded Normal Browser in
capped 4096-byte chunks and the terminating chunk; the old synchronous
`normalPage` route is no longer installed.

## Verification

The focused isolation contract models stalled page/SSE admission, ordinary
capacity reservation, fd/generation reuse and exactly-once release. Stage 14C
Browser runtime contracts, Stage 15 control/network contracts, persistent
storage, WP3 and WP4 contracts pass. Live hardware HTTP/SSE timing and the
product-profile stalled-client campaign remain physical validation work; no
controller was flashed for this implementation.

