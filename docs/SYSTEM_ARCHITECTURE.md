# Plonk & Play™ System Architecture

**Status:** Version 1.0  
**Purpose:** Record the architecture agreed so far. This document defines responsibilities and boundaries, not software classes, files, processors, protocols or detailed product implementation.

This document is subordinate to the Design Constitution and System Requirements.

## 1. Architectural approach

Plonk & Play™ separates responsibilities so that hardware, transport, race rules, presentation and storage can evolve independently.

Working principles:

- **Design broadly. Implement narrowly.**
- Engineer in proportion to the product: P&P is a consumer slot-car system, not a safety-critical control system.
- Hardware describes what it **is** and what it **can do**.
- Configuration describes what it is **being used for**.
- Race Control decides what an event **means in the current session**.
- Replace locally. Fail locally.
- New functionality should predominantly require new code, not changes to unrelated existing code.

The architecture must not assume that today's known sensors, outputs, race modes, lane count or optional features are exhaustive.

## 2. High-level responsibilities

The system currently comprises these responsibility areas:

1. Hardware / Device Abstraction
2. Event Mapping
3. Race Control
4. Output Mapping and Hardware Output
5. Presentation and User Interaction
6. Supporting Services:
   - Registry / Discovery
   - Configuration
   - P&P System Time
   - Persistent / History Storage
   - Test / Diagnostics
   - Update / Recovery

These are architectural responsibilities, not a proposed source-code directory structure.

## 3. Devices, capabilities and physical mounting

A **device** is something intelligent/addressable that P&P can discover or communicate with.

A device may expose one or more **capabilities**, for example detector inputs, controllable outputs, local timestamping or other future capabilities.

A **detector** is one particular input capability that can report physical state changes.

Example:

```text
Device ABC123
├── Detector 1
├── Detector 2
├── Detector 3
└── Detector 4
```

A gantry, bridge, bracket or enclosure is normally a physical/mechanical concept, not an architectural module. One physical gantry might contain one detector or eight.

However, physical grouping and location are useful to the person configuring the system. Configuration may therefore retain human-friendly metadata such as **"Main gantry"**, **"Back straight"** or **"Pit box"**.

Race logic must not depend on that physical description.

## 4. Input architecture

### 4.1 Hardware abstraction

Sensor-specific behaviour belongs in the hardware abstraction/adapter layer.

A particular ToF sensor, IR detector or future detection technology is translated into the standard detector event contract without Race Control knowing how the physical detection was performed.

An intelligent remote device may perform this translation locally before transmitting the event.

### 4.2 Detector events

The current provisional detector contract is a state transition with a stable detector identity and timestamp, conceptually:

```text
DETECTOR ID : STATE : TIMESTAMP
```

Typical states are:

- `ACTIVE` — the configured physical detection condition became true.
- `INACTIVE` — the configured physical detection condition became false/re-armed.

The timestamp represents when the physical event was detected as closely as practical, not when a communications packet happened to arrive at Race Control.

Signal conditioning, debounce, hysteresis and re-arming belong close to the detector. The detector/adapter must expose clean `ACTIVE` / `INACTIVE` transitions rather than raw sensor fluctuations. Race meaning does not.

A race-domain plausibility rule such as rejecting an impossibly short lap is not detector debounce or signal conditioning; it belongs to the race/timing logic interpreting otherwise valid clean detector events.

Detector-specific conditioning parameters are owned by the detector/adapter. They need only become P&P installation configuration if the product requires them to be user- or installation-configurable; otherwise they may remain implementation details of that detector/adapter.

### 4.3 Extensible events

The higher-level event boundary must not assume that every future input is necessarily a detector state or lane event.

Conceptually an event can carry:

```text
source
event type
timestamp
event data
```

This allows future event types to be added without redefining unrelated existing components.

### 4.4 Event mapping

Event Mapping translates a stable hardware capability into its configured purpose.

Example:

```text
ABC123:1 : ACTIVE : 123.456
        ↓
Lane 1 : START_FINISH : ACTIVE : 123.456
```

Event Mapping knows the installation/configuration assignment. It does not decide whether the event constitutes a lap, false start, sector time or anything else in the race.

Mapped events may be associated with a lane, another configured context, or the system as a whole; lane identity is not mandatory. For example, a shared physical control could map to `TRACK_CALL : ACTIVE : TIME` without a lane.

The same physical detector can be reassigned to a different role without becoming a different kind of sensor.

### 4.5 Separation of knowledge

The intended separation is:

