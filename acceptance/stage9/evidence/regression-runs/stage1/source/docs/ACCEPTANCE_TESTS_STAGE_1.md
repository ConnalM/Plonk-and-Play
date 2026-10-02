# P&P Stage 1 Acceptance Tests

**Status:** FROZEN — agreed before retrospective Stage 1 acceptance testing  
**Scope:** Prototype Stage 1 — ESP32 skeleton  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `SYSTEM_LIFECYCLE.md`, `DESIGN_CONSTITUTION.md`, `MESSAGE_CONTRACT.md` and `DEVELOPMENT_WORKFLOW.md`.

## 1. Test governance

These acceptance tests are derived from the agreed P&P documentation. They do not redefine the product or architecture.

For each test, retained evidence must identify:

- project: Plonk & Play;
- repository: `ConnalM/Plonk-and-Play`;
- exact firmware/source commit and build under test;
- exact acceptance-test specification commit;
- predetermined stimulus;
- predetermined expected result;
- captured actual result;
- PASS or FAIL.

A prose assertion that a test passed is not sufficient where raw or independently captured evidence can reasonably be retained.

The implementation under test must not alter these acceptance criteria to make itself pass. Builder/unit tests may be added separately.

If an acceptance test exposes an ambiguity or contradiction in the agreed specification, the implementation must not invent the product decision. Report the expected and observed behaviour and stop for design resolution.

Testing must use the defined P&P architectural boundaries. Test-only shortcuts must not become alternative production paths.

## 2. Stage 1 acceptance tests

### 1.1 Clean boot

**Stimulus:** Start the Stage 1 firmware from a clean/reset ESP32.

**Expected:** P&P initialises successfully and reaches its Stage 1 operational state. No race behaviour starts.

### 1.2 P&P System Time

**Stimulus:** Observe/read P&P System Time at known intervals during operation.

**Expected:** P&P System Time is available, monotonic and advances correctly; it does not go backwards. It is the System Controller's P&P time source rather than a diagnostic-only clock.

### 1.3 First-start Memory and configuration

**Stimulus:** Start with no valid remembered customer configuration.

**Expected:** Memory initialises normally and appropriate factory/default configuration is loaded into working RAM. Absence of saved configuration is a normal first-start condition, not a fault.

### 1.4 Remembered configuration

**Stimulus:** Save a deliberately identifiable valid configuration, restart P&P.

**Expected:** The remembered value is recovered through Memory and loaded into working RAM rather than silently reverting to the factory value.

### 1.5 Invalid/corrupt persistent data

**Stimulus:** Present deliberately invalid/corrupt saved configuration and restart.

**Expected:** P&P does not use corrupt information as valid configuration. It recovers safely according to the existing persistence design and reaches a valid working configuration.

### 1.6 Working-RAM independence

**Stimulus:** Complete startup, then exercise/read working configuration during normal operation.

**Expected:** Normal configuration use comes from working RAM. Persistent Memory is not repeatedly consulted as the ordinary operational source.

### 1.7 Message Bus — permitted delivery

**Stimulus:** Have an authorised test publisher send a known test message to an authorised subscriber through the P&P Message Bus.

**Expected:** The authorised subscriber receives exactly the permitted message through the real P&P Message Bus. No private point-to-point inter-component path is used.

### 1.8 Message Bus — authority

**Stimulus:** Attempt publication of a Message Type from a component not permitted to publish that type.

**Expected:** The unauthorised publication is rejected/not delivered. Attachment to the bus grants connectivity, not authority.

### 1.9 Message Bus — subscription

**Stimulus:** Publish a known message with both subscribed and non-subscribed test participants present.

**Expected:** The appropriate subscriber receives it. An unrelated/non-subscribed participant does not receive it merely because it is connected to the bus.

### 1.10 Message Bus self-test

**Stimulus:** Run the Stage 1 Message Bus self-test.

**Expected:** The intended bus behaviours are genuinely exercised and their actual results are reported. Evidence must show the exercised behaviour/results rather than only a final `[BUS SELF-TEST] PASS` assertion.

### 1.11 Required development diagnostics

**Stimulus:** Perform normal startup with controlled development diagnostics enabled.

**Expected:** Serial diagnostics visibly report initialisation/readiness of P&P System Time, Memory, working configuration and the P&P Message Bus, plus the Message Bus self-test result.

### 1.12 Diagnostics are non-essential

**Stimulus:** Run the same Stage 1 firmware with development diagnostics disabled.

**Expected:** P&P still boots and operates correctly at Stage 1. Diagnostics are not required for correct operation and do not become authoritative P&P State.

### 1.13 Architectural boundaries

**Method:** Inspect/build-test the Stage 1 implementation against the accepted architecture.

**Expected:** P&P System Time remains a shared service; configuration remains information; Memory owns persistence; normal inter-component communication uses the P&P Message Bus; Stage 1 has not created competing authoritative ownership, inappropriate operational modules, or hidden private inter-component production paths.

This test is deliberately partly structural inspection because these architectural properties cannot all be proven from external serial output alone.

## 3. Acceptance-harness sanity test

### 1.T Deliberate failure

**Stimulus:** Deliberately supply the acceptance harness with a known incorrect expected value/result.

**Expected:** The harness reports FAIL and retains the mismatch. Restore the correct expectation after this sanity test.

This tests the acceptance mechanism, not P&P product behaviour.

## 4. Stage gate

Stage 1 is accepted only when tests 1.1–1.13 pass and the acceptance mechanism has demonstrated 1.T.

A failed acceptance test is not automatically proof of an implementation defect. Where failure reveals a possible error or ambiguity in the agreed requirement/test, preserve the evidence and return the issue for design resolution rather than changing the frozen acceptance criterion.
