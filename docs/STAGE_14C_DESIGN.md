# Stage 14C Design — Endurance

**Status: PROPOSED / REVIEW REQUIRED**

This document proposes the Endurance implementation slice after accepted Stage 14B. It is not a frozen implementation authorisation. Stages 1–14B remain frozen and unchanged.

## Product boundary

Endurance is official timed competition using the existing Lap Race mechanics. A configured duration replaces the Lap Race lap target. The active competitors are the configured active Race Entries; the implementation capacity is not a product maximum.

Stage 14C implements only Endurance. It does not implement Timed Practice, sectors, theoretical-best calculations, pit stops, fuel, mandatory pit rules, Stop/Go, new Yellow/Red/RMS behaviour, new Power Module behaviour, Timed Stage, Drag, unrelated Browser redesign, or active-race persistence/recovery.

## Duration and setup

- Duration is a whole number of minutes from 1 through 999. Seconds are not entered.
- The Browser duration control supports direct typing, up/down arrows and mouse-wheel editing consistently with the existing Lap numeric control.
- The selected duration is part of the immutable Session Definition.
- Duration is measured in Relevant Racing Time. Time while PAUSED does not consume it.
- The authoritative duration begins at GO.
- The implementation may represent the expiry internally as an authoritative Relevant-Time boundary, accumulated running intervals, or an equivalent representation, provided the observable boundary rules below are preserved.

## Architecture and authority

Endurance uses the existing Race Control, Race Engine, Session Definition, Input Module, Message Bus, Noticeboard, Browser, Results and History boundaries.

- Browser and Taster submit authorised setup and operation requests.
- Race Control validates authority, freezes the Session Definition, owns lifecycle and coordinates START, GO, PAUSE, RESUME and END RACE.
- Input Module publishes ordinary role-neutral detector events and settlement markers.
- Race Engine interprets detector events using the immutable Session Definition and owns lap timing, expiry, classification and results.
- Noticeboard exposes current authoritative state for Browser reconstruction.
- Facts remain events and are not a results database.

No Browser-to-Race-Engine shortcut, parallel timing path or mode-specific Message Bus operation is introduced. Existing generic `RESUME` is reused. Endurance manual termination uses the existing `END RACE` abandonment operation; it must not use Practice `END SESSION` semantics.

## Lifecycle

Endurance uses the existing lifecycle values. It does not add a new lifecycle enum.

1. READY: setup can select Endurance, duration and finish policy.
2. START accepted: an immutable Endurance Session Definition is created.
3. STARTING: the normal configured countdown/start lights run.
4. GO: the authoritative Endurance duration clock starts.
5. RACING: lap timing and remaining duration are active.
6. PAUSED: the existing settlement fence applies and duration stops consuming Relevant Racing Time.
7. RESUME: the generic operation resumes the same race with its remaining duration.
8. FINISHED: only normal duration completion reaches official results.
9. END RACE: manual premature termination abandons the race and creates no normal completed Endurance result.

## False starts

The initial Endurance choices are:

- OFF — a diagnostic false-start event may be recorded, but it produces no warning, penalty or outcome change.
- WARNING — the event is recorded and presented without changing classified distance.
- -1 LAP — the penalty is applied to the final classified lap count. The factual completed-lap count remains unchanged, the classified count is `max(0, completed laps - 1)`, and Results/History preserve enough information to show both counts and the applied penalty.

The Lap Race `+1 LAP` required-distance rule is not reused for Endurance. Stop/Go is reserved for a later slice shared by Lap and Endurance.

## Lap timing and retention

Entries use the existing independent Start/Finish crossing and Relevant-Time timing machinery. Crossings are ordered by Relevant Time, never by delivery order. The authoritative practical lap count and required statistics are independent of detailed lap-record retention.

Endurance must not have a 16-lap product limit. The current bounded detailed-record array is an implementation constraint only. The proposed implementation retains a bounded recent/detail set sufficient for Results/Details and persistence resource limits, while maintaining authoritative lap totals, last/best/fastest values and classification independently. The chosen retention policy and measured resource cost must be documented in the implementation evidence; it must never silently change the classified lap count.

## Expiry and finish policies

Endurance has exactly two normal finish policies.

### Stop at Zero

At the authoritative expiry boundary, the race ends immediately. Only laps whose completion Relevant Time is less than or equal to expiry count. Classification is by final classified lap count; equal totals are ordered by the earlier Relevant Time of the last counted Start/Finish crossing.

