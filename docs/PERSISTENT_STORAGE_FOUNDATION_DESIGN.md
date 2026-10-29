# P&P Persistent Storage Foundation Design

**Status: ACCEPTED / FROZEN**

## Purpose

This foundation defines the persistent-storage boundary required by the accepted P&P race architecture. Production SD storage is authoritative for completed-race data; NVS remains for small essential controller state.

It does not change race timing, authority, session, Results, History, Practice, Endurance, PB or Track Record semantics.

## Storage responsibilities

### NVS

NVS stores only small essential controller state, including configuration,
remembered setup, the ESP-IDF/Arduino Wi-Fi driver's persisted STA
configuration for the one remembered external network, retained Browser/Race
Director authority, storage-format or migration markers, and small indexes or
generations where useful. The driver-managed flash-backed STA configuration is
the sole persistent credential store for optional AP+STA reconnection; P&P does
not maintain a duplicate SSID/password record. Network credentials remain
outside History, Results, diagnostic storage and support exports.

NVS is not the production backend for completed-race History or detailed Results/Details. The current Wokwi NVS `NOT_ENOUGH_SPACE` evidence remains valid and is a development failure-boundary case.

### SD

The production SD card is canonical for completed official race History, persisted Results and Details, PBs and Track Records, Record Era state, and future persistent MUG data.

The number of active entries (`PP_MAX_ENTRIES`) is independent of the number of persistent MUG identities, History capacity, and the number of records displayed by a Browser. History capacity is a storage policy independent of UI display count.

### Physical SD ownership

When one physical SD card serves more than one P&P storage function, exactly
one P&P physical-storage owner owns that device's FSPI initialisation,
`SD.begin()`/mount, FAT/filesystem lifecycle and recovery. History/Records,
active/detail persistence and Diagnostics are logical consumers. They use
subordinate storage interfaces and namespaces supplied by the physical owner;
they do not create their own `SPIClass`, initialise FSPI, call `SD.begin()` or
independently remount the card.

All actual filesystem access is coordinated and serialized inside the physical
storage boundary. Consumers submit bounded work through their defined storage
boundaries; filesystem latency, queueing or failure must not block
timing-critical Race Engine processing or alter authoritative racing. The
owner may schedule bounded recovery work, but a consumer may not retry an
impossible filesystem operation opportunistically or on every race tick.

There is one mount attempt for each physical-storage lifecycle or explicit
owner-controlled recovery attempt. A successful mount is shared by all logical
consumers. The owner reports physical availability separately from each
consumer's logical health. A logical consumer may be degraded or faulted while
another remains usable, but no consumer may report durable SD-backed operation
while the shared physical device is unavailable. Diagnostics may use its
existing bounded RAM-only/degraded path when durable SD is unavailable.

An SD failure does not stop racing or HTTP. History does not publish a result
as durable unless its existing commit contract succeeds; diagnostic failure
follows the bounded degraded/RAM-only rules. A failure in one namespace does
not imply failure in another unless the common physical layer is the cause.

On reboot, or at an explicit owner-controlled recovery boundary in an eligible
non-active lifecycle state, the physical owner establishes the device once and
then each logical consumer scans/reconstructs its own namespace through its
subordinate interface. Committed History, detail, diagnostic evidence and
their existing sequence/retention semantics are preserved.

History writes, active-detail streaming, diagnostic appends, diagnostic
rolling and diagnostic export reads share the owner's bounded arbitration. No
one consumer may monopolise filesystem work or make another consumer's
timing-critical authority path wait on SD latency.

Physical recovery is prohibited while the authoritative session lifecycle is
`STARTING`, `RACING`, `PAUSED` or `RESTARTING`, and while the completion
settlement/finishing boundary has not yet reached `FINISHED`. Racing continues
under the existing failure-isolation rules while recovery is deferred. Recovery
is eligible in `READY` and in `FINISHED` after the normal completion settlement
has reached that state. A failed recovery leaves physical and logical health
unavailable/degraded and does not affect racing or HTTP. No logical consumer
may independently retry or remount at any lifecycle state.

