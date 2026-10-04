# Stage 14A Design — Variable Race Entry Capacity

**Status: ACCEPTED / FROZEN**

Stage 14A removes the current hard-coded two-entry implementation limitation before Practice or Endurance is implemented. It generalises the existing Lap Race data model and execution paths to support a variable number of active Race Entries while preserving all accepted Stage 1–13 behaviour.

Stage 14A does not implement Practice, Endurance, open-ended running, timed races, new finish rules, retirement, withdrawal, DNF, or any other new race mode.

## Product boundary

P&P has no arbitrary product maximum lane count. Two lanes remain the initial, default and Taster configuration. A particular firmware build may have a bounded implementation capacity because the ESP32 uses deterministic static storage, but that capacity is an implementation/resource limit and must never be presented as a product rule.

The proposed implementation capacity is `PP_MAX_ENTRIES = 8`. This is an engineering default, not a claim that P&P supports only eight lanes. No Race Engine algorithm may depend on the value eight. Increasing the capacity later must require storage/configuration changes and additional validation, rather than a new competition algorithm or product redesign.

The four-entry fixture described below is acceptance evidence only. It is not a product maximum.

## Existing behaviour that remains authoritative

The following remain unchanged:

- default/Taster setup is two lanes;
- Lap Race lifecycle, GO/start sequence, false-start policy, Pause settlement, Honour Restart and Grid Restart;
- Relevant-Time ordering and all accepted timing boundaries;
- Race Director and Spectator authority boundaries;
- Race Engine ownership of competition and results;
- Input Module role-neutral event and settlement behaviour;
- Noticeboard as authoritative reconstruction State;
- Results, Details, History, PB and Track Record semantics;
- Stage 1–13 Message Bus numeric values and ownership boundaries;
- Taster, Attract and existing Browser behaviour for the default two-entry configuration.

Stage 13 deferred multi-lane expansion is the subject of this refactor. No other Stage 1–13 requirement is superseded.

## Proposed data model

### ProposedRaceSetup

Replace the separate first and second entry fields in `ProposedRaceSetup` with an indexed collection:

```cpp
struct ProposedRaceEntry {
    SessionInputRole startFinish;
    uint32_t mugId;
};

struct ProposedRaceSetup {
    ProposedRaceEntry entries[PP_MAX_ENTRIES];
    uint8_t activeEntryCount;
    // existing Lap Race configuration remains here
};
```

Validation iterates over every active entry. It preserves existing validity rules for input identity, Start/Finish role, lane identity, MUG identity and Lap Race configuration. The default setup populates two entries exactly as it does today.

Temporary first-entry and second-entry accessors may remain as compatibility helpers, but core code must use the indexed collection and count.

### SessionDefinition and role mappings

`SessionDefinition` stores `entryCount`, one `RaceEntryDefinition` per active entry and one role mapping per active entry. Session construction copies all configured entries and assigns stable Race Entry IDs in order. Race Again and other existing setup-copying paths copy the complete collection, not only the first two entries.

`nextRaceEntryId` advances by the active entry count. Existing two-entry IDs and ordering remain unchanged.

### Race Engine storage and algorithms

Race Engine entry state, penalties, pending event state and completed-result state use `PP_MAX_ENTRIES` storage and the active entry count. Every loop, lookup and completion operation is indexed by `entryCount`.

The current two-entry completion masks must not remain the algorithmic basis. Completion/observation state should use indexed flags or a capacity-sized bitset whose operations are expressed as entry iteration. No algorithm may assume that eight is the number of entries.

Result sealing must rank all active entries using the existing comparison rules and stable ordering. The current special case that swaps only two result entries becomes a stable multi-entry ordering operation. Existing two-entry tie and completion ordering must remain identical.

The existing `laps()` and `lastLapTime()` convenience accessors may continue to summarize entry zero for compatibility. Indexed accessors are authoritative for multi-entry behaviour.

`MaxLaps` remains the existing per-entry storage limit. This stage does not alter lap-count semantics.

### Noticeboard State

`NoticeboardState` contains `entryCount` and one authoritative `NoticeboardEntryState` for every active entry. State construction copies all active Race Engine entries.

Existing top-level first-entry summary fields may remain as compatibility aliases for Stage 1–13 clients. They must not be used as the complete multi-entry representation.

### CompletedRaceResult and History

`CompletedRaceResult` contains an indexed result entry for every active entry, including immutable lap records, rank, laps-behind, completion information and best-lap information.

The new result and persistence representations must be explicitly versioned. There is no requirement to migrate current development History, PB or Track Record data: this project is pre-production and those existing development formats may be invalidated or reset when the representation changes. Future production data must have a version identity and a migration path.

History reconstruction must decode the new version into the same authoritative Results and Details model without Fact replay or Browser calculation.

The result payload size remains guarded by a compile-time assertion against the History storage bound. The bound must be increased or otherwise sized for `PP_MAX_ENTRIES = 8`.

### Race Control and Race Again

Race Control copies all entries, role mappings, MUG associations and Lap Race configuration when preparing Race Again, Restart Race and other existing setup-copying operations. No operation may silently drop entries three and above.

