# Acceptance Tests — P&P Persistent Storage Foundation

**Status: ACCEPTED / FROZEN**

## Purpose

These tests verify the persistent-storage boundary without changing frozen Stage 1–14C race, timing, authority or record semantics.

## Acceptance matrix

| ID | Requirement | Boundary |
|---|---|---|
| PSF.1 | Race Engine uses only the persistence boundary; no direct SD, FATFS or NVS dependency exists in race logic. | Development |
| PSF.2 | History, Results/Details, PB, Track Record and Record Era data use explicit serialized versioned formats, never raw C++ structure dumps. | Development |
| PSF.3 | A completed LAP result appends, reconstructs and reloads after controller restart with unchanged Results/Details data. | Development |
| PSF.4 | A completed Endurance Stop at Zero result reconstructs with duration, expiry, overtime, factual/classified laps, ranking and details intact. | Development |
| PSF.5 | A completed Endurance Finish Current Lap result reconstructs with expiry, overtime, final crossings, factual/classified laps and details intact. | Development |
| PSF.6 | Abandoned races create no official History entry. | Development |
| PSF.7 | Practice creates no official History, PB or Track Record data. | Development |
| PSF.8 | Multiple History records retain unique sequence identities and reconstruct newest-first ordering. | Development |
| PSF.9 | Configured rolling retention removes only the oldest committed entry after the replacement is durably committed. | Development |
| PSF.10 | History capacity is independent of `PP_MAX_ENTRIES` and of the number of records displayed by the Browser. | Development |
| PSF.11 | PB and Track Record updates persist and reconstruct independently of History eviction. | Development |
| PSF.12 | Record Era creation, individual clears, all-record clears and History clears preserve frozen Stage 13 separation rules. | Development |
| PSF.13 | Missing SD/backend is reported as a persistence fault without invalidating an authoritative in-RAM result. | Development and Hardware |
| PSF.14 | Full storage is reported without exposing an uncommitted result as official History. | Development and Hardware |
| PSF.15 | A payload write failure leaves previous committed History intact and latches the new persistence fault. | Development |
| PSF.16 | A commit/index write failure leaves the failed payload uncommitted and preserves previous committed History. | Development |
| PSF.17 | Simulated power loss before payload flush, after payload flush, before commit, and after commit reconstructs only valid committed data. | Development |
| PSF.18 | Corrupt payloads and indexes are rejected, reported and excluded from reconstructed History. | Development and Hardware |
| PSF.19 | Unsupported format versions follow the defined migration/reset boundary and are never decoded as another schema. | Development |
| PSF.20 | A failed History operation latches `persistencePending`/`persistenceFault`, reports one storage fault, and does not retry on every Race Engine tick. | Development and Hardware |
| PSF.21 | PB/Track Record persistence failure is reported explicitly and is not silently ignored; race validity and lap eligibility remain unchanged. | Development and Hardware |
| PSF.22 | A valid authoritative result remains valid in RAM after every persistence failure case. | Development and Hardware |
| PSF.23 | Restart/reconnect reconstructs committed History, Results and Details through the production Browser/API boundary without Fact replay or Browser-side calculation. | Development and Hardware |
| PSF.24 | The current Wokwi NVS exhaustion (`slot-10 NOT_ENOUGH_SPACE`) is reproduced as a bounded development failure case, with one latched fault and no per-tick retry storm. | Development |
| PSF.25 | File-backed and failure-injection backends produce equivalent boundary outcomes for success, failure, recovery and version handling. | Development |
| PSF.26 | The production SD/FATFS backend stores and reconstructs the same serialized records using the same storage contract. | Hardware |
| PSF.27 | SD removal, remount and recovery in the permitted non-active lifecycle states preserve committed records and expose unavailable/pending state without changing authoritative race state; recovery is deferred during prohibited session states. | Hardware |
| PSF.28 | Repeated real power interruption during active SD persistence operations, followed by reboot/remount/reconstruction, proves that no partial or uncommitted result appears as official History; previously committed records remain valid; newly committed records appear only when their commit completed; corrupt/incomplete temporary data is ignored or reclaimed; PB/Track Record/Record Era stores remain internally consistent; and persistence faults/recovery are surfaced correctly. | Hardware |
| PSF.29 | Complete applicable Stage 1–14C automated, structural, Browser, Wokwi and shared-path regressions remain passing. | Development |
| PSF.30 | The deliberate evaluator-failure/recovery proof remains genuine and passes against the final storage implementation. | Development |
| WP2E.1 | Repeated rolling-100 replacement keeps the physical backing model bounded; the evicted slot is reclaimed only after replacement metadata commits. | Development |
| WP2E.2 | Reclamation failure does not invalidate committed History, and narrowly identified temporary transaction objects are cleaned without deleting committed data. | Development |
| WP2E.3 | Capacity checks distinguish healthy/low/insufficient/unavailable storage without changing authoritative race operation. | Development and Hardware |
| WP2E.4 | Current and supported legacy format versions are accepted by the matrix; unsupported/corrupt versions are rejected safely and no migration is performed when direct legacy reading is sufficient. | Development |
| WP2E.5 | Normal persistence writes are event-driven and bounded: no main-loop rewrite or retry storm; unchanged PB/Track Record/configuration is not rewritten. | Development |
| WP2E.6 | Practical payload/metadata estimates are recorded as measured development evidence, while final SD capacity/wear thresholds remain hardware debt. | Development |
| WP2E.7 | Active-detail append and final seal reject an insufficient transaction before creating more temporary data, while authoritative racing remains unaffected. | Development and Hardware |
| WP2F.1 | A normal Race Engine/Input scenario with a recoverable temporary storage interruption completes, commits, reconstructs Result/Details and reconstructs PB/Track Record consequences without changing authoritative race facts. | Development |
| PSF.31 | One physical SD owner performs the only FSPI/`SD.begin()` mount for a lifecycle; History, active/detail persistence and Diagnostics consume subordinate interfaces and do not independently mount or remount. | Development and Hardware |
| PSF.32 | One successful physical mount is shared by History and Diagnostics; a physical mount failure produces the defined independent logical degraded/fault states, with Diagnostics using RAM-only/degraded operation where permitted. | Development and Hardware |
| PSF.33 | Reboot or explicit owner-controlled recovery in an eligible non-active lifecycle state mounts once, reconstructs the History and Diagnostics namespaces, and preserves committed records and existing sequence/retention semantics. | Development and Hardware |
| PSF.34 | History writes, active-detail streaming, diagnostic appends, rolling and export reads coexist through bounded owner arbitration without corruption, starvation or uncontrolled temporary growth. | Development and Hardware |
| PSF.35 | Slow or failing SD access cannot block or alter authoritative Race Engine timing, input processing, results or HTTP operation; failed History work remains uncommitted and diagnostic failure follows its existing bounded degraded/RAM-only rules. | Development and Hardware |
| PSF.36 | Recovery is requested only through the physical owner boundary; logical consumers cannot retry `SD.begin()` independently at any lifecycle state. | Development and Hardware |
| PSF.37 | Boot performs exactly one normal PhysicalSdOwner mount attempt. If that attempt fails, the owner remains `UNAVAILABLE`/non-durable and enters explicit-recovery-required; repeated controller loops and later card insertion perform zero mounts. Physical recovery is suppressed in `STARTING`, `RACING`, `PAUSED`, `RESTARTING` and before completion settlement reaches `FINISHED`; authoritative racing continues while deferred. Recovery becomes eligible in `READY` and settled `FINISHED` only after one authorised `Retry storage` request, performs at most one owner-controlled mount, reconstructs each subordinate namespace before advertising durable health, and leaves racing/HTTP unaffected if it fails. | Development and Hardware |
| PSF.38 | The controlling Race Director/Master can issue one authorised `Retry storage` request after the physical problem is corrected. The request performs no storage work itself, crosses the PhysicalSdOwner boundary, is accepted only in `READY`/settled `FINISHED`, creates at most one pending attempt, rejects prohibited/healthy/already-pending requests, and requires a new explicit request after failure. | Development and Hardware |
| PSF.39 | The controlling Race Director/Master can issue one authorised read-only `Check storage` request in `READY`/settled `FINISHED`; it performs exactly one bounded owner-mediated filesystem check, reports physical success or failure explicitly, propagates genuine I/O failure through the owner, never mounts/remounts/retries or modifies committed data, and leaves logical History integrity distinct from physical accessibility. | Development and Hardware |

