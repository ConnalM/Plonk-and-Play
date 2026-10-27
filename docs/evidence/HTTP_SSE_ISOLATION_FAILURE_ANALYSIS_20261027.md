# HTTP/SSE isolation failure analysis

Candidate `c20a45211fcc46681d438ad44dc3d4636dd41eed` was physically tested on
2026-10-27. The source ELF is `9DECE6BCB7C8EB7F46A216109C9EF11F29D72DBEAF47E6EBF0B53A7C83ADA059` and the capture is `firmware/logs/device-monitor-261027-100733.log`.

## Proven physical facts

The controller booted normally, mounted SD, brought up AP/STA/mDNS/HTTP, and
remained healthy until the transport test. One root-page client and one SSE
client were held open without reading. A separate client observed `/health`
timeout at 10.034 s, `/context` 200 in 261 ms, `/state` 200 in 80 ms,
`/proposal` 200 in 87 ms, and a second SSE connection closed at about 4.988 s.
After the stalled clients were released, `/health` returned 200 in 200 ms and
`/context` in 94 ms. No race, authority mutation, or persistence operation was
performed.

Serial then recorded a `/normal` send error and two reboots from the same SSE
worker name:

```
[DEV] HTTP ERROR route=/normal duration_ms=5549 result=45062 ...
Debug exception reason: Stack canary watchpoint triggered (pp_sse_0)
...
Backtrace: 0x420e5b0a ... 0x420188cd ... 0x42017bc9 ... 0x420178b1 ... 0x4037efd9
...
Debug exception reason: Stack canary watchpoint triggered (pp_sse_0)
...
Backtrace: 0x420e5ae7 ... 0x420868b8 ... 0x420188fb ... 0x42017bc9 ... 0x420178b1 ... 0x4037efd9
```

`45062` is `0xB006`, `ESP_ERR_HTTPD_RESP_SEND`.

## A — SSE stack exhaustion

The c20a452 ELF symbol/line mapping is:

| PC | Symbol/source | Meaning |
|---|---|---|
| `0x420e5b0a` | `snprintf.c:80` | first panic while formatting the initial SSE frame |
| `0x420188cd` | `BrowserInterface::sendEventNotification`, `browser_interface.h:1513` | `snprintf(initial, ...)` |
| `0x42017bc9` | `BrowserInterface::runSseWorker`, line 1275 | calls `sendEventNotification` |
| `0x420178b1` | `BrowserInterface::sseWorkerTask`, line 1204 | task notification dispatch |
| `0x4037efd9` | `vPortTaskWrapper` | FreeRTOS task wrapper |
| `0x420e5ae7` | `snprintf.c:84` | second panic during the same initial-frame path |
| `0x420868b8` | IDF `httpd_resp_send_chunk`, `httpd_txrx.c:363` | initial-frame response send |
| `0x420188fb` | `sendEventNotification`, `browser_interface.h:1517` | `httpd_resp_send_chunk(initial)` |

Thus both panics are in `pp_sse_0`, not the HTTPD task. The second trace is
explicitly the initial `httpd_resp_send_chunk` path; this is before normal
keepalive/notification payloads. The call chain is

`sseWorkerTask -> runSseWorker -> sendEventNotification -> snprintf and/or
httpd_resp_send_chunk -> IDF HTTP response/send stack`.

The only material automatic SSE locals are `char initial[128]` and
`char payload[192]`. The `snprintf` implementation and IDF response path add
their own frames; the worker also carries FreeRTOS notification/task frames.
Normal c20a452 does not compile the Stage-14C Serial trace branches, so those
diagnostic `Serial.printf` calls are not the cause here. No race/authority or
storage lock is held by this path.

`SseWorkerStackWords` is `1024`, i.e. 4096 bytes on this target. The value was
introduced as a provisional constant in the original isolation commit; there
is no high-water measurement or derivation in the source. The only existing
stack high-water report is for the loop task, not these workers. Consequently
the exact peak for idle, initial, notification, keepalive, stalled-send and
disconnect cleanup is not yet measured. The canary proves 4096 is insufficient
for at least the initial-send path.

