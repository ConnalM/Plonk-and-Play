# Session Configuration Architecture Review

**Review scope:** read-only review during the Stage 14C real-hardware checkpoint.  This document records the current production architecture and the smallest evidence-based recommendation.  It does not authorize or implement a refactor, change a frozen requirement, or alter the checkpoint.

## 1. Current architecture

P&P has two deliberately different data boundaries.

* **Proposed Race Setup / working configuration** describes the next session.  The configuration and identity specifications say that the SMUG edits Race Setup, that remembered/default setup is loaded into working RAM, and that P&P creates a fixed Session Definition only when START is accepted (`docs/CONFIGURATION_AND_IDENTITY_SPEC.md:185-240`, `docs/SYSTEM_LIFECYCLE.md:51-89,186-203`, `docs/SYSTEM_ARCHITECTURE.md:745-748`).
* **Session Definition** describes the accepted active session.  It is immutable working data, not persistent configuration, not a Message Bus participant, and not something a SMUG edits while the session is active (`docs/RACE_CONTROL_ENGINE_DESIGN.md:76-99`, `docs/SYSTEM_ARCHITECTURE.md:426-432,745-748`).

The responsibility boundary is also explicit. Race Control owns session operation and lifecycle; Race Engine owns competition interpretation, live competition state, rules, calculations, results and normal competition completion (`docs/RACE_CONTROL_ENGINE_DESIGN.md:12-37,121-149`; `docs/SYSTEM_ARCHITECTURE.md:484-496`). The Noticeboard is a read-only current-state view assembled from those owners; Browser and Taster consume it and issue authorised logical Requests (`docs/SYSTEM_ARCHITECTURE.md:438-450,672-706`).

The code reflects this architecture for the active session:

* `SessionMode` contains `LapRace`, `OpenPractice` and `Endurance` (`firmware/include/pp/core.h:15`).
* `ProposedRaceSetup` holds indexed entries, active-lane count, lap target, duration, finish policy, mode and start/false-start options (`firmware/include/pp/session_definition.h:15-36`). `valid()` checks mode-specific setup, capacity, mappings and uniqueness (`firmware/include/pp/session_definition.h:50-70`).
* `SessionDefinition` copies the validated setup into fixed indexed roles and entry definitions; `ActiveSessionDefinition::commit()` constructs the immutable active object (`firmware/include/pp/session_definition.h:73-119`).
* `RaceControlModule` receives a working setup pointer, owns lifecycle and accepts Start/Session-operation Requests (`firmware/include/pp/race_control.h:17-58,98-147`).
* `RaceEngineModule` is given the active definition and interprets stable Input Events through it. It owns lap timing, competition state, results, records and completion (`firmware/include/pp/race_engine.h:10-43,60-125`; `docs/RACE_CONTROL_ENGINE_DESIGN.md:41-58`).
* `Noticeboard::current()` combines Race Control and Race Engine authority into current State, including mode, duration, expiry, entries, laps, integrity and result fields (`firmware/include/pp/noticeboard.h:5-24`).

The common bus remains the architectural route for Requests, Events/Facts and NoticeboardChanged. Relevant-Time, not message arrival order or Browser presentation latency, governs timing (`docs/RACE_CONTROL_ENGINE_DESIGN.md:101-111`; `docs/SYSTEM_ARCHITECTURE.md:832-852,706-714`).

## 2. Actual production data and control flow

### Configuration and startup

At boot, Memory supplies working configuration. The current demo path builds a `ProposedRaceSetup`, assigns the two simulated detector roles/MUG IDs and active lanes, and gives that object to Race Control (`firmware/src/main.cpp:94-96,417-434`). The production architectural intention is the same: remembered/default setup is working information, not an active race (`docs/SYSTEM_LIFECYCLE.md:51-69`; `docs/CONFIGURATION_AND_IDENTITY_SPEC.md:119-127,234-240`).

### Browser authority and requests

Browser identity and the retained Master binding are Browser Interface state (`firmware/include/pp/browser_interface.h:10-16,42-49,120-136`). `/bootstrap` deliberately assigns authority; `/context` reports Race Director or Spectator (`firmware/include/pp/browser_interface.h:420-441`). Race Control still validates every Request; browser disabling is only presentation gating (`docs/SYSTEM_ARCHITECTURE.md:628-632,672-700`).

