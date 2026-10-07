# Plonk & Play™ — Configuration and Identity Specification

**Status:** Committed product-design specification — Stage 15 UX reconciliation  
**Scope:** v1 Racer, optional Car, Race Entry and installation configuration.  
**Principle:** Identity enhances P&P; it must never be required to use P&P.

## 1. Immediate use without setup

P&P remains genuinely Plonk & Play. A new/default two-lane system can race using anonymous lane identities without creating profiles, cars or a database first.

## 2. Persistent Racer identity

A saved Racer has a permanent internal ID and a user-visible name. Name is the only required v1 profile field.

Historical identity belongs to the internal ID, not the displayed name, so renaming a Racer does not split previous results.

## 3. Selecting Racers

Selecting the Racer area of a Race Entry opens a simple list of saved Racers plus suitable temporary/default choices, Guest and **+ New Racer**.

A Racer already assigned to another active lane remains visible but unavailable rather than silently disappearing.

A Guest may race without becoming a permanent saved Racer. An optional temporary Guest name may be promoted later without re-entry.

## 4. Cars

Car recording is optional and should not clutter normal setup when unused.

Cars are independent entities; a Car does not permanently belong to a Racer. A Race Entry may reference an optional saved Car.

**Race Entry = Racer + Lane + optional Car**

The complete Race Entry moves when lane assignment changes.

## 5. Racers screen

The Racers area manages people, optional Cars and modest records; it does not assign lanes for the next race.

A normal Racers screen may provide:

- saved Racer list;
- selected Racer detail;
- Edit and Delete;
- optional Car management;
- useful summary such as races, wins/podiums and best lap;
- personal records and recent results.

Delete requires confirmation. Racer management must not become a prerequisite to racing.

## 6. Race Setup and lane assignment

Race Setup prepares the imminent race. The number of lane rows comes from the configured physical Track Configuration.

Each row represents a fixed physical lane and contains its pending Race Entry. Racer/Car areas are the natural selection points; a separate per-row CHANGE button is not required.

Arbitrary manual reassignment is performed directly by drag-and-drop of complete Race Entries among fixed lane rows.

Systematic lane movement is **ROTATE LANES**, not Swap Lanes. Rotation behaviour, direction, automatic post-race rotation, scaling to configured lane count and numeric controls are governed by `RACE_SETUP_LANE_ASSIGNMENT_SPEC.md`.

## 7. Remembering the last setup

P&P automatically remembers the most recently useful straightforward Race Setup. Returning to race preparation should normally restore it, allowing regular users to reach START with minimal interaction.

A separate routine Save Setup / Load Setup system is not required for v1.

## 8. Track configuration

The v1 product assumes one remembered physical Track Configuration rather than requiring a named multi-track database.

Track Configuration describes the installation and may include:

- configured lane count;
- physical Timing Points and their sensors;
- remembered/default role proposals used when forming Race Setup and Session Definition;
- Start/Finish arrangement;
- optional track length and scale.

Track Setup tells P&P what physical timing hardware exists, where it is and what capabilities the installation can support. It is distinct from Race Setup.

A physical Sensor has persistent hardware identity. A Timing Point represents a physical measurement location and may contain one or multiple sensors, commonly one per lane. Features/race modes use Timing Points rather than requiring the user to redefine every sensor for every race.

The graphical Track Setup may record approximate topology/position; exact measured properties such as track length or speed-trap distance remain explicit numeric data where required.

## 9. Configuration presentation

P&P separates **what are we doing now?** from **what is this installation?**

### Race Setup

Contains the small number of settings commonly needed for the imminent race: Race Entries, target/duration as appropriate, finish behaviour, lane rotation, More Race Options and START.

### More Race Options

Contains settings that genuinely vary from race to race without deserving permanent front-screen space, such as start-light count, GO style, supported false-start behaviour, race sounds and Finish Display duration.

### Track Setup

Describes the physical track, Timing Points, sensors and installed capabilities.

### Racers

Manages saved people, optional Cars and their records/history summaries.

### Settings

Normal owner preferences and system settings. Less-common knowledgeable-owner controls live under **Advanced Settings**. Engineering/support internals live separately under **Developer / Diagnostics**.

## 10. Settings hierarchy

Normal Settings should remain sparse. A subject may expose its basic controls on the main Settings page and a **MORE…** / subject-settings route for detailed supported controls.

**Advanced Settings** is polished product UI for knowledgeable owners. It may contain uncommon timing/input/start/display/audio controls and Demo facilities.

**Developer / Diagnostics** is for engineering/support and may expose internal terminology and state such as Race Control, Race Engine, Message Bus, Session Definition, raw input events, synchronization, discovered hardware, errors, storage and logs.

Advanced Settings and Developer / Diagnostics are distinct audiences and must not be mixed merely because both contain uncommon controls.

Firmware Update and Factory Restore belong in Advanced/System administration rather than everyday race setup. Destructive actions require confirmation.

## 11. Save and confirmation behaviour

P&P does not require routine SAVE/APPLY interaction where intention is unambiguous.

Race settings become the current/remembered pending Race Setup as they change. P&P creates the immutable Session Definition only when START is accepted.

Confirm destructive, disruptive or genuinely ambiguous actions such as deleting persistent information, materially reassigning hardware, restarting/abandoning an active race or factory reset.

> **Save automatically where intention is unambiguous. Confirm where an action is destructive, disruptive or ambiguous.**

## 12. Out-of-box base configuration

The base product requires zero software configuration when installed according to the recommended quick-start arrangement.

Factory/default base proposals are:

- Sensor 1 → Lane 1 Start/Finish;
- Sensor 2 → Lane 2 Start/Finish;
- recommended physical arrangement → sensors before the Start/Finish line.

These are installation/session-role proposals, not racing meaning owned by the Input Module. START freezes the roles required for the accepted Session Definition.

Straightforward lap counting works immediately. Capabilities requiring a different sensor relationship, including applicable false-start/reaction measurements, are unavailable unless the physical/configuration arrangement can genuinely measure them. Such options may remain visible but unavailable with a concise explanation.

Optional track length, scale and Racer names are not first-boot requirements.

## 13. Quick-setup guide

The base product needs a short highly visual quick-start guide whose job is to get an ordinary owner from the box to a working basic race with very few decisions:

1. **PLONK** — position the two sensors as illustrated.
2. **PLUG** — connect Sensor 1/Lane 1, Sensor 2/Lane 2 and power P&P.
3. **CONNECT** — where full browser P&P is wanted, connect phone/tablet/computer.
4. **PLAY** — use the prepared/default race and press START.

The software defaults must match the recommended physical installation so no Setup step is required before basic racing.

## 14. Future compatibility without v1 scope creep

The model should not unnecessarily prevent richer Racer/Car profiles, multiple saved physical Track Configurations, competitions, heats/rounds or future screen configurability.

These are not v1 requirements merely because the model leaves room for them.

> **Design broadly. Implement narrowly.**
