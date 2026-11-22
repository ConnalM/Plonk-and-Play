# Drag design review — regression and provenance

**Status: consolidated review candidate; Chromium execution pending.** No final design approval claimed. All tests concern supplied visual-review fixtures, not the production race engine.

## Exact committed source

- [HTML, commit 87cf26a](https://github.com/ConnalM/Plonk-and-Play/blob/87cf26aa6ed12913ca59f24d35fd0bcedbdeadc4/design/smugland/PP_Drag_review.html), **Git blob 3a17cce430c5b447f0ab1be6986db4cdba97bc83**. Only the V1 scope label differs from the previously refined working candidate; Timed Stage and V21 unchanged.
- [Source regression, commit 0ec2491](https://github.com/ConnalM/Plonk-and-Play/blob/0ec24913011c28c662ef54ec95fff94e0761b012/design/smugland/PP_Drag_regression.cjs).
- [Chromium regression, commit b76b93b](https://github.com/ConnalM/Plonk-and-Play/blob/b76b93b45f11280af6fd92d23f332e42ef1e78dc/design/smugland/PP_Drag_browser_regression.py).
- [Concise manual chapter, commit ad0ec57](https://github.com/ConnalM/Plonk-and-Play/blob/ad0ec57053eac2118f24320b124507b10f8158f8/design/smugland/PP_Drag_manual.md).

## Execution log

| Gate | Status | Evidence |
|---|---|---|
| Source-level rules and state-transition assertions | **PASS, 43/43** | Full committed script's `test(script)` executed in an isolated JavaScript runtime against the exact committed HTML script. All 43 rows passed. |
| HTML JavaScript syntax | **PASS** | Function compiled from exact committed script during regression. |
| Chromium desktop 1440×900 | **NOT RUN** | Reproducible script committed; interactive/visual evidence not yet gathered. |
| Chromium tablet 768×1024 | **NOT RUN** | Reproducible script committed. |
| Chromium mobile 390×844 | **NOT RUN** | Reproducible script committed. |
| Auxiliary 800×480 no overflow | **NOT RUN** | Exact frame, scroll clipping, tree and red-light checks are scripted, not yet executed. |
| Physical Taster and firmware acceptance | **OUT OF SCOPE** | Remains Al's later implementation acceptance work. |

Source-level coverage includes: two-sensor mapping; single-beam staging and rollback; pre-tree reblock without false start; blocked-tree start rejection; Sportsman sequential amber and Pro simultaneous amber; GO and separate reaction/ET; early crossing and negative reaction; valid/red-light results; optional average and scale-equivalent speed; absent ET; dormant REU-only trap speed; view-only authority; no SAVE; immutable session-local fixture history; reset of stale metrics on NEW SETUP; rejection of duplicate Finish.

## Chromium reproduction

Install the Python Playwright dependency and its Chromium browser, place the linked HTML and Python suite together, then run:

```bash
python -m playwright install chromium
python PP_Drag_browser_regression.py
```

With system Chromium, set `CHROMIUM_PATH` to the executable. The suite prints the exact HTML Git blob and individual PASS/FAIL rows and exits nonzero on failures. The suite does not operate live hardware. Before using its output for approval, verify the printed blob equals **3a17cce430c5b447f0ab1be6986db4cdba97bc83**, investigate failures, and retain the test output. No browser pass count is asserted prematurely.

## Design semantics and constraints

Design specification §6 controls base **single-lane two-sensor Drag**. Optional length and track scale do not block reaction and ET, PRE-STAGE remains dormant on the base hardware, and red-light runs can retain negative reaction and elapsed time. The engineering-only signal injector and session-local results are explicitly labelled simulated, not production result storage. No new drag sporting rule was invented.

Open acceptance gates: independently execute Chromium suite and inspect auxiliary physical-scale readability, plus later real detector, reaction/ET accuracy, authority and eligible records integration under implementation assignments. **Not** part of this review: dual-lane competition, dual-beam REU, hardware speed trap, V21 or Timed Stage modifications, Al work, production storage or merging.

## Scope

Owner explicitly placed **basic Drag and Timed Stage in V1** on [issue #2](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7061304177). This package is only the Drag visual design; V1 production implementation is separate.
