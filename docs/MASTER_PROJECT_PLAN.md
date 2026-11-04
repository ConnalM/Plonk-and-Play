# P&P Consolidated Completion Plan

**Status:** Authoritative project-control plan — corrected 4 November 2026
**Purpose:** One route from the present engineering system to a releasable P&P v1.

This file supersedes the old Stage plan, the earlier WP0–WP12 execution plan and ad-hoc gap lists **as project roadmaps**. Those documents remain authoritative specifications and evidence where applicable. Nothing already proved is to be rerun merely because this plan is newer.

The detailed reconciliation that feeds this plan is `docs/P_AND_P_COMPLETION_RECONCILIATION_2026-10-23.md`.

## 1. Project-control rules

### One-plan rule
Every known requirement or unresolved obligation must have exactly one disposition:

- **DONE** — sufficient evidence exists;
- **CURRENT/LATER WP** — required for v1 and assigned here;
- **DEFERRED POST-v1** — deliberately outside v1;
- **SUPERSEDED** — historical path/wording no longer applicable.

No separate Stage/WP/gap list may become a competing execution plan.

### Development rhythm

**Frozen behaviour/product decision → implementation + automated tests → PC/Wokwi integration → semantic review → software gate → one hardware candidate → concentrated hardware validation.**

Connal is not routine firmware test labour. Hardware testing is grouped at deliberate gates.

### Evidence rule
Existing valid host, Wokwi and physical evidence is credited. Physical requirements still require physical evidence, but an already passed physical test is not repeated merely for paperwork.

### Project-owner information rule
Connal is Project Owner, not the technical message bus. He is involved when a product/scope/cost decision, physical action or acceptance observation is required.

Archie and Al resolve routine technical detail between themselves through the repository wherever possible. Al records concise reconciliation/implementation outcomes in GitHub; Archie reads them directly. Connal is not expected to interpret source code, logs or detailed acceptance reports.

## 2. Roles

- **Connal — Project Owner:** scope, priorities, customer behaviour, cost decisions, deliberate physical acceptance and final approval.
- **Archie — Project Manager / Senior Architect:** roadmap, dependencies, scope control, architecture, gates, reconciliation and readiness decisions.
- **Al — Chief Programmer / Troubleshooter:** repository reality, implementation, builds, automated tests, Wokwi, diagnostics and technical investigation.
- **Gina — UX/UI Lead:** Browser/local-display interaction, wording and presentation once underlying behaviour is settled.
- **Bob — Independent Test & Verification:** later release-candidate evidence review; not another designer/coder.

## 3. v1 boundary

P&P v1 includes:

- Lap Race with all three finish policies;
- Open Practice;
- Endurance: Stop at Zero and Finish Current Lap;
- 1–8 lane software/product architecture, initially provable with two physical lanes;
- Normal Browser, one Race Director/controller lease and spectator Browsers;
- local 7-inch display/Taster operation;
- real optical lap detection and Track Setup;
- Results, Finish Display, History/Details, PB, Track Record and Record Era where applicable;
- SD persistence, transactional behaviour and recovery;
- always-available factory AP plus optional AP+STA external networking;
- `plonk-and-play.local` on LAN with IP fallback;
- diagnostics, support download, health/status/version;
- restart/reset/factory recovery and customer firmware update;
- useful basic audio;
- production PSU, connectors/cabling, enclosure, instructions, production test and support process.

Product promise:

> Take P&P out of the box, connect the sensors, switch it on and race — without a PC, router, account or technical setup.

Deferred post-v1: Personal MUG, dedicated TV/projector display, Timed Practice, Stage/Rally/Hill Climb, Drag, wireless detectors, Screen Designer, championships/events, Rex/REU-dependent facilities and other later expansion unless explicitly promoted by Connal through scope control.

## 4. Work packages and closure gates

### WP0 — Baseline, reconciliation and scope
**Status: CLOSED.**

The repository, Stage/WP documents, implementation, tests, evidence, hardware inventory and known gaps have been reconciled. The v1 boundary above is frozen. Historical overlaps/superseded paths are recorded in the reconciliation report.

**Closure gate G0 — PASSED:** every known item is classified DONE, assigned below, deferred or superseded. No new implementation is required merely to close WP0.

---

### WP1 — Racing core
**Status: CLOSED / PASS (G1 accepted).**

**Scope:** Lap, Open Practice, Endurance, lifecycle, start/pause/resume/restart/abandon, all three Lap finish policies, authoritative race state and sporting semantics.

**Already credited:** core architecture/timing/input abstraction; Stage 14A variable entries; Open Practice real-ESP32 acceptance; extensive Lap/Endurance automated coverage.

