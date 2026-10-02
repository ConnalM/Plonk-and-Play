# P&P Stage 3 Acceptance Tests

**Status:** FROZEN — agreed after Stage 2 acceptance and before Stage 3 implementation/acceptance testing  
**Scope:** Prototype Stage 3 — P&P Message Bus delivery  
**Governing specifications:** The current accepted P&P documentation on `main`, especially `FIRST_IMPLEMENTATION_BEHAVIOUR.md`, `SYSTEM_ARCHITECTURE.md`, `MESSAGE_CONTRACT.md` and `DESIGN_CONSTITUTION.md`.

## 1. Purpose

Stage 3 proves that the genuine clean physical `INPUT_EVENT` produced through the accepted Stage 2 path crosses a real P&P component boundary through the common P&P Message Bus and is delivered to permitted subscribers without a private point-to-point path.

The path under test is:

simulated source → Input Device → Input Module → P&P Message Bus → authorised Test/Diagnostics consumer

Stage 3 does not assign race meaning, create a Session Definition, implement Race Engine or Race Control behaviour, or test browser behaviour.

Stage 3 is not the rapid-event/load campaign. The accepted Message Contract requires race-critical events not to silently disappear where loss would change authoritative competition behaviour, but Stage 3 does not invent delivery/retry/acknowledgement mechanisms that the governing specifications deliberately leave as implementation detail. Rapid and closely spaced input testing belongs to the later implementation stage defined for that purpose.

## 2. Stage 3 acceptance tests

### 3.1 End-to-end permitted delivery

**Stimulus:** Drive the accepted Stage 2 simulated detector from its source-facing side through one valid detection satisfying its configured recognition rules.

**Expected:** Exactly one clean standard `INPUT_EVENT` follows the real path Input Device → Input Module → P&P Message Bus → authorised Test/Diagnostics consumer. The consumer receives exactly one event. The acceptance harness must not manufacture an `INPUT_EVENT` downstream of the Input Module for this test.

### 3.2 Payload preservation

**Stimulus:** Produce one valid detection using a predetermined stable Input ID/capability and capture the Relevant Time assigned to the clean event at the Input Module boundary.

**Expected:** The authorised consumer receives the same stable Input ID/capability and the same Relevant Time. Message Bus delivery does not reinterpret, replace or derive either value from consumer receipt time.

### 3.3 Source preservation

**Stimulus:** Produce one valid source-facing detection and inspect the P&P message received by the authorised consumer.

**Expected:** The P&P Message Source identifies the Input Module as the publisher/originating P&P component. The simulated Input Device remains a component of the Input Module and does not become a separate P&P Message Bus participant.

### 3.4 Subscription controls ordinary delivery

**Setup:** Authorised Test/Diagnostics consumer A subscribes to `INPUT_EVENT`. Test participant B is connected to the Message Bus but is not subscribed to `INPUT_EVENT`.

**Stimulus:** Produce one valid source-facing detection.

**Expected:** A receives exactly one `INPUT_EVENT`. B receives no `INPUT_EVENT` merely because it is connected to the bus.

### 3.5 Multiple permitted subscribers

**Setup:** Authorised Test/Diagnostics consumers A and B both subscribe to `INPUT_EVENT`.

**Stimulus:** Produce one valid source-facing detection.

**Expected:** Both permitted subscribers receive the same published `INPUT_EVENT`, preserving its stable identity and Relevant Time. The Input Module does not manufacture consumer-specific copies or require knowledge of the subscriber set.

### 3.6 Publisher independence from consumers

**Method:** Exercise the Stage 3 path with an authorised Test/Diagnostics subscriber present, then repeat with a permitted subscriber added, removed or replaced as appropriate for the harness. Inspect the implementation boundary where required.

**Expected:** The Input Module's publication behaviour and production code path do not depend on the identity or number of Test/Diagnostics consumers. It publishes the standard `INPUT_EVENT` to the P&P Message Bus rather than addressing a particular downstream consumer.

### 3.7 INPUT_EVENT publication authority

**Stimulus:** A deliberately unauthorised test participant attempts to originate an `INPUT_EVENT` on the P&P Message Bus.

**Expected:** The attempted publication is rejected/not delivered as an authoritative `INPUT_EVENT`. Connectivity to the Message Bus does not grant authority to publish this Message Type.

### 3.8 Subscription does not grant publication authority

**Setup:** A Test/Diagnostics participant is legitimately permitted to consume and subscribes to `INPUT_EVENT`.

**Stimulus:** That participant attempts to originate an `INPUT_EVENT`.

**Expected:** Its consumer/subscriber status does not grant publisher authority. The attempted `INPUT_EVENT` publication is rejected/not delivered as authoritative.

### 3.9 No private point-to-point path

**Method:** Structural inspection plus instrumented acceptance evidence as appropriate.

**Expected:** Inter-component delivery from the Input Module to the authorised consumer occurs through the common P&P Message Bus subscription boundary. There is no private Input Module → consumer callback, direct invocation, private inter-component queue or other production path acting as an alternative to the P&P Message Bus.

### 3.10 No race meaning added

**Stimulus:** Produce a valid source-facing detection and inspect the `INPUT_EVENT` after Message Bus delivery.

**Expected:** The event still contains only the standard physical input meaning: stable Input ID/capability and Relevant Time, plus the generic P&P message envelope information required by the Message Contract. It contains or implies no Lane, Start/Finish, sector, drag, speed-trap, MUG or other competition interpretation.

### 3.11 Consumer independence

**Setup:** Two permitted Test/Diagnostics consumers are subscribed and receiving `INPUT_EVENT`. Remove, disable or unsubscribe one consumer while leaving the other subscribed.

**Stimulus:** Produce another valid source-facing detection.

**Expected:** The remaining consumer continues to receive the standard `INPUT_EVENT` normally. Publication by the Input Module does not depend on a particular consumer remaining present.

### 3.T Deliberate acceptance-harness failure

**Method:** Run one acceptance comparison with a deliberately wrong predetermined expectation, such as an incorrect expected event count or identity.

**Expected:** The harness reports FAIL and retains the mismatch as evidence. Restore the correct frozen expectation and confirm the corresponding acceptance test passes.

## 3. Evidence requirements

For every acceptance test retain evidence identifying:

- project/repository;
- exact firmware/source commit or otherwise unambiguous source snapshot under test;
- exact frozen Stage 3 acceptance-test commit;
- identifiable firmware build/hash where applicable;
- predetermined stimulus;
- predetermined expected result;
- captured actual result;
- PASS/FAIL result.

Retain raw Wokwi/serial/CLI output and structural-review evidence where they substantiate a test. A final PASS summary alone is not sufficient evidence.

## 4. Test discipline

The frozen criteria must not be changed to make an implementation pass.

Ordinary implementation defects may be corrected and affected tests rerun.

If a test exposes an ambiguity, contradiction or required product/architecture decision not answered by the governing documentation, Stage 3 acceptance stops and returns to the design side. The test harness must not silently decide the missing specification.

Acceptance tooling may observe or stimulate defined P&P boundaries but must not create a production shortcut around the boundary being tested.

## 5. Stage gate

Stage 3 is accepted only when tests 3.1–3.11 pass and the acceptance mechanism has demonstrated 3.T.

Passing Stage 3 does not authorise Stage 4 to begin until Stage 3 results have been reviewed and accepted and the next stage has then been frozen/released under the agreed development process.
