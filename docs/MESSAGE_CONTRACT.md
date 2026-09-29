# P&P Message Contract

**Status:** Draft v0.2  
**Purpose:** Record the generic communication rules and the first concrete Message Types agreed for the common P&P communications bus ("Pavlov Bus"). This document defines message semantics and authority, not packet encoding, C++ structures or transport protocols.

This document is subordinate to the Design Constitution, System Requirements and System Architecture.

## 1. Definition

A **P&P Message** is communication sent by one authorised P&P component over the common P&P communications bus for consumption by one or more permitted P&P components.

The bus provides communication. It does not grant authority.

For every defined Message Type, its contract must state:

- what the message means;
- the payload required by that meaning;
- which component(s) may publish it;
- which component(s) may consume it;
- any timing or delivery semantics required by that Message Type.

## 2. Common message envelope

The generic conceptual envelope is:

- **Message Type** — identifies the defined message contract.
- **Source** — the originating P&P component instance.
- **Relevant Time** — where the message meaning requires time, the P&P System Time at which the event happened or the scheduled action is to become authoritative.
- **Payload** — only the information required by that Message Type.
- **Correlation ID** — used by Requests and their Request Results; may be used elsewhere only where a defined contract requires it.
- **Message/Event ID** — optional; used where a Message Type requires duplicate detection or another explicit identity requirement.

The exact binary/text representation is deliberately not specified.

A message must not duplicate authoritative State or configuration merely for convenience.

## 3. Message classes

The current semantic classes are:

1. **Request** — asks the owning P&P responsibility to do something.
2. **Request Result** — reports that a Request was accepted or rejected, including a useful reason where rejected.
3. **Input Event** — reports a clean physical/system input trigger.
4. **Fact/Event** — reports something authoritative P&P has determined happened.
5. **Action** — requires an authorised P&P component/device to perform a defined action.
6. **Noticeboard-change notification** — indicates that something material on the authoritative Noticeboard has changed.

A Noticeboard-change notification is a notification to look again at current truth; it is not a replacement copy of that truth.

## 4. Publication, subscription and authority

Messages are normally **published rather than individually addressed**.

A component subscribes to the Message Types it needs. Subscriptions determine ordinary delivery; the Message Contract determines permission and authority.

Connection to the bus or subscription to a Message Type does not authorise a component to originate that message.

The bus must not require publishers to know every consumer. Adding a new permitted consumer should not require an existing publisher to send a special additional copy.

## 5. Source identity

Source identifies the P&P component instance that originated the message sufficiently to establish provenance and apply the Message Contract.

A Transport Adapter preserves the originating Source; it does not replace the Source merely because it carried the message.

Transport changes how a message travels, not what it means or who originated it.

## 6. Time semantics

Where a Message Type has race-relevant time, **Relevant Time means the time relevant to the message's meaning**, not the time at which the message happened to arrive at a consumer.

Examples:

- an Input Event carries the time the clean physical trigger occurred;
- a scheduled GO carries the future P&P System Time at which GO becomes authoritative.

Transmission, receive and diagnostic timestamps may exist as implementation metadata but must not redefine authoritative race timing.

## 7. Input events and device meaning

Input Devices are components of the Input Module. Their source-specific work is internal to that module and is not a separate Pavlov exchange merely because a particular physical implementation is remote.

For the first detector-style input contract, the Input Device owns detection, filtering, debounce/hysteresis, clearing and re-arming. Once it has recognised a clean trigger, the Input Module publishes an Input Event identifying:

- the stable input/device capability that produced the trigger;
- the Relevant Time at which that trigger occurred.

The bus does not require a subsequent INACTIVE/clear event merely to tell the Race Engine that the detector has re-armed. Re-arming remains an Input Device responsibility. A future input type may define different semantics if it genuinely requires them.

The Input Module does **not** publish lane, Start/Finish, sector, drag, speed-trap or MUG meaning.

The active Session Definition freezes the role assigned to each input capability required by that session. The Race Engine interprets the physical Input Event against that fixed Session Definition.

Therefore the same Input Module message can acquire different competition meaning in different sessions without changing the Input Module.

## 8. Output actions and device meaning

Output Devices are owned by the Output Module/device boundary and remain ignorant of race mode, lane and MUG meaning.

For an active session, the relevant race/session responsibility uses the fixed Session Definition to resolve a semantic requirement to the stable output capability assigned that role. The resulting device-level logical Action is published on the bus.

The Output Module/Output Device carries out that action using its hardware-specific implementation. It does not need to know why the output is being operated.

The first exact OUTPUT_ACTION payload is deliberately deferred until the output prototype stage requires it.

## 9. Browser clients and Human Interfaces

The Browser interface is the P&P bus participant. Individual connected browsers are clients behind that interface rather than independent full Pavlov participants.

Each connected client has trusted server-side **Client Context**, for example Race Director/SMUG, a particular current Race Entry/MUG, or spectator.

Client Context controls:

