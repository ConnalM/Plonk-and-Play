# Acceptance Tests — Stage 10 Multiple Browsers and Recovery

**Status:** FROZEN before Stage 10 implementation  
**Scope:** The minimum retained one-Master Browser model, multiple Browser
presentation, and recovery to current authoritative State.

## 1. Purpose

Stage 10 proves that P&P can serve several independent Browser identities
without making Browser presence, performance, connection order or transport
state part of authoritative race operation.

It adds the minimum one-Master model:

```text
Browser identity (opaque server-issued persistent identity)
→ Browser Interface server-side authority binding
→ trusted Client Context
→ P&P Message Bus
→ Race Control validation / Request Result
```

One retained Browser identity is the Master/Race Director. Other identities
are Spectators. The Browser Interface is the only Browser-side P&P Message Bus
participant. Race Control receives a trusted logical Client Context and has no
knowledge of Browser identities, tokens, cookies, connections, tabs or HTTP.

All Browser identities obtain current authoritative Noticeboard State on first
connection or reconnection. They do not require event replay. Browser activity
must never delay, change or stop authoritative race operation.

## 2. Governing behaviour

- A Browser identity is an opaque, high-entropy value issued by the Browser
  Interface in a persistent cookie. It has no encoded role, authority or user
  claim and does not depend on HTTPS or the `Secure` cookie attribute.
- The Browser Interface resolves that identity only against server-side
  authority State. Browser JSON, headers, query parameters, UI State and
  connection order cannot create, select or upgrade authority.
- A controller with no retained Master binding has **NO MASTER**. Opening,
  polling, synchronising or submitting `START_REQUEST` does not grant Master
  authority.
- A deliberate, one-time empty-body `POST /bootstrap` action may bind the
  initial Master only while no retained Master binding exists. Passive Browser
  activity never performs bootstrap.
- The retained Master binding survives Master Browser refresh/reopen and
  Browser Interface/ESP32 restart. Other identities remain Spectators across
  restart. A Spectator already connected at restart must not become Master.
- Master authority is held by a Browser identity/session, not a named human.
  A missing, sleeping or disconnected Master does not affect an active race.
- If the Master identity is lost, for example through Browser site-data loss,
  Stage 10 does not automatically assign a replacement. Deliberate takeover,
  relinquishment and recovery UI are deferred.
- A Spectator receives authoritative State/Facts permitted for presentation,
  but cannot issue `START_REQUEST` or other Race Director controls. Race
  Control separately validates the current lifecycle and other normal request
  conditions for a Master request.
- Request Results are private to the originating Browser identity. A client
  cannot obtain another identity's result by reusing or guessing its visible
  correlation ID.
- Current Noticeboard State, rather than event replay, is the reconstruction
  mechanism for joining, reconnecting and reloading Browsers.

## 3. Deterministic acceptance configuration

### 3.1 Browser identities and authority fixture

The harness establishes at least three distinct opaque Browser identities:

| Identity | Initial server-side status |
| --- | --- |
| `master-A` | no role until the deliberate bootstrap binds it as Master |
| `viewer-B` | Spectator |
| `viewer-C` | Spectator |

The fixture must treat each as a distinct persistent cookie-store context. It
must not represent multiple clients merely by separate tabs sharing one
Browser profile. For controlled fixture preparation only, it may initialise or
clear the retained Master binding before a test begins. That preparation is not
a Browser or product control path.

The retained Master store is durable controller-side storage and contains only a safe fingerprint/handle of the Master identity, never a Browser-supplied role claim. It starts empty for bootstrap tests and is then retained across the specified Browser Interface restart tests. The fixture records its contents only as safe identity fingerprints/handles.

### 3.2 Race fixture

Use an existing deterministic valid Stage 9 two-lane Race Setup and normal
accepted Stage 8 `START_REQUEST` path. The fixture supplies controlled Input
Device crossings sufficient to keep a race active while clients join, leave,
sleep, reconnect or exert presentation pressure. It does not create State or
race events through the Browser Interface.

### 3.3 Bootstrap and request representation

The one-time bootstrap is `POST /bootstrap` with an empty request body. It is
a Browser Interface action, not a Race Control operation. The Browser sends no
role, Master, Race Director, SMUG, identity or authority assertion. Browser
Interface identifies the requester only from its automatically associated
persistent opaque Browser identity cookie.

`POST /bootstrap` succeeds only when the retained Master store is empty. It
atomically binds the caller's existing opaque identity as Master, after which
that Browser may display `Race Director`; all other identities display
`Spectator`. A non-empty Master store produces a deterministic rejection and
cannot change the binding. Passive page loading, polling, State retrieval and
connection order never invoke bootstrap.

