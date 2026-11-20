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
