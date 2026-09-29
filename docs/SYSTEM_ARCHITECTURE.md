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
- Race Control owns session operation/lifecycle; the Race Engine interprets competition events and owns competition state, rules and calculations.
- Replace locally. Fail locally.
- New functionality should predominantly require new code, not changes to unrelated existing code.

The architecture must not assume that today's known sensors, outputs, race modes, lane count or optional features are exhaustive.

## 2. High-level responsibilities

The system currently comprises these responsibility areas:

1. Input Module / Input Devices, including application of configured input mapping
2. Race Control
3. Race Engine
4. Output Devices, including application of configured output mapping
5. Presentation and User Interaction
6. Supporting responsibilities/services:
   - Registry / Discovery
   - P&P System Time
   - Memory / Persistence
   - Test / Diagnostics
   - Update / Recovery

Configuration, Race Setup, Session Definition, Registry and authoritative State are information/state rather than extra operational modules merely because other parts of P&P use them. These are architectural responsibilities and information relationships, not a proposed source-code directory structure.

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

### 4.1 Input Module and Input Devices

The **Input Module** is the P&P subsystem boundary through which physical, simulated or test competition input becomes clean, meaningful standard P&P competition Input Events. It may contain any number of **Input Devices** and owns the responsibility for applying the configured installation mapping before events leave the Input Module for race use. The resulting competition Input Events are published onto the common P&P communications bus; the Race Engine consumes the events relevant to its responsibility.

An **Input Device** is one complete working source of physical input. It owns everything specific to that source that is required to produce a clean standard physical Input Event. For a ToF detector this can include sensor reading, thresholds, filtering, debounce/hysteresis, re-arming and device-specific protocol handling. A keyboard or test-harness Input Device performs its own equivalent source-specific handling.

Each Input Device translates its own native behaviour into the standard physical P&P input contract. Adding a new Input Device type must not require a central translator to be modified merely to understand that device's native output.

An intelligent remote device may perform some or all of this source-specific interpretation locally before transmitting the standard physical event. Where an Input Device and the mapping responsibility are separate P&P components, that physical event is communicated through the common bus/appropriate Transport Adapter like other inter-component communication.

The Input Module consumes the relevant clean physical Input Events, applies the current configured mapping held in working RAM, and publishes the resulting meaningful competition Input Events back onto the common bus for race use. The Race Engine therefore does not need to know whether a competition event originated from ToF hardware, a keyboard, a test harness or another future source, which detector identity produced it, or which physical transport carried it.

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

### 4.4 Configured event mapping

Source-specific translation is already complete inside the Input Device. The **configured event mapping is configuration data**, not a separate module or bus participant. It assigns the resulting stable Input Device/capability identity to its installation or racing purpose; it does not translate native device protocols.

The persistent mapping is owned by Memory and loaded into working RAM. The Input Module applies the current working mapping locally; it does not consult persistent Memory or send a Pavlov message for every detector crossing.

The mapping translates a stable input capability into its configured purpose.

Example:

```text
ABC123:1 : ACTIVE : 123.456
        ↓
Lane 1 : START_FINISH : ACTIVE : 123.456
```

The Input Module knows the current installation/configuration assignment through its working mapping. Applying that mapping does not decide whether the resulting event constitutes a lap, false start, sector time or anything else in the race.

Mapped events may be associated with a lane, another configured context, or the system as a whole; lane identity is not mandatory. For example, a shared physical control could map to `TRACK_CALL : ACTIVE : TIME` without a lane.

The same physical detector can be reassigned to a different role without becoming a different kind of sensor.

### 4.5 Transport independence and Transport Adapters

An Input Device is defined by the standard P&P Input Events it produces, not by whether it is local, wired or wireless.

A local detector, a wired remote detector and a wireless intelligent detector may therefore all publish the same standard detector event. Their different communications mechanisms are handled by **Transport Adapters** at the edge of the common P&P communications bus.

A Transport Adapter bridges a physical or software communications mechanism to the logical P&P bus. It may support, for example, an in-process connection, GPIO/I2C-connected hardware, ESP-NOW, Wi-Fi/WebSocket or a future wired remote transport. It has no race meaning and must not reinterpret an Input Event as a lap, false start or other competition fact.