The cookie contains only a high-entropy opaque Browser identity; it contains no
role or authority data. It persists across ordinary reload and Browser
close/reopen and is automatically associated with that Browser's requests.
Separate cookie stores, such as normal Chrome and Incognito, are separate
Browser identities. Stage 10 does not require HTTPS or the `Secure` cookie
attribute.

Stage 10 continues to use `POST /request/start` for `START_REQUEST`; it must
be evaluated using the Browser Interface's trusted resolved Client Context.

## 4. Acceptance tests

### 10.1 No Master by observation

**Setup:** Start with an empty retained Master store and separate Browser
identities `master-A`, `viewer-B` and `viewer-C`.

**Stimulus:** Each identity opens, polls and synchronises Browser State in each
arrival order; each may also submit a `START_REQUEST` before bootstrap.

**Expected:** The Browser Interface has no Master. Every identity is resolved
as a Spectator. Every `START_REQUEST` is rejected for lack of permission;
opening, polling, synchronising, connection order and rejected requests leave
the retained Master store empty and authoritative race State unchanged.

### 10.2 Deliberate one-time Master bootstrap

**Setup:** Test 10.1's empty retained Master store.

**Stimulus:** `master-A` performs empty-body `POST /bootstrap` through the
clearly labelled `Make this Browser Race Director` action.

**Expected:** Browser Interface atomically creates the retained Master binding
for `master-A` and resolves it as the only Race Director context. A duplicate
bootstrap from `master-A` and concurrent/subsequent bootstrap attempts from
`viewer-B`/`viewer-C` cannot replace, add or transfer the Master.

### 10.3 Browser claims cannot create authority

**Setup:** `master-A` is retained Master; `viewer-B` is Spectator.

**Stimulus:** `viewer-B` sends role/authority/identity claims in request JSON,
HTTP headers, query parameters and any Browser/UI fields made available by the
transport.

**Expected:** The Browser Interface continues to resolve `viewer-B` as
Spectator. Race Control receives Spectator context and rejects its
`START_REQUEST`; the Master binding and authoritative State do not change.

### 10.4 Master request path and normal validation

**Setup:** Retained Master `master-A`; valid READY fixture.

**Stimulus:** `master-A` submits `START_REQUEST` through the normal Browser
Interface and P&P Message Bus path.

**Expected:** Race Control receives trusted Race Director context, validates
normal Stage 8 conditions, creates the Session Definition before externally
publishing a private correlated `ACCEPTED` result, and begins the existing
authoritative start procedure. Browser submission does not directly change
State or schedule GO.

### 10.5 Spectator denial does not affect a race

**Setup:** A Master-started race is active; `viewer-B` is synchronised.

**Stimulus:** `viewer-B` submits `START_REQUEST` and polls for its result.

**Expected:** `viewer-B` receives its own correlated `REJECTED` result with a
deterministic permission reason. Existing authoritative race State, Session
Definition, scheduled GO and Race Engine progression remain unchanged.

### 10.6 Client-private Request Results

**Setup:** `master-A` and `viewer-B` issue requests using equal or deliberately
colliding visible correlation IDs.

**Stimulus:** Each identity retrieves its own result and attempts to retrieve
the other's result by correlation ID and by altered Browser-visible parameters.

**Expected:** Each identity can retrieve only its own result. No result is
exposed, overwritten or misattributed across identities. Result routing stays
within Browser Interface; Race Control remains unaware of Browser connection
identity.

### 10.7 Multiple viewers reconstruct current State

**Setup:** A Master-started two-lane race has authoritative progress on both
Race Entries.

**Stimulus:** Connect `viewer-B` and `viewer-C` at different times, including
while authoritative State changes.

**Expected:** Each independently becomes synchronised and reconstructs current
authoritative Noticeboard State, including both Race Entries and current
integrity/result State. Neither requires Fact replay, gains control authority
or affects the race.

### 10.8 Mid-race join has no replay dependency

**Setup:** Run a race with prior lap Facts and state changes while `viewer-C`
is absent.

**Stimulus:** Connect `viewer-C` mid-race.

**Expected:** It reconstructs sufficient current authoritative State from the
Noticeboard. Missing historical Facts do not leave its State incorrect, and no
replay endpoint or Browser-owned State reconstruction becomes required.

### 10.9 Master disconnection isolation

**Setup:** A Master-started race is active and at least one Viewer is present.

**Stimulus:** Disconnect, sleep or stop polling the Master Browser identity;
continue deterministic input crossings.

**Expected:** Race Control, Race Engine, System Time, input capture and
Noticeboard State continue correctly. The retained Master binding remains
unchanged; no Viewer is promoted.

