# Acceptance Tests — Stage 9 Two Lanes and Rapid-Event Testing

**Status:** FROZEN before Stage 9 implementation  
**Scope:** Two-lane Lap Race, rapid race-critical input delivery, and the minimum
race-integrity response to input-delivery overrun.

## 1. Purpose

Stage 9 proves that P&P can run a genuine two-lane Lap Race through the accepted
production boundaries:

```text
Two simulated sources
→ two Input Devices inside the Input Module
→ reliable standard INPUT_EVENT delivery on the P&P Message Bus
→ Race Engine + fixed two-lane Session Definition
→ authoritative per-Race-Entry lap State / LAP_COMPLETED / completion
→ Noticeboard
→ Browser presentation
```

It proves that rapid, simultaneous and delayed/out-of-order events preserve
their original authoritative Relevant Times. Browser, network, diagnostics,
storage and presentation work remain non-authoritative and cannot cause
race-critical input to disappear.

Stage 9 is a software-architecture timing proof. It does not claim that a
one-microsecond software test establishes one-microsecond physical accuracy or
resolution for the standard low-cost sensor arrangement.

## 2. Governing behaviour

- Input Devices accept/condition their own source-facing signals. The Input
  Module publishes only stable physical input identity, event identity and
  original Relevant Time; it does not assign Lane, Start/Finish, MUG or lap
  meaning.
- The immutable Session Definition maps the two stable inputs to Lane 1 and
  Lane 2 Start/Finish roles, and maps two stable Race Entries to those lanes.
- Race Engine owns independent lap/timing State for each Race Entry and obtains
  all race meaning from the fixed Session Definition.
- Race Engine uses Relevant Time rather than receipt, queue or Browser order.
  Equal effective Relevant Times do not gain an artificial order from lane,
  queue or processing order.
- The selected and only implemented Stage 9 finish rule is **Immediate finish**:
  winner finishes → all stop immediately.
- A clean accepted crossing is a historical race fact. Temporary downstream
  congestion must not silently lose or retimestamp it.
- A genuine finite-capacity overrun must latch a trustworthy race-integrity
  fault. P&P must not continue to present a plausible valid race result.
- Browser State is presentation/cache State. Noticeboard State remains
  authoritative. Slow, disconnected or absent Browser activity must not delay
  race capture, ordering, timing or State maintenance.

## 3. Stage 9 test configuration

### 3.1 Two simulated physical inputs

The fixture uses two independent source-facing simulated Input Devices within
one Input Module:

| Device role in fixture | Stable Input ID/capability |
| --- | --- |
| detector A | predetermined stable identity A |
| detector B | predetermined stable identity B |

Each detector retains the accepted source-side detection, debounce, clearing
and re-arm behaviour. A fixture must stimulate source state; it must not
manufacture `INPUT_EVENT` downstream of the Input Module.

### 3.2 Principal two-lane proposed Race Setup

The deterministic working proposed Race Setup contains:

- Lap Race;
- two active lanes;
- two Race Entries with predetermined stable Race Entry and MUG identities;
- Lane 1 Start/Finish mapped to detector A;
- Lane 2 Start/Finish mapped to detector B;
- 20-lap target for the rapid campaign; and
- separate earlier-winner and dead-heat completion fixtures use a two-lap target, with each Race Entry already on Lap 1 before the final crossing;
- cars starting before their respective Start/Finish detectors;
- Immediate finish;
- both required Start/Finish capabilities available; and
- optional race features disabled.

On accepted START, Race Control creates an immutable Session Definition from
this setup. Subsequent mutable setup changes cannot alter it.

### 3.3 Rapid legitimate input fixture

The fixture drives each real simulated detector through its full clean cycle:

- 20 ms active recognition and 20 ms clear/re-arm;
- one clean trigger every 40 ms per detector;
- 25 clean crossings per second per lane;
- 50 clean crossings per second aggregate; and
- at least 16 paired crossings: 32 authoritative input events.

The two lanes have paired and overlapping activity. The deterministic 20-lap target means the 16 paired crossings leave both entries below completion.

This is deliberately much harder than normal two-lane Lap Race traffic while
remaining within the actual simulated detector's clean-trigger behaviour.