### Browser State, Results and Details

The authoritative Browser/API representation uses a dynamic entry collection:

```json
{
  "entryCount": 4,
  "entries": [
    { "raceEntryId": 1, "laps": 2 },
    { "raceEntryId": 2, "laps": 1 },
    { "raceEntryId": 3, "laps": 2 },
    { "raceEntryId": 4, "laps": 0 }
  ]
}
```

Results and Details serialize every active entry and all immutable lap records. Browser reconstruction uses current authoritative State and persisted result data, never fixed two-entry assumptions.

Stage 1–13 top-level fields and Lane 1/Lane 2 records aliases may remain for compatibility. The indexed collection is authoritative.

The human-readable diagnostic Browser may continue to expose only the default two simulated detector controls. Multi-entry acceptance uses the normal API/Input Module boundary and does not require a polished multi-lane control surface.

### PB and Track Records

PB and Track Record processing is generalized so every active Race Entry in a session is evaluated independently. This removes the current two-entry assumption from eligibility processing while preserving exactly the Stage 13 eligibility rule.

`PP_MAX_ENTRIES` limits simultaneous active Race Entries in this firmware build. It does not limit the number of persistent MUG identities or PBs that the product may ultimately retain. Stage 14A does not introduce a MUG database or complete MUG/SMUG configuration system. Persistent identity storage remains an implementation boundary to be defined by that future configuration work.

The records API gains an indexed view of the active entries for the current race while retaining Lane 1 and Lane 2 compatibility aliases. Multi-entry acceptance must prove PB/Track Record processing for every active entry without implying one permanent PB slot per Race Entry.

The new persisted record representation is versioned. Existing development records may be cleared or reset during the format change; future production records must be migratable.

### Input Module and acceptance fixtures

The simulated detector/source arrays used by the development and acceptance boundary are generalized to the same implementation capacity. Events still enter through the normal protected Input Module FIFO and Message Bus. Fixtures must not manipulate Race Engine timing or lap state directly.

The default Stage 1–13 simulator remains two-entry. A deterministic four-entry fixture supplies additional input identities and role mappings only for this refactor's acceptance evidence.

## Capacity and resource rationale

Static bounded storage is appropriate for the ESP32 because it provides deterministic memory use, avoids race-time heap fragmentation, keeps persistence sizing explicit and supports compile-time assertions.

With the current 16-lap per-entry records, an additional active entry is expected to add approximately 1–1.5 KB of live engine/result storage. Moving from two to eight entries is therefore expected to add approximately 8–10 KB of live RAM. A completed eight-entry result is expected to require approximately 5.5–6 KB before codec overhead. `HistoryStore::MaxBytes` should be increased to at least 8192 bytes, subject to measured sizes and storage verification.

The exact sizes must be measured during implementation. Capacity changes later must be confined to the capacity definition, storage sizing, serializers/codecs and tests. They must not require changes to Lap Race timing, authority, settlement or ranking concepts.

If a setup exceeds the compiled implementation capacity, it may be rejected as unavailable in this firmware build. That message must describe an implementation/resource limit, not a product maximum.

## Affected components

- `firmware/include/pp/session_definition.h` — setup and Session Definition collections.
- `firmware/include/pp/race_engine.h` — entry storage, completion state, ranking and result storage.
- `firmware/include/pp/noticeboard.h` — variable entry State.
- `firmware/include/pp/race_control.h` — complete setup copying and Race Entry ID allocation.
- `firmware/include/pp/record_store.h` — variable PB storage and versioned record data.
- `firmware/include/pp/history_store.h` — result sizing and versioned persistence codec integration.
- `firmware/include/pp/browser_interface.h` — dynamic State, Results, Details, History and Records serialization.
- `firmware/include/pp/core.h` — shared capacity and generalized simulated Input Module storage.
- `firmware/src/main.cpp` — default two-entry setup remains unchanged; fixtures gain indexed setup support.
- `firmware/tests` and `acceptance` — deterministic multi-entry fixtures and structural/evaluator checks.

Relevant governing constraints are in `docs/SYSTEM_REQUIREMENTS.md`, `docs/SYSTEM_ARCHITECTURE.md`, `docs/RACE_CONTROL_ENGINE_DESIGN.md`, `docs/MESSAGE_CONTRACT.md`, `docs/PP_TASTER_SPEC.md`, `docs/COMMON_DISPLAY_SPEC.md` and the accepted Stage 1–13 documents.

## Risks and compatibility

- Fixed-width completion masks may overflow or silently omit entries.
- Two-entry sorting and setup-copying special cases may be hidden in less obvious paths.
- Existing Browser clients may expect Lane 1/Lane 2 aliases.
- Binary result and record formats require explicit version identities for future migration.
- Dynamic Browser responses may exceed current fixed buffers.
- Race Entry ID allocation must remain unique across Race Again and multi-entry sessions.
- Default Taster and two-entry behaviour must not be changed by generalized storage.
- The full Stage 1–13 automated/regression suite must remain passing.

No Practice or Endurance design is introduced by this document.
