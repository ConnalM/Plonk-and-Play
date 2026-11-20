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
- WP8 race audio with an internal speaker, firmware tones and P&P-supplied SD
  speech and effects, as specified under WP8 below;
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

#### Approved audio scope (18 November 2026)

[Product Facilities §3](PRODUCT_FACILITIES_SPEC.md#3-audio-and-removable-storage)
defines the approved v1 audio behaviour, factory settings, Endurance warnings,
priority order and failure handling. The standard internal speaker starts at
60% volume. Essential tones reside in firmware, and P&P-supplied SD packs carry
speech and effects. Launch voices are British male and female, with provision
for future languages. Customer-installed packs are outside scope.

SMUG configures events individually, and the Taster retains preferences in
standalone operation. Destinations are P&P, Browser, Both or Mute. Browser-only
falls back to P&P when standalone, and Mute always applies. Audio preview and
independent speaker/Browser tests are disabled during active races.

WP8 audio implementation and acceptance remain open pending the compliance
review. Acceptance must cover the factory defaults, selected Endurance cues,
priority and stale-queue handling, silent PAUSED lights, restart/reset
persistence, offline tone fallback and diagnostics. Racing must remain
responsive with audio enabled and when media fails.

**Deferred post-v1:** personalised spoken announcements using MUG names.

WP8 is an entry dependency for the deferred physical Taster obligations recorded
under WP5: once the physical display/Taster candidate exists, the mandatory
Stage 14C Browser/Taster checkpoint and Stage 15 Phase 8B Browser/Taster
coexistence acceptance must be executed without changing or weakening those
acceptance criteria.

#### SMUGLAND Browser visual design and acceptance

**Status: OPEN. Owner: Gina (visual design).** Establish the approved
SMUGLAND Browser visual language, screen designs and asset set, then implement
and accept the Browser presentation separately from its existing functional
contracts. Acceptance must cover the approved desktop and supported mobile
layouts, race setup clarity, authority/state presentation, accessibility and
the Browser/Taster coexistence journey. This workstream does not weaken the
existing Browser functional, authority or socket-limit regression obligations.

#### Superseded animation handover — static Taster presentation

This records the superseded animation restart plan. The physical Taster remains
on its stable single-framebuffer static presentation; moving cars and animated
chequered flags remain deferred. Static READY/RACING/PAUSED/FINISHED
presentation, the 450 ms lane brightness pulse (with perimeter-highlight
fallback), and the static winner wreath are not evidence that moving animation
is solved.

**Authoritative design and source.**  Animation is a presentation response to
authoritative lane events; it must never create timing, GO, pause, expiry,
finish or overtime authority.  The common rules are recorded in
`docs/IMPLEMENTATION_SPEC.md` (Lap and finish animation),
`docs/COMMON_DISPLAY_SPEC.md` and `docs/PRESENTATION_DESIGN_SPEC.md`.
`docs/PP_TASTER_SPEC.md` defines the two fixed lane rows, lane-local animation,
and the finishing flag treatment.  The original Wokwi implementation is
`firmware/taster_wokwi/pp-taster-display.chip.c`: `draw_flag_fabric()` (the
continuous waving checker fabric), `draw_lane_car()` (the original artwork,
streaks, 500-pixel journey and quadratic position formula),
`update_lane_animation()` (one lane arms only when its authoritative lap or
finish state changes) and `animation_tick()` (the frame timer).  The physical
port is retained in `firmware/include/pp/wp7_physical_racing_ui.h`:
`drawFlagFabric()`, `drawAnimatedCar()`, `observeAnimationEvents()`,
`drawLaneStatic()` and `drawCurrent()`.  The physical port keeps the original
MGB/E-Type assets and lane independence; `PP_WP7_DISABLE_ANIMATIONS` currently
sets `kAnimationsEnabled=false`, so those paths are compiled for future use but
not used by the accepted physical target.

The physical RGB565/RLE artwork is preserved in
`firmware/include/pp/wp7_ready_artwork.h` (`kWp7Mgb`, `kWp7Etype` and the P&P
logo).  It is the artwork source to reuse; do not regenerate substitute car
sprites while restarting the animation work.

The current physical constants are `kCarStartX=52`,
`kCarTravelPixels=500`, `kAnimationFrameMs=50`,
`kLapAnimationFrames=14` (about 700 ms) and
`kFinishAnimationFrames=18` (about 900 ms).  The Wokwi reference uses seven
ordinary and nine finishing frames at 50 ms.  The difference is deliberate
physical interpolation work, not a change to race rules.  The position formula
and its perceived ease-in/ease-out direction must be re-measured against the
reference before acceptance; comments call it ease-out while earlier physical
observation suggested the opposite.  Do not silently “correct” that formula.

**Observed failure history.**  The original single-framebuffer RGB path showed
visible flicker when timer or READY values changed, and intermittent corruption
where the top of the image appeared at the bottom, with small groups of bad
pixels on horizontal lines.  Large row/full-screen redraws and cache write-back
made the symptom worse.  During animation experiments, unsynchronised drawing
and clearing of the one scanout framebuffer could expose a partially rendered
frame to RGB DMA, producing tearing/flicker and making a 350 ms, seven-frame
journey hard to follow.  A source-built integrated direct-ESP-IDF
double-buffer attempt also produced horizontal rolling and, in one revision, a
blank panel.  The isolated red/blue diagnostic proved that two driver-owned
PSRAM buffers and `esp_lcd_panel_draw_bitmap()` can alternate in a minimal
ESP-IDF 5.5.5 program, but that result did not transfer safely to the complete
Arduino/P&P integration.  Production therefore returned to the known-good
single-framebuffer path; no animation acceptance may rely on the isolated
diagnostic.

**Single-buffer and rendering lessons.**  `firmware/include/pp/board_display_esp32s3.h`
is the hardware boundary.  The validated panel is 800x480 RGB, 12 MHz pixel
clock, DE 41, VSYNC 40, HSYNC 39, PCLK 42, RGB pins R={14,21,47,48,45},
G={9,46,3,8,16,1}, B={15,7,6,5,4}, backlight GPIO2, with the established
20-line bounce buffers (64,000 bytes of internal DMA-capable SRAM in total)
and the full framebuffer in PSRAM.  Drawing must not change these timings or
the bounce-buffer contract.  A moving object must restore its own background
before drawing the next position; stationary timing fields, lane panels,
labels and cars must not be repainted as collateral.  A single RGB framebuffer
has no atomic “frame present” boundary, so keeping dirty rectangles small is a
stability requirement, not merely an optimisation.

The central dirty-region implementation and diagnostics live in
`wp7_physical_racing_ui.h`: `DirtyRegionSet`, `renderSignature()`,
`drawCurrent()`, `reportDirtyRegions()` and `FullFrameReason` (initial draw,
lifecycle transition, welcome transition, dirty-region overflow and unmapped
signature change).  READY signatures were corrected to represent the displayed
local mode/value, preventing a pending-to-accepted acknowledgement from causing
an unnecessary full-frame redraw.  START-button and lap-cell invalidation were
also narrowed.  These fixes are solved for static presentation, but they do not
prove that an animated corridor can be updated safely under RGB scanout.

The corrected signature path is important because the old pending-to-accepted
acknowledgement could look like an unmapped visible change and fall through to
the full-frame safety fallback.  The fallback is still required for genuine
unmapped changes; it must be logged as `unmapped-signature-change` rather than
silently removed.  This distinction is part of the animation handover: a lane
animation may dirty only its lane corridor, while an actual lifecycle or
unmapped presentation change is allowed to request a full frame.

An earlier measurement found that READY persistence was another trigger.  With
normal synchronous NVS writes, visible full-screen jumps occurred at presses 6,
19 and 31 (intervals 13 and 12).  A no-NVS diagnostic ran about 49 adjustments
without those jumps, while still showing occasional local flashes.  The
source-built XIP comparison retained normal NVS persistence and removed the
recurring jumps/corruption in physical testing, while settings survived power
disconnection.  The working XIP configuration is
`CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, Octal PSRAM at 80 MHz,
`CONFIG_LCD_RGB_RESTART_IN_VSYNC=y`, and single-framebuffer recovery mode.
The RGB translation unit requires the reproducible, narrowly scoped `-Os`
workaround for the Windows GCC IRA crash; all other compilation remains at its
normal settings.  XIP and the RGB workaround are stability prerequisites for
future animation experiments, not animation proof.

**Verified artifacts and recovery.**  Keep these outside temporary build
directories and do not overwrite them:

- accepted XIP static baseline:
  `C:\Users\conna\Documents\Plonk-and-Play\WP8-stage2-xip-enabled-20261112\firmware.bin`
  SHA-256 `A647A4355A4984E911E2EB5BBF68D8523FDC37C1B603D78E7F0F64F387E4C52D`;
- read-back recovery image:
  `C:\Users\conna\Documents\Plonk-and-Play\WP7-recovery-readback-20261109\firmware-recovery.bin`
  SHA-256 `C8906CD37C6C1D796C89AFD41C9FDD9A1B9F830D3EBA6DBD44B32FF3B1F31CDD`;
- earlier welcome candidate (not a replacement for the accepted baseline):
  `C:\Users\conna\Documents\Plonk-and-Play\WP7-XIP-welcome-candidate-20261112\firmware.bin`
  SHA-256 `8973C4B62EA413D7E9D1CD1EFF4C005FC214D1B3E275A87E7087B165E76C6B12`;
- latest XIP welcome/startup candidate built from the dirty worktree, not
  physically accepted:
  `C:\Users\conna\Documents\Plonk-and-Play\WP8-wp7-welcome-xip-short-20261112\plonk-and-play.bin`
  SHA-256 `E441FCE63903D6D31040680BA102813326F648FAFA7A2ABEDF831A10C906E1C2`.
- verified WP7 physical-race checkpoint (17 November 2026):
  `C:\Users\conna\Documents\P&P Backups\2026-11-17-WP7-physical-tested\firmware\plonk-and-play.bin`
  SHA-256 `D748ACD92F7F637DA05F37DD6A6B4D1CC85DBAA6D8E9E4390A7D7A2AD31A983E`.
  Physical testing confirmed that laps stop at the configured target, both
  lanes finish correctly, countdown/GO remain steady, cars remain static, and
  only the FINISHED transition produces a visible flash.

- WP7 formatting acceptance (18 November 2026): the physical Taster passed
  the left-aligned active heading, blank pre-GO lane counters, LAPS/ENDURANCE
  counter formats, stationary MM:SS timer layout, and smooth three-phase
  setup adjustment/wraparound checks.  The current display work remains on
  the established XIP, Octal-PSRAM, single-RGB-framebuffer configuration.

- WP8 GO transition acceptance (18 November 2026): the physical Taster passed
  the local dirty-region red-to-green transition with the one-second green
  presentation intact. The subsequent header/control checkpoint below records
  physical acceptance of PAUSE feedback and alternating warning lamps.

- WP8 physical header/control acceptance (18 November 2026): the physical
  Taster accepted the approved 145x55 logo, five centred 60x60 lamp assets,
  the static bezel, the one-second GO green presentation, PAUSE amber
  `RELEASE` feedback after a 700 ms hold, and the 500 ms alternating PAUSED
  yellow/OFF sequence. The accepted flashed application image is
  `3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08`;
  its preserved recovery copy is
  `C:\Users\conna\Documents\P&P Backups\2026-11-18-WP8-centred-lamps\plonk-and-play-3649CEE0.bin`.
  This confirms the static header/control milestone only; it does not close
  deferred Browser/Taster coexistence, audio, animation, real-track or release
  obligations.

The accepted baseline image is the recovery target before every animation
experiment.  The RGB-only `-Os` workaround, XIP configuration and current
single-framebuffer settings must be recorded with any replacement image; a
dirty-worktree build must not be described as a committed release artifact.

**Status at this checkpoint.**

- **SOLVED:** static physical Taster presentation is stable enough for the
  accepted READY/racing workflow; central dirty-region invalidation and fixed
  numeric geometry are in place; redundant START/lap-cell redraws and the
  periodic NVS/cache corruption were addressed; XIP plus normal NVS persistence
  was physically tested and settings survived a power cycle; original car and
  flag source/artwork and per-lane event wiring remain preserved.
- **UNPROVEN:** flicker-free physical moving-car animation on the current
  single-framebuffer panel; whether a lane-local animated corridor can coexist
  with RGB scanout without tearing or underruns; exact physical easing and
  flag-wave fidelity; and any claim that the integrated double-buffer path is
  reliable in the full P&P firmware.
- **DEFERRED:** all moving MGB/E-Type and animated chequered-flag presentation
  on the physical Taster; production double-buffer/direct-ESP-IDF switching;
  and any broader animation polish until the core product is complete.  The
  Wokwi animation remains a reference implementation, not physical evidence.

**Staged restart plan (do not skip gates).**

1. Restore and verify the accepted XIP single-framebuffer baseline and confirm
   static READY/RACING, touch, NVS persistence and the recovery procedure.
2. Enable only one ordinary Lane 1 lap animation on an otherwise static
   RACING screen.  Arm it solely from the authoritative noticeboard lap
   transition; never use a timer to invent a lap.  Keep Lane 2 and all timing
   fields frozen.  Redraw only the Lane 1 car corridor, restoring the reserved
   background before each frame.
3. Instrument every frame with lane, frame index, rectangle(s), render time,
   cache-write bytes/rows, maximum main-loop delay, and any RGB underrun or
   presentation error.  Capture serial output and a physical video/photo.
   Start conservatively with the existing 14-frame/700 ms ordinary-lap
   constants; compare the original Wokwi seven-frame/50 ms sequence only after
   the corridor is stable.
4. Pass a gate of at least 30 repeated authoritative Lane 1 events with no
   tear, top-to-bottom wrap, stationary-field change, touch loss, watchdog or
   RGB error and with bounded render/cache metrics.  If this fails, stop and
   return to the accepted baseline; do not add more animation or switch to
   double buffering speculatively.
5. Only after Lane 1 passes, add Lane 2 with independent state and the same
   gate; then add the finishing-lap ribbon/flag for one lane; finally test the
   other lane finishing while the first lane remains settled.  Each step must
   preserve authoritative timing and the full-race completion policy.

**Reproduction evidence.**  Focused offline checks that passed with the current
   source include:

```
python acceptance/wp7_welcome_startup_test.py
python acceptance/stage8a/taster_structure.py
```

The C++ contract sources are `acceptance/wp8_presentation_contract_test.cpp`,
`acceptance/stage8a_taster_surface_test.cpp`,
`acceptance/stage8a_dirty_region_test.cpp` and
`acceptance/wp7_gt911_safety_test.cpp` (the last includes
`firmware/lib/TAMC_GT911/src`).  They produced, respectively,
`WP8_PRESENTATION_CONTRACT_PASS`, `STAGE8A_TASTER_SURFACE PASS`,
`DIRTY_REGION_TEST_PASS` and `GT911_SAFETY_TEST_PASS` in the existing focused
harness.  The build scaffold is `firmware/wp7_idf_arduino_dbuf` using the
ESP-IDF 5.5.5/Arduino-as-component external route, `sdkconfig.xip`, and
environment switch `PP_WP7_IDF_SINGLE_FB_XIP=1`; compile commands must show
`PP_WP7_SINGLE_FB_RECOVERY=1`, Octal PSRAM, XIP and the RGB-only `-Os` option.
Always run `git diff --check`, `python acceptance/wp8_http_socket_limit_test.py`,
`esptool image_info` (checksum and validation hash), and record the final
SHA-256 before any physical flash. The socket-limit regression is mandatory:
the XIP LWIP socket limit must accommodate the Browser HTTPD request after
HTTPD's internal socket reservation.

The [physical Taster XIP build reference](WP7_XIP_APPLICATION_BUILD_REFERENCE.md)
contains the full commands and validation procedure.

**WP7 application-only build recovery (confirmed 2026-11-18).** Use the
existing `bwp7welcome` build directory, not a newly configured build tree.
Its successful configuration uses the ESP-IDF checkout mounted at `Y:\`, the
ESP-IDF Ninja executable, and the generated response-file toolchain flags in
`bwp7welcome/toolchain/`.  Set `IDF_PATH=Y:\`,
`IDF_TOOLS_PATH=C:\Users\conna\.espressif-wp7`,
`IDF_PYTHON_ENV_PATH=C:\Users\conna\.espressif-wp7\python_env\idf5.5_py3.11_env`,
`PP_WP7_IDF_SINGLE_FB_XIP=1`, and the established Arduino/GFX/TAMC paths
before invoking Ninja.  The build directory's `.ninja_log` and `.ninja_deps`
allow only read/execute to the sandbox group: run Ninja with the approved
external owner-level execution permission so it can compact those files.
This preserves the response-file and compiler-path workaround; do not replace
it with a fresh toolchain setup or a generic CMake build.

For physical correlation, preserve plain serial output rather than relying on
terminal rendering: `[WP7 PHYS] redraw regions=... reason=...` reports the
dirty set and full-frame reason; `[WP8 DIAG] adjust`, `persist begin/end`,
`loop` and `touch` lines correlate adjustment number, NVS duration, render
duration, maximum loop delay and GT911 errors; `[DISPLAY]` reports the
single-frame initialisation/bounce configuration; and `[DBUF]` belongs only to
the disposable double-buffer diagnostic.  A clean boot or callback log is not
animation evidence; a physical frame must be observed with its stationary
content intact.

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

**Current activity: WP8 local-appliance implementation and physical Taster
acceptance; WP7 real hardware remains open.**

**Project-owner decisions (18 November 2026).** Official-record eligibility is
frozen in the authoritative `SessionDefinition` at START. A standalone Taster
race is non-official: it must not enter official History, PB or Track Records;
a later SMUG connection cannot change that eligibility, and simulated-race
classification remains preserved. The approved display description is one full
RGB framebuffer plus 20-line bounce buffers. The isolated issue-review branch
now implements the frozen field, trusted local Taster START provenance and
Race Engine persistence gates, with focused host evidence only; physical and
full regression acceptance remain open.

WP0–WP5 accepted completion evidence remains credited. WP6 retains its parked
signed-validation and automatic-rollback physical obligations. WP7 has bounded
software work but no accepted real detector/track system. WP8 is in progress:
the static Taster header/control milestone is physically accepted, while its
remaining appliance obligations are open.

Immediate technical order is:

1. Complete WP8 software: the static Taster presentation, Browser/Taster
   coexistence, audio, Browser visual design and regression obligations.
   obligations without weakening their acceptance criteria.
2. Complete the [standalone Taster record-eligibility acceptance
   plan](STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md): review the isolated
   implementation, then run the remaining Endurance, lease-transition,
   physical-storage and real-detector evidence before release credit.
3. After WP8 software completion, select and integrate real WP7 detector hardware, wiring and gantry, then prove
   two-lane product-intent detection and replacement/fault behaviour.
4. Execute the shared-configuration/Race Engine/HTTP-socket/WP6 adapter
   regression matrix recorded in the compliance reconciliation.
   The [checkpoint evidence](ARCHIE_CHECKPOINT_REVIEW_2026-11-18.md) records an
   legacy compatibility fix and its host regression evidence. The source
   preserves a historically valid 1,200-lap target and unrelated settings.
   Physical NVS and Browser/Taster coexistence evidence remain open.
   The standalone Taster rejects START above 999 until an explicit local
   distance adjustment is made; Browser-controlled races retain the shared
   positive 32-bit range. Focused host acceptance has passed. Physical
   Browser/Taster coexistence acceptance remains open, and the restriction must
   never be met by overwriting shared configuration.
5. Resolve and regress Race Engine penalty-lap arithmetic: its current
   32-bit `lapTarget() + penalties_` finish calculation can overflow at an
   extreme valid shared lap target. Until corrected, UINT32_MAX is storage and
   Browser-boundary evidence only, not a safe Race Engine target.
6. Establish Gina's SMUGLAND Browser visual design and complete its dedicated
   implementation and acceptance workstream.

## 6. Status reporting

When Connal asks **“where are we?”**, the answer must come from this file plus current repository evidence, not memory.

Routine status should contain only:

- current WP/gate;
- what has closed since last report;
- blockers;
- whether Connal must decide/do anything;
- next gate.

Technical detail remains in the underlying specifications, reconciliation, commits and acceptance evidence unless Connal asks for it.
