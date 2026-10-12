# Stage 15 Logging and Diagnostics Foundation Acceptance

**Status:** Foundation acceptance contract ACCEPTED/FROZEN for implementation. Final Stage 15 acceptance/freeze remains outstanding.

No numeric capacity, queue, file-size, or performance threshold is frozen by this document before the resource-measurement campaign.

## Foundation contract checks

| ID | Requirement | Evidence |
|---|---|---|
| PS15.1 | Lifecycle uses only `DISABLED/STARTING/ACTIVE/STOPPING/COMPLETED/INTERRUPTED`. | Contract/unit check. |
| PS15.2 | Storage health is separate and may be `DURABLE_SD/RAM_ONLY/DEGRADED/FAULT` while lifecycle is `ACTIVE`. | Contract/unit check. |
| PS15.3 | One active Development session exists per controller. A second start returns `ALREADY_ACTIVE` with existing identity/status. | Recorder contract test. |
| PS15.4 | Multiple Browser clients contribute to one session with distinct attribution. | Multi-client contract test. |
| PS15.5 | Session start succeeds without durable storage and reports volatile/incomplete RAM-only or degraded operation. | Failure-injection/backend test. |
| PS15.6 | Structured versioned records contain epoch, session, client, sequence, timestamp, event kind, severity, correlation, and payload metadata. | Compile/serialization contract test. |
| PS15.7 | Browser observations remain non-authoritative. | Structural review and integration test. |
| PS15.8 | End-to-end correlation can connect Browser action, P&P decision, authoritative change, notification, collection, and render observation. | Instrumented development trace. |
| PS15.9 | Recorder and backend operations are bounded/non-blocking and cannot affect product timing or authority. | Structural and failure-injection test. |
| PS15.10 | Duplicate, rejected, and already-active requests are explicit and do not create replacement or merged sessions. | Recorder contract test. |
| PS15.11 | Reload/reconnect reconstructs current diagnostic status from P&P authority. | Browser/API test. |
| PS15.12 | Controller restart creates a new epoch; durable recovery and RAM-only interruption/loss are reported correctly. | Backend/restart test. |
| PS15.13 | Missing, full, corrupt, unsupported, write-failed, and commit-failed storage produces explicit health/fault results without retry storms. | Failure-injection test. |
| PS15.14 | Diagnostic overload is bounded and observable; dropped/incomplete evidence is marked. | Resource campaign and overload test. |
| PS15.15 | Host/file, Wokwi-compatible, fake/failure-injection, and eventual SD/FATFS backends satisfy the same storage contract. | Backend contract suite. |
| PS15.16 | Existing Serial, health, Message Bus, Noticeboard/SSE, persistence, Browser, and resource facilities can be represented and correlated. | Integration review. |
| PS15.17 | Race behavior, timing, classification, persistence authority, and Browser product authority remain unchanged. | Stage 1–14C regression suite. |

## Phase 2 focused checks

| ID | Requirement | Evidence |
|---|---|---|
| PS15.18 | In-memory session follows normal start, active, stop, and completed transitions. | Phase 2 unit test. |
| PS15.19 | Active-session START returns `ALREADY_ACTIVE` with the existing session identity and does not replace it. | Phase 2 unit test. |
| PS15.20 | Multiple client identities are preserved in one session's records. | Phase 2 unit test. |
| PS15.21 | In-memory operation reports `ACTIVE + RAM_ONLY` and explicit volatility. | Phase 2 unit test. |
| PS15.22 | Records append in deterministic sequence order and can be retrieved in that order. | Phase 2 unit test. |
| PS15.23 | Provisional test capacity is bounded; overflow drops new evidence, counts loss, marks incomplete/degraded, and does not overwrite earlier records. | Phase 2 unit test. |
| PS15.24 | Backend append failure latches fault, reports the failure, and causes later submissions to return without retrying the backend. | Phase 2 failure-injection test. |
| PS15.25 | Interruption reports `INTERRUPTED` and incomplete evidence without creating product state. | Phase 2 unit test. |