## Required evidence

Each result retains the source/build manifest, persistence-format version, backend identity, predetermined stimulus, serialized-byte or checksum evidence, reconstructed State/Results/Details, Request Result and persistence status, and PASS/FAIL outcome.

Capacity evidence distinguishes development-backend measurements from production SD capacity.

### PSF.38 Retry-storage acceptance sequence

The required proof begins with a boot-absent prelude: absent SD at boot; exactly
one failed normal boot mount; explicit-recovery-required with zero further
automatic mounts; physical SD insertion with no health restoration or mount; one
authorised `Retry storage` request and exactly one owner mount/reconstruction to
restore the healthy baseline. The physical sequence then continues: healthy
mounted storage; physical SD removal;
one genuine consumer I/O failure; authoritative owner `DEGRADED`/unavailable;
reinsertion with no health restoration or mount; one authorised `Retry storage`
request; exactly one owner mount; History, active-detail/Results, Diagnostics and
History-count reconstruction; sequences 9, 10 and 11 retained and consistent;
durable `HEALTHY` advertised only after reconstruction succeeds. The sequence
must also cover duplicate/double requests, a request while recovery is pending,
a request while storage is already healthy, failure followed by a later explicit
retry, and rejection during prohibited lifecycle/authority states.

### PSF.39 Check-storage acceptance sequence

