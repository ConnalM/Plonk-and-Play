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

Selecting Drag assigns the standard two-sensor base hardware contextually: Sensor 1 Start, Sensor 2 Finish. Do not ask SMUG to reassign those sensors unnecessarily.

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