## Phase 3 focused checks

| ID | Requirement | Evidence |
|---|---|---|
| PS15.26 | Host/file-backed development storage creates, appends, finalizes, reads, and reconstructs ordered records. | Phase 3 backend contract test. |
| PS15.27 | Development files retain controller/session provenance and checksummed versioned record envelopes. | Phase 3 backend contract test. |
| PS15.28 | Unfinalized sessions are recognized as incomplete and are not promoted to completed evidence. | Phase 3 recovery test. |
| PS15.29 | Corrupt and unsupported-version development files are rejected deterministically. | Phase 3 recovery test. |
| PS15.30 | Start, append, finalize, recover, read, and unavailable failures can be injected deterministically without timing or randomness. | Phase 3 failure-injection test. |
| PS15.31 | Memory and host/file backends satisfy the same DiagnosticStorage contract; no distinct Wokwi host-filesystem backend is required. | Phase 3 contract test and structural review. |

## Phase 4 focused checks

| ID | Requirement | Evidence |
|---|---|---|
| PS15.32 | The controller-side trace identifies START by operation name and preserves the existing request correlation. | Phase 4 trace test. |
| PS15.33 | Accepted START records the observed request, decision/result, authoritative lifecycle transition, and generic Noticeboard invalidation in coherent order. | Phase 4 trace test and development integration. |
| PS15.34 | Rejected START records the operation, correlation, and rejection reason without a false authoritative transition. | Phase 4 trace test. |
| PS15.35 | Noticeboard correlation limitations are recorded without changing the frozen generic Noticeboard message contract. | Phase 4 structural review. |
| PS15.36 | Recorder/backend failure cannot change START acceptance, rejection, state transition, or product operation count. | Phase 4 failure-independence test. |
| PS15.37 | The trace observes the existing path through a separate Diagnostics observer; it does not create duplicate product requests or delay bus delivery. | Phase 4 integration/structural test. |

## Phase 5 focused checks

| PS15.38 | A Browser action observation, HTTP/interface receipt, existing P&P request, and RequestResult can be joined by one diagnostic trace identity while preserving the product correlations separately. | Phase 5 Browser trace test. |
| PS15.39 | Browser client attribution is preserved for multiple clients contributing to one controller diagnostic session. | Phase 5 multi-client trace test. |
| PS15.40 | Accepted and rejected Browser requests are observable with operation name and result; a rejected request never gains a false authoritative transition. | Phase 5 trace test. |
| PS15.41 | Duplicate/repeated actions remain individually attributable; diagnostic observation does not suppress, retry, manufacture, or duplicate the product request. | Phase 5 request-path test and structural review. |
| PS15.42 | Missing or malformed optional Browser diagnostic metadata is ignored without changing product request parsing or authority. | Phase 5 parser/contract test. |
| PS15.43 | Diagnostic recorder/backend failure cannot prevent or alter the real Browser request dispatch or its P&P result. | Phase 5 failure-independence test. |
| PS15.44 | The Browser trace remains bounded and non-blocking and leaves existing product request/result, authority, and race semantics unchanged. | Phase 1–4 regressions plus Stage 1–14C applicable regressions. |

## Phase 6 focused checks

| PS15.45 | An accepted START can be traced through the existing NoticeboardChanged notification, Browser receipt, authoritative State fetch/receipt, State application and actual Browser render observation. | Phase 6 trace test and standard Browser path review. |
| PS15.46 | Notification receipt, State fetch, State application and render evidence preserve client, trace, visible correlation, P&P correlation, revision/epoch and diagnostic sequence as separate identities. | Phase 6 contract test. |
| PS15.47 | Reconstruction/reconnect during an active state records State acquisition/application/render without manufacturing a START action. | Phase 6 reconstruction test. |
| PS15.48 | Two Browser clients observing one change remain individually attributable and cannot affect authority or each other. | Phase 6 multi-client test. |
| PS15.49 | The evidence distinguishes missing notification, incomplete State acquisition, State received but not applied, applied but not rendered, and a complete chain. | Phase 6 incomplete-chain test. |
| PS15.50 | Diagnostic recorder/backend failure cannot prevent notification handling, State collection, State application or render. | Phase 6 failure-independence test. |
| PS15.51 | Phase 6 uses only existing notification, State and render paths; it adds no diagnostic state channel, polling/SSE transport, Browser authority or product semantics. | Structural review and Stage 1–14C applicable regressions. |

