# Timed Stage regression evidence — initial execution

20 November 2026. Candidate: [ec0078a](https://github.com/ConnalM/Plonk-and-Play/blob/ec0078a9aa259430eddbc30a1dbeeebed6ac0ce3/design/smugland/PP_Timed_Stage_review.html), HTML blob 0cf2e99ab6d3a462bfc37ca9c15f9db8446c5461. [Manual](https://github.com/ConnalM/Plonk-and-Play/blob/a91a6affd0aef92b06bbe9cb376c5bffaf9af574/design/smugland/PP_Timed_Stage_manual.md). [Executable suite](https://github.com/ConnalM/Plonk-and-Play/blob/717e378a5b2f38bd40ab20e2dd706a28cb68f0fd/design/smugland/PP_Timed_Stage_regression.cjs).

## Actual execution

Executed candidate JavaScript with a minimal document/element shim and deterministic timer callbacks, not Chromium or physical hardware. 36 checks: **34 PASS, 2 FAIL**. The standalone Node harness is included for reproduction; run `node PP_Timed_Stage_regression.cjs` beside the HTML. Expected non-zero exit until the failures are resolved.

| Area | Checks | Result |
|---|---:|---|
| Defaults, scheduling, single-stage queue, mappings | 8 | 8 pass |
| Ready, countdown, armed, running, split hold and final priority | 8 | 8 pass |
| Retry, accepted slots, false-start sanctions, adjusted time, DNF continuation | 10 | 10 pass |
| Stage boundary, next stage, point-to-point split absence | 3 | 3 pass |
| MUG start protection, read-only auxiliary, result fixture disclosure, expandable detail | 4 | 4 pass |
| Auxiliary stage classification | 1 | Fail |
| Return to active run after navigating to setup | 1 | Fail |
| Event completion | 1 | Pass |

## Blocking corrections

1. Auxiliary results state: add review-only stage/event classification presentation driven by supplied state, without independently calculating race results.
2. Navigation: provide an explicit return to active run from setup and ensure active session review remains accessible without restarting. Guard stage progression against premature navigation; test real DOM and roles.

## Browser matrix still to execute

At desktop 1440, tablet 768 and mobile 390: verify setup card editors, geometry diagrams, Advanced input retention, role loss/reacquisition, all routes, keyboard access, wrapping, horizontal overflow limited to designated scroll regions and no JavaScript errors. At auxiliary 800 × 480: verify ready/countdown/armed/running/split/finish/false start/DNF/retry and stage/event classifications; compare bezel with V21; confirm no clipping of long names. Test navigation during active timing, stage transition and final return, invalid inputs, multiple attempts and retry replacement, single/multi-stage and all geometry fixtures.

## Limits

Source checks are not real browser, layout, sensor, scoring engine, persistence, or firmware tests. Fixed example classification and timing facts are not computed from arbitrary setup. Open sporting choices remain those listed in the manual editorial appendix. The MUG-order precedence check records illustrative behavior only, not an approved sporting decision.

**Not formal-review-ready.** V21 frozen, no Drag, no firmware or Al changes.
