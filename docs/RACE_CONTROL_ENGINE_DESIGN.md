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
   - original P&P timestamps are preserved.

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

This does not prescribe whether the implementation copies, references, serialises or otherwise represents the definition.

## 5. Time and event ordering

Race Control establishes authoritative session-state boundaries using P&P System Time.

Where timing affects competition interpretation, the Race Engine uses event timestamps relative to those boundaries rather than communication arrival order.

For example, if PAUSE is effective at P&P time 250.000:
- an event timestamped 249.999 belongs before the pause even if it arrives afterwards;
- an event timestamped 250.001 belongs after the pause.

Detailed buffering and sequencing mechanisms remain implementation/subsystem decisions.

START and GO are distinct concepts:
- START begins the operational start procedure;
- GO is the authoritative P&P timestamp at which competition becomes active.

## 6. Completion, stop and reset

Normal completion originates from the Race Engine because it owns the competition rules.

Race Control then performs the appropriate session-lifecycle transition and operational consequences.

Manual STOP originates from Race Control and is distinct from normal competition completion.

RESET is requested operationally by Race Control, but Race Control must not reach into Race Engine state and directly zero lap counts, timers or other competition variables. The Race Engine owns the creation and clearing of its own competition state.

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

## 9. Hardware, availability and outputs

Race Engine does not discover hardware failures.

The Registry owns current device/capability availability information. Race Control receives or obtains availability information relevant to the current session and decides its operational consequence.

Loss of an optional capability need not stop a session. Loss of a capability essential to meaningful continuation may require the session to be stopped or restarted.

Race Engine does not manipulate physical outputs.

Race Control requests logical operational actions through the established output architecture. For example, track power is coordinated by Race Control through Output Mapping; neither Race Control nor Race Engine manipulates GPIO or transport-specific hardware directly.

Detailed false-start behaviour remains a later design decision and depends upon the physical/start arrangement. In particular, a system in which lane power is held off until GO cannot use pre-GO vehicle movement in the same way as a continuously powered arrangement.

## 10. Information boundaries

Each subsystem receives the information necessary to perform its responsibility, not implementation details that produced that information.

Race Engine consumes mapped semantic competition events. It does not require knowledge of:
- sensor model;
- detector transport;
- I2C/GPIO details;
- ESP-NOW/Wi-Fi details;
- browser/client identity.

Similarly, Race Control should receive capability/function availability rather than unnecessary low-level failure details.

This boundary should be enforceable structurally during implementation wherever practical.

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
