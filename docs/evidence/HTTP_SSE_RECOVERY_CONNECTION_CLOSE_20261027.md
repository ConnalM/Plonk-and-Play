# Browser recovery connection-lifecycle correction

Implementation: `230f1ba` (`fix: close Browser recovery acquisition responses`)  
Date: 2026-10-27

## Correction

The production `/state`, `/proposal` and `/context` handlers now set the
response header:

```text
Connection: close
```

The scope is intentionally global for these three small, read-only
authoritative acquisition responses. The Browser cannot reliably set the
forbidden HTTP `Connection` request header from `fetch()`, and the server has
no extra recovery marker. Applying the response policy at these routes is the
narrowest protocol change that guarantees completed recovery acquisitions do
not remain idle HTTP/1.1 clients. It does not alter JSON, framing, authority,
SSE, page, race or persistence semantics.

`max_open_sockets=13`, `OrdinaryRequestReserve=4`, the `clients < 9` SSE
boundary, four-SSE limit, one EventSource/retry owner and 2/4/8/16-second
recovery schedule are unchanged.

## Deterministic proof

`acceptance/stage14c/browser_reboot_recovery_runtime.js` now models the framed
response lifecycle explicitly:

1. two Browser identities (Master and spectator) recover concurrently;
2. each Browser performs only `state -> proposal -> context` and never has more
   than one acquisition in flight;
3. every ordinary acquisition session is present while its response is active,
   then emits `ordinary-close` and is removed before the next step;
4. the client population returns to the six-ordinary/two-SSE baseline after
   both acquisitions;
5. added ordinary pressure drives populations through 9 and 10;
6. one recovered SSE send failure is followed by unchanged 503/close rejection
   at 9 and 10;
7. releasing pressure admits the replacement, which completes fresh sequential
   acquisition;
8. both SSE streams, Browser roles and the baseline population are restored;
9. no race or persistence event is permitted by the model.

The Stage 10 structural contract separately proves that all three production
route bodies set `Connection: close`, while the existing 503/close SSE
rejection, fd/generation protection and ordinary reserve checks remain present.

## Checks

Passed:

- `python acceptance/stage10/http_sse_isolation_contract.py`
- `python acceptance/stage10/http_sse_isolation_deterministic.py`
- `python acceptance/stage10/structural_review.py`
- all Stage 14C JavaScript runtime checks, including the reboot-shaped recovery
  regression and liveness/admission suites;
- Stage 15 network contract, multiclient SSE contract and control-lease suite;
- WP3 Browser/observation/sequence/export/production suites;
- WP4A diagnostics durability contract;
- Persistent Storage Foundation/WP2/WP5 suite.

The clean `stage8bp1s3sd` compile also passed:

```text
RAM:   88,884 bytes / 327,680 (27.1%)
Flash: 1,437,619 bytes / 6,553,600 (21.9%)
platform: espressif32 8.0.0
Arduino-ESP32: 3.3.12
ESP-IDF libraries: 3.3.12 (IDF 5.5.5)
```

Only pre-existing compiler warnings were emitted. No physical artifact was
preserved, flashed or built for acceptance in this step.

## Architectural conformance

The change adds no polling, transport mutex, authority source, duplicate
EventSource owner, retry state or race/persistence path. It only causes the
three completed acquisition responses to close their HTTP session, which is
the lifecycle correction required by the 88515b8 failure analysis.
