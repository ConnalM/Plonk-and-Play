# Stage 13 Acceptance Tests — Complete Lap Race

**Status: FROZEN before Stage 13 implementation**

This plan freezes the Stage 13 design boundary; it does not authorise Stage 13 implementation and does not alter frozen Stage 1–12 tests or supersessions.

## Automated acceptance tests

| ID | Required proof | Coverage |
|---|---|---|
| 13.1 | Default five-red schedule uses one-second red intervals, selects one final delay in 0.5–2.0 s once, and commits one authoritative GO instant. | timing/behaviour |
| 13.2 | Configured three/five-red, Lights Out/Green, Immediate/Fixed/Random and Advanced interval choices produce the defined schedule without Browser timing authority. | configuration/timing |
| 13.3 | Browser disconnect, presentation pressure and delayed rendering do not prevent scheduled GO or alter race timing. | resilience/authority |
| 13.4 | Exact false-start boundary: `t = GO-1 µs` is false and `t = GO`/later is legal, independent of processing order. | timing boundary |
| 13.5 | Unsupported base/Taster sensors do not claim false-start capability; configured capable input does, through Session Definition rather than detector code. | capability/structure |
| 13.6 | False-start OFF may record the generic event diagnostically but produces no warning, penalty or race-outcome change; WARNING records the entry/time warning without changing required distance; +1 LAP increases required genuine distance. | behaviour |
| 13.7 | Repeated +1 LAP penalties accumulate and completed-lap counts remain genuine; GO is never aborted or rescheduled. | behaviour/integrity |
| 13.8 | False-start identity and Relevant Time cross the normal Input Module/Message Bus/Race Engine boundary; later mode-specific consequences remain outside the Lap Race implementation. | structure/boundary |
| 13.9 | PAUSE settlement is required before Restart Race or End Race; delayed pre-P completion wins over either operation. | settlement/authority |
| 13.10 | Opening and cancelling destructive confirmations leave authoritative State, timing, entries and Facts unchanged. | authority/behaviour |
| 13.11 | Confirmed Restart Race abandons the current race without History, retains proposed setup, returns READY, and the next START creates a fresh Session Definition/entry IDs at zero. | lifecycle |
| 13.12 | Confirmed End Race abandons the current race without History and returns Home; no abandoned result is presented as completed Results. | lifecycle |
| 13.13 | Spectators and forged Browser authority fields cannot request either destructive operation; Request Results remain private. | authority/security |
| 13.14 | A FINISHED race rejects Restart Race/End Race and retains its sealed result when a delayed completion wins before operation acceptance. | race integrity |
| 13.15 | Every FINISHED result automatically enters the persistence process with a unique sequence identity and enough data to reconstruct Stage 12 Results/Details after controller restart when persistence succeeds; storage failure is governed by 13.18 and never changes the authoritative result. | persistence/recovery |
| 13.16 | History is newest first, retains immutable lap records, excludes abandoned races, and preserves unique identities through Race Again and rolling oldest-entry eviction. | History |
| 13.17 | Clear History requires confirmation, removes History only, and leaves PBs/Track Records unchanged. | destructive separation |
| 13.18 | Storage failure is surfaced/retried without changing authoritative timing, State, result sealing or competition outcome. | fault isolation |
| 13.19 | Default `DEFAULT TRACK` with `LANE 1`/`LANE 2` is record-eligible without SMUG configuration; configured context remains an implementation extension point. | records/context |
| 13.20 | A completed lap whose own authoritative timing and validity are sound at lap completion updates PB/Track Record even if the race later End Races; Honour timing qualifies, Grid-discarded progress does not. | PB/records |
| 13.21 | A later race fault does not retrospectively invalidate an already-established eligible PB/Track Record unless it establishes that the lap itself was invalid; demo laps cannot update records, and valid record updates survive controller restart. | eligibility/recovery |
| 13.22 | Individual PB, Track Record and all-record clears require confirmation, create a new record era, preserve History, and prevent old History from resurrecting records. | record clearing |
| 13.23 | Browser History/Results/Details reconstruct persistent authoritative data without Facts replay or Browser-side recalculation; Taster remains useful without Internet/RTC. | reconstruction/boundary |
| 13.24 | Message types append after accepted Stage 12 values 18 and 19; ownership and Browser/Message Bus boundaries remain intact. | structural regression |
| 13.25 | Complete Stage 1–12 regressions, deliberate evaluator failure, and deterministic seeded start/false-start/history/record stress scenarios pass. | regression/stress |
| 13.T | Deliberately corrupt one expected evaluator condition; the evaluator fails, then passes again after restoration. | evaluator integrity |

## Human checkpoint

After all environment preparation, the human observes the default red-light sequence and GO, performs a short two-lane race, observes a human-readable false-start warning or configured consequence where the fixture genuinely supports detection, pauses and confirms Restart Race or End Race, verifies no abandoned History entry, completes one valid race, views Results/Details and History after a Browser refresh, and verifies a record-eligible lap/record presentation. The sequence is fixed in advance and contains only simple detector/control actions; no technical diagnosis is delegated.

## Required evidence and explicit deferrals

Evidence must include authoritative schedule and GO times, false-start Relevant Times and consequences, settlement fences, confirmation/request results, persistent sequence identities, storage-failure handling, History reconstruction, PB/record era transitions, Browser-client evidence, structural checks, and Stage 1–12 regression output.

The deferrals are those listed in `STAGE_13_DESIGN.md`: future race modes, sectors, wireless/distributed input, Power Module/physical RMS enforcement, multi-lane expansion, car-specific records, automatic track equivalence, RTC/civil-time dependence, full identity/configuration UI, polished Browser/Screen Designer, championships, general takeover/conflict resolution, future-mode penalties, active-race persistence, Taster History UI, and History rescanning after record clearing.
