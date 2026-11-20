# V21 — final bounded corrections

Review candidate: b794e9e78642d586b107e6982d0372d7dc356b53
Responds to issue #2 comment 7038910090. V20 remains unchanged.

## Changes
- Auxiliary pause and resume: preserve the running sequence item and lane page during PAUSED, resume STARTING and GO. Keep race information beneath a compact translucent overlay (five flashing yellow lamp positions for competitive paused review fixture; configured red lamps for resume; all five green or all off at GO). Page cycling stops for interruption and resumes on original page with a fresh dwell after GO. Practice resumes immediately without competitive yellow signalling. Timed Stage pause retains neutral overlay.
- Competitive gap: parent fixture supplies gap ahead using preceding classified entry, not lane order or leader, based only on supported lap-count difference; same-lap timing and ambiguous predecessor are unavailable (—). Browser live rows, parent snapshot and sealed live results share the fact. Auxiliary setting/header explicitly Gap ahead. Static historic and specialist fixtures retain existing semantics.
- Optional Practice initial countdown: shared effectiveStartReds() used by parent tick/GO, Browser pod and auxiliary snapshot; five reds irrespective of competitive 3/5 choice. Choice remains optional, shared GO style unchanged. Review selector displays fixed five during Practice and restores competitive preference on mode switch.

## Executed checks
Headless Chromium via Playwright, in-memory HTML; JS syntax checked for both parent and embedded editor.
- 128 assertions in cumulative V20 regression suite rerun against V21 (with Practice label and effective count expectation updated). Includes 66 route/settings/authority/layout assertions across V18 and V21 at 1440/768/390; 62 auxiliary/lifecycle/storage assertions.
- 28 focused assertions passed: predecessor gap 3/1/2 vs leader 3/4/6; matching Browser/snapshot, explicit label, non-lane order, same-lap unavailable, tie ambiguity; multi-screen/page interruption at page 3; page and names preserved; yellow on/off five lamps; Browser flash elements; clock and sequence freeze; three-red resume; green and lights-out GO overlays; restored original page, subsequent cycling; Practice five reds despite competitive preference three (fourth/fifth before GO); immediate Practice resume; competitive three-red preference restored; no JS errors.
- Focused fixtures set deterministic parent state/tick values for reproducibility and check actual timer-driven transitions. Initial test-harness timing/context failures corrected before passing rerun; no outstanding failures in these suites.

## Limits
This is still a design prototype, not firmware or physical display acceptance. As in V20, browser storage reload/fallback exercised using a test-only stub, not native-origin persistence. Hardware, SD, actual optional Practice and 1–8 lane expansion remain later technical work. The competitive pause signal is illustrated by the existing paused fixture; detailed RMS/Yellow Off behaviour is not claimed. Flash phase is presentation-only, not GO or race authority. No new layout or configuration manager design, no production changes. No merge. Archie to review before freeze.
