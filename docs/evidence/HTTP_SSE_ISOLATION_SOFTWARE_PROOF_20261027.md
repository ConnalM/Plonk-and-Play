# HTTP/SSE isolation software correction and proof

This evidence covers the migration-stack correction after the c20a452 physical
diagnostic. It is software-only; no controller was flashed by this change.

## Corrections

* ESP-IDF/FreeRTOS on the installed Arduino-ESP32 3.3.12 stack interprets
  `xTaskCreate()` stack depth and `uxTaskGetStackHighWaterMark()` in **bytes**.
  The SSE diagnostic allocation is now explicitly `SseWorkerStackBytes = 8192`
  per worker, and all HWM fields/output use `hwm_bytes`/`stack_bytes` names.
  Session-owned 128-byte and 192-byte frame buffers remain in place. Four
  workers therefore reserve 32,768 bytes of stack storage before TCBs and
  session records; the final production stack remains evidence-led.
* The generated product routes `/` and `/normal` to the existing bounded
  `page()` admission handler. The page worker sends the complete embedded
  Normal Browser in 4096-byte-capped chunks and the terminating chunk. The old
  synchronous `normalPage` route is not installed.
* The deterministic HTTP harness now terminates a response at Content-Length
  or the chunked zero-length terminator and does not wait for a keep-alive TCP
  connection to close.

## ESP-IDF HTTPD thread-safety boundary

The installed framework header is
`C:/Users/conna/.platformio/packages/framework-arduinoespressif32-esp32s3-libs/include/esp_http_server/include/esp_http_server.h`.
It documents `httpd_req_async_handler_begin()` as the supported operation for
obtaining a request copy usable on an asynchronous thread and requires one
matching `httpd_req_async_handler_complete()` on every terminal path. The
response API documentation warns that general HTTPD APIs are not thread-safe.

The implementation therefore gives each request copy to one exclusive owner:
one page worker or one SSE worker, never two workers for one request/fd. Before
send and release, the worker checks server generation, fd, request generation
and registered session identity. Session replacement, disconnect and shutdown
converge on one completion path. No global send mutex is used, because that
would make one stalled client block every other client.

This is the smallest supported ownership boundary exposed by the framework;
arbitrary concurrent use of one request/session remains forbidden. The
physical validation must still exercise distinct client sessions under load;
the software proof does not claim that the generic non-thread-safe API warning
has been removed by the framework.

## Software proof

The following passed after the correction:

* Stage 10 evaluator 10.1–10.17 and 10.T.
* bounded HTTP/SSE contract and deterministic isolation model, including
  stalled page/SSE admission, ordinary response budget, fd reuse and exactly
  once release.
* Stage 14C notification, liveness, presentation, recovery, Results, proposal
  and setup synchronisation runtime tests; Stage 14C structural review.
* Stage 15 Network contract, control-lease contract, WP3 Browser diagnostics,
  WP3 diagnostic export and WP4A diagnostics durability contracts.
* Persistent Storage Foundation, WP2 and Track Record suites.
* clean `stage8bp1s3sd` build.

The live-socket Stage 15 multi-client scripts require a controller endpoint and
were not run against a device in this software-only pass. Physical validation
is still required for stalled page + stalled SSE isolation, healthy ordinary
routes, healthy second SSE, reclamation and no panic.

