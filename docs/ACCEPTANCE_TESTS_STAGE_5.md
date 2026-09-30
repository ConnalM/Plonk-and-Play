# P&P Stage 5 Acceptance Tests

**Status:** FROZEN — agreed after Stage 4 acceptance and before Stage 5 implementation/acceptance testing  
**Scope:** Prototype Stage 5 — Race Engine lap interpretation  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `RACE_CONTROL_ENGINE_DESIGN.md`, `MESSAGE_CONTRACT.md`, `RACE_MODES_PRODUCT_SPEC.md` and `DESIGN_CONSTITUTION.md`.

## 1. Purpose

Stage 5 implements the real Race Engine boundary required to interpret bus-delivered physical `INPUT_EVENT` messages through a fixed Session Definition and produce authoritative Lap Race lap counts, lap times and `LAP_COMPLETED` facts without requiring a browser.

The exercised path is:

simulated source → Input Device → Input Module → P&P Message Bus → Race Engine + fixed Session Definition → authoritative competition state / `LAP_COMPLETED`

The first Stage 5 test session is deliberately narrow: one Race Entry, one lane, Lap Race, the accepted stable input assigned as Lane 1 Start/Finish, and the documented default first-crossing arrangement in which the car starts before the Start/Finish detector. Therefore the first valid post-GO Start/Finish crossing completes Lap 1 and its lap time is measured from GO.

Race Control and its production start procedure are not implemented until Stage 6. Stage 5 may therefore use explicit test scaffolding to establish the predetermined GO/start condition required to test Race Engine interpretation. This scaffolding must not masquerade as Race Control or become a competing production lifecycle path.

Competition-completion behaviour is deliberately outside this Stage 5 milestone. Stage 5 proves lap interpretation and authoritative lap facts. It does not implement the later Race Control lifecycle or the three Lap Race finish behaviours merely to anticipate them.

## 2. Stage 5 acceptance tests

### 5.1 Race Engine consumes bus-delivered INPUT_EVENT

**Stimulus:** Drive one valid post-GO crossing from the accepted simulated detector's source-facing side.

**Expected:** The clean physical event follows the accepted Input Device → Input Module → P&P Message Bus path and is consumed by the real Race Engine through its permitted Message Bus subscription. The acceptance harness does not bypass the path by directly calling Race Engine with a manufactured physical event for this end-to-end test.

### 5.2 Session role interpretation belongs to Race Engine

**Stimulus:** Deliver the physical event for the stable input assigned by the fixed Session Definition as Lane 1 Start/Finish.

**Expected:** Race Engine obtains Lane 1 Start/Finish meaning from the fixed Session Definition and interprets the event accordingly. The Input Device, Input Module and `INPUT_EVENT` remain free of lane/race-role meaning.

### 5.3 First post-GO crossing completes Lap 1

**Setup:** The fixed test Session Definition uses the documented default arrangement: the car starts before the Start/Finish detector. Establish a predetermined GO time through Stage 5 test scaffolding.

**Stimulus:** Produce the first valid Start/Finish crossing after GO.

**Expected:** Race Engine authoritative competition state records Lap 1 complete for the test Race Entry.

### 5.4 Lap 1 time is crossing Relevant Time minus GO

**Setup:** Predetermine GO and first-crossing P&P System Times.

**Stimulus:** Produce the first valid post-GO Start/Finish crossing and observe/process it after any deliberate harness delay required by the test.

**Expected:** Lap 1 time equals the crossing's `INPUT_EVENT` Relevant Time minus GO time. Consumer arrival, polling, diagnostic or observation time does not redefine the official lap time.

### 5.5 Subsequent crossing completes and times Lap 2

**Stimulus:** After Lap 1, produce a second valid Start/Finish crossing at a predetermined Relevant Time.

**Expected:** Race Engine records Lap 2 complete. Lap 2 time equals crossing 2 Relevant Time minus crossing 1 Relevant Time.

### 5.6 Several laps accumulate correctly

**Stimulus:** Produce a predetermined sequence of valid Start/Finish crossings at known Relevant Times.

**Expected:** Authoritative Race Engine state contains the expected completed-lap count and corresponding individual lap times derived from the event Relevant Times and the documented first-crossing behaviour.

### 5.7 LAP_COMPLETED contract

**Stimulus:** Complete a lap through the real Stage 5 path and observe the resulting P&P Message Bus publication.

**Expected:** Race Engine publishes one authoritative `LAP_COMPLETED` fact for that completed lap. It contains the Race Entry ID, lap number, lap time and Relevant Time at which the lap was completed, consistent with the frozen Session Definition and Message Contract. It does not redundantly copy Lane, MUG identity, Car identity or other information that belongs to the Race Entry/Session Definition.

### 5.8 Irrelevant physical input does not alter lap state