## Phase 7 focused checks

| PS15.52 | Diagnostic status reports the current session identity/lifecycle, separate storage health/backend/failure, retained/drop counts, volatility and incompleteness. | Phase 7 contract test and `/diag/status` structural check. |
| PS15.53 | Cursor retrieval returns ordered structured records with controller/session/client provenance and deterministic next-cursor/end markers. | Phase 7 retrieval test. |
| PS15.54 | Retrieval is bounded and chunked; empty/end-of-view, read failure and incomplete evidence are explicit, and reads do not mutate or clear records. | Phase 7 retrieval/resource test. |
| PS15.55 | Appends after an earlier read are visible from a later cursor without claiming an atomic snapshot. | Phase 7 moving-view test. |
| PS15.56 | Overflow/drop and backend append/read/finalize failures are surfaced through status/retrieval without retry storms or product-state mutation. | Phase 7 failure/overflow test. |
| PS15.57 | Host/file, in-memory and deterministic failure-injection implementations remain interchangeable at the DiagnosticStorage boundary; no host filesystem is required by Wokwi firmware. | Phase 1–7 backend contract tests and structural review. |
| PS15.58 | Resource measurements cover record/chunk sizes, bounded request allocation, retained/drop behavior and representative retrieval cost without freezing final capacities or thresholds. | Phase 7 measurement output and firmware build review. |
| PS15.59 | Diagnostic status/retrieval endpoints remain observation-only and do not alter racing, authority, timing, persistence or Browser product behavior. | Phase 1–7 failure-independence checks and Stage 1–14C applicable regressions. |

### Phase 7 measurement record

The host retrieval campaign measured `sizeof(RecordEnvelope)=80` and
`sizeof(RetrievalChunk)=120`, retained eight records, and explicitly dropped
two records after the provisional in-memory bound was reached. The development
file backend remains bounded at 64 records and 4096 payload bytes. Firmware
baseline measurements with `PP_MAX_ENTRIES=8` are 101304 static RAM / 937385
application flash for `stage14cdemo`, and 98808 static RAM / 991885 application
flash for `stage14cacceptance`. `/diag/records` is bounded to a 4096-byte
payload buffer and 10240-byte provisional JSON inspection response per request.
No numeric performance threshold or production capacity is frozen by these
measurements.

## Phase 8 hardware definition

The real-hardware checkpoint uses the established ESP32-8048S070 7-inch
800x480 ESP32-S3 board: 16 MB flash, on-board PSRAM and an on-board microSD
socket. The checkpoint uses the repository's esp32-8048s070 PlatformIO board
definition, derived from the previously used esp32-s3-devkitc-1 target and
corrected for the N16R8 16 MB/PSRAM hardware. The provisional SPI wiring used by
the board microSD is CS GPIO10, MOSI GPIO11, SCK GPIO12 and MISO GPIO13. Only
those four signals are part of the verified software definition; card-detect,
write-protect and other board electrical details remain unspecified unless
runtime evidence establishes them.

These facts identify the Phase 8 test hardware. They do not freeze production
diagnostic capacity, filesystem layout, export encoding or SD housekeeping.

## Final acceptance gates, intentionally outstanding

Final Stage 15 acceptance/freeze requires, in addition:

- measured capacities and performance thresholds;
- real ESP32-S3 plus SD/FATFS checkpoint;
- permission/security policy;
- privacy/redaction policy;
- final Browser diagnostic UX;
- export encoding;
- SD housekeeping and wear policy;
- exact Browser render-evidence contract;
- implementation-informed storage-health transition/recovery behavior.

