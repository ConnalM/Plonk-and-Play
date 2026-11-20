# WP8 — V21 implementation handoff

**Prepared:** 20 November 2026 | **Owner:** Archie | **Implementer:** Al | **Status:** Prepared only; NOT authorised to start.

## Authority and entry gate

V21 is the owner-approved design baseline, subject to physical testing ([approval](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7039376953)). Use [the frozen HTML](https://github.com/ConnalM/Plonk-and-Play/blob/b794e9e78642d586b107e6982d0372d7dc356b53/design/smugland/PP_Browser_v21_auxiliary_review.html), [design review](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7039289523), [reported prototype evidence](https://github.com/ConnalM/Plonk-and-Play/blob/06fcc4973abda975d912c29ececc6f28ad1bbd2c/design/smugland/PP_Browser_v21_test_evidence.md), [governing specification](PP_DESIGN_SPEC.md) and [single project backlog](MASTER_PROJECT_PLAN.md). Prototype fixtures are illustrative only; governing semantics prevail. Report inconsistencies to Archie before implementation; do not invent missing behavior.

Start only after WP8-02 physical acceptance checkpoint and Archie review, with explicit owner authorisation. Resolve legacy fixture compatibility/release ingress isolation and other relevant pre-merge blockers at their existing gates. Preserve verified baseline and recovery images. No flash, merge or scope extension authorised by this document. Gina is stood down unless a design defect needs review.

## Proposed implementation sequence (subject to feasibility review)

| Step | Existing work package | Deliverable and exit evidence |
|---|---|---|
| A | WP8-04 / WP8-08 / WP8-06 | Inventory current Browser routes, contracts, display resources, storage and test harnesses against V21. Map design fields to authoritative existing facts. Quantify socket/SSE concurrency ( current XIP permits seven HTTP client sockets), heap/PSRAM, rendering and task budgets; prove feasible representative rendering and configuration access. Flag missing capabilities rather than simulate them in production. |
| B | WP8-04 | Implement Browser visual shell, navigation, responsive layouts and screens using real controller setup/state/history/results and existing authority/lease commands. Preserve V18 approved behaviour within V21. Exercise real routes, multiple clients, reconnect and mobile/desktop layouts. |
| C | WP8-03 / WP8-05 | Implement shared remembered display/audio settings and standalone configuration per governing contract; verify persistence and restoration, independent per-lane names/cars, factory defaults, role handoff and standalone restrictions. Do not reset track configuration or racer identity inadvertently. Preserve physical race display and timing. |
| D | WP8-07 | Agree false-start/repeat-start semantics with Archie/owner before firmware work on this item. Implement optional five-red Practice start (immediate default), immediate Practice resume, normal Practice timing and no competitive records. Preserve competitive configured three/five and GO semantics. |
| E | WP8-08 | Implement auxiliary editor, configuration management, SD persistence/recovery and applied renderer, with real parent snapshots, non-disruptive Apply, lifecycle event overrides and readable layouts. Initially accept only actual supported physical lane counts; 1–8 design/review coverage is not proof of production lane capacity. |
| F | All affected WP8 items | Cumulative host, Browser and physical acceptance, recovery and negative tests; Archie review and explicit owner approval before firmware flash/merge at each applicable gate. |

Sequence is a proposal, not a new backlog or a requirement to combine changes into one build. Give each increment its own source/evidence checkpoint. Reorder only by agreed dependencies.

## Non-negotiable behavior

- Existing authoritative controller owns lifecycle, timing, classification, signals, eligibility and commands; Browser/auxiliary display consume facts, do not create a second race engine. Unknown gaps/timings display unavailable. Gap ahead is to preceding classified racer, not leader. No synthetic production winner, timing, specialist modes or race features. Preserve ties/no winner and mode-specific Practice summaries.
- Standalone Taster contract remains intact: two-lane supported installation, remembered Lap/Timed values, start/pause/resume/abandon/finish and Race Again semantics, local operation only without controlling SMUG, automatic authority transfer and recovery, no official standalone records. Built-in graphics per lane only. Preserve ability to race without Browser.
- Distinguish draft, applied configuration and named saves. Save versus Apply independent; cancelling edits must not change live selection. Factory protected manual/fixed configuration is fallback; restore auxiliary defaults retains named custom configurations and does not touch unrelated setup. Race-type association selects at mode selection. Maintain active selection across reconnect/reboot per specification. Native persistence must be tested, not inferred from browser storage stubs. Follow existing storage contract for atomicity, validation, absent/corrupt SD and recovery; do not allow storage I/O to stall race control.
- Preview normally reflects actual parent state; independent review overrides clearly labelled and never alter live race or lane installation. Stable editor across tick/navigation where specified. Applied renderer uses applied data, not draft. Support approved fixed/cycling layout controls and event priority; preserve same lane page and frozen race information under pause/restart/GO, stop rotation when required and restart dwell on return to racing. Preserve winner on every finish page and automatic result pagination. Practice optional countdown five reds regardless of shared competitive preference.
- Maintain HTTP/SSE authority, record isolation and responsiveness under concurrent load; no HTTP callbacks directly mutating main-loop-owned state. Bound queues, memory, redraws, timers, session lifetimes and storage operations. Keep release firmware free of test-only control ingress. No customer Demo/showcase scope.

## Required evidence before acceptance

1. Traceability: each changed approved V21 screen/control and existing WP8 requirement mapped to source, authoritative fact/command and test. Identify deferred functionality and actual physical limitations explicitly.
2. Pre-build: socket-limit regression, relevant host/regression suite, corrected external acceptance scripts and release ingress checks. Preserve existing XIP build constraints unless separately reviewed. Exact commit, config flags, artifact SHA-256, validation and local recovery.
3. Browser: representative responsive layouts, normal lifecycle and mode navigation, real lease transfer/reconnect/concurrent clients, race running while editing/applying, draft retention, role loss, rejected commands, authoritative setup/results/history, no official writes from simulation. Confirm display settings restore isolation.
4. Physical: startup/HTTP health and authority preflight, real supported lane counts, legibility and worst-case redraw/touch performance, event overlay and finish paging, standalone control and genuine touch, actual detector tests where required, shared sounds, power-cycle and SD absence/corruption/fallback. Check non-disruptive apply during race and no lost timing. Measure resource headroom against existing baseline; report any limits before expanded lane claims.
5. Acceptance: distinguish source review, host tests, actual HTTP/controller tests, human screen witness, storage tests and actual detector checks. Preserve records and settings or restore agreed test proposal safely. Record failures and stop conditions; do not infer pixel or hardware acceptance from HTML assertions. Owner approval of design is not approval of production implementation.

## Reporting and scope

Use PR #3 or a separately agreed implementation PR for evidence and Master Project Plan for status; do not create a competing task list. Report blockers or review-ready increments. Keep Gina's V21 artifact unchanged unless an approved defect correction requires a new design revision. Do not start this work while the current acceptance candidate is being corrected/tested.