### 10.10 Viewer disconnection and presentation pressure isolation

**Setup:** A Master-started race is active with multiple viewers.

**Stimulus:** Make viewers slow, sleeping, disconnected or non-consuming while
rapid normal race State/Facts are produced; reconnect them afterwards.

**Expected:** Authoritative timing, input delivery, State and result remain
correct. Presentation may supersede ordinary notification work according to
its established contract, but must not block or lose race-critical operation.
Reconnected viewers reconstruct current State.

### 10.11 Master refresh and reopen retention

**Setup:** Bind `master-A` as Master, then close its Browser session and reopen
it in the same persistent Browser identity context; separately refresh it.

**Expected:** `master-A` regains Race Director context without bootstrap.
`viewer-B` remains Spectator. A Master absence does not trigger reassignment.

### 10.12 Browser Interface/ESP32 restart retains authority

**Setup:** Bind `master-A`; leave `viewer-B` open and synchronised. Persist the
Master binding, then restart the Browser Interface/ESP32 as defined by the
fixture.

**Expected:** On service return, `viewer-B` is still Spectator even if it polls
first. `master-A` restores Race Director context when it reconnects. No
connection order, START request or polling promotes `viewer-B`.

The test records separately that an ESP32 power cycle follows the existing
prototype lifecycle rules for volatile active-race State; this test proves
retained authority identity, not prohibited live-race persistence.

### 10.13 Lost Master identity does not reassign authority

**Setup:** Bind `master-A`, then remove only `master-A`'s local identity
storage while retaining the controller's Master binding.

**Stimulus:** Connect `master-A` as a fresh identity and connect/poll
`viewer-B` and `viewer-C`, including after Browser Interface restart.

**Expected:** No identity automatically becomes Master. All unmatched identities
remain Spectators, the retained binding is not silently replaced, and active
race operation remains unaffected. Later recovery/takeover remains out of
scope.

### 10.14 Reconnect/reload current-State recovery

**Setup:** A race has current authoritative two-lane State; connect a Master
and one Viewer, then interrupt each connection separately.

**Stimulus:** Reload/reconnect each identity with its retained Browser storage.

**Expected:** Each reconstructs current Noticeboard State and reaches
synchronised presentation. The Master restores only its previously retained
context; the Viewer remains viewer. No historical event replay is required.

### 10.15 Browser controls do not define race validity

**Setup:** A Master Browser is present in READY, then in an active lifecycle
State.

**Stimulus:** Submit valid and invalid Master `START_REQUEST`s while varying
Browser local State, rendered controls, refresh timing and request result
retrieval.

**Expected:** Race Control alone decides lifecycle validity. A Master request
is accepted only when current authoritative conditions permit it; invalid
requests are rejected without race change. Browser display/transport State
cannot manufacture State or GO.

### 10.16 Browser Interface is the only Browser Bus participant

**Method:** Structural review, Bus subscription/publisher inspection and
production-path trace for bootstrap, request, result routing and presentation.

**Expected:** Individual Browsers do not become Bus participants. Browser
identity/authority retention and private result routing are owned by the
Browser Interface/Presentation boundary. Race Control sees only trusted logical
Client Context. There is no Browser-to-Race-Control private control route, no
race-state ownership in Browser code, and no identity data embedded in Session
Definition or Noticeboard State.

### 10.17 Stage 1–9 regression

**Method:** Rerun applicable frozen Stage 1–9 campaigns after Stage 10 changes.

**Expected:** All applicable earlier behaviour passes with retained evidence.
Only the previously authorised supersessions may be used:

| Frozen assertion | Limited authorised supersession |
| --- | --- |
| Stage 1 test 1.7 | Historical `DiagnosticProbe` numeric-value expectation only; current permitted delivery behaviour remains required. |
| Stage 6 test 6.13 | Only its explicit prohibition on production Browser `START_REQUEST`/acceptance, introduced by frozen Stage 8. |
| Stage 6 test 6.14 | Only its explicit no-Browser-START scope assertion; its no-Browser-dependency lifecycle behaviour remains required. |

No Stage 10 supersession is proposed. A prior stage's narrower fixture or
statement that multiple-Browser work was deferred is historical scope, not a
regression exception.

### 10.T Deliberate acceptance-harness failure

**Method:** Deliberately replace one expected authority/context, private result
owner, retained Master identity or reconstructed authoritative State with a
wrong expected value.

**Expected:** The evaluator reports FAIL and retains the stimulus, expected and
actual outcome. Restore the correct expectation and confirm PASS.

## 5. Structural and isolation requirements