The first justified next diagnostic is a no-behaviour-change worker stack
telemetry build using `uxTaskGetStackHighWaterMark()` at idle, before/after
initial frame, notification, keepalive, send failure and disconnect/replacement
cleanup. The frame buffers should also be moved from worker stack into
session-owned storage in the eventual correction, so the stack budget is not
spent on `initial[128]`/`payload[192]`. The page worker already uses an 8192-byte
stack and successfully reached the same IDF chunk-send family, so 8192 bytes is
an evidence-based first candidate, not a final unmeasured answer. The final
size should be measured peak plus a documented margin, with the four-worker
RAM cost and the buffer storage included in the resource report.

## B — `/health` isolation

`/health` is still a synchronous ordinary HTTPD handler (`browser_interface.h`
lines 2743–2753). It formats a fixed `char body[512]` and calls one
`httpd_resp_sendstr`; it does not read SD, export diagnostics, allocate a large
buffer, or take a race-critical/product-authority lock. `/context` is likewise
a small synchronous `char json[320]` response; `/state` snapshots and serializes
a heap buffer before one send; `/proposal` serializes a heap buffer before one
send. None of these ordinary routes is moved to the page-worker queue.

The capture has no `/health` enter/exit/error marker, so the 10.034-second
timeout does not prove a `/health` handler wait or send duration. It is close to
the test client's 10-second timeout, while the configured IDF receive/send
waits are each 5 seconds. The source path contains no two 5-second waits in
`health()` itself. The fact that `/context`, `/state` and `/proposal` completed
while the same stalled clients were held proves that the HTTPD task was not
globally dead; the later `/health` success after release proves the endpoint
itself was still functional.

The most likely remaining boundary is HTTPD/session/socket scheduling or
admission pressure around the stalled async sessions, not a health-specific
diagnostic lock. The implementation's `httpd_get_client_list()` checks reject
new page/SSE admission once a count threshold is reached, but this is not a
reserved ordinary socket pool: ordinary routes still share the one HTTPD task
and global socket/session machinery (`max_open_sockets=13`, three framework
internal sockets, `lru_purge_enable=true`, 5-second receive/send waits).
Therefore the logical reserve exists for page/SSE admission, but the physical
ordinary-request isolation guarantee is not yet proven and can still fail at
the HTTPD/session boundary. A route-entry/exit diagnostic or a deterministic
ESP-IDF harness is required before assigning the timeout to a particular
socket, server-task or client-side path.

## C — healthy SSE close

There is no evidence of a fourth-session admission rejection or a 503. The
healthy SSE was admitted, then the controller hit a `pp_sse_0` canary panic;
any live SSE socket is necessarily closed by the resulting reboot. This makes
the stack panic the leading direct cause of the observed healthy-stream close.
The capture does not timestamp the client close against the panic finely enough
to exclude an earlier send/admission failure, so a separate pre-panic close is
not proven. `httpd_sess_trigger_close()` failure is logged only when it returns
an error; no such marker establishes a distinct cleanup failure in this run.

## D — architecture conformance

The core worker split is real: page jobs execute in two page tasks and each
SSE session has its own task; no `httpd_queue_work()` or raw socket send is used
for transport. Generation/fd/session checks and exactly-once completion are
present. This explains why three ordinary endpoints completed while a page/SSE
client was stalled.

However the implementation is not yet conformant: the SSE stack budget is
unsafe, and ordinary capacity is a logical admission check rather than a
physically isolated ordinary-request service path. `/health` remains on the
shared synchronous HTTPD path, so a hidden server/session operation can still
affect it. The design boundary is therefore sound but incomplete in sizing and
ordinary-request coverage; no authority, race or persistence semantics need to
change.

## Required software evidence before another flash

1. Run the worker high-water diagnostic across idle, initial, notification,
   keepalive, stalled-send, failure, disconnect and fd-replacement paths; prove
   no canary violation and report peak per worker.
2. Add a deterministic route-entry/exit/fd/result trace for `/health` and the
   other ordinary routes, then reproduce the stalled page + stalled SSE case.
   This must distinguish handler-not-entered, send-blocked, socket-admission,
   HTTPD-task and client-timeout cases.
3. The focused contract must hold with both stalled clients: `/health`,
   `/context`, `/state`, `/proposal` and a healthy second SSE complete within a
   bounded budget; stalled work completes/fails explicitly; sessions and fd
   generations are reclaimed; no panic, socket leak, authority/race mutation or
   persistence mutation occurs.
4. Include a test that exercises the same initial SSE frame and IDF response
   send on the selected stack budget, plus replacement/fd reuse and close
   failure. Do not credit a larger stack alone as the complete correction.

No production source or configuration change is made by this analysis.
