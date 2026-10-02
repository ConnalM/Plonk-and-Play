# Stage 11 Acceptance Tests — Pause and restart behaviour

**Status: FROZEN before Stage 11 implementation**

This document defines the accepted Stage 11 boundary. It does not authorise Stage 11 implementation, alter a frozen earlier acceptance plan, or create a new supersession.

## 1. Purpose and governing boundary

Stage 11 proves a minimal, authoritative pause and restart flow for a running two-lane Lap Race:

- a Race Director can PAUSE a running race;
- after locally accepted pre-pause input has been settled, the Race Director can choose either **Honour Restart** or **Grid Restart**;
- all entries pause and restart together;
- input timing is interpreted against authoritative pause and restart boundaries, never against message arrival or Browser activity; and
- State remains sufficient for a reconnecting Browser to reconstruct the current race.

Stage 11 is governed by the architecture, Message Contract, Race Control/Engine design, lifecycle rules, product behaviour, and frozen Stage 1–10 acceptance plans. Existing message-type numeric values remain immutable. Any new message type is appended after the accepted Stage 10 range.

The only retained regression supersessions are unchanged:

1. Stage 1 test 1.7: the historical DiagnosticProbe numeric-value assertion only.
2. Stage 6 test 6.13: the explicit historical Browser-START prohibition only.
3. Stage 6 test 6.14: the authorised narrow Browser-START scope exception only.

Stage 11 introduces no new supersession.

## 2. Fixed Stage 11 semantics

### 2.1 Authority and lifecycle

Only the Stage 10 Race Director Client Context may request PAUSE, Honour Restart, or Grid Restart. A Spectator receives a client-private rejection and cannot alter authoritative State.

PAUSE is valid only in `RACING`. It is rejected in `READY`, `STARTING`, `PAUSED`, `RESTARTING`, `FINISHED`, and integrity-faulted states. In particular, PAUSE during the initial start countdown is rejected.

Race Control accepts PAUSE at one authoritative P&P System Time, **P**, and transitions the race to `PAUSED`. There is no pause countdown. Both entries pause together.

Honour Restart and Grid Restart are valid only when the race is `PAUSED` and the local pre-pause settlement fence has completed. A request made sooner is rejected with the specific reason `PAUSE_SETTLEMENT_PENDING`; it is not silently queued or delayed.

When a restart is accepted, Race Control fixes one authoritative restart instant:

```
R = restart-request acceptance time + 3,000,000 microseconds
```

The lifecycle becomes `RESTARTING`. The State carries `scheduledRestartAt = R`; presentation may derive a countdown and must not cause changing authoritative State merely to display it.

### 2.2 Relevant-Time boundaries

The Race Engine applies the following rules to an authoritative input time `t`:

| Relevant Time | Competition meaning |
|---|---|
| `t < P` | Eligible pre-pause input, even if delivered or processed after PAUSE. |
| `P <= t < R` | Does not advance competition progress. In particular, `t == P` does not count. |
| `t >= R` | Eligible after restart. In particular, `t == R` counts. |

This preserves the existing rule that Relevant Time, rather than arrival or processing order, controls race meaning.

PAUSED State alone does **not** assert that displayed authoritative lap State is final for the pre-pause interval. A delayed clean input with `t < P` can legitimately alter competition State after PAUSED first becomes visible. `PAUSE_SETTLED(P)` is the point at which every clean `INPUT_EVENT` accepted by the **local Input Module** before P has been authoritatively interpreted.

If a delayed eligible `t < P` event completes the competition, `FINISHED` wins over `PAUSED` or `RESTARTING`; a pending restart is cancelled.

### 2.3 Local pause-settlement fence

Stage 11 uses the existing protected Input Module FIFO and the normal P&P Message Bus. It introduces no private Input-to-Race-Engine path.

1. Race Control publishes `SESSION_OPERATION(PAUSE, P)` to Race Engine and Input Module through the Message Bus.
2. Input Module treats that operation only as a local delivery fence. After all clean inputs it accepted locally before P, it publishes one `INPUT_SETTLEMENT(P)` through its existing Bus endpoint.
3. FIFO order from that Input Module endpoint ensures the settlement marker follows those accepted inputs at Race Engine's mailbox.
4. Race Engine interprets the preceding inputs and publishes `PAUSE_SETTLED(P)` to Race Control through the Message Bus.
5. Race Control accepts either restart only after receiving that status.

This proves local pre-P settlement. It deliberately does **not** define a watermark, clock-synchronisation, or remote-device delivery protocol. A future remote/distributed Input Device that first delivers an earlier-timestamped event to the local Input Module after P is outside Stage 11.

### 2.4 Restart methods

**Honour Restart** preserves each entry's interrupted-lap timing origin. For a later valid crossing at time `t >= R`, the paused interval `R - P` is excluded from that interrupted lap's elapsed time. Completed laps and existing per-entry progress remain intact.

**Grid Restart** retains each entry's completed-lap count and last completed-lap information. It discards every entry's incomplete-lap progress and rebases every entry's next-lap timing origin to R. For its first valid crossing at `t >= R`, an entry's next lap time is `t - R`. Different partial progress at P has no timing consequence after Grid Restart. Inputs in `P <= t < R`, including physical return-to-grid activity, do not advance progress.

