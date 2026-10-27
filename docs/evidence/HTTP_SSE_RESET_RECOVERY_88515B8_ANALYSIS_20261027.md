# 88515b8 RESET/EN recovery failure — HTTPD population analysis

Date: 2026-10-27  
Implementation: `88515b88f4f972da52d3b6a89888483189ff1b3f`  
Environment: `stage8bp1s3sd`  
Artifact SHA-256: `597BD42EE40A90B77BE18449C9811A0BDED64039ACDB33C08E8C57B7794B9E23`  
Capture: [`firmware/logs/device-monitor-88515b8-reset-recovery.log`](../../firmware/logs/device-monitor-88515b8-reset-recovery.log)

This is a read-only analysis. No firmware, browser, authority, race, persistence,
socket limit or reserve was changed.

## Result

The narrowest supported cause is **ordinary HTTPD client-session pressure during
simultaneous two-Browser recovery**. The corrected admission boundary is working
as designed. A recovered SSE (generation 5) failed, and its replacements arrived
while completed `/state`, `/proposal` and `/context` sessions were still resident
in the HTTPD client list. With `max_open_sockets = 13` and an ordinary reserve of
4, `clients >= 9` must reject a new SSE. The trace proves that consequence; it
does not identify the initiating socket error for generation 5 and does not prove
a persistent P&P SSE/session leak.

## 1. Reconstructed lifecycle

The capture includes the deliberate RESET/EN boot marker and remains attached
through the post-boot sequence:

| Ordered phase | Trace evidence | Interpretation |
|---|---|---|
| Boot | ESP-ROM marker, build `88515b8`, SD/AP/STA/mDNS/HTTP ready | Genuine controller reboot; no reset loop. |
| First Browser recovery | `/events` fd 51, clients 5, admitted, generation 1; then `/proposal` fd 53, `/state` fd 55, `/context` fd 56; second `/events` fd 57, clients 7, admitted, generation 2 | Both pages reacquired ordinary state and two SSE slots. `/context` identifies client 1 as spectator. |
| Replacement chain | `/events` fd 52, clients 8, admitted, generation 3; generation 1 cleanup=`replacement`; later fd 58 admitted as generation 4 and generation 2 is replaced; fd 57 admitted as generation 5 and generation 3 is replaced | Server-side token replacement and fd/generation cleanup are active. The trace cannot map each replacement to a physical page. |
| Fresh acquisition | client 2 performs `/state` fd 59 at clients 9, `/proposal` fd 61 at 11, `/context` fd 62 at 12; `/context` reports client 2 Master | Per-page recovery acquisition is sequential, but the two pages' sequences are interleaved. |
| First complete failure boundary | generation 5 fd 57 reports `send_failure`, `cleanup=send_failure` | One recovered SSE is removed; no errno, peer-close or stale-fd marker is emitted. |
| Rejection pressure | `/events` fd 54 rejected at clients 12; fd 56 rejected at 11; fd 54 rejected at 10; all `ordinary_capacity_exhausted`, result 257 | The unchanged reserve guard correctly blocks recovery while the client population is 9 or more. |
| Eventual admission | `/events` fd 55 admitted at clients 7, generation 6; `/state` fd 54, `/proposal` fd 56, `/context` fd 57 complete; client 1 is reconstructed spectator with `owner_connected=false` | Capacity later falls below the boundary and one replacement succeeds. The run stopped before a second settled healthy SSE and fresh tablet-Master context were observed. |

There is no panic, additional reboot, race mutation, persistence mutation or
authority-transfer request in this run.

## 2. What is known about clients 12, 11 and 10

The trace reports a population from `httpd_get_client_list()` and route/fd
activity, not a complete fd-to-Browser/session map. The following is therefore
the maximum justified accounting:

| Population | Proven occupants/events | Not provable from this capture |
|---:|---|---|
| 12 | At least one resident SSE (generation 4 or 5 is still reporting keepalive); the client-2 `/context` request is entered, sends and exits at 12. Other ordinary sessions are resident or in transition. | Which exact fds are the other SSE/ordinary sessions; whether each is a Browser connection, a pooled keep-alive, or a peer awaiting reap. |
| 11 | A `/events` replacement request is entered and rejected while one SSE slot remains. The immediately preceding `/proposal` completed at 11. | Exact identities and whether the population includes the just-completed proposal fd or other idle keep-alives. |
| 10 | A `/state` request completes at 10, then another `/events` request is rejected. One SSE slot remains until generation 5 send failure. | Exact identities of the remaining ordinary sessions and whether any are closing but still counted. |

The successful `/state`, `/proposal` and `/context` handlers all log
`send_complete` and `exit`; they do not log a close. The capture cannot prove a
per-route socket ownership map, so no per-fd identity is invented. Rejected
`/events` handlers have `session_ctx=0`, allocate no `SseSession` or async copy,
return the terminal error and are subsequently consistent with fd reuse/count
falling. They are not shown to be a permanent P&P leak.

## 3. Effect of sequential recovery acquisition

The current generated Browser code (`firmware/browser/embed_normal.py`) uses:

```text
state -> proposal -> context
```

for each `reconnect`/`recovery` cycle. `polling` prevents overlap within one
Browser. This is a real reduction from the former per-Browser `Promise.all`
fan-out: a single page has at most one recovery acquisition request in flight.

It is not a global serialiser. Laptop and tablet recover concurrently, so their
sequences interleave. More importantly, completion does not immediately remove
the HTTPD client session, so sequential request *execution* does not imply a
small sequential client *population*. The trace therefore confirms the intended
per-Browser change while also showing why it is insufficient by itself.

