# P&P First Implementation Behaviour

**Status:** Draft v0.1  
**Parent architecture:** SYSTEM_ARCHITECTURE.md v1.0

## 1. Purpose

This document defines the customer-visible and system behaviour required for the first useful P&P implementation.

It does not redefine the P&P architecture and does not prescribe software classes, protocols, packet formats, browser technology, storage mechanisms, hardware pinouts or other implementation details.

The first implementation is deliberately narrow, while its behaviour must remain consistent with the general P&P architecture.

Initial implementation scope:

- two-lane analogue slot-car timing;
- Start/Finish detection;
- basic Lap Race;
- browser-based control and display.

The behaviour described here should use general P&P mechanisms rather than special-case logic that only works for this first product.

**Design broadly. Implement narrowly.**

## 2. General startup behaviour

P&P uses the same general startup approach regardless of the amount of equipment installed.

Conceptually:

```text
POWER ON
   ↓
Initialise P&P
   ↓
Discover what is actually available
   ↓
Reconcile discovered equipment with remembered information
   ↓
Apply remembered configuration where still valid
   ↓
Apply defaults where appropriate
   ↓
Determine what configured functions are currently available
   ↓
Present the resulting system to the user
```

Discovery establishes what actually exists.

Remembered configuration and defaults determine what P&P proposes to do with it.

Discovery does not itself imply assignment or use.

## 3. First startup and defaults

On first startup there is no remembered customer configuration.

P&P discovers all supported equipment that is actually available and then applies the factory/default configuration.

The initial base configuration is:

- two lanes;
- Lap Race;
- 10 laps;
- optional race features off;
- the recommended base physical arrangement places each lane's Start/Finish sensor before the Start/Finish line in the direction of travel.

With that recommended arrangement, a car starts before its Start/Finish detector. After GO, the first Start/Finish crossing completes Lap 1, timed from GO. This is the default first-implementation behaviour; alternative physical arrangements remain supported through installation configuration and Race Setup as defined by `RACE_CONTROL_ENGINE_DESIGN.md`.

Where the base product or remembered installation makes an assignment unambiguous, P&P may make it automatically. Where more than one valid assignment is possible, P&P must not guess.

For example, if the base product defines two physically distinct standard timing inputs as Lane 1 and Lane 2 Start/Finish, P&P may assign those roles automatically. Discovering several otherwise identical unassigned detectors does not by itself make their roles unambiguous.

Equipment not required by the default configuration remains available but unassigned.

P&P must not invent purposes for equipment merely because it has discovered it.

## 4. Unassigned equipment

Useful discovered equipment that remains unassigned is brought to the user's attention.

For example:

`2 unassigned detectors found`

The user may choose to configure that equipment or ignore it for now.

Unassigned optional equipment does not prevent normal operation of the configured system.

The objective is to make the user aware of useful additional capability without forcing unnecessary setup before racing.

## 5. Remembered configuration

On subsequent startup, remembered configuration takes precedence over factory defaults where it remains valid.

P&P discovers the equipment actually available and reconciles that reality with the remembered configuration.

A missing known device or capability retains its remembered/default role proposal but is marked unavailable. The active racing role is not owned by the device or Input Module; roles actually used by a race are frozen into its Session Definition when START is accepted.

P&P must not silently move another device into that role merely because it is available.

## 6. Required and optional capabilities

Whether a capability is required is determined by the selected activity and configuration, not by the hardware itself.

For example, the same compatible detector might be:

- required when assigned as the only Start/Finish detector for an active lane;
- optional when assigned as a sector detector for a Lap Race.

Loss of an optional capability must not unnecessarily prevent the activity from operating.

Loss of a required capability prevents the affected activity from starting or continuing where meaningful operation is no longer possible.

The selected activity and its enabled features declare the capabilities upon which they depend and whether those dependencies are required or optional.

These dependencies must be declarative and extensible.

P&P must not depend on a fixed central firmware table that has to be modified whenever a future supported activity, feature or capability is introduced.

A genuinely new activity may still require new executable race logic. Declarative configuration is not expected to create functionality which P&P does not otherwise implement.

## 7. Missing equipment and customer recovery

When configured equipment is unavailable, P&P tells the user:

- what is unavailable;
- what effect that has on the selected activity.

If only optional functionality is unavailable, the activity may still be started.

If required functionality is unavailable, START is unavailable.

P&P may offer:

- Try Again;
- Setup.

Where another currently available capability is known to be compatible with the missing required role, P&P may additionally offer the user a simple reassignment.

For example, an available sector detector might be offered as a replacement for a missing Start/Finish detector if it provides the required compatible detector capability.

P&P must not perform such reassignment silently.

The user confirms the change and the resulting assignment becomes configuration.

This is a general compatibility/reassignment mechanism, not a collection of individually programmed fault scenarios.

## 8. Pre-race screen

The main pre-race screen is primarily a summary of what will happen if the user presses START.

It shows the effective forthcoming **Race Setup** rather than the machinery used to create it.

