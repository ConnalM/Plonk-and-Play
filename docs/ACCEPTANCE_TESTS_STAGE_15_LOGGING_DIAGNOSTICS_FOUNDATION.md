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

