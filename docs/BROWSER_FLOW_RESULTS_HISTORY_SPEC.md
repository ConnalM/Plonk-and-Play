# Browser Flow, Results and History

**Status:** Stage 15 product UX baseline candidate  
**Scope:** Browser navigation, race preparation, results/history and presentation architecture.

## Home

Home is a calm dashboard for the race P&P is currently prepared to run. It is not a mode-picker or configuration menu.

It shows:

- the prepared race type and important summary, for example **Lap Race — 10 laps**;
- current Race Entries, for example **Lane 1 — Dad**, **Lane 2 — Baird**;
- a large primary **START** action when the setup is ready;
- **Change Race**, which opens Race Setup;
- a compact latest-result panel with **View Results** where a previous result exists.

Home does not provide a separate **Race Again** action. Pressing START already repeats the currently prepared race. Race Again belongs on Results.

The supported race modes remain available through Race Setup; they are not required to appear as five mode cards on Home.

## Browser navigation and header

The normal browser header uses stable geometry:

**P&P logo | Race Setup | Results | Track Setup | Racers | Settings | status/action area**

The P&P logo is the Home action. Home therefore requires no separate Home tab and no operational tab is selected while Home is shown.

The right-hand status/action area remains in a fixed position across normal browser pages:

- ready setup: **READY → START**;
- genuine blocker: **NOT READY / FIX**, with a concise reason and a direct route to the required correction;
- non-blocking warnings do not remove START.

The header must not jump as state changes. Home deliberately contains both the large hero START and the permanent header START; both invoke the same authoritative start request.

During an active race, normal navigation chrome yields to the live race presentation so race information receives the available screen area.

## Browser presentation architecture

P&P ships with good purpose-appropriate presentations. Users are not required to design a screen before racing.

There are no user-selectable v1 presets named **Classic** and **Race Control**. The standard browser race display is the normal default.

P&P nevertheless retains reusable presentation components and common data bindings because the product legitimately needs different presentations of the same authoritative race state, including:

- integrated Taster/local display;
- normal browser race display;
- personal/Racer phone display;
- TV/projector spectator display;
- dedicated Race Control presentation where appropriate.

Layouts may differ by purpose and display class while using the same authoritative state and P&P visual language.

Future advanced-user screen configuration may allow supported components to be shown, hidden, rearranged or resized. That future facility must extend the common presentation component model rather than require arbitrary HTML/CSS.

A generic Screen Designer or configurable layout engine is **not a Stage 15 requirement**. Stage 15 should implement the required standard/purpose layouts without creating a rigid monolithic presentation that unnecessarily prevents later configurability.

Custom or purpose-specific presentations remain presentation only. They do not acquire authority over Race Control, Race Engine or timing.

## Lap Race setup

The normal setup presents:

- one Race Entry row per configured physical lane;
- Racer assignment;
- optional Car assignment where car recording is enabled;
- **ROTATE LANES**;
- rotation direction control;
- **Automatically rotate after each race**;
- target laps using the standard numeric adjustment control;
- finish behaviour;
- **MORE RACE OPTIONS**;
- the permanent READY/START header action.

Race Entry behaviour, drag-and-drop reassignment, lane rotation and numeric adjustment are governed by `RACE_SETUP_LANE_ASSIGNMENT_SPEC.md`.

More Race Options may contain start-light count, GO style, supported false-start behaviour, race sounds and Finish Display duration. Less-common detailed parameters belong in Advanced Settings rather than cluttering the normal setup.

Settings auto-save where intention is unambiguous.

## Practice setup and control

Practice offers **Open** or **Timed** as visible either/or choices.

Open Practice continues until Race Control selects **END SESSION**. Timed Practice ends automatically at its configured duration and may also be ended early.

Session-level controls belong to Race Control rather than personal Racer displays.

## Endurance setup

Endurance setup includes:

- Racer/lane assignment;
- editable duration;
- **Stop at zero** or **Finish current lap**;
- More Race Options.

Time remaining is dominant during the race.

## Timed Stage setup

Timed Stage first offers two clear geometry choices:

- **Loop**;
- **Point-to-Point**.

It then supports **Single Stage** or **Multi-Stage Event** without requiring a motorsport-discipline label.

Loop setup includes Racer, stage laps and Runs. Point-to-Point includes Racer, A→B/B→A direction and Runs.

Where multiple Runs are configured, established scoring choices such as All Runs Count / Best Run Counts may be offered.

A Multi-Stage Event is a lightweight collection of Loop and/or Point-to-Point stages with event progress and cumulative results. Do not expand v1 into a general competition-management system.

## Drag setup

Drag setup includes:

- Racer;
- Sportsman or Pro start tree;
- optional contextual course length and scale information;
- STAGE as the primary action.

Selecting Drag proposes the standard two-sensor base roles contextually: Sensor 1 Start, Sensor 2 Finish. Do not ask the user to reassign those sensors unnecessarily. When START is accepted, those Drag roles are frozen into the Session Definition for that session; the Input Module itself does not acquire Start/Finish meaning.

Drag remains usable with no length or scale configured.

## START and active race transition

START always means start the prepared race; it is not navigation to another preparation page.

When START is accepted, Race Control freezes the Session Definition and the browser moves automatically into the live race presentation. The browser renders the authoritative start sequence and race state; it does not invent its own start timing.

The active session is immutable from Race Setup. Changes to the pending next-race setup must not alter the active race.

## Immediate result

The primary Results screen answers who won without becoming an analysis dashboard.

Show, as appropriate:

- finishing position;
- Racer or lane;
- total result/finish time or gap;
- each Racer's Best Lap;
- overall Fastest Lap;
- **RACE AGAIN**;
- **CHANGE RACE**;
- **HISTORY**;
- **DETAILS**.

DETAILS exposes lap-by-lap data and other available information such as penalties, sectors or speed where relevant.

Results are stored automatically. There is no routine Save Results action.

### Race Again

**RACE AGAIN does not immediately start another race.**

It prepares the same race again and returns P&P to READY. The user must still press START. This avoids an unexpected start while cars are being retrieved or repositioned.

If automatic lane rotation is enabled, a properly completed race first rotates the pending next-race Race Entries according to the configured direction. Race Again therefore uses that already-rotated pending assignment. The completed result always retains the lanes actually used in that race.

Abandoned or restarted races do not trigger automatic post-race rotation.

## Results & History

History is a simple newest-first list of completed sessions across supported modes.

A basic mode filter may be provided:
**All | Lap | Practice | Endurance | Timed Stage | Drag**.

Selecting a history item reopens its result/details. Practice does not invent a winner where the mode has none.

Do not turn v1 History into a general statistics/database-management interface.

### Rolling history buffer

History is a fixed-size rolling buffer. When full, storing a new completed session automatically discards the oldest retained session. The user is not required to manage storage or respond to capacity warnings during normal use.

The exact factory buffer size is an implementation choice after realistic storage testing.

Persistent record summaries are independent of the rolling session buffer. Expiry of an old session must not make a valid personal best or track record disappear merely because its originating session has fallen out of History.

### v1 records

Keep records deliberately modest:

- personal fastest lap per Racer;
- overall fastest lap / track record.

Store useful underlying race data so richer statistics can be added later without making them v1 requirements.

## Storage implementation boundary

The product requirement is persistent structured history, not a particular storage engine. Browser behaviour must not depend on the implementation choice.

> **Design broadly. Implement narrowly.**
