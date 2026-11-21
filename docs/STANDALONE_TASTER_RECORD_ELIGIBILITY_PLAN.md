# Standalone Taster record eligibility

**Status: ACCEPTANCE REFERENCE ONLY.** Separate implementation plan retired; accepted WP8-02 evidence and the remaining physical gates are recorded in the [Master Project Plan](MASTER_PROJECT_PLAN.md). Binding behaviour resides in [Design Specification](PP_DESIGN_SPEC.md#race-control-engine-design--41-session-source-and-record-policy). The [original sequence](https://github.com/ConnalM/Plonk-and-Play/blob/13662fbc5e42fbc83204b7662643a2c43fe8fefb/docs/STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md#implementation-sequence) is preserved historically. Test obligations below are unchanged; consolidation grants no acceptance beyond the separately recorded WP8-02 closure.

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

## Implemented source boundary and evidence (references only)

The isolated PR #3 source at `9f78657` registers a dedicated physical Integrated
Display ingress as `StandaloneTaster`; Browser START retains its controller
lease check. The immutable Session Definition freezes official eligibility and
Race Engine excludes non-official History, PB/Track and official detail
writes. This is consistent with the design section referenced above.

The [18 November initial host evidence](https://github.com/ConnalM/Plonk-and-Play/blob/9f78657830cabbd8ac17108cb738e79c2b8330f6/docs/STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md#initial-implementation-evidence--18-november-2026)
and the later [19 November build/regression evidence](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7012016540)
are preserved. The exact physical
[candidate and recovery checkpoint](https://github.com/ConnalM/Plonk-and-Play/pull/3#issuecomment-7012216516)
remain separately identified. The Master Project Plan owns acceptance status
and outstanding work; neither synthetic detector inputs nor a successful
build prove physical detector/official persistence or Browser acceptance.

## Accepted WP8-02 physical evidence — 21 November 2026

**WP8-02 is CLOSED / PASS.** The accepted application-only image is
`F6A415CF20AE943095491D8AD7DDCB460E228B05CD74B52505C1744F35C1040A`.
Stage 15 passed Browser controller/spectator authority, reconnect and official
record isolation. Stage 13 passed Results lifecycle, Browser/SSE handling and
RACE AGAIN cleanup. Stage 8 passed automated Lap/Endurance, pause/resume,
finish-policy and Browser-handover assertions. The independent guided Stage 8
Taster screen observation also passed, with no observed display glitches.

The immutable full-suite evidence is retained at
`acceptance/evidence/wp8-02-final-20261120-232321/`; the guided Stage 8
evidence is retained at
`acceptance/evidence/wp8-02-guided-stage8-20261121-170852/`. Both record
proposal-baseline restoration. The owner confirmed the `ENDURANCE / 943`
proposal was created during manual +/- testing, not by configuration
corruption.

Original settings and official records were preserved. The temporary SD failure
was a physically absent card; powered-off reinsertion restored access without
data repair or writes. Final read-only checks retained 12 readable History
entries and two explicitly flagged pre-existing unreadable entries, Lane PBs
and Track Record. The source-reviewed `8669011d` change only paces generated
external-test crossings by at least 1.05 seconds; it does not alter production
firmware, rejected input behaviour or the accepted physical image.

This bounded closure proves the stated automated acceptance scenarios and
observed Taster presentation. It does **not** complete the separately open
real-detector, physical standalone-record positive/negative, power-cycle,
general production-storage, Stage 14C/15 production or broader release gates.
Source/host, real-device and human visual evidence remain distinct; no
firmware merge or product-release acceptance is implied.
