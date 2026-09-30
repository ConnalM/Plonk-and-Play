# P&P Stage 4 Acceptance Tests

**Status:** FROZEN — agreed after Stage 3 acceptance and before Stage 4 implementation/acceptance testing  
**Scope:** Prototype Stage 4 — Session role assignment  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `MESSAGE_CONTRACT.md`, `SYSTEM_LIFECYCLE.md` and `DESIGN_CONSTITUTION.md`.

## 1. Purpose

Stage 4 proves the separation between stable physical input identity and the session-specific racing role assigned to that input.

The accepted Stage 2/3 path remains:

simulated source → Input Device → Input Module → P&P Message Bus → standard physical `INPUT_EVENT`

Stage 4 adds a minimal Session Definition in working RAM that can assign the stable input capability a session role such as Lane 1 Start/Finish. The Input Device and Input Module remain unaware of that role.

The governing architecture creates a production Session Definition when START is accepted by Race Control. Race Control is not implemented until Stage 6. Stage 4 therefore uses explicit test/session-preparation scaffolding to construct the minimal Session Definition required to prove its data semantics. This scaffolding must not masquerade as Race Control, START acceptance or a permanent alternative session-creation path.

Stage 4 does not implement Race Engine interpretation, lap counting, Race Control, browser behaviour or output operation.

## 2. Stage 4 acceptance tests

### 4.1 Minimal Session Definition creation

**Setup:** Use the accepted stable simulated input capability and a predetermined minimal session-role assignment of that capability to Lane 1 Start/Finish.

**Stimulus:** Construct the Stage 4 Session Definition through the explicit test/session-preparation fixture.

**Expected:** A fixed Session Definition exists in working RAM and contains the mapping from the stable input capability to Lane 1 Start/Finish. It is session data, not a modification of the Input Device or Input Module.

### 4.2 Physical INPUT_EVENT remains unchanged

**Stimulus:** Drive a valid detection from the accepted source-facing simulated detector through Input Device → Input Module → P&P Message Bus while the Stage 4 Session Definition exists.

**Expected:** The resulting `INPUT_EVENT` remains the standard physical event defined by the Message Contract: stable Input ID/capability plus original Relevant Time and generic message-envelope information only. It contains or implies no Lane, Start/Finish or other competition role.

### 4.3 Session role lookup

**Stimulus:** Use the stable Input ID/capability from the physical event to query/use the fixed Session Definition mapping.

**Expected:** The Session Definition resolves that stable capability to Lane 1 Start/Finish. The role comes from the Session Definition rather than the Input Module or Message Bus.

### 4.4 Assignment is frozen for the session

**Setup:** Create a Session Definition mapping the stable input to Lane 1 Start/Finish.

**Stimulus:** Alter the mutable/proposed working assignment after that Session Definition has been created.

**Expected:** The existing Session Definition remains unchanged and continues to map the stable input to Lane 1 Start/Finish. Later changes to mutable setup/configuration do not silently alter the active fixed session data.

### 4.5 A later session may reassign the same input

**Setup:** Retain evidence of the first fixed Session Definition, then end/discard that test session and change the proposed assignment for a subsequent session.

**Stimulus:** Construct a new Session Definition assigning the same stable input capability to a different legitimate role, using Lane 2 Start/Finish for this test.

**Expected:** The new Session Definition maps the same stable input capability to Lane 2 Start/Finish. The first Session Definition remains unchanged. The stable physical input identity and Input Module behaviour are unchanged between sessions.

### 4.6 Session meaning, not hardware implementation detail

**Method:** Inspect the minimal Session Definition structure and retained Stage 4 evidence.

**Expected:** The Session Definition contains the session-specific input role mapping and only other minimal session information needed by the Stage 4 structure. It does not contain hardware implementation details such as GPIO numbers, temporary transport/bus addresses, detector thresholds or Browser connections.

### 4.7 Session Definition is data, not an operational module

**Method:** Structural inspection/build-test against the accepted architecture.

**Expected:** The Session Definition is fixed working data in RAM. It is not implemented as an operational P&P module merely because other components will use it, and it does not publish, subscribe or become a P&P Message Bus participant.

### 4.8 Input Device/Input Module remain independent of session roles

**Method:** Exercise the physical input path under the two different Stage 4 session assignments and inspect the relevant production boundaries.

**Expected:** No Session Definition lookup, lane mapping or race-role interpretation occurs inside the Input Device or Input Module. Reassigning the stable capability between sessions requires no change to their production event path.

### 4.9 No Race Engine interpretation yet

**Stimulus:** Deliver a valid physical `INPUT_EVENT` while the Session Definition maps its stable input capability to Lane 1 Start/Finish.

**Expected:** Stage 4 can demonstrate the mapping but does not turn the crossing into a lap, sector completion, race result or other Race Engine fact. No Race Engine behaviour is introduced merely to prove role assignment.

### 4.10 No fake Race Control or START path

**Method:** Inspect the Stage 4 construction path and production boundaries.

**Expected:** The Session Definition is created only through clearly identified Stage 4 test/session-preparation scaffolding. Stage 4 does not implement a fake or competing production Race Control, START Request, START acceptance or lifecycle authority. The scaffolding is replaceable by the real Race Control creation path when that later stage is implemented.

### 4.T Deliberate acceptance-harness failure

**Method:** Run one acceptance comparison with a deliberately wrong predetermined role expectation, for example expecting Lane 2 Start/Finish from the first Session Definition.

**Expected:** The harness reports FAIL and retains the mismatch as evidence. Restore the correct frozen expectation and confirm the corresponding acceptance test passes.

## 3. Scope discipline

Stage 4 implements only the minimum Session Definition structure required to prove stable-input-to-session-role assignment and immutability.

The complete Session Definition v1 ultimately includes session identity, race rules, Race Entries, required input/output role assignments and relevant physical/rule parameters. Stage 4 need not implement full Race Entries, MUG/Car information, output roles or race-rule behaviour merely to anticipate later stages. Any minimal placeholder/session identity required for a coherent production data structure must remain consistent with the governing Session Definition contract and must not invent future product semantics.

## 4. Evidence requirements

For every acceptance test retain evidence identifying:

- project/repository;
- exact firmware/source commit or otherwise unambiguous source snapshot under test;
- exact frozen Stage 4 acceptance-test commit;
- identifiable firmware build/hash where applicable;
- predetermined stimulus;
- predetermined expected result;
- captured actual result;
- PASS/FAIL result.

Retain raw Wokwi/serial/CLI output and structural-review evidence where they substantiate a test. A final PASS summary alone is not sufficient evidence.

## 5. Test discipline

The frozen criteria must not be changed to make an implementation pass.

Ordinary implementation or test-harness defects may be corrected and affected tests rerun.

If a test exposes an ambiguity, contradiction or required product/architecture decision not answered by the governing documentation, Stage 4 acceptance stops and returns to the design side. The test harness must not silently decide the missing specification.

Acceptance tooling may construct the Session Definition through the explicit Stage 4 fixture because Race Control is deliberately outside this stage. It must not introduce a permanent production shortcut around the later Race Control ownership boundary.

## 6. Stage gate

Stage 4 is accepted only when tests 4.1–4.10 pass and the acceptance mechanism has demonstrated 4.T.

Passing Stage 4 does not authorise Stage 5 to begin until Stage 4 results have been reviewed and accepted and the next stage has then been frozen/released under the agreed development process.