### Explicit production recovery request

The production support/diagnostics boundary exposes one command/API action named
`Retry storage`. It is a request only: it never calls `SD.begin()`, probes for a
card, mounts, or performs filesystem work in the request handler. The request is
forwarded through the Physical SD Owner boundary, which remains the sole
authority capable of performing recovery.

Only the controlling Race Director/Master may submit the action, and only while
the authoritative lifecycle is `READY` or settled `FINISHED`. A request during
`STARTING`, `RACING`, `PAUSED`, `RESTARTING` or unfinished settlement is rejected
with no retained pending work. A request while storage is already healthy is a
bounded no-op/rejection and performs no mount. If recovery is already pending,
additional requests are rejected as already pending; they never create another
attempt. One accepted request creates at most one pending owner recovery
attempt.

The owner consumes that request once. A successful physical mount is followed by
the existing History, active-detail/Results, Diagnostics and History-count
reconstruction sequence. Durable healthy state is advertised only after all
required reconstruction succeeds. Failed recovery remains unavailable/degraded
and is not retried automatically; another attempt requires another explicit
`Retry storage` request. Reinsertion alone remains inert.

### Explicit production storage check

The support/diagnostics boundary also exposes one authorised, read-only
`Check storage` action. It is a bounded support operation, not a recovery or
test-only hook. Only the controlling Race Director/Master may request it, and
only in `READY` or settled `FINISHED`. One accepted request performs exactly
one owner-mediated filesystem check against an existing required storage
object or metadata whose presence is deterministic when the medium is mounted.
The request performs no `SD.begin()`, mount, remount, card probe, polling,
retry loop, write, repair, migration, deletion or committed-data modification.

The check crosses the Physical SD Owner boundary and uses the existing lease
and `reportPhysicalIoFailure` path. A genuine open/read/I/O failure therefore
transitions the owner to unavailable/non-durable and exposes the corresponding
logical History/Diagnostics/storage health and explicit bounded result. A
successful check reports physical accessibility but does not clear or conceal
logical History corruption or unsupported-record degradation. Physical
accessibility and logical namespace integrity remain separate facts.

`Check storage` never performs recovery. Reinsertion alone remains inert, and
the existing authorised `Retry storage` request remains the only explicit
recovery action. After a failed check, a later reinsertion is followed by
`Retry storage` under the existing lifecycle gate; no automatic retry is
created. The request, attempt, result, resulting owner/logical health and
absence of remount/retry/race/authority side effects are recorded in bounded
diagnostic evidence suitable for support and WP5 acceptance.

## Authority and storage boundary

Race Engine remains authoritative for timing, laps, classification, integrity and completed results. It depends only on a persistence/storage boundary and not directly on SD, FATFS, NVS, filenames or filesystem layout.

The boundary supports append/load/enumerate committed History, clear History, PB/Track Record persistence, and Record Era persistence, with explicit status and fault reporting. Production, development and acceptance backends implement the same boundary.

## Serialized formats

Persistent data uses explicit serialized formats; raw C++ structure dumps are prohibited. Each record contains a magic identifier, record type, format version, sequence or generation identity, payload length, fixed-width encoded fields, checksum, and a commit state or equivalent validity marker.

The format preserves the immutable result information required by the frozen Results, Details and History contracts: Session Definition and mode, entry identities and lane mappings, factual and classified laps, penalties, ranks, ties and laps behind, finish/winning/expiry/overtime data, best/last/fastest lap information, retained valid lap details, and persistent sequence identity.

Detailed retention remains bounded by the available production storage, not by an Endurance product lap ceiling. Authoritative counts and statistics never depend on retained-detail count. The current WP2C development representation uses a provisional bounded dynamic active-detail workspace per entry; its byte budget is an engineering test value, not a product lap limit, and is replaced by streamed per-race SD detail in the production representation.

