# Stage 14C Acceptance Tests — Endurance

**Status: ACCEPTED / FROZEN**

These tests define the frozen acceptance requirements for the Endurance implementation slice. They do not by themselves authorize production implementation. Stages 1–14B remain frozen.

Every automated result must be bound to the final source/build identity and retain deterministic setup, seed, Relevant Times, ordered delivery evidence, Message Bus traces, authoritative State, Facts, Request Results and production Browser/API responses.

## Automated acceptance

| ID | Required proof |
|---|---|
| 14C.1 | Production setup accepts Endurance with a whole-minute duration from 1 through 999 and rejects zero, fractional, negative, overflow and malformed values without State change. |
| 14C.2 | Duration editing through the production Browser/API supports typing, arrows and wheel-equivalent updates and serializes the selected value correctly. |
| 14C.3 | START creates an immutable Endurance Session Definition containing duration, finish policy, every configured entry and all frozen role/start mappings. |
| 14C.4 | Endurance uses the normal STARTING countdown, configured lights and authoritative GO; duration begins exactly at GO and not at setup or START acceptance. |
| 14C.5 | Independent entries use normal Input Module and Message Bus crossings; first and subsequent laps, last/best/fastest values and entry identities are correct. |
| 14C.6 | False-start OFF records only permitted diagnostics, WARNING changes no classified distance, and -1 LAP leaves factual completed laps unchanged while setting classified laps to `max(0, completed laps - 1)`; prove both a zero-completed-lap case and a normal non-zero case, with penalty, factual count and classified count reconstructed through Results/History. |
| 14C.7 | Stop at Zero ends at the authoritative expiry boundary and counts only laps completed at or before expiry. |
| 14C.8 | Finish Current Lap allows at most one eligible post-expiry crossing per entry; eligibility requires that the entry was legitimately already on that lap before expiry; an entry without that pre-expiry timing origin receives no post-expiry lap; and unequal totals are produced where appropriate. |
| 14C.9 | Exact expiry boundaries are proven: before expiry, exactly at expiry, after expiry, and delayed delivery of an event carrying an earlier Relevant Time. An event exactly at expiry may complete a lap already in progress, but cannot establish a new timing origin or grant another post-expiry completion. |
| 14C.10 | Classification is correct for both policies, including equal-lap ordering, ties and post-expiry finish ordering using authoritative Relevant Time. |
| 14C.11 | Remaining duration reaches authoritative zero; Finish Current Lap keeps the main countdown at zero and exposes a separate increasing overtime value. |
| 14C.12 | PAUSE uses the existing settlement fence; paused time does not consume duration; crossings in the paused interval cannot advance timing; generic RESUME continues with remaining time. |
| 14C.13 | Multiple PAUSE → RESUME cycles preserve the frozen LAP race timing and pause semantics: completed laps/statistics remain, interrupted-lap timing follows the frozen LAP PAUSE/RESUME rules, paused time does not consume Endurance duration, settlement completes through the normal fence, Resume continues correctly, and no Open Practice-specific incomplete-lap discard behaviour is introduced. |
| 14C.14 | END RACE is Race Director-only, abandons the active Endurance session, creates no normal result and creates no History entry. |
| 14C.15 | Normal expiry completion creates an authoritative official result with duration, policy, laps, ranking, finish/overtime data, penalties and valid retained lap details. |
| 14C.16 | Results and Details reconstruct through the production Browser/API boundary without Browser-side authoritative calculations. |
| 14C.17 | Normally completed Endurance results persist through the production persistence boundary with versioned data and reconstruct after controller restart. |
| 14C.18 | Interrupted active Endurance remains volatile under the existing controller-restart boundary and is not reconstructed as a completed result. |
| 14C.19 | Valid completed Endurance laps update PB/Track Record data, including a valid final Finish-Current-Lap lap; partial laps do not; Practice remains excluded. |
| 14C.20 | Browser disconnect/reconnect reconstructs live Endurance State, remaining time, PAUSED state, expiry and overtime without Fact replay. |
| 14C.21 | Presentation pressure, delayed polling and Browser absence do not alter authoritative duration, crossings, classification or expiry. |
| 14C.22 | Taster TIMED selection maps to Endurance for 1–999 minutes while preserving the established simple Taster interaction and existing Lap behaviour. |
| 14C.23 | Detailed-lap retention is bounded safely without imposing a 16-lap Endurance limit; authoritative lap totals and required statistics remain correct beyond the current 16-record capacity. |
| 14C.24 | Persistence/result format versioning and incompatible-version handling are safe and reconstruct all required Endurance data. |
| 14C.25 | Invalid mode, duration, finish policy, entry, lifecycle, operation and forged-authority requests are rejected with authoritative State unchanged. |
| 14C.26 | All Endurance operations and detector events use the normal Browser Interface, Race Control, Race Engine, Input Module, Noticeboard and Message Bus boundaries; no private bypass exists. |
| 14C.27 | A genuine deliberate structural or behavioural corruption causes the evaluator to fail, and restoring the condition causes it to pass. Synthetic failure flags do not count. |
| 14C.28 | The complete applicable Stage 1–14B automated, structural, Wokwi, shared-path and Browser regression campaigns pass against the final Stage 14C source/build state. |

## Human ESP32 Browser/Taster checkpoint

Using the prepared Endurance image, the operator will:

1. Confirm clean READY and the documented authority state.
2. Select Endurance through the real Browser and edit a whole-minute duration.
3. Confirm the normal countdown and GO, then verify the duration begins at GO.
4. Trigger at least two lanes independently and observe lap counts, last/best lap and remaining time.
5. Verify a real PAUSE, settlement, frozen duration, and generic RESUME with remaining time continuing afterward.
6. Run one Stop-at-Zero completion and verify official Results/History reconstruction.
7. Run one Finish-Current-Lap completion and verify zero countdown, overtime and final-lap classification.
8. Refresh/disconnect/reconnect the Browser during active, PAUSED and post-expiry states and verify authoritative reconstruction.
9. Verify a valid Endurance lap appears in the intended PB/Track Record presentation and that no partial lap does.
10. Verify Taster TIMED selection maps to Endurance without changing established Lap behaviour.
11. Verify END RACE abandons an active run without manufacturing a normal result.

The checkpoint must retain visible Browser/Taster evidence plus State, Fact, Request Result, Results, Details, History, persistence and record snapshots.

## Required evidence

The evidence bundle must include:

- final source/build hashes and artifact manifests;
- deterministic fixture seed and ordered event log;
- authoritative expiry, pause, resume and overtime Relevant Times;
- Message Bus delivery and settlement traces;
- State, Fact and Request Result snapshots;
- production Browser/API and Taster responses;
- Stop-at-Zero and Finish-Current-Lap result/history evidence;
- persistence/versioning and controller-restart evidence;
- bounded detailed-record/resource evidence beyond 16 laps;
- PB/Track Record eligibility evidence;
- complete Stage 1–14B regression manifests;
- deliberate evaluator-failure and recovery evidence;
- real ESP32 Browser/Taster human checkpoint record.

## Explicit exclusions

Absence of Timed Practice, sectors, theoretical best, pits, fuel, mandatory pit rules, Stop/Go, new Power Module behaviour, new Yellow/Red/RMS behaviour, Timed Stage, Drag, unrelated Browser redesign and active-race persistence/recovery cannot fail this Stage 14C campaign because they are outside its frozen scope.
