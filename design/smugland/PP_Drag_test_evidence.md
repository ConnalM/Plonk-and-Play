# Drag — final visual design regression evidence

**Gina final submission candidate for Archie/owner approval, 22 November 2026.** This evidence supersedes the preliminary unexecuted-browser status. No design freeze is claimed until owner approval.

## Exact reviewed artifacts

| Artifact | Exact commit / blob |
|---|---|
| [Drag HTML](https://github.com/ConnalM/Plonk-and-Play/blob/62bb2657519db57a065570c953311efdd638efdb/design/smugland/PP_Drag_review.html) | Commit `62bb2657519db57a065570c953311efdd638efdb`; **Git blob `b98fc6a090dae4f6348ff015f79949dc3affb94a`** |
| [Drag manual chapter](https://github.com/ConnalM/Plonk-and-Play/blob/ad0ec57053eac2118f24320b124507b10f8158f8/design/smugland/PP_Drag_manual.md) | `ad0ec57053eac2118f24320b124507b10f8158f8` |
| [Source regression](https://github.com/ConnalM/Plonk-and-Play/blob/0ec24913011c28c662ef54ec95fff94e0761b012/design/smugland/PP_Drag_regression.cjs) | `0ec24913011c28c662ef54ec95fff94e0761b012` |
| [Chromium regression with geometry and screenshots](https://github.com/ConnalM/Plonk-and-Play/blob/5a41ef3e5dc130c3f11e822ffc93f471ec58e7dc/design/smugland/PP_Drag_browser_regression.py) | `5a41ef3e5dc130c3f11e822ffc93f471ec58e7dc` |
| [Continuous test workflow](https://github.com/ConnalM/Plonk-and-Play/blob/8ebfe6aabd20e917d7702edded2bc8ec16bc6975/.github/workflows/gina-drag-browser.yml) | Workflow on design branch only |

**Final executed GitHub Actions run:** [45066450633 — success](https://github.com/ConnalM/Plonk-and-Play/actions/runs/45066450633), head commit `5a41ef3e5dc130c3f11e822ffc93f471ec58e7dc`. The executed browser program printed **Git blob b98fc6a090dae4f6348ff015f79949dc3affb94a**, matching the HTML above. Full logs and PNG screen evidence are retained in the [run's artifact](https://github.com/ConnalM/Plonk-and-Play/actions/runs/45066450633/artifacts/14771721833).

## Executed results

| Check | Pass / total |
|---|---:|
| Node/source transitions and presentation fixtures | **43 / 43** |
| Chromium desktop 1440×900 | **24 / 24** |
| Chromium tablet 768×1024 | **24 / 24** |
| Chromium mobile 390×844 | **24 / 24** |
| Pro tree, early RED LIGHT, red auxiliary geometry, retry and history | **10 / 10** |
| MUG view-only control | **1 / 1** |
| Unknown ET handling | **1 / 1** |
| **Total** | **127 / 127** |

No browser page exceptions occurred in the executed desktop, tablet, mobile or Pro scenario checks. The viewport tests verified initial setup, staging, blocked STAGE, rollback, reblocking before start without red-light, sequential Sportsman amber, GO, reaction, elapsed-time result, no invented speed, unavailable REU fields, full seven-lamp tree, session-local fixture results and setup locking.

Pro tests verified all-three-ambers start, premature crossing RED LIGHT with negative reaction, retained 6.71 s sample ET, whole-run average speed not labelled trap speed, 800×480 red display, RACE AGAIN, and isolated review history. View-only STAGE was disabled; missing ET remains visibly unavailable.

## Physical-pixel screenshot inspection — 800 × 480

The actual browser-rendered PNG captures were inspected after the successful run. Both the **valid result** (ET **6.84 s**, reaction **0.23 s**, green lamp) and **false start** (**RED LIGHT** visually dominant, ET **6.71 s**, reaction **−0.08 s**, red lamp) fit and remain legible inside the native 800×480 display. PRE-STAGE is dormant, all seven lamps/labels remain visible, the REU trap-speed line is marked unavailable, and the bottom sensor mapping is separate and readable.

The original script's frame/no-scroll assertions had missed overlapping content. The **initial run failed 77/80**; screenshot inspection showed collapsed panels. Root cause: **a missing closing quote in the dynamically generated `class` attribute of the Drag result panel**. Fixed in HTML commit `62bb265`. Regression added actual panel/text bounding boxes, minimum usable panel heights, tree-versus-metrics separation and text-within-panel assertions; the enhanced final browser suite passed **84/84**. The defect was not excused by the earlier false no-overflow pass.

Responsive results: all three normal page widths passed document overflow checks, and the results table is wrapped in a horizontal-scroll container to retain columns on narrow screens. The scripted checks did **not** prove all text is readable on a physical handset or test touchscreen interactions by a human.

## Limits and safety boundary

Design review uses explicitly **simulated** detector/GO/finish inputs and immutable example results. The review-only signal injector is **not** a production SMUG control and simulated history is **not** persistent product History, Personal Best, Track Record or storage. No actual sensor event ingress, authoritative timing, race engine, SD/NVS, race records, firmware, flashing or physical Taster testing was performed.

Production acceptance must separately verify Start/Finish mapping, real reaction/ET timing and precision, Director/view-only authority, false-start lifecycle and eligible automatic record integration, controller recovery and physical readability. This candidate is for basic **V1 single-lane two-sensor Drag only**. Dual-beam REU, trap-speed hardware and two-lane competition Drag remain out of scope. Frozen SMUGLAND V21 and Timed Stage untouched; no Al work changed; no merge.

Owner V1 scope: [issue #2](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7061304177). Archie visual gate: [review](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7085118538).