The active-race implementation separates sporting truth from detail retention.
Race Engine records factual crossings and authoritative totals immediately;
an Active Lap-Detail Store accepts records into a bounded queue and drains them
in order to a temporary per-race SD stream on the production backend.
`CompletedRaceResult` is a summary plus detail association/status, not a fixed
lap-array product limit.
If the detail queue/stream or storage transaction cannot complete, the sealed
Result remains authoritative in RAM but the race is not published as committed
History. A summary-only or falsely-complete History entry is forbidden.

The Race Engine never waits for SD while accepting a lap. At completion the
temporary stream is drained, finalized and verified, then the summary and
detail are encoded and published through one linked transactional History
commit. Temporary streams are never enumerated as History.

Existing LAP, Endurance and Practice semantics remain unchanged. Completed official LAP and Endurance results may enter History; abandoned races and Practice do not create official History; existing PB/Track Record eligibility and Record Era semantics remain unchanged.

## Atomic persistence

Completed results use a two-phase persistence model: serialize and checksum the payload; write it to a temporary or uncommitted location; flush/sync it; verify it; write the commit/index record; flush/sync that record; and expose the result only after a valid committed pair exists.

Recovery reconstructs only committed, checksum-valid records. Incomplete temporary data is ignored or reclaimed, and an uncommitted payload cannot appear in History. The same principle applies to PB/Track Record and Record Era updates.

## Persistence failures

A persistence failure never invalidates an authoritative result held in RAM. For a failed History write, the result remains sealed and valid, `persistencePending` remains true, `persistenceFault` becomes true, one storage-fault event/report is emitted, no `HistoryStored` event is emitted, and the same impossible operation is not retried on every Race Engine tick. A retry may occur only through an explicit controlled storage-recovery boundary.

An active-detail storage interruption is distinguished from actual detail loss. While required detail remains in the bounded queue, storage may be recovered through the explicit owner-controlled recovery boundary once the lifecycle is eligible, and the normal drain/finalize/verify/transaction sequence may still publish the race. Physical recovery is not attempted during `STARTING`, `RACING`, `PAUSED`, `RESTARTING` or unfinished completion settlement. Once a required detail record is dropped or the stream is proven incomplete, `detailIncomplete` is permanent for that race; later recovery cannot make it eligible for History. Active temporary streams are disposable uncommitted debris after restart: they are never History, are never attached to another session, and may be deterministically reclaimed without deleting committed records.

PB/Track Record failures are explicit and latched; their write result is not silently ignored. A failed record write does not alter race validity or lap eligibility.

## SD failure cases

The implementation defines observable behaviour for SD absence, startup unavailability, full storage, payload failure, commit/index failure, corrupt payload or index, unsupported format version, power loss at commit boundaries, and media removal or remount failure.

In every case authoritative in-RAM state is unchanged, invalid or uncommitted data is not presented as official History, faults are visible through persistence status and diagnostics, and valid older committed records remain available where possible.

Unsupported old development formats may be reset under the pre-production migration policy. Future production formats require explicit migration or a safe read-only incompatibility result.

## History retention

History is a bounded rolling collection of the **most recent 100 completed official sessions**. It is not tied to `PP_MAX_ENTRIES` or to the number of records displayed by a Browser. When race 101 is durably committed, the oldest retained completed race is evicted only after the replacement commit is valid. Each retained race is intended to preserve its complete available lap-by-lap detail. The current four-race / 8,192-byte development `HistoryStore` and the provisional bounded active-detail workspace are implementation bounds only; they are not the production retention policy.

## Development and acceptance backends

Before final SD hardware is available, the same boundary is exercised with a file-backed development backend, failure-injection controls, an acceptance fake, and the existing bounded NVS backend where necessary to reproduce development limitations. The Wokwi `slot-10 NOT_ENOUGH_SPACE` result is retained as a deliberate development failure-boundary case, not a production capacity requirement.

