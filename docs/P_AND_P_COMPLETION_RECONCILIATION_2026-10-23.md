# P&P consolidated completion reconciliation

**Date:** 2026-10-23
**Branch:** `browser-product-ux-stage15`
**HEAD at review:** `1fcc63ccb89ddb18f0e9af5be70b6b1037b9fc0b`
**Purpose:** repository-level reconciliation only. This report does not change
product scope, implementation or acceptance requirements.

## Method and evidence boundary

The review covered `docs/MASTER_PROJECT_PLAN.md`, Stage 1–15 design and
acceptance documents, the Persistent Storage Foundation documents, the system
architecture/lifecycle and implementation specifications, the Browser handover,
the investigations and hardware records in `docs/`, the `acceptance/` runners
and evidence, and the current `firmware/include/pp`, `firmware/src`, Browser,
Taster and PlatformIO implementation.

The classifications below distinguish implementation from acceptance. A host,
Wokwi or structural test does not prove the corresponding physical product
behaviour.

## Current WP5 status addendum — 2026-10-28

This addendum supersedes the older Browser/HTTP-SSE and multi-client status
rows below where later accepted evidence exists. It does not reopen any
accepted product behaviour.

- **CLOSED — Browser HTTP/SSE transport and reboot recovery.** The approved
  `230f1ba09465f3751aa9916624b78848f2457925` / `stage8bp1s3sd` candidate
  passed the two-Browser RESET/EN acceptance: fresh sequential
  `/state -> /proposal -> /context`, retained Master/spectator reconstruction,
  two settled SSE generations, bounded client population and no authority,
  race or persistence mutation. Durable evidence is commit
  `4134080afcb92eaa2c5b0d48775d7d4161d71cf2`.
- **CLOSED — P16/P17/P18 physical Lap finish policies.** The accepted Session 1
  evidence records All Stop, Finish Current Lap and Complete Full Race Distance
  physical passes.
- **DEFERRED-BY-DEPENDENCY — Stage 14C Browser/Taster and Phase 8B
  Browser/Taster coexistence.** The physical Taster/display implementation is
  assigned to WP8; the frozen Stage 14C/Phase 8B semantics remain mandatory
  WP8 acceptance and are not WP5 blockers.
- **OPEN — production SD hardware acceptance.** The remaining WP5 SD scope is
  reboot/second-race retention, controlled removal/remount/recovery and
  degraded/unavailable diagnostics (the applicable PSF.27/PSF.31–37 and
  PS15.78/PS15.84/PS15.90–PS15.92 evidence). The intermittent cold-boot mount
  observation remains separately recorded and is not silently converted into
  a firmware change.
- **OPEN — PSF.28.** The frozen requirement for repeated real power
  interruption during active SD persistence, followed by safe reboot/remount/
  reconstruction, remains unexercised.

Accordingly WP5 hardware acceptance remains **OPEN**. The next required
bounded session is the SD recovery/retention and degraded-diagnostics session;
PSF.28 remains the subsequent separate destructive-storage session. No new
physical action is authorized by this documentation update.

## Consolidated requirement matrix

