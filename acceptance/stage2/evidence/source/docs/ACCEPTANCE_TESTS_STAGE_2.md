# P&P Stage 2 Acceptance Tests

**Status:** FROZEN — agreed before Stage 2 implementation/acceptance testing  
**Scope:** Prototype Stage 2 — one simulated Input Device  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `MESSAGE_CONTRACT.md` and `DESIGN_CONSTITUTION.md`.

## 1. Test governance

These acceptance tests are derived from the agreed P&P documentation. They do not redefine the product or architecture.

For each test, retained evidence must identify the exact source/build under test, exact acceptance-test specification commit, predetermined stimulus, predetermined expected result, captured actual result and PASS/FAIL.

The implementation under test must not alter these acceptance criteria to make itself pass. If a test exposes an ambiguity or contradiction in the agreed specification, preserve the evidence and return the issue for design resolution.

Testing must use the defined P&P architectural boundaries.

## 2. Stage 2 acceptance tests

### 2.1 One normal detection

**Stimulus:** Simulated detector transitions `INACTIVE → ACTIVE → INACTIVE`, satisfying the configured recognition rules.

**Expected:** Exactly one clean trigger results in exactly one standard physical `INPUT_EVENT`. The event contains the stable Input ID/capability identity and Relevant Time. It contains no ACTIVE/INACTIVE field and no race meaning.

### 2.2 Remains ACTIVE

**Stimulus:** `INACTIVE → ACTIVE`, then remain ACTIVE.

**Expected:** Exactly one trigger/event. Remaining ACTIVE does not repeatedly generate events.

### 2.3 Clearing and re-arming

**Stimulus:** `INACTIVE → ACTIVE → INACTIVE → ACTIVE → INACTIVE`, with both detections and clearing satisfying the Input Device's configured recognition/re-arm rules.

**Expected:** Exactly two clean triggers/events. Returning INACTIVE re-arms the Input Device internally. No INACTIVE/clear event is published merely to report re-arming.

### 2.4 Input noise/chatter

**Stimulus:** Source transitions/activity that do not satisfy the configured detection/filter/debounce/hysteresis rules, followed by a valid detection.

**Expected:** No `INPUT_EVENT` is produced by rejected activity. The subsequent valid detection produces exactly one normal `INPUT_EVENT`.

### 2.5 Re-arm noise/chatter

**Stimulus:** After a valid detection, provide activity that does not satisfy configured clearing/re-arm rules, then a valid clear/re-arm followed by another valid detection.

**Expected:** The Input Device does not prematurely re-arm or manufacture another trigger from rejected activity. Once genuinely re-armed, the next valid detection produces exactly one new event.

### 2.6 Relevant Time

**Stimulus:** Arrange a recognised clean trigger at a predetermined P&P System Time and deliberately allow subsequent processing/observation to occur later.

**Expected:** `INPUT_EVENT` Relevant Time identifies when the clean trigger occurred, not when a consumer later received or inspected it.

### 2.7 Stable identity

**Stimulus:** Produce several valid detections from the same simulated Input Device, then reboot/reinitialise as appropriate and repeat.

**Expected:** Every resulting event carries the same stable Input ID/capability identity. The identity does not become a temporary low-level address or arbitrary new identity after restart.

### 2.8 No race meaning

**Method:** Inspect every externally produced standard physical event.

**Expected:** No event contains or implies Lane, Start/Finish, sector, drag, speed-trap, MUG or other competition meaning. It identifies only the stable input/capability that produced the clean trigger and when.

### 2.9 Correct architectural injection point

**Method:** The acceptance harness stimulates the source-facing side of the Input Device.

**Expected:** The exercised path is simulated source → Input Device cleaning/recognition → Input Module → standard physical event. The acceptance test must not manufacture an `INPUT_EVENT` and inject it downstream, because that would bypass the Stage 2 behaviour under test.

### 2.10 Input Device/Input Module boundary

**Method:** Structural inspection/build-test against the accepted architecture.

**Expected:**
- source-specific handling is owned inside the Input Device;
- the Input Device remains a component of the Input Module rather than becoming a separate P&P Message Bus participant;
- the Input Module is responsible for publishing the clean standard physical event;
- no central translator is introduced that needs to understand the simulated device's native behaviour;
- no racing interpretation exists in either the Input Device or Input Module.

## 3. Acceptance-harness sanity test

### 2.T Deliberate failure

**Stimulus:** Deliberately supply one known incorrect expected result.

**Expected:** The harness reports FAIL and retains the mismatch. Restore the correct expectation afterwards.

## 4. Parameter discipline

The accepted architecture assigns detection, filtering, debounce/hysteresis, clearing and re-arming to the Input Device but does not prescribe particular parameter values for the Stage 2 prototype.

Acceptance testing therefore proves behaviour around the configured values; it must not invent a product requirement such as an arbitrary debounce duration merely to make the test concrete.

## 5. Stage gate

Stage 2 is accepted only when tests 2.1–2.10 pass and the acceptance mechanism has demonstrated 2.T.

A failed acceptance test is not automatically proof of an implementation defect. Where failure reveals a possible error or ambiguity in the agreed requirement/test, preserve the evidence and return the issue for design resolution rather than changing the frozen acceptance criterion.
