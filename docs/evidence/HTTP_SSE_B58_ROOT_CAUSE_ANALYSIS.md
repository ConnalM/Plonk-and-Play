# b58 HTTP/SSE diagnostic root-cause analysis

Date: 2026-10-27

This analysis covers the exact b58a6a27 ELF (`F518CBA9...57C722`) and capture `firmware/logs/device-monitor-261027-143835.log`.

## 1. `pp_sse_0` panic path

The panic is a stack-canary watchpoint, not a reset caused by the race/persistence system. The useful decoded frames are:

| backtrace frames | decoded function/source | layer |
|---|---|---|
| `0x403767c9`, `0x40376909`, `0x403764b9`, `0x403776c4` | `heap_caps_aligned_alloc_base` → `heap_caps_malloc_base` → `heap_caps_malloc` → `esp_coex_common_malloc_internal_wrapper` | ESP-IDF heap/coexistence |
| `0x420fd982`, `0x421007e9`, `0x400398b5`, `0x420fcf51`, `0x420fd0b5` | `esf_buf_alloc_dynamic` → `esf_buf_alloc` → ROM Wi-Fi routine → `ieee80211_alloc_tx_buf` → `ieee80211_output_do` | ESP-IDF Wi-Fi/ROM |
| `0x420ac751` through `0x4206585b` | `esp_wifi_internal_tx_by_ref` → `wifi_transmit_wrap` → `esp_netif_transmit_wrap` → lwIP `low_level_output`/`ethernet_output`/ARP/IP output | ESP-IDF netif/lwIP |
| `0x42061a09`, `0x4206efc6`, `0x4206fdc8`, `0x4206e98d`, `0x4206e9d9`, `0x4206bfb4` | lwIP `tcp_output` → `lwip_netconn_do_writemore` → `lwip_netconn_do_write` → `netconn_write_vectors_partly` → `netconn_write_partly` → `lwip_send` | lwIP |
| `0x42087d89`, `0x420f8bf5`, `0x4208796d` | `httpd_default_send` → `httpd_send_all$isra$0` → `httpd_resp_send_chunk` | ESP-IDF HTTPD |
| `0x420195c3` | `BrowserInterface::sendEventNotification`, generated `browser_interface.h:1692`, initial-frame `httpd_resp_send_chunk` | P&P |
| `0x42018793` | `BrowserInterface::runSseWorker`, generated `browser_interface.h:1428` | P&P |
| `0x42018389` | `BrowserInterface::sseWorkerTask`, generated `browser_interface.h:1344` | P&P/FreeRTOS |
| `0x4037efd9` | `vPortTaskWrapper` | FreeRTOS |

The runtime sequence is therefore proven to have reached: `/events` admission, `httpd_req_async_handler_begin`, session initialisation, worker notification/wake, initial-frame `snprintf`, and the first `httpd_resp_send_chunk`. It did not return from the send, record a non-idle telemetry line, complete the async request, or execute cleanup. The telemetry ring record was written before the send, but the loop-side drain could not print it before the worker panic.

## 2. The apparent stack contradiction is resolved

The constant is named `SseWorkerStackWords`, but the installed ESP-IDF FreeRTOS headers explicitly define `xTaskCreate()`'s `usStackDepth` as a **number of bytes**, unlike vanilla FreeRTOS. The same headers document `uxTaskGetStackHighWaterMark()` as returning the minimum free stack space in bytes. Consequently:

- `SseWorkerStackWords = 2048` allocated **2,048 bytes**, not 8,192 bytes;
- the diagnostic label `stack_words=2048` was wrong;
- idle HWM `1440` meant 1,440 bytes free, not 5,760 bytes;
- the first HTTPD/lwIP/Wi-Fi send path consumed the remaining stack and tripped the canary.

