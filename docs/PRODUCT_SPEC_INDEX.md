# Plonk & Play™ — Product Specification Index

**Status:** Architecture baseline accepted — ready for Stage 1 implementation  
**Baseline accepted:** 30 September 2026

The governing P&P architecture has completed its pre-implementation consistency review. No unresolved architecture blockers remain. Deferred implementation choices and future product detail remain deliberately deferred and do not prevent Stage 1 from beginning.

Changes to the accepted architectural boundaries should now be made deliberately and recorded as architecture changes, rather than emerging accidentally during implementation.

This file identifies where agreed P&P decisions live so later design work does not rely on conversation memory alone.

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
- detailed visual styling/animation;
- detailed persistence/history schema.

When one of these areas is designed, update the appropriate specification or add a new specification and link it here.

## Working rule

A settled product decision should be written into GitHub before the design conversation moves far beyond it.

> **GitHub, not chat memory, is the durable source of truth.**

- `PP_TASTER_SPEC.md` — built-in two-sensor/two-lane standalone Taster, SMUG configuration, MINIMUG operation and browser boundary.

- `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md` — browser Home/mode setup flows, immediate results, rolling History and minimal v1 records.