- which authoritative Noticeboard information that client may obtain;
- which events are relevant to that client;
- which Requests that client is permitted to make.

A browser must not gain authority merely by claiming an identity or role in its Request.

For a Request, two checks are distinct:

1. **Permission** — does the trusted Client Context permit this client to make the Request?
2. **Validity** — does the P&P responsibility that owns the requested change accept it in the current authoritative state?

A permitted Request may therefore still be rejected as invalid at that time.

State is obtained according to the current client/screen need. Browser clients do not need to receive every piece of browser-displayable State merely because another screen might use it.

The Taster is another Human Interface using the same common P&P Message Contract. It does not have a separate Taster control protocol. It sends only the Requests it supports and consumes only the messages it understands or needs.

## 10. The Noticeboard and the ding

The **Noticeboard** is P&P's authoritative current externally presentable view of itself. It contains everything relevant and current needed to understand P&P or an active session, plus any current information that P&P makes available to a Browser, Taster or other presentation interface. It is information/state, not another operational module and not another owner of the underlying information.

Examples include current session lifecycle state, relevant Session Definition information, current lap counts, positions and the current fastest lap. Outside an active race it can also expose current Race Setup, available modes/options, current capability/availability information, saved identities offered for selection and user-facing status/fault information where a presentation interface needs them. If a fastest lap was established several events ago and is still the fastest lap, that remains the current authoritative fastest-lap state on the Noticeboard.

The Noticeboard is not History and does not contain every past event merely because it happened. Nor does it expose private hardware or implementation detail merely because that detail exists. The practical test is: **if a Browser or Taster needs to know something about P&P now, it obtains that current externally presentable information through the Noticeboard rather than rummaging inside owning modules.**

A **ding** is the informal design term for the Pavlov notification that something material on the Noticeboard has changed. The concrete Message Type is **NOTICEBOARD_CHANGED**.

NOTICEBOARD_CHANGED does not carry a replacement copy of the Noticeboard. Interested consumers obtain the current authoritative information they need.

The architecture does **not** require the Noticeboard itself to expose a version number. An implementation may use revision counters, dirty flags, sequence numbers or another internal mechanism if useful, but that mechanism is not part of the conceptual P&P contract unless a later concrete requirement makes it necessary.

A ding is not generated merely because a derived displayed value changes with the passage of time. For example, if authoritative State records the GO/start time and duration, a Browser or Taster can display a counting clock from P&P System Time without P&P changing the Noticeboard every second.

Facts/Events and Noticeboard State remain distinct:

- a Fact/Event says that something happened;
- the Noticeboard says what is true now as a result.

A recovering or newly connected consumer can obtain what is true now from the Noticeboard without replaying every intermediate state change. Where a consumer must react to a particular Fact/Event as it happens, that event uses its own Message Type and delivery semantics.

## 11. Delivery semantics

Different Message Types may require different delivery behaviour. The Message Contract for each type must state what is required rather than relying on one universal reliability rule.

Current categories include:

- **race-critical event** — must not silently disappear where loss would change authoritative competition behaviour;
- **Noticeboard-change notification** — may be combined or superseded because the consumer can look again at current authoritative State;
- **ephemeral presentation** — may be best-effort where loss cannot affect authoritative operation.

Where duplicate delivery could change authoritative state, the Message Type must provide sufficient identity/handling to prevent an accidental duplicate from being interpreted twice.

Transport Adapters must satisfy the required semantics without changing message meaning or authority.

## 12. Priority and scheduling

There is no sender-selected universal Priority field in the generic envelope.

Timing-critical requirements belong to the Message Contract and bus implementation. Browser, storage, audio or other slow/non-critical work must not block timing-critical race processing.

## 13. First concrete Message Types

These are semantic contracts. Names are fixed for the first implementation; exact packet/structure encoding remains deferred.

### 13.1 INPUT_EVENT

**Class:** Input Event  
**Publisher:** Input Module  
**Permitted consumers:** Race Engine; authorised Test/Diagnostics where required.

**Payload:**
- stable Input ID/capability identity.

**Relevant Time:** the P&P System Time at which the clean trigger occurred.

**Meaning:** "This input was triggered at this time."

For the first detector-style input, there is no ACTIVE/INACTIVE field. Clearing and re-arming are internal Input Device work.

The message carries no lane, MUG, Start/Finish, sector, drag, speed-trap or lap meaning. The Race Engine obtains the session meaning from the fixed Session Definition.

### 13.2 START_REQUEST

**Class:** Request  
**Publisher:** authorised Human Interface, including Browser interface or Taster.  
**Consumer:** Race Control.

**Payload:** no Race Setup copy is required.

**Correlation ID:** required.

**Meaning:** "Please start the race described by the current authoritative Race Setup."

Race Control checks permission, validates current Race Setup and required capabilities, creates the immutable Session Definition when the Request is accepted, and begins the start procedure. The requesting interface does not supply its own competing copy of Race Setup.

