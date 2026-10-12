# Stage 15 Logging and Diagnostics Foundation Design

**Status:** Foundation Design ACCEPTED/FROZEN for implementation. Final Stage 15 acceptance/freeze is not granted.

## 1. Scope

This foundation records observations about Browser actions, P&P requests and decisions, authoritative state changes, notifications, rendering, storage, and system health. It does not become product state, race authority, timing authority, classification authority, Browser configuration authority, or product persistence.

The implementation must preserve all Stage 1–14C semantics and the preserved Stage 14C candidate.

## 2. Session and storage-health state

The diagnostic session lifecycle is exactly:

`DISABLED`, `STARTING`, `ACTIVE`, `STOPPING`, `COMPLETED`, `INTERRUPTED`.

Storage health is a separate dimension. The foundation contract uses:

`DURABLE_SD`, `RAM_ONLY`, `DEGRADED`, `FAULT`.

`DEGRADED_RAM_ONLY` is not a lifecycle state. An `ACTIVE` session may be `RAM_ONLY` or `DEGRADED`.

Starting without durable storage is permitted. The session starts in clearly identified RAM-only/degraded operation with an explicit warning that evidence is volatile, may be incomplete, and is lost on controller restart unless committed elsewhere.

## 3. Ownership

There is one active Development diagnostic session per P&P controller. A request to start another session returns `ALREADY_ACTIVE` together with the existing session status and identity. It never creates, replaces, merges, or silently joins a session.

Other Browser clients may observe or contribute to the existing session. Each client has an independent client identity recorded in its diagnostic events.

## 4. Structured records

Every record has a versioned envelope containing controller epoch, session identity, client identity where applicable, sequence, diagnostic timestamp, event kind, severity, correlation fields, payload type, and structured payload.

The correlation chain is:

`USER ACTION → Browser request → interface request → Message Bus → P&P decision → authoritative change → notification → Browser collection → render observation`.

Message Bus facts and Noticeboard/SSE notifications remain separate from diagnostic records. Missed diagnostic events must not affect product behavior.

## 5. Recorder and backend boundary

The `DiagnosticRecorder` owns session lifecycle, record sequencing, attribution, status, and fault reporting. The `DiagnosticStorage` boundary abstracts RAM, host/file-backed development storage, Wokwi-compatible development storage, failure-injection storage, and the future ESP32-S3 SD/FATFS backend.

The Phase 1 interfaces do not freeze capacities, queue sizes, file sizes, export encoding, SD layout, or performance thresholds.

Recorder operations must be bounded and non-blocking from timing-critical product code. A diagnostic failure may drop or mark evidence, but must not block or alter racing, timing, classification, persistence authority, or Browser authority.

## 6. Restart and recovery

Each controller boot creates a new controller epoch. Durable diagnostic evidence follows the selected backend recovery rules. A RAM-only active session cannot be reconstructed after restart and is reported as interrupted/lost rather than completed.

Browser reload/reconnect reconstructs diagnostic status from P&P authority. No active-race recovery is introduced.

## 7. Final acceptance boundary

Foundation Design Freeze permits implementation. Final Stage 15 acceptance/freeze additionally requires resource measurement, production ESP32-S3 plus SD/FATFS evidence, permission/security, privacy/redaction, final Browser UX, export encoding, SD housekeeping/wear policy, exact render-evidence rules, and implementation-informed storage-health recovery details.

## 8. Phase 2 development implementation

Phase 2 provides an in-memory recorder and backend solely to exercise the contract. It uses an explicitly named provisional test bound and retains records in insertion order. When the bound is reached, new records are dropped, loss is counted, and storage health becomes `DEGRADED`; records are not silently overwritten. Backend failures are latched as `FAULT`, subsequent submissions do not retry the failed backend operation, and the diagnostic session remains separate from product authority.

The Phase 2 bound is a test/development implementation detail. It is not a product retention capacity, SD capacity, or final performance threshold.

## 9. Phase 3 development storage

Phase 3 adds a host/file-backed development backend and a deterministic failure-injection wrapper. The file representation is explicitly development/test-only, versioned, checksummed, session-scoped, and capable of recognizing finalized, unfinalized, corrupt, and unsupported-version data. It is not the production SD layout or final export encoding.

The in-memory backend remains the firmware-compatible development backend for Wokwi tests. No Wokwi firmware access to host files is required, and no separate Wokwi storage architecture is introduced in Phase 3.

## 10. Phase 4 controller-side operation trace

Phase 4 adds one observation-only controller trace spine for an existing named
operation, initially `START`. The development/demo controller observes the
existing `StartRequest`, `RequestResult`, authoritative state transition, and
generic `NoticeboardChanged` messages through a separate Diagnostics bus
observer. It records the operation name, existing request correlation, event
ordering, and the observed authoritative lifecycle snapshot in the existing
bounded in-memory recorder.

There is no new product request path and no new product message contract. The
current `RequestResult` is the available Race Control decision/result boundary;
there is no separate downstream decision message to correlate. A generic
`NoticeboardChanged` has no request correlation, so the trace associates it with
the pending accepted START observation only and records that limitation rather
than changing the frozen Noticeboard contract.

Accepted and rejected START requests are both observable. A rejected request
must not produce a diagnostic state-transition record. Recorder/backend failure
may lose or mark diagnostic evidence but cannot affect request acceptance,
authoritative state, timing, or Noticeboard publication. Phase 4 does not add
Browser click/render tracing, detector/lap tracing, persistence tracing, SD,
SSE transport tracing, or final diagnostic UX.

