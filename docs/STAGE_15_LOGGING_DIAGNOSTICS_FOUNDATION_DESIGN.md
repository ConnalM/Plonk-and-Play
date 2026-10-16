# Stage 15 Logging and Diagnostics Foundation Design

**Status:** Foundation Design ACCEPTED/FROZEN for implementation. Final Stage 15 acceptance/freeze is not granted.

## 1. Scope

This foundation records observations about Browser actions, P&P requests and decisions, authoritative state changes, notifications, rendering, storage, and system health. It does not become product state, race authority, timing authority, classification authority, Browser configuration authority, or product persistence.

The implementation must preserve all Stage 1–14C semantics and the preserved Stage 14C candidate.

## 1A. Diagnostic privacy and access contract

Diagnostic recording is for technical and product diagnosis only. P&P may
record structured technical information needed to diagnose behaviour,
including controller/build identity, lifecycle and race state, race
configuration, lane information, timings, requests/results, correlation and
trace identities, detector/input events, persistence/storage events,
resource/health information, Browser/Taster observations, errors, drops and
diagnostic health.

Racer/MUG names or identifiers may appear when they are genuinely part of the
product state being diagnosed. They must not be duplicated merely for
logging. Diagnostics must never deliberately record Wi-Fi passwords,
passwords, secrets, authentication tokens, private/signing keys,
cookies/session credentials, complete HTTP headers for convenience, arbitrary
Browser form contents, or unrelated user data. Free-form or unknown external
data is not copied automatically; records use defined structured fields.

Normal diagnostics may run automatically during ordinary P&P operation.
Development diagnostics are an explicit troubleshooting facility under the
advanced diagnostic facilities and require the same P&P
Master/Race-Director/controlling-SMUG control lease used by other advanced
controls. Starting and stopping Development diagnostic sessions require that
same lease. Downloading a diagnostic artifact is an observation-only action
available to any connected Browser with normal P&P access; it does not grant
control and must not offer TAKE CONTROL merely to download. Status and
diagnostic viewing are likewise observation-only unless a narrower rule is
explicitly required by the product UI. No separate SMUG capability, account,
password, PIN or authentication system is introduced. Diagnostic access never
grants Race Control or Race Engine authority, and diagnostic, access or export
failure must not alter product/race operation.

No elaborate account/login system is introduced solely for Stage 15. The
existing/frozen P&P configuration and access model must enforce the
Development boundary at the request boundary. The production implementation contains the controlling-lease check for Development start/stop, and status/retrieval/download remain observation-only. These checks are request-boundary rules, not permissions inferred from Browser presentation.

## 1B. WP3A product support contract

WP3A freezes the customer-facing diagnostic contract and design direction. It
does not claim that the production recorder, export route or final Browser UI
is implemented. WP3B is the subsequent implementation package.

### Customer workflow

The supported workflow is:

`Settings → Advanced → Diagnostics → DOWNLOAD DIAGNOSTICS → send one item to support`.

The customer receives one P&P-generated artifact. They do not select raw logs,
sessions, cursors, sinks, files or endpoints. A download may be requested while
recording is active. P&P owns the snapshot boundary and recording continues
after the snapshot. The Browser displays the authoritative export capability
and result; it does not implement a snapshot or recovery state machine.

### Artifact representation and contents

The WP3A support artifact is a versioned UTF-8 newline-delimited JSON file
(`.ndjson`), with one self-identifying header record, structured diagnostic
records in deterministic sequence order, and a summary/footer record. The
header identifies the product, firmware/build, controller epoch, export
schema, snapshot boundary, storage-health and capability state. Records retain
the structured envelope, timestamps, client/session identity where applicable,
correlations, event kind and allow-listed payload. The summary reports record
count, drops and whether evidence is incomplete, volatile or degraded.

