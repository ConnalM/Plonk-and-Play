# Acceptance Tests — Stage 7 Browser Connection

**Status:** FROZEN before Stage 7 implementation

## 1. Purpose

Stage 7 proves the Browser presentation boundary defined by the current P&P architecture.

The Stage 7 Browser can connect to P&P, obtain and present authoritative current Noticeboard State, respond to `NOTICEBOARD_CHANGED`, and receive relevant Events/Facts without owning or affecting authoritative race operation.

Stage 7 does **not** implement Browser operational Requests. In particular, Browser START is Stage 8.

## 2. Governing behaviour

These tests are derived from the current committed P&P architecture and first-implementation sequence.

For Stage 7:

- Race Control and Race Engine remain the authoritative race/session responsibilities.
- The Noticeboard is the authoritative current externally presentable view of P&P; it is information/state, not another operational owner.
- A newly connected Browser obtains sufficient current authoritative State to construct its display from nothing.
- A material Noticeboard change can produce `NOTICEBOARD_CHANGED`; the notification tells the Browser to refresh and does not itself carry replacement State.
- Events/Facts remain distinct from current State.
- Missing an ordinary transient Event/Fact must not leave a Browser permanently wrong.
- Browser disconnection, rendering or network delay must not affect authoritative race timing or state.
- Stage 7 may choose a suitable prototype Browser transport/representation. The architecture does not prescribe WebSocket or another representation.

## 3. Stage 7 test configuration

Use the accepted Stage 6 one-lane fixed-lap race as the authoritative race path.

The Browser display for acceptance may be deliberately plain. It must expose enough information for the tests to observe connection/synchronisation and the current race state. At minimum the acceptance presentation shall make observable:

- connection/synchronisation status;
- current session lifecycle;
- relevant Race Entry identity;
- lap count;
- last lap time when one exists.

The Browser may additionally present relevant Events/Facts and scheduled GO information.

## 4. Acceptance tests

### 7.1 Browser interface connects

**Stimulus:** Start P&P and connect the Stage 7 Browser client through the selected prototype Browser transport.

**Expected evidence:** The Browser interface establishes a usable connection and the Browser reaches a synchronised state.

### 7.2 Initial synchronisation constructs the display from current State

**Stimulus:** Connect a Browser with no previously cached P&P State.

**Expected evidence:** It obtains sufficient authoritative Noticeboard State to construct the required Stage 7 display without requiring replay of earlier Events/Facts.

### 7.3 Race Control lifecycle State is presented

**Stimulus:** Observe the Browser while authoritative lifecycle State changes through the accepted Stage 6 race path.

**Expected evidence:** The Browser presents the current authoritative lifecycle State, including the relevant READY/STARTING/RACING/FINISHED progression, without becoming the owner of that State.

### 7.4 Race Engine competition State is presented

**Stimulus:** Generate accepted detector crossings during the race.

**Expected evidence:** The Browser presents authoritative competition State including Race Entry identity, lap count and last lap time.

### 7.5 Browser does not own authoritative race State

**Stimulus:** Exercise the Stage 7 Browser presentation while the race runs.

**Expected evidence:** Race Control and Race Engine remain the owners of lifecycle and competition behaviour. Browser-side display/cache State cannot independently change authoritative race operation.

### 7.6 Material State change produces NOTICEBOARD_CHANGED

**Stimulus:** Cause a material change to authoritative externally presentable State.

**Expected evidence:** The Browser interface can receive a standard `NOTICEBOARD_CHANGED` notification associated with the material change.

### 7.7 NOTICEBOARD_CHANGED is notification, not replacement State

**Stimulus:** Observe the Browser-facing `NOTICEBOARD_CHANGED` representation.

**Expected evidence:** The notification does not carry a replacement copy of the Noticeboard or changed authoritative State. It means that current State should be obtained again.

### 7.8 Browser refresh converges on current authoritative State

**Stimulus:** After `NOTICEBOARD_CHANGED`, allow the Browser to refresh the State needed by its current view, including a case where authoritative State changes again during or before completion of the refresh where practical in the harness.

**Expected evidence:** The Browser ends synchronised with current authoritative State. It is not required to render every superseded intermediate State.

### 7.9 Events/Facts are distinct from Noticeboard State

**Stimulus:** Complete a lap that produces an authoritative `LAP_COMPLETED` Fact/Event and resulting competition State.

**Expected evidence:** The Browser-facing boundary can present/observe the individual Fact/Event separately from obtaining the resulting current Noticeboard State.