This is proven by the framework header at `framework-arduinoespressif32-esp32s3-libs/include/freertos/FreeRTOS-Kernel/include/freertos/task.h` (the `usStackDepth` and HWM comments) and `portable/xtensa/include/freertos/portmacro.h` (`StackType_t` is `uint8_t`). No P&P automatic frame or session-buffer overwrite is needed to explain the panic. The session buffers are in `SseSession`, not worker automatic storage. A separate lifecycle risk remains: the HTTPD session callback and worker mutate the same session record concurrently using volatile flags but no ownership lock; this run did not reach a disconnect/replacement race and therefore does not prove that risk causal.

The exact first failing operation is the initial `httpd_resp_send_chunk` while the ESP-IDF/lwIP/Wi-Fi transmit path allocates a TX buffer. An 8,192-byte *intended* stack was never present in this binary.

## 3. `/health`, `/context`, and the page path

The accepted physical run contains no `/health` `HTTP_TRACE enter`: the request was not dispatched to `health()` before the controller reboot. `/context` was entered after the panic reboot and completed its handler send in 38 ms:

```text
[HTTP_TRACE] phase=enter route=/context ...
[HTTP_TRACE] phase=send_begin route=/context ...
[HTTP_TRACE] phase=send_complete route=/context ...
[HTTP_TRACE] phase=exit route=/context ... result=0 elapsed_ms=38
```

The client harness waited for TCP close and timed out after receiving the 367-byte HTTP 200 response. That is consistent with HTTP/1.1 keep-alive: handler completion and response completion do not require immediate connection close. It is not evidence that `/context` remained blocked.

The separate `HTTP ERROR route=/normal duration_ms=10736 result=45062` is the important ordinary-request failure. The generated production header routes both `/` and `/normal` to `normalPage` (`firmware/browser/embed_normal.py:126` and generated `browser_interface.h:2990-2991`). `normalPage` sends the 4096-byte chunks synchronously in the HTTPD task. The bounded async `page()` admission/worker path exists in `browser_interface.h:2379-2428`, but the product routes bypass it. Therefore the page worker and ordinary-request reserve cannot protect `/health`; a stalled root page can monopolise the shared HTTPD task exactly as observed.

This is a concrete implementation defect, not a generic performance observation. The smallest proven correction is to route the product root and `/normal` alias through the bounded async page admission path while preserving the same generated response content and endpoint semantics.

## 4. Architecture verdict

The async request ownership API is intended to produce a request copy for an async thread, so the concept is not disproved by this run. However, the installed ESP-IDF documentation also states that HTTP server APIs are not thread-safe. The current implementation invokes `httpd_resp_send_chunk()` concurrently from up to four independent SSE workers, with no transport-level serialisation. This run proves the worker send path is under-sized; it does not by itself prove a cross-worker HTTPD race. Before physical retest, software proof must use the correct byte-sized stack allocation and demonstrate the supported thread-safety/lifecycle boundary for concurrent sends. A mutex around all sends would reintroduce the very isolation failure being corrected, so it is not an acceptable unreviewed fix.

The `/health` failure and SSE canary share the HTTP/SSE transport-pressure scenario but have distinct immediate causes:

1. `/health` was starved because the current root page route still performs synchronous chunked sends in the HTTPD task.
2. `pp_sse_0` panicked because the nominal 2,048-byte worker stack was too small for the first HTTPD/lwIP/Wi-Fi send path (with the diagnostic units mislabeled).

## 5. Required software proof before another flash

- Change only the diagnostic naming/allocation unit first; allocate a measured byte-sized worker stack and retain raw HWM telemetry.
- Prove generated `/` and `/normal` routes enter bounded page admission and never call synchronous page sends from the HTTPD task.
- Run the deterministic stalled-page + stalled-SSE + healthy-client test with correct response parsing (stop at Content-Length/chunk terminator, not TCP close).
- Prove `/health`, `/context`, `/state`, `/proposal` and a healthy SSE remain responsive while both stalls are held.
- Exercise initial, notification, keepalive, failed-send, disconnect and replacement paths; prove no fd/generation reuse or session leak.
- Resolve the ESP-IDF HTTPD API thread-safety boundary in software before claiming four concurrent SSE workers are safe.

No production source was changed by this analysis.

