# Plonk & Play™ — Browser UX Specification

**Status:** Stage 15 product UX baseline candidate  
**Purpose:** Top-level browser journey and ownership contract. Specialist specifications provide detailed mode, display, identity and setup behaviour.

## 1. Governing journey

The normal browser journey is:

`Switch on → Home → Race Setup (when needed) → START → STARTING → RACING → FINISHING → Results → READY again`

History, Track Setup, Racers, Settings, Advanced Settings and Developer / Diagnostics branch from this normal journey without becoming prerequisites to basic racing.

The browser is a client of authoritative P&P state. It never creates independent race timing or race truth.

## 2. Switch-on

P&P performs its normal core initialisation, loads last-known-good configuration and reconciles available hardware.

A healthy system takes the user to Home. Boot/discovery internals are not normal customer content. A genuine readiness problem is surfaced concisely with a route to fix it.

## 3. Permanent browser header

Normal browser pages use stable header geometry:

**P&P logo | Race Setup | Results | Track Setup | Racers | Settings | status/action**

The P&P logo returns Home. Home has no separate Home tab.

The fixed right-hand status/action area shows:

- **READY → START** when the prepared race can start;
- **NOT READY / FIX** when a genuine blocker exists.

Warnings that do not prevent racing do not remove START.

The location of the action area must not jump between pages or states.

During active racing, normal navigation chrome is stripped back so the race takes over the screen.

## 4. Home

Home is a dashboard, not a mode picker.

It shows the current prepared race, current Race Entries, a large hero START when ready, Change Race and a compact latest-result panel where useful.

The hero START and permanent header START are deliberately duplicated and invoke the same start request.

There is no Home Race Again button; START already repeats the prepared race.

## 5. Race Setup

Race Setup answers **what are we racing now?** It does not configure the physical installation.

The configured Track determines the physical lane rows. Race Entries assign Racers and optional Cars to those lanes.

Lap Race front-screen controls include target laps, finish behaviour, Rotate Lanes/direction, automatic post-race rotation and access to More Race Options.

Arbitrary lane reassignment uses direct drag-and-drop. Systematic movement uses Rotate Lanes. Detailed behaviour is governed by `RACE_SETUP_LANE_ASSIGNMENT_SPEC.md`.

More Race Options contains secondary per-race choices; uncommon owner configuration belongs in Advanced Settings.

## 6. START

START means start the prepared race. It is never a disguised Continue/Next navigation action.

Race Control validates the request and, when accepted, freezes the immutable Session Definition. The browser automatically becomes the live race presentation.

The browser renders the authoritative start-light/GO state. It does not run an independent JavaScript start clock and then tell P&P that the race has begun.

## 7. Live race

The race gets the screen.

The shallow Race Control Pod occupies the top; fixed physical lane rows occupy the useful body. Position changes inside each row rather than moving rows.

For the standard 8-lane target at intended desktop/tablet size, all eight rows are simultaneously visible at a useful readable size without scrolling.

The baseline live Lap information is:

**Position | Lane/Racer | Progress | Last | Best | Gap**

Optional Car identity may appear with the Racer. Additional data should not turn the standard screen into a telemetry wall.

The Race Control Pod renders authoritative start/race-control state and short events such as FASTEST LAP, FINAL LAP, PENALTY and FINISHED.

## 8. Pause

Ordinary PAUSE freezes the authoritative race state and keeps the same live race screen visible behind a centred overlay:

**RACE PAUSED**

**RESUME · RESTART · ABANDON**

While paused the five lamps alternate `Y G Y G Y` and `G Y G Y G`.

Resume runs the normal authoritative start-light sequence and resumes at GO with paused time excluded and race progress preserved.

Restart and Abandon require confirmation.

PAUSE is distinct from future formal Yellow/Red RMS procedures. Stage 15 must not infer or implement the deferred RMS state machine from the Pause controls.

## 9. Finish

Race Engine determines the finish according to the selected race rules.

P&P enters FINISHING/FINISHED and the presentation provides the configured Finish Display, default 5 seconds and allowing zero.

Finish theatre may use the pod housing/surround; it does not fake a five-yellow-lamp race-control state.

After the delay the browser moves automatically to Results.

## 10. Results and Race Again

Results switch from fixed lane order to finishing order and answer **who won and what happened?** first.

Completed sessions save automatically; there is no routine Save Results button.

**RACE AGAIN** prepares the same race and returns to READY. It does not start immediately. The user still presses START after retrieving/repositioning cars.

If automatic lane rotation is enabled, a properly completed race updates the pending next-race Race Entries before Race Again. The completed result retains the actual lanes used.

## 11. History

History is newest-first completed sessions with simple filtering and access to full result/detail. Practice does not acquire a fake winner merely to fit the Results layout.

Persistent PB/track-record summaries survive expiry of their originating session from the rolling history buffer.

## 12. Track Setup

Track Setup answers **what is this physical installation?**

It covers configured lane count, Timing Points, sensors, approximate topology/placement and measured track properties where relevant.

It is occasional installation configuration and must not become a prerequisite for ordinary racing when the default installation is healthy.

## 13. Racers

Racers manages saved people, optional Cars, modest records and recent results. It does not assign lanes for the imminent race; that belongs to Race Setup.

## 14. Settings hierarchy

The normal top-level header contains Settings only.

Within the product:

1. **Settings** — ordinary owner preferences;
2. **Advanced Settings** — knowledgeable-owner controls and Demo facilities;
3. **Developer / Diagnostics** — engineering/support internals.

Demo belongs in Advanced Settings. Developer / Diagnostics may expose Race Control/Race Engine/Message Bus/Session/raw-event/log detail and must remain clearly separate from owner-facing Advanced controls.

## 15. Presentation purposes

P&P has one authoritative race state but several legitimate presentations:

- integrated Taster/local display;
- standard browser race display;
- personal Racer phone display;
- TV/projector spectator display;
- dedicated Race Control presentation where useful.

These are purpose-specific layouts, not separate race systems and not v1 user-selectable presets named Classic/Race Control.

Reusable presentation components/data bindings should allow future advanced-user configurability without requiring Stage 15 to implement a generic Screen Designer or arbitrary layout engine.

## 16. Authority and multiple browsers

Multiple browsers may be connected simultaneously and may show different purpose-appropriate views. They converge on authoritative P&P state.

Reload/reconnect reconstructs the presentation from authoritative state. Browser loss or latency never changes official timing or race state.

## 17. Stage 15 boundary

Stage 15 should implement the agreed browser/product UX against the existing authoritative P&P architecture.

Stage 15 must not silently expand into:

- generic Screen Designer/configurable layout engine;
- detailed Yellow/Red RMS;
- general championship/competition management;
- invented hardware capability;
- a second browser-owned timing/race engine;
- functionality inferred solely from visual mock-up example content.

Visual prototypes guide presentation. Product specifications govern behaviour.

> **Easy to start. Deeply configurable when you want it.**
