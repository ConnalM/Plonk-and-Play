# P&P Message Contract

**Status:** Draft v0.1  
**Purpose:** Record the generic communication rules agreed for the common P&P communications bus ("Pavlov Bus"). This document defines message semantics and authority, not packet encoding, C++ structures or transport protocols.

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
- **Correlation ID** — optional; used where one message must be associated with an earlier Request or conversation.
- **Message/Event ID** — optional; used where a Message Type requires duplicate detection or another explicit identity requirement.

The exact binary/text representation is deliberately not specified.

A message must not duplicate authoritative State or configuration merely for convenience.

## 3. Message classes

The current semantic classes are:

1. **Request** — asks the owning P&P responsibility to do something.
2. **Request Result** — reports that a Request was accepted or rejected, including a useful reason where rejected.
3. **Input Event** — reports a clean physical/system input event.
4. **Fact/Event** — reports something authoritative P&P has determined happened.
5. **Action** — requires an authorised P&P component/device to perform a defined action.
6. **State-change notification** — indicates that previously obtained authoritative State may now be stale.

A State-change notification is a notification to refresh current truth; it is not a replacement copy of authoritative State.

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

- an input event carries the time the clean physical event occurred;
- a scheduled GO carries the future P&P System Time at which GO becomes authoritative.

Transmission, receive and diagnostic timestamps may exist as implementation metadata but must not redefine authoritative race timing.

## 7. Input events and device meaning

Input Devices are components of the Input Module. Their source-specific work is internal to that module and is not a separate Pavlov exchange merely because a particular physical implementation is remote.

The Input Module publishes a clean physical Input Event identifying:

- the stable input/device capability that produced it;
- the clean state/event;
- the Relevant Time.

It does **not** publish lane, Start/Finish, sector, drag, speed-trap or MUG meaning.

The active Session Definition freezes the role assigned to each input capability required by that session. The Race Engine interprets the physical Input Event against that fixed Session Definition.

Therefore the same Input Module message can acquire different competition meaning in different sessions without changing the Input Module.

## 8. Output actions and device meaning

Output Devices are owned by the Output Module/device boundary and remain ignorant of race mode, lane and MUG meaning.

For an active session, the relevant race/session responsibility uses the fixed Session Definition to resolve a semantic requirement to the stable output capability assigned that role. The resulting device-level logical Action is published on the bus.

The Output Module/Output Device carries out that action using its hardware-specific implementation. It does not need to know why the output is being operated.

## 9. Browser clients

The Browser interface is the P&P bus participant. Individual connected browsers are clients behind that interface rather than independent full Pavlov participants.

Each connected client has trusted server-side **Client Context**, for example Race Director/SMUG, a particular current Race Entry/MUG, or spectator.

Client Context controls:

- which authoritative State that client may obtain;
- which notifications/events are relevant to that client;
- which Requests that client is permitted to make.

A browser must not gain authority merely by claiming an identity or role in its Request.

For a Request, two checks are distinct:

1. **Permission** — does the trusted Client Context permit this client to make the Request?
2. **Validity** — does the P&P responsibility that owns the requested change accept it in the current authoritative state?

A permitted Request may therefore still be rejected as invalid at that time.

State is obtained according to the current client/screen need. Browser clients do not need to receive every piece of browser-displayable State merely because another screen might use it.

## 10. Delivery semantics

Different Message Types may require different delivery behaviour. The Message Contract for each type must state what is required rather than relying on one universal reliability rule.

Current categories include:

- **race-critical event** — must not silently disappear where loss would change authoritative competition behaviour;
- **current-state notification** — may be combined or superseded because the consumer can refresh the latest authoritative State;
- **ephemeral presentation** — may be best-effort where loss cannot affect authoritative operation.

Where duplicate delivery could change authoritative state, the Message Type must provide sufficient identity/handling to prevent an accidental duplicate from being interpreted twice.

Transport Adapters must satisfy the required semantics without changing message meaning or authority.

## 11. Priority and scheduling

There is no sender-selected universal Priority field in the generic envelope.

Timing-critical requirements belong to the Message Contract and bus implementation. Browser, storage, audio or other slow/non-critical work must not block timing-critical race processing.

## 12. Implementation boundary

This document deliberately does not yet specify:

- packet encoding;
- C++ structures/classes;
- queue implementation;
- subscription API;
- WebSocket representation;
- ESP-NOW representation;
- retry algorithms;
- sequence-number format;
- exact list of Message Types and payload schemas.

Those are defined only when required by the implementation, while preserving the semantics above.

**Design broadly. Implement narrowly.**