**Historical closure notes (credited or transferred; not an open WP1 register):**
- reconcile/fix only genuine software defects found by review;
- close production-path semantics for P16/P17/P18: All Stop, Finish Current Lap, Complete Full Race Distance;
- ensure Endurance software acceptance is complete; the related physical
  Browser/Taster proof is credited or deferred to WP5/WP8 as recorded.

**G1 closure:** all racing-core automated/Wokwi/spec acceptance passes; no known racing-core software defect remains. Physical proof is explicitly deferred to WP5 rather than blocking software closure.

### WP2 — Results, records and persistence
**Status: CLOSED / PASS (G2 accepted).**

**Scope:** Results/Details/History, PB, Track Record, Record Era, rolling retention, reconstruction, transactional/failure semantics and authoritative SD ownership.

**Already credited:** WP2A–F software acceptance, versioning, reconstruction, rolling retention, active-detail streaming and failure-injection work.

**Historical closure notes (credited or transferred; not an open WP2 register):**
- classify the historical Lane 2 `1 lap`/missing-time anomaly as legacy or reproduce/fix it;
- verify no unresolved software-only persistence/records obligation remains.

Real-card removal/remount, reboot reconstruction, interruption, coexistence and recovery belong to WP5.

**G2 closure:** software records/persistence matrix is clean; anomaly resolved/classified; no known software-only WP2 defect remains.

### WP3 — Browser, authority and networking
**Status: CLOSED / PASS (G3 accepted).**

**Scope:** normal Browser lifecycle/reconstruction, Race Director lease, spectators, multi-client SSE, KEEP/ALLOW, reconnect/reacquisition, AP+STA and LAN access.

**Already credited:** Scan/CONNECT/AP+STA/LAN/DISCONNECT/reboot reconnect/FORGET/Forget+reboot hardware passes; `plonk-and-play.local` was physically resolved successfully on 23 October and is accepted evidence; no repeat mDNS test is required merely to create an artifact.

**Historical closure notes (credited or transferred; not an open WP3 register):**
- software/automated closure for live multi-client authority semantics;
- retained Race Director identity/restoration path after controller/P&P restart must be software-correct before hardware proof.

**G3 closure:** Browser/authority/network software suite and semantic review pass; networking behaviour remains frozen; no known software defect remains. Live multi-client/reboot authority proof belongs to WP5.

### WP4 — Diagnostics and support
**Status: CLOSED / PASS (G4 accepted).**

**Scope:** production recorder, privacy/redaction boundary, health/status/version, bounded/degraded storage behaviour and P&P → SD artifact → Browser DOWNLOAD support path.

**Already credited:** recorder/export implementation and tests, successful real export, diagnostics Browser route and NDJSON support artifact.

**Historical closure notes (credited or transferred; not an open WP4 register):** software-only privacy/redaction/capacity/recovery/storage-health obligations and support-artifact semantics were covered by the accepted G4 evidence; later physical/qualification obligations remain assigned below.

**G4 closure:** end-to-end simulated/automated support path passes and diagnostic failure cannot affect racing or product authority. Physical SD/degraded-storage proof belongs to WP5.

### GATE A — BASE SOFTWARE FREEZE
WP1–WP4 must all be closed. Archie performs a cross-WP semantic review. Al produces one identified firmware candidate. No known base-software defect may be carried silently into hardware acceptance.

---

### WP5 — Base-system hardware acceptance
**Status: CLOSED / PASS (30 October 2026).** The applicable WP5 base-system
hardware campaign, including the final PSF.28 power-interruption obligation,
has been accepted against the recorded candidate evidence. The display/Taster
items explicitly deferred by dependency remain assigned to WP8 and do not
reopen this WP5 closure. The bounded Diagnostics-backend rollover observation
is a separate follow-up, not a WP5 storage-acceptance blocker.

**Closure disposition:** The applicable base-system campaign is closed against the
recorded accepted evidence. The former checklist is retained by the underlying
acceptance documents, but is not an outstanding WP5 register. The only related
work still visible in the roadmap is explicitly transferred: the deferred
Browser/Taster and Endurance checkpoint belongs to WP8, and recurring resource
and long-run qualification belongs to WP10. The bounded Diagnostics-backend
rollover observation remains a separate follow-up.

**GATE B — BASE PROVEN:** all applicable base hardware acceptance passes on a controlled candidate; failures are fixed and only affected tests plus necessary cumulative checks are rerun; evidence is recorded. The base is then frozen against casual redesign.

---

### WP6 — Recovery, Reset and customer firmware update
**Status: OPEN.** The approved contract, state machine, platform/API slices and
Browser UX correction (`ca70b1232a11476b509ba751e534d09e6c459795`) were accepted
offline. The physical signed firmware validation rejection test is **PARKED**.
Automatic rollback acceptance is **PARKED / NOT PASSED**. These items do not
reopen WP5 and remain the reason G6 is not closed.