### Finish Current Lap

Eligibility is determined at the expiry boundary: the entry must already have a legitimate timing origin and be on that lap before expiry. A crossing exactly at expiry may complete that already-in-progress lap, but cannot establish a new timing origin or grant another post-expiry completion.

At expiry, an entry that had established a legitimate timing origin and was already on a lap may complete that lap at its first eligible post-expiry Start/Finish crossing. It cannot begin another counted lap. An entry without a legitimate pre-expiry timing origin/lap receives no post-expiry lap. Different entries may finish with different totals. Classification is by final classified lap count; equal totals are ordered by post-expiry finishing Relevant Time.

The main duration display reaches and remains at `00:00`. Finish Current Lap also exposes a separate authoritative overtime value beginning at expiry and counting upward until the last eligible entry completes.

## Exact Relevant-Time boundaries

- A crossing with Relevant Time before expiry is normal-time input.
- A crossing exactly at expiry completes a normal-time lap.
- A crossing exactly at expiry may complete the lap that was already legitimately in progress before expiry, but does not begin a new post-expiry lap or establish a new timing origin for one.
- A crossing delivered after expiry but carrying Relevant Time less than or equal to expiry is classified by that Relevant Time.
- A crossing with Relevant Time greater than expiry is post-expiry input.
- Each eligible entry receives at most one post-expiry completion crossing under Finish Current Lap, and eligibility requires a legitimate timing origin/lap established before expiry.
- PAUSE settlement and all existing event-boundary rules remain authoritative.
- Paused time does not advance the Endurance duration or overtime clock.

## PAUSE, RESUME and END RACE

Endurance reuses the frozen LAP PAUSE settlement/fence and Relevant-Time timing semantics, including preservation of interrupted-lap timing across the normal resume path. Completed laps and valid statistics remain. The duration clock stops while PAUSED and resumes with the remaining duration after generic RESUME. Endurance does not import Open Practice's discard-partial-lap rule. An END RACE request is a manual abandonment and does not manufacture a normal result or History entry.

## State, Browser and Taster

Authoritative State must expose enough information to reconstruct Endurance, including session mode, duration, finish policy, remaining duration, expiry state, overtime where applicable, lifecycle, per-entry identities, laps, last/best lap, integrity and validity.

The Browser provides the smallest real product path for selecting Endurance, editing duration, selecting the two finish policies, START, PAUSE, RESUME and END RACE, and observing remaining time, positions, laps and relevant lap information. Presentation is derived from authoritative State and must not calculate results.

The existing Taster already supports LAPS/TIMED values from 1–999. Stage 14C maps the existing TIMED Taster selection to Endurance while preserving the simple Taster interaction model. It does not redesign Taster or add unrelated controls.

Expiry warnings are limited to authoritative continuously available remaining time and the authoritative zero boundary. Configurable warning schedules, audio and light warnings remain deferred.

## Results, History and records

Normally completed Endurance sessions create official Results and persistent History. The stored result must include enough versioned data to reconstruct duration, finish policy, lap totals, ranking, finish/overtime information, penalties and retained valid lap details after controller restart.

Manual END RACE produces no normal completed result or History record. Interrupted active Endurance remains volatile under the accepted Stage 11–13 controller-restart rules.

Valid completed Endurance laps are eligible for PB and Track Record updates, including a valid final Finish-Current-Lap lap. Partial/incomplete laps are not eligible. Existing validity and integrity rules apply. Practice remains excluded.

## Compatibility and open implementation choices

No contradiction with the frozen Stage 1–14B architecture has been found. The following are implementation choices intentionally left open by this proposal, rather than product changes:

- exact internal duration/expiry representation;
- versioned result/persistence field layout;
- bounded detailed-lap retention strategy and measured capacity;
- exact Browser/Taster control layout within the required operations;
- whether an optional non-authoritative overtime summary is shown in every result view.

These choices must preserve every normative behaviour in this document and be covered by acceptance evidence. They must not introduce a product maximum or an Endurance-specific timing architecture.

## Cumulative regression

Stage 14C acceptance requires the complete applicable Stage 1–14B automated, structural, shared-path, Wokwi and Browser regression campaigns, plus the deliberate evaluator-failure/recovery proof and a real ESP32 Browser/Taster human checkpoint. Stage 14C must not be marked accepted until those checks and the human checkpoint pass.