Restart Race from zero is a separate operation and is not implemented by either restart method.

### 2.5 State, Facts, and client results

The authoritative Noticeboard State includes:

- lifecycle `PAUSED` or `RESTARTING` where applicable;
- `pauseEffectiveAt`;
- `scheduledRestartAt` and `restartMethod: HONOUR | GRID` while applicable;
- existing per-entry authoritative lap and timing State;
- existing Stage 9 integrity/result-validity State.

The system publishes facts named `PAUSED`, `RESTART_SCHEDULED`, and `RESUMED`. Facts support live/history semantics; Browser recovery is always from current State and never from Fact replay. Request Results remain private to their originating Stage 10 Browser identity.

An existing Stage 9 `RACE_INTEGRITY_FAULT` overrides PAUSED or RESTARTING: the compromised race becomes faulted/invalid, and no restart may make it valid.

## 3. Proposed message additions

The following proposed message types are appended after the accepted `RACE_INTEGRITY_FAULT` value; no existing numeric type changes:

| Proposed message | Publisher | Consumer(s) | Meaning |
|---|---|---|---|
| `SESSION_OPERATION_REQUEST` | Browser Interface | Race Control | Trusted Client Context plus PAUSE, HONOUR_RESTART, or GRID_RESTART request and correlation. |
| `SESSION_OPERATION` | Race Control | Race Engine, Input Module | Authoritative PAUSE boundary P or accepted restart boundary R/method. |
| `INPUT_SETTLEMENT` | Input Module | Race Engine | Local fence marker for inputs accepted before P. |
| `PAUSE_SETTLED` | Race Engine | Race Control | Every locally accepted pre-P input has been interpreted. |
| `PAUSED` | Race Control | Presentation, diagnostics | Fact that PAUSE became effective. |
| `RESTART_SCHEDULED` | Race Control | Presentation, diagnostics | Fact carrying R and restart method. |
| `RESUMED` | Race Control | Presentation, diagnostics | Fact that R became effective. |

The Input Module neither interprets lanes, laps, MUGs, lifecycle, nor restart method. Race Control receives trusted logical Client Context only; Browser cookies, connection details, and identities remain inside the Browser Interface boundary.

## 4. Deterministic automated acceptance fixture

The fixture uses the real production Message Bus, Race Control, Race Engine, Noticeboard, protected local Input Module path, and Browser Interface. It has two genuine Race Entries, independently controlled simulated inputs, controllable Relevant Times, and two independent HTTP cookie stores (Race Director and Spectator).

It may apply deterministic local delivery pressure only through the production fixture/input mechanisms. It must not fake State, Facts, Request Results, Client Context, or completion; bypass a production boundary; or insert a private input-to-engine path.

## 5. Automated acceptance tests