Define and implement restart/reset/factory-reset/recovery semantics; customer firmware upload, validation, SD staging, installation, versioning, interrupted/bad-update recovery and customer-data retention.

**G6 closure:** the accepted software slices are ready for integrated hardware
proof, but overall G6 remains OPEN until the parked physical validation and
rollback obligations are resolved.

### WP7 — Track hardware system
**Status: OPEN.** WP7.1 — the detector-provider boundary and deterministic
two-lane health seam — is implemented as a software-only slice in
`fce2b8a`/`7affe55` (including mandatory explicit provider injection and the
Stage 2/3 boundary-oracle updates; no physical detector driver or physical
acceptance claimed).

The slice adds an injectable `DetectorProvider` boundary behind `InputModule`,
retains the deterministic simulated provider, publishes stable role-neutral
timestamped events for two independent sources, and exposes authoritative
missing/fault/replacement status without silently changing detector identity.
Focused evidence is `acceptance/stage7/detector_provider_test.cpp` and its
structural review; the existing Stage 2–9/14A structural/regression guards
remain passing.

WP7.2 — detector readiness integration — is now implemented as a bounded
software-only slice. Race Control readiness is refreshed from the current
normalized Session Definition assignments and the injected provider's
authoritative status: missing, faulted or unreconciled replacement required
detectors block START with the existing `CapabilityUnavailable` reason;
unassigned/optional provider entries do not. Detector identities remain fixed
and no lane is silently reassigned. Focused evidence is
`acceptance/stage7/detector_readiness_test.cpp` and
`acceptance/stage7/run_detector_readiness_test.ps1`. No physical detector
driver or physical acceptance is claimed.

WP7.3 — two-lane Track Setup — is implemented as a bounded software-only
slice. The normal Browser reads `/track/setup` from the injected detector
provider, presents identities and health, and lets the controlling Race
Director request two-lane assignments through the existing `/request/setup`
and Message Bus boundary. Race Control validates duplicate, missing, faulted
and unreconciled assignments before publishing the authoritative proposal;
accepted installation assignments are synchronously written and read-verified
through the existing Store boundary in an A/B Track Configuration record
before the proposal is published; persistence failure returns the existing
`StorageUnavailable` rejection and leaves the prior proposal unchanged.
Focused evidence is
`acceptance/stage7/track_setup_test.cpp`,
`acceptance/stage7/run_track_setup_test.ps1`, and
`acceptance/stage7/track_setup_structural.py`. This remains software-only:
simulated detectors and keyboard-triggered racing are covered, while physical
detector discovery/drivers and physical acceptance remain open.

WP7.3 racer/MUG selection is now implemented as a bounded software-only
extension (4 November 2026). The normal Browser exposes two lane selectors
through the authoritative `/racers` and existing `/request/setup` boundaries;
Race Control rejects duplicate/invalid identities, persists accepted lane
selection through the existing `Memory`/`Store` configuration record, and
reconstructs the latest accepted identities on restart. START freezes those
identities into the existing Session Definition. Focused evidence is
`acceptance/stage7/racer_selection_test.cpp`,
`acceptance/stage7/run_racer_selection_test.ps1`,
`acceptance/stage7/racer_browser_contract.py`, and
`acceptance/stage7/run_racer_browser_contract.ps1`. The existing product
defaults (MUG 1/MUG 2) and persisted internal IDs are selectable; profile
creation, naming/editing and deletion remain outside this slice because no
saved-MUG catalog exists in the current implementation and must not become a
second configuration authority.

The racer-selection persistence boundary was tightened on 4 November 2026.
Race Control now rejects a request that combines detector and racer assignment
masks before either namespace is written. `Memory` stages racer configuration
outside its remembered A/B slots, verifies it, publishes through the existing
metadata boundary, and treats cleanup as non-authoritative housekeeping. A
failed racer change therefore preserves the prior proposal and reconstruction
state even when staging cleanup fails. Focused evidence now also covers staging
write/read-back, publication and cleanup failures, restart after each rejected
change, successful recovery, ordinary settings preservation and explicit
reconstruction of a valid old-format record with MUG 1/MUG 2 defaults.
Evidence remains in `acceptance/stage7/racer_selection_test.cpp` and
`acceptance/stage7/run_racer_selection_test.ps1`; this does not implement the
deferred saved-MUG catalog or profile management.