> **Hardware knows itself.**  
> **Event Mapping knows what it is being used for.**  
> **Race Control knows what that means.**

## 5. Device failure and replacement

The hardware/device layer detects and reports device/capability status to Registry. Registry maintains P&P's current availability information. Neither decides the race consequence of losing a device or capability.

For example:

- loss of an optional sector detector need not stop lap timing;
- loss of a speed trap need not stop a race;
- loss of the only Start/Finish detector may make accurate lap racing impossible.

Race Control decides the consequence according to the active session and available capabilities.

A replacement device identifies itself and its capabilities, not its assumed physical role. Configuration/Discovery may transfer remembered assignments automatically where replacement is unambiguous. Where there is a genuine choice, the user is asked.

## 6. Registry / Discovery

Registry / Discovery answers:

> **What equipment actually exists?**

Discovery and Registry are related but distinct responsibilities:

- **Discovery** finds equipment and learns what it claims to provide.
- **Registry** maintains P&P's current knowledge of that equipment: identity, capabilities, version and availability.

Conceptually:

```text
Physical / remote devices
          ↓
      Discovery
          ↓
       Registry
          ↕
    Configuration
```

Registry describes what exists. Configuration describes what P&P has decided to use it for. Neither should absorb the other's responsibility.

### 6.1 Stable identity

Every intelligent device must have a stable identity that does not change merely because it is unplugged, moved, given a different network address or communicates through a different transport.

Individual capabilities also require stable identities within the device.

For example:

```text
Device ABC123
 ├─ Detector 1
 ├─ Detector 2
 ├─ Detector 3
 └─ Detector 4
```

Configuration can therefore remember an assignment such as:

```text
ABC123:1 → Lane 1 Start/Finish
```

If ABC123 disappears and later returns, its remembered assignments remain available without requiring the customer to configure it again.

Physical movement of the same device does not change its identity. Where P&P cannot determine that the customer has physically repurposed a device, the customer must change the assignment.

### 6.2 Replacement devices

A newly discovered device with similar capabilities to a missing known device must not silently inherit that device's identity.

Where replacement is strongly suggested and unambiguous, P&P may ask a simple human question such as whether the new unit replaces the missing one. Confirmation can then transfer the existing assignments.

Where there is a genuine choice, P&P asks rather than guesses.

### 6.3 Simple directly connected hardware

Not every physical sensor needs its own intelligent identity.

For simple hardware connected directly to a stable controller capability, the connection/capability itself may provide the persistent identity.

For example:

```text
Main Controller
 ├─ Detector Port 1
 └─ Detector Port 2
```

Configuration may remember:

```text
Controller:Detector1 → Lane 1 Start/Finish
Controller:Detector2 → Lane 2 Start/Finish
```

Replacing a simple sensor connected to Detector Port 1 with another compatible sensor therefore need not require reconfiguration.

Principle:

> **Intelligent devices identify themselves. Simple devices are identified by the stable capability/connection through which P&P sees them.**

This distinction must not leak into Race Control; after hardware abstraction and event mapping, both sources produce the same logical mapped events.

### 6.4 Capability-level availability

Availability is not necessarily only a whole-device PRESENT/MISSING property.

Where hardware permits it, Registry can represent availability at capability level. A multi-input device may remain present while one detector/input is unavailable.

For example:

```text
ABC123
  Device: PRESENT
  Detector 1: AVAILABLE
  Detector 2: AVAILABLE
  Detector 3: UNAVAILABLE
  Detector 4: AVAILABLE
```

Registry reports availability. It does not decide the racing consequence.

> **Availability belongs to devices and capabilities. The significance of unavailability belongs to the function currently using them.**

Thus loss of a speed-trap capability need not prevent an otherwise valid race, while loss of a required Start/Finish capability may prevent a particular race configuration from operating normally.

### 6.5 Transport-independent discovery

Different physical/communications transports may require different discovery mechanisms, but they must feed the same logical Registry.

Conceptually:

```text
        WIRED                         WIRELESS
          ↓                              ↓
   transport-specific             transport-specific
      discovery                      discovery
          └──────────────┬───────────────┘
                         ↓
                      REGISTRY
```

A discovered intelligent device should provide, where applicable:

- stable identity;
- available capabilities/resources;
- relevant supported features;
- software/firmware version;
- current availability/status.

The exact discovery protocol is deliberately not specified.

> **Transport discovers a device. Transport does not define the device.**

A device may eventually support more than one transport without becoming two different logical devices.

### 6.6 Power-on reconciliation and readiness