The Browser sends a START Request through `/request/start`. The route parses correlation ID, mode, duration and finish policy from the request body and calls `BrowserInterface::submitStart()` (`firmware/include/pp/browser_interface.h:399-418,443-461`). The Browser sends generic operation Requests for PAUSE, RESUME, END SESSION, Honour/Grid Restart and Race Again (`firmware/include/pp/browser_interface.h:463-474`).

### START and Session Definition creation

Race Control checks Race Director context, lifecycle and start reservation, copies the externally supplied setup, applies the mode from the Start Request, and for Endurance applies the duration and finish policy from that same Request. It validates the resulting copy, commits it into `ActiveSessionDefinition`, sets `definition_` to the committed object, and only then begins the start procedure (`firmware/include/pp/race_control.h:149-170`). This is the exact point where proposed data becomes an active immutable Session Definition.

This path differs by mode only in the values applied and the start procedure:

* **Lap Race:** the normal proposed setup supplies active entries, role mappings and lap target. The fixed definition drives the normal STARTING/GO/RACING lifecycle and configured finish behaviour.
* **Open Practice:** the accepted mode is fixed in the definition; Race Control begins active `RACING` without a normal countdown, and Race Engine applies Practice timing until generic END SESSION (`docs/STAGE_14B_DESIGN.md:35-48,50-79`; `firmware/include/pp/race_control.h:60-68`).
* **Endurance:** the accepted mode, duration and finish policy are fixed in the definition; the normal Lap start procedure runs and duration begins at authoritative GO (`docs/STAGE_14C_DESIGN.md:13-47`; `firmware/include/pp/race_control.h:40-42`).

After START, Race Control alone owns lifecycle boundaries. Race Engine receives the fixed definition and standard Input Events on the bus; it does not know Browser clients or detector technology (`docs/RACE_CONTROL_ENGINE_DESIGN.md:41-58,159-177`).

### Active operation, completion and termination

PAUSE is a Race Control operation with an authoritative P&P time and settlement fence. RESUME is accepted only after settlement; Endurance resumes remaining duration, while Practice uses its mode-specific timing rule (`firmware/include/pp/race_control.h:115-147`; `firmware/include/pp/race_engine.h:75-107`; `docs/RACE_CONTROL_ENGINE_DESIGN.md:101-111`).

Normal competition completion originates in Race Engine and is then reflected by Race Control as FINISHED. END RACE is a manual abandonment and must not manufacture a normal result (`docs/RACE_CONTROL_ENGINE_DESIGN.md:121-131`; `docs/STAGE_14C_DESIGN.md:35-47,92-112`). END SESSION is Practice-specific at the mode-contract level and returns the system toward READY without an official result (`docs/STAGE_14B_DESIGN.md:41-48,114-122`).

Race Again copies the accepted definition back into a proposed setup, permits replacement, returns READY, and leaves the next session to a fresh START (`firmware/include/pp/race_control.h:111-114,173-185`). The copy includes indexed entries and the accepted mode/duration/finish values.

## 3. Ownership table