The first saved-MUG catalog foundation is now implemented as a bounded,
software-only storage slice (4 November 2026). `pp::MugCatalog` is a
controller-owned SD/Store consumer with a dedicated slot namespace (210/211
plus staging 212), versioned/checksummed A/B records, staged read-back
verification, monotonic saved IDs beginning at 3, a 100-entry limit, strict
1–40-character UTF-8 names, rename-without-ID-change and archive-without-ID-
reuse. MUG 1 and MUG 2 remain built-in defaults. Failed staging, publication or
cleanup paths preserve the prior authoritative catalog, and restart
reconstructs the newest committed copy. Focused evidence is
`acceptance/stage7/mug_catalog_test.cpp` and
`acceptance/stage7/run_mug_catalog_test.ps1`. Browser controls, Race Control
catalog membership/selection validation, Guest handling and historical name
freezing remain later slices; this foundation does not change the current
numeric racer-selection path or any History/Record format.

Productise real detection: sensor addressing, lane assignment, health, Track Setup, minimum-lap/detection behaviour, connections/cabling, gantry and 4/8-lane expansion architecture. Final MCP23017/alternative choice is made here, not assumed by WP0.

**G7 closure:** two-lane product-intent detection works reliably; setup/fault/replacement behaviour is defined and tested; expansion architecture is sufficiently proven for the v1 promise.

### WP8 — Local appliance: display, Taster and audio
**Status: FUTURE / NOT STARTED.**

Complete local 7-inch presentation, Taster/local controls, start lights, pause/resume/recovery/AP information, authoritative audio, volume/mute and coexistence with Browser authority.

WP8 is an entry dependency for the deferred physical Taster obligations recorded
under WP5: once the physical display/Taster candidate exists, the mandatory
Stage 14C Browser/Taster checkpoint and Stage 15 Phase 8B Browser/Taster
coexistence acceptance must be executed without changing or weakening those
acceptance criteria.

**G8 closure:** P&P can perform the promised core race experience without phone/PC; Browser and local operation converge correctly; display/audio/control failure behaviour is defined.

### GATE C — COMPLETE APPLIANCE PROVEN
Concentrated hardware integration of WP6–WP8 plus cumulative base behaviour: reset/update failures, real sensors/multiple lanes, SD/power failures, display/Taster, Browser/multiple clients, audio, Wi-Fi, diagnostics and long-running races. Gate closes only when the appliance works as one system.

---

### WP9 — v1 optional expansion decision
This is a **scope gate**, not an invitation to add features. Connal and Archie decide whether any currently deferred display/MUG/race-mode facility is essential to v1.

Default answer is **none**. Anything not explicitly promoted remains post-v1.

**GATE D — FEATURE FREEZE:** final v1 feature list frozen. No Rex/REU, Drag, Rally or other attractive rabbit hole may enter v1 without an explicit scope/cost/schedule decision by Connal.

---

### WP10 — Qualification and release-candidate acceptance
Run the deliberate abuse/soak campaign: repeated races, reboots, AP+STA cycles, router loss/restore, wrong credentials, DHCP changes, SD removal/recovery, power interruption, storage load/wear/throughput, heap/stack/resources, long-duration races, sensor faults, update/reset/recovery and first-use abuse.

Bob may independently review evidence here.

**G10 closure:** qualification matrix passes with no release-blocking defect and no unexplained resource/stability trend.

### WP11 — Production hardware
Finalise PCB/electronics as applicable, PSU, connectors, cables, SD, enclosure, gantry/mounting and labels. Build production-intent unit(s).

**G11 closure:** production-intent unit passes all relevant cumulative acceptance and can be built/tested reproducibly.

### WP12 — Release
Complete instructions, first-start/access experience, firmware/release process, unit production test, support procedure, diagnostic support workflow and novice-user Plonk & Play trial.

**GATE E — P&P v1 RELEASED:** a novice can unpack, connect, power on and race; release package/support path is complete; no open release blocker remains.

## 5. Current project position

**Current activity: WP7 preparation and implementation.**

WP0–WP5 accepted completion evidence remains credited. WP6 is open only for the
explicitly parked signed-validation and automatic-rollback physical obligations;
WP7 is the next active package, while WP8 remains future work.

Immediate technical order is:

1. Resolve the already-identified WP7 sensor/interface and expansion decisions.
2. WP7.1, WP7.2 and WP7.3, plus the saved-MUG catalog storage foundation, are
   implemented and tested in software-only slices; no real-provider or
   physical-detector work is claimed.
3. Select and integrate the real detector hardware, wiring and gantry, then prove
   two-lane product-intent detection and replacement/fault behaviour.
4. Record the 4/8-lane expansion evidence and prepare the later WP8 appliance work.

## 6. Status reporting

When Connal asks **“where are we?”**, the answer must come from this file plus current repository evidence, not memory.

Routine status should contain only:

- current WP/gate;
- what has closed since last report;
- blockers;
- whether Connal must decide/do anything;
- next gate.

Technical detail remains in the underlying specifications, reconciliation, commits and acceptance evidence unless Connal asks for it.