Transport Adapters may be bidirectional where required for discovery, configuration, status, time synchronisation or updates. Remote timestamping must remain reliably related to P&P System Time, and remote-device availability is represented through the normal Discovery/Registry responsibilities.

> **Transport changes how a P&P message travels, not what the message means.**

### 4.6 Separation of knowledge

The intended separation is:

> **Each Input Device knows how to produce a clean standard physical P&P Input Event.**  
> **The Input Module applies the configured working mapping and publishes the resulting meaningful competition Input Event.**  
> **Race Control owns session operation; the Race Engine owns competition interpretation.**

## 5. Device failure and replacement

The hardware/device layer detects and reports device/capability status to Registry. Registry maintains P&P's current availability information. Neither decides the race consequence of losing a device or capability.

For example:

- loss of an optional sector detector need not stop lap timing;
- loss of a speed trap need not stop a race;
- loss of the only Start/Finish detector may make accurate lap racing impossible.

Race Control decides the consequence according to the active session and available capabilities.

A replacement device identifies itself and its capabilities, not its assumed physical role. Configuration/Discovery may transfer remembered assignments automatically where replacement is unambiguous. Where there is a genuine choice, the user is asked.

## 6. Registry and Discovery

Registry and Discovery answer the related question:

> **What equipment actually exists?**

They are deliberately not the same kind of architectural thing:

- **Discovery** is an active responsibility. It finds equipment, learns what it claims to provide, detects relevant availability changes and updates Registry.
- **Registry** is shared authoritative equipment state, not an operational module. It maintains P&P's current knowledge of equipment: identity, capabilities, version and availability.

Registry does not decide the operational significance of the information it holds. Other responsibilities may read Registry information or react to Registry changes through standard P&P communication. For example, loss of a required detector may matter to Race Control, while the same change may also be presented by a human interface or observed by diagnostics.

Where useful, equipment/capability changes may be published on the common P&P communications bus. Publication reports the change; it does not transfer responsibility for deciding its consequence to Registry.

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

This distinction must not leak into Race Control or the Race Engine; after Input Device abstraction and application of the configured mapping by the Input Module, both sources produce the same meaningful competition Input Events.

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

## 7. Configuration, Race Setup and working RAM

Configuration answers:

> **What have we decided to use this equipment and system for?**

Configuration is information, not a standalone operational module. Persistent configuration is owned by Memory. At startup, P&P loads the configuration needed for operation into working RAM; the appropriate modules then use their working configuration locally. They do not turn ordinary local configuration lookups into Pavlov messages or repeatedly consult persistent storage for timing-critical work.

The Pavlov Bus remains the normal route when one P&P component needs to communicate with another. Local access by a module to configuration/state already available to that module in working RAM is not inter-component communication and does not require the bus.

Configuration is distinct from Registry / Discovery and from live Race Control state.

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

### 7.2 Race Setup

Before START, the SMUG edits **Race Setup**: the mutable working choices for the proposed race/session, for example:

```text
Mode = Lap race
Length = 20 laps
Fuel = On
```

Race Setup is working data, not a module. P&P may remember the last useful Race Setup or future named presets in Memory, but the SMUG does not edit a Session Definition directly.

### 7.3 User / presentation preferences

Preferences such as display choices, units, sounds and similar user choices are distinct from both installation configuration and live race state.

### 7.4 Configuration versus live state

Configuration describes **how P&P should behave**.

Live race/session state describes **what is happening or has happened as a result**, with ownership divided between Race Control session state and Race Engine competition state as defined in `RACE_CONTROL_ENGINE_DESIGN.md`.

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

When START is accepted, P&P validates the proposed Race Setup against the applicable installation/capability information and creates the fixed **Session Definition** in working RAM. The Session Definition is immutable working data for that session, not a module or Pavlov participant and not something the SMUG edits directly.

Race Control and the Race Engine operate from that fixed Session Definition rather than repeatedly reading mutable configuration and discovering that operation or rules have silently changed.

### 7.7 Optional driver/car data

A small persistent list of drivers and/or cars is a possible product feature, particularly for more competitors than available lanes. It is not currently considered a fundamental architectural issue and is not committed as a feature.

## 8. P&P System Time

P&P System Time is a **shared service**, not an operational module. It provides the common authoritative time domain used by modules and devices that need to describe, compare or schedule instants.

