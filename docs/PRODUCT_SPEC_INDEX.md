# Plonk & Play™ — Product Specification Index

**Status:** Architecture baseline accepted — implementation in progress; see
`docs/MASTER_PROJECT_PLAN.md` for current WP status
**Baseline accepted:** 30 September 2026

The governing P&P architecture has completed its pre-implementation consistency review. No unresolved architecture blockers remain. Deferred implementation choices and future product detail remain deliberately deferred and do not prevent Stage 1 from beginning.

Changes to the accepted architectural boundaries should now be made deliberately and recorded as architecture changes, rather than emerging accidentally during implementation.

This file identifies where agreed P&P decisions live so later design work does not rely on conversation memory alone.

## Approved documentation structure — 19 November 2026

P&P is one product. The owner has agreed one consolidated design specification
and one consolidated project plan. The Constitution and high-level System
Requirements govern the design; test specifications, engineering reference
material and acceptance evidence remain separate from product design.

The consolidated design will bring architecture, product behaviour, detailed
component design and interfaces together in one document, with sections for
system architecture, Race Control, Race Engine, Inputs, Outputs, communications
and system time, memory/storage, Presentation (including Integrated Display,
Browser and audio), configuration, diagnostics/recovery and physical product.
A requirement has one authoritative home within that document. Cross-references
are preferred to repeated rules.

`MASTER_PROJECT_PLAN.md` is the single development backlog and roadmap for all
work, including current v1 work, later v1 work and post-v1/v2 candidates.
No competing task list or automatic promotion of post-v1 items is permitted.

**Migration status:** the existing documents listed below remain governing until
their content is reconciled, transferred and explicitly superseded. Preserve
approved requirements, source links, implementation obligations and acceptance
evidence; do not treat historical drafts or prototype behaviour as new approval.
No firmware change follows from this documentation decision.

## Governing architecture

- `DESIGN_CONSTITUTION.md` — governing design principles.
- `SYSTEM_REQUIREMENTS.md` — platform requirements and future capability boundaries.
- `SYSTEM_ARCHITECTURE.md` — system responsibilities and architectural boundaries.
- `SYSTEM_LIFECYCLE.md` — lifecycle/recovery responsibilities.
- `RACE_CONTROL_ENGINE_DESIGN.md` — Race Control / Race Engine ownership and boundaries.
- `FIRST_IMPLEMENTATION_BEHAVIOUR.md` — deliberately narrow first useful implementation, including the three Lap Race finish behaviours.
- `MESSAGE_CONTRACT.md` — common P&P Message Bus rules, first concrete Message Types, Session Definition v1 information and authoritative Noticeboard-change behaviour.
- `DEVELOPMENT_WORKFLOW.md` — proven local Work/PlatformIO/Wokwi development and test-bench workflow; development infrastructure only, not product architecture.

## Product behaviour specifications

- `RACE_MODES_PRODUCT_SPEC.md` — agreed Lap, Practice, Endurance, Timed Stage and Drag behaviour.
- `COMMON_DISPLAY_SPEC.md` — common Lap/Endurance/Timed Stage display, personal MUG displays, Results and Yellow/Red display principles.
- `PRESENTATION_DESIGN_SPEC.md` — shared P&P visual language, reusable presentation components, responsive hierarchy and consistency rules across the integrated display and browser clients.
- `PRODUCT_FACILITIES_SPEC.md` — Power Module, audio/SD, track setup, speed calculations, browser/SC split, solo/multi-user behaviour and option philosophy.
- `CONFIGURATION_AND_IDENTITY_SPEC.md` — v1 MUG identity, Guest promotion, optional independent cars, lane assignment, Swap Lanes and remembered setup.

## Deliberately deferred

The following are not yet fully specified and should not be silently invented during implementation:

- remaining Drag/REU detail beyond the committed base Drag staging, tree, timing and results behaviour;
- detailed RMS Yellow/Red state machine and resume sequencing;
- exact Power Module electrical behaviour;
- exact audio/SD hardware implementation;
- exact dual-beam Drag/REU implementation;
- detailed competition/championship management implied by `CONTINUE`;
- exact presentation tokens, fonts, icon set, dimensions and animation timings within the accepted `PRESENTATION_DESIGN_SPEC.md` visual direction;
- detailed persistence/history schema.

When one of these areas is designed, update the appropriate specification or add a new specification and link it here.

## Working rule

A settled product decision should be written into GitHub before the design conversation moves far beyond it.

> **GitHub, not chat memory, is the durable source of truth.**

- `PP_TASTER_SPEC.md` — built-in two-sensor/two-lane standalone Taster, SMUG configuration, MINIMUG operation and browser boundary.

- `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md` — browser Home/mode setup flows, immediate results, rolling History and minimal v1 records.
- `WP6_RECOVERY_RESET_UPDATE_CONTRACT.md` — approved WP6 customer recovery,
  reset, factory-reset and firmware-update contract; physical signed-validation
  and rollback acceptance status is tracked in the master plan.
- `WP6_FAILURE_RECOVERY_MATRIX.md` — deterministic software/Wokwi and later hardware failure/recovery proof matrix for WP6.
