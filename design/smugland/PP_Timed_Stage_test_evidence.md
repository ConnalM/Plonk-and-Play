# Timed Stage regression evidence — consolidated design candidate

20 November 2026. Post-V1 design review only; issue #2.

- [Tested HTML, f76de70](https://github.com/ConnalM/Plonk-and-Play/blob/f76de701dd80a41ff0a61be389035d8f8e1d9a67/design/smugland/PP_Timed_Stage_review.html), Git blob `addf506daf8e8e142f3dda84d7f2e9afce6cbf5d`.
- [Manual, a91a6af](https://github.com/ConnalM/Plonk-and-Play/blob/a91a6affd0aef92b06bbe9cb376c5bffaf9af574/design/smugland/PP_Timed_Stage_manual.md).
- [Source-level regression suite, fd94f4b](https://github.com/ConnalM/Plonk-and-Play/blob/fd94f4bcd1286dc4d53dfd3ba66034e67e65ec69/design/smugland/PP_Timed_Stage_regression.cjs).
- [Reproducible Chromium regression, 3513a79](https://github.com/ConnalM/Plonk-and-Play/blob/3513a79ba8ef3200f90f2e602bacb093e27c9154/design/smugland/PP_Timed_Stage_browser_regression.py).

## Results

| Test group | Passed | Failed |
|---|---:|---:|
| Source-level JavaScript assertions (document shim, deterministic timers) | 39 | 0 |
| Chromium desktop, 1440 × 900 | 23 | 0 |
| Chromium tablet, 768 × 1024 | 23 | 0 |
| Chromium mobile, 390 × 844 | 23 | 0 |
| Chromium auxiliary display states, actual 800 × 480 element | 12 | 0 |
| Total assertions | 120 | 0 |

The 39 source checks were executed again against the final commit. Chromium executed the exact HTML bytes (local Git blob hash matched the final fetched GitHub blob). Playwright loaded the contents into an isolated headless Chromium page; no live-origin, real controller, network or hardware testing is claimed. The browser tests were performed in two scripts, consolidated into the reproducible Python file above; the consolidated file itself has not been independently executed after publication. Counts include repeated assertions at different viewport sizes, not 120 unique behaviours.

## Defects identified and corrected

1. Initial source execution: 34/36 passed. Missing auxiliary stage classification and no return to active run from setup. Corrected at [59ef1dd](https://github.com/ConnalM/Plonk-and-Play/commit/59ef1dd240e95b39fdcf20215f5942b0c77ef057). The auxiliary classification assertion was moved to the actual stage boundary, with separate checks for next-stage live display and final classification; navigation return explicitly tested. The active event configuration stays locked.
2. Initial Chromium execution: 68/69 passed. Mobile setup inputs and the attempt-order select exceeded the viewport. Corrected at [f76de70](https://github.com/ConnalM/Plonk-and-Play/commit/f76de701dd80a41ff0a61be389035d8f8e1d9a67). The rerun passed 69/69.
3. Auxiliary state check: 12/12 fit the 800 × 480 element without measured scroll overflow; visual spot-check of the stage classification screenshot confirms readable default fixture. Exact frozen V21 bezel appearance has not been independently compared.

## Screen/state matrix and coverage

| Area | Evidence |
|---|---|
| Setup | Default order/options, separate attempt order, single-stage queue, loop/A-to-B/B-to-A mapping examples, advanced entry retention, responsive setup layout. |
| READY and start | READY accepted, three illustrative countdown lamps, authoritative simulated GO and timer zero. |
| Running and split | Supplied running time, precise split, three-second default, configurable hold (real one-second test in Chromium), return to live snapshot, split table retention, finish cancelling pending hold at source level. |
| Run review | Raw/adjusted time distinction, false-start sanction and DNF paths, retry same slot, no retry on valid finish, accepted slot progression. |
| Stage/event progress | Default queue, alternative racer ordering, illustrative MUG order, DNF continuation, stage boundary, NEXT STAGE, final slot/event completion. |
| Navigation and authority | Return from locked setup without reset, MUG cannot start, role restoration, results and details view. |
| Results | Supplied example classification disclosure, expandable lap splits, auxiliary stage and final classification, next-stage return to live view. |
| Layout | Desktop/tablet/mobile document fit for setup, run, run review, auxiliary and results; no uncaught JS errors during tested actions. |
| Auxiliary | READY, countdown, GO armed, running, split, return to live, finish, false start, DNF, retry, stage results, event results. Fixed-size preview and default-fixture dimensional checks. |

## Reproduce

Run beside the HTML:
```sh
node PP_Timed_Stage_regression.cjs
python PP_Timed_Stage_browser_regression.py
```
Python requires Playwright with Chromium available (or set CHROMIUM_PATH to a system Chromium binary). Nonzero exit indicates failed assertions; inspect output.

## Limits and review questions

- Review fixture, not production race/scoring engine. Timing, sensor identity, results, gaps and split examples are labelled simulated. Classification and matrix are fixed illustrative snapshots, not authoritative outputs from arbitrary custom setup. In particular stage and cumulative example totals are not recalculated as slots are accepted. No storage or records are written.
- The five sporting questions in the manual appendix await Archie/owner decision, including MUG/attempt-order interaction, DNF best-run/comparator cases, tie/interim standings, repeated false starts and retry declined/failed. MUG order precedence is illustrative, not an approved new rule.
- Representative responsive checks, not exhaustive accessibility, keyboard, extremely long names, arbitrary numeric input, complete signal permutations, native storage, physical rendering/readability or exact V21 bezel fidelity. The approved screen concept is preserved as reference, but visual fidelity and result semantics remain for formal review.
- No V21 modifications, Drag work, Al work, firmware, flash or merge.

**Submit as a tested design review candidate, not final design acceptance or production readiness.**


## Final correction round — 20 November 2026

Owner-approved sporting decisions: [issue #2](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7046590097). Prior review: [Archie's corrections](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7046418072).

Final candidate commits:

- [HTML c26ff55](https://github.com/ConnalM/Plonk-and-Play/blob/c26ff55dc6d21cd1317d92ca239c3db063b85f07/design/smugland/PP_Timed_Stage_review.html), exact Git blob 4820e7c028d80b44360263515f917a9754f7e879.
- [Manual d3f46e8](https://github.com/ConnalM/Plonk-and-Play/blob/d3f46e8a56e134e11388e6127d957283259167af/design/smugland/PP_Timed_Stage_manual.md).
- [Source suite a4f2b66](https://github.com/ConnalM/Plonk-and-Play/blob/a4f2b66d96ac3917726ebd327c71b0dc5dfeef48/design/smugland/PP_Timed_Stage_regression.cjs).
- [Chromium suite d76fe2b](https://github.com/ConnalM/Plonk-and-Play/blob/d76fe2b77613797cf99eb132d6bdbb5577cb8b77/design/smugland/PP_Timed_Stage_browser_regression.py).

| Executed against exact final HTML | Passed | Failed |
|---|---:|---:|
| Source regression (expanded) | 46 | 0 |
| Chromium desktop/tablet/mobile | 69 | 0 |
| Chromium auxiliary states | 12 | 0 |
| Targeted real-DOM Chromium checks | 19 | 0 |
| Total | 146 | 0 |

The exact revised HTML was reconstructed locally from the previously verified blob and a delta; Git blob hash matched the fetched final revision before Chromium execution. The browser checks ran as three local scripts; the committed combined Python suite contains equivalent checks and strengthened phase assertions but has not itself been executed as a single file. The earlier 120 result applies to the earlier candidate; the 146 count above supersedes it and includes repeated viewport assertions.

### Approved decisions and correction coverage

1. MUG order overrides attempt ordering; selection disabled under MUG order, restored under Stage order. Queue ordering verified.
2. Best Run fastest valid adjusted time; all DNF uses configured rule, no slowest comparator uses separately configured fixed DNF time. Representative labelled supplied example, not a scoring engine.
3. Competition rank 1,1,3, leader gap and provisional unequal-stage-count caution shown in supplied examples. No production classification calculations.
4. False start warning/penalty requires Director confirms position and restarts countdown; timer zero, penalties accumulate per crossing, DNF immediate. Tested double penalty and stale finish rejection.
5. Further retries discretionary, fallback DNF rule selected in setup, only final accepted slot counts; repeated retries and decline path verified through accepted slot record.
6. Acceptance identity protected by loadedSlot/cursor match; repeated acceptance at boundary rejected until NEXT STAGE. Verified by injecting actual DOM action buttons, including forbidden valid-run retry.
7. Asserted actual auxiliary heading Stage classification at boundary and Event results at end; READY resumes on NEXT STAGE. 
8. Frozen V21 embedded auxiliary source inspected: SVG border 5px solid #303439, radius 9px, background #091521. Matched these properties in separate Timed Stage bezel, verified computed CSS in Chromium at 800x480. This is source-style fidelity, not a pixel-identical image comparison or physical device acceptance.

All sporting decisions are now documented; earlier open discussion in this evidence is historical. The remaining limitations are design-only supplied timing/classification, representative rather than exhaustive validation, and no firmware, storage, hardware, or production scoring. Submit for **final design approval**; V21 frozen, Drag and Al untouched.