Evidence must confirm that:

- Browser Interface remains the sole Browser-side Message Bus participant;
- opaque Browser identity values carry no role or authority claim;
- Master binding and Browser-to-result ownership are server-side Presentation /
  Browser Interface State, outside Race Control, Race Engine, Session
  Definition and Noticeboard authoritative race State;
- Race Control receives trusted logical Client Context only and retains normal
  request validity responsibility;
- no connection, polling, request ordering or Browser-visible field can create
  or transfer Master authority;
- a private Request Result is routed only to its originating Browser identity;
- no Browser, presentation backpressure, disconnection or reconnection blocks
  or alters P&P System Time, Input Module, Race Engine, Race Control or
  authoritative Noticeboard maintenance; and
- current State reconstruction uses the established Noticeboard path rather
  than a new event-replay dependency.

## 6. Evidence requirements

For every automated test retain:

- source commit/build identity and frozen-plan revision;
- distinct safe Browser-identity handles and their server-resolved Client
  Contexts (never raw secrets);
- retained Master-store before/after evidence where applicable;
- request, trusted context, Bus, Race Control, Request Result ownership and
  Noticeboard-State evidence as applicable;
- controlled race/input stimulus and resulting authoritative State where a
  race is active;
- restart/disconnect/reconnect evidence; and
- expected, actual and PASS/FAIL outcome.

Retain raw evidence for bootstrap races, rejected spectator claims, colliding
correlation IDs, restart with an already-open spectator, presentation pressure
and the deliberate evaluator failure. The source manifest must include the
production Browser authority/result-routing code and harness.

## 7. Scope discipline and explicit deferrals

Stage 10 implements only the minimum retained one-Master model and
multiple-Browser current-State recovery. It does not implement:

- accounts, passwords, pairing, user identity or security hardening;
- an HTTPS-only, `Secure`-cookie or broader cookie-hardening requirement;
- automatic promotion, election, lease expiry or connection-order authority;
- deliberate Master takeover, relinquishment or user-facing Master-recovery UI;
- persistent race, session, result or history recovery after power loss;
- Browser Race Setup editing;
- new race-control operations beyond existing Stage 8 `START_REQUEST`;
- polished/dedicated Race Director or spectator screens;
- event replay as a current-State recovery protocol;
- Pause/Resume, Results/History, physical outputs or audio; or
- a general multi-client conflict-resolution framework.

## 8. Prepared human Browser checkpoint

This short checkpoint follows `docs/HUMAN_CHECKPOINT_PROCEDURE.md`. It begins
only after the coding agent has built and dry-reviewed a deterministic Stage
10 demonstration from a clean local restart, verified the exact firmware and
`[DEV]` identity, gateway/browser path, two separate Browser identity contexts,
and all initial-state expectations.

The agent prepares a normal Chrome window and an Incognito window (or two
separate Browser profiles). They must be separate identity stores, not two
tabs/windows from one profile. The deterministic demo starts with no retained
Master binding and a valid two-lane proposed Race Setup.

Predetermined manual sequence:

1. In normal Chrome, open the Stage 10 diagnostic Browser, confirm
   `synchronised` authoritative READY State and `No Race Director`, then
   choose the clearly labelled `Make this Browser Race Director` action.
   Expected: this Browser visibly becomes `Race Director`; no race starts
   merely from bootstrap.
2. In Incognito, open the same diagnostic URL. Expected: it synchronises to the
   same current State but visibly says `Spectator` and has no usable START
   control.
3. In normal Chrome, issue the existing START control. Expected: private
   correlated `ACCEPTED` result and authoritative race start.
4. Leave or close normal Chrome while the race is active. Use the prepared
   Wokwi/simulated detector controls to generate the supplied deterministic
   two-lane input sequence; Incognito only observes current authoritative
   State. Expected: the race continues while the Master Browser is absent, and
   Incognito remains unable to control it.
5. Reopen normal Chrome and reload Incognito at their predetermined points.
   Expected: each reconstructs current authoritative State; normal Chrome
   retains Master context and Incognito remains Spectator.

The checkpoint is observational. It does not manually test secret guessing,
header/JSON claims, result-collision attacks, restart races or presentation
pressure; those remain deterministic automated tests.

## 9. Stage gate

Stage 10 is ready for acceptance only when tests 10.1–10.17 and 10.T pass,
structural/isolation review passes, applicable Stage 1–9 regressions have
retained evidence under only the listed earlier supersessions, and the prepared
human Browser checkpoint passes.

Implementation must stop for design review if a test exposes a genuine
architecture or product contradiction. This draft must be reviewed and frozen
before any Stage 10 production implementation begins.