## 4. Why ordinary sessions remain resident

`G(path)` in the generated Browser performs `fetch(path, {cache:'no-store',
signal})`; it does not request `Connection: close`. Browser fetch uses HTTP/1.1
connection reuse/pooling where available. The P&P HTTPD configuration has
`recv_wait_timeout=5`, `send_wait_timeout=5`, `lru_purge_enable=true` and
`max_open_sockets=13`. Ordinary handlers complete quickly, but HTTPD's client
list counts connected sessions, not only handlers currently executing. An idle
keep-alive session therefore remains counted until the peer closes it, the idle
timeout/reaper/LRU path removes it, or the fd is reused.

The trace directly demonstrates this distinction: after each ordinary handler
logs `send_complete` and `exit`, the population is still 9, 10, 11 or 12 and
continues to affect the next `/events` admission. Thus the reserve model is not
wrong; its implicit assumption that a completed acquisition immediately frees a
slot is wrong for persistent HTTP/1.1 sessions. The exact contribution of
Chrome connection pooling versus HTTPD idle/reap timing cannot be separated from
this serial capture alone.

The rejection path itself is explicit and bounded in this build: it sets 503 and
`Connection: close`, allocates no SSE session/async request and returns one
terminal close result. Rejected attempts are consequently not the main resident
population in this run.

## 5. Generation 5 send failure

Generation 5 is `fd=57`; it reports keepalives immediately before the failure,
then `stage=send_failure`, `hwm_bytes=5992`, `cleanup=send_failure`. There is no
socket errno, peer-close reason, HTTPD session-close callback, stale-generation
marker, session-mismatch marker or browser-console trace in the evidence.
The following are therefore proven: the worker reached its send-failure branch,
the session was cleaned up, and the remaining SSE slot count allowed later
admission. The initiating transport cause (peer closure, a transient socket
failure, or a replacement race) is **not knowable** from this capture.

## 6. Comparison with the 8090794 failure

The prior valid 8090794 failure had the same immediate mechanism: after two-page
reboot recovery, one SSE send failed; replacements arrived at clients 10 and 9
and were correctly rejected while the other SSE stayed healthy. The 88515b8
correction improved the per-Browser acquisition from concurrent to sequential and
still preserved fresh state/proposal/context and authority reconstruction. It did
not change the shared HTTP/1.1 keep-alive lifecycle or globally serialise the two
Browsers, so populations still reached 12 and the common reserve-pressure
mechanism remained.

The two failures support one common mechanism: a transient recovered-SSE failure
plus ordinary persistent HTTP sessions occupying the reserve during simultaneous
two-Browser recovery. They do not prove the same initiating send errno or a P&P
SSE leak.

## 7. Admission and conformance

The intended arithmetic remains:

```text
max_open_sockets = 13
ordinary reserve = 4
SSE admission: clients < 13 - 4, i.e. clients < 9
```

The 9+ boundary must not be weakened. The run is consistent with all current
architecture invariants:

- one EventSource/retry owner per Browser and bounded 2/4/8/16-second recovery;
- fresh controller-derived state/proposal/context before controls are restored;
- no second authority source or authority transfer caused by transport pressure;
- fd/generation/session checks and exactly-once SSE cleanup remain in force;
- no global transport lock, race change or persistence change;
- no normal-operation polling redesign.

The remaining issue is lifecycle management of ordinary recovery HTTP sessions,
not an argument for more sockets, fewer reserve slots, or an SSE rewrite.

## 8. Smallest evidence-supported correction

Before changing code, the narrowest correction to validate is a server-side
connection-lifecycle policy for the three recovery acquisition responses:
ensure that the completed recovery `/state`, `/proposal` and `/context` request
sessions are explicitly closed after their framed response, so each acquisition
cannot remain an idle HTTPD client while the next recovery step or SSE replacement
is admitted. Browser JavaScript cannot reliably set the forbidden `Connection`
request header itself; the policy must therefore be implemented at the server
response boundary (using an explicit recovery request marker if needed), without
changing authority data or the normal healthy SSE path.

Do not delete/close an arbitrary client fd from the controller, increase
`max_open_sockets`, reduce the reserve, add polling, or alter retry/authority
semantics. If a recovery marker is not already available, the deterministic test
should first establish whether applying `Connection: close` to these small
read-only acquisition responses globally is product-compatible; prefer the
narrowest route/marker scope.

## 9. Required deterministic proof before implementation/physical testing

Add a reboot-shaped two-Browser software regression that records client counts
after each framed recovery response and exercises:

1. two identities (Master and spectator), two admitted SSE sessions;
2. simultaneous generation loss and recovery;
3. one recovered SSE send failure;
4. per-Browser `state -> proposal -> context`, one request in flight per page;
5. populations through 8, 9 and 10, with unchanged 9+ 503/close rejection;
6. no `SseSession`/async allocation on rejection and fd/generation-safe reuse;
7. prompt release of recovery HTTP sessions before the next replacement attempt;
8. `/health`, `/context`, `/state`, `/proposal` and the healthy SSE responsive;
9. automatic later admission and fresh acquisition for the failed Browser;
10. final Master/spectator roles and baseline client/session counts;
11. no race, authority or persistence mutation.

This report deliberately stops before implementation. It does not claim that
the exact generation-5 send error is proven; it identifies the first proven
recovery blocker and the smallest lifecycle correction that can be tested
without weakening the accepted isolation guarantees.
