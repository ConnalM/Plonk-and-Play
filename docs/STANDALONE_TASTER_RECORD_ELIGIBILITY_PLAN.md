# Standalone Taster record eligibility

**Status: ACCEPTANCE REFERENCE ONLY.** Separate implementation plan retired; execution tasks belong to WP8-02 and the physical gate in [Master Project Plan](MASTER_PROJECT_PLAN.md). Binding behaviour resides in [Design Specification](PP_DESIGN_SPEC.md#race-control-engine-design--41-session-source-and-record-policy). The [original sequence](https://github.com/ConnalM/Plonk-and-Play/blob/13662fbc5e42fbc83204b7662643a2c43fe8fefb/docs/STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md#implementation-sequence) is preserved historically. Test obligations below are unchanged; consolidation grants no acceptance.

## Acceptance evidence

Use fresh host tests with real Race Control/Engine flow and instrumented History, PB/Track and detail-store fakes. Seed existing values, send valid passages faster than the seed so a leak is detectable, and compare values, sequence, write counts and emitted facts before and after. Host fixtures exercising real-source policy must use a detector fake without enabling the development-simulation classification.

| Case | Required result |
|---|---|
| Standalone Lap Race and Endurance | Trusted local START freezes non-official eligibility. Both modes seal results and preserve available live timing. No official History append or sequence advance, PB/Track update, official detail write or `HistoryStored` fact. Exercise both Endurance finish policies. |
| SMUG arrives after standalone START | Repeat arrival during countdown, active racing, settled pause and after finish. Exercise authorised handover where supported. Eligibility remains non-official and no later persistence or Browser reconstruction publishes the race as official. |
| Official SMUG starts, then disconnects | Valid eligible runs still produce the expected official History and record updates after control changes. Exercise both lanes, both race modes and normal finish settlement. |
| Simulation, practice and abandonment | Existing simulated flag and exclusion remain observable. Open Practice and abandoned/invalid results retain existing dispositions. |
| Trusted source and ordering | Cover unprivileged and wrongly labelled input, a local request while SMUG controls, concurrent START attempts and both orderings of lease transition versus START. One accepted START freezes exactly one classification. |
| Session transitions | Pause/resume and in-session restart retain eligibility. RACE AGAIN and RESTART RACE followed by a new START recompute using the new authority, with no inherited write state. |
| Storage and detail bounds | Standalone racing works without official storage. No false pending or storage fault solely from skipping records. Long runs respect the bounded detail contract. Official persistence and fault handling retain their existing behaviour. |

At the separately authorised physical checkpoint, record the application hash and seed official History, PB and Track Records on representative storage. Run a standalone race, connect SMUG before finish, then verify the three official views and storage snapshots through Browser reconnect and power-cycle. Repeat a newly started official SMUG race as a positive control and retain the existing simulated-race check. Record the test IDs, pre/post values, session IDs, frozen classification, visible live results and serial or request evidence. Credit physical results separately from host assertions and preserve Stage 14C and Stage 15 Phase 8B acceptance requirements. Real detector acceptance remains a WP7 hardware obligation.