At power-on P&P discovers currently available equipment and reconciles it with remembered Registry/Configuration information.

Normal unchanged installation:

```text
Power on
   ↓
Discover equipment
   ↓
Recognise known devices/capabilities
   ↓
Restore remembered configuration
   ↓
Ready
```

The customer should not be required to reconfirm information P&P already knows.

If an optional capability is absent, P&P should report the reduced capability without unnecessarily preventing unrelated functions.

If a capability required by the requested session is absent, that session cannot truthfully be considered ready, but this does not necessarily make the entire P&P system unusable for other session types.

Therefore:

> **Readiness is capability- and activity-based, not one global READY / NOT READY state.**

When new equipment appears, P&P should infer its use only where the answer is genuinely unambiguous. Where its physical purpose cannot be discovered, the customer is asked only for the missing information.

Overall startup principle:

> **Don't make the customer confirm things P&P already knows. Don't hide things the customer actually needs to know. Ask only about things the machine cannot determine.**

## 7. Configuration

Configuration answers:

> **What have we decided to use this equipment and system for?**

It is distinct from Registry / Discovery and from live Race Control state.

### 7.1 Installation configuration

Persistent installation information includes assignments such as:

```text
Device ABC123 = "Main gantry"
ABC123:1 → Lane 1 : Start/Finish
ABC123:2 → Lane 2 : Start/Finish

Device DEF456 = "Back straight"
DEF456:1 → Lane 1 : Sector 1
DEF456:2 → Lane 2 : Sector 1
```

This survives between races and power cycles.

Installation configuration may also contain physical parameters required to interpret configured functions, such as the distance between detection points forming a speed trap.

### 7.2 Session configuration

Session configuration describes how a particular race/session should operate, for example:

```text
Mode = Lap race
Length = 20 laps
Fuel = On
```

Saved race setups may simply be persistent presets of session configuration.

### 7.3 User / presentation preferences

Preferences such as display choices, units, sounds and similar user choices are distinct from both installation configuration and live race state.

### 7.4 Configuration versus live state

Configuration describes **how P&P should behave**.

Live Race Control state describes **what is happening or has happened as a result**.

For example, current lap, current fuel level, race running/paused state and current position are not Configuration.

### 7.5 Startup reconciliation

At startup P&P compares remembered configuration with currently discovered hardware:

- **known + present** — restore and continue;
- **known + missing** — retain its assignments/configuration but mark it unavailable;
- **new + unambiguous** — infer/restore an appropriate assignment where safe;
- **new + ambiguous** — ask the user.

Principle:

> **Restore what is known. Infer what is unambiguous. Ask only when there is a genuine choice.**

A startup summary may show the interpreted current configuration briefly so the user can verify it or choose to change setup. Exact UI behaviour is not yet specified.

### 7.6 Configuration changes during operation

Configuration changes must be controlled according to their effect on the live system.

Some changes may be safe at any time, some only while idle and some must not change underneath an active race.

Race Control should operate from a defined session configuration rather than repeatedly reading mutable configuration and discovering that its rules have silently changed.

### 7.7 Optional driver/car data

A small persistent list of drivers and/or cars is a possible product feature, particularly for more competitors than available lanes. It is not currently considered a fundamental architectural issue and is not committed as a feature.

## 8. P&P System Time

The System Controller is the authority for the common P&P time domain. P&P System Time defines common instants; it does not require every distributed local clock to display the same numerical value.

Intelligent devices may use local high-resolution clocks. Where timing information must be compared across devices, local timestamps must be reliably relatable or convertible to P&P System Time with sufficient accuracy for the function.

Important input events should be timestamped at or near their source so communications latency does not determine official timing. Local intervals may be measured using a device's own stable clock.

Straightforward synchronisation is preferred where practical. A known clock relationship is also acceptable, including measured offset and drift correction or rechecking where required. Approximate clocks whose effective timing reference is packet arrival are not acceptable.

The specific synchronisation algorithm is deliberately not chosen. This requirement is not justification for unnecessarily elaborate clock-synchronisation machinery.

Across defined P&P boundaries, timestamps must use one defined P&P representation with sufficient resolution for the supported timing functions and sufficient range that representation rollover cannot occur during any supported session. Local hardware clocks may use different native units, widths or rollover behaviour; conversion into or out of the P&P representation belongs at the appropriate boundary. The exact P&P unit and integer representation are selected during implementation design, but must be common at those boundaries.

The same common time domain supports scheduled time-critical outputs. A scheduled action such as GO at a specified P&P time denotes one common instant; a remote module may convert that instant to its local clock and execute locally.

