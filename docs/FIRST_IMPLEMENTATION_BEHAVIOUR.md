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
- optional race features off.

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

A missing known device or capability retains its remembered assignment but is marked unavailable.

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

It shows the effective forthcoming race configuration rather than the machinery used to create that configuration.

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

Immediately after START is requested, P&P performs a final readiness evaluation so that a capability lost since the screen was displayed cannot silently invalidate the race.

No unnecessary confirmation screen is required.

## 10. Fixing the race configuration

When START is accepted, the effective session configuration becomes fixed for that race.

Normal race operation may change race state, but ordinary configuration changes must not silently alter the active session definition.

START begins the start procedure configured for that race.

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

RESTART RACE abandons the current attempt and begins the same fixed race configuration again through its normal start procedure.

END RACE abandons the current race and returns the user to the pre-race state.

The detailed race, timing, detector and output behaviour while paused remains to be defined.

## 14. Normal race completion

Normal completion is determined by the Race Engine according to the selected race rules.

For a Lap Race, the initial design supports three configurable finish behaviours:

1. **Immediate finish** — when the first competitor completes the configured number of laps, the race ends immediately. Other competitors' finishing positions/state are determined at that point.
2. **Complete current lap** — when the first competitor completes the configured number of laps, each remaining active competitor continues until their next Start/Finish crossing, at which point that competitor is finished.
3. **Complete full race distance** — every active competitor continues until they have completed the configured number of laps.

More sophisticated finish behaviour may be added later without changing the general Race Control/Race Engine responsibility split.

The selected finish rule is fixed as part of the race configuration when START is accepted.

## 15. Results

When a race completes normally, P&P presents the authoritative final results appropriate to that race.

The detailed contents and layout of the results screen, retained race history and post-race options are later product-design decisions.

## 16. Implementation discipline

The first implementation must implement the behaviour defined above through the responsibilities and boundaries established by the governing P&P documents. It must not introduce first-product shortcuts that bypass or redefine those boundaries.

Where this document deliberately leaves behaviour or implementation undecided, implementation must not silently turn that omission into a permanent architectural decision.

**Design broadly. Implement narrowly.**

## 17. Pre-implementation consistency review

Before the first implementation specification is treated as settled for coding, all governing and detailed-design documents must be cross-checked for:

- contradictions;
- stale wording;
- duplicated decisions;
- places where later decisions have refined earlier ones.

Any conflicts must be resolved so that implementation is working from one consistent set of instructions.

In particular, the startup/reconciliation ordering in SYSTEM_LIFECYCLE.md must be reviewed against the behaviour defined in this document.
