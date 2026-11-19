# P&P Consolidated Completion Plan

**Status:** Authoritative project-control plan — reconciled checkpoint 19 November 2026
**Purpose:** One roadmap and backlog for P&P: current v1 work, later v1 work and post-v1/v2 candidates. Current release delivery remains the priority.

This file supersedes the old Stage plan, the earlier WP0–WP12 execution plan and ad-hoc gap lists **as project roadmaps**. Their requirements are now governed by the [single P&P Design Specification](PP_DESIGN_SPEC.md); earlier revisions and dated evidence remain historical references. Nothing already proved is to be rerun merely because this plan is newer.

The detailed reconciliation that feeds this plan is `docs/P_AND_P_COMPLETION_RECONCILIATION_2026-10-23.md`.

## 1. Project-control rules

### One-plan rule
Every known requirement or unresolved obligation must have exactly one disposition:

- **DONE** — sufficient evidence exists;
- **CURRENT/LATER WP** — required for v1 and assigned here;
- **DEFERRED POST-v1** — deliberately outside v1;
- **SUPERSEDED** — historical path/wording no longer applicable.

No separate Stage/WP/gap list may become a competing execution plan. The owner-approved documentation structure (19 November 2026) requires all outstanding work, including later-v1 obligations and v2 candidates, to have a single home in this plan. Record release disposition, WP/dependency, priority, owner and status for each item as applicable; reference the governing design and acceptance criteria rather than restating them. The deferred post-v1 register below is not a commitment to ship any particular v2 feature.

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
- complete Race Entry reassignment and manual/automatic lane rotation, including optional Car identity;
- real optical lap detection and Track Setup;
- Results, Finish Display, History/Details, PB, Track Record and Record Era where applicable;
- SD persistence, transactional behaviour and recovery;
- always-available factory AP plus optional AP+STA external networking;
- `plonk-and-play.local` on LAN with IP fallback;
- diagnostics, support download, health/status/version;
- restart/reset/factory recovery and customer firmware update;
- WP8 race audio with an internal speaker, firmware tones and P&P-supplied SD
  speech and effects, as specified in the [shared audio design](PP_DESIGN_SPEC.md#product-facilities-spec--3-audio-and-removable-storage);
- production PSU, connectors/cabling, enclosure, instructions, production test and support process.

Product promise:

> Take P&P out of the box, connect the sensors, switch it on and race — without a PC, router, account or technical setup.

Post-v1 candidates are recorded in the single backlog under §7; none enter v1 unless explicitly promoted by the Project Owner through scope control.

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
`acceptance/stage7/run_racer_browser_contract.ps1`.

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
reconstructs the newest committed copy. A failure reading either authoritative
slot now fails closed rather than commissioning the other snapshot; an
explicit recovery reload reconstructs the committed catalog once both reads
recover.

Saved-MUG catalog integration is now implemented as a bounded software-only
Browser/controller slice (4 November 2026). The controlling Race Director can
create a named MUG from Race Setup through the controller-authoritative
`/request/mug/create` boundary; `/racers` lists built-in and active saved
profiles with their actual names and permanent IDs; remembered unknown or
archived identities remain visible only as an explicit reconciliation state,
never as ordinary selectable choices. Lane selection continues through
`/request/setup`, with Race Control validating catalog membership,
archived/unknown IDs and duplicate assignments before persistence and proposal
publication. At START the current permanent ID and display name are copied into
the authoritative Session Definition, then into the sealed result. Versioned
History records persist that frozen identity; the new wire version remains
backward-readable for older ID-only records without consulting the current
catalog. Results, Details and History therefore retain the name that was
actually raced even after a later rename or archive. The catalog is reloaded
after an authorised storage recovery, and restart continues to use the existing
proposal, Memory, Session Definition and History boundaries. Evidence is
`acceptance/stage7/mug_catalog_integration_test.cpp`,
`acceptance/stage7/frozen_mug_history_test.cpp`,
`acceptance/stage7/racer_browser_contract.py`, and the catalog/selection tests.
Rename/archive Browser controls, Guest handling and historical name freezing
outside this transaction remain deferred; no Race Engine or Message Bus
architecture was changed.

The complete keyboard/simulated two-lane racing journey is now implemented as
a bounded development-only software slice (4 November 2026). A controller-owned
simulated-session arm flag is frozen into the Session Definition; Race Engine
continues to run the normal countdown, timing, live-gap, sealed-result and
Race Again paths, while the authoritative eligibility boundary suppresses
History, Personal Best and Track Record writes for the simulated session. The
dedicated `stage8bp1s3sd-simulated-racing` environment exposes clearly labelled
TEST ONLY keyboard/Lane 1/Lane 2 controls; the normal `stage8bp1s3sd` Browser
payload contains none of those controls. Focused evidence is
`acceptance/stage8/simulated_racing_journey_test.cpp`,
`acceptance/stage8/run_simulated_racing_journey.ps1`, and
`acceptance/stage8/simulated_racing_browser_contract.py`. This is software
evidence only: it is not physical detector acceptance, Demo, Taster or WP8
work.

Productise real detection: sensor addressing, lane assignment, health, Track Setup, minimum-lap/detection behaviour, connections/cabling, gantry and 4/8-lane expansion architecture. Final MCP23017/alternative choice is made here, not assumed by WP0.

**G7 closure:** two-lane product-intent detection works reliably; setup/fault/replacement behaviour is defined and tested; expansion architecture is sufficiently proven for the v1 promise.

### WP8 — Local appliance: display, Taster and audio
**Status: IN PROGRESS.**  WP8 physical Taster work is active; it is not a
project-wide closure claim.  WP7 real detector hardware/system acceptance also
remains OPEN.

Complete local 7-inch presentation, Taster/local controls, start lights, pause/resume/recovery/AP information, authoritative audio, volume/mute and coexistence with Browser authority.

#### Outstanding display and audio implementation

Implement and accept the [Integrated Display configuration/restoration](PP_DESIGN_SPEC.md#pp-taster-spec--smug-configurable-taster) and [shared audio contract](PP_DESIGN_SPEC.md#product-facilities-spec--3-audio-and-removable-storage). Acceptance includes factory settings, independent warning cues, priority/stale-queue handling, silent PAUSED lamps, restart/reset retention, offline fallback, diagnostics and responsive racing under media failure. Personalised spoken names remain in §7.

WP8 is an entry dependency for the deferred physical Taster obligations recorded
under WP5: once the physical display/Taster candidate exists, the mandatory
Stage 14C Browser/Taster checkpoint and Stage 15 Phase 8B Browser/Taster
coexistence acceptance must be executed without changing or weakening those
acceptance criteria.

#### Browser visual design and acceptance

**DESIGN APPROVED SUBJECT TO TESTING — 19 November 2026.** Gina's design handover and Archie source review are complete. [V18 approved design baseline](https://github.com/ConnalM/Plonk-and-Play/blob/ec46ff5b8df07d1477fa40543423509c778a22d1/design/smugland/PP_Browser_v18_navigation.html); [Archie review](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7012907261). All three V16 corrections present in V17; V18 script exactly matches V17 with cosmetic navigation icons. Gina reports 42 focused V17 assertions and V18 responsive checks passing, not independently rerun by Archie. Representative fixtures only; production implementation, authority/timing/persistence and acceptance remain open. No new design expansion; implementation and testing may identify defects or necessary refinements for review.

#### Physical display milestone and deferrals

Accepted static header/control milestone (18 November), not WP8 closure. Preserved application SHA-256 `3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08`; recovery copy `C:\Users\conna\Documents\P&P Backups\2026-11-18-WP8-centred-lamps\plonk-and-play-3649CEE0.bin`. [Design constraints](PP_DESIGN_SPEC.md#physical-integrated-display-constraints); [complete earlier evidence, artifact hashes, investigation, source locations and animation procedure](https://github.com/ConnalM/Plonk-and-Play/blob/13662fbc5e42fbc83204b7662643a2c43fe8fefb/docs/MASTER_PROJECT_PLAN.md#superseded-animation-handover--static-taster-presentation). No physical animation, audio, coexistence or real detector acceptance inferred.

Moving cars/flags, physical double buffering and animation polish are deferred until core completion; WP9 decides scope. No new animation experiment now.

Use [XIP build reference](WP7_XIP_APPLICATION_BUILD_REFERENCE.md) and current PR evidence. The original proven tree/image remain protected; separate isolated XIP configuration and full app build passed on 19 November (see §5). Physical and real Browser acceptance remain open.

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

## 5. Current project position and active work

**WP8 local appliance in progress; WP7 real hardware OPEN.** WP0–WP5 completion evidence remains credited. WP6 physical signed validation/rollback and WP10 qualification remain later gates. The static header/control milestone is accepted; full appliance acceptance is not.

### 5.1 Current v1 tasks

| ID / WP | Task / owner | Status and next evidence |
|---|---|---|
| WP8-01 / WP8 | Physical UI/Noticeboard mismatch and isolated XIP build — Archie/Al | PASSED source correction, focused timing regression and candidate build at `9f78657`; exact tree matches Archie's prepared correction. Reported image validation passed; SHA-256 `4A01F0EBE45569719E3282BC63292FACA76C453894E0E7405635F659D5A38812`, image size 0x23fa00, 64% app partition free. XIP/80 MHz Octal PSRAM/single framebuffer/animations disabled confirmed. [Evidence](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7012016540). Candidate and accepted recovery image preserved separately and rehashed; [local artifact and proposed checkpoint](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7012216516). Cloud copy unverified; physical acceptance remains WP8/Gate C. |
| WP8-02 / WP8+WP3 | Record eligibility and Browser authority — Archie/Al | Reported host record/lease matrices, concurrent authority/Stage15 lease, 3500-lap detail tests PASS. Real HTTP/SSE acquisition, transfer, reconnect and cross-task timing/lifetime validation OPEN; Stage13/15 Python harnesses syntax-only. Credit reported Practice summary, active-detail recovery, shared SD recovery PASS. Credit the additional physical Noticeboard timing matrix (normal and Stage13 variants), presentation, dirty-region and Taster-surface checks. Actual controller evidence still required. Archie prepared corrected harnesses on separate branch at `4583ada` ([instructions](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7013575605)); syntax and real execution remain open. The tracked XIP build does not define PP_SIMULATED_RACING_TEST, which registers the simulation-arm callback; confirm actual candidate flags. Do not inject synthetic Browser laps in the preserved candidate without verified simulation arming. Separately authorised arm-enabled acceptance build or real-detector procedure required. [Preflight](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7012839077). No merge/flash. [PR #3 details](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7007520404). |
| WP8-03 / WP8 | Display setup and shared audio — Archie/Al | OPEN: implement and accept shared remembered settings/restoration and sound. |
| WP8-04 / WP8 | Browser design / implementation — Gina (design complete), Archie/Al | V18 owner-approved subject to testing; production implementation and acceptance OPEN. Correct issues identified by testing through review; no new scope. |
| WP8-05 / WP1+WP8 | Shared configuration/Race Engine — Archie/Al | Legacy 1200-lap and >999 local adjustment host evidence passes. Physical NVS and coexistence remain open. Fix/regress lapTarget + penalties overflow; UINT32_MAX is currently storage/Browser boundary evidence only, not safe Race Engine target. |
| WP8-06 / WP3+WP6 | HTTP socket and WP6 regressions — Al | Prior source/config and WP6 adapter host tests pass; live saturation/reconnect/coexistence and physical signed update/rollback remain open. |
| DOC-01 / Support | Documentation — Archie | 22 design documents retired; 48 initial consistency corrections plus follow-up removal of obsolete phase planning/status, direct cross-references and deferred scope reconciliation. Internal anchors validated. Verified 454 unique legacy anchors, all 453 retirement destination links and main-document relative links, retained the overlapping PR #3 acceptance matrix unchanged and preserved its additional evidence. PR #4 is documentation-only; integration awaits coordinated resolution of the two overlapping files (this plan and the standalone eligibility acceptance reference) when PR #3 is ready. Do not alter the tested firmware branch. Further semantic review is incremental; no blanket claim that every old paragraph is reconciled. |

Evidence: [PR #3](https://github.com/ConnalM/Plonk-and-Play/pull/3), [issue #2](https://github.com/ConnalM/Plonk-and-Play/issues/2), [18 November checkpoint](ARCHIE_CHECKPOINT_REVIEW_2026-11-18.md) and [compliance reconciliation](ARCHIE_COMPLIANCE_RECONCILIATION_2026-11-18.md). Do not rerun credited tests without relevant change; no flash without separately authorised candidate.

### 5.2 Later v1 tasks — deferred by dependency, not abandoned

| WP | Outstanding work |
|---|---|
| WP7 | Integrate real optical detectors, wiring, gantry, Track Setup, replacement/fault handling, two-lane product-intent detection and expansion architecture. |
| WP6 | Physical signed firmware validation/automatic rollback. |
| WP8 / Gate C | Concentrated Browser/Integrated Display coexistence, Stage14C and Stage15 Phase8B, official-record isolation/reconnect/power-cycle, NVS, physical storage, audio and local operation. |
| WP9 | Owner scope gate; optional features excluded unless explicitly promoted. |
| WP10 | Long-run qualification/abuse, network/SD/power/resource and diagnostic rollover follow-up, detector faults, independent review. |
| WP11 | Production PSU, electronics, cables, connectors, enclosure, gantry and reproducible tests. |
| WP12 | Instructions, novice-user trial, production/release/support process. |

## 6. Status reporting

When Connal asks **“where are we?”**, the answer must come from this file plus current repository evidence, not memory.

Routine status should contain only:

- current WP/gate;
- what has closed since last report;
- blockers;
- whether Connal must decide/do anything;
- next gate.

Technical detail remains in the underlying specifications, reconciliation, commits and acceptance evidence unless Connal asks for it.

## 7. Post-v1 / v2 candidate backlog

These entries preserve the existing deferred scope decisions. They are not a
separate plan or a commitment to a v2 release feature set. Prioritisation and
design approval remain outstanding; no entry may be promoted into v1 without
the Project Owner's explicit approval.

| Candidate | Disposition |
|---|---|
| Personal MUG displays | Post-v1 |
| Dedicated TV/projector display | Post-v1 |
| Timed Practice | Post-v1 |
| Stage/Rally/Hill Climb | Post-v1 |
| Drag | Post-v1 |
| Wireless detectors | Post-v1 |
| Screen Designer | Post-v1 |
| Championships/events | Post-v1 |
| Rex/REU-dependent facilities | Post-v1 |
| Personalised spoken announcements using racer names | Post-v1 |
| Named Race Setup presets / Save Setup and Load Setup | Post-v1 candidate if real use warrants it |
| Richer Racer/Car profiles, long-term statistics and car-specific records | Post-v1 candidates; stable identities retained in v1 |
| Other later expansion | Post-v1 unless explicitly promoted |

### 7.1 Previously deferred/unallocated design decisions

| Decision | Disposition |
|---|---|
| Exact controller hardware, electronics, speaker/amplifier, power and physical product | Required v1 under WP7/WP8/WP11 as applicable |
| Audio/SD implementation | WP8-03; required v1 audio and existing storage foundation |
| Persistence/history schema | Existing frozen storage design controls; outstanding compatibility and qualification in WP8-05/WP10 |
| Visual tokens, fonts, responsive design | WP8-04 for v1 |
| Physical moving cars, chequered flags, double-buffer experiments, animation polish | Deferred until core complete; WP9 scope decision |
| Power Module electrical design and RMS Yellow/Red/resume/track-power detail | Unallocated optional scope; WP9 |
| Attract hardware/artwork/sequence/wake-exit and local/Browser scope | Unallocated; WP9 |
| Automatic Demo Race virtual-car timing, physical detection during Demo, detailed test controls | Unallocated; WP9, preserve non-official Demo classification |
| QR connection aid | Later optional enhancement |
| Drag dual-beam/REU design | Post-v1 |
| Championship management including CONTINUE | Post-v1 |
| Optional fuel/pit/refuelling, tyres/weather and sector/trap capabilities | Future candidates, no v1 commitment |
| Customer-installed audio packs | Outside approved scope, no automatic promotion |

Residual reconciliation is DOC-01; old stage deferrals are not automatically new tasks. Credit later acceptance and preserve existing scope. This register is not a promise about the contents of v2.