## 9. Race Control

Race Control is the **single authoritative owner** of live race/session state.

Race Control is an architectural authority, not necessarily one monolithic implementation component. Its internal responsibilities may be separated during detailed design provided they continue to present one authoritative race state and preserve the boundaries defined here.

It receives abstract mapped events, session configuration, relevant capability/availability changes and P&P time. It does not depend on sensor models, GPIOs, wireless addresses, physical gantries, browser implementation or storage media.

Its responsibilities currently include:

- session lifecycle/state;
- interpretation of mapped events according to the active rules;
- authoritative race state;
- race-domain calculations;
- decisions about logical race actions;
- generation of race facts/events for consumers;
- production of a deliberately limited result record for history.

### 9.1 Race modes

Different race forms are treated as replaceable rule sets within the common Race Control framework rather than separate hardware/timing systems.

Known modes include:

- lap race;
- timed race;
- practice;
- rally;
- drag racing;
- future modes.

A new mode should ideally add its rules without altering unrelated existing modes.

### 9.2 Optional race features

Features that can operate across multiple race modes should not be buried inside one mode.

Fuel is the current example:

```text
Lap race + fuel
Lap race without fuel
Timed race + fuel
Timed race without fuel
```

Future optional race features may be added where justified without pre-building speculative functionality.

Optional features maintain their own feature state and respond to relevant race events. Where a feature affects the competition, its consequence is applied through Race Control rather than the feature directly controlling hardware or redefining the Race Mode.

P&P is analogue-first. Optional features must not assume that a physical effect can be imposed on a lane or car. For example, simulated fuel can provide display, sound and procedural race behaviour without any physical intervention. Physical enforcement such as removing power from one lane is available only where installed hardware exposes an appropriate capability. A future lane-power or other control module may add that capability without changing the Fuel feature's fundamental role.

### 9.3 Race state and calculations

Race Control owns authoritative live information such as:

- session state;
- current lap;
- lap/sector times;
- positions;
- elapsed time;
- active optional-feature state such as fuel.

An incoming event changes authoritative race state once. Where the relative timing of events affects the authoritative race result, Race Control must use their P&P System Time timestamps rather than communication arrival order. The architecture must permit delayed or out-of-order events to be correctly sequenced where necessary. The detailed buffering or sequencing mechanism is a later subsystem-design decision.

Displays, sound and other consumers must not maintain competing calculations of the race.

Derived calculations such as lap time, sector time, reaction time and speed may remain race-domain calculations unless a future requirement demonstrates a genuine need for a separate measurement-processing responsibility.

### 9.4 Race facts versus actions

Race Control reports **facts/events** independently of how they are presented.

Example:

```text
FASTEST_LAP
lane = 3
time = 5.21
```

A sound system might announce it, a browser might highlight it and history storage might retain it.

Separately, Race Control can request **logical actions**, such as scheduled start lights or track power changes. Output hardware details remain outside Race Control.

## 10. Output architecture

The output path mirrors the input separation:

```text
Race Control
     ↓
Logical action
     ↓
Output Mapping
     ↓
Hardware Abstraction
     ↓
Physical hardware
```

Race Control requests logical outcomes and does not know GPIOs, addresses or device-specific command protocols.

### 10.1 Time-critical outputs

Where timing matters, outputs should be scheduled against P&P System Time rather than relying on command-arrival time.

A start sequence may therefore be transmitted ahead of time with authoritative execution times. A capable local output device can queue and execute those actions against its synchronised clock.

### 10.2 Failure behaviour

Every output that materially affects racing requires a defined behaviour if its controlling device fails.

The precise failure behaviour of particular products, including track-power hardware, is a later hardware/product decision.

### 10.3 Track power

Track power is architecturally supported as a logical output capability.

It only qualifies as a Plonk & Play™ product feature where the physical installation can itself be made genuinely simple, for example through a suitable plug-in power-control accessory rather than requiring the customer to cut and splice track wiring.

## 11. Sound

Sound is an output/presentation capability, not part of Race Control.

Race Control may report a fact such as `FASTEST_LAP`; the sound capability decides how that fact is rendered as speech/audio.

Sound may also be requested as a scheduled logical action where timing matters, such as start-sequence beeps. Such actions use P&P System Time in the same way as other time-critical outputs rather than depending on message-arrival time.

The likely product direction includes pre-recorded audio files on removable storage and sufficiently capable audio hardware for decent trackside sound. Exact hardware and audio format are not yet fixed.

