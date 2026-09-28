# Race Control and Race Engine Detailed Design

**Status:** Draft v0.1  
**Parent architecture:** SYSTEM_ARCHITECTURE.md v1.0

## 1. Purpose

This document resolves the Race Control / Race Engine responsibility question deliberately left open in System Architecture v1.0. It defines the responsibility boundary sufficiently for later implementation and isolated testing without prescribing C++ classes, files, packet formats or other implementation details.

## 2. Responsibility split

**Race Control owns the operation and lifecycle of a session.**

This includes:
- preparing and starting a session;
- the start procedure;
- establishing the authoritative GO time in P&P System Time;
- PAUSE and RESUME;
- operational STOP;
- RESET as an operational request;
- coordinating operational actions such as start sequences and track power;
- deciding the operational consequence of relevant capability/availability changes.

**Race Engine owns the interpretation, state, rules and calculations of the competition within that session.**

This includes:
- interpreting mapped competition events;
- lap, sector and other competition timing;
- lap counts, positions and other competition state;
- race-mode rules;
- optional competition features such as simulated fuel;
- results;
- determining when the competition's rules say it has completed.

Together Race Control and Race Engine remain the single authoritative source of P&P race/session state, with each responsibility having one owner.

Race Control may command the Race Engine through defined operations but must not manipulate its internal competition state. The Race Engine reports competition facts and completion conditions but does not control session lifecycle or physical hardware.

## 3. Conceptual boundary

### Race Control to Race Engine

The conceptual information/operations crossing the boundary are:

1. **PREPARE**
   - supplies the fixed session definition for the competition, for example mode, race length and enabled optional features.

2. **GO @ P&P time**
   - establishes the authoritative time from which the Race Engine regards competition as active.

3. **SESSION OPERATION / STATE CHANGE @ P&P time**
   - communicates operational changes such as Pause, Resume and Stop where relevant.
   - RESET is an operational request; the Race Engine remains responsible for creating, clearing or reinitialising its own competition state.

4. **COMPETITION INPUT**
   - mapped semantic events relevant to competition;
   - original P&P timestamps are preserved;
   - these events are delivered to the Race Engine through its defined boundary and need not be mechanically relayed through Race Control merely because Race Control owns session lifecycle.

### Race Engine to Race Control

1. **COMPETITION FACTS**
   - examples include lap completed, fastest lap, position changed and pit/refuelling state.

2. **CURRENT COMPETITION STATE**
   - authoritative competition information such as laps, times, positions and enabled-feature state.

3. **COMPLETION**
   - indicates that the active rule set says the competition has completed normally.

4. **PROBLEM / STATUS**
   - reports conditions that prevent or impair correct interpretation of the competition.

These categories are conceptual contracts, not final packet formats, APIs or C++ methods.

## 4. Session definition

An active session uses a fixed session definition.

Ordinary configuration changes must not silently alter a session already in progress. When Race Control prepares a session, the definition supplied to the Race Engine remains the definition for that session unless a particular runtime change is explicitly supported by later design.

For the initial product, race-affecting configuration is not changed while an individual race is active. The Race Director changes it after that race has ended. This is a deliberate product simplification, not an architectural prohibition on harmless live presentation preferences or a future explicitly designed runtime feature.

This does not prescribe whether the implementation copies, references, serialises or otherwise represents the definition.

## 5. Time and event ordering

Race Control establishes authoritative session-state boundaries using P&P System Time.

Where timing affects competition interpretation, the Race Engine uses event timestamps relative to those boundaries rather than communication arrival order.

For example, if PAUSE is effective at P&P time 250.000:
- an event timestamped 249.999 belongs before the pause even if it arrives afterwards;
- an event timestamped 250.001 belongs after the pause.

Detailed buffering and sequencing mechanisms remain implementation/subsystem decisions.

If two relevant events have the same effective P&P timestamp, the responsibility interpreting those events defines the consequence of equality. P&P does not require a universal tie-breaking mechanism merely to force an artificial order.

START and GO are distinct concepts:
- START begins the operational start procedure;
- GO is the authoritative P&P timestamp at which competition becomes active.

## 6. Completion, stop and reset

Normal completion originates from the Race Engine because it owns the competition rules.

Race Control then performs the appropriate session-lifecycle transition and operational consequences.

Manual STOP originates from Race Control and is distinct from normal competition completion. Stopping a race must not manufacture a normal competition-completion event or result. The later product design may decide how a partial stopped race is displayed, retained, discarded or restarted.

RESET of an individual race is requested operationally by Race Control, but Race Control must not reach into Race Engine state and directly zero lap counts, timers or other competition variables. The Race Engine owns the creation and clearing of its own competition state.

An individual-race reset/restart, abandonment/reset of a larger competition and a factory/system reset are distinct operations owned at their appropriate boundaries. They must not be collapsed into a universal `resetEverything()` operation.

Starting a new individual race creates fresh individual-race state within the already-running P&P system. It does not normally reinitialise unrelated services, devices, installation configuration, retained results or longer-lived competition state.

## 7. Race modes

Race modes are replaceable rule sets within the common Race Engine framework rather than a single growing collection of mode-specific conditions.

Known modes include:
- lap race;
- timed race;
- practice;
- rally;
- drag racing;
- future modes.

Different modes may have different concepts of start, competitor, completion and result while retaining the same Race Control / Race Engine responsibility boundary.

Practice demonstrates that a mode need not have a normal competition finishing condition: Race Control still owns lifecycle while the Race Engine times laps and maintains practice state until operational STOP.

## 8. Optional race features

Optional race features are separate from race modes.

For example, simulated fuel may operate across more than one mode and maintains its own competition-feature state while consuming relevant mapped semantic events.

