# P&P START Scheduling and Presentation Reconciliation — 2026-10-26

**Status:** Authoritative documentation reconciliation for the current P&P design.  
**Scope:** START scheduling, presentation timing, Browser/display clock relationship and multi-display coordination.  
**Code impact:** None. This is documentation only.

This reconciliation resolves contradictory wording that accumulated between the governing architecture and later Browser/display implementation handover material. It does not change the core authority model: **P&P owns race timing, START and GO. Presentation clients do not.**

## 1. Decision

Once a START Request is accepted, Race Control establishes a complete future start schedule in **P&P System Time** sufficiently far in advance for connected presentation devices to prepare and render it locally.

The schedule includes:

- a deliberate preparation/READY interval before the first red light;
- the configured three-red or five-red start sequence;
- the configured GO style;
- any configured random pre-GO hold/delay, chosen **once by Race Control**;
- the single authoritative future GO instant.

The normal human experience is therefore:

`START accepted → preparation / READY → red-light sequence → configured random hold (if any) → GO`

START acceptance is not GO and must not cause the first red light to illuminate immediately merely because the START Request has been accepted.

The exact preparation duration and permitted random-delay range remain product/configuration values to be frozen separately. The expected normal design places GO several seconds into the future (approximately 7–8 seconds was identified as a plausible working range), but that estimate is **not** frozen by this document.

## 2. Authority

P&P remains authoritative throughout.

Race Control:

- accepts or rejects START;
- freezes the applicable Session Definition;
- selects any configured random delay once;
- establishes the authoritative future schedule and GO;
- publishes enough information for permitted consumers to present that schedule;
- causes authoritative race operation to use the same GO instant.

A Browser, local display, Taster, audio/output device or other presentation consumer:

- may use its own local monotonic clock/timers to render the **P&P-prescribed** schedule;
- must maintain a sufficiently accurate known relationship between its local scheduling clock and P&P System Time;
- must not choose, alter, postpone or independently infer authoritative GO;
- must not become an authoritative race/start state machine;
- must not make race timing depend on network arrival time or presentation timing.

Local presentation scheduling is therefore permitted and required where needed for coordinated START presentation. What is forbidden is **local authority**, not local execution of P&P's authoritative schedule.

## 3. Clock relationship

Presentation devices do not need correct UTC, civil time, Internet time or an RTC for this function.

They need a sufficiently accurate mapping between their local monotonic scheduling clock and **P&P System Time**. If P&P System Time is offset from real-world civil time, that does not matter to race operation provided the relationship used for coordinated presentation is known accurately enough.

The exact clock-synchronisation/offset-estimation mechanism remains an implementation decision. It may measure offset, round-trip delay and drift as necessary. The implementation must define and prove an acceptable synchronisation tolerance rather than assume that network message arrival is simultaneous.

If a client cannot establish or maintain adequate timing relationship/schedule acquisition, it must not pretend that its presentation is synchronised. This does not alter authoritative P&P race operation.

The low-stakes human-readable display clock described elsewhere is not itself the race timing authority and must not be confused with the high-resolution P&P System Time relationship required here.

## 4. Complete future schedule

The architectural minimum is that each presentation consumer receives enough authoritative information in advance to determine every required presentation transition relative to the same GO.

Conceptually, a schedule may be represented as:

- Start/Session identity;
- preparation/READY timing;
- Red 1 timing;
- Red 2 timing;
- Red 3 timing;
- Red 4/5 timing where configured;
- GO style;
- authoritative GO time.

This does not require those fields to be duplicated literally in one packet if the immutable Session Definition/start configuration plus authoritative GO already determines them unambiguously. Packet/structure encoding remains an implementation decision.

The important contract is that different presentation implementations must not independently invent different timing rules. The random delay is selected once by Race Control and is therefore the same schedule for PC Browser, tablet Browser, future local 7-inch display and any other authorised presentation/output consumer.

## 5. GO_SCHEDULED

`GO_SCHEDULED` retains its existing core meaning:

> GO for the active session occurs at this exact future P&P System Time.

It is explicitly **not** "GO now".

The START presentation contract supplements that fact: consumers must also have the applicable authoritative start-sequence definition/schedule early enough to prepare the presentation against that GO.

Whether this is carried by an expanded `GO_SCHEDULED` payload, a companion schedule message, immutable Session Definition data or another clean representation is an implementation decision. The semantic requirements above are mandatory.

## 6. Browser/display behaviour

Connected healthy presentations schedule the prescribed sequence locally against their mapping to P&P System Time. They do not wait for P&P to push Red 1, Red 2, Red 3, etc. across the network at the instant each lamp should change.

Ordinary `NOTICEBOARD_CHANGED` coalescing remains valid for ordinary current State. It is **not** the timing mechanism for a coordinated start-light sequence.

A Browser may therefore use local timers for presentation of the authoritative future schedule. This does not violate the rule that the Browser must not run an independent authoritative race clock or start state machine.

At authoritative GO:

- Race Engine/Race Control behaviour is governed by P&P System Time;
- each healthy synchronised presentation reaches its configured GO presentation against the same scheduled instant;
- presentation delay, failure, suspension or disconnection does not move GO or alter race timing.

## 7. Reconnection and late arrival

A newly connected or recovering presentation must reconstruct what is true **now**.

If it reconnects during a scheduled start sequence and has sufficient current authoritative schedule/time information, it renders the correct current presentation point. It does not replay obsolete red-light stages merely because it missed them.

If it reconnects after GO, it presents current authoritative RACING/state. It does not replay the start sequence.

