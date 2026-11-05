# WP7 — Taster fixed touchscreen layout and interaction reference

**Status:** Connal/Gina-approved visual requirements, pending implementation and visual acceptance.
**Scope:** Refinement of the existing 800 × 480 Wokwi Taster. This document specifies presentation and touch interaction only; it does not supersede authoritative Race Control or introduce racing logic.

## Source and precedence

Reviewed reference mock-ups, supplied 5 November 2026 as `taster_wp7_fixed_layouts(1).zip`: `ready.png`, `racing.png`, `paused.png`, `finished.png` (each 800 × 480). The archive also contains `review.png`. The four layouts are **provisional visual references**, not pixel-perfect final authority: the corrections and requirements in this document take precedence over discrepancies in the PNGs.

The approved corrected local `PP_TASTER_SPEC.md` (Al's unpushed documentation commit) remains authoritative for lifecycle and controls; existing P&P Race Control and Browser specifications remain authoritative for countdown, pause/resume, finish and records. The `main` branch's old encoder-based `docs/PP_TASTER_SPEC.md` is obsolete and **conflicts** with this touchscreen decision; do not implement its encoder or direct-restart behaviours. Reconcile after the approved corrected specification is published.

## Layout invariants

- Preserve Gina's original Wokwi visual language and existing MGB/E-Type car artwork, car animations, flowing chequered flag, five starting lights and bottom message strip. Reuse the established P&P logo asset in the permanent top-left position; do not redraw or substitute a new logo.
- All permanent screen elements retain **identical position and dimensions** across READY, RACING and FINISHED. They may change content, disable or hide; no shifting racer rows, timing columns, lights, logo or main action button.
- Keep two stable racer rows and fixed data columns. Use Lane 1 green (original MGB), Lane 2 red (original E-Type); factory labels `LANE 1` and `LANE 2`. SMUG-configured names may replace labels, fitted/clipped within the same fixed name area without moving anything. No Taster name editing.
- One **permanent yellow** main action button, same position and size in READY, RACING and FINISHED: `START` / `HOLD TO PAUSE` / `RACE AGAIN`. The mock-up's grey RACING button is **incorrect** and must be yellow. The pause action requires a continuous **700 ms hold** (release semantics follow the approved Taster specification). `RACE AGAIN` returns to READY with retained setup; a further START is required.
- READY: show LAPS/TIMED selection and directly adjustable 1–999 lap distance or 1–999 minute duration (factory 10 LAPS, persistence per approved spec). On starting, remove these setup controls and reuse **exactly the same region** for GO!, RACING, RACE PAUSED, RACE FINISHED and other authoritative status text. Do not move racer rows or columns.
- Start lights: progressively illuminate five **red** lights; at authoritative GO all five turn **green together**. Do not generate an independent countdown on the display.
- PAUSED: preserve/dim the underlying race screen, show an overlay with `RESUME` and `ABANDON RACE`; abandoning needs a **separate confirmation**. RESUME preserves authoritative race state and follows the existing specified restart sequence. While PAUSED, alternate the five lights once per second between `YOYOY` and `OYOYO` (Y yellow, O off), consistent with Browser behaviour. The overlay is an exception to the otherwise fixed permanent layout; do not reposition underlying content.
- FINISHED: preserve both original racer rows, show results within the same fixed areas, preserve the original flowing chequered-flag animation and display yellow `RACE AGAIN`. Results remain until the user acts.

## Boundaries and verification

- These are UI requirements, **not permission** to change racing authority, finish policy, storage/record semantics, existing animations or unrelated features.
- Wokwi labelled buttons may simulate touchscreen zones; they do **not** validate the physical touchscreen driver or performance.
- Al to map the visual changes onto the existing Wokwi Taster and authoritative P&P interfaces, preserving unrelated changes. Gina/Connal to review actual Wokwi screens and interactions before visual acceptance.
- Acceptance: inspect READY, RACING, PAUSED, FINISHED at 800 × 480; verify invariant positions, yellow button across states, setup/status reuse, lane colours/name fitting, logo asset, red-to-green lights, paused alternating yellow pattern, overlay/confirmation, resume state preservation and RACE AGAIN returning to READY. Validate countdown and pause against authoritative state, not a display-owned timer.
- The supplied PNGs are review references; they have not been added to the GitHub repository in this documentation-only change. Preserve their provenance for implementation handoff.