**Race Mode defines the fundamental form and completion rules of the competition. Optional Race Features add behaviours that can operate across more than one race mode.**

The Race Engine must not know detector identities or sensor technologies. A fuel/pit feature may consume an event such as:

`Lane 1 : PIT_ENTRY : ACTIVE : time`

but not `Detector 5` or a particular sensor API.

Physical enforcement of a feature is possible only where installed hardware exposes the required capability.

## 8.1 Start/Finish first-crossing behaviour

Lap-race behaviour must support the physical starting-grid arrangement relative to the Start/Finish detector.

The grid/Start-Finish relationship is installation configuration because it describes the physical track arrangement. When a Lap Race is prepared, the effective first-crossing behaviour derived from that installation configuration becomes part of the fixed session definition for that race.

Where cars start **after** the Start/Finish detector in the direction of travel, they cross the detector shortly after GO. That first post-GO crossing does not complete a lap; it establishes the timing origin for Lap 1. The next Start/Finish crossing completes Lap 1.

Where cars start **before** the Start/Finish detector in the direction of travel, they travel almost a complete circuit before their first post-GO crossing. That first Start/Finish crossing completes Lap 1, timed from GO.

This behaviour is not inferred by the detector or Event Mapping. The detector reports the same physical event in either arrangement; the Lap Race rule set interprets it according to the fixed session definition. Other race modes may interpret the same mapped Start/Finish event differently according to their own rules.

## 9. Hardware, availability and outputs

Race Engine does not discover hardware failures.

The Registry owns current device/capability availability information. Race Control receives or obtains availability information relevant to the current session and decides its operational consequence.

When a START request is made, Race Control performs the final authoritative readiness check against the Registry's current capability availability before accepting the start. Presentation may indicate apparent readiness, but it does not authorise a session to start.

Loss of an optional capability need not stop a session. Loss of a capability essential to meaningful continuation may require the session to be stopped or restarted.

Race Engine does not manipulate physical outputs.

Race Control requests logical operational actions through the established output architecture. For example, track power is coordinated by Race Control through Output Mapping; neither Race Control nor Race Engine manipulates GPIO or transport-specific hardware directly.

Detailed false-start behaviour remains a later design decision and depends upon the physical/start arrangement. In particular, a system in which lane power is held off until GO cannot use pre-GO vehicle movement in the same way as a continuously powered arrangement.

## 10. Information boundaries

Each component publishes and receives information only through its defined boundaries. It may publish information for which it is the authoritative owner and consume information made available through its defined interfaces. It must not bypass those boundaries to read, alter or reproduce another component's internal state.

A source reports what happened at its own boundary; it does not need to know or control the eventual consequence. For example, a physical control can report that a button was activated just as a detector reports a state transition. The responsibility receiving that information decides what it means in its own context. Where an interface deliberately defines a genuine operation such as PREPARE, GO or RESET between Race Control and Race Engine, that operation remains part of that defined boundary.

Race Engine consumes mapped semantic competition events. It does not require knowledge of:
- sensor model;
- detector transport;
- I2C/GPIO details;
- ESP-NOW/Wi-Fi details;
- browser/client identity.

Similarly, Race Control should receive capability/function availability rather than unnecessary low-level failure details.

Operational consumers receive information in the form appropriate to their boundary and do not depend on upstream implementation or transport. Diagnostics may trace information across boundaries without becoming part of the operational dependency chain.

This boundary should be enforceable structurally during implementation wherever practical.

## 10.1 Authoritative facts, current state and persistence

P&P distinguishes between three information behaviours:

- **facts/events** describe something that happened;
- **current state** describes what is true now;
- **persistent information/configuration** describes information that must remain available across the relevant lifecycle.

Authoritative race facts/events are published once through a common logical interface. Interested consumers may consume them independently. The producer does not require knowledge of those consumers, and consumers must not independently recreate or alter authoritative race state.

Current-state snapshots may combine information from several authoritative owners for consumption, but the snapshot does not become another owner of that information. Presentation never owns authoritative P&P data; it displays or derives views from information supplied through defined boundaries by the components that own it.

A newly connected or recovering current-state consumer obtains the authoritative current state it needs and then consumes new relevant facts/events. It need not replay every event that occurred before it connected.

Continuous values must not generate streams of authoritative events merely because their displayed value changes with time. Significant transitions may be published as facts/events, while consumers obtain or derive the current value from authoritative state and P&P System Time where appropriate.

The authoritative fact/state exists independently of whether a particular consumer successfully receives a live notification. Current-state consumers can resynchronise; obsolete transient notifications may simply be missed; information that P&P has decided must persist is retained by the appropriate persistent-storage responsibility rather than depending on a live consumer receiving a notification.

Consumers may derive information for their own presentation or local use, but this does not create new authoritative P&P state or facts. If derived information must itself be authoritative or shared, it is produced by the responsibility that owns that meaning and exposed through its defined boundary.

Retained history may preserve earlier facts for later retrieval without requiring those facts to remain live current-state information.

## 11. Testability

The Race Control / Race Engine boundary must support controlled test implementations in either direction.

Examples:
- Race Control can be tested with a stub Race Engine returning predetermined competition facts, state, completion and status.
- Race Engine can be tested with predetermined mapped events and session definitions without real sensors, Event Mapping or browser clients.

Production and test implementations must use the same defined interfaces; testing must not rely on special shortcuts through the production architecture.

Neither side may reach into the other's internal variables.

## 12. Proportionality

The design must remain proportionate to a consumer slot-car timing system.

The separation exists to provide clear ownership, replaceability, testability and resistance to accidental architectural coupling. It is not justification for unnecessary layers, distributed-state machinery or high-availability engineering.

**Design broadly. Implement narrowly.**
