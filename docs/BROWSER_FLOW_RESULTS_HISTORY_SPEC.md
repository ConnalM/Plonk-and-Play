# Browser Flow, Results and History

## Home

The browser Home screen presents the five principal modes as visible selectable cards/tiles:

- **Lap Race** — visually dominant/default;
- **Practice**;
- **Endurance**;
- **Timed Stage**;
- **Drag**.

Do not hide modes in a dropdown merely to save space.

Secondary Home actions include **Race Again**, **Results & History** and **Setup**.

## Browser presentation architecture

P&P ships with a small number of good **out-of-the-box (OOTB) race-screen designs** rather than requiring a MUG or SMUG to design a screen before racing. The initial target is at least a clear **Classic** race display and a more information-rich **Race Control** display.

The standard Browser race displays shall be constructed from **reusable presentation components** using the same declarative screen-layout model intended for future SMUG-editable screens. OOTB screen designs are presets, not separately hard-coded interfaces.

The presentation component set may include, as appropriate to the active mode and installed capabilities, items such as MUG name, car name, lane, position, lap count, last lap, best lap, gap, race clock, sector information, fuel, reaction time, start lights and race/flag status.

The layout model shall support three display classes:

- **Mobile**;
- **Tablet**;
- **PC / large display**.

A screen design may share presentation choices and data bindings across those classes while allowing component position and size to differ by display class. A SMUG should therefore be able to customise a screen without requiring a PC layout to be mechanically squeezed onto a phone.

The future **Screen Designer** shall edit this declarative presentation model rather than HTML or executable application code. Its intended interaction is a constrained visual editor: choose P&P components/fields from a palette, position and resize them, and adjust permitted presentation properties such as typography, colours, backgrounds, lights and simple effects.

A SMUG shall be able to duplicate an OOTB preset and customise the copy. OOTB presets remain available for straightforward recovery and use.

This does **not** make the Screen Designer a v1 implementation requirement. The architectural requirement for v1 is that ordinary Browser race screens are built from reusable components and a declarative layout/preset model so that later SMUG editing extends the same system rather than requiring the Browser UI to be rewritten.

Custom screens remain presentation only. Their available live data comes from P&P's externally presentable state/Noticeboard and standard presentation interfaces. A custom layout does not acquire authority over Race Control, Race Engine or other operational state merely because it displays their information.

## Lap Race setup

The normal setup presents:
- lane/MUG assignment;
- optional car only when car recording is enabled;
- SWAP LANES;
- target laps using decrement / editable value / increment;
- finish behaviour;
- MORE RACE OPTIONS;
- a visually dominant START control.

More Race Options expands without requiring a separate page and may contain start lights, GO style, final delay, false-start behaviour and audio. Advanced contains less commonly adjusted timing details.

Settings auto-save where intention is unambiguous.

## Practice setup and control

Practice offers **Open** or **Timed** as visible either/or choices.

Open Practice continues until SMUG/Race Control selects **END SESSION**.

Timed Practice ends automatically at its configured duration; SMUG may also end it early.

Session-level controls belong to SMUG/Race Control, not ordinary MUG displays.

## Endurance setup

Endurance setup includes:
- MUG/lane assignment;
- editable duration;
- the two established finish behaviours: **Stop at zero** or **Finish current lap**;
- More Race Options.

Time remaining is dominant during the race.

## Timed Stage setup

Timed Stage first offers two clear geometry choices:
- **Loop**;
- **Point-to-Point**.

It then supports **Single Stage** or **Multi-Stage Event** without requiring a motorsport-discipline label.

Loop setup includes MUG, stage laps and Runs.

Point-to-Point includes MUG, A→B/B→A direction and Runs.

Where multiple Runs are configured, established scoring choices such as All Runs Count / Best Run Counts may be offered.

A Multi-Stage Event is a lightweight collection of Loop and/or Point-to-Point stages with event progress and cumulative results. Do not expand v1 into a general competition-management system.

## Drag setup

Drag setup includes:
- MUG;
- Sportsman or Pro start tree;
- optional contextual course length and scale information;
- STAGE as the primary action.

Selecting Drag proposes the standard two-sensor base roles contextually: Sensor 1 Start, Sensor 2 Finish. Do not ask SMUG to reassign those sensors unnecessarily. When START is accepted, those Drag roles are frozen into the Session Definition for that session; the Input Module itself does not acquire Start/Finish meaning.

Drag remains usable with no length or scale configured. Missing optional values may be added directly from Drag setup.

## Immediate Lap Race result

The primary Results screen should answer who won without becoming an analysis dashboard.

Show:
- finishing position;
- MUG or lane;
- total result/finish time or gap as appropriate;
- each MUG's Best Lap;
- overall Fastest Lap;
- RACE AGAIN;
- HOME;
- DETAILS.

DETAILS exposes the lap-by-lap data and other available information such as penalties, sectors or speed where relevant.

Results are stored automatically. There is no routine Save Results action.

## Results & History

History is a simple newest-first list of completed sessions across the supported modes.

A basic mode filter may be provided:
**All | Lap | Practice | Endurance | Timed Stage | Drag**.

Selecting a history item reopens its result/details.

Do not turn v1 History into a general statistics/database-management interface.

### Rolling history buffer

History is a fixed-size rolling buffer.

When the buffer is full, storing a new completed session automatically discards the oldest retained session. The MUG is not required to manage storage or respond to capacity warnings during normal use.

The exact factory buffer size is an implementation choice to be set after realistic session-size/storage testing.

Persistent record summaries are independent of the rolling session buffer. Expiry of an old session must not make a valid PB or track record revert merely because its originating session has fallen out of History.

### v1 records

Keep records deliberately modest:
- personal fastest lap per MUG;
- overall fastest lap / track record.

Store useful underlying race data so richer statistics can be added later without making them v1 requirements.

## Storage implementation boundary

The product requirement is **persistent structured history**, not a particular storage engine.

SQLite, structured files or another suitable implementation may be evaluated during implementation/prototyping. Browser behaviour must not depend on that choice.

General principle:

> **Design broadly. Implement narrowly.**
