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

## 9. Demonstration and Attract facilities

P&P should provide demonstration facilities as a product capability, distinct from normal race operation. These facilities are intended for showroom/exhibition presentation, user familiarisation, development testing and fault diagnosis.

The facilities are deliberately separated into **Attract Mode**, **Automatic Demo Race** and **Manual Demo/Test**.

### 9.1 Attract Mode

Attract Mode is a presentation facility analogous to the demonstration displays used on retail hi-fi and similar equipment.

It is not intended to be generally available as an ordinary race mode. A SMUG-controlled configuration option enables or disables it.

When enabled, an idle P&P unit may automatically run a short repeating presentation sequence. The sequence may demonstrate the product using lightweight graphics and animation, for example:

- the P&P logo/identity;
- a simple track drawing itself;
- small animated cars;
- start lights;
- lap-count and timing displays;
- fastest-lap callouts;
- a close finish and chequered flag;
- results;
- short visual demonstrations of supported race types and expansion capability.

Attract Mode is presentation, not authoritative race simulation. Its race sequence may therefore be deliberately scripted and choreographed for clarity and visual interest.

The display implementation should favour simple, lightweight graphical primitives, text, icons and small bitmap/sprite assets suitable for an ESP32-class controller. Presentation work must remain subordinate to authoritative race processing and must never compromise race timing or control.

The exact display hardware, resolution, artwork, animation style, sequence length and wake/exit behaviour remain deferred design decisions.

Attract Mode may ultimately be available on the local Taster display and/or connected Browser displays. That presentation scope is deliberately not fixed yet.

#### V1 Attract presentation guidance

The initial implementation should use the following approximately 35-second loop as a practical starting storyboard:

- **0–3 s:** P&P / PLONK & PLAY identity;
- **3–6 s:** a simple track draws onto the screen, followed by two lane lines and small car sprites;
- **6–9 s:** start-light sequence followed by GO;
- **9–18 s:** a short choreographed race with changing lap/timing information, cars changing relative position and a FASTEST LAP moment;
- **18–22 s:** close finish, chequered flag and winner/gap presentation;
- **22–30 s:** brief feature flashes illustrating **RACE → RALLY → DRAG** and **2 → 4 → 6 → 8 LANES**;
- **30–35 s:** return to P&P / PLONK & PLAY identity, pause briefly, then repeat.

This storyboard is **presentation guidance for V1, not a frozen behavioural contract**. Timing, transitions, wording, sprites, artwork, sound effects and individual beats may be tuned once the sequence is seen on real display hardware. Such presentation tuning does not require an architectural/product-specification change provided the purpose and boundaries of Attract Mode remain unchanged.

A final P&P logo is not a dependency for V1. Until a satisfactory logo is designed, the presentation may use a clean **P&P / PLONK & PLAY** text identity.

### 9.2 Automatic Demo Race

Automatic Demo Race is different from Attract Mode: it is a genuine P&P race driven by simulated detector activity.

Virtual detector events must enter at the defined sensor/input boundary so that the normal downstream product path is exercised. Race Control, Race Engine, Message Bus, Noticeboard, displays, audio and results processing should operate as they would for physical detector events.

The simulator may create plausible virtual competitors with slightly varying lap times. Demonstrations may be tuned to produce interesting, reasonably close racing, but the result should be determined by the real race logic rather than by a separately scripted fake result.

Automatic Demo Race may be initiated through SMUG-controlled demonstration facilities. It is not an ordinary Taster race option that a MUG can accidentally select.

### 9.3 Manual Demo/Test

A manual demonstration/test facility may expose virtual versions of the installed detector inputs through SMUG.

For example, a two-lane Lap Race installation may provide virtual controls equivalent to:

- Lane 1 Start/Finish detector;
- Lane 2 Start/Finish detector.

Additional configured detectors may be represented where appropriate.

Activating a virtual detector should inject the corresponding simulated event at the same defined simulation boundary used by Automatic Demo Race, exercising the real downstream system.

This provides a useful diagnostic boundary. If a virtual detector produces the expected race behaviour while the corresponding physical detector does not, investigation can concentrate on the physical sensor/input side rather than the downstream race system.

### 9.4 Demo-session integrity

Simulated race sessions must be authoritatively identifiable as **DEMO**, not merely labelled by presentation code.

Demo results must not contaminate genuine sporting records, including personal bests, track records, championship results or equivalent persistent competitive data.

Displays should identify simulated operation clearly enough that a demo cannot reasonably be mistaken for a live race.

The eventual persistence design may retain demo sessions for testing or support purposes, but they must remain distinguishable from genuine results.

Physical and simulated detector events must not accidentally mingle within an authoritative race. Exact behaviour when real detector activity occurs during an Automatic Demo Race remains a later design decision; possible policies include cleanly terminating the demo or ignoring physical detector activity for that demo session.

### 9.5 Architectural principle

Demo capability must not create a second fake Race Engine.

Where the purpose is to demonstrate or test real P&P behaviour, simulation occurs at the sensor/input boundary and everything downstream remains normal product behaviour.

Attract Mode is the deliberate exception because it is presentation rather than race simulation.

## 10. Deferred decisions

The following remain deliberately open:

- exact ESP32/System Controller hardware;
- exact audio/SD implementation;
- exact speaker/amplifier power;
- exact Power Module electrical design;
- exact dual-beam Drag hardware;
- detailed RMS state machine;
- detailed visual styling and animations;
- detailed persistence schema;
- exact Attract Mode display hardware, graphics, sequence and wake/exit behaviour;
- whether Attract Mode presentation is local-display only or also available on Browser displays;
- exact Automatic Demo Race virtual-car model and timing variation;
- exact behaviour when physical detector activity occurs during an Automatic Demo Race;
- detailed SMUG controls for demonstration/test facilities.

These are not forgotten requirements; they are deferred implementation/product decisions.
