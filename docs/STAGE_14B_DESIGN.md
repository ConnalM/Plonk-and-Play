# Stage 14B Design — Open Practice

**Status: PROPOSED / REVIEWED**

**Implementation status: NOT STARTED**

Stage 14B is the next implementation slice after the accepted Stage 14A variable-entry refactor. It implements Open Practice only. It does not implement Timed Practice, Endurance, Timed Stage/Rally, Drag, or any other new mode.

## Product boundary

Open Practice is Practice, not an official race. It has no winner, ranking, finishing position, official result, PB update, Track Record update or official History entry. All Practice information is session-local and non-official.

The number of simultaneous Practice cars is the number of configured active entries, subject to the existing firmware implementation capacity. Two lanes remain the default/Taster configuration. The implementation capacity is not a product maximum.

Open Practice has no configured duration and no lap target. It continues until the Race Director ends the session.

## Existing architecture retained

Open Practice uses the existing Race Control, Race Engine, Session Definition, Input Module, Message Bus and Noticeboard boundaries:

- Browser/Taster submits an authorised request through Browser Interface.
- Race Control owns authority and session lifecycle.
- START creates an immutable Practice Session Definition with the configured active entries and role mappings.
- Input Module publishes ordinary clean `INPUT_EVENT`s through the Message Bus.
- Race Engine interprets those events using the Session Definition and owns Practice timing/state.
- Noticeboard State is authoritative for reconstruction.
- Browser presence never affects Practice timing.

No parallel Practice timing path, Browser-to-Race-Engine path or Race-Control access to Input Module private state is permitted.

## Mode and lifecycle

The active lifecycle remains the existing `RACING` value with authoritative `sessionMode = OPEN_PRACTICE`. No new `PRACTISING` lifecycle value is introduced.

The operational flow is:

1. READY: no Practice session exists.
2. START accepted: the Practice Session Definition is created and Practice becomes active immediately.
3. `RACING` plus `OPEN_PRACTICE`: timing is active with no countdown, starting lights or GO requirement.
4. PAUSED: Practice activity is stopped and incomplete timing is discarded.
5. RESUME accepted: Practice becomes active immediately and every lane waits for a new timing origin.
6. END SESSION accepted: Practice ends and the system returns towards READY/Home.

Practice never enters normal Lap Race `FINISHED` and never publishes a normal competition-completion result.

## Practice timing

Each active entry independently stores:

- whether it is waiting for a timing origin;
- its timing-origin Relevant Time when established;
- completed lap count;
- last completed lap;
- best completed lap;
- session-fastest information where straightforward.

At Practice start, every lane waits for a timing origin. The first eligible Start/Finish crossing for a lane establishes that lane's timing origin. Each subsequent eligible crossing completes one lap using the Relevant-Time difference and establishes the next timing origin.

Relevant Time remains authoritative over delivery or processing order. Existing Input Module debounce/re-arm and normal Message Bus delivery guarantees remain unchanged.

Sector timing and theoretical-best calculations/presentation are explicitly optional for this slice. Their absence must not fail acceptance. They may be added later through the existing architecture.

## Pause and Resume

Pause is deliberately simpler than Lap Race Honour/Grid Restart:

- Pause stops Practice activity.
- Completed Practice laps and statistics remain.
- Any incomplete lap is discarded.
- Crossings while paused do not advance Practice timing.
- Resume is immediate.
- After Resume, each lane waits for a new timing-origin crossing.
- The following crossing completes the next timed lap.

The existing Relevant-Time, Message Bus settlement/fence and authoritative event-boundary rules still apply underneath. Any internal boundary needed to classify events is an implementation detail and is not a new user-visible Practice concept.

## Operations and authority

The existing generic session-operation contract is used. The architecture already defines generic RESUME and session stop concepts, but the current firmware operation enum has PAUSE and `EndRace` and does not yet provide generic `Resume` or `EndSession` values. `EndRace` is not semantically suitable because it represents race abandonment/ending.

The smallest compatible contract change is therefore:

- append generic `Resume` and `EndSession` operation values;
- continue using the existing `SESSION_OPERATION_REQUEST` and `SESSION_OPERATION` Message Bus types;
- let Race Control validate and interpret those generic operations according to the active Session Definition mode.

No Practice-specific operation or Message Bus type is introduced. Existing Message Bus numeric values remain unchanged; any appended values are never renumbered.

Only Race Director/SMUG context may start, pause, resume or end Open Practice. Spectator and ordinary MUG/driver contexts cannot end the overall session. Forged Browser role fields cannot change this.

## Browser and Taster

Open Practice must be selectable and operable through the real Browser/product path on the ESP32. The minimum diagnostic control layer is:

- an Open Practice mode selection;
- START;
- PAUSE;
- RESUME;
- END SESSION;
- per-lane Practice state showing lap count, last lap, best lap and session-fastest information where available.

Polished UI is outside this slice.

The existing Taster surface remains unchanged: it continues to provide its documented two-lane Lap Race controls. This slice does not add Practice selection to Taster.

## State, Facts and presentation

Authoritative State includes session mode, lifecycle, active entry collection, per-entry lane/entry identity, completed laps, last/best Practice lap where available, pause information and existing integrity/validity fields.

No winner, rank, finish time, official result or result-sealed Practice state is created. Existing common PAUSED and RESUMED Facts may be reused where their semantics remain exact. Any Practice-ended Fact is optional and must use the normal Message Bus path.

An optional lightweight non-official end summary may be implemented only if it is essentially free and non-disruptive. It is not an acceptance requirement and must never become an official Results object.

## Persistence, History and records

Open Practice does not create a CompletedRaceResult, write official History, update PBs, update Track Records, alter record eras or introduce a persistent Practice-results schema.

Existing controller-restart rules remain authoritative: an interrupted active Practice session is not reconstructed as an official result. Persistent Race Setup/configuration may continue to follow existing rules.

## Explicit exclusions

This slice does not define or implement:

- Timed Practice;
- Endurance;
- Timed Stage/Rally;
- Drag;
- winner/ranking/finish policies;
- official PB or Track Record processing;
- persistent Practice History;
- sector timing or theoretical-best calculations as required features;
- new Yellow/Red/RMS or Power Module behaviour;
- new Taster Practice controls;
- polished final UI.

## Compatibility requirement

All applicable Stage 1–14A behaviour and acceptance remains unchanged. The complete Stage 1–14A regression campaign is required for Stage 14B acceptance.