Sound hardware and persistent storage may eventually share physical storage if suitable hardware permits it, but the architecture must not depend on that physical arrangement.

## 12. Presentation and user interaction

Presentation observes authoritative P&P state. It does not create or maintain a competing race state.

A newly connected or reconnected display must be able to obtain the complete current state required to present the session rather than replaying every event it missed.

Multiple clients may simultaneously present different views of the same race, for example:

- full Race Director view;
- individual lane/driver view;
- spectator display;
- future trackside display.

Loss, sleep or disconnection of a browser must not stop the race.

### 12.1 Context-sensitive controls

Race Control determines which commands are valid in its current state. Presentation exposes only controls appropriate to those valid commands.

For example, **Pause** is relevant only once a race is running, while **Resume** is relevant only when paused.

Hiding an invalid control is a usability measure, not the security/validity mechanism: Race Control must still reject commands that are invalid for its current state.

### 12.2 One Master / Race Director

There is one Master control authority at a time: the **Race Director**.

The controller-side Presentation / User Interaction responsibility owns client/session identity, which client currently holds Race Director authority, and deliberate transfer or recovery of that authority. It determines whether a particular client is permitted to submit Race Director commands.

Race Control does not need to know browser, connection or transport details. It receives authorised logical commands and separately determines whether each command is valid in the current race/session state and what consequence it has.

Only the Master may issue authoritative race-control and race-configuration commands such as:

- start;
- pause;
- resume;
- end/abort;
- configure race length/rules.

Connecting another browser must never accidentally create another Race Director.

Master authority belongs to a client/browser session, not to the human identity of the Race Director. The first suitable control client may become Race Director automatically.

A second control client may view the control screen but remains non-Master. It may deliberately choose **Take Control**. If another Race Director is still active, takeover should require an explicit confirmation. Authority then transfers and the former client immediately loses its Master privilege.

This is deliberately a trust model rather than an account/security system. P&P enforces exactly one Master but does not require user accounts, passwords or PINs merely to arbitrate control around a home slot-car track.

Loss, sleep or disconnection of the Race Director client does not affect Race Control or the running race. Another client can deliberately recover Master authority. Changing Race Director changes who may issue control commands; Race Control itself never moves into the browser.

### 12.3 Non-Master clients

Other clients may observe the same or different presentation views but cannot take authoritative race control.

They may submit specifically permitted race inputs/requests. An example is a driver's **CAR OFF / re-call** button.

A lane-associated client can report that its car is off the track without becoming a controller.

The Race Director/session configuration can determine what a **CAR OFF** input means. It may, for example, be disabled, notify/request action from the Race Director or automatically invoke the configured track-call behaviour.

The client always reports the same semantic event — **CAR OFF** — rather than directly commanding a pause or power change. Race Control interprets the event according to the active session configuration.

### 12.4 Requests and browser synchronisation

User interfaces send logical **requests**, not direct mutations of authoritative P&P variables. A request asks the appropriate authoritative responsibility to perform an action or change configuration. The request is accepted or rejected, with a reason where useful; the browser must not assume that submitting a request means the requested change occurred.

Physical controls enter through the appropriate hardware/input path. Depending on the device, that path may map a simple physical detector state into a semantic race input, or an intelligent device may report an appropriate standard semantic input directly.

Both physical controls and user interfaces ultimately present the appropriate P&P responsibility with defined semantic inputs or requests which are validated and interpreted according to current state and configuration. Race Director requests such as START, PAUSE and RESUME are validated against current state. Other permitted inputs, such as CAR OFF or TRACK_CALL_REQUEST, are interpreted according to session rules/configuration and may result in an authoritative action.

Browser-facing communication distinguishes five semantic kinds of information:

- **Request** — browser to P&P: asks P&P to do something.
- **Request Result** — P&P to the requesting browser: reports whether that request was accepted or rejected, with a reason where useful.
- **State** — authoritative information maintained by P&P describing what is true now.
- **State-change notification** — P&P to browser: indicates that authoritative State has changed and the browser's previously obtained State may now be stale.
- **Event/Fact** — P&P to browser: notification that something has happened, useful for presentation such as animation, sound or temporary messages.

P&P owns and maintains authoritative live State. Browsers decide which State they need for the view they are presenting; P&P does not need knowledge of individual browser screens or to construct a screen-specific authoritative State model for each connected browser.

On initial connection or reconnection, a browser obtains authoritative State sufficient to construct the correct display from nothing. It need not replay every event that occurred before or while it was disconnected.

