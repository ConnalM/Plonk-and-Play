# P&P Master Project Plan

**Status:** Project-control baseline — October 2026  
**Purpose:** High-level, tickable plan for taking P&P from the current engineering system to a complete, supportable product. Detailed stage/acceptance documents remain authoritative for product behaviour.

## Development rhythm

P&P work is organised into coherent software/specification batches followed by deliberate real-hardware validation rounds. Do not bounce between Wokwi and physical hardware for ordinary software changes.

Normal flow:

**Frozen docs / product decision → implementation + automated tests → PC/Wokwi integration → semantic sanity review → hardware candidate → concentrated hardware validation**

Real hardware complements simulation and must cumulatively revalidate applicable accepted behaviour.

## Roles

- **Connal — Project Owner:** product scope, priorities, customer behaviour and final approval; physical testing at deliberate hardware checkpoints, not routine firmware test labour.
- **Archie — Project Manager / Senior Architect:** roadmap, dependencies, scope control, architecture, docs-first discipline, acceptance gates and readiness to move from specification to software to hardware.
- **Al — Chief Programmer / Troubleshooter:** repository reality, implementation, builds, automated tests, Wokwi, diagnostics and technical investigation within frozen behaviour.
- **Gina — UX/UI Lead:** Browser and local-display interaction/presentation after underlying behaviour is settled.
- **Bob — Test & Verification:** later independent acceptance/evidence review; not another designer or coder.

## High-level work packages

- [ ] **WP0 — Establish exact current baseline**  
  Owners: Archie + Al + Connal.  
  Workplace: repository/docs + existing hardware evidence.  
  Produce one agreed software and hardware status showing DONE / PARTLY DONE / DESIGNED ONLY / NOT STARTED / DEFERRED. Establish the proposed v1 boundary and dependencies before further implementation.

### Software Batch 1 — Finish the base

- [ ] **WP1 — Complete current P&P racing base**  
  Owners: Archie + Al + Gina.  
  Workplace: specification + PC/Wokwi.  
  Complete/cohere currently supported Lap, Open Practice and Endurance behaviour, Normal Browser lifecycle/presentation, all Lap finish variants, Results semantics, Race Director/multiple-client behaviour and mode-specific automated acceptance.

- [ ] **WP2 — Complete storage and records**  
  Owners: Archie + Al.  
  Workplace: software/Wokwi first.  
  Complete Results/History/detail/PB/Track Record/Record Era/reconstruction/retention and transactional/failure behaviour that can be developed without real hardware.

- [ ] **WP3 — Complete diagnostics and support path**  
  Owners: Archie + Al + Gina where UI is required.  
  Workplace: software/Wokwi.  
  Finish production diagnostic recording, health/status/version information and P&P → SD diagnostic artifact → Browser DOWNLOAD support flow.

### Hardware Round 1 — Prove the base

- [ ] **WP4 — Base-system real-hardware validation**  
  Owners: Connal + Al + Archie.  
  Workplace: physical ESP32-S3 P&P.  
  Continue from evidence already obtained rather than restarting. Validate multi-client/Race Director behaviour, completed Results/History, SD reboot reconstruction, second-race retention, storage failure/recovery, resources/stability and cumulative applicable hardware acceptance.

### Software Batch 2 — Turn the base into an appliance

- [ ] **WP5 — Recovery, Reset and firmware update**  
  Owners: Archie + Al + Gina; Connal approves customer behaviour.  
  Workplace: specification + software/Wokwi first.  
  Define and implement restart/reset/factory-reset/recovery semantics plus customer firmware upload, validation, SD staging, install, interrupted/bad-update recovery, versioning and customer-data retention.

- [ ] **WP6 — Track hardware system**  
  Owners: Archie + Al + Connal.  
  Workplace: software/Wokwi then bench hardware.  
  Turn detector architecture into product track hardware: sensor addressing, lane assignment, health, Track Setup, minimum-lap/detection behaviour, connections, gantry and expansion architecture.

- [ ] **WP7 — Local P&P: Taster, display and audio**  
  Owners: Archie + Gina + Al + Connal.  
  Workplace: software first, then physical hardware.  
  Complete local controls/display/start lights/pause-resume/recovery/AP information and define/implement authoritative useful audio, volume and mute behaviour.

### Hardware Round 2 — Prove the appliance

- [ ] **WP8 — Complete-appliance hardware integration**  
  Owners: Connal + Al + Archie.  
  Workplace: real P&P.  
  Concentrated validation of Reset/recovery, firmware update/failure, real sensors/multiple lanes, SD/power failures, Taster/display, Browser/multiple clients, audio, Wi-Fi, diagnostics download and long-running races.

### Software Batch 3 — Expansion selected for v1

- [ ] **WP9 — Additional displays / MUG**  
  Owners: Archie + Gina + Al.  
  Workplace: software/Wokwi.  
  Implement only the additional display/MUG capabilities selected for v1. Other ideas move to v1.x/v2.

- [ ] **WP10 — Additional race modes**  
  Owners: Archie + Al + Gina.  
  Workplace: specification + software/Wokwi.  
  Implement only additional modes selected for v1 (candidate future areas include Timed Practice, Stage/Rally and Drag). Do not hold v1 hostage to every planned mode.

### Hardware Round 3 — Release candidate

- [ ] **WP11 — Full cumulative v1 hardware acceptance**  
  Owners: Connal + Archie + Al; Bob for independent verification.  
  Workplace: real product hardware.  
  Full applicable acceptance programme plus soak/repeated races/reboots/power/storage/network/sensor/update/reset/first-use abuse. This round is expected to pass rather than serve as exploratory development.

- [ ] **WP12 — Productisation and release**  
  Owners: Connal + Archie + Al + Gina as appropriate.  
  Workplace: mixed.  
  Final enclosure/connectors/cables/PSU/SD/labels/instructions/first-start access/release process/unit production test/support procedure and novice-user Plonk & Play test.

## WP0 hardware status method

For every physical subsystem classify it separately as:

**HAVE IT → BASIC TESTED → INTEGRATED WITH P&P → ACCEPTANCE TESTED**

Minimum hardware inventory to audit:

- ESP32-S3 controller board
- 7-inch 800×480 display/backlight
- flash and PSRAM
- microSD interface/card
- Wi-Fi AP
- speaker/amplifier/audio
- rotary encoder/buttons/Taster
- VL53L0X detector(s)
- intended VL53L4CD detector(s)
- two-lane connections
- MCP23017/IO expansion
- 4/8-lane expansion
- gantry/mounting
- wired sensor cabling
- wireless detector concept
- power supply
- production connectors
- enclosure

Known evidence already obtained must be credited rather than rerun merely to populate the table. WP0 determines exactly what remains unproved.

## Current project position

**NOW: WP0 — establish exact baseline.**

After WP0: **Software Batch 1 (WP1–WP3)**, then return to and complete **Hardware Round 1 (WP4)**, which has already partially occurred.

Do not start new implementation simply because an item is visible later in this plan. Each work package is opened deliberately after its dependencies and exit criteria are agreed.
