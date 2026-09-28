# Plonk & Play™ — Product Facilities Specification

**Status:** Committed product-design specification  
**Scope:** Cross-mode facilities and product decisions agreed during Lap Counter product design.

## 1. Base product hardware direction

The base product has two standard optical timing sensors.

The same sensors are repurposed according to mode rather than being permanently tied to one race form.

The System Controller is authoritative for timing, race logic and state. Browser clients are presentation/control clients and must not become timing authorities.

## 2. Power Module

Track-power control is an optional hardware capability.

Without a Power Module, P&P can still detect, report and score behaviours such as false starts and penalties, but it cannot physically cut, hold or otherwise control lane power.

Where a Power Module is fitted, modes may offer appropriate power-control responses.

Hardware-dependent options should normally remain visible but locked/greyed with a concise explanation such as **Power Module required**, rather than disappearing and concealing the system's expandable capability.

Reduced-power Yellow operation is only a valid product option if the eventual Power Module hardware genuinely supports controlled reduced power.

## 3. Audio and removable storage

### 3.1 Base requirement

Every normal P&P System Controller should include:

- persistent microSD/removable storage capability;
- onboard audio capability;
- a small internal speaker sufficient for clear race-control tones and speech.

This is a standard base capability, not an upgrade.

The exact hardware implementation is not yet fixed.

### 3.2 Storage uses

Persistent removable storage may retain:

- race/session results;
- lap-by-lap data;
- Timed Stage results;
- personal bests and records;
- track records;
- MUG profiles;
- event history;
- CSV exports/backups;
- configuration backup;
- audio/voice files.

The detailed record schema remains implementation design.

### 3.3 Audio implementation

The implementation should favour a small, reliable, low-BOM solution with sane file/folder handling.

Options to compare before hardware freeze include:

1. off-the-shelf audio + SD module;
2. separate microSD plus audio module/amplifier;
3. audio/SD circuitry integrated into P&P's own PCB.

Previous awkward module folder/naming conventions must not dictate the new architecture.

High audio power is not a goal in itself. Clear internal race-control sound matters more than 5 W versus 10 W; a few watts may be sufficient.

### 3.4 Audio destinations

Audio event/type and audio destination are separate choices.

Possible destinations:

- P&P unit;
- Browser;
- Both;
- Mute.

Browser audio enables individual MUG alerts without requiring separate physical sound hardware for every competitor.

Start audio may include beeps, differentiated pitches and spoken numeric countdowns.

## 4. Browser and System Controller responsibilities

The System Controller owns:

- authoritative timing;
- race/session logic;
- authoritative state;
- small race facts/events and state updates.

Browsers own:

- graphics;
- animation;
- formatting;
- browser audio;
- presentation appropriate to their role.

Official timing must never depend on browser latency.

The architecture should support several simultaneous browser clients. A representative four-MUG setup may involve six browsers:

- one main scoreboard;
- four personal MUG displays;
- one Race Director display.

This scale should be prototype-tested under realistic worst-case conditions before hardware is frozen.

## 5. Track Setup

Track Setup may optionally store information that normally changes rarely:

### 5.1 Track length

- metres or feet;
- Unknown / Not entered allowed.

### 5.2 Track scale

Preset examples include:

- 1:32;
- 1:24;
- 1:43;
- 1:64;
- Custom;
- Unknown / Not entered.

From track length and timing P&P can calculate actual model average speed.

Where scale is known it can also calculate scale-equivalent speed.

Speed may be calculated per lap, for fastest lap, stage or whole race where meaningful.

Speed does not need to occupy the default live scoreboard. It may appear in results, analysis or event callouts.

## 6. Start presentation and audio

Start behaviour is a race-mode concern, but presentation facilities should support parameterised sequences rather than hard-coded animations.

For normal Lap racing this includes three- or five-light sequences, configurable red-light interval and configurable final delay.

The same authoritative GO time must govern race timing, display and any scheduled physical/audio outputs. Presentation may be theatrical; timing remains boring and reliable.

## 7. Solo and multi-user product behaviour

P&P must remain usable without a dedicated Race Director.

Supported operating patterns include:

- dedicated Race Director;
- Race Director who is also racing;
- solo MUG.

Every normal race/session must be runnable by one MUG.

At the other end, P&P may scale to a main display, several personal MUG displays and a separate Race Director display without changing the fundamental race engine.

## 8. Product-option philosophy

P&P should be **simple by default, sophisticated when wanted**.

Software capability should not be artificially withheld merely to manufacture product tiers when exposing it costs essentially nothing.

Advanced or uncommon choices may be placed under **Advanced** so the ordinary MUG is not confronted with unnecessary complexity.

Hardware-dependent premium/expansion capabilities can remain visible but locked with a clear requirement explanation.

## 9. Deferred decisions

The following remain deliberately open:

- exact ESP32/System Controller hardware;
- exact audio/SD implementation;
- exact speaker/amplifier power;
- exact Power Module electrical design;
- exact dual-beam Drag hardware;
- detailed RMS state machine;
- detailed visual styling and animations;
- detailed persistence schema.

These are not forgotten requirements; they are deferred implementation/product decisions.