The artifact must contain enough retained Normal material and Development
material up to the snapshot to reconstruct chronology, reboot/session
boundaries, health/storage faults, controller/network context, race/session
state, requests/results and input context. It must remain inspectable and
deterministic for tests. The current `/diag/records` hexadecimal response is a
development inspection view, not this support artifact. Compression, byte
limits and final SD placement remain implementation/physical-validation
details and are not frozen here.

### Recording scope

Normal diagnostics are always-on, quiet, bounded and rolling. They capture the
allow-listed Stage 15 taxonomy needed to explain ordinary customer faults:
boot/controller/build identity, session/reconstruction boundaries, material
request acceptance/rejection, authoritative lifecycle/state changes,
persistence and storage-health transitions, resource-health faults/recovery,
and relevant notification/transport-health transitions. They do not record
unchanged loops, raw traffic or arbitrary form data. Detailed detector/input,
Browser request/result, notification, collection and render observations are
Development-level evidence, with Normal retaining only material fault or
transition observations needed for support.

Development diagnostics include Normal scope plus the deeper structured
request, input/detector, Message Bus/Race decision, Browser notification,
authoritative collection, render, persistence, resource, fault, drop and
reconstruction observations already defined by the Stage 15 taxonomy. Both
scopes use an allow-list; neither copies raw headers, secrets, cookies,
arbitrary form contents or unrelated personal data.

### Retention, storage health and failure

Normal retention is a bounded rolling window; the oldest diagnostic evidence
is discarded when its bound is reached, without outranking authoritative
History/Results data. Development retention is bounded and reports explicit
drops/loss. Temporary export material is bounded, reclaimable and leaves no
uncontrolled debris. Writes must not occur per loop or retry unchanged status
indefinitely; exact capacities, write timing, SD throughput and wear tuning
remain provisional until WP4 hardware evidence.

Export capability is P&P-owned and reports one of `AVAILABLE`, `LIMITED` or
`UNAVAILABLE`. `LIMITED` means a useful artifact can be produced from retained
RAM/current status/build/health despite degraded or missing durable storage;
`UNAVAILABLE` means no useful artifact can be produced. SD unavailability does
not disable diagnostics or racing, and diagnostic failures never alter product
authority. Existing storage-health state is observed; no second diagnostic
recovery state machine is introduced.

### Browser hierarchy

The final Browser hierarchy is Settings → Advanced → Diagnostics. Availability
and DOWNLOAD DIAGNOSTICS are primary. Firmware/build, System Health,
Diagnostic Storage and Normal Diagnostics state are secondary. TECHNICAL
DETAILS and DEVELOPMENT DIAGNOSTICS are tertiary. Raw logs, files, cursors,
JSON, API endpoints, addresses, Message Bus/SD paths, raw HTTP, live console
and CLEAR DIAGNOSTICS are not customer controls. Normal diagnostics has no
customer Start/Stop. Development START/STOP is offered to the controlling
SMUG. Any connected Browser may request the observation-only download.

### WP3A status boundary

This section records the **WP3A Foundation/Product Design Freeze**. WP3B–WP3E
software implementation is complete for review, including the production
recorder integration, Browser workflow, export route/encoding and support
artifact. This does not grant final Stage 15 acceptance. Final acceptance also
requires the resource, policy, real ESP32-S3/SD, recovery, housekeeping and
render-evidence gates in Section 7.

### Development duration and reboot volatility

Development records already retained or durably written are not deleted merely
because the mode expires and remain available under the normal bounded
retention policy.

The enabled/active Development-logging mode is volatile and is not persisted.
A reboot, reset or power loss ends Development logging. P&P always starts or
restarts in Normal diagnostics only. There is no automatic resume and no
resume prompt. Previously recorded diagnostic material remains available
subject to bounded retention. If further Development detail is required, the
controlling SMUG deliberately starts a new Development session. This
volatility applies to the mode/enabled state, not to already-recorded evidence.

## 2. Session and storage-health state

The diagnostic session lifecycle is exactly:

`DISABLED`, `STARTING`, `ACTIVE`, `STOPPING`, `COMPLETED`, `INTERRUPTED`.