**Stimulus:** Deliver an authorised standard `INPUT_EVENT` whose stable Input ID/capability is not assigned a relevant Start/Finish role in the active Session Definition.

**Expected:** Race Engine does not increment the test Race Entry's lap count, create a lap time or publish `LAP_COMPLETED` as a consequence of that irrelevant input.

For this downstream Race Engine test, the accepted architecture permits test tooling to impersonate the Input Module and publish an authorised standard `INPUT_EVENT` where the physical sensing boundary is not itself under test.

### 5.9 One incoming competition event changes authoritative state once

**Stimulus:** Present the same identifiable incoming competition event more than once in a way that exercises duplicate handling.

**Expected:** Race Engine authoritative competition state changes once only. The duplicate must not create a second lap completion or second authoritative consequence.

The implementation mechanism used to identify/handle duplicates is deliberately not prescribed by this test beyond what the governing message architecture requires.

### 5.10 Communication/observation delay does not determine official timing

**Stimulus:** Arrange for a valid Start/Finish event to have a predetermined Relevant Time and be consumed/observed later.

**Expected:** Official lap count and lap time are determined from the event's Relevant Time and active Session Definition, not from Message Bus arrival order, diagnostic output time or test observation time.

This test does not invent a general transport latency or buffering requirement beyond the accepted architecture.

### 5.11 Race Engine owns competition state

**Method:** Structural inspection plus behavioural evidence.

**Expected:** Completed-lap count and lap timing state are owned within the Race Engine competition boundary. They are not owned by the Input Module, Session Definition, diagnostics, test fixture or a pretend Race Control. Session Definition supplies fixed meaning/rules; it does not become mutable competition state.

### 5.12 No browser dependency

**Method:** Run the Stage 5 acceptance campaign without Browser/Presentation participation.

**Expected:** Race Engine consumes the relevant physical events, maintains authoritative lap state and publishes required lap facts without any browser connection or presentation client.

### 5.13 No Race Control implementation in Stage 5

**Method:** Inspect the Stage 5 start-condition/test path and production boundaries.

**Expected:** The predetermined GO/start condition used by Stage 5 is clearly identified test scaffolding. Stage 5 does not implement a fake or competing production Race Control, START Request/acceptance, start sequence or authoritative `GO_SCHEDULED` publisher. The real Race Control lifecycle remains Stage 6 work.

### 5.T Deliberate acceptance-harness failure

**Method:** Run one acceptance comparison with a deliberately wrong predetermined expectation, such as an incorrect expected lap count or lap time.

**Expected:** The harness reports FAIL and retains the mismatch as evidence. Restore the correct frozen expectation and confirm the corresponding acceptance test passes.

## 3. Minimum Stage 5 production scope

Stage 5 may extend the production Session Definition only with the minimum information genuinely required by the real Race Engine for this one-lane Lap Race, including a stable Race Entry identity and the applicable Lap Race/first-crossing information required by the governing documents.

Do not add speculative Session Definition fields merely because later product modes will eventually require them.

Race Engine is a real production responsibility/module. Its competition state and interpretation logic must not be placed in the Input Module, Session Definition, diagnostics or acceptance fixture.

Stage 5 does not implement competition completion, the three Lap Race finish behaviours, Race Control lifecycle, browser behaviour, multiple lanes, rapid-event stress testing, Pause/Resume, History/persistence or Outputs.

## 4. Evidence requirements

For every acceptance test retain evidence identifying:

- project/repository;
- exact firmware/source commit or otherwise unambiguous source snapshot under test;
- exact frozen Stage 5 acceptance-test commit;
- identifiable firmware build/hash where applicable;
- predetermined stimulus;
- predetermined expected result;
- captured actual result;
- PASS/FAIL result.

Retain raw Wokwi/serial/CLI output, Message Bus observations and structural/code-review evidence where they substantiate a test. A final PASS summary alone is not sufficient evidence.

## 5. Test discipline

The frozen criteria must not be changed to make an implementation pass.

Ordinary implementation or test-harness defects may be corrected and affected tests rerun.

If a test exposes an ambiguity, contradiction or required product/architecture decision not answered by the governing documentation, Stage 5 acceptance stops and returns to the design side. The test harness must not silently decide the missing specification.

Stage 5 test scaffolding may establish the predetermined GO/start condition because Race Control is deliberately outside this stage. It must not become a permanent production shortcut around Race Control ownership.

## 6. Stage gate

Stage 5 is accepted only when tests 5.1–5.13 pass and the acceptance mechanism has demonstrated 5.T.

Acceptance also requires production-code review against the governing P&P architecture before commit.

Passing Stage 5 does not authorise Stage 6 to begin until Stage 5 results and production code have been reviewed and accepted and the next stage has then been frozen/released under the agreed development process.
