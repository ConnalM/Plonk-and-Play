# Plonk & Play™ — Product Specification Index

**Status:** Working source-of-truth map

This file identifies where agreed P&P decisions live so later design work does not rely on conversation memory alone.

## Governing architecture

- `DESIGN_CONSTITUTION.md` — governing design principles.
- `SYSTEM_REQUIREMENTS.md` — platform requirements and future capability boundaries.
- `SYSTEM_ARCHITECTURE.md` — system responsibilities and architectural boundaries.
- `SYSTEM_LIFECYCLE.md` — lifecycle/recovery responsibilities.
- `RACE_CONTROL_ENGINE_DESIGN.md` — Race Control / Race Engine ownership and boundaries.
- `FIRST_IMPLEMENTATION_BEHAVIOUR.md` — deliberately narrow first useful implementation, including the three Lap Race finish behaviours.

## Product behaviour specifications

- `RACE_MODES_PRODUCT_SPEC.md` — agreed Lap, Practice, Endurance, Rally and Drag behaviour.
- `COMMON_DISPLAY_SPEC.md` — common Lap/Endurance/Rally display, personal MUG displays, Results and Yellow/Red display principles.
- `PRODUCT_FACILITIES_SPEC.md` — Power Module, audio/SD, track setup, speed calculations, browser/SC split, solo/multi-user behaviour and option philosophy.

## Deliberately deferred

The following are not yet fully specified and should not be silently invented during implementation:

- detailed Drag screen design;
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