Every meaningful authoritative State change causes a State-change notification. A notification need not identify the fields that changed or contain their new values; its purpose is to tell a browser that State it previously obtained may now be stale.

On receiving such a notification, a browser refreshes the authoritative State needed by its current view. Further State-change notifications received while that refresh is already in progress do not require parallel refreshes. The refresh process must leave the browser with an internally consistent current version of the relevant authoritative State, including any further changes that occurred while an earlier refresh was being initiated or completed.

Rapid authoritative State changes therefore do not require a browser to observe or render every intermediate State. Superseded intermediate State may be skipped; the browser ultimately renders what is currently true. Where individual occurrences themselves matter for presentation, their Events/Facts remain distinct from current State.

A browser may retain previously obtained State while it remains synchronised and no State-change notification has been received. If connection/synchronisation is lost, it must no longer assume that cached State is current. Recovery requires obtaining current authoritative State again.

A browser may issue operational or configuration Requests only while it is synchronised with current authoritative P&P State. If synchronisation is lost, browser controls that would issue such Requests remain unavailable until full synchronisation has completed again. This client-side restriction does not replace P&P validation: P&P remains responsible for validating every Request against current authoritative State and control authority before accepting it.

A missed Event/Fact must never leave a browser permanently wrong. Events/Facts are transient aids for timely presentation such as sounds, flashes, animations or temporary messages; ordinary live Events/Facts do not require guaranteed delivery, acknowledgement, queuing or replay to a browser. If information must remain valid after the moment has passed, it must be represented in authoritative State independently of any Event/Fact. Thus an Event/Fact such as LAP_COMPLETED or NEW_FASTEST_LAP may enhance immediate presentation, while the resulting lap count, last/best lap or current overall fastest lap remain authoritative State.

Persistent results/history are separate from browser Event/Fact delivery. Information that P&P must retain as a lasting record is stored by P&P according to the results/history model and does not depend on any browser having received a live Event/Fact.

Timing-critical physical event capture, ordering, race interpretation and authoritative State maintenance must never wait for browser reads, browser rendering, network delivery or other presentation activity. Slow or disconnected clients must not delay or compromise race timing or authoritative race processing.

The implementation used to identify consistent State versions, determine what must be refreshed, combine rapid changes, and transport notifications or State remains an implementation/prototyping decision. Possible mechanisms such as revisions, dirty flags, logical State blocks, deltas or broader snapshots are not prescribed by this architecture.

For time-critical coordinated actions, current State alone is not sufficient. Once a START Request has been accepted, P&P determines the authoritative future **GO instant** using its own high-resolution timing clock. Connected presentation/output devices are given sufficient advance information to schedule their local start-sequence presentation against that same authoritative GO instant rather than waiting for a network message sent at GO.

This allows browser lights, local displays, sounds and other outputs to appear to reach GO together despite ordinary communication latency. P&P itself judges detector events, reaction timing and false starts against the same authoritative GO instant. Presentation latency must therefore not redefine when GO actually occurred.

The mechanism used to relate a browser or output device's local scheduling to P&P's authoritative timing clock is an implementation/prototyping decision. The architectural requirement is common scheduling against a P&P-dictated future instant, not a particular clock-synchronisation protocol.

### 12.5 Browser clock/display updates

P&P remains authoritative for race timing. Timing calculations use the required high-resolution P&P timing representation independently of the coarser resolution chosen for normal display.

Where a normal running clock or countdown is displayed in whole seconds, P&P sends the browser an authoritative time/state update once per displayed second while that value is changing. A meaningful state change or event between those regular ticks causes an immediate appropriate update rather than waiting for the next one-second tick.

Thus display update cadence does not determine timing accuracy: a detector event may be timestamped and processed at high resolution even though the ordinary running clock changes only once per second.

The browser need not run an independent authoritative race clock. This keeps multiple displays tied to P&P's authoritative timing while avoiding unnecessary continuous browser updates.

Separately from race timing, P&P may maintain a low-stakes human-readable clock for browser display and connection indication. P&P does not require an RTC, Internet time or manually entered wall-clock time for racing. When a browser connects, it may supply its own approximate local time to initialise this display clock. Accuracy to real-world time is not important to race operation.

Once initialised, P&P maintains this display clock and sends its current value to connected browsers once per second. Browsers display the P&P-supplied value rather than independently advancing their own copy. These regular clock updates therefore also provide a simple connection/health indication without requiring a separate routine heartbeat.