### 3.4 Software timing-order fixtures

Separate deterministic fixtures provide valid clean events with:

- identical Relevant Times, one for each lane;
- Relevant Times separated by exactly one microsecond; and
- delivery/publication order opposite to Relevant-Time order.

These are software timing/ordering tests. They prove preservation and
interpretation of authoritative values supplied by an input/REU. They do not
claim physical one-microsecond discrimination by a standard low-cost sensor.

### 3.5 Approved protected Input Module backlog

**Approved capacity: 8 complete `INPUT_EVENT`s.**

The Input Module holds an accepted event in this fixed FIFO until authoritative
Race Engine delivery succeeds. It retains the original stable Input ID,
event identity and Relevant Time without modification.

The approved capacity matches the current
8-message Race Engine mailbox. If Race Engine's mailbox is full, the protected
Input Module FIFO adds a further 8 accepted events: 16 events in flight in
total, equivalent to 320 ms at the defined 50-event/second aggregate rate.
The FIFO itself covers 160 ms of temporary downstream congestion after the
mailbox is full. That is a meaningful ESP32 scheduling margin without using a
large queue to conceal a sustained processing failure.

The legitimate rapid campaign must record the maximum protected-backlog depth.
Its acceptance criterion is **maximum depth no greater than 2 events**: at most
one simultaneous two-lane pair may be waiting. A greater depth demonstrates
accumulation across clean-trigger cycles and fails the campaign even if no
event was eventually lost. This proves the normal path is keeping up rather
than merely surviving behind the buffer.
### 3.6 Overload fixture

Hold authoritative Race Engine delivery long enough to fill the protected
Input Module backlog, then produce the next clean trigger.

The fixture must capture the complete retained backlog, the triggering
capacity-exhaustion condition and the resulting authoritative fault State. It
must never describe the rejected/overflowing crossing as successfully recorded.

## 4. Approved minimum race-integrity mechanism for Stage 9

Stage 9 uses the following narrow contract, rather than a broad future
fault-management framework.

### 4.1 `RACE_INTEGRITY_FAULT` status/fact

The Input Module publishes a standard P&P `RACE_INTEGRITY_FAULT` status/fact
when, and only when, its protected accepted-input FIFO cannot retain the next
clean trigger. For Stage 9 its only reason is:

```text
INPUT_EVENT_DELIVERY_OVERRUN
```

The message identifies the fault reason and the P&P System Time at which
capacity exhaustion was detected. It is not a replacement `INPUT_EVENT` and
must not claim that the overflowing crossing was successfully recorded.

### 4.2 Authoritative consequence and Noticeboard State

Race Control consumes this status/fact and latches the corresponding current
race-integrity State. The minimum externally presentable Noticeboard fields are:

```text
raceIntegrity: FAULTED
raceIntegrityReason: INPUT_EVENT_DELIVERY_OVERRUN
resultValid: false
```

Race Control aborts/invalidates the active race and prevents further normal
race interpretation. Race Engine must not manufacture `COMPETITION_COMPLETE`
or a valid result from the compromised session. Already accepted/recorded
information may remain visible for diagnosis, but it is explicitly invalid for
race-result purposes.

This representation, message name and field names are the approved minimum
Stage 9 proof only; they do not create a universal future fault taxonomy.

### 4.3 Recovery boundary

Stage 9 does not implement Browser RESET or a general product recovery
protocol. A deliberate **system restart/reinitialisation** is the prototype
recovery boundary after a Race Integrity Fault. Until that restart, the fault
must not clear itself and the compromised race must never resume or present
itself as valid.
## 5. Acceptance tests

### 9.1 Two independent source-facing Input Devices

**Stimulus:** Independently stimulate detector A and detector B through their
source-facing interfaces.

**Expected:** Each accepts/re-arms independently and produces clean standard
`INPUT_EVENT`s with its own stable identity. No source-facing state, filtering
or clearing is published as race data.

### 9.2 Two-lane Session Definition

**Setup:** Principal two-lane proposed Race Setup and accepted START.

