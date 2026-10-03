# Stage 12 Acceptance Tests — Results and post-race flow

**Status: FROZEN before Stage 12 implementation**

This document freezes the Stage 12 design boundary; it does not authorise production implementation. Existing Stage 1–11 requirements and supersessions remain unchanged.

## Automated tests

| ID | Required proof |
|---|---|
| 12.1 | Immediate Finish: W=F, winner and completed progress are authoritative; no inferred track position. |
| 12.2 | Complete Current Lap: W, exactly one post-W crossing per other active entry, F and equal-time dead heat. |
| 12.2a | Complete Current Lap ranking is progress-first, then finishing Relevant Time; a lap-behind entry cannot overtake a more advanced entry. |
| 12.2b | Active-at-W semantics: every valid non-winning-group entry requires the defined crossing; Browser presence cannot affect activity; joint winners need no artificial crossing. |
| 12.3 | Complete Full Race Distance: W, final required crossing, F and authoritative order. |
| 12.3a | Joint ranks/dead heats are represented per entry for winner and non-winner equality cases. |
| 12.4 | FINISH_SETTLEMENT/FINISH_SETTLED proves delayed eligible input with Relevant Time <= F is interpreted before FINISHED. |
| 12.4a | FINISH_SETTLEMENT and FINISH_SETTLED use normal Message Bus routing; Input Module does not interpret race rules. |
| 12.5 | Input after F and repeated post-F detector storms cannot mutate the sealed result. |
| 12.5a | Exact W/F boundaries reject or ignore input according to Relevant Time, independent of processing order. |
| 12.6 | Result immutability after sealing, including later Browser requests and setup edits. |
| 12.7 | Equal Immediate-Finish progress is represented as equal at the known level. |
| 12.8 | Incomplete competitors show measured progress/laps behind without fabricated finish times. |
| 12.9 | Complete lap history is retained and reconstructed. |
| 12.10 | Best Lap uses only valid completed laps; incomplete/Grid-discarded laps do not qualify. |
| 12.11 | Fastest Lap, exact ties and no-valid-lap display (`—`). |
| 12.12 | Honour-Restart lap appears with paused time excluded. |
| 12.13 | FINISHED State exposes W/F, result and Details data through Noticeboard. |
| 12.14 | Browser refresh, reconnect and new Browser reconstruct the same FINISHED Results/Details without Fact replay. |
| 12.15 | Spectator can view Results/Details but cannot perform Race Again or other operational flow changes. |
| 12.16 | Race Director Race Again restores the prior setup as proposed without starting. |
| 12.17 | Proposed setup edits affect only the next race and cannot alter the sealed result. |
| 12.18 | START after Race Again creates a fresh Session Definition, fresh entry IDs and zeroed race state. |
| 12.19 | HOME is Browser-local, works for Spectator, leaves FINISHED/result/setup unchanged and requires no Race Director authority. |
| 12.20 | Controller restart does not falsely reconstruct the old completed result. |
| 12.21 | Completion settlement and result messages use normal Message Bus authority/boundaries; new types append after Stage 11 as 18 and 19. |
| 12.21a | Repeated races reset completion-settlement state and cannot inherit stale fences. |
| 12.22 | Applicable Stage 1–11 regressions and deterministic stress scenarios pass, including W/F ordering, finish settlement and reconnect. |
| 12.T | Deliberate evaluator failure is detected and restored. |

## Human checkpoint draft

After environment preparation, Connal completes a short race and observes Results, Details and genuine lap information; returns with BACK; triggers detectors after FINISHED and confirms the result does not change; refreshes/reopens Browser and confirms reconstruction; observes Spectator read-only Results; uses Race Again, changes one setup value, presses START and confirms a fresh race begins. No technical diagnosis or setup work is delegated to the operator.

## Evidence and deferrals

Retain production-path State, Facts, Request Results, W/F values, ordered settlement inventory, result records, Browser-client evidence and structural output. Do not substitute Browser-derived calculations. Persistence, History, controller-restart recovery, active-race Restart Race, End Race, other modes, championship management and polished UI are outside Stage 12.
