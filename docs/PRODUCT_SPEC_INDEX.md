# Plonk & Play™ — Product Specification Index

**Status:** Architecture baseline accepted; Stage 15 Browser/Product UX reconciliation in progress  
**Architecture baseline accepted:** 30 September 2026

The governing P&P architecture remains accepted. Later product/UX specifications refine presentation and customer behaviour without silently changing Race Control, Race Engine, Message Bus or timing ownership.

This file identifies where agreed P&P decisions live so later work does not rely on conversation memory alone.

## Governing architecture

- `DESIGN_CONSTITUTION.md` — governing design principles.
- `SYSTEM_REQUIREMENTS.md` — platform requirements and future capability boundaries.
- `SYSTEM_ARCHITECTURE.md` — system responsibilities and architectural boundaries.
- `SYSTEM_LIFECYCLE.md` — lifecycle/recovery responsibilities.
- `RACE_CONTROL_ENGINE_DESIGN.md` — Race Control / Race Engine ownership and boundaries.
- `FIRST_IMPLEMENTATION_BEHAVIOUR.md` — deliberately narrow first useful implementation, including Lap Race finish behaviours.
- `MESSAGE_CONTRACT.md` — Message Bus rules, message types, Session Definition and authoritative Noticeboard behaviour.
- `DEVELOPMENT_WORKFLOW.md` — development/test workflow; infrastructure rather than product architecture.

## Product behaviour specifications

- `BROWSER_UX_SPEC.md` — top-level Stage 15 browser journey and UX ownership contract: Home → Race Setup → START → Racing/Pause → Finish → Results, plus navigation and Settings hierarchy.
- `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md` — detailed browser preparation flows, Results, Race Again, History and records.
- `RACE_SETUP_LANE_ASSIGNMENT_SPEC.md` — Race Entry assignment, drag/rearrange, systematic lane rotation, automatic rotation and numeric-control behaviour.
- `COMMON_DISPLAY_SPEC.md` — common live display, Race Control Pod, ordinary Pause, Results and Yellow/Red display boundaries.
- `PRESENTATION_DESIGN_SPEC.md` — shared P&P visual language, reusable presentation components, purpose-specific displays and responsive hierarchy.
- `RACE_MODES_PRODUCT_SPEC.md` — agreed Lap, Practice, Endurance, Timed Stage and Drag behaviour.
- `PRODUCT_FACILITIES_SPEC.md` — cross-mode facilities including audio/storage, Track Setup principles, Demo/Attract and Settings audience boundaries.
- `CONFIGURATION_AND_IDENTITY_SPEC.md` — Racer identity, optional Cars, Race Entries, Track/Settings configuration and out-of-box principles.
- `PP_TASTER_SPEC.md` — built-in two-sensor/two-lane standalone Taster and browser boundary.

## Deliberately deferred

The following are not to be silently invented during implementation:

- remaining Drag/REU detail beyond committed base behaviour;
- detailed RMS Yellow/Red state machine and power/resume sequencing;
- exact Power Module electrical behaviour;
- exact audio/storage hardware implementation;
- exact dual-beam Drag/REU implementation;
- detailed competition/championship management;
- exact presentation tokens, fonts, icons, dimensions and animation timings;
- detailed persistence/history schema;
- generic Screen Designer/configurable layout engine and exact future advanced-user screen-customisation UX.

Reusable presentation components and purpose-specific layouts are current requirements; the generic future Screen Designer is not a Stage 15 requirement.

## Historical acceptance material

Stage acceptance specifications/evidence record the requirement and implementation baselines used for those stages. Later terminology or UX refinements do not retroactively rewrite those historical acceptance baselines.

## Working rule

A settled product decision should be written into GitHub before design moves far beyond it.

> **GitHub, not chat memory, is the durable source of truth.**