**Expected:** The fixed Session Definition contains exactly the required
Lane 1 and Lane 2 Start/Finish mappings and two Race Entries. Input Devices and
the Input Module have no Session Definition lookup or race-role logic.

### 9.3 Independent Race Entry interpretation

**Stimulus:** Produce one valid post-GO crossing from each detector.

**Expected:** Race Engine resolves each physical identity to the correct Lane
and Race Entry. Each first crossing completes only that Race Entry's Lap 1,
timed from the authoritative GO instant.

### 9.4 Per-lane lap count and timing

**Stimulus:** Produce a predetermined mixed sequence of valid crossings for
both lanes at known Relevant Times.

**Expected:** Each Race Entry has its own authoritative lap count, last lap
time and crossing origin. A crossing on one lane does not alter the other
lane's timing State.

### 9.5 LAP_COMPLETED identity and timing

**Stimulus:** Complete predetermined laps on both lanes.

**Expected:** Each `LAP_COMPLETED` carries the correct Race Entry ID, lap
number, lap time and original completion Relevant Time. It does not copy lane,
MUG or sensor implementation detail into the Fact.

### 9.6 No cross-lane contamination

**Stimulus:** Vary one lane's crossing pattern while holding the other lane's
predetermined valid crossings constant.

**Expected:** The unchanged lane's authoritative lap State and Facts remain
unchanged.

### 9.7 Earlier winning crossing under Immediate finish

**Setup:** Both Race Entries approach the configured final lap with distinct
predetermined final-crossing Relevant Times.

**Stimulus:** Produce Lane 1's final crossing earlier than Lane 2's.

**Expected:** Lane 1 is authoritative first; Race Engine publishes normal
competition completion from the Immediate-finish rule; Race Control finishes
the session. Lane 2 cannot receive an invented later completion after the
race has ended.

### 9.8 Equal-time winning crossings are a dead heat

**Setup:** Both Race Entries approach the configured final lap.

**Stimulus:** Produce both winning crossings with exactly equal effective
Relevant Time, in each publication order where practical.

**Expected:** The authoritative result is a dead heat. Arrival order, lane
number, queue order and processing order do not manufacture a winner.

### 9.9 One-microsecond timing distinction

**Stimulus:** Produce valid Lane 1 and Lane 2 crossings whose Relevant Times
differ by exactly one microsecond.

**Expected:** Race Engine preserves and correctly uses that distinction. This
proves software preservation only; it makes no claim about standard-sensor
physical timing resolution or accuracy.

### 9.10 Delivery order does not redefine time order

**Stimulus:** Deliver valid events in the opposite order to their Relevant
Times.

**Expected:** Authoritative lane timing and any race consequence follow
Relevant Time, not publication, mailbox or polling order.

### 9.11 Duplicate protection across two lanes

**Stimulus:** Deliver a non-adjacent duplicate for each lane's identifiable
source event during mixed lane traffic.

**Expected:** Each source event changes authoritative State at most once. A
duplicate cannot add a lap, alter a lap time, create a false winner or repair a
previously rejected stale event.

### 9.12 Legitimate high-rate production-path campaign

**Stimulus:** Run the 16 paired-crossing, 40 ms per-lane, 50 Hz aggregate
fixture through the real Input Devices, Input Module, protected backlog, Bus
and Race Engine.

**Expected:** All 32 accepted events are delivered and interpreted exactly
once; original identities and Relevant Times are retained; no authoritative
input delivery overrun occurs; per-lane State matches the predetermined result;
and retained instrumentation records a maximum protected-backlog depth of no
more than 2 events.

### 9.13 Temporary Race Engine congestion is lossless

**Stimulus:** Arrange a temporary full Race Engine mailbox while the protected
Input Module backlog has capacity, then allow normal delivery to resume.

**Expected:** The original complete events remain in the Input Module backlog
and are later delivered to Race Engine exactly once, retaining original Input
IDs, event identities and Relevant Times. No Browser, Diagnostics or
presentation backlog can cause loss.

### 9.14 Race-critical backlog overrun is explicit

**Stimulus:** Run the overload fixture until the fixed protected backlog is
exhausted.

