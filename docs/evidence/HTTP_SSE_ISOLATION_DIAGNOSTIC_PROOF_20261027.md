# HTTP/SSE isolation diagnostic/proof candidate

Candidate branch: `codex/http-sse-isolation`  
Implementation commit: `56ee14a0fba4b4f9549ee0dc4ee8cd43be4f11f1`  
Baseline approved implementation: `c20a45211fcc46681d438ad44dc3d4636dd41eed`  
Scope: diagnostic correction only; no Browser product, authority, race,
persistence, AP+STA or display semantics are changed.

## Changes under test

* The four SSE workers now start with an **8,192-byte (2,048-word) diagnostic
  stack**. This is a measured-design starting point, not the final product
  allocation. Runtime `[SSE_STACK]` records cover idle, initial frame,
  notification, keepalive, stalled send, send failure, disconnect,
  replacement/fd reuse and shutdown cleanup. `uxTaskGetStackHighWaterMark()`
  reports the remaining words at each boundary.
* The initial 128-byte frame and 192-byte notification frame are owned by each
  bounded `SseSession`, not automatic worker-stack storage. Four sessions add
  1,280 bytes for these two frame buffers; the HWM/cleanup fields add roughly
  80 bytes across the four records, plus alignment. The four worker stacks
  reserve 32,768 bytes, 16,384 bytes
  more than the unsafe 4,096-byte baseline. FreeRTOS TCB/task overhead is
  additional runtime allocation.
* `/health`, `/context`, `/state` and `/proposal` emit bounded
  `[HTTP_TRACE]` enter/send_begin/send_complete/exit records when the
  diagnostic build flag is enabled. Each record carries route, fd, current
  session-context presence, client count, active handler count, result,
  elapsed time and server generation. Page/SSE admission and rejection emit
  `[HTTP_ADMISSION]` with socket, SSE-slot and page-queue counts.
* Admission remains finite: two page workers, a depth-two page queue, four SSE
  sessions, and an ordinary-request reserve of four client sockets. Exhaustion
  returns an explicit bounded failure; it does not consume the ordinary
  reserve.

## Deterministic software proof

`acceptance/stage10/http_sse_isolation_deterministic.py` is a virtual transport
regression, not a claim of live-socket timing. It simultaneously admits one
stalled page, one healthy page, two queued pages, four SSE streams (one
stalled), and four healthy ordinary routes. It proves the bounded admission,
ordinary-request budget, healthy-SSE independence, exactly-once reclamation,
and fd-reuse/generation rejection model. The deterministic budget is 500 ms;
the model timings are `/health` 120 ms, `/context` 90 ms, `/state` 80 ms,
`/proposal` 110 ms and healthy SSE 140 ms.

The existing structural contract was extended to require the session-owned
buffers, the 8,192-byte starting stack, all lifecycle HWM markers, detailed
route tracing and admission tracing. `acceptance/stage15/run_sse_multiclient.ps1`
continues to enforce the production four-client/generation contract.

## Build/resource evidence

Clean PlatformIO build: `stage8bp1s3sd`, Espressif32 8.0.0, Arduino-ESP32
3.3.12 / ESP-IDF 5.5.5, Arduino_GFX 1.6.5, ESP32-S3 N16R8 / 16 MB flash.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `firmware.bin` | 1,478,656 bytes | `C53F29910C8B51FEBA0ECE1615A9ADB2C17934F2AA8163A3EC27D0CCD8D2FDB5` |
| `firmware.elf` | 15,115,900 bytes | `57C353A7D930FDD9EC80B670972BBD23A2CA3EF7021B24315AC35BBAD2770CCA` |

PlatformIO reports 88,492 bytes static RAM (27.0% of 327,680) and 1,450,431
bytes application flash (22.1% of the 6,553,600-byte application region).
The c20a452 implementation reported 87,148 bytes RAM and 1,446,843 bytes
application flash; this diagnostic candidate is therefore +1,344 bytes RAM and
+3,588 bytes application flash. The four SSE task stacks account for the
explicit +16,384-byte runtime reservation; task/TCB and queue allocations are
not represented by the static linker RAM figure.

No runtime HWM values exist yet because this step deliberately does not flash
the controller. The 8,192-byte value is therefore retained as the diagnostic
starting point only. The final product stack must be selected from the lowest
observed HWM across every listed path plus an explicit safety margin, then
rechecked against four simultaneous workers and runtime heap headroom.

## Test status and remaining boundary

PASS: Stage 10 structural/isolation contract; deterministic bounded transport
proof; Stage 15 SSE/multi-client contract; Stage 15 control-lease and network
contracts; Persistent Storage Foundation/WP2 suites; Stage 14C Browser SSE
liveness, notification and recovery runtimes; clean `stage8bp1s3sd` build.

The Stage 14C integration-review invocation was not a valid result because the
requested pre-existing acceptance/demo merged images were absent/stale in this
worktree; the Browser runtime and structural checks passed. This does not
replace the physical NET15.14 test.

The previous `/health` timeout remains unresolved by design. The route tracing
now records the first accepted fd/session, handler entry, send boundary and
exit so the next focused run can distinguish HTTPD/session/socket starvation
from handler-side delay. No additional structural transport correction is
claimed until that evidence exists.

The next reviewable step is Archie inspection of this diagnostic candidate.
Only after review should one normal-candidate physical run collect the SSE HWM
paths and route traces under the stalled-page + stalled-SSE scenario.