The common P&P communications bus transports messages that may contain timestamps or scheduled instants; System Time gives those values their common meaning. The bus does not itself establish timing authority.

The System Controller is the authority for the common P&P time domain. P&P System Time defines common instants; it does not require every distributed local clock to display the same numerical value.

Intelligent devices may use local high-resolution clocks. Where timing information must be compared across devices, local timestamps must be reliably relatable or convertible to P&P System Time with sufficient accuracy for the function.

Important input events should be timestamped at or near their source so communications latency does not determine official timing. Local intervals may be measured using a device's own stable clock.

Straightforward synchronisation is preferred where practical. A known clock relationship is also acceptable, including measured offset and drift correction or rechecking where required. Approximate clocks whose effective timing reference is packet arrival are not acceptable.

The specific synchronisation algorithm is deliberately not chosen. This requirement is not justification for unnecessarily elaborate clock-synchronisation machinery.

Across defined P&P boundaries, timestamps must use one defined P&P representation with sufficient resolution for the supported timing functions and sufficient range that representation rollover cannot occur during any supported session. Local hardware clocks may use different native units, widths or rollover behaviour; conversion into or out of the P&P representation belongs at the appropriate boundary. The exact P&P unit and integer representation are selected during implementation design, but must be common at those boundaries.

The same common time domain supports scheduled time-critical outputs. A scheduled action such as GO at a specified P&P time denotes one common instant; a remote module may convert that instant to its local clock and execute locally.

## 9. Race Control

Race Control and the Race Engine together form the **single authoritative race/session authority**, with non-overlapping ownership defined in `RACE_CONTROL_ENGINE_DESIGN.md`.

### 9.0 Normal live communication

The common P&P communications bus is the normal route for communication between P&P components, including communication between Race Control and the Race Engine.

This does **not** create a universal decision-making dispatcher or receptionist. The bus transports standard P&P messages; responsibility and authority remain with the component that owns the meaning.

Examples:

- Presentation clients such as the browser or Taster publish authorised operational **Requests**; the P&P responsibility that owns the requested change consumes the Request. Race Control consumes session-operation Requests such as START, PAUSE and RESUME.
- Input Devices publish clean standard physical Input Events; the Input Module consumes them, applies the configured working mapping and publishes meaningful competition Input Events; the Race Engine consumes the competition events relevant to it.
- Race Control may publish an authoritative scheduled GO; the Race Engine and relevant presentation/output consumers may consume the same publication.
- The Race Engine may publish competition completion; Race Control consumes that fact and applies the session-lifecycle consequence.

Race Control and the Race Engine therefore do not require a privileged private communications path merely because both are contained within the System Controller. Their ownership boundaries remain distinct while their defined inter-component communication uses the same common bus as the rest of P&P.

The **System Controller (SC)** is the enclosing authoritative race-controller platform containing Race Control and the Race Engine. It is not an additional functional module and is not an extra message-routing hop.

Race Control owns session operation and lifecycle, including preparation, start procedure, authoritative GO, Pause/Resume/Stop and coordination of operational logical actions. The Race Engine owns competition interpretation, competition state, rules, calculations, results and competition-completion conditions.

They do not depend on sensor models, GPIOs, wireless addresses, physical gantries, browser implementation or storage media.

### 9.1 Race modes

Different race forms are treated as replaceable Race Engine rule sets within the common Race Control / Race Engine framework rather than separate hardware/timing systems.

Known customer-facing modes include:

- Lap Race;
- Practice;
- Endurance;
- Timed Stage;
- Drag;
- future modes.

A new mode should ideally add its rules without altering unrelated existing modes.

### 9.2 Optional race features

Features that can operate across multiple race modes should not be buried inside one mode.

Fuel is the current example:

```text
Lap Race + fuel
Lap Race without fuel
Endurance + fuel
Endurance without fuel
```

Future optional race features may be added where justified without pre-building speculative functionality.

Optional features maintain their own feature state and respond to relevant race events. Where a feature affects competition state or scoring, its consequence is applied through the Race Engine's defined competition boundary; where it requires an operational action, Race Control coordinates that action. The feature does not directly control hardware or redefine the Race Mode.

