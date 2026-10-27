# 8090794 two-Browser reboot recovery: read-only root-cause analysis

Date: 2026-10-27  
Implementation under analysis: `8090794f134713e5c7e21943f1dbc329491680a5`  
Physical capture: [`firmware/logs/device-monitor-8090794-reboot-recovery.log`](../../firmware/logs/device-monitor-8090794-reboot-recovery.log)  
Related successful single-Browser recovery: [`http-sse-8090794-tablet-wifi-restore-20261027.md`](http-sse-8090794-tablet-wifi-restore-20261027.md)

This is an evidence-only report. No firmware, limits, retry timing, authority behaviour or test state was changed.

## Executive finding

The corrected admission boundary is working as designed. After reboot, one recovered SSE stream later failed its send; its replacement attempts were rejected because the HTTPD client population was already at 10 and then 9, which correctly violates the four-client ordinary-request reserve (`13 - 4 = 9`). The other SSE remained healthy.

The failure is best classified as **D — a combination of an SSE/recovery lifecycle boundary and ordinary HTTP connection pressure**, with the initiating cause of the send failure itself not proven by this capture. The capture does not prove a P&P SSE-session leak or stale-fd delivery. It does prove that simultaneous two-page recovery creates a larger resident ordinary-HTTP population than the successful one-page restoration, and that the failed stream cannot be readmitted while those sessions remain resident.

## Reconstructed lifecycle

### Boot and first recovery

The deliberate power cycle was a normal boot (`rst:0x1 (POWERON)`) with SD mount OK, AP/mDNS/HTTP ready, STA `GOT_IP`, source commit `8090794`, and no panic/reset.

The first post-boot admissions were:

```
/events fd=51 clients=1  admitted; generation=1; initial
/events fd=52 clients=2  admitted; generation=2; initial
```

Fresh ordinary acquisition requests then completed successfully. The first group used `/state` fd=55, `/proposal` fd=54 and `/context` fd=55 at population 5. The next group used `/state` fd=57, `/context` fd=53 and `/proposal` fd=55 at population 7. The contexts reconstructed the laptop as spectator (`client_tag=1`, `is_master=false`) and the tablet as Master (`client_tag=2`, `is_master=true`).

### Replacement sequence

Before the final failure, the server observed two successive admissions at population 8:

```
/events fd=58 clients=8 admitted; generation=3; generation=1 cleanup=replacement
/events fd=51 clients=8 admitted; generation=4; generation=3 cleanup=replacement
```

This is a server-observable replacement chain: each new request caused `events()` to find a prior matching token session and invalidate it before admitting the replacement. Because the serial trace does not emit the private browser token alongside the generation, it is consistent with one Browser's late/duplicate recovery activity but cannot map the chain to “laptop” or “tablet” with certainty.

The source does protect the server side against stale work: `events()` calls `invalidateSseSession(previous, true, false, Replacement)`; worker sends require the exact session, fd context, server generation and event generation; completion detaches the fd-bound context before releasing the async request (`firmware/include/pp/browser_interface.h`, `events()`, `invalidateSseSession()`, `sendEventNotification()`, `completeSseRequest()`). No stale-work or session-mismatch marker was emitted in this run.

### First complete failure boundary

The first terminal failure was:

```
[SSE_STACK] generation=4 fd=51 stage=send_failure hwm_bytes=5980 stack_bytes=8192 cleanup=send_failure
```

The other active stream, generation 2 on fd 52, continued keepalives. The failed stream was removed/closed by the existing send-failure path; the next admissions were:

```
/events fd=59 clients=10 sse_slots=1 ordinary_capacity_exhausted result=257
/events fd=61 clients=9  sse_slots=1 ordinary_capacity_exhausted result=257
```

Both were terminal explicit-rejection responses. There was no `close_failed`, panic, reset, stale-work, or session-mismatch evidence. The capture stopped before the failed Browser could recover.

## Client-population accounting

The exact identity of every HTTPD fd is not available: `HTTP_TRACE` reports route/fd/population, while only `/context` reports a privacy-safe Browser tag. Therefore the following distinction is important:

**Proven:** population reached 12 during fresh `/context`/`/proposal` acquisition, then was 10 and 9 when the failed SSE retried. The surviving SSE accounts for one SSE slot; the remaining population is ordinary HTTPD client sessions and/or in-flight ordinary request sessions. Rejection at 9+ is exactly the configured reserve rule.

**Source-supported mechanism:** the generated Browser recovery `poll()` performs `/state`, `/proposal` and `/context` concurrently for a non-active state (`Promise.all`) in the recovery path. The source is `firmware/browser/normal_browser.html` (the generated transformation is applied by `firmware/browser/embed_normal.py`, lines 96–106 and 116–142). Each fetch is a separate HTTP request, and the server does not set `Connection: close` on these ordinary responses; HTTP/1.1 keep-alive therefore leaves completed sessions resident until the peer/HTTPD lifecycle releases them. The server admission count is the HTTPD client-session population, not the number of currently executing handlers.