The same principle applies to RESUME where competitive Resume uses a fresh STARTING/start-presentation/GO procedure.

## 8. Random delay

Any configured random pre-GO delay is a Race Control decision, not a presentation-client decision.

Race Control selects the random value **once before publishing the final schedule**. The resulting GO is therefore identical for all consumers.

Browsers/displays must never each generate their own random hold. Doing so would create different GO presentations and violate the common authoritative schedule.

## 9. Preparation / READY interval

A deliberate preparation interval exists between accepted START and the first red light.

Its product purpose includes allowing a solo MUG or racing Race Director to press START, put down or reposition the control device, take up the driving controller and attend to the track before the visible countdown begins.

Presentation may use a suitable READY/GET READY treatment during this interval. Exact wording and visual treatment are UX decisions; the existence and timing of the preparation interval are race/start behaviour supplied by P&P rather than accidental network/UI delay.

## 10. Lap and ordinary live-event presentation are different

This reconciliation does **not** predict or schedule future laps.

A lap remains an authoritative occurrence determined after detector input by the Race Engine. `LAP_COMPLETED` is a distinct Fact/Event and resulting current State is exposed authoritatively.

Connected Browsers should receive and reconcile lap information promptly. A late/reconnecting Browser obtains current State rather than replaying all missed lap presentation events.

The observed approximately one-second tablet lag for ordinary lap presentation therefore remains a separate transport/acquisition/rendering defect to investigate. Scheduled START presentation does not excuse that lag.

## 11. HTTP/SSE transport work is separate

The current WP5 HTTP defect is also separate from START scheduling.

A slow/stalled Browser client must not monopolise the HTTP server such that another Browser or ordinary request such as `/health` or `/context` becomes unavailable. Network/presentation work must not block race-critical processing.

Framework/HTTP work may improve notification and state-acquisition latency, but it must not be used to preserve the wrong model of pushing each start-light transition live across the network.

The framework migration and async HTTP/SSE isolation work therefore remain independently reviewable from the START scheduling correction.

## 12. Reconciliation of existing documents

Until the affected documents are textually folded into this decision, interpret them as follows.

### `SYSTEM_ARCHITECTURE.md`

The existing requirement that presentation/output devices receive sufficient advance information to schedule local start presentation against the authoritative future GO remains correct and governing.

The statement that the exact clock-synchronisation mechanism is an implementation/prototyping decision also remains correct.

### `MESSAGE_CONTRACT.md`

The existing `GO_SCHEDULED` contract remains correct but is incomplete as a description of the full presentation schedule. It must be read together with this reconciliation: GO is one authoritative future instant within a P&P-prescribed start schedule, including preparation and any once-only Race Control random delay.

### `COMMON_DISPLAY_SPEC.md`

The phrase that Browser/physical presentation must not create "a local GO timer" is superseded where it could be read as prohibiting local scheduled presentation.

Correct interpretation:

> A presentation must not create or alter an authoritative GO. It may and, for coordinated START presentation, must use local scheduling to execute P&P's prescribed future start schedule against its known relationship to P&P System Time.

The existing three/five-light and Lights Out/Green visual rules remain otherwise unchanged.

### `IMPLEMENTATION_SPEC.md`

The sentence "The Browser must not recreate the start state machine from its own elapsed time" is superseded where it prohibits the intended scheduled presentation.

Correct interpretation:

> The Browser must not invent an authoritative start state machine or infer GO from its own elapsed time. It locally renders the P&P-prescribed future start schedule against its synchronised relationship to P&P System Time.

Any current implementation or test that requires successive current start-presentation snapshots to be the mechanism for start-light timing must be reconciled before START implementation proceeds.

## 13. Acceptance requirements to add/reconcile

Future START acceptance coverage must prove at least:

1. START acceptance does not immediately illuminate Red 1; the configured preparation interval occurs first.
2. Race Control chooses any configured random delay once and all consumers use the resulting common GO.
3. Two independently connected Browsers receive sufficient advance schedule information and present the sequence against the same authoritative GO rather than network-arrival timing.
4. Normal network latency/jitter does not materially separate the clients' intended light transitions/GO beyond the accepted synchronisation tolerance.
5. A delayed client does not cause another client to skip/coalesce required start-light stages.
6. Race Engine authoritative GO is unchanged by Browser/display delay or loss.
7. A Browser reconnecting during STARTING reconstructs the correct current point rather than replaying obsolete stages.
8. A Browser reconnecting after GO presents current RACING state and does not replay STARTING.
9. Three-red/five-red and Lights Out/Green configurations use the same authority/scheduling model.
10. RESUME for competitive modes uses the same scheduled-GO principle where a start presentation is required.
11. A client whose P&P-time relationship is outside the accepted tolerance does not falsely claim synchronised presentation.
12. Ordinary lap/event delivery remains prompt and is tested separately from predictable START scheduling.

The exact numeric synchronisation tolerance and final preparation/random-delay product values are to be established and frozen through implementation/prototyping evidence rather than guessed in this reconciliation.

## 14. Implementation boundary

This document does not authorise implementation.

Before START implementation:

- reconcile affected specifications/tests with this decision;
- identify the clock relationship/synchronisation mechanism;
- define the schedule representation and lifecycle;
- define behaviour when synchronisation is inadequate;
- retain P&P as sole authority for START, GO and race timing.

**Core rule:** P&P decides **what happens and when**. Presentation devices know how their local clock relates to P&P System Time and locally present that P&P-prescribed schedule. Network arrival time does not decide GO.