| Information or action | Current authoritative owner | Browser role | Evidence |
|---|---|---|---|
| Remembered/default installation and race setup | Memory/working configuration and the responsibility that edits it | Presents/requests changes | `docs/SYSTEM_LIFECYCLE.md:51-89`; `docs/CONFIGURATION_AND_IDENTITY_SPEC.md:185-240` |
| Pending next-session entries, roles and lap target in this build | `ProposedRaceSetup` object supplied to Race Control; the working object is owned outside Race Control | Submits START; no direct mutation | `firmware/include/pp/session_definition.h:15-36`; `firmware/src/main.cpp:94-96,417-434`; `firmware/include/pp/race_control.h:23-26` |
| Pending mode/duration/Endurance finish in the current Browser path | Browser JavaScript variables until START (`practiceSelected`, `selected`, duration and finish controls); mode/duration/finish are carried in Start Request | Holds local proposal state and sends it at START | `firmware/include/pp/browser_interface.h:558-560`; `firmware/include/pp/race_control.h:149-160` |
| Master/Race Director identity | Browser Interface plus `AuthorityStore` | Authenticated request client | `firmware/include/pp/browser_interface.h:10-16,120-136`; `firmware/src/main.cpp:66-73` |
| Accepted active Session Definition | `ActiveSessionDefinition`, committed by Race Control | Reads its consequences | `firmware/include/pp/session_definition.h:111-119`; `firmware/include/pp/race_control.h:162-166` |
| Session lifecycle and pause/resume/stop | Race Control | Requests only | `docs/RACE_CONTROL_ENGINE_DESIGN.md:12-24`; `firmware/include/pp/race_control.h:31-50,111-147` |
| Competition interpretation, lap state, results and normal completion | Race Engine | Reads Noticeboard/Results/Details | `docs/RACE_CONTROL_ENGINE_DESIGN.md:24-37`; `firmware/include/pp/race_engine.h:33-43,103-125` |
| Current externally presentable state | Noticeboard assembled from owners | Polls/reconstructs | `docs/SYSTEM_ARCHITECTURE.md:438-450`; `firmware/include/pp/noticeboard.h:9-24` |
| Official results/history/records | Race Engine and persistence/record stores through defined boundaries | Reads API; never authors | `docs/SYSTEM_ARCHITECTURE.md:752-758`; `firmware/include/pp/browser_interface.h:341-365` |

The table shows a real asymmetry: active-session ownership is singular, but pending mode and some mode parameters are currently Browser-local in the production presentation path.

## 4. Proposed configuration versus active Session Definition

The intended distinction is sound: a READY setup is a proposal; accepted START creates immutable session data. The implementation currently represents the distinction only partially.

For entries, lane mappings and lap target, there is a genuine working `ProposedRaceSetup` referenced by Race Control (`firmware/include/pp/race_control.h:23-26`; `firmware/include/pp/session_definition.h:15-36`). `RaceControl::setProposedRaceSetup()` stores a pointer to that external working object; it does not copy or own the object. The current demo object is global working state (`firmware/src/main.cpp:94-96`).

For mode selection and Endurance parameters, the Stage 14B/14C presentation scripts keep local variables. Open Practice uses `practiceSelected`; Endurance uses `selected`, `minutes` and `finish`; the scripts add those values to the START request (`firmware/include/pp/browser_interface.h:558-560`). There is no production HTTP read/write route for a pending mode proposal. Consequently, mode/duration/finish are not a single server-owned pending Session Configuration before START.

The architecture documents are clear that the SMUG edits proposed Race Setup and P&P creates the Session Definition on START. They do not require Browser-local variables to be authoritative (`docs/CONFIGURATION_AND_IDENTITY_SPEC.md:234-240`; `docs/RACE_CONTROL_ENGINE_DESIGN.md:78-95`). The current split is therefore an implementation boundary weakness, not a product decision that the Browser should own configuration.

While `READY`, `Noticeboard::current()` reports `control_.mode()`. With no active definition, `RaceControl::mode()` returns the mode in `setup_`, otherwise `LapRace` (`firmware/include/pp/race_control.h:44-45`; `firmware/include/pp/noticeboard.h:14`). In the normal demo this is usually `LAP_RACE` because the proposed setup defaults to Lap Race. That value means the current pending/default setup is Lap Race in this implementation; it does **not** prove that a Browser-local Endurance or Practice proposal has been committed, because no active Session Definition exists yet. It must not be treated as an authoritative rejection of an uncommitted Browser proposal.

## 5. Browser reconstruction behaviour

### A. Configuring the next session in READY

The common Browser poll obtains `/noticeboard`, `/context` and `/state` and renders current State (`firmware/include/pp/browser_interface.h:550-551`). The base page is deliberately a consumer and does not calculate authoritative race state. Stage 14B and Stage 14C scripts add local proposal flags and controls (`firmware/include/pp/browser_interface.h:558-560`). The current reconciliation preserves those flags while lifecycle is READY and clears/overwrites them when an active non-READY authoritative mode is visible. This prevents the earlier bug where READY's default Lap Race state erased a legitimate pre-START Endurance proposal, but it also confirms that a READY reload cannot reconstruct an uncommitted local proposal from P&P.

