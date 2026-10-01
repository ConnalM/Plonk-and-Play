# Acceptance Tests — Stage 8 Browser Requests

**Status:** FROZEN before Stage 8 implementation  
**Scope:** Browser `START_REQUEST` only

## 1. Purpose

Stage 8 proves the first operational Browser request through the accepted path:

```text
Browser client
→ Browser Interface
→ P&P Message Bus
→ Race Control
→ REQUEST_RESULT / authoritative lifecycle State
→ Noticeboard
→ Browser refresh
```

The Browser requests an operation. It does not mutate authoritative State,
create a Session Definition, schedule GO, or gain authority by claiming an
identity. Race Control remains the responsibility that decides the request,
creates session data and owns the start procedure.

Stage 8 uses the existing mutable proposed Race Setup in working RAM. It does
not add Browser Race Setup editing or configuration.

## 2. Governing behaviour

These tests are derived from the committed P&P architecture, Message Contract
and first-implementation sequence.

- The Browser Interface is the P&P Message Bus participant. Individual
  browsers are clients behind it.
- Each Browser client has trusted server-side Client Context. A Browser does
  not gain authority by claiming a role or identity in a request.
- Permission and validity are separate checks. Permission comes from trusted
  Client Context; Race Control decides validity against current authoritative
  lifecycle State, proposed Race Setup and required capability availability.
- `START_REQUEST` contains no Race Setup copy and has a required correlation
  ID. It asks Race Control to start the race described by the current
  authoritative proposed Race Setup.
- Race Control validates the request and commits acceptance by successfully
  creating the immutable Session Definition in working RAM from the accepted
  proposed Race Setup and applicable installation/capability information.
  **Only after that Session Definition has been successfully established** may
  Race Control externally publish the correlated `REQUEST_RESULT: ACCEPTED`
  and proceed with the authoritative start procedure.
- If Session Definition creation cannot succeed, Race Control rejects the
  request and must not publish `ACCEPTED`, transition lifecycle State or
  schedule GO.
- `ACCEPTED` means the request was accepted. It does not mean that GO has
  occurred or that the start procedure has completed.
- Current authoritative State and Facts remain available through the
  Noticeboard and accepted Stage 7 refresh path.
- Browser/network delay, rendering or failure must never delay authoritative
  race processing, P&P System Time or scheduled GO.

## 3. Stage 8 test configuration

### 3.1 Deterministic trusted Client Context

The acceptance harness establishes Client Context at the Browser Interface's
server side before the Browser request is accepted. The request payload does
not select, create or upgrade context.

| Context | START permission |
| --- | --- |
| Race Director / SMUG | authorised |
| Spectator / non-start client | not authorised |

No login, account, password, pairing or persistent authentication mechanism is
part of Stage 8.

### 3.2 Principal valid proposed Race Setup

Before the Browser request, the fixture establishes this mutable working Race
Setup:

- Lap Race;
- one active lane;
- one selected MUG/competitor;
- target of two laps;
- Immediate finish;
- the accepted stable simulated detector proposed as Lane 1 Start/Finish;
- documented arrangement in which the car starts before Start/Finish;
- required Start/Finish capability currently available; and
- optional race features disabled.

This is proposed working data, not a Session Definition. It contains no
pre-created Session ID or Race Entry ID. Race Control creates session identity
and Race Entry identity only when it commits acceptance.

### 3.3 Second valid proposed Race Setup

For test 8.16, establish another valid one-lane Lap Race setup that differs in
at least selected MUG/competitor and lap target. The fixture uses a target of
three laps. The request representation remains identical.

### 3.4 Invalid proposed Race Setup

For the deterministic invalid-setup test, establish a Lap Race with target
`0` laps. It is invalid for this Stage 8 fixture because a Lap Race requires a
positive target.

Expected deterministic rejection reason:

```text
invalid race setup: lap target must be at least 1
```

This is a Stage 8 acceptance reason, not a universal future vocabulary.

### 3.5 Unavailable required-capability fixture

Use the otherwise valid principal setup, but mark its required stable Lane 1
Start/Finish capability unavailable through the controlled availability fixture.

Expected deterministic rejection reason:

```text
required Start/Finish capability unavailable
```

## 4. Browser transport representation

Stage 8 extends the existing Stage 7 HTTP Browser Interface with these two
fixed paths. This preserves the existing HTTP polling model and avoids a
general-purpose Browser command protocol or WebSocket implementation.

### 4.1 Submit a START request

```http
POST /request/start
Content-Type: application/json
Cache-Control: no-store

{"correlationId":12345}
```

`correlationId` is the only Stage 8 request payload field. The Browser
Interface acknowledges successful transport submission as follows:

```http
202 Accepted
Content-Type: application/json
Cache-Control: no-store

{"submitted":true,"correlationId":12345}
```