| Area / requirement | Status | Completion area | Evidence or remaining obligation |
|---|---|---|---|
| Core architecture, Message Bus, module boundaries and observation-only diagnostics | DONE | — | Stage 1–7 design/acceptance material, `core.h`, Message Contract and the cumulative regression records. |
| System Time and Relevant Time ordering | DONE | — | Stage 1–9 requirements, Race Engine and timing regression suites. |
| Input Module and detector abstraction | DONE | — | Stage 2–4 acceptance and normal Input Module → Message Bus paths. Physical optical sensors remain a separate hardware item. |
| Race Engine and Race Control authority boundaries | REMAINS FOR V1 | racing core | Stage 8–13 tests and source are present. Production-path acceptance still depends on the open Lap/multi-client runs below. |
| ProposedRaceSetup / SessionDefinition and 1–8 software capacity | DONE | — | Stage 9 and Stage 14A; Stage 14A records automated and real-ESP32 acceptance. `PP_MAX_ENTRIES` is a build capacity, not active-count semantics. |
| Lap Race mechanics, start lights, pause/resume, restart, abandon and Finish Display | REMAINS FOR V1 | racing core | Stage 11–13 and Browser tests exist. The production three-policy and multi-client executions are still open. |
| Lap finish: all stop when winner finishes | REMAINS FOR V1 | racing core | Required by Stage 12/13 and P16; automated coverage exists, production-path execution is unrecorded. |
| Lap finish: winner finishes, others finish current lap | REMAINS FOR V1 | racing core | Required by Stage 12/13 and P17; automated coverage exists, production-path execution is unrecorded. |
| Lap finish: winner finishes, others complete full distance | REMAINS FOR V1 | racing core | Required by Stage 12/13 and P18; automated coverage exists, production-path execution is unrecorded. |
| Open Practice, Practice Summary, Practice Again and Home lifecycle | DONE | racing core | `ACCEPTANCE_TESTS_STAGE_14B.md` records automated, cumulative and real-ESP32 Browser acceptance. |
| Endurance, Stop at Zero and Finish Current Lap | REMAINS FOR V1 | racing core | Stage 14C.1–14C.28 are specified and implemented/tested in software; the required real ESP32 Browser/Taster checkpoint is not recorded as passed. |
| Results, Details, Finish Display and coherent Lap presentation | REMAINS FOR V1 | Results/persistence | Current coherent columns and authoritative timing path are implemented. A prior real result showed one lane as `1 lap` without a time; no evidence has yet closed whether that is legacy data, reconstruction or a current defect. |
| History, History Details, PB, Track Record and Record Era software semantics | REMAINS FOR V1 | Results/persistence | WP2A–F are recorded software-accepted with versioning, reconstruction, rolling retention and active-detail work. Real SD reconstruction and failure evidence remain open. |
| Transactional storage, active/detail streaming and failure isolation | REMAINS FOR V1 | Results/persistence | PSF development and failure-injection suites, WP2F semantic acceptance and production active-detail boundary are present. PSF.26–28 remain hardware obligations. |
| Physical SD owner and shared History/Diagnostics boundary | REMAINS FOR V1 | base hardware acceptance | `physical_sd_owner.h`, PSF.31–37 implementation and the one-owner/shared-mount boot checkpoint exist. Removal/remount, deferred recovery, interruption and coexistence under real load remain. |
| Normal Browser entry, state reconstruction and Results/History routes | REMAINS FOR V1 | Browser/authority/networking | Root entry now serves the current Browser. Results stack-overflow correction passed one real request; full multi-client and persisted-result evidence remains. |
| Race Director lease, spectators, takeover and bounded multi-client SSE | REMAINS FOR V1 | Browser/authority/networking | Bounded four-client, generation/fd-reuse, control-lease and structural tests exist. Live production fan-out, KEEP/ALLOW, reconnect/reacquisition and race continuity are explicitly unexecuted in the WP1 closure record. |
| Race Director identity restoration after controller restart | REMAINS FOR V1 | Browser/authority/networking | Stage 10.12–10.14 require retained Master restoration. Source has delayed restoration after store initialisation, but the last targeted hardware evidence showed authority controls disabled after reboot; a passing targeted run is absent. |
| AP+STA, factory AP, Connect, Reconnect, Disconnect, Forget and LAN Browser access | DONE | Browser/authority/networking | Focused hardware checks pass Scan, Connect, AP+STA, LAN access, Disconnect retention, reboot reconnect, Forget and Forget-plus-reboot. The larger repeated qualification campaign is later qualification. |
| mDNS `plonk-and-play.local` with LAN-IP fallback | REMAINS FOR V1 | Browser/authority/networking | NET15.7 is frozen; `ESPmDNS`, HTTP service advertisement and `/network/status` host reporting are implemented. No dedicated committed hostname-resolution artifact was found in this checkout. |
| Production diagnostics recorder, export and privacy boundary | REMAINS FOR V1 | diagnostics/support | WP3 implementation and export tests exist; real export has succeeded with complete records. PS15.78, PS15.82–PS15.92 still require final physical SD, privacy/redaction, capacity, recovery and storage-health evidence. |
| Diagnostic download/support workflow | REMAINS FOR V1 | diagnostics/support | `/diag/download`, `.txt` NDJSON artifact, host retrieval tooling and Browser route are present. Final support-artifact and degraded-storage acceptance is not closed. |
| Taster software boundary and local authority convergence | REMAINS FOR V1 | local display/Taster/audio | Phase 8A and Taster contract evidence pass. Phase 8B requires the real Browser/Taster concurrent choreography and reconstruction. |
| Display/backlight and local presentation | REMAINS FOR V1 | local display/Taster/audio | ESP32 display hardware has been exercised, but full product Taster/display acceptance and stable final presentation evidence are not closed. |
| Physical audio | REMAINS FOR V1 | local display/Taster/audio | Basic useful audio is inside the agreed v1 promise, but the physical speaker/audio hardware was not connected in the WP0 inventory and no acceptance exists. |
| Track Setup and real optical lap detection | REMAINS FOR V1 | track hardware | The software abstractions exist; the recorded physical VL53 modules were not connected and sensor/gantry/cabling hardware was unavailable. WP6 remains open. |
| Reset/factory recovery and customer firmware update | REMAINS FOR V1 | recovery/update | These are explicitly WP5 work packages and are not implemented/accepted as a complete customer workflow. |
| Production PSU, connectors/cabling and enclosure | REMAINS FOR V1 | production hardware | WP0 inventory records these as unavailable. WP8/WP12 remain open. |
| Repeated network, SD, reboot, power-loss and long-run campaigns | REMAINS FOR V1 | qualification | These are qualification evidence, not reasons to redesign the current AP+STA or storage architecture. |
| Personal MUG, dedicated TV/projector, Timed Practice, Stage/Rally, Drag, wireless detectors, Screen Designer and championships/events | DEFERRED POST-v1 | — | The WP0 boundary and the mode/product specifications retain these as later facilities. They are not current completion blockers. |
| REU/dual-beam and other advanced Drag hardware | DEFERRED POST-v1 | — | `RACE_MODES_PRODUCT_SPEC.md` explicitly parks REU-dependent measurements and future dual-beam behaviour. |