Therefore a Browser refresh or a second Browser joining while READY sees the server's current proposed/default information and its own fresh local UI defaults, not a canonical mode/duration/finish proposal. Two clients can disagree before START.

### B. STARTING/RACING/PAUSED

Once START commits the definition, `/state` and Noticeboard values are authoritative. The Browser reconstructs lifecycle, `sessionMode`, entries, laps, duration, expiry, finish behaviour, pause/restart information, integrity and validity from the current snapshot. Reconnect does not replay missed facts; it obtains current State as required by the architecture (`docs/SYSTEM_ARCHITECTURE.md:688-704`). Active race timing continues independently of Browser presence (`docs/SYSTEM_ARCHITECTURE.md:624-626,706`; `docs/STAGE_14B_DESIGN.md:21-33`; `docs/STAGE_14C_DESIGN.md:24-33`).

### C. FINISHED

The Browser reads authoritative Results, Details and History routes. The Browser formats those views but does not calculate ranking, lap timing, official result or persistence (`firmware/include/pp/browser_interface.h:341-365`; `docs/BROWSER_FLOW_RESULTS_HISTORY_SPEC.md:104-153`).

### D. Return to READY

Practice END SESSION clears the active definition and returns Race Control to READY (`firmware/include/pp/race_control.h:123-127`). END RACE/Restart Race have their distinct abandonment/restart semantics (`firmware/include/pp/race_control.h:129-136`; `docs/RACE_CONTROL_ENGINE_DESIGN.md:127-133`). Race Again copies the accepted definition into the next proposed setup (`firmware/include/pp/race_control.h:111-114,173-185`). A clean reset may intentionally clear the active definition and, depending on fixture/development path, authority and setup. None of these active-state paths should be confused with a Browser-local pre-START proposal.

## 6. Test-boundary analysis

The current evidence has strong lower-boundary coverage but incomplete end-to-end proposal coverage.

**Covered production boundaries:**

* Race Control and Race Engine probes exercise Message Bus Requests/Events, immutable Session Definitions, Relevant-Time ordering, pause settlement, mode-specific rules, results and persistence.
* Browser serializer and route checks exercise production `/state`, `/results`, `/details`, `/history`, `/records`, Facts and Noticeboard formatting (`firmware/include/pp/browser_interface.h:148-155,320-365`).
* Stage 14C acceptance includes exact embedded Browser presentation extraction and the runtime proposal test (`acceptance/stage14c/browser_proposal_runtime.js`), plus static/source and merged-binary review in `acceptance/stage14c/browser_integration_review.py` and `acceptance/stage14c/run.ps1`.
* Earlier HTTP acceptance scripts exercise real route submission and response handling, but they do not cover the current READY proposal/reconciliation path as a complete mode-selection-to-Session-Definition flow.

**Below or beside the production Browser boundary:**

* `firmware/tests/stage14c_acceptance_probe.inc` calls `BrowserInterface::submitStart()` directly with Endurance parameters. That proves Browser Interface to Race Control and Session Definition handling, but bypasses the embedded HTML/JavaScript, actual HTTP parsing and Browser polling/reconciliation.
* The Stage 14B/14C runtime script executes the extracted production scripts under a fake DOM/fetch. It is valuable exact-path presentation coverage, but it is not an ESP32 HTTP round trip and cannot expose every server/client ordering issue.
* Static string checks and Wokwi acceptance can prove the expected asset is compiled and the lower boundary works, but a passing direct probe can coexist with a real Browser proposal bug. The recent human checkpoint demonstrated this gap.

The missing automated proof is a production HTTP integration sequence that performs, against the same embedded route and state endpoints: READY → authority acquisition → mode selection → polling/reconciliation → START → authoritative Session Definition/State. That gap explains why a direct `submitStart()` acceptance could pass while a real Browser mode proposal was lost or rendered inconsistently.

## 7. Recent Stage 14 problems classified

The observed issues are related in symptoms but not all the same defect.

