# Timed Stage — user manual chapter (design draft)

**Status:** Post-V1 proposed feature; not released or hardware-accepted. Prepared for issue #2 review, 20 November 2026. This chapter describes the agreed intended user experience, not a claim that the current P&P firmware supports it.

Design references: [governing specification, Race Modes §5](https://github.com/ConnalM/Plonk-and-Play/blob/archie/wp8-approved-display-defaults/docs/PP_DESIGN_SPEC.md#race-modes-product-spec--5-timed-stage), [owner-approved screen decisions](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7043883926), [sporting rules](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7044111322), [HTML review](https://github.com/ConnalM/Plonk-and-Play/blob/ec0078a9aa259430eddbc30a1dbeeebed6ac0ce3/design/smugland/PP_Timed_Stage_review.html). V21 remains the frozen common Browser and auxiliary display reference.

## 1. What is Timed Stage?

Timed Stage is a race against the clock rather than a simultaneous lap race. It can represent a rally stage, hill climb or sprint without requiring a separate mode for each discipline. The fastest adjusted time wins a single stage; the lowest cumulative adjusted time wins a multi-stage event.

The time between GO and crossing the starting sensor is not part of the stage time. GO gives permission to start; the actual start crossing begins official timing.

## 2. Set up a stage

In the Race screen choose Timed Stage, then choose Loop or Point-to-Point. Select Single Stage for a standalone stage or Multi-Stage Event for a series of stages. Name the event and select the MUGs taking part.

Enter the settings for each stage card:

| Setting | Meaning |
| --- | --- |
| Stage name | Identifies the stage in the running order and results. |
| Geometry | Loop, A → B or B → A. |
| Laps | Number of timed laps for a Loop stage; not needed for Point-to-Point. |
| Attempts | Number of permitted timed attempts per MUG. |
| Scoring | All runs count, or best run counts. |

For multi-stage events, add and order the stage cards. A stage is an event definition, not a new physical track record: one physical track may host several differently named stages.

### Check the sensor mapping

| Geometry | Start | Finish |
| --- | --- | --- |
| Loop | First crossing of configured Start/Finish sensor after GO | Same sensor after the required number of timed laps |
| A → B | Configured sensor A | Configured sensor B |
| B → A | Configured sensor B | Configured sensor A |

The diagrams illustrate the intended car position and roles. On a loop, place the car behind Start/Finish before starting. For a point-to-point run, position it behind the mapped start sensor. Check the actual physical mapping for the selected stage; labels A and B do not inherently identify hardware channels. Changing direction reverses roles without requiring the sensors to move.

## 3. Advanced options

The normal event organisation is **Stage order**: all MUGs complete a stage before moving to the next. Alternatively, choose MUG order to allow one MUG to complete all stages before the next MUG.

The normal attempt order within a stage is **all racers attempt 1, then all racers attempt 2, and so on**. Alternatively, each racer completes their attempts before the next racer. Event organisation and attempt ordering are distinct settings; see the review questions below for their interaction.

For an unsuccessful run (DNF), choose the agreed scoring treatment: fixed DNF time, slowest completed time plus a configurable penalty, exclusion from overall classification, or retry at the Race Director's discretion. Set the applicable time or penalty.

False-start policy defaults to **Warning only**. Alternatively configure a time penalty or classify the invalid attempt as DNF. A false start means crossing the starting sensor before GO, not simply moving the car.

Split hold defaults to **three seconds**. The live timing display briefly shows the completed split at full precision, then returns to live time. Timing continues throughout. The finish display takes precedence over any outstanding split hold.

## 4. Run the stage

The Race Director (SMUG) controls the event. Other views show the race state without granting race-control authority.

| Screen | What to do or expect |
| --- | --- |
| READY | Confirm the stage, MUG, attempt and mapping. Position the car; the Race Director starts the countdown. |
| Countdown | Wait for the authoritative start signal. |
| GO / ARMED | Launch the car. Stage timer remains zero until the first valid starting crossing. |
| Running | Follow the live elapsed time (minutes and seconds). For a loop, the lap count and split table show completed laps. |
| Split | The just-completed split appears at precise timing resolution (mm:ss.th); the underlying official timer continues. |
| FINISHED | The required final crossing stops the stage clock. The precise final result takes priority. |
| False start | The display identifies the premature crossing and configured sanction. Follow the Race Director's instructions. |
| DNF | The run is unsuccessful; the configured scoring policy applies. |

A loop start crossing begins timing but does not itself complete a lap. Subsequent crossings record lap splits. A point-to-point stage ends at the mapped finish sensor and has no automatically invented lap splits.

The split table keeps the completed split times after the prominent display returns to the live clock. The BEST column separates the available best split from ordinary split times.

## 5. Review a run

After a run the Race Director reviews the supplied raw stage time, penalties and adjusted time. Reaction time is excluded from the official stage time. Penalties are shown separately rather than silently added to the raw time.

If the attempt was unsuccessful, the Race Director may authorise a retry. A retry replaces that unsuccessful attempt in the same scheduled slot. It is not an additional scoring opportunity. A valid completed attempt cannot be retried simply to improve the time.

A DNF does not prevent the MUG competing on later stages. Accept the run and proceed according to the event's running order. The prototype uses example snapshots for this sequence; actual timing, sanctions and classification are the controller's authoritative facts.

## 6. Stage and event results

The main classification shows Position, MUG, Stage time, Penalty, Timed Stage total and Gap. The stage results identify the fastest classified stage result and show the updated cumulative standings where available.

After a stage, use NEXT STAGE to proceed through the configured event. At the end use EVENT RESULTS. For a single-stage event the final classification is the stage classification. A missing or unavailable fact is displayed as a dash rather than as zero or an inferred gap.

Select Details to view the stage-by-stage matrix. Expand the relevant run to inspect individual lap splits. The detailed breakdown keeps stage results, penalties and cumulative totals distinguishable. Actual official record visibility follows the governing P&P session-source and storage rules; the review demonstration writes no records and has no Save Results operation.

## 7. Integrated auxiliary display

The 800 × 480 display follows the controller-authoritative phase, showing the stage, MUG, attempt and dominant timing or start indication. READY and GO show zero elapsed time; a live run shows elapsed time; a completed split or final result uses precise time. The display distinguishes false start and DNF from a valid finish. On a loop it shows lap progress where supplied; on point-to-point it shows the relevant sensor direction.

The auxiliary preview is read-only in this review candidate. It does not run a second clock engine or accept race authority. Browser navigation does not change the controller's run state.

## 8. Illustrative example

Three MUGs enter a two-stage event. Forest Loop has three laps and two attempts per MUG, scored by best run. Hill Road has one A → B attempt per MUG. With default ordering, all three complete Forest Loop attempt 1, then all three complete its attempt 2. The event moves on to Hill Road and finally shows the cumulative classification.

If a racer fails Forest Loop attempt 2 and the Race Director authorises a retry, the replacement occupies attempt 2; later stage eligibility remains unchanged. If a time penalty of five seconds is applied to an otherwise valid raw run of 01:12.34, the adjusted result is 01:17.34.

## 9. Review boundaries and open questions (editorial appendix; not final user-manual text)

- Confirm how the two ordering options interact when MUG order and round-based attempts are both selected: a racer completing every stage necessarily prevents event-wide attempt rounds. The prototype currently gives MUG order precedence; this is not an additional approved sporting rule.
- Confirm Best Run treatment when one attempt is DNF, when every attempt is DNF, and when slowest-completed-plus-penalty has no completed comparator.
- Confirm tie ranking and cumulative gap semantics, and whether interim classification in MUG order is explicitly provisional.
- Confirm treatment of further early sensor crossings and restart/re-arm sequences after a warning or penalty.
- Confirm the scoring treatment when a DNF retry is declined or fails again under the retry policy.
- The HTML is a visual review fixture, not a timing or scoring engine. It contains fixed sample split and classification data, illustrative sensor mapping, simulated signals, and a UI-only split hold. Custom setups do not produce authoritative custom results. The preview currently does not demonstrate every agreed auxiliary results state; exact approved bezel fidelity and responsive rendering still need visual validation. The manual records intended behaviour, not evidence that these paths already pass.
- No production changes, firmware, V1 scope expansion, Drag or hardware acceptance. Browser regression evidence is a separate deliverable.