## Requirements between the old Stage plan and WP0–WP12

The following are real obligations but were previously split across several
plans:

1. Stage 10 live multi-client authority execution is a WP1 exit requirement,
   while its physical validation belongs in WP4.
2. Stage 12/13 production Lap finish and Results/History checks are WP1 exits;
   their SD reboot/recovery portion belongs in WP4 after WP2 software closure.
3. Stage 14C Endurance is a current supported racing mode, but its final human
   checkpoint is not explicitly reflected as a separate WP1 closure row.
4. Phase 8B combines Taster, display, SD, Browser, resources and failure
   behaviour; the relevant work is distributed between WP4, WP6 and WP7.
5. Stage 15 diagnostics final gates overlap WP3 software completion and WP4
   physical SD validation. The master plan correctly treats the software as
   review-complete while retaining physical acceptance debt.
6. The Stage 15 acceptance document still contains historical wording saying
   WP3B production integration “has not started”, while the master plan and
   current source record that implementation as complete for review. That is
   stale project-control wording, not a newly discovered product requirement.
7. Older standalone SD probes and legacy Browser `/normal` entry assumptions
   are superseded by the P&P-owned SD boundary and current root Browser entry;
   they must not be counted as separate product paths.

The following historical planning artifacts or paths are **SUPERSEDED**, not
additional work:

- the overlapping Stage/WP gap lists as independent execution plans;
- the old Stage 15 wording that treated production recorder integration as
  not started, where the WP3 implementation record and current source now show
  it implemented for review;
- standalone Phase 8 SD probe behaviour as a product storage path;
- the legacy `/normal` entry requirement after the root Browser correction;
- stale structural checks audited as test-maintenance debt, including the old
  `masterFingerprint_` and exact-text committed-GO checks.

## Evidence and acceptance limitations

The repository contains extensive generated Stage 1–14C and Stage 15 evidence,
host/Wokwi tests, source-manifest checks, storage failure-injection tests,
Browser contract tests and real-S3 logs. Those records are useful evidence but
do not substitute for the hardware obligations marked Hardware in the frozen
acceptance documents.

No new test was run for this reconciliation, and no evidence was invalidated
solely because it predates this report.

## Local-only working-tree state

The current checkout has **256 dirty/untracked entries** that are not visible
to Archie through GitHub:

- approximately **245 acceptance evidence/source snapshots**, manifests,
  serial captures and generated reports under `acceptance/**/evidence/`;
- generated acceptance executables and Python cache directories;
- firmware build logs, `firmware/logs/device-monitor-*.log`, `.vscode` data and
  other disposable build output;
- an empty untracked `firmware/embed_normal.py` and other local measurement
  logs requiring review before any cleanup.

There are no direct dirty product files under `docs/`, `firmware/include/pp`,
`firmware/src` or the current tracked Browser source. This report deliberately
does not stage, clean, or reinterpret those local files.

## Recommended completion order

1. Resolve or formally classify the known historical Results missing-time
   anomaly.
2. Execute production P16/P17/P18 Lap acceptance.
3. Execute live multi-client/lease acceptance and the targeted retained-Master
   reboot test.
4. Run the real Endurance and Browser/Taster checkpoint.
5. Complete the real SD persistence, recovery and diagnostics checkpoint.
6. Close the resulting evidence as the current P&P core, then run repeated
   qualification and the separate WP5–WP12 productisation work.

This report is a reconciliation record, not a new product-scope decision.
