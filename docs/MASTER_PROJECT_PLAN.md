# P&P Consolidated Completion Plan

**Status:** Authoritative project-control plan — 23 October 2026  
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
**Scope:** Lap, Open Practice, Endurance, lifecycle, start/pause/resume/restart/abandon, all three Lap finish policies, authoritative race state and sporting semantics.

**Already credited:** core architecture/timing/input abstraction; Stage 14A variable entries; Open Practice real-ESP32 acceptance; extensive Lap/Endurance automated coverage.

**Remaining before closure:**
- reconcile/fix only genuine software defects found by review;
- close production-path semantics for P16/P17/P18: All Stop, Finish Current Lap, Complete Full Race Distance;
- ensure Endurance software acceptance is complete; physical Browser/Taster proof belongs to WP5.

**G1 closure:** all racing-core automated/Wokwi/spec acceptance passes; no known racing-core software defect remains. Physical proof is explicitly deferred to WP5 rather than blocking software closure.

### WP2 — Results, records and persistence
**Scope:** Results/Details/History, PB, Track Record, Record Era, rolling retention, reconstruction, transactional/failure semantics and authoritative SD ownership.

**Already credited:** WP2A–F software acceptance, versioning, reconstruction, rolling retention, active-detail streaming and failure-injection work.

**Remaining before closure:**
- classify the historical Lane 2 `1 lap`/missing-time anomaly as legacy or reproduce/fix it;
- verify no unresolved software-only persistence/records obligation remains.

Real-card removal/remount, reboot reconstruction, interruption, coexistence and recovery belong to WP5.

**G2 closure:** software records/persistence matrix is clean; anomaly resolved/classified; no known software-only WP2 defect remains.

### WP3 — Browser, authority and networking
**Scope:** normal Browser lifecycle/reconstruction, Race Director lease, spectators, multi-client SSE, bounded HTTP/SSE transport isolation, KEEP/ALLOW, reconnect/reacquisition, AP+STA and LAN access.

**Already credited:** Scan/CONNECT/AP+STA/LAN/DISCONNECT/reboot reconnect/FORGET/Forget+reboot hardware passes; `plonk-and-play.local` was physically resolved successfully on 23 October and is accepted evidence; no repeat mDNS test is required merely to create an artifact.

**Remaining before closure:**
- software/automated closure for live multi-client authority semantics;
- retained Race Director identity/restoration path after controller/P&P restart must be software-correct before hardware proof.
- implement and verify the bounded HTTP/SSE isolation contract: stalled page/SSE
  work cannot block ordinary HTTP or another healthy SSE client, while fresh
  state/proposal/context reconstruction and existing authority semantics remain
  unchanged.

**G3 closure:** Browser/authority/network software suite and semantic review pass; networking behaviour remains frozen; no known software defect remains. Live multi-client/reboot authority proof belongs to WP5.

### WP4 — Diagnostics and support
**Scope:** production recorder, privacy/redaction boundary, health/status/version, bounded/degraded storage behaviour and P&P → SD artifact → Browser DOWNLOAD support path.

**Already credited:** recorder/export implementation and tests, successful real export, diagnostics Browser route and NDJSON support artifact.

**Remaining before closure:** software-only privacy/redaction/capacity/recovery/storage-health obligations and support-artifact semantics.

**G4 closure:** end-to-end simulated/automated support path passes and diagnostic failure cannot affect racing or product authority. Physical SD/degraded-storage proof belongs to WP5.

### GATE A — BASE SOFTWARE FREEZE
WP1–WP4 must all be closed. Archie performs a cross-WP semantic review. Al produces one identified firmware candidate. No known base-software defect may be carried silently into hardware acceptance.

---

### WP5 — Base-system hardware acceptance
**Purpose:** one concentrated physical campaign against the Gate-A candidate, crediting all existing evidence.

Must close the remaining physical obligations for the base, including:
- all three Lap finish policies with fresh Results/Details/History inspection (P16 already physically satisfied; P17 Finish Current Lap and P18 Complete Full Race Distance passed physically in Session 1 on candidate `031834d`);
- multi-Browser controller/spectator fan-out, KEEP/ALLOW, reconnect/reacquisition and race continuity;
- retained Race Director restoration after reboot;
- Endurance physical checkpoint *(DEFERRED-BY-DEPENDENCY to WP8: the physical display/Taster implementation is assigned to WP8 and no pre-WP8 physical Taster exists; the Stage 14C criterion remains mandatory and is not waived)*;
- Browser/Taster coexistence required at this base checkpoint *(DEFERRED-BY-DEPENDENCY to WP8 for the same reason; the Phase 8B criterion remains mandatory and is not waived)*;
- fresh Results check sufficient to close the historical missing-time concern if not already classified;
- SD reboot reconstruction, second-race retention, removal/remount, failure/recovery and interruption obligations applicable here;
- diagnostics download/degraded-storage physical path;
- resource/stability checks appropriate to this candidate.

**GATE B — BASE PROVEN:** all applicable base hardware acceptance passes on a controlled candidate; failures are fixed and only affected tests plus necessary cumulative checks are rerun; evidence is recorded. The base is then frozen against casual redesign.

---

### WP6 — Recovery, Reset and customer firmware update
Define and implement restart/reset/factory-reset/recovery semantics; customer firmware upload, validation, SD staging, installation, versioning, interrupted/bad-update recovery and customer-data retention.

**G6 closure:** automated failure/recovery matrix passes and the candidate is ready for integrated hardware proof.

### WP7 — Track hardware system
Productise real detection: sensor addressing, lane assignment, health, Track Setup, minimum-lap/detection behaviour, connections/cabling, gantry and 4/8-lane expansion architecture. Final MCP23017/alternative choice is made here, not assumed by WP0.

**G7 closure:** two-lane product-intent detection works reliably; setup/fault/replacement behaviour is defined and tested; expansion architecture is sufficiently proven for the v1 promise.

### WP8 — Local appliance: display, Taster and audio
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

**Current activity: WP1–WP4 software closure against Gates G1–G4.**

WP0 is closed. Much of WP1–WP4 is already implemented and accepted; these packages are closure reviews, not rewrites. Connal should not be asked for routine physical testing until Gate A produces the deliberate WP5 hardware candidate.

Immediate technical order is:

1. Archie/Al close WP1 software obligations.
2. Close WP2 anomaly/software obligations.
3. Close WP3 authority software obligations while retaining the already-passed networking evidence.
4. Close WP4 software-only diagnostics obligations.
5. Archie performs Gate A review; Al produces the hardware candidate.
6. Connal performs the bounded WP5 physical campaign one focused test at a time.

## 6. Status reporting

When Connal asks **“where are we?”**, the answer must come from this file plus current repository evidence, not memory.

Routine status should contain only:

- current WP/gate;
- what has closed since last report;
- blockers;
- whether Connal must decide/do anything;
- next gate.

Technical detail remains in the underlying specifications, reconciliation, commits and acceptance evidence unless Connal asks for it.
