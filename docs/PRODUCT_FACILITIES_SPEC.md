# Plonk & Play™ — Product Facilities Specification

**Status:** Committed product-design specification — Stage 15 UX reconciliation  
**Scope:** Cross-mode facilities and product decisions.

## 1. Base product direction

The base product has standard optical timing sensors whose authoritative events enter the normal P&P architecture. Sensors may be repurposed according to mode rather than permanently tied to one race form.

The System Controller is authoritative for timing, race logic and state. Browser clients are presentation/control clients and never timing authorities.

## 2. Power Module

Track-power control is optional hardware capability. Without it P&P may still detect/report/score supported behaviours but cannot physically cut/hold/control lane power.

Hardware-dependent options should normally remain visible but unavailable with a concise explanation rather than disappearing and concealing expandable capability.

Detailed Power Module behaviour remains deferred.

## 3. Audio and removable storage

A normal P&P System Controller should provide persistent removable storage capability, onboard audio capability and a small internal speaker suitable for race-control tones/speech. Exact hardware remains implementation design.

Persistent storage may retain results, lap/stage data, personal bests/records, Racer profiles, history, exports/backups, configuration and audio assets.

Audio event/type and destination are separate choices. Destinations may include P&P unit, Browser, Both or Mute. Browser audio can provide Racer-specific alerts.

Normal Settings should expose only the useful everyday audio controls. Detailed supported audio/event/destination controls belong in the relevant detailed/Advanced settings rather than cluttering Race Setup.

## 4. Browser and System Controller responsibilities

System Controller owns authoritative timing, race/session logic and state. Browsers own graphics, animation, formatting, browser audio and purpose-appropriate presentation.

Several simultaneous browser clients are supported. Different clients may show normal race, personal Racer, spectator or dedicated Race Control presentations without changing the underlying Race Engine.

Official timing never depends on browser latency.

## 5. Track Setup

Track Setup stores physical installation information that changes rarely, including configured lane count, Timing Points/sensors and optional track length/scale.

From track length and timing P&P may calculate actual model average speed. Where scale is known it may also calculate scale-equivalent speed. Speed need not occupy the default live scoreboard.

Track Setup is physical installation configuration, not preparation of today's race.

## 6. Start presentation and audio

Start presentation supports parameterised sequences rather than hard-coded browser animations. The same authoritative GO time governs race timing, display and scheduled physical/audio outputs.

Presentation may be theatrical; timing remains boring and reliable.

## 7. Solo and multi-user behaviour

P&P must remain usable without a dedicated Race Director. It supports a dedicated controller, a Racer who also operates the race and solo operation.

Every normal race/session must be runnable by one Racer.

## 8. Product-option philosophy

P&P is **simple by default, sophisticated when wanted**.

Software capability should not be artificially withheld merely to manufacture product tiers. Advanced/uncommon choices belong under **Advanced Settings** so ordinary operation remains clean.

Hardware-dependent expansion capabilities may remain visible but unavailable with a clear explanation.

## 9. Demonstration and Attract facilities

P&P provides demonstration facilities distinct from normal race operation. They support showroom/exhibition presentation, familiarisation and useful testing.

Customer-accessible Demo controls belong under **Advanced Settings** for knowledgeable owners. They are not ordinary race modes and are not hidden inside Developer / Diagnostics.

Developer / Diagnostics may expose deeper engineering evidence and raw event/state information, but that is a separate audience.

The three concepts remain **Attract Mode**, **Automatic Demo Race** and **Manual Demo/Test**.

### 9.1 Attract Mode

Attract Mode is scripted presentation rather than authoritative race simulation. It may show P&P identity, simple track/car graphics, start lights, timing, fastest-lap/finish moments and brief feature demonstrations.

It should use lightweight presentation suitable for the controller and never compromise real race processing.

The approximately 35-second V1 storyboard remains useful guidance rather than a frozen behavioural contract:

- 0–3 s: P&P identity;
- 3–6 s: track/lane presentation;
- 6–9 s: start sequence and GO;
- 9–18 s: short choreographed race;
- 18–22 s: close finish/result;
- 22–30 s: brief race-type/expansion feature flashes;
- 30–35 s: return to P&P identity.

Exact artwork, timings, sounds and transitions may be tuned without changing the architectural purpose.

### 9.2 Automatic Demo Race

Automatic Demo Race is a genuine P&P race driven by simulated detector activity.

Virtual detector events enter at the defined sensor/input boundary so the normal downstream architecture handles them. Race Control, Race Engine, Message Bus, Noticeboard, displays, audio and results processing operate as for physical detector events.

Virtual competitors may use plausible varying lap times, but the real Race Engine determines the result rather than a separately scripted fake winner.

Advanced Settings may expose suitable owner-level Demo configuration such as Demo Race selection, demo laps and automatic lap generation. Exact supported parameters are implementation/product-detail choices and must not become a back door to a full RMS.

### 9.3 Manual Demo/Test

Manual Demo/Test may expose virtual versions of installed detector inputs through Advanced Settings. Activating one injects the corresponding simulated event at the same defined simulation boundary.

This provides a useful owner/test boundary: if a virtual event produces expected downstream behaviour while the physical detector does not, investigation can focus on the physical input side.

Developer / Diagnostics may additionally expose raw engineering detail, logs and internal state for support/development.

### 9.4 Demo-session integrity

Simulated race sessions are authoritatively identifiable as **DEMO**. Demo results must not contaminate genuine personal bests, track records, championships or equivalent persistent sporting data.

Displays identify simulated operation clearly enough that it cannot reasonably be mistaken for a live race.

Physical and simulated detector events must not accidentally mingle within one authoritative race. Exact policy for physical activity during Automatic Demo remains a later design decision.

### 9.5 Architectural principle

Demo capability must not create a second fake Race Engine. Simulation for real behaviour/testing occurs at the sensor/input boundary and everything downstream remains normal product behaviour.

Attract Mode is the deliberate exception because it is presentation rather than race simulation.

## 10. Settings audience boundary

Three levels are deliberately distinct:

- **Settings** — ordinary owner preferences;
- **Advanced Settings** — polished product UI for knowledgeable owners, including Demo and uncommon supported controls;
- **Developer / Diagnostics** — engineering/support view of internals such as Race Control, Race Engine, Message Bus, Session Definition, raw events, hardware state, synchronization, errors, storage and logs.

Do not mix Developer terminology into normal/Advanced owner UI merely because a feature is uncommon.

## 11. Deferred decisions

Still deliberately open are exact controller hardware, audio/storage implementation, Power Module electrical design, detailed RMS Yellow/Red state machine, detailed persistence schema, exact Attract artwork/wake behaviour, exact Automatic Demo virtual-car model and exact behaviour when physical detector activity occurs during Automatic Demo.

These are deferred decisions, not permission for Stage 15 to invent them.
