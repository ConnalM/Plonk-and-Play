# Stage 12 Design — Completed Results and Post-Race Flow

**Status: FROZEN DESIGN — Stage 12 implementation not yet authorised**

Stage 12 adds an authoritative in-RAM completed-race result and the minimal Results, Details, Home and Race Again flow. It does not add persistence, History, controller-restart recovery, or a production restart-from-zero operation while a race is active.

## Ownership and lifetime

Race Engine owns a `CompletedRaceResult` created only after completion settlement. It contains the immutable Session Definition reference, W/F, finish behaviour, per-entry completed progress, valid lap records, completion times where measured, finishing order/dead heats, Best Lap and overall Fastest Lap, and a sealed flag. It remains available in RAM through Results, Details, Home and Browser reconnect until the next accepted START, fixture reset, or controller restart. Browser and Noticeboard expose it; neither calculates authoritative results.

START after Race Again creates a new Session Definition and fresh Race Entry IDs. Editing the proposed setup cannot mutate the sealed previous result.

## W, F and completion settlement

Immediate Finish has W=F at the first target-distance crossing. Complete Current Lap and Complete Full Race Distance retain required post-W crossings; F is the Relevant Time of the last required crossing. A candidate F starts an explicit Message Bus settlement: `FINISH_SETTLEMENT(F)` reaches the local Input Module, which settles all locally accepted eligible input through F; `FINISH_SETTLED(F)` returns through the normal bus. Race Engine then orders and interprets the settled inventory, seals the result, and reports completion to Race Control. Input with Relevant Time after F cannot mutate the sealed result.

The exact finish-rule algorithms and settlement-pending rejection reason are frozen by the acceptance plan. No generalized distributed watermark or persistence protocol is introduced.

## Browser flow

FINISHED State offers Results and read-only Details. Details shows genuine retained lap records only. BACK returns to Results. Spectators may view both. Race Director authority is required for Race Again and any Home operation that changes authoritative session flow. Race Again restores the completed setup as proposed without starting. HOME returns to normal mode selection and does not factory-reset remembered setup.

## Explicit deferrals

Persistent Results/History, rolling history, PBs, track records, persistence schema, controller-restart result recovery, PAUSE-to-Restart-Race, PAUSE-to-End-Race, other modes, championship management and polished UI remain deferred.


## Decisions 14–21 incorporated

**W/F:** Immediate Finish uses W=F. Complete Current Lap sets W at the winner crossing and requires exactly one valid post-W crossing from every other active competitor; F is the last such crossing. Complete Full Race Distance keeps required competitors active to target distance; F is the last target crossing.

Complete Current Lap ranking is completed progress first, then finishing Relevant Time, with equality only when both are equal. Immediate Finish non-winners use completed progress only; no physical position or finish time is inferred. Results use explicit normal competition ranks (`1, =2, =2, 4`) and joint first where applicable.

**HOME and Race Again:** HOME is Browser-local navigation and never changes FINISHED or requires Race Director authority. Race Again is Race Director-only and seeds a new editable Proposed Race Setup from the completed Session Definition, then validates current capabilities when START creates a fresh Session Definition and fresh entry IDs.

**CompletedRaceResult:** Race Engine owns one sealed in-RAM object. Results and Details are views of that object, exposed through current Noticeboard State/API. Facts remain events and are never used as a history database. The object ends at the next accepted START, full fixture reset or controller restart.

**Settlement:** Proposed appended types are `FINISH_SETTLEMENT=18` and `FINISH_SETTLED=19`. Race Engine owns candidate F and finish-rule interpretation; Input Module only fences its locally accepted FIFO. Race Engine seals only after FINISH_SETTLED, and later Relevant Time values cannot mutate the result. Settlement flags reset for every new race.


## Decision 22 — active competitor at W

An active competitor at W is a race entry in the current Session Definition which has not ceased participation under an existing authoritative race rule before W. Stage 12 introduces no retirement, withdrawal, DNF, disqualification, pit/inactive state or Browser-dependent participation. In this implementation every valid entry remains active except an entry already finished as part of the winning group. Browser presence never affects activity. Complete Current Lap therefore requires exactly one further valid crossing from every other active entry; Complete Full Race Distance continues every other active entry to target distance. If all active entries satisfy the winning condition at the same Relevant Time they may be joint winners and no artificial crossing is added.

Completed-result Noticeboard and Browser field names/serialization are implementation details, constrained by the semantic requirements and existing boundaries. They are not remaining product-design decisions and must not turn State into persistent History.
