# P&P Stage 6 Acceptance Tests

**Status:** FROZEN — agreed after Stage 5 acceptance and before Stage 6 implementation/acceptance testing  
**Scope:** Prototype Stage 6 — Race Control lifecycle and scheduled GO  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `RACE_CONTROL_ENGINE_DESIGN.md`, `MESSAGE_CONTRACT.md`, `SYSTEM_LIFECYCLE.md`, `RACE_MODES_PRODUCT_SPEC.md` and the accepted Stage 5 baseline.

## 1. Purpose

Stage 6 implements the real Race Control boundary and proves the first complete Race Control/Race Engine lifecycle:

READY → STARTING → scheduled GO → RACING → FINISHED.

Race Control owns session operation/lifecycle and the authoritative scheduled GO. Race Engine retains ownership of competition interpretation, lap state/timing and normal competition completion. Communication between the two uses the common P&P Message Bus.

The first Stage 6 session remains deliberately narrow: one Race Entry, one lane, Lap Race, the accepted Start/Finish input and a small fixed lap target sufficient to prove normal completion. The minimum completion case uses Immediate finish; the other Lap Race finish behaviours and their multi-competitor consequences are not required by this stage.

The Stage 6 READY lifecycle state means that this prepared test session is ready to begin its start procedure. It must not create or imply one global system READY/NOT READY flag; governing P&P readiness remains capability- and activity-based.

Browser Requests and production START acceptance are Stage 8. Stage 6 may therefore use explicit preparation/test scaffolding to supply an already-fixed Session Definition and request commencement of the start procedure. Once that boundary is crossed, the start procedure, scheduled GO and lifecycle behaviour under test are real Race Control production behaviour. The fixture must not publish authoritative GO or become a competing Race Control implementation.

## 2. Stage 6 acceptance tests

### 6.1 Prepared session enters STARTING through Race Control

**Setup:** Supply the real Race Control with the fixed Stage 6 Session Definition using clearly identified Stage 6 preparation/test scaffolding.

**Stimulus:** Initiate the prepared session's start procedure through the Stage 6 boundary.

**Expected:** Race Control changes its authoritative lifecycle state from READY to STARTING. The fixture does not directly set STARTING as a substitute for Race Control behaviour.

### 6.2 Race Control publishes a future scheduled GO

**Stimulus:** Begin the start procedure.

**Expected:** Race Control publishes `GO_SCHEDULED` on the P&P Message Bus with an authoritative future GO time expressed in P&P System Time. The message denotes the exact future instant at which competition becomes active; it is not an immediate "GO now" command.

### 6.3 Race Control is the authoritative GO publisher

**Method:** Message Bus observation and structural inspection.

**Expected:** The real Race Control is the publisher of `GO_SCHEDULED`. Race Engine, Input Module and Stage 6 fixture do not originate authoritative GO.

### 6.4 Scheduling GO does not immediately enter RACING

**Stimulus:** Observe Race Control after `GO_SCHEDULED` has been published but before the scheduled GO instant.

**Expected:** Race Control remains STARTING. Merely publishing the future GO time does not make competition live early.

### 6.5 Scheduled GO instant enters RACING

**Stimulus:** Advance/observe P&P System Time to the authoritative scheduled GO instant.

**Expected:** Race Control changes its authoritative lifecycle state to RACING at that common instant.

### 6.6 Race Engine receives the real scheduled GO through the Message Bus

**Stimulus:** Run the real Stage 6 start procedure.

**Expected:** Race Engine consumes the authoritative `GO_SCHEDULED` publication through its permitted P&P Message Bus boundary and uses that GO as the timing origin required by the active Lap Race. The Stage 5 acceptance-only direct GO setup is not the production path used by this Stage 6 integration.

### 6.7 First completed lap is timed from the scheduled GO

**Setup:** Use the documented default first-crossing arrangement in which the car starts before its Start/Finish detector.

**Stimulus:** After RACING begins, produce the first valid Start/Finish crossing through the accepted source-facing Input Device path at a predetermined Relevant Time.

**Expected:** Race Engine completes Lap 1 and its authoritative lap time equals crossing Relevant Time minus the GO time scheduled by Race Control.

### 6.8 Race Control and Race Engine retain separate ownership

**Method:** Behavioural evidence and structural/code inspection.

**Expected:** Race Control owns READY/STARTING/RACING/FINISHED lifecycle state and authoritative GO scheduling. Race Engine owns lap count, lap timing, competition interpretation and competition-completion determination. Race Control does not directly modify Race Engine lap state; Race Engine does not own the session lifecycle.

### 6.9 RC↔RE communication uses the P&P Message Bus

**Method:** Structural inspection and Message Bus evidence.

**Expected:** Inter-component communication required for Stage 6, including scheduled GO and normal competition completion, uses the common P&P Message Bus. There is no privileged private Race Control/Race Engine shortcut introduced merely because both run inside the System Controller.

### 6.10 Normal competition completion originates in Race Engine

**Setup:** The fixed Stage 6 Session Definition contains the minimum fixed-lap Lap Race target and Immediate finish rule required for this test.

**Stimulus:** Produce valid Start/Finish crossings until the configured lap target is satisfied.