HTTP `202` is a transport acknowledgement only. It is not Race Control's
decision and must never be presented as a P&P `REQUEST_RESULT: ACCEPTED`.

### 4.2 Obtain a correlated Request Result

```http
GET /request-result?correlationId=12345
Cache-Control: no-store
```

Before Race Control has decided, this returns `204 No Content`. Once decided,
it returns the real P&P Request Result:

```json
{"correlationId":12345,"result":"ACCEPTED"}
```

or:

```json
{"correlationId":12345,"result":"REJECTED","reason":"required Start/Finish capability unavailable"}
```

Authoritative current State and Facts continue through Stage 7 `/noticeboard`,
`/state` and `/fact` paths. A Request Result is distinct from State and from a
transient Fact/Event.

## 5. Acceptance tests

### 8.1 Browser originates START_REQUEST

**Setup:** Principal valid proposed Race Setup; Race Director/SMUG Client Context.

**Stimulus:** Submit `POST /request/start` from the Browser.

**Expected:** The Browser Interface originates one standard `START_REQUEST`.
There is no direct Browser-to-Race-Control control route.

### 8.2 Correlation ID is retained

**Stimulus:** Submit a request with a predetermined correlation ID.

**Expected:** The Bus-delivered `START_REQUEST` contains that ID and its
eventual `REQUEST_RESULT` contains the same ID.

### 8.3 Request uses the real P&P Message Bus

**Method:** Trace the Stage 8 request from Browser Interface to Race Control.

**Expected:** Race Control consumes the genuine Bus-delivered request.
Structural evidence confirms no private point-to-point control path.

### 8.4 Browser-supplied authority claim is ignored

**Setup:** Spectator/non-start Client Context.

**Stimulus:** Submit START with an extraneous Browser-supplied authority claim,
such as `X-Claimed-Role: RaceDirector`.

**Expected:** The trusted server-side spectator context remains decisive. The
claim grants no authority and Race Control rejects the request.

### 8.5 Authorised Client Context may request START

**Setup:** Principal valid proposed Race Setup; Race Director/SMUG Client Context.

**Stimulus:** Submit START.

**Expected:** The request passes the permission check and reaches Race
Control's validity checks.

### 8.6 Unauthorised START is rejected

**Setup:** Principal valid proposed Race Setup; spectator/non-start Client Context.

**Stimulus:** Submit START.

**Expected:** A correlated `REQUEST_RESULT: REJECTED` gives the deterministic
useful reason `START permission denied`. No Session Definition is created and
authoritative lifecycle State does not begin starting.

### 8.7 Lifecycle validity is checked

**Setup:** Race Control lifecycle State is not startable.

**Stimulus:** An authorised Browser submits START.

**Expected:** Race Control returns a correlated rejection with a useful
lifecycle reason. It creates no new Session Definition, start procedure or GO.

### 8.8 Invalid Race Setup is rejected

**Setup:** The deterministic zero-lap invalid proposed Race Setup.

**Stimulus:** An authorised Browser submits START.

**Expected:** Race Control returns a correlated rejection with
`invalid race setup: lap target must be at least 1`. It creates no Session
Definition and does not change authoritative lifecycle State.

### 8.9 Unavailable required capability is rejected

**Setup:** The deterministic unavailable-capability fixture.

**Stimulus:** An authorised Browser submits START.

**Expected:** Race Control returns a correlated rejection with
`required Start/Finish capability unavailable`. It creates no Session
Definition and does not change authoritative lifecycle State.

### 8.10 Accepted Request Result

**Setup:** Principal valid proposed Race Setup; Race Director/SMUG Client Context.

**Stimulus:** Submit START.

**Expected:** Race Control makes exactly one correlated
`REQUEST_RESULT: ACCEPTED` available to the requesting Browser. The prior
successful creation of the fixed Session Definition is evidenced by test 8.11.

### 8.11 Session Definition is committed before ACCEPTED is visible

**Method:** Inspect and trace the accepted-request path.

**Expected:** Race Control validates the request and successfully establishes
exactly one immutable Session Definition in working RAM from the accepted
proposed Race Setup and applicable installation/capability information as part
of committing acceptance. That fixed Session Definition exists before the
correlated `REQUEST_RESULT: ACCEPTED` becomes externally visible. No accepted
result, lifecycle transition or GO may be externally visible without it. If
creation fails, the request is rejected and Race Control publishes neither
`ACCEPTED` nor resulting lifecycle State/GO.

### 8.12 Session Definition remains fixed

**Setup:** A Session Definition has been created by an accepted request.

**Stimulus:** Change the mutable proposed Race Setup.

**Expected:** The active Session Definition remains unchanged. Its race rules,
selected entry and capability-role mappings do not silently change.

### 8.13 Accepted START follows the existing Race Control path

