# WP8 S4b fifth increment — board-neutral startup and service ownership

This document records the S4b fourth-increment inventory and the deliberately
small extraction made from it. It does not create a Core or Taster firmware,
select a board, alter the accepted integrated entry point, or establish linked
image isolation.

## Integrated inventory

The only live entry point is `firmware/src/main.cpp`. Its current `setup()`
constructs NVS-backed preferences/configuration, optional physical SD/history/
records/MUG/detail stores, simulated input, Race Control, Race Engine,
Noticeboard, Browser/HTTP/Wi-Fi/OTA, diagnostics, and the accepted local Taster
display/touch authority. Its current authoritative loop order is:

1. Memory tick and (acceptance-only) queued fixture handoff.
2. Detector readiness, Race Control, Input Module, then Race Engine.
3. Accepted physical Taster UI, detail storage/recovery, then Browser and
   firmware-health service.
4. Diagnostics and configuration-result handoff.

The existing physical Taster composition is therefore intentionally retained.
`main.cpp`, its source order and integrated service ordering are not changed by
this increment. The two new entry-point files are compile-only boundary
descriptions: they provide neither `app_main`, `setup`, `loop`, board mapping,
hardware activation, nor substitute sporting services.

## Ownership map and extraction boundary

| Area | Future owner | Current source boundary | S4b fourth-increment action |
| --- | --- | --- | --- |
| Race Control, Race Engine, timing, Input/physical detectors | Core | `core.h`, `race_control.h`, `race_engine.h` and integrated `main.cpp` | no sporting extraction yet |
| Browser/SMUGLAND, HTTP/SSE, OTA, History/PBs/Track Records, SD and committed shared preferences | Core | `browser_interface.h`, stores and integrated `main.cpp` | no storage/server extraction yet |
| Display, GT911 touch, local gesture/presentation preferences and gated remote requests | Taster | WP7 display/UI and Taster link headers | no display/touch extraction yet |
| Approval facts, role startup state and one-role event registration policy | Shared | former duplicate declarations in ESP role startup common header | extracted to `taster_link_role_contract.h` |
| Taster-link transport/crypto/protocol and presentation payload contracts | Shared | existing `taster_link_*` contracts | retained shared; no opposite-role implementation is included by the extracted contract |

The fifth increment adds explicit, board-neutral startup ownership plans. They
are not a claim that a runnable independent target exists:

| Future role | Ordered startup boundary | Services exclusively required by that boundary |
| --- | --- | --- |
| Core | configuration → committed persistent ownership → sporting authority → Browser/remote control → customer-update authority | committed preferences, SD/history/records, detector input, Race Control/Engine/timing, SMUGLAND HTTP/SSE, customer update, Taster-link listener |
| Taster | configuration → local pairing/locator ownership → gated remote client → local presentation → touch-gated interaction | local presentation preferences, protected pairing/locator material, Taster-link client, display, GT911 touch and gated remote interaction |

The Core plan intentionally contains no display, touch, local presentation
preferences or Taster-side interaction. The Taster plan intentionally contains
no Race Engine/timing/detectors, Browser/SMUGLAND HTTP, SD/history/records or
committed Core preferences. The plans re-use the role manifest only; they do
not invent an alternate implementation of any current service.

The extraction is intentionally limited to the three genuinely shared,
board-neutral startup concepts above. Core and Taster startup headers now consume
that contract, while their ESP-specific radio/network/platform code remains
role-specific. No live service is replaced with a stub and no role is activated.

## Production-role diagnostic guard

`s4_production_role_guard.h` rejects a selected future production Core or
Taster compilation when it carries any of the mandatory forbidden definitions:

- `PP_PHASE8B_P1_TEST`
- `PP_WP7_ACCEPTANCE_SIMULATION`
- `PP_SIMULATED_RACING_TEST`
- `PP_UPDATE_TEST_KEY`
- `PP_WP7_PHYSICAL_RACING_TEST`

It also rejects the audited acceptance/demo/fixture paths present in the current
source: rollback acceptance, generic acceptance/verification, NVS diagnostic,
Wokwi Taster fixture, WP7 touch/READY checks, the Phase 15 SD fixture, and
Stages 2–14C demo/acceptance definitions. The same list is checked before the
top-level CMake file can reach ESP-IDF when a future Core/Taster profile is
given `PP_S4_PRODUCTION_ROLE_DEFINES`.

That CMake input has a deliberately narrow grammar: a semicolon-separated list
of `NAME` or `NAME=value`. `NAME` starts with an ASCII letter or underscore and
then contains only letters, digits or underscores. `value` is non-empty and
contains no whitespace, semicolon or further `=`. Empty list elements,
malformed names, blank/multiple values, literal semicolons (including escaped
or bracket-contained forms) and compiler-style `-DNAME` forms are rejected.
Matching is by `NAME`, so `PP_UPDATE_TEST_KEY`,
`PP_UPDATE_TEST_KEY=1` and `PP_UPDATE_TEST_KEY=0` are all rejected for a
production role. This is guard-input validation only: a later real role target
must validate its effective compiler definitions before role enablement.

The top-level role scaffold now applies the same grammar and forbidden-flag
check to all three future effective-definition inputs before it could enter
ESP-IDF: target-local `PP_S4_PRODUCTION_ROLE_DEFINES`, transitive component
`PP_S4_COMPONENT_COMPILE_DEFINES`, and global
`PP_S4_GLOBAL_COMPILE_DEFINES`. Each future profile also declares its required
role definition and a required forced inclusion of
`include/pp/s4_production_role_guard.h`; the non-linkable scaffold validates
that guard-coverage declaration.

This is deliberately *host-checkable wiring*, not evidence of effective target
compiler enforcement. A future approved role target must pass its actual
target-local, component-transitive and global compiler definitions to this
validation, define exactly its declared role macro, and force-include the guard
before any application header. It must be verified from the actual compiler
commands/dependency graph, not assumed from these profile lists.

These guards are active only when `PP_S4_PRODUCTION_CORE` or
`PP_S4_PRODUCTION_TASTER` is selected. The accepted integrated composition has
neither macro, so its existing diagnostic definitions remain valid and
unchanged. Core and Taster profiles remain non-linkable and fail closed.

Residual audit gap: `PP_DIAGNOSTICS` and `PP_WIFI_SCAN_INSTRUMENTATION` are
observational diagnostics rather than fixture/authority switches. This
increment does not make a production policy decision for them. Before a role is
enabled, that policy and any new test flag must be added to both the compile
guard and the configure-time guard.

## Missing facts and next safe slice

No approved Core board, detector electrical contract/pins, Core SD topology,
audio wiring, final partition budget or production provisioning facts exist.
No controlled component auto-discovery or Arduino transitive-dependency audit
has yet been performed. The next validation slice must establish both without
turning either profile linkable, and must inspect actual future compiler
commands for the guard and all three definition sources. A Windows
response-file/short-workspace candidate remains a separate target-validation
gate. `PP_DIAGNOSTICS` and `PP_WIFI_SCAN_INSTRUMENTATION` policy remains
unresolved as described above.