A single missed update need not imply failure. If updates cease beyond an implementation-defined tolerance, the browser treats itself as unsynchronised, freezes rather than locally advancing the P&P display clock, and indicates connection loss. Recovery requires a fresh full authoritative state snapshot before normal delta processing resumes.

The display clock is not an authoritative source for lap times, elapsed/remaining race timing, reaction times, scheduled race actions or other competition timing. Those remain based on P&P's high-resolution monotonic/system timing. History need not retain wall-clock/date information merely because the display clock exists; retained race timing concerns durations and results.

The transport, protocol and payload representation used to carry Requests, Request Results, State and Events/Facts remain implementation decisions.

## 13. Persistent / History Storage

Persistent / History Storage is a supporting responsibility distinct from Configuration and live Race Control state.

It provides somewhere to retain useful race/session information without requiring Race Control to know whether the implementation is SD card, flash, database, files or something else.

The initial product should retain a deliberately modest useful result/history record rather than logging everything simply because storage is available.

Possible basic retained information includes:

- date/time;
- race type/length;
- competitors or lanes where applicable;
- result/finishing order;
- lap count;
- fastest lap;
- potentially individual lap times if later judged worthwhile.

The exact initial record is not yet fixed.

The storage model must be extensible so that substantially richer information can be retained later without redesigning Race Control or making older records unusable.

## 13.1 Information ownership and delivery

P&P may distribute information widely, but it must not distribute ownership. For each authoritative fact or state there is one owner and one truth. Other components may hold views, snapshots or derived presentation data without becoming another authority.

A component publishes and receives information only through its defined boundaries. Publication means information has been made available; it does not prove every consumer received it and does not by itself imply persistence.

Facts/events describe what happened. Current state describes what is true now. Persistent information describes what must remain available across the relevant lifecycle. A recovering consumer resynchronises from the authoritative current state it needs and then consumes new relevant facts/events; it does not require replay of every earlier transient event.

Where another component's resulting state matters, message delivery alone is not proof that the requested action occurred. The resulting state/status must be available through that component's boundary where the function requires it.

A physical output has one authoritative owner of commanded state. Other functions provide information or request action through boundaries; they do not independently drive it.

A component receives only the information required for its responsibility. The existence of complete authoritative P&P state does not justify a universal mutable state object shared across unrelated components.

## 13.2 Event ordering, duplication and availability

Where authoritative timestamps establish order, P&P uses those timestamps rather than communication arrival order. Where timestamps genuinely do not establish an order, arrival order must not manufacture one; the relevant race/mode rules determine how simultaneous events are treated.

A single source event must not affect authoritative state more than once. The mechanism is an implementation decision and should be placed at the lowest sensible boundary rather than forcing Race Engine to compensate for avoidable transport duplication.

There is no universal age at which a delayed event becomes invalid. Its timestamp is preserved and the responsibility owning its meaning decides whether it remains relevant. Where capability availability changes affect event interpretation, those changes are related to P&P System Time so that an event is interpreted against the state applicable at its event time rather than merely the state when it arrived.

Capability availability is authoritative at the boundary of the responsibility that owns or monitors that capability. That responsibility may use acknowledgements, heartbeat, timeout, self-test, connection state or another suitable mechanism internally. Race Control consumes the functional status needed for race decisions, not routine low-level communications or detailed diagnostics.

Communication status, capability status, event publication/delivery and authoritative processing are distinct. Successful communication is not proof of downstream physical operation or successful event processing unless the relevant boundary explicitly provides that information.

Loss of a capability invalidates any stale claim that its last reported physical state is still current. On recovery it establishes current state afresh and is not operationally available until required initialisation, compatibility and timing conditions are satisfied.

## 13.3 Identity and low-level addressing

Anything P&P needs to distinguish must have unambiguous stable identity within P&P. Human-readable names are labels, not identities.

Intelligent devices use stable identity independent of transport or temporary communications addresses. Simple devices that cannot identify themselves may instead be represented by the stable System Controller capability/connection through which they are used.

Low-level addresses assigned during hardware initialisation are hardware/device-adapter details and must not become P&P identity. Identical I2C sensors, for example, may receive temporary per-boot bus addresses while their stable P&P meaning remains attached to controller detector ports/capabilities.

Assignments belong to configuration, not to a temporary address. A confirmed replacement intelligent device retains its own identity while configuration may transfer the previous assignment to it.

P&P restores and discovers what the hardware can actually establish. It must not pretend to detect an otherwise unobservable physical swap of indistinguishable simple devices.