Storage health is a separate dimension. The foundation contract uses:

`DURABLE_SD`, `RAM_ONLY`, `DEGRADED`, `FAULT`.

`DEGRADED_RAM_ONLY` is not a lifecycle state. An `ACTIVE` session may be `RAM_ONLY` or `DEGRADED`.

Starting without durable storage is permitted. The session starts in clearly identified RAM-only/degraded operation with an explicit warning that evidence is volatile, may be incomplete, and is lost on controller restart unless committed elsewhere.

## 3. Ownership

There is one active Development diagnostic session per P&P controller. A request to start another session returns `ALREADY_ACTIVE` together with the existing session status and identity. It never creates, replaces, merges, or silently joins a session. Access to these session controls is checked against the authoritative single Master/Race Director/controlling-SMUG lease; Browser presentation flags are not an authorization source.

Other Browser clients may observe or contribute to the existing session. Each client has an independent client identity recorded in its diagnostic events.

## 4. Structured records

Every record has a versioned envelope containing controller epoch, session identity, client identity where applicable, sequence, diagnostic timestamp, event kind, severity, correlation fields, payload type, and structured payload. For durable diagnostic storage, sequence is monotonically increasing and unique within the retained durable diagnostic stream/storage generation, including across controller restarts; `controllerEpoch` remains separate provenance identifying the boot that produced a record. A volatile RAM-only stream may begin a new sequence after reboot because its prior stream no longer exists. A failed or ambiguous append consumes its reserved sequence; a resulting gap is reported as incomplete evidence rather than reusing the candidate.

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

Foundation Design Freeze and the WP3A product design freeze permit
implementation. Final Stage 15 acceptance/freeze additionally requires
resource measurement, production ESP32-S3 plus SD/FATFS evidence,
permission/security, privacy/redaction, final Browser UX, export encoding, SD
housekeeping/wear policy, exact render-evidence rules, and
implementation-informed storage-health recovery details. WP3A itself is not
complete merely because this design is frozen.

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

## 11. Phase 5 Browser/HTTP request trace

Phase 5 extends the observation spine to the Browser request boundary for the
development/diagnostic Browser path. A Browser action observation and the
corresponding HTTP/interface receipt carry the server-attributed Browser
client identity, the Browser-visible product correlation, and a separate
diagnostic trace identity. The existing internal P&P correlation remains
unchanged and is recorded separately, so diagnostic metadata cannot become a
permission, sporting, timing, configuration, or result authority.

The trace distinguishes an observed Browser action from a request received at
the HTTP boundary and from the existing Message Bus request/result records.
An action record without a request record means that the request was not
observed at the controller boundary; a request without a later bus/result
record remains an explicit transport/interface gap. Malformed or absent
optional diagnostic metadata is ignored by product request parsing and never
changes request acceptance.

The Browser-side observation is fire-and-forget and is never a prerequisite
for dispatching the real product request. It does not retry, suppress, or
duplicate a product request. Multiple Browser clients contribute to the one
active diagnostic session using their server-attributed client identities.
The bounded recorder may drop diagnostic records or latch a fault without
changing product operation behaviour.

Phase 5 does not define final Browser diagnostic UX, security/permission or
privacy policy, export encoding, storage capacities, SD layout, SSE/state
tracing, or render-evidence semantics.

## 12. Phase 6 authoritative state to Browser render trace

Phase 6 observes the existing presentation path only:

`authoritative P&P change → NoticeboardChanged → existing notification → Browser receipt → authoritative /state collection → state application → render evidence`.

The generic NoticeboardChanged contract remains unchanged and carries no new
product correlation. Diagnostic records may associate a notification with a
trace only where the existing metadata makes that association legitimate; a
generic notification without proven causation is recorded as such. Browser
notification receipt is attributed to the server-assigned Browser client and
preserves any available diagnostic trace, visible correlation, revision and
event identifiers separately.