**Expected:** Input Module publishes `RACE_INTEGRITY_FAULT` with
reason `INPUT_EVENT_DELIVERY_OVERRUN`; P&P latches the corresponding
Race Integrity Fault. It does not silently count the overflowing crossing or
continue presenting a plausible valid race.

### 9.15 Integrity-fault lifecycle and State

**Stimulus:** Observe P&P after test 9.14.

**Expected:** Race Control aborts/invalidates the active competition. The
Noticeboard exposes `raceIntegrity: FAULTED`, the overrun reason and
`resultValid: false`. No normal completion/result is manufactured from the
compromised race.

### 9.16 Integrity fault requires deliberate recovery

**Stimulus:** After a latched integrity fault, attempt ordinary detector
crossings and START without restarting P&P, then deliberately restart and
reinitialise P&P.

**Expected:** Before restart, P&P does not silently resume valid racing. The
latched fault clears only through the Stage 9 prototype recovery boundary of
system restart/reinitialisation; a new valid session is then established only
through the normal accepted START path.

### 9.17 Browser/presentation cannot compromise timing

**Stimulus:** Run mixed and rapid two-lane input with the Browser slow,
disconnected or not consuming presentation traffic.

**Expected:** Authoritative capture, Input Module backlog delivery, Race Engine
interpretation, Relevant Times and competition result remain correct. Ordinary
presentation notifications/facts may be superseded only according to their
existing delivery contract.

### 9.18 Browser reconstructs two-lane current State

**Stimulus:** Connect or reload a fresh Browser after both Race Entries have
progressed.

**Expected:** The Browser reconstructs authoritative two-lane State from the
Noticeboard, including both Race Entry identities and their per-lane progress.
It does not require replay of all transient facts and does not become a race
owner.

### 9.19 No private race-critical bypass

**Method:** Structural review and trace of the complete input path.

**Expected:** Accepted crossings travel through the Input Module and real P&P
Message Bus to Race Engine. The protected backlog is Input Module delivery
state, not a private Input-to-Race-Engine processing route. Session Definition
remains fixed data, and Browser/Diagnostics do not participate in timing
ownership.

### 9.20 Stage 1–8 regression

**Method:** Rerun applicable earlier frozen campaigns after Stage 9 changes.

**Expected:** All applicable prior behaviour passes with retained evidence.
No new Stage 9 supersession is proposed. The only permitted supersessions remain
those previously authorised: Stage 1 test 1.7's historical DiagnosticProbe
numeric-value assertion only, and Stage 6 tests 6.13/6.14 only to the extent
that their explicit no-production-Browser-START assertions conflict with
frozen Stage 8 Browser START functionality. No ordinary regression, harness
defect or unexplained failure may be classified as superseded.

### 9.T Deliberate acceptance-harness failure

**Method:** Deliberately replace one predetermined per-lane expected lap count,
lap time, dead-heat result or integrity-fault condition with a wrong expected
value.

**Expected:** The evaluator reports FAIL, retaining the stimulus, expected and
actual authoritative result. Restore the correct expectation and confirm PASS.

## 6. Structural and isolation requirements

Evidence must confirm that:

- two Input Devices remain internal to the Input Module and race-ignorant;
- only Input Module publishes standard physical `INPUT_EVENT`;
- Input Module retains complete accepted events only until authoritative Bus
  delivery succeeds; it does not interpret laps or directly alter Race Engine
  State;
- Race Engine owns per-Race-Entry lap/timing, dead-heat result and competition
  interpretation;
- Race Control owns the operational abort/invalidation consequence of a latched
  race-integrity fault;
- Session Definition contains fixed two-lane roles and Race Entries but no GPIO,
  transport address, detector threshold or Browser connection;
- Browser/Diagnostics/storage delivery cannot block or cause loss of an
  authoritative input event; and
- no generic "all Presentation messages are lossy" rule is introduced.

## 7. Evidence requirements

For every test retain:

- exact source commit/build identity and frozen-plan revision;
- source-facing detector stimulus, stable input identity, event identity and
  predetermined Relevant Time(s);
- Session Definition, Race Entry, Bus delivery/backlog and Race Engine
  authoritative State evidence as applicable;