The production backend remains an ESP32-S3 SD/FATFS implementation and requires a later real-hardware checkpoint. It must store per-race detail without requiring all retained History, or arbitrarily large lap arrays, to remain in RAM; detail loading/streaming and the rolling-100 index are part of that later implementation.

The established Phase 8 hardware record is the ESP32-8048S070 7-inch
800x480 ESP32-S3 board with 16 MB flash, on-board PSRAM and microSD. Its
provisional SPI microSD mapping for the diagnostic checkpoint is CS GPIO10,
MOSI GPIO11, SCK GPIO12 and MISO GPIO13. This identifies the test hardware
only; it does not define the eventual production SD filesystem or wiring
contract beyond these verified board signals.

## Interim containment

Before the SD backend is implemented, the current development History path must latch the first failed persistence operation, expose `persistencePending` and `persistenceFault`, publish/report the storage fault once, and stop retrying the same failed operation on every Race Engine tick. NVS preflight prediction is not a product requirement, and NVS is not the production History backend.

## WP2E housekeeping policy

The storage boundary exposes advisory backend capacity and health information
(`healthy`, `low`, `insufficient`, `unavailable`, `corrupt` or `unsupported`).
These values describe storage only; they never gate or replace Race Engine
authority. A backend may reject creation of a temporary payload when the
transaction cannot fit. The sealed in-RAM result remains authoritative and the
existing pending/fault boundary applies.

The production History policy remains a rolling 100 committed races. The
current SD implementation uses a fixed bounded set of payload slots. When a
replacement is committed, metadata is published first; only then may the
evicted slot be reclaimed. A failed replacement or failed reclamation cannot
invalidate previously committed metadata. This is a storage bound, not a
sporting or lap limit.

At mount/recovery, only narrowly identified uncommitted transaction temporary
objects are removed: slot payload `.tmp` files and active-detail `.tmp`/`.ok`
objects. Committed payloads and metadata are never removed by age. The fixed
slot set prevents superseded payload growth without requiring an unbounded
index.

Active-detail append and seal operations use the same advisory free-space
check before creating/appending temporary data. A failed check leaves the
authoritative race running and enters the existing pending/fault boundary; it
does not invent a lap or race limit.

The current format matrix is deliberately small:

| Data | Current | Readable legacy | Unsupported behaviour |
|---|---:|---:|---|
| Versioned History/result/detail payload | PSF container v3, result schema v3 | PSF v2/result v2 | reject checksum/schema/version; retain prior committed data |
| History metadata/index | slot metadata v1; legacy metadata reader retained | legacy metadata form | ignore invalid generation and use older valid copy |
| PB/Track Record/Record Era | record format v2 | no migration required | ignore invalid/unsupported record and keep defaults; do not reinterpret |
| NVS configuration | existing dual-copy configuration format | existing readable copy | report invalid/missing and use existing defaults path |

No migration is required for the currently supported legacy data: it is read
directly. No rewrite-on-boot migration is introduced. A future migration must
write and verify a new copy before retiring the old one.

Normal writes are event-driven: active detail is appended as accepted detail
records are drained; one History payload and one metadata generation are
published per completed race; PB/Track Record writes occur only when a record
actually improves or a clear changes it; unchanged values and the main loop do
not cause persistence writes. A failed operation is latched and is not retried
from every tick.

The provisional low-free-space threshold used by the current S3 development
backend is an engineering/test value only. Final capacities, FAT allocation,
wear policy and near-full-card behaviour remain Phase 8B hardware evidence.

## Scope exclusions

This foundation introduces no active-race persistence or recovery, new race modes, new PB/Track Record eligibility rules, new MUG user interface, new Practice or Endurance behaviour, or product maximum based on implementation capacity.