State-fetch start/receipt, state application and render evidence are separate
observation records. They carry compact lifecycle/session information and the
existing P&P revision/epoch identifiers where available; they do not duplicate
full State or invent a Browser-owned revision. Render evidence is a narrow
observation of the existing DOM/presentation update, not a screenshot system,
renderer duplicate or authority source. Diagnostic submission is fire-and-
forget and must never be required for notification handling, state collection,
state application or rendering.

Initial connection, reload, reconnect and controller-epoch reconstruction are
marked as reconstruction observations. They do not manufacture a START trace.
Multiple Browser clients remain independently attributable while observing the
same authoritative state. The trace must make it possible to distinguish a
missing notification, incomplete state acquisition, state received but not
applied, applied but not rendered, and the complete chain. Phase 6 does not
add diagnostic state delivery, SSE/polling, a Browser product state machine,
general screen instrumentation, detector/lap tracing or persistence tracing.

## 13. Phase 7 resource, retrieval and robustness foundation

Phase 7 measures the bounded development implementations and exposes the
existing diagnostic session through observation-only status and cursor-based
retrieval. `GET /diag/status` reports session identity/lifecycle, storage
health/backend/failure, retained record and payload-byte occupancy, dropped
counts, and volatility or incompleteness. `GET /diag/records?cursor=N` returns one current record at a
time with its envelope provenance and a provisional hexadecimal payload view.
This transport representation is development inspection only; it does not
freeze the final export encoding, permission model, privacy policy, SD layout,
or product retention capacity. It is not the WP3A support artifact; the
customer download path uses the versioned NDJSON contract in Section 1B.

Retrieval is a bounded moving view. Each request reads the current retained
count at its cursor; records appended after a read are visible from a later
cursor, and reads do not clear or mutate evidence. End-of-view, read failure,
and incomplete evidence are explicit in the response. The implementation uses
bounded per-request buffers and no new diagnostic task, queue, or SD I/O path.
Status/retrieval failures remain observation-only and cannot affect product
authority or timing.

Phase 7's host/file and in-memory limits remain provisional development/test
values. The resource campaign records measured sizes, latency and loss
behavior for later capacity and performance decisions. Production ESP32-S3 SD
storage and final recovery/housekeeping remain outside this phase.

## 14. Phase 7 measured baseline

The focused host campaign measured an 80-byte `RecordEnvelope` and a 120-byte
`RetrievalChunk`. The exercised in-memory test bound retained eight records and
reported two explicit drops after exhaustion; the development file bound is 64
records with a 4096-byte payload limit. Each `/diag/records` request uses a
4096-byte payload buffer and a 10240-byte provisional JSON inspection buffer.

The standard instrumented ESP32 builds used `PP_MAX_ENTRIES=8`. The demo image
used 101304 bytes static RAM and 937385 bytes application flash; the acceptance
image used 98808 bytes static RAM and 991885 bytes application flash. These are
baseline measurements, not acceptance thresholds. Sustained Wokwi load, live
heap/stack pressure and SD latency remain environment or real-hardware campaign
items and do not become claims from this host test.

## 15. Phase 8 hardware baseline

The Phase 8 real-hardware checkpoint targets the established ESP32-8048S070
7-inch 800x480 ESP32-S3 display board. The board record identifies 16 MB flash,
on-board PSRAM, and an on-board microSD socket. The checkpoint uses the
repository's `esp32-8048s070` PlatformIO board definition, derived from the
previously used `esp32-s3-devkitc-1` target and corrected for the N16R8
16 MB/PSRAM hardware. It does not claim that display peripherals are part of
the diagnostic firmware.

The board microSD is exercised in SPI mode with the verified signal mapping:
CS GPIO10, MOSI GPIO11, SCK GPIO12, and MISO GPIO13. No card-detect,
write-protect, display, touch, or other electrical details are assumed. Runtime
firmware must report the detected card, filesystem and capacity rather than
assuming them.

This is a Phase 8 hardware-test definition only. It does not freeze the
production diagnostic directory layout, export encoding, capacity, wear policy,
or final SD integration architecture.