- Browser/Noticeboard State and fault evidence where applicable;
- protected-backlog depth after every rapid campaign event and the recorded
  maximum depth;
- expected and captured actual result; and
- PASS or FAIL outcome.

Retain raw high-rate, reversed-order, temporary-congestion and overload output.
Retain deliberate-failure evidence separately. The source manifest must include
the production delivery/backlog implementation and harness.

## 8. Scope discipline and explicit deferrals

Stage 9 implements only the minimum required for two-lane Lap Race,
rapid-event delivery and the narrow input-delivery-overrun integrity response.
It does not implement:

- physical REU design or sensor timing-quality metadata such as resolution,
  accuracy, uncertainty or sample rate;
- Browser Race Setup editing;
- Stage 10 multiple-Browser/reconnect campaign;
- Pause/Resume;
- persistence/history or full results storage;
- physical outputs/audio;
- Complete Current Lap or Complete Full Race Distance finish behaviour;
- broader multi-competitor ranking/partial-lap policy; or
- a broad general-purpose fault-management framework;
- Browser RESET or other product recovery controls.

## 9. Regression supersession position

**No new Stage 9 supersession is proposed.** The following earlier wording is
not a supersession case:

- Stage 3's statement that it was not the rapid-event/load campaign is a
  historical stage purpose, while tests 3.1–3.11 require the same Input Module
  → P&P Message Bus boundary that Stage 9 continues to use.
- Stage 5's one-lane fixture and scope exclusions do not require later
  production code to remain one-lane. Tests 5.1–5.13 continue to run against
  their frozen one-lane fixture.
- Stage 6's exclusion of second-lane/rapid testing is a stage-scope statement;
  its numbered lifecycle tests do not assert that later production must lack
  those capabilities. Its existing 6.13/6.14 Browser-START supersession remains
  narrow and unchanged.
- Stage 7's excluded two-lane/rapid work is a deferral. Tests 7.1–7.16 require
  Browser synchronisation and non-interference, both of which Stage 9 must
  preserve and exercise with two-lane State.
- Stage 8's two-lane/rapid exclusion is likewise a deferral. Test 8.16 still
  requires START to carry no Race Setup copy, which Stage 9 must preserve.

The protected Input Module backlog is not a private Input Module → Race Engine
route: it holds an accepted event before publication and delivery through the
real P&P Message Bus. Any implementation that bypassed that Bus would fail
frozen Stage 3 test 3.9 and Stage 9 test 9.19; it is not superseded.

The complete approved regression supersession list remains:

| Frozen assertion | Limited authorised supersession |
| --- | --- |
| Stage 1 test 1.7 | Historical `DiagnosticProbe` numeric-value expectation only; current permitted delivery behaviour remains required. |
| Stage 6 test 6.13 | Only its explicit prohibition on production Browser `START_REQUEST`/acceptance, introduced by frozen Stage 8. |
| Stage 6 test 6.14 | Only its explicit no-Browser-START scope assertion; its no-Browser-dependency lifecycle behaviour remains required. |

## 10. Human Browser checkpoint
After the automated campaign passes, a short human checkpoint is required on
the real Browser/firmware path:

1. Open the Stage 9 diagnostic Browser and confirm it is synchronised.
2. Start the prepared two-lane demonstration race through the normal accepted
   START path.
3. Trigger the clearly labelled Lane 1 and Lane 2 simulated sources in the
   supplied short demonstration sequence.
4. Observe both Race Entries independently accumulating laps, with no
   cross-lane change, then observe correct completion.

This checkpoint is observational only. It supplements automated evidence and
does not require rapid overload, dead-heat or fault injection by the user.

## 11. Stage gate

Stage 9 is ready for acceptance only when tests 9.1–9.20 and 9.T pass, the
approved race-critical backlog capacity and integrity-fault behaviour are
proven, the 50 Hz campaign records maximum protected backlog depth no greater
than 2, two-lane Browser State is reconstructed correctly, the human checkpoint
passes, and applicable Stage 1–8 regressions have retained evidence.

Implementation must stop for design review if a test exposes a genuine
architecture or product contradiction. This draft must be reviewed and frozen
before any Stage 9 production implementation begins.