* **Open Practice/Endurance both appearing selected:** an ordinary presentation-state bug caused by independent Browser-local flags and reconciliation order. The intended product choice is one mode proposal; the active Session Definition already has one `SessionMode` value. `firmware/include/pp/browser_interface.h:558-560` shows the separate local flags and their cross-clearing functions.
* **Authority gating:** the core authority model is correct. `bootstrap`, `context`, Browser owner tokens and Race Control context checks are the security/authority path (`firmware/include/pp/browser_interface.h:120-136,420-451`; `firmware/include/pp/race_control.h:149-161`). A control that is visually enabled too early is a presentation defect, not permission to accept an unauthorised Request.
* **Endurance disappearing during polling:** a genuine ownership/reconciliation mistake when READY's default `LAP_RACE` was interpreted as an authoritative rejection of an uncommitted Browser proposal. The current guard that preserves proposals while lifecycle is READY is the smallest presentation fix, but does not create a canonical server-side pending proposal.
* **START creating LAP_RACE after Endurance selection:** this was the consequence of the same missing/unstable proposal boundary: the Browser request was either sent with default mode or its local selection was cleared before START. Race Control then correctly validated and committed the request it received. Race Control itself did not incorrectly reinterpret an accepted active Endurance definition.
* **Active-session reconstruction:** the architecture and current `/state` path are correct once START has committed the definition. Active `sessionMode`, duration, finish policy and entry data are reconstructed from authoritative State; this is distinct from READY proposal reconstruction.
* **Stale DOM/reconnect behaviour:** Browser DOM can hold local proposal flags until a poll or reload. A reconnect obtains authoritative active State, as required, but a pending local READY proposal has never been committed and therefore cannot be reconstructed from active State. This is an ownership/contract ambiguity, not an active Race Engine defect.
* **Detector and timing faults:** the earlier Stage 14A/14C detector and timing findings were at the demo fixture or acceptance-boundary level. They do not show that Race Engine authority moved into Browser. The normal path remains Input Module → Message Bus → Race Engine.

## 8. Future architectural pressure

The core architecture has a clean place for future active-session meaning: new mode-specific fields can be validated in proposed setup, frozen into Session Definition, interpreted by Race Engine, and exposed through Noticeboard/Results. The product specifications already describe Practice, Endurance, Timed Stage and Drag as mode/rule sets rather than separate timing engines (`docs/RACE_CONTROL_ENGINE_DESIGN.md:135-149`; `docs/RACE_MODES_PRODUCT_SPEC.md:7-17,87-111,111-170,172-269,270-421`).

The pressure point is before START. Rally/Timed Stage/Drag, pit stops, mandatory stops, fuel, Stop/Go penalties and additional setup options will add more fields. If each field is held in a separate Browser-local variable and appended to START bodies, every mode increases the chance of divergent clients, lost refresh state, stale DOM and mode-specific parsing. A server-owned pending Race Setup would give all of those options one validated home without changing active-session authority.

The Taster is intentionally narrower. It retains a simple two-lane LAPS/TIMED surface and defaults, while expanded installations use the full Browser (`docs/PP_TASTER_SPEC.md:34-77,104-128`). That should remain a presentation/product boundary rather than forcing Browser proposal state into Taster internals.

## 9. Risks

1. **Pending-proposal ownership is ambiguous.** Entries/lap target are in a working object referenced by Race Control, while mode/duration/finish can exist only in one Browser's JavaScript. This is the largest architectural risk.
2. **Multi-client disagreement.** Browser A can select Endurance while Browser B connects in READY and sees default Lap Race/neutral controls. Either client may then submit START with a different proposal. The existing architecture permits multiple MUGs and one Master authority, but it does not define a canonical pending-proposal readback (`docs/SYSTEM_ARCHITECTURE.md:650-662`).
3. **Refresh loss before START.** A Browser reload loses local mode and Endurance parameters because no authoritative pending-proposal route exists. This is acceptable only if the product deliberately treats those controls as purely local until START; the configuration documents instead describe Race Setup as product working data.
4. **Repeated option growth.** More mode fields will enlarge request parsing and duplicate validation unless proposal editing is centralized.
5. **False confidence from lower-boundary tests.** Direct BrowserInterface probes and static embedded-script checks do not prove a real Browser HTTP/reconciliation sequence. The recent hardware checkpoint is evidence of this test-boundary risk.
6. **Do not confuse active-session recovery with pending setup.** Active race persistence/recovery is explicitly not required by the current architecture (`docs/SYSTEM_ARCHITECTURE.md:907-909`; `docs/STAGE_14B_DESIGN.md:120-122`; `docs/STAGE_14C_DESIGN.md:108-110`). A pending setup snapshot is a separate, much smaller concern.