**Not proven:** the capture cannot assign each of clients 9/10/12 to a particular Browser, nor distinguish a normal keep-alive session from an aborted/replaced page request solely from these lines. It also does not prove a persistent P&P SSE-session leak: `sse_slots=1` after the failure is consistent with the failed session having been removed while the other SSE remained.

## Why the reserve blocked recovery

At 12, ordinary acquisition traffic from both pages was resident while both SSE streams were present. When generation 4 failed, its replacement arrived at 10 and then 9. With `HttpdMaxOpenSockets=13` and `OrdinaryRequestReserve=4`, the admission condition is `clients >= 9`, so both rejections are correct and intentional. Weakening that check would consume the ordinary reserve and violate the accepted isolation contract; this is not an admission-accounting defect.

The failed send itself is not causally identified. The trace shows the send-failure boundary, but no socket error, fd-reuse marker, session-mismatch marker, or Browser-side console trace. It could have been a transient post-reboot transport failure. What is proven is the deterministic consequence: the recovery attempt was made while ordinary sessions remained resident, so the reserve correctly prevented a new SSE.

## Comparison with successful tablet Wi-Fi restoration

The accepted `ff6efef` run had one recovering Browser and one continuously healthy SSE. One replacement was admitted at population 8 (`generation=5` to `generation=6`), followed by fresh `/state`, `/proposal`, `/context`; the topology settled without progressive growth. The reboot run had two pages recovering concurrently, a replacement chain (`generation 1 -> 3 -> 4`) before the failure, and concurrent acquisition from both pages; population reached 12. Thus the material difference is simultaneous two-page recovery and its ordinary-session fan-out, not a changed authority result or a weakened admission boundary.

## Browser/server recovery assessment

The Browser source has one JavaScript `eventStream` owner per page, retires the object on failure, and gates retries through the bounded 2/4/8/16-second schedule (`embed_normal.py` lines 113–142). `polling` prevents overlapping acquisition within one page, and reconnect acquisition is required before synchronisation is restored. This is why no evidence supports an uncontrolled 250 ms polling storm or a second authority source.

However, one page-level EventSource variable cannot cancel a network request that is already in the browser's native connection/retry machinery at the instant a controller disappears. The two successive same-token replacements at fd 58 and fd 51 are consistent with that boundary, but the capture cannot prove whether the second request was a late native retry or another browser lifecycle race. Server-side replacement is deterministic and generation/fd protected, but it does not make the HTTPD ordinary-session population disappear immediately.

## Classification

- **A (SSE lifecycle defect):** partially implicated by the unexplained send failure/replacement chain, but the initiating send error is not proven.
- **B (ordinary HTTP pressure):** proven as the immediate recovery blocker; completed ordinary HTTP sessions remain counted while recovery is underway.
- **C (Browser recovery coordination):** implicated by simultaneous per-page recovery fan-out and the observable replacement chain.
- **D (combination):** the best current classification: a transient/uncertain SSE failure plus ordinary-session pressure during simultaneous recovery.
- **E (insufficient evidence):** applies only to the exact trigger of generation 4's send failure and the ownership of each individual ordinary fd.

## Smallest justified correction proposal

Do not change `max_open_sockets`, `OrdinaryRequestReserve`, the four-SSE limit, explicit 503/close rejection, authority, or race/persistence semantics.

The smallest correction justified by the evidence is to make **recovery acquisition resource-bounded**, rather than launching the three ordinary `/state`, `/proposal`, `/context` fetches concurrently for each recovering page. The recovery cycle should issue at most one ordinary acquisition request at a time and must not begin the next cycle until the current stream/acquisition generation has completed or failed. This preserves the existing fresh-authority requirement while preventing simultaneous two-page recovery from creating a six-request ordinary-session burst at the same boundary.

Separately, the existing per-page EventSource recovery epoch should be treated as the only owner of a generation: a late/native request from a retired stream must be ignored or deterministically closed before a replacement is admitted. The current server already provides the required token replacement, fd/session identity and exactly-once async cleanup; no evidence justifies weakening those guarantees or changing the reserve arithmetic.

This proposal is intentionally bounded: it does not claim that serialization alone explains the original send failure, and it does not turn normal healthy operation into polling. A software regression must first prove the two-browser reboot sequence with concurrent recovery pressure, per-page one-stream ownership, no progressive client growth, and the unchanged 9+ rejection boundary before any physical retest.

## Required proof before another physical test

The deterministic regression should add the reboot-shaped case to the existing transport tests:

1. two Browser identities, one Master and one spectator;
2. simultaneous controller-generation loss/recovery;
3. at most one EventSource/recovery epoch per Browser;
4. bounded sequential recovery acquisition and no duplicate in-flight cycle;
5. a transient send failure on one stream;
6. ordinary client population recorded through 8, 9 and 10;
7. 503/`Connection: close` at the unchanged reserve boundary;
8. the healthy SSE and `/health`, `/context`, `/state`, `/proposal` remain responsive;
9. stale fd/generation work cannot deliver to a replacement;
10. resources return to baseline and both Browser identities reacquire fresh state/proposal/context with Master/spectator roles unchanged;
11. no race, authority or persistence mutation.