P&P is analogue-first. Optional features must not assume that a physical effect can be imposed on a lane or car. For example, simulated fuel can provide display, sound and procedural race behaviour without any physical intervention. Physical enforcement such as removing power from one lane is available only where installed hardware exposes an appropriate capability. A future lane-power or other control module may add that capability without changing the Fuel feature's fundamental role.

### 9.3 Race/session state and calculations

Authoritative live information has one owner according to meaning:

- Race Control owns session lifecycle/operational state such as preparing, starting, running, paused or stopped;
- the Race Engine owns competition state such as current lap, lap/sector times, positions, competition elapsed time, results and competition-affecting feature state.

An incoming competition event changes authoritative competition state once. Where the relative timing of events affects the authoritative competition result, the Race Engine uses their P&P System Time timestamps rather than communication arrival order. The architecture must permit delayed or out-of-order events to be correctly sequenced where necessary. The detailed buffering or sequencing mechanism is an implementation/subsystem-design decision.

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
Race Control / other authorised producer
     ↓
Logical P&P action on the Pavlov Bus
     ↓
Output side applies configured working mapping
     ↓
Output Device hardware abstraction
     ↓
Physical hardware
```

**Output mapping is configuration data, not a separate module.** Its persistent form is owned by Memory and the applicable working mapping is held in RAM. The output side applies that working mapping locally to select/drive the appropriate Output Device; it does not query persistent Memory for every action.

Each Output Device owns the hardware-specific implementation required to carry out its standard P&P action. Race Control and the Race Engine therefore do not know GPIOs, addresses or device-specific command protocols.

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

The authoritative race responsibility may publish a fact such as `FASTEST_LAP`; the sound capability decides how that fact is rendered as speech/audio.

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

Race Control determines which session-operation Requests are valid in its current state. Other authoritative responsibilities validate Requests that belong to them. Presentation exposes only controls appropriate to the currently valid Requests.

For example, **Pause** is relevant only once a race is running, while **Resume** is relevant only when paused.

Hiding an invalid control is a usability measure, not the security/validity mechanism: Race Control must still reject session-operation Requests that are invalid for its current state.

### 12.2 One Master / Race Director

There is one Master control authority at a time: the **Race Director**.

The controller-side Presentation / User Interaction responsibility owns client/session identity, which client currently holds Race Director authority, and deliberate transfer or recovery of that authority. It determines whether a particular client is permitted to submit Race Director Requests.

Race Control does not need to know browser, connection or transport details. It receives authorised logical session-operation Requests and separately determines whether each Request is valid in the current race/session state and what consequence it has.

Only the Master may issue Race Director Requests such as:

- start;
- pause;
- resume;
- end/abort;
- configure race length/rules.

Connecting another browser must never accidentally create another Race Director.

Master authority belongs to a client/browser session, not to the human identity of the Race Director. The first suitable control client may become Race Director automatically.

A second control client may view the control screen but remains non-Master. It may deliberately choose **Take Control**. If another Race Director is still active, takeover should require an explicit confirmation. Authority then transfers and the former client immediately loses its Master privilege.

This is deliberately a trust model rather than an account/security system. P&P enforces exactly one Master but does not require user accounts, passwords or PINs merely to arbitrate control around a home slot-car track.

Loss, sleep or disconnection of the Race Director client does not affect Race Control or the running race. Another client can deliberately recover Master authority. Changing Race Director changes who may issue Race Director Requests; Race Control itself never moves into the browser.

### 12.3 Non-Master clients

Other clients may observe the same or different presentation views but cannot take authoritative race control.

They may submit specifically permitted race inputs/requests. An example is a driver's **CAR OFF / re-call** button.

A lane-associated client can report that its car is off the track without becoming a controller.

The active Session Definition can determine what a **CAR OFF** input means. It may, for example, be disabled, notify/request action from the Race Director or automatically invoke the configured track-call behaviour.

The client always reports the same semantic event — **CAR OFF** — rather than directly commanding a pause or power change. Race Control interprets the event according to the active Session Definition.

### 12.4 Requests and browser synchronisation

User interfaces send logical **requests**, not direct mutations of authoritative P&P variables. A request asks the appropriate authoritative responsibility to perform an action or change configuration. The request is accepted or rejected, with a reason where useful; the browser must not assume that submitting a request means the requested change occurred.

Physical controls enter through the appropriate hardware/input path. Depending on the device, that path may map a simple physical detector state into a semantic race input, or an intelligent device may report an appropriate standard semantic input directly.

Both physical controls and user interfaces ultimately present the appropriate P&P responsibility with defined semantic inputs or requests which are validated and interpreted according to current state and configuration. Race Director requests such as START, PAUSE and RESUME are validated against current state. Other permitted inputs, such as CAR OFF or TRACK_CALL_REQUEST, are interpreted according to the active Session Definition and current authoritative state and may result in an authoritative action.

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

## 13. Memory Module

The **Memory Module** owns information that P&P deliberately retains across power cycles. Its internal storage technology may be flash, SD card, files, a database or another suitable implementation; other modules do not depend on that choice.

Persistent information may include installation/configuration data, remembered setup and preferences, MUG/car records where supported, race/session history, lap data, personal-best summaries and overall track-record summaries.

Memory has three distinct relationships with the rest of P&P:

1. **Persistent information** — Memory owns the persistent copy of installation configuration, preferences, remembered Race Setup, identities, history, records and similar information that must survive power loss.
2. **Working information** — at startup, and when an authorised persistent change is accepted, the applicable information is made available in working RAM to the modules/responsibilities that need it. Normal operation then uses that working data locally. A sensor crossing, output action or race calculation must not require a fresh persistent-storage lookup merely to obtain configuration already loaded for operation.
3. **Session preparation and recording** — the proposed Race Setup plus relevant working installation/capability information contribute to creation of a fixed **Session Definition in RAM** when START is accepted. Authoritative race/session information worth retaining is written to Memory as appropriate during or after a session.

The Session Definition is therefore not a gateway to Memory and is not a module. It is frozen working data that isolates an active session from later changes to persistent configuration.

Pavlov is used when components communicate changes, Requests, facts or state to one another. A module reading configuration/state already present in its own working RAM is local data access, not a Pavlov transaction.

Memory must not become a shadow Race Engine by reconstructing competition state from every live event. The Race Engine remains authoritative for competition interpretation and live competition state; Memory retains the authoritative results/history information provided for persistence.

The initial product should retain a deliberately modest useful result/history record rather than logging everything simply because storage is available.

The product-level retained-history behaviour is defined in `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md`; the detailed persistence schema and storage technology remain implementation decisions. Wall-clock date/time is not required for retained race history.

The storage model must be extensible so that substantially richer information can be retained later without redesigning Race Control or making older records unusable.

## 13.1 Information ownership and delivery

P&P may distribute information widely, but it must not distribute ownership. For each authoritative fact or state there is one owner and one truth. Other components may hold views, snapshots or derived presentation data without becoming another authority.

Architectural modules own the internal state required to perform their responsibility. Other modules consume defined outputs, facts, state or requests through the owning module's boundary; they do not depend on its internal workings. In particular, Race Control does not need to know the Race Engine's internal lap, position or calculation process. It consumes only defined Race Engine outputs that matter to session operation, such as competition completion. The architecture therefore does not require a universal central State module.

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

The **System Controller (SC)** is the central P&P controller platform containing the authoritative Race Control and Race Engine responsibilities. Race Control is a distinct responsibility within the SC, not another name for the SC.

P&P uses the common communications bus as the normal path for inter-component communication. Mapped information and other messages are consumed by the responsibility that owns their meaning; they are not mechanically forwarded through Race Control or through the SC merely for routing.

A private point-to-point inter-component path is therefore not part of the normal architecture. If implementation later demonstrates a genuine technical need for one, it is an explicit exception and must preserve the same message meaning, authority and observable system state rather than becoming a hidden alternative architecture.

## 13.5 P&P communications bus

P&P components communicate through a common logical message backbone, referred to in design discussion as the **Pavlov Bus**. This is an architectural and programming concept, not necessarily one physical electrical bus or one transport protocol.

The default architectural rule is:

> **If P&P components need to communicate with one another, they do so through the Pavlov Bus.**

Modules, devices and human interfaces attach to the common bus and exchange standard P&P messages without requiring direct knowledge of each other's implementation. Local software participants may attach in-process; remote participants reach the same logical bus through appropriate Transport Adapters.

The bus may carry Requests, Request Results, standard Input Events, Actions, Events/Facts, State-change notifications and other defined P&P messages.

### 13.5.1 Authority and message policing

Connection to the bus grants **connectivity, not authority**.

Each standard P&P message contract defines:

- what the message means and the data it carries;
- which participant or responsibility is permitted to publish/originate it;
- which participant(s) or responsibilities are permitted or expected to consume it.

A component must not acquire authority merely because it can technically place a message on the bus. For example, an Input Device may originate a valid detector event but cannot authoritatively declare that a lap has been completed; that interpretation belongs to the Race Engine. A browser may originate an authorised START Request but cannot publish an authoritative GO or mutate competition state.

Consumers subscribe to or receive only the message types appropriate to their responsibilities. This replaces architectural policing by dedicated point-to-point pathways while preserving the same responsibility boundaries.

The bus itself does not decide whether a message is true, valid or operationally permitted. Validation and consequence remain with the authoritative responsibility defined for that message.

### 13.5.2 One message, one or many consumers

A message may have one intended consumer or several. A START Request is handled by Race Control. A competition detector event is handled by the Race Engine. An authoritative scheduled start publication may be consumed by the Race Engine, lights, browser presentation, Taster, audio and test equipment.

This allows a new consumer, such as a future scoreboard, to subscribe to existing standard P&P information without requiring Race Control or the Race Engine to be modified merely to know that the new consumer exists.

Race Control and the Race Engine use this same bus for their own defined communication. They do not require a special private link.

### 13.5.3 State notification and resynchronisation

The Pavlov Bus does not replace authoritative P&P State.

Authoritative State remains owned by the relevant P&P responsibilities. A State-change notification on the bus acts as the notification that previously obtained State may now be stale. A browser or other current-state consumer then refreshes the authoritative State it needs.

Rapid successive changes may therefore collapse naturally from the consumer's point of view. A browser that last obtained State revision 1042 may receive a notification and refresh directly to revision 1045 without reconstructing revisions 1043 and 1044. Events/Facts that matter individually remain separate bus messages where their individual occurrence is relevant.

In design discussion this is the **ding-and-scoop** model: the bus carries the ding; authoritative State is the noticeboard from which the consumer scoops the latest truth.

### 13.5.4 Transport Adapters

The Pavlov Bus is a logical topology. It need not be one physical network and does not require every message to be physically broadcast to every participant.

A **Transport Adapter** connects a particular communications mechanism to the logical bus while preserving standard P&P message meaning and authority. Examples may include local in-process delivery, wired remote communication, ESP-NOW and Wi-Fi/WebSocket. The exact transports and software mechanisms remain implementation decisions.

A Transport Adapter may filter/deliver only messages relevant to participants reachable through it. A slow or disconnected remote participant must not block timing-critical local processing.

The logical topology may therefore be pictured similarly to a shared Ethernet backbone: Input Devices, SC responsibilities, Memory, human interfaces, physical outputs, test tooling and future devices are participants attached to a common communications system. Direction and authority are properties of individual messages and responsibilities, not of the bus itself.

Human-interface participants such as the Browser and Taster are two-way: they consume P&P information for presentation and may originate authorised Requests. One-way output participants such as start lights may simply consume the message types they require.

## 13.6 Manual User Gateway (MUG)

A **Manual User Gateway (MUG)** is a phone, tablet, computer or other browser host acting as a human-facing P&P interface.

Multiple MUGs may be connected simultaneously. One MUG may hold Race Director authority while others operate as permitted driver, display or spectator clients. A MUG does not own race state and its disconnection must not stop the race.

MUG is internal/technical vocabulary; customer-facing interfaces may use ordinary terms such as Race Director, driver or display.

## 14. Test and diagnostics

Testability is a permanent architectural responsibility.

Real wired detectors, wireless devices, future devices and simulated inputs should use the same standard P&P message contracts and common communications bus. Test tooling may publish permitted synthetic/test messages and subscribe to relevant outputs at the same architectural boundaries used by real components.

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

Product-level persistence/history behaviour and race-mode behaviour are now defined in the relevant product specifications. Detailed persistence schema, storage technology and implementation structures remain implementation decisions rather than architectural prerequisites.

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