## 10. Recommendation

**Judgement: B — the architecture is basically sound, but a small boundary/ownership cleanup should be done before adding more modes.**

The strongest evidence is that the difficult authority boundary already works: Race Control validates and commits one immutable Session Definition (`firmware/include/pp/race_control.h:149-170`), Race Engine interprets normal Message Bus Input Events through it (`firmware/include/pp/race_engine.h:33-43`; `docs/RACE_CONTROL_ENGINE_DESIGN.md:41-58`), and Noticeboard/Browser reconstruct active state without owning timing (`docs/SYSTEM_ARCHITECTURE.md:438-450,688-706`). The failures occurred at the pre-START Browser proposal boundary, where local mode flags and parameters were mistaken for, or overwritten by, active State.

The smallest conceptual correction is to make one pending `ProposedRaceSetup`/working Race Setup authoritative for the next session. An authorised setup-edit boundary should update mode, duration, finish policy and other proposed fields; a read-only snapshot should let every Browser reconstruct the proposal while READY. START should validate and commit that one object. Existing active-session ownership, generic operations, Message Bus, Relevant-Time rules, immutable Session Definition, Race Engine separation and Browser non-authority should remain unchanged. A transition may preserve the current Start Request fields for compatibility, but they should normalize into the single pending setup rather than remain an independent Browser-only authority.

This is a conceptual recommendation only. No implementation is included in this review.

### Concise report

* **A/B/C judgement:** **B**.
* **Strongest evidence:** active Race Control → immutable Session Definition → Race Engine → Noticeboard boundaries are correct; the real failures are READY proposal ownership/reconciliation and incomplete end-to-end Browser tests.
* **Current owner of proposed configuration:** entries/lane mappings/lap target are a working `ProposedRaceSetup` object supplied to Race Control; mode/duration/Endurance finish are currently Browser-local until START, then carried in `StartRequest`.
* **Current owner of active Session Definition:** `ActiveSessionDefinition`, committed by Race Control and consumed immutably by Race Control and Race Engine.
* **Single biggest architectural risk:** no canonical server-side pending proposal for mode and mode-specific parameters, allowing Browser clients/reloads to disagree before START.
* **Smallest recommended corrective action:** centralize the pending next-session proposal and expose an authorised edit plus readback boundary; have START validate/commit that proposal.
* **Timing recommendation:** do this **before Stage 14D** if Stage 14D adds another mode or materially more setup options. If Stage 14D is only a narrowly scoped slice that adds no new proposal fields, it can be deferred, but every new mode-specific Browser-local option increases the risk.

## What should not change

The review finds no reason to change the following frozen principles:

* Race Control remains the owner of session operation/lifecycle; Race Engine remains the owner of competition interpretation, rules, calculations and normal completion.
* The active Session Definition remains immutable for the session and is not edited by Browser/Taster during PAUSE or RESUME.
* Input Module events remain role-neutral; Session Definition supplies session-specific meaning.
* Message Bus and Noticeboard boundaries remain the normal communication path.
* Browser remains a presentation/request client, never an alternative Race Engine or timing authority.
* Relevant-Time ordering, settlement fences, authoritative GO and browser-independent timing remain unchanged.
* Taster's simple documented LAPS/TIMED behaviour remains separate from the full Browser configuration surface.
* Results, History, PB and Track Record remain owned by their existing authoritative/persistence paths; Browser only reconstructs and presents them.
* No active-race crash-recovery or persistent live-session reconstruction should be introduced as part of this proposal.

