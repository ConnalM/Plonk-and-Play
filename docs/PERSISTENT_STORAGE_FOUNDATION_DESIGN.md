# P&P Persistent Storage Foundation Design

**Status: ACCEPTED / FROZEN**

## Purpose

This foundation defines the persistent-storage boundary required by the accepted P&P race architecture. Production SD storage is authoritative for completed-race data; NVS remains for small essential controller state.

It does not change race timing, authority, session, Results, History, Practice, Endurance, PB or Track Record semantics.

## Storage responsibilities

### NVS

NVS stores only small essential controller state, including configuration, remembered setup, retained Browser/Race Director authority, storage-format or migration markers, and small indexes or generations where useful.

NVS is not the production backend for completed-race History or detailed Results/Details. The current Wokwi NVS `NOT_ENOUGH_SPACE` evidence remains valid and is a development failure-boundary case.

### SD

The production SD card is canonical for completed official race History, persisted Results and Details, PBs and Track Records, Record Era state, and future persistent MUG data.

The number of active entries (`PP_MAX_ENTRIES`) is independent of the number of persistent MUG identities, History capacity, and the number of records displayed by a Browser. History capacity is a storage policy independent of UI display count.

## Authority and storage boundary

Race Engine remains authoritative for timing, laps, classification, integrity and completed results. It depends only on a persistence/storage boundary and not directly on SD, FATFS, NVS, filenames or filesystem layout.

The boundary supports append/load/enumerate committed History, clear History, PB/Track Record persistence, and Record Era persistence, with explicit status and fault reporting. Production, development and acceptance backends implement the same boundary.

## Serialized formats

Persistent data uses explicit serialized formats; raw C++ structure dumps are prohibited. Each record contains a magic identifier, record type, format version, sequence or generation identity, payload length, fixed-width encoded fields, checksum, and a commit state or equivalent validity marker.

The format preserves the immutable result information required by the frozen Results, Details and History contracts: Session Definition and mode, entry identities and lane mappings, factual and classified laps, penalties, ranks, ties and laps behind, finish/winning/expiry/overtime data, best/last/fastest lap information, retained valid lap details, and persistent sequence identity.

Detailed retention remains bounded where the applicable frozen mode permits it. Authoritative counts and statistics never depend on retained-detail count.

Existing LAP, Endurance and Practice semantics remain unchanged. Completed official LAP and Endurance results may enter History; abandoned races and Practice do not create official History; existing PB/Track Record eligibility and Record Era semantics remain unchanged.

## Atomic persistence

Completed results use a two-phase persistence model: serialize and checksum the payload; write it to a temporary or uncommitted location; flush/sync it; verify it; write the commit/index record; flush/sync that record; and expose the result only after a valid committed pair exists.

Recovery reconstructs only committed, checksum-valid records. Incomplete temporary data is ignored or reclaimed, and an uncommitted payload cannot appear in History. The same principle applies to PB/Track Record and Record Era updates.

## Persistence failures

A persistence failure never invalidates an authoritative result held in RAM. For a failed History write, the result remains sealed and valid, `persistencePending` remains true, `persistenceFault` becomes true, one storage-fault event/report is emitted, no `HistoryStored` event is emitted, and the same impossible operation is not retried on every Race Engine tick. A retry may occur only through an explicit controlled storage-recovery boundary.

PB/Track Record failures are explicit and latched; their write result is not silently ignored. A failed record write does not alter race validity or lap eligibility.

## SD failure cases

The implementation defines observable behaviour for SD absence, startup unavailability, full storage, payload failure, commit/index failure, corrupt payload or index, unsupported format version, power loss at commit boundaries, and media removal or remount failure.

In every case authoritative in-RAM state is unchanged, invalid or uncommitted data is not presented as official History, faults are visible through persistence status and diagnostics, and valid older committed records remain available where possible.

Unsupported old development formats may be reset under the pre-production migration policy. Future production formats require explicit migration or a safe read-only incompatibility result.

## History retention

History remains a bounded rolling collection of completed official sessions. Factory retention capacity is selected after realistic SD-size, scan-time and reliability measurements. It is not tied to `PP_MAX_ENTRIES` and is not limited to four races. When capacity is reached, the oldest committed entry is removed only after its replacement has been durably committed.

## Development and acceptance backends

Before final SD hardware is available, the same boundary is exercised with a file-backed development backend, failure-injection controls, an acceptance fake, and the existing bounded NVS backend where necessary to reproduce development limitations. The Wokwi `slot-10 NOT_ENOUGH_SPACE` result is retained as a deliberate development failure-boundary case, not a production capacity requirement.

The production backend remains an ESP32-S3 SD/FATFS implementation and requires a later real-hardware checkpoint.

## Interim containment

Before the SD backend is implemented, the current development History path must latch the first failed persistence operation, expose `persistencePending` and `persistenceFault`, publish/report the storage fault once, and stop retrying the same failed operation on every Race Engine tick. NVS preflight prediction is not a product requirement, and NVS is not the production History backend.

## Scope exclusions

This foundation introduces no active-race persistence or recovery, new race modes, new PB/Track Record eligibility rules, new MUG user interface, new Practice or Endurance behaviour, or product maximum based on implementation capacity.