Frequently changed race settings should be directly accessible where practical.

Less frequent installation configuration, including detector assignments and discovered equipment, is available through Setup.

The screen should not be cluttered with hardware details during normal operation.

Hardware/configuration information becomes prominent when the user needs to know or act upon it.

## 9. START and readiness

P&P does not require a separate prominent READY indicator.

The START control itself communicates readiness.

If all requirements of the selected activity/configuration are satisfied, START is available.

If a required capability is unavailable, START is unavailable and P&P explains why.

Missing optional equipment may be reported without preventing START.

Immediately after START is requested, Race Control performs the final readiness decision using the Registry's authoritative current capability availability, so that a capability lost since the screen was displayed cannot silently invalidate the race. Presentation may display whether START appears available, but it is not the authority that permits the session to start.

No unnecessary confirmation screen is required.

## 10. Fixing the Session Definition

Before START, the SMUG changes the proposed **Race Setup**. The SMUG does not edit a Session Definition.

When START is accepted, P&P validates the Race Setup against the applicable working installation/capability information and creates the fixed **Session Definition in RAM** for that race. It freezes both the race choices and the session-specific roles of every input, output and other capability required for that race.

Normal race operation may change race state, but ordinary configuration changes must not silently alter the active Session Definition.

START begins the start procedure defined for that race.

## 11. Start procedure

Race Control coordinates the configured start procedure and establishes the authoritative GO time.

Outputs participating in that procedure receive the appropriate logical actions through their defined boundaries.

Where independent components must perform timing-critical actions together, P&P coordinates them sufficiently accurately for their intended purpose.

The mechanism used to achieve this is an implementation decision.

Presentation latency must not determine official race timing.

## 12. Running race presentation

A display receives the authoritative information necessary for its configured purpose.

Race-owning components do not tailor or send information directly to individual displays.

The running presentation shows information relevant to the current race and configured display purpose. Features not in use need not occupy presentation space.

Whether implementation distributes all relevant authoritative information to consumers or filters it according to consumer requirements is an implementation decision and must not affect ownership or race operation.

## 13. Running race control

The primary Race Director intervention during a running race is PAUSE.

The basic control flow is:

```text
RUNNING
   ↓
PAUSE
   ↓
RESUME / RESTART RACE / END RACE
```

RESUME continues the existing race.

RESTART RACE abandons the current attempt and begins the same fixed Session Definition again through its normal start procedure.

END RACE abandons the current race and returns the user to the pre-race state.

### 13.1 Basic PAUSE behaviour

PAUSE suspends competition timing and competition progress without ending the race.

The default restart method is **Honour restart**:

- MUGs stop their cars as soon as reasonably possible when PAUSE/Yellow is called.
- Cars should remain where they stopped. If a car must be handled, it should be returned as close as reasonably possible to that position.
- P&P freezes competition timing while paused. Detector activity during the paused interval does not advance competition progress.
- The interrupted lap is preserved. Time spent paused is excluded from that lap time.
- RESUME uses a short restart countdown and one authoritative scheduled restart instant so all MUGs receive a predictable common restart rather than racing becoming live at the instant the RESUME Request is pressed.

This is deliberately an honour-system behaviour for the base product. P&P does not claim to know a car's physical position between configured detection points.

An alternative **Grid Restart** may be selected where a more controlled restart is wanted:

- cars are returned to defined Start/Finish grid positions;
- incomplete lap progress at PAUSE is discarded;
- completed laps and other completed competition results remain;
- the race resumes from the defined grid through the restart sequence.

The exact physical grid placement relative to the Start/Finish detectors must be defined by track/setup guidance so placing cars on the grid does not create an unintended competition crossing.

Future optional hardware such as an REU may provide sufficient authoritative position/progress information for a more accurate position-aware restart. The detailed REU mechanism is not defined here and must not be assumed by the base implementation.

Detailed RMS Yellow/Red state transitions, track-power behaviour and other hardware-dependent pause effects remain deliberately deferred.

## 14. Normal race completion

Normal completion is determined by the Race Engine according to the selected race rules.

For a Lap Race, the initial design supports three configurable finish behaviours:

1. **Immediate finish** — when the first competitor completes the configured number of laps, the race ends immediately. Other competitors' finishing positions/state are determined at that point.
2. **Complete current lap** — when the first competitor completes the configured number of laps, each remaining active competitor continues until their next Start/Finish crossing, at which point that competitor is finished.
3. **Complete full race distance** — every active competitor continues until they have completed the configured number of laps.

More sophisticated finish behaviour may be added later without changing the general Race Control/Race Engine responsibility split.

The selected finish rule becomes part of the fixed Session Definition when START is accepted.

## 15. Results

When a race completes normally, P&P presents the authoritative final results appropriate to that race.

The product-level results screen, retained History behaviour and post-race options are defined in `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md` and the relevant display/mode specifications. Detailed storage schema and implementation remain implementation decisions.