| ID | Test and required evidence |
|---|---|
| 11.1 | **Director PAUSE.** A Race Director pauses a two-entry RACING race. Capture the authoritative P in Race Control's operation, Input Module fence, Race Engine boundary handling, and Noticeboard State; all copies must be identical, with both entries paused together and the Session Definition unchanged. |
| 11.2 | **PAUSE rejection.** PAUSE is rejected outside RACING, including STARTING; duplicate PAUSE and restart requests outside valid PAUSED/settled state are rejected without State change and with the defined private reasons. |
| 11.3 | **Exact P boundary.** Use literal events at `P - 1 µs` and `P`. The former counts and the latter does not, while delivery/processing order is deliberately reversed and recorded. |
| 11.4 | **Delayed pre-P delivery.** Accept a real clean event locally at `t < P`, hold it in the production delivery path, make PAUSED externally visible first, then release it. It must update authoritative pre-pause State with its original identity and Relevant Time. |
| 11.5 | **Settlement fence.** Record the complete inventory of every clean event accepted by the local Input Module before P. A restart request before `PAUSE_SETTLED(P)` receives `PAUSE_SETTLEMENT_PENDING`. The real Input Module sends one marker through its normal Bus endpoint; the Race Engine may emit `PAUSE_SETTLED(P)` only after the inventory has been delivered and interpreted exactly once, with matching IDs, event identities and Relevant Times. Restart becomes eligible only after that evidence. |
| 11.6 | **Delayed completion wins.** After PAUSED is visible, submit restart before settlement and prove rejection. Then release a delayed eligible pre-P event that completes the race. State becomes FINISHED, any pending restart scheduling is cancelled, and every later Honour/Grid request is rejected. |
| 11.7 | **Accepted Honour Restart.** After settlement, accept Honour Restart and prove the same accepted operation, private result, scheduled State, Race Engine operation and R all agree; R is exactly acceptance time plus 3,000,000 microseconds and is distinct from initial GO. |
| 11.8 | **Honour timing.** With lap origin L, pause P, restart R, and completion t, require the exact interrupted-lap result `(t - L) - (R - P)`; completed laps remain unchanged. |
| 11.9 | **Grid timing.** Use two entries with different completed-lap counts and different partial-lap progress at P. Grid Restart must retain each completed count/last lap, discard both incomplete portions, rebase both origins exactly to R, and produce first post-R lap time exactly `t - R` for each entry. |
| 11.10 | **Exact R boundary and no leakage.** Use literal events at `R - 1 µs` and `R`; the former does not count and the latter counts. Deliver the interval events only after R as well, proving they cannot leak through later or alter either entry. |
| 11.11 | **Two-entry common operation.** Both entries carry the identical authoritative P and R, pause/restart together, retain independent data, and have no lane-specific pause/restart route or cross-lane contamination. |
| 11.12 | **State and Facts.** PAUSED, RESTART_SCHEDULED and RESUMED Facts have exact order, boundary and method payloads. State exposes P, R, method and per-entry data without a ticking countdown field, and remains correct after Facts are withheld. |
| 11.13 | **Browser authority and private results.** Two real HTTP clients use separate cookie stores. Forge role/identity/authority fields in JSON, headers, query parameters and UI data for PAUSE, Honour Restart and Grid Restart; Spectator requests are rejected without State change. Colliding visible correlation IDs still produce private results. |
| 11.14 | **Disconnect/reconstruction.** Join, reload, disconnect and reconnect during both PAUSED and RESTARTING. Compare each client with the authoritative current snapshot, including P, R, method, integrity State and both entries, while withholding all Facts and replay endpoints. Master absence does not affect operation. |
| 11.15 | **Presentation isolation.** Apply slow, disconnected and repeated-polling Browser pressure concurrently with P/R boundary events and settlement. P, R, input eligibility, protected delivery, and entry timing remain unchanged. |
| 11.16 | **Integrity fault precedence.** Create real Stage 9 protected-input overruns separately in PAUSED and RESTARTING. In both cases the fault latches, makes `resultValid: false`, preserves useful State where possible, blocks restart/resumption, and cannot self-clear. |
| 11.17 | **Controller-restart volatility.** Restart the controller during PAUSED and during RESTARTING. The interrupted race, volatile Session Definition and timing are not reconstructed; a new valid session requires the normal setup/start path. |
| 11.18 | **Structural review.** Verify appended, never-renumbered message values; normal Bus routing for operations, settlement and acknowledgements; Race Control/Engine/Input/Browser authority boundaries; no Browser Bus participant; no token/connection data in Race Control; no private input bypass; and no remote-settlement mechanism. |
| 11.19 | **Applicable Stage 1–10 regression campaign.** Preserve all applicable accepted behaviour under the three retained narrow supersessions only. |
| 11.20 | **Relevant-Time versus processing-order.** Across the production path, reverse delivery/processing order for `P - 1`, `P`, `R - 1` and `R` events and compare against their authoritative Relevant Times. Only `P - 1` and `R` may advance competition progress. |
| 11.T | **Deliberate evaluator failure.** In an isolated run corrupt one exact settlement-inventory, P/R boundary, or asymmetric Grid-R timing expectation. The evaluator must fail with expected/actual evidence; restore the expectation and confirm PASS. |

## 6. Evidence requirements

Retain source/build identity, frozen-plan revision, complete pre-P inventory and interpretation ledger, exact P/R values, delivery order, State/Facts/Request Results, browser-client evidence, structural results, regression outcomes, and deliberate evaluator-failure output. Evidence must identify the production path used and must never substitute a fake State, Fact, Request Result, Client Context, or completion.

## 7. Mandatory human Browser checkpoint

After all automated tests pass, preparation follows `HUMAN_CHECKPOINT_PROCEDURE.md`: the coding agent builds the named Stage 11 demo firmware, establishes a fresh no-stale-state environment, verifies the gateway/Browser route, prepares normal Chrome and Incognito as separate identities, and dry-reviews every expected State before involving Connal.

The short human sequence is:

1. In normal Chrome as the prepared Race Director, start the prepared two-lane fixture and create visible independent partial progress for both entries.
2. Pause from Race Director; confirm PAUSED and P are visible. Incognito remains Spectator, observes the same State, and its control is unavailable/denied.
3. From Race Director choose Honour Restart after settlement; observe RESTARTING with one scheduled R, then use the prepared detector controls after R and confirm preserved interrupted-lap timing.
4. In a fresh prepared run, create partial progress again, Pause, choose Grid Restart, and confirm retained completed laps plus first post-restart lap timing from R.
5. Refresh/reopen the Spectator at the predetermined point and confirm State reconstruction without needing a Fact replay.

The detector inputs are generated by the prepared simulated detector controls, independently of either Browser. The human checkpoint neither diagnoses infrastructure nor attempts controller-restart recovery.

## 8. Explicit deferrals

Stage 11 deliberately defers:

- Restart Race from zero;
- End Race or operational abandonment;
- physical yellow lights, track-power control, and physical grid guidance;
- polished race-control UI;
- persistence and controller/ESP32 restart recovery of race/session/timing State;
- distributed or remote-input settlement, clock synchronisation, and watermark protocol;
- results/history/persistence work beyond the existing authoritative current State and Facts.

## 9. Freeze rule

**Status: FROZEN before Stage 11 implementation.** Implementation begins only after this plan is committed.