The required physical sequence is: healthy mounted storage; physical SD removal;
one authorised `Check storage` request from the controlling Race Director/Master
in `READY` or settled `FINISHED`; exactly one bounded owner-mediated filesystem
check; explicit physical success or I/O failure; owner and logical health
reporting; reinsertion with no autonomous recovery; one authorised `Retry
storage` request; exactly one owner mount/reconstruction; and durable health
restored only after reconstruction succeeds. The check must not write, repair,
migrate, delete, poll, retry, remount or alter race/authority state. Evidence
must identify request receipt, attempt, result, health transition and absence
of recovery or race side effects. A successful check must not conceal any
independent logical History corruption/unsupported-record degradation.

## Development acceptance boundary

Before SD hardware is available, the following may be accepted against the file-backed, failure-injection and acceptance backends: serialization/versioning; storage-boundary independence; append/reconstruct/reboot; rolling retention; PB/Track Record and Record Era persistence; missing/full/corrupt storage; payload and commit failures; simulated power loss at every defined boundary; unsupported versions; fault latching/retry suppression; Wokwi NVS exhaustion reproduction; and cumulative Stage 1–14C regressions.

These results do not claim production SD capacity or real-hardware power-loss behaviour.

## Hardware acceptance boundary

The following require the real ESP32-S3 integrated board with SD/FATFS: SD mount/startup recovery; realistic History capacity and scan time; SD absent/full/removal/reconnection; real payload and commit failures; repeated real power interruption during active persistence operations followed by reboot/remount/reconstruction; production Browser reconstruction after controller restart; and resource, wear and timing measurements for the selected factory retention capacity.

The hardware test does not require artificially precise interruption at every internal commit boundary. It requires repeated real interruptions during active persistence and evidence of safe committed-record recovery, ignored or reclaimed incomplete data, consistent PB/Track Record/Record Era stores, and correctly surfaced faults/recovery.

WP2E development evidence does not claim real-card free-space, allocation,
wear or throughput behaviour. Those remain hardware acceptance work.

## Cumulative regression rule

No storage change may weaken or rewrite frozen Stage 1–14C requirements. All applicable earlier automated, structural, Wokwi, Browser and human hardware evidence must remain valid or be rerun where the shared persistence boundary is affected.

Practice remains non-official. Abandoned races remain excluded from official History. PB/Track Record eligibility and Record Era semantics remain unchanged.

## WP2F final software campaign record

WP2F software acceptance is complete when the focused PSF/WP2A–E suite and
`wp2f_semantic_test.cpp` pass. The semantic test drives the normal
Race Engine/Input path through a recoverable temporary detail interruption,
explicit recovery, commit, Result/Details reconstruction and PB/Track Record
reconstruction. This is a development acceptance result and does not replace
the physical SD/power-loss requirements marked Hardware above.

## Production active-detail streaming and software failure boundary (WP2D)

The production SD path accepts lap records through a bounded non-blocking
queue and drains them to a temporary per-race detail stream. A healthy normal
Race Engine path must keep complete active detail out of the sealed result
object while the race is running and must support runs beyond the former
128 KiB development workspace. Completion drains, finalizes and verifies the
temporary stream before linked summary/detail History publication.

Software acceptance injects active-stream open, append/write and
finalize/verification failures. In each case sporting state remains valid,
factual totals and timing remain correct, incomplete detail is explicit, no
History record is appended, the sealed RAM Result remains available, and the
failure is latched without a retry storm. Real card removal, power loss and
physical remount remain hardware acceptance work.

Software recovery distinguishes temporary unavailability from actual detail
loss. A controlled recovery before queue exhaustion drains the ordered backlog
and permits the normal finalize/verify/transaction boundary to publish the
race. Queue exhaustion, truncation, corruption or other proven required-detail
loss latches `detailIncomplete` permanently; later recovery cannot publish that
race. Active temporary streams left after restart are ignored or reclaimed
deterministically, are never attached to a new session and are never History.

## Active lap-detail acceptance (WP2C)

The active detail path is tested through the normal Race Engine/Input Module
boundary, not by constructing a synthetic completed result.  Healthy detail
must survive 33, 100 and more than 255 live laps without truncating factual
totals, classification or Best Lap.  A detail-buffer or storage failure may
not alter racing; it marks detail incomplete and prevents publication of that
race into committed History.  A sealed in-RAM Result remains authoritative.
Details are reconstructed on demand from the linked committed summary/detail
representation, while active temporary detail is never enumerated as History.
Practice remains outside official History unless a later product decision says
otherwise.