### 7.10 Missing a transient Event/Fact cannot leave the Browser permanently wrong

**Stimulus:** Arrange for a Browser not to consume one transient lap Event/Fact, then obtain current Noticeboard State.

**Expected evidence:** The Browser obtains the correct current lap-related State without requiring replay of the missed Event/Fact.

### 7.11 Scheduled GO can be presented without Browser timing authority

**Stimulus:** Run the accepted Stage 6 start procedure and expose the authoritative scheduled GO information to the Browser presentation boundary.

**Expected evidence:** The Browser can present/use the scheduled GO information, while the authoritative GO instant and race timing remain defined by P&P rather than Browser arrival/render time.

### 7.12 Browser disconnection does not affect the race

**Stimulus:** Disconnect the Browser while an authoritative race is active and continue the race through detector input.

**Expected evidence:** Race Control/Race Engine continue correctly, including authoritative timing and competition progress, without waiting for or depending on the Browser.

### 7.13 Late connection obtains what is true now

**Stimulus:** Start and progress a race without the Browser connected, then connect a fresh Browser after authoritative State has changed.

**Expected evidence:** The new Browser obtains sufficient current authoritative State to display the race correctly without replaying all earlier Events/Facts.

### 7.14 Browser/network work does not block timing-critical race processing

**Stimulus:** Exercise slow, delayed or absent Browser consumption while detector events and race processing continue.

**Expected evidence:** Authoritative event timing, lap interpretation and race progression do not wait for Browser reads, rendering or network delivery.

### 7.15 No Browser operational Requests are implemented in Stage 7

**Stimulus:** Inspect and exercise the Stage 7 Browser boundary.

**Expected evidence:** There is no production Browser START Request path or other newly implemented Browser operational Request path. Race initiation for Stage 7 acceptance uses controlled test/demonstration scaffolding around the accepted Race Control path. Browser Requests remain Stage 8 scope.

### 7.16 Browser presentation State is not a competing authoritative race model

**Stimulus:** Inspect the Browser implementation and exercise synchronisation/reconnection behaviour.

**Expected evidence:** Browser-held State is presentation/cache State only. Current truth is recovered from the P&P Noticeboard boundary; Browser state is not used as an authoritative source for Race Control or Race Engine.

### 7.T Acceptance harness deliberate-failure check

**Stimulus:** Run the Stage 7 acceptance evaluator against deliberately incorrect Browser-observed State/evidence.

**Expected evidence:** The evaluator reports failure and returns a failing result. The deliberate failure is retained separately from the passing evidence and does not alter production behaviour.

## 5. Excluded from Stage 7

Stage 7 does not require:

- Browser START or other operational Requests;
- Request Results;
- creation of the Session Definition from a Browser Request;
- Race Director authority/takeover workflow;
- multiple simultaneous rendered Browser clients;
- reconnect/recovery campaign beyond the Stage 7 current-State principles above;
- two-lane or rapid-event stress testing;
- polished customer UI;
- final responsive layouts;
- Screen Designer;
- Pause/Resume;
- History/persistent results;
- physical outputs.

Those belong to later implementation stages unless a Stage 7 implementation detail is necessary only to support the Browser connection boundary.

## 6. Evidence requirements

Retain sufficient evidence to identify and reproduce the accepted Stage 7 campaign, including:

- project/repository identity;
- exact source commit/build identity used by the campaign;
- exact frozen Stage 7 acceptance specification commit;
- platform/environment identity;
- build output;
- raw authoritative P&P/Browser-boundary observations used by the evaluator;
- evaluator results for 7.1–7.16;
- separate deliberate-failure evidence for 7.T;
- a source manifest sufficient to establish which accepted source/harness files produced the retained evidence.

Passing tests alone do not complete the stage. Production-code review remains part of the Stage 7 gate.

## 7. Human checkpoint

Before Stage 7 is accepted and committed, the user shall be shown a real Browser presentation connected to the simulated P&P system.

A plain diagnostic presentation is sufficient. The user should be able to observe a race progressing through the Browser, including current lifecycle and lap information, and see the display reach FINISHED.

This human checkpoint supplements rather than replaces automated acceptance evidence.

## 8. Freeze rule

Once this specification is committed as the frozen Stage 7 acceptance definition, Stage 7 implementation may begin.

The implementation side may add unit or diagnostic tests but must not alter these acceptance criteria to make an implementation pass. If implementation exposes a genuine specification problem, stop and return the issue for design decision rather than silently changing this file.