## 13.4 System Controller and communication topology

The **System Controller (SC)** is the central P&P controller platform and authority for system decisions, permissions and coordination. Race Control is a distinct responsibility within the SC, not another name for the SC.

Mapped information should be delivered to the responsibility that owns its meaning. Routine mapped competition events need not be mechanically forwarded through Race Control merely because Race Control owns session lifecycle.

Direct module-to-module communication is an allowed future design option, not the default pattern. It may be used where it provides genuine technical benefit, provided it does not create a second source of authority, bypass required decision-making or make authoritative P&P state unknowable to the System Controller.

## 13.5 Manual User Gateway (MUG)

A **Manual User Gateway (MUG)** is a phone, tablet, computer or other browser host acting as a human-facing P&P interface.

Multiple MUGs may be connected simultaneously. One MUG may hold Race Director authority while others operate as permitted driver, display or spectator clients. A MUG does not own race state and its disconnection must not stop the race.

MUG is internal/technical vocabulary; customer-facing interfaces may use ordinary terms such as Race Director, driver or display.

## 14. Test and diagnostics

Testability is a permanent architectural responsibility.

Real wired detectors, wireless devices, future devices and simulated inputs should feed the same standard interfaces.

Testing should support, as appropriate:

- manual event generation;
- scripted races;
- synthetic devices/capabilities/events;
- delayed or out-of-order communication;
- clock drift;
- missing/disconnected devices;
- replacement devices;
- unknown capabilities/events.

Test paths must not create a separate version of Race Control.

A key extensibility test is:

> **When something new is introduced, how far does the required change propagate?**

Unknown or unsupported optional capabilities should fail locally and must not prevent unaffected functions from operating.

## 15. Failure and recovery

Failure handling is based on the **function/capability lost**, not merely the component that failed.

A fault should propagate only as far as the functions that depend upon it. Loss of an optional sector detector, display or sound capability should not unnecessarily stop unaffected racing functions. Loss of a capability essential to the active session may require the session to pause, stop or otherwise be treated as degraded.

Registry reports availability; Race Control determines the consequence for the current session.

When a failed component returns, technical reconnection does not imply that information missed during its absence can be reconstructed. A returning component rejoins from authoritative current P&P state rather than blindly resuming stale pre-failure state.

Recovery engineering must remain proportionate to the product. P&P is not required to reconstruct a live race after every conceivable controller crash or power failure. Persistent configuration should survive and the system should restart cleanly; detailed live-race crash recovery is a later product decision and should only be implemented where its benefit justifies the complexity.

## 16. Updates and compatibility

The P&P Controller is the customer-facing update authority.

Controller firmware should support customer-initiated OTA updating through the normal P&P interface when the customer chooses to provide Internet access. Ordinary racing must remain independent of Internet availability.

The controller update mechanism must be recoverable: installing a replacement must not deliberately destroy the only known-working firmware before the new firmware has been successfully installed and validated. The exact mechanism is an implementation decision.

Intelligent modules expose sufficient version and capability information for P&P to determine compatibility.

The architecture permits intelligent modules to be updated through the P&P controller where appropriate, without requiring the customer to connect each module to development tools or separately configure it for Internet access. **Remote updating is optional for a module**; simple or inexpensive modules are not required to implement OTA merely to qualify as P&P-compatible.

An incompatible or outdated optional component should be identified clearly without unnecessarily preventing compatible functions from operating.

## 17. Detailed design status

The Race Control / Race Engine responsibility question has been resolved in `RACE_CONTROL_ENGINE_DESIGN.md`: Race Control owns session operation/lifecycle, while Race Engine owns competition interpretation, state, rules and calculations. Together they preserve one authoritative P&P race/session authority without duplicate ownership.

The final architecture review also confirmed that components exchange information only through their defined boundaries, authoritative facts can be consumed independently without making consumers authoritative, Presentation never owns authoritative P&P data, and current-state consumers can resynchronise from authoritative state rather than reconstructing it from missed live events.

Detailed persistence/history records and detailed race-rule/product behaviour belong to later subsystem and product design rather than being prerequisites for architectural completion.

Implementation choices deliberately remain outside this document at this stage, including:

- ESP32 model;
- sensor model;
- connectors and pinouts;
- transport/protocol details;
- clock-synchronisation algorithm;
- storage technology;
- web framework;
- packet formats;
- C++ classes;
- source-tree structure;
- detailed UI;
- exact audio hardware.

---

**Governing question:**  
**Does this make the system more Plonk & Play™ for the customer, or less?**
