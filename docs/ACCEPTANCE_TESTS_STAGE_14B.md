# Stage 14B Acceptance Tests — Open Practice

**Status: PROPOSED / REVIEWED**

**Implementation status: NOT STARTED**

Stage 14B implements Open Practice only. These tests do not authorise Timed Practice, Endurance, Timed Stage/Rally, Drag or any other Stage 14 behaviour.

Every automated result must be bound to the final source/build identity and retain deterministic fixture configuration, Relevant Times, ordered delivery evidence, Message Bus traces, State, Facts, Request Results and Browser/API responses.

## Automated acceptance

| ID | Required proof | Coverage |
|---|---|---|
| 14B.1 | Production Browser selects Open Practice and authoritative setup/state records `OPEN_PRACTICE`. | Browser/mode |
| 14B.2 | START creates an immutable Practice Session Definition containing every configured active entry and role mapping. | architecture |
| 14B.3 | Practice becomes active immediately with no countdown, red lights, scheduled GO or GO requirement. | lifecycle |
| 14B.4 | A first eligible crossing establishes a lane timing origin but completes no lap. | timing |
| 14B.5 | A subsequent crossing completes exactly one lap using Relevant-Time difference and establishes the next origin. | timing |
| 14B.6 | At least four configured entries maintain independent timing origins, lap counts, last laps and best laps. | multi-entry |
| 14B.7 | One-, two-, four- and eight-entry Practice State and Browser serialization work independently; malformed/out-of-range counts are safely rejected. | capacity |
| 14B.8 | A later Relevant-Time event deliberately delivered before an earlier event still produces timing determined by Relevant Time, not processing order. | ordering |
| 14B.9 | Only Race Director/SMUG may PAUSE; unauthorised, READY and invalid-lifecycle requests are rejected with State unchanged. | authority |
| 14B.10 | A locally accepted pre-pause event delayed until after PAUSED is visible is handled through the normal settlement/fence path. | settlement |
| 14B.11 | Pause retains completed laps/statistics and discards incomplete timing. | pause |
| 14B.12 | Crossings with Relevant Times in the paused interval cannot advance Practice, including when delivered later. | pause boundary |
| 14B.13 | Generic RESUME is accepted only from PAUSED, is interpreted according to `OPEN_PRACTICE`, and has no countdown. | operation |
| 14B.14 | After Resume, the first eligible crossing establishes a new timing origin and the following crossing completes the next lap. | resume |
| 14B.15 | Multiple Pause → Resume cycles retain completed statistics, discard each incomplete lap and never leave settlement pending. | repeated cycles |
| 14B.16 | Only Race Director/SMUG may issue generic END SESSION; forged role fields and Spectator/MUG requests are rejected unchanged. | authority |
| 14B.17 | END SESSION returns toward READY/Home without normal Lap Race FINISHED or competition-completion semantics. | lifecycle |
| 14B.18 | Unequal lane activity produces no winner, rank, finishing position, laps-behind classification or finish policy. | non-official |
| 14B.19 | Production Results reports no official completed race after Practice ends. | isolation |
| 14B.20 | Official History sequence and retained entries are unchanged by Practice. | isolation |
| 14B.21 | Practice laps do not update PBs, Track Records or record eras. | records |
| 14B.22 | Browser disconnect/reconnect during active Practice, PAUSED and resumed Practice reconstructs authoritative State without Fact replay. | reconstruction |
| 14B.23 | Real Browser polling/refresh pressure cannot alter Practice timing or State. | presentation isolation |
| 14B.24 | Controller restart does not reconstruct interrupted Practice as an official race/result. | volatility |
| 14B.25 | START, PAUSE, RESUME, END SESSION and detector events use the normal Browser Interface, Race Control, Race Engine, Input Module and Message Bus boundaries. | structure |
| 14B.26 | Invalid mode, lifecycle, entry data, operation and forged authority requests are rejected without authoritative State change. | safety |
| 14B.27 | The complete Stage 14A acceptance campaign passes against the final Stage 14B source/build state. | regression |
| 14B.28 | The complete applicable Stage 1–13 automated, structural, Wokwi, shared-path and Browser regression campaign passes. | regression |
| 14B.29 | A real structural or behavioural corruption causes the evaluator to fail; restoring the condition causes it to pass. Synthetic failure flags do not count. | evaluator integrity |

## 14B.T Human ESP32 Browser checkpoint

Using the prepared Open Practice image:

1. Confirm clean READY / No Race Director.
2. Acquire Race Director through the real Browser.
3. Select Open Practice.
4. Start it and confirm no countdown or GO requirement.
5. Trigger one lane twice and observe one completed Practice lap.
6. Trigger a second lane independently and observe independent timing.
7. Pause during an incomplete lap and verify partial progress is discarded.
8. Resume and verify the next crossing establishes timing and the following crossing completes a lap.
9. Refresh/disconnect/reconnect the Browser during active and paused states and verify reconstruction.
10. End Session.
11. Verify return toward READY/Home and confirm no official Results, History, PB or Track Record change.

The checkpoint must retain visible Browser evidence plus State, Fact and Request Result snapshots.

## Required evidence

The evidence bundle must include:

- final source/build hashes;
- deterministic fixture seed and ordered event log;
- Message Bus delivery traces;
- authoritative State and Fact snapshots;
- production Browser/API responses;
- real ESP32 Browser evidence including multi-lane independent timing and reconnect;
- one-, two-, four- and eight-entry evidence;
- resource-size evidence;
- complete Stage 1–14A regression manifests;
- deliberate evaluator-failure and recovery evidence;
- human checkpoint record.

Sector timing and theoretical-best calculations are not required evidence for this slice. Their absence cannot fail Open Practice acceptance.

No Stage 14B acceptance is complete until all applicable tests and cumulative regressions pass.