**Expected:** Race Engine determines that the active competition's completion rule has been satisfied. Race Control does not count laps or independently decide that the target has been reached.

### 6.11 Race Engine publishes COMPETITION_COMPLETE through the bus

**Stimulus:** Satisfy the fixed Stage 6 Lap Race completion condition.

**Expected:** Race Engine publishes one authoritative `COMPETITION_COMPLETE` fact according to the existing Message Contract. Its Relevant Time is the P&P System Time at which Race Engine determines that the competition completion rule has been satisfied. Race Control consumes the fact through the P&P Message Bus.

Winner/results/final positions remain Race Engine competition state and are not redundantly copied into the completion message.

### 6.12 Race Control enters FINISHED because competition completed

**Stimulus:** Race Control consumes the valid `COMPETITION_COMPLETE` fact for the active competition.

**Expected:** Race Control changes its authoritative lifecycle state from RACING to FINISHED and owns any session-lifecycle consequence. It does not manufacture a second competition completion or reach into Race Engine state to finish the race.

### 6.13 No production START Request/acceptance in Stage 6

**Method:** Structural inspection of the Stage 6 initiation path.

**Expected:** Stage 6 preparation/test scaffolding supplies the already-fixed Session Definition and initiates the start procedure without implementing Browser `START_REQUEST`, permission checking, Race Setup validation or production Session Definition creation. Those remain Stage 8 work.

The fixture must be clearly replaceable and must not become a permanent production shortcut around later Race Control START ownership.

### 6.14 No Browser dependency

**Method:** Run the complete Stage 6 lifecycle campaign without Browser/Presentation participation.

**Expected:** READY → STARTING → scheduled GO → RACING → FINISHED is achieved with real Race Control and Race Engine without a Browser connection.

### 6.15 Complete lifecycle ordering is observable

**Method:** Retain timestamped/state-transition and Message Bus evidence from one complete Stage 6 run.

**Expected:** Evidence demonstrates, in order:

1. prepared session READY;
2. Race Control enters STARTING;
3. Race Control publishes future `GO_SCHEDULED`;
4. before GO, lifecycle remains STARTING;
5. at scheduled GO, lifecycle becomes RACING;
6. Race Engine produces authoritative lap state/facts;
7. Race Engine publishes `COMPETITION_COMPLETE` when the fixed target is satisfied;
8. Race Control consumes completion and enters FINISHED.

The evidence must distinguish scheduled GO publication time from the future authoritative GO instant.

### 6.T Deliberate acceptance-harness failure

**Method:** Run one Stage 6 acceptance comparison with a deliberately wrong predetermined expectation, such as an incorrect GO instant, premature RACING state or incorrect final lifecycle state.

**Expected:** The harness reports FAIL and retains the mismatch as evidence. Restore the correct frozen expectation and confirm the corresponding acceptance check passes.

## 3. Minimum Stage 6 production scope

Stage 6 may extend the production Session Definition and Race Engine only with the minimum information/behaviour genuinely required to prove a one-lane fixed-lap Lap Race completing normally under Immediate finish.

Race Control is real production code and must own its lifecycle state and scheduled GO behaviour.

Race Engine remains real production code and gains the minimum completion-rule behaviour required to publish `COMPETITION_COMPLETE`. Do not move competition ownership into Race Control merely because Stage 6 is centred on Race Control.

Stage 5's acceptance-only GO setup may remain for Stage 5 regression testing, but it must not be the production integration path used to satisfy Stage 6.

Stage 6 does not implement Browser/Presentation, production START Request/acceptance, permission handling, full Race Setup validation, second-lane racing, the two other Lap Race finish behaviours, rapid-event stress testing, Pause/Resume, History/persistence or Outputs.

## 4. Evidence requirements

For every acceptance test retain evidence identifying:

- project/repository;
- exact firmware/source commit or otherwise unambiguous source snapshot under test;
- exact frozen Stage 6 acceptance-test commit;
- identifiable firmware build/hash where applicable;
- predetermined stimulus;
- predetermined expected result;
- captured actual result;
- PASS/FAIL result.

Retain raw Wokwi/serial/CLI output, Message Bus observations, lifecycle transitions and structural/code-review evidence where they substantiate a test. A final PASS summary alone is not sufficient evidence.

## 5. Test discipline

The frozen criteria must not be changed to make an implementation pass.

Ordinary implementation or test-harness defects may be corrected and affected tests rerun.

If a test exposes an ambiguity, contradiction or required product/architecture decision not answered by the governing documentation, Stage 6 acceptance stops and returns to the design side. The harness must not silently decide the missing specification.

The Stage 6 preparation fixture may provide the already-fixed Session Definition and initiate the start procedure because production START Request/acceptance is deliberately Stage 8. It may not publish authoritative GO, manipulate Race Engine lap state or directly force Race Control lifecycle transitions.

## 6. Stage gate

Stage 6 is accepted only when tests 6.1–6.15 pass and the acceptance mechanism has demonstrated 6.T.

Acceptance also requires production-code review against the governing P&P architecture before commit.

Passing Stage 6 does not authorise Stage 7 to begin until Stage 6 results and production code have been reviewed and accepted and the next stage has then been frozen/released under the agreed development process.
