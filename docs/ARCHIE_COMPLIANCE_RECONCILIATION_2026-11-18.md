# Archie compliance reconciliation — 18 November 2026

## Status correction

WP8 is **IN PROGRESS**, not future/not started. The physically accepted static
Taster milestone is firmware `3649CEE0EE12EF4630EFD5A0825B57EB37B7C3603543E7A9093AA540691D1A08`, preserved at `C:\Users\conna\Documents\P&P Backups\2026-11-18-WP8-centred-lamps\plonk-and-play-3649CEE0.bin`.

Accepted on physical hardware: approved logo; five centred approved lamps and
bezel; smooth countdown/GO; one-second green; static cars; PAUSE 700 ms amber
`RELEASE` feedback; and 500 ms alternating PAUSED yellow/OFF lamps.  This does
not close WP8 Browser/Taster coexistence, audio, animation or release work.
WP7 real detector/track hardware acceptance remains OPEN.

## Archie decisions — binding, implementation still open

1. Freeze official-record eligibility in the authoritative `SessionDefinition`
   at START. Standalone Taster races are non-official: exclude them from
   official History, PB and Track Records; do not allow a later SMUG connection
   to alter eligibility; preserve simulated-race classification.
2. The approved build description is one full RGB framebuffer plus 20-line
   bounce buffers.
3. The shared configuration, Race Engine, HTTP socket-limit and WP6 adapter
   regressions must be run and their actual results recorded before a controlled
   checkpoint is approved.

## Specification reconciliation and decisions

| Decision | Current implementation/evidence | Specification position | Conflict / decision needed |
|---|---|---|---|
| READY adjustment | 1..999; wraps 999→1 and 1→999; three phases, reaching ±10 at 3 s then 100 ms; one setup submission on release | No conflicting numeric cadence found in the authoritative specs | Record this as an approved WP8 interaction decision; retain GT911 release qualification. |
| PAUSE feedback | Hold for 700 ms changes only the button to amber `RELEASE`; pause remains authoritative on release | Existing lifecycle/pause specifications define authoritative pause but not this local cue | Add as approved local presentation behaviour, not a new authority path. |
| PAUSED lights | Five local 60x60 dirty regions alternate Y/O every 500 ms | Existing plan formerly said disabled pending assets | The old plan statement conflicted; corrected plan records acceptance. |
| Static presentation | Cars/finish flag disabled; local dirty regions, single RGB framebuffer, XIP/Octal PSRAM retained | Animation documents require later event-driven animation | No conflict if static mode remains explicitly deferred, not described as animation completion. |
| Timing/counters | fixed MM:SS positions, stationary colon; LAPS completed/target; ENDURANCE completed only | Taster/common display documents require clear authoritative timing but do not mandate a different accepted geometry | Treat as approved WP8 layout decision; re-check Browser/Taster checkpoint requirements. |
| Record policy | Taster can submit setup/start but no standalone non-official policy is represented in the Taster authority interface | Architecture and Race Control design require frozen source/policy split: standalone Taster non-official vs SMUG official | **Open defect/decision:** define and test the standalone Taster record-policy input before claiming local standalone session support. |
| Build identity | `bwp7welcome` is an application-only ESP-IDF XIP build with one RGB framebuffer plus 20-line bounce buffers | Earlier notes call it simply “single framebuffer” | **Documentation correction:** it is a single full RGB framebuffer, not a zero-buffer or generic single-buffer build; bounce buffers remain part of the approved configuration. |

## Dirty worktree classification

The authoritative raw inventory is `git status --short`; this report classifies
every category without deleting or normalising it.

| Class | Paths/patterns | Proposed treatment |
|---|---|---|
| Intended shared-core changes | modified `firmware/include/pp/{board_display_esp32s3,browser_interface,core,esp32_firmware_update_adapter,noticeboard,race_control,race_engine,taster_runtime,taster_surface}.h`, `firmware/src/main.cpp`, `firmware/platformio.ini` | Separate review and regression gate; do not mix blindly with WP8. |
| Intended WP8 source/artwork | untracked `wp7_physical_racing_ui.h`, presentation/dirty headers, fonts, ready artwork, `firmware/assets/wp7_approved/`, IDF CMake files and targeted probes | Candidate controlled checkpoint after provenance/artwork review. |
| Intended acceptance/tests | untracked WP7/WP8/Stage8A test sources and modified Stage7/Stage8A sources/evidence | Include source tests; exclude compiled executables and caches. |
| Intended documentation | modified master plan; current evidence documents; this reconciliation | Include after Archie approval. |
| Historical/Wokwi diagnostics | modified `firmware/taster_wokwi/*`; untracked scenarios/logs/screenshots; `_http-sse-implementation/`, `_approved-artifacts/` | Keep out of WP8 checkpoint unless a named requirement consumes it. |
| Disposable build/tool artifacts | `bwp7welcome/`, `CMake*`, `.pio*`, `.chrome*`, `.analysis-index`, `toolchain/`, `generated/`, `*.exe`, `*.bin`, `__pycache__`, build logs/readbacks | Exclude from any commit. |
| Investigate before inclusion | `acceptance/stage10/transport_probe_local.py`, `physical_transport_pressure_b58.py`, `_tmp_addr.py`, root temporary/config/log files | Require owner classification; no automatic inclusion. |

## Regression obligations

1. **Shared configuration-record migration:** verify schema/default migration,
   freeze-at-START behaviour, persistence/restart compatibility and Browser/Taster
   agreement for setup, lane assignment and stored records.
2. **Race Engine changes:** rerun lane target guard, finish settlement, lap timing,
   endurance expiry/overtime, pause/resume and two-lane independence tests;
   perform a real detector checkpoint when WP7 hardware exists.
3. **HTTP socket limit:** rerun the socket-limit contract, reconnect/liveness,
   Browser/Taster coexistence and long-running session tests with the configured
   client limit.
4. **WP6 adapter changes:** rerun signed envelope validation, recovery/reset,
   rollback and update state-machine tests; no WP8 acceptance substitutes for
   these recovery obligations.

## Proposed controlled GitHub checkpoint (approval required)

The [review file list and regression results](ARCHIE_CHECKPOINT_REVIEW_2026-11-18.md)
record the current checkpoint proposal and the limits of the host evidence.

**Proposed scope:** (a) approved WP8 static Taster implementation/artwork and
focused source tests; (b) reconciled documentation; (c) only those shared-core
changes which pass the four regression groups above.  Split shared-core/WP6
changes from the WP8 presentation commit if their provenance or tests remain
unclear.

**Include:** source headers/CPP, `firmware/assets/wp7_approved` RGB565 source
assets, `firmware/wp7_idf_arduino_dbuf` CMake/sdconfig source configuration,
accepted test source, test evidence text, master plan and this reconciliation.

**Exclude:** all build trees, `bwp7welcome`, `CMakeCache*`, toolchains,
managed/downloadable dependencies, `*.exe`, firmware/readback `*.bin`, logs,
screenshots unless specifically accepted as evidence, browser-analysis folders,
temporary scripts, credentials and recovery images.

Before any commit: verify a clean proposed index, check artwork hashes against
the approved package, record the candidate firmware hash/recovery checkpoint,
and obtain Archie approval for the policy/build-identity decisions above.