### 13.3 REQUEST_RESULT

**Class:** Request Result  
**Publisher:** the P&P responsibility that owns/decides the Request.  
**Consumer:** the requesting Human Interface.

**Payload:**
- Correlation ID of the Request;
- result: ACCEPTED or REJECTED;
- useful rejection reason when rejected.

**Meaning:** "This is the acceptance result of the identified Request."

ACCEPTED does not mean that the requested operation has already completed. Subsequent authoritative State and Facts/Events describe what then happens.

The same Request/Request Result mechanism is used by Browser, Taster and future Human Interfaces; simpler interfaces do not require a separate protocol.

### 13.4 GO_SCHEDULED

**Class:** Fact/Event  
**Publisher:** Race Control.  
**Permitted consumers:** Race Engine; Browser interface; Taster; authorised output/presentation responsibilities and Test Harness where required.

**Payload:**
- authoritative future GO time in P&P System Time.

**Meaning:** "GO for the active session occurs at this exact System Time."

This is not "GO now". Consumers can prepare presentation/behaviour in advance against the same authoritative instant. It does not require a Correlation ID merely because START_REQUEST preceded it.

### 13.5 LAP_COMPLETED

**Class:** Fact/Event  
**Publisher:** Race Engine.  
**Permitted consumers:** Race Control where required; Browser interface; Taster; History/Memory responsibility where required; Test Harness.

**Payload:**
- Race Entry ID;
- completed lap number;
- lap time.

**Relevant Time:** P&P System Time at which the lap was completed.

**Meaning:** "The Race Engine has authoritatively determined that this Race Entry completed this lap at this time."

Race Entry ID identifies the competitor entry created as part of the Session Definition. Lane, MUG identity and optional Car identity remain defined by that Race Entry; they are not redundantly copied into every LAP_COMPLETED message.

Position, fastest-lap status, total race State and similar current information are not added to this message merely for display convenience.

### 13.6 COMPETITION_COMPLETE

**Class:** Fact/Event  
**Publisher:** Race Engine.  
**Consumer:** Race Control; other permitted interested consumers may subscribe where useful.

**Payload:** none required for v1.

**Relevant Time:** P&P System Time at which the Race Engine determined that the active competition's completion rules had been satisfied.

**Meaning:** "Under the rules of the active Session Definition, this competition is now complete."

Race Engine reports the fact. Race Control owns the resulting session-lifecycle and operational consequences. Winner/results/final positions remain authoritative competition State rather than being duplicated into this completion message.

### 13.7 NOTICEBOARD_CHANGED

**Class:** Noticeboard-change notification  
**Publisher:** the P&P responsibility through which a material authoritative Noticeboard change is exposed. Exact internal aggregation/notification mechanics are an implementation decision and must not create a new owner of State.  
**Consumers:** Browser interface, Taster and other consumers that need to refresh current authoritative information.

**Payload:** none conceptually required.

**Meaning:** "Something material on the authoritative Noticeboard has changed; look again if you need current State."

This message is the formal equivalent of the design-discussion **ding**. It does not require a Noticeboard version number and does not itself carry the changed State.

## 14. Session Definition v1 information required by these contracts

For the first two-lane Lap Race, the Session Definition is an immutable snapshot created when START is accepted. It contains only information required to interpret and execute that session, rather than a frozen copy of the whole installation.

It includes:

- **Session identity** — stable Session ID.
- **Race rules** — mode, lap/distance target, finish behaviour, enabled race features and relevant start/first-crossing behaviour.
- **Race Entries** — a stable Race Entry ID for each competitor; lane for this session; stable MUG ID plus frozen MUG display name; optional stable Car ID plus frozen Car display name where Cars are enabled.
- **Input roles** — each session-required semantic input role mapped to its stable Input ID/capability.
- **Output roles** — each session-required semantic output role mapped to its stable Output ID/capability.
- **Relevant physical/rule parameters** — only parameters required to interpret or execute this session.

Unused installed inputs/outputs need not appear merely because they exist.

The Session Definition contains session meaning, not hardware implementation details such as GPIO numbers, ESP-NOW addresses, ToF thresholds or Browser connections.

Stable record IDs are retained where a permanent link matters. Display values whose later alteration would change the historical meaning of the race are frozen for the session. Thus renaming a saved MUG later does not retrospectively rename the competitor in an already-run session/result.

> **Modules own the hardware identity and operation of their devices. The Session Definition owns the meaning assigned to those devices for the active session.**

## 15. Implementation boundary

This document deliberately does not yet specify:

- packet encoding;
- C++ structures/classes;
- queue implementation;
- subscription API;
- WebSocket representation;
- ESP-NOW representation;
- retry algorithms;
- sequence-number format;
- internal Noticeboard refresh/revision mechanism;
- Message Types not yet required by the first prototype stages.

Those are defined only when required by implementation, while preserving the semantics above.

**Design broadly. Implement narrowly.**