**Stimulus:** Follow an accepted request into operation.

**Expected:** Race Control begins the existing authoritative start procedure:
`READY → STARTING → GO_SCHEDULED → RACING`. Browser code does not create State
or GO.

### 8.14 Browser obtains authoritative outcome correctly

**Stimulus:** Observe both a rejected request and an accepted request in the Browser.

**Expected:** The Browser obtains the correlated Request Result through
`/request-result`. It obtains resulting lifecycle/session State and Facts
through the existing authoritative Noticeboard refresh path. Request submission
is not treated as State.

### 8.15 Closely arriving double START is safe

**Setup:** Principal valid proposed Race Setup; Race Director/SMUG Client Context.

**Stimulus:** Submit two `START_REQUEST`s with distinct predetermined
correlation IDs as closely as the harness can arrange, without waiting for a
visible lifecycle transition.

**Expected:** Regardless of arrival closeness, exactly one request is accepted,
exactly one Session Definition is created, at most one start procedure begins,
and at most one GO is scheduled. The other request does not create or initiate
a second session/start.

### 8.16 START has no Race Setup copy

**Setup:** Run the accepted path with both deterministic valid proposed Race Setups.

**Stimulus:** Submit the same-format `START_REQUEST` for each setup.

**Expected:** The request contains only its correlation ID. It contains no Race
Setup copy, lane count, lap target, selected MUG, finish behaviour or other race
choice. Race Control operates on the current authoritative proposed Race Setup.

### 8.17 Stage 7 Browser regression

**Method:** Rerun the complete frozen Stage 7 synchronisation campaign after
Stage 8 request activity.

**Expected:** Initial State retrieval, State-refresh retry, separation of Fact
failure from State synchronisation and recovery remain correct. Browser request
activity does not delay authoritative processing.

### 8.T Deliberate acceptance-harness failure

**Method:** Deliberately expect `ACCEPTED` for a known unauthorised or invalid request.

**Expected:** The harness reports FAIL and retains the correlation ID, expected
result, actual result and rejection reason. Restore the correct expected
rejection and confirm PASS.

## 6. Structural and isolation requirements

Evidence must confirm that:

- Race Control, not Race Engine, Noticeboard, Session Definition or Browser
  code, consumes `START_REQUEST` and decides the request;
- Race Control owns Session Definition creation and the start procedure;
- Session Definition remains fixed working data, not a Message Bus participant;
- Browser Interface is the Bus participant while individual Browsers remain
  clients behind it;
- the Browser cannot directly mutate authoritative State or manufacture GO;
- slow, disconnected or abandoned Browser transport cannot delay Race Control,
  Race Engine, timing or scheduled GO; and
- Browser State remains presentation/cache State; Noticeboard State remains
  authoritative.

## 7. Duplicate-request boundary

For Stage 8, distinct correlation IDs represent distinct requests. Test 8.15
therefore requires independent safe decisions for its two requests.

Repeated use of the same correlation ID has no defined retransmission,
deduplication, replay or idempotency semantics in Stage 8. That work is
deliberately deferred to later network and multi-client robustness stages.

## 8. Evidence requirements

For every test retain:

- exact source commit and PlatformIO build identity;
- exact frozen Stage 8 test-specification revision;
- predetermined fixture, Browser Client Context, stimulus and correlation ID;
- authoritative lifecycle State before and after the stimulus;
- captured Browser transport submission/result, Bus delivery and Race Control
  decision where applicable;
- Session Definition, Noticeboard and GO evidence where applicable;
- expected and actual result; and
- PASS or FAIL outcome.

Retain raw output and structural evidence where required. The deliberate failure
evidence for 8.T must be retained alongside the passing campaign.

## 9. Scope discipline and explicit deferrals

Stage 8 implements only Browser `START_REQUEST` and its correlated
`REQUEST_RESULT` boundary. It does not implement:

- Browser Race Setup editing or configuration requests;
- a complete Race Setup schema or universal future rejection-reason vocabulary;
- login, accounts, passwords, pairing, persistent authentication or remote
authorisation design;
- same-correlation retransmission, replay or idempotency semantics;
- Browser PAUSE, RESUME, STOP, RESET or other operational requests;
- two-lane or rapid-input testing;
- multiple Browser clients, disconnect/reconnect or mid-race joining;
- persistence, history, results or outputs; or
- Browser visual polish, WebSocket transport or a general-purpose command protocol.

## 10. Stage gate

Stage 8 is ready for acceptance only when tests 8.1–8.17 and 8.T pass, the
required evidence is retained, the frozen Stage 7 regression passes, and the
implementation preserves the governing authority boundaries.

Implementation must stop and return to design review if a test exposes a
genuine governing-document ambiguity or contradiction rather than an ordinary
implementation or harness defect.