## 16. Implementation discipline

The first implementation must implement the behaviour defined above through the responsibilities and boundaries established by the governing P&P documents. It must not introduce first-product shortcuts that bypass or redefine those boundaries.

Where this document deliberately leaves behaviour or implementation undecided, implementation must not silently turn that omission into a permanent architectural decision.

### 16.1 First software implementation milestone

The first implementation milestone is a **software/architecture prototype**, not a declaration that the complete first saleable hardware product is finished.

Wokwi may be used as the development test bench for the ESP32-side software, with simulated sensors, buttons and other suitable devices standing in for physical hardware while real browser clients exercise the P&P user/presentation boundary.

Wokwi is **not part of the P&P architecture or product contract**. No architectural responsibility, interface or behaviour may depend on Wokwi-specific facilities. A simulated device must be replaceable by real hardware implementing the same P&P boundary without requiring race/session logic to be redesigned.

The prototype should prove the architectural path and boundaries, including:

- physical/simulated input through Input Devices and the Input Module, producing stable device/capability events without race meaning;
- the common P&P communications bus as the normal inter-component path;
- separate Race Control and Race Engine responsibilities;
- authoritative timing, State and Events/Facts;
- Requests and Request Results;
- scheduled GO coordination;
- browser initial synchronisation and current-State refresh;
- multiple browser clients;
- disconnect/reconnect and recovery to current authoritative State;
- rapid input/state changes without browser/network activity delaying timing-critical race processing;
- configuration and persistence behaviour required by the implemented features;
- logical output boundaries using simulated outputs where real output hardware is not yet present.

Product features should then be added through these same boundaries rather than bypassing them for convenience.

### 16.2 Prototype implementation sequence

The prototype should be built in small proving stages so that each stage leaves a testable system and validates the next architectural boundary:

1. **ESP32 skeleton** — boot, P&P System Time, Memory access, working configuration in RAM, the common P&P communications bus and module boundaries exist; no race behaviour is required.
2. **One simulated Input Device** — prove a physical/simulated event is cleaned by its Input Device and the Input Module produces the standard physical event identifying the stable input/capability and event time, with no lane or race-role meaning attached.
3. **P&P Message Bus delivery** — publish that physical Input Event on the common bus and prove an authorised subscriber receives it without a private point-to-point path.
4. **Session role assignment** — create a minimal accepted Session Definition that assigns that stable input as Lane 1 Start/Finish, and prove the assignment is frozen for the session while the Input Module remains unchanged.
5. **Race Engine** — bus-delivered physical Input Events interpreted through the Session Definition produce authoritative lap counts and lap times without requiring a browser.
6. **Race Control** — prove READY → STARTING → scheduled GO → RACING → FINISHED, with Race Control and Race Engine communicating through the common bus and retaining their separate ownership boundaries.
7. **Browser connection** — prove authoritative Noticeboard State, `NOTICEBOARD_CHANGED` notifications and Events/Facts can be presented without the browser owning race operation.
8. **Browser Requests** — START is the first operational Request, including Accepted/Rejected Request Results, creation of the fixed Session Definition in RAM after acceptance and authoritative State change after acceptance.
9. **Two lanes and rapid-event testing** — add a second simulated detector and deliberately exercise closely spaced and rapid inputs; browser/network speed must not compromise authoritative timing.
10. **Multiple browsers and recovery** — exercise several clients, disconnection, reconnection and a new browser joining mid-race; each must obtain the current authoritative Noticeboard State it needs without affecting the race.

**Stages 1–10 are the first prototype checkpoint.** At that checkpoint the communication and architectural approach must be assessed before additional product behaviour is piled on top.

After that checkpoint, continue through the same established boundaries:

11. **PAUSE / RESUME** — Honour restart first, then Grid Restart.
12. **Results and persistence** — completed race, Results, History, Race Again and appropriate reboot persistence.
13. **Outputs** — assign required output roles in the Session Definition, resolve them to stable output capabilities, and prove simulated Output Devices execute device-level actions without the Output Module acquiring lane, MUG or race-mode knowledge.

Practice, Endurance, Timed Stage, Drag and other product features can then be added through the proven architecture rather than being required to prove the architecture itself.

**Design broadly. Implement narrowly.**

## 17. Pre-implementation consistency review

Before the first implementation specification is treated as settled for coding, all governing and detailed-design documents must be cross-checked for:

- contradictions;
- stale wording;
- duplicated decisions;
- places where later decisions have refined earlier ones.

Any conflicts must be resolved so that implementation is working from one consistent set of instructions.

The startup/reconciliation ordering is governed by SYSTEM_LIFECYCLE.md: load remembered persistent state into working RAM first, then discover and reconcile actual equipment, initialise/synchronise it as required, establish current capability availability, and finally present the effective forthcoming Race Setup. FIRST_IMPLEMENTATION_BEHAVIOUR.md defines the customer-visible behaviour produced by that lifecycle and does not introduce a competing startup sequence.
