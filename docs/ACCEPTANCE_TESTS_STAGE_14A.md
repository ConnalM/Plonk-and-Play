# Stage 14A Acceptance Tests — Variable Race Entry Capacity

**Status: ACCEPTED / FROZEN**

This plan covers only the variable-entry refactor. It does not authorize Practice, Endurance, open-ended races, timed races or any other Stage 14 mode. It does not alter frozen Stage 1–13 tests or behaviour.

`PP_MAX_ENTRIES = 8` is an implementation/build capacity used by the tests. The four-entry fixture is evidence of generality, not a product maximum.

## Automated acceptance tests

| ID | Required proof | Coverage |
|---|---|---|
| 14A.1 | A one-entry Lap Race still starts, runs, completes, seals and reconstructs exactly as before. | baseline behaviour |
| 14A.2 | The existing two-entry Lap Race produces the same lifecycle, timing, authority, Pause/Restart, false-start, Results, Details, History and record outcomes as Stage 1–13. | regression |
| 14A.3 | A four-entry Session Definition is represented with four stable Race Entry IDs, four Start/Finish role mappings and four independent entry associations. | structure |
| 14A.4 | Proposed Race Setup validates and copies every active entry; invalid duplicate or missing mappings are rejected without mutating State. | setup/authority |
| 14A.5 | Four distinct Input Module identities produce normal Message Bus `INPUT_EVENT`s and each event reaches only its mapped Race Entry. | routing/structure |
| 14A.6 | Four entries can accumulate different lap counts, partial progress and lap times without cross-entry state changes. | behaviour |
| 14A.7 | Relevant Time, rather than processing order, determines each entry's lap timing when four-entry events are deliberately delayed and delivered in reverse order. | timing/order |
| 14A.8 | Existing Lap Finish Behaviours produce correct all-entry completion, ranking, laps-behind, tie and fastest-lap results for four entries. | results |
| 14A.9 | Result sealing serializes every active entry, all immutable lap records, ranks and completion data; no entry three or above is omitted. | results/structure |
| 14A.10 | Noticeboard State reports the active count and every entry for counts 0, 1, 2, 4 and `PP_MAX_ENTRIES`; no authoritative path assumes exactly two. | reconstruction |
| 14A.11 | Browser State, Results and Details reconstruct all four entries after refresh/reconnect, while existing Lane 1/Lane 2 compatibility aliases remain correct. | Browser compatibility |
| 14A.12 | Race Again preserves all four configured roles, associations and Lap Race setup, allocates fresh Race Entry IDs and starts the new race with zero competition progress. | lifecycle |
| 14A.13 | A four-entry completed result is persisted and reconstructed in History with all entries, ranks, finish data and immutable lap details. | History |
| 14A.14 | New result and record persistence payloads carry explicit format versions; an implementation test proves that current development data may be reset without being mistaken for a production migration guarantee. | persistence |
| 14A.15 | PB and Track Record eligibility is processed independently for every active entry in a four-entry race, including fastest eligible lap selection and the existing Stage 13 later-fault rules; this does not imply one permanent PB slot per Race Entry or a `PP_MAX_ENTRIES` limit on persistent MUG identities. | records |
| 14A.16 | The current-race records view serializes every active entry and retains Lane 1/Lane 2 compatibility fields without changing record eligibility semantics or introducing a MUG database. | records/API |
| 14A.17 | Simulated four-entry detector passages use the protected Input Module FIFO and normal Message Bus routing; no fixture writes Race Engine State directly. | boundary/integrity |
| 14A.18 | Race Engine completion, ranking, settlement and result algorithms are expressed in entry iteration or capacity-sized structures and contain no algorithmic assumption that the capacity is eight. | structural |
| 14A.19 | No accepted Message Bus type or payload introduces a two-entry assumption; existing numeric values remain unchanged and any new values are appended. | message contract |
| 14A.20 | Result and Browser serializers remain bounded and valid at one, two, four and capacity-sized entry counts; malformed counts cannot cause out-of-bounds access. | safety |
| 14A.21 | A setup above the compiled capacity fails explicitly as an implementation/resource limitation without changing the product model or partially mutating authoritative State. | capacity boundary |
| 14A.22 | A deliberately altered fixed-two-entry evaluator condition fails the acceptance run, proving that the structural checks detect reintroduction of the limitation. | evaluator integrity |
| 14A.23 | The complete applicable Stage 1–13 automated, structural, Wokwi, Browser and regression suites pass unchanged. | regression |
| 14A.T | The deliberate evaluator-failure fixture fails when its expected condition is corrupted and passes again after restoration. | deliberate failure |

## Required four-entry fixture evidence

The deterministic fixture must record:

- reproducible seed/configuration;
- ordered input events and authoritative Relevant Times;
- Session Definition and role mappings;
- Race Entry IDs;
- Noticeboard State before and after each operation;
- Results, Details and History payloads;
- PB/Track Record observations;
- any failure's first divergent State and complete event trace.

The fixture must include unequal lap progress and different valid lap times. At least one scenario must deliver events in an order different from Relevant Time order.

## Compatibility evidence

The acceptance record must show that:

- the default two-entry setup remains the factory/Taster setup;
- existing Stage 1–13 Browser aliases remain usable;
- two-entry Pause, Honour Restart, Grid Restart, false-start, result sealing, History, PB and Track Record behaviour is unchanged;
- existing message numeric values and ownership remain unchanged;
- no Practice or Endurance behaviour has been introduced.

## Persistence rule for this refactor

Current development History, PB and Track Record data may be invalidated or reset when the representation changes. This is permitted because the project is pre-production and has no customer data. The new result and record formats must nevertheless contain explicit version identities so future production data can be migrated rather than silently discarded.

`PP_MAX_ENTRIES` is only the simultaneous active-entry capacity of this firmware build. It is not a limit on the number of persistent MUG identities or PBs. Stage 14A proves multi-entry eligibility processing but does not define or implement a MUG database/configuration system.

## Evidence and deferrals

Evidence must include build-size/static-assert output, structural scans, four-entry traces, Browser/API payloads, History reconstruction, record calculations, capacity-boundary behaviour and the complete Stage 1–13 regression output.

The following remain deliberately deferred:

- Practice mode;
- Endurance mode;
- open-ended or time-limited race semantics;
- retirement, withdrawal, DNF or new participation rules;
- new Yellow/Pause/Restart behaviour;
- new Results or History product semantics;
- new MUG/SMUG configuration behaviour;
- polished multi-lane Browser controls;
- physical multi-lane hardware validation beyond the project-wide Hardware Revalidation Rule.

No implementation is authorized by this proposal. It must be reviewed and frozen separately before firmware changes begin.
