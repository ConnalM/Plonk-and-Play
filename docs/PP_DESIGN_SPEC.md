# Plonk & Play™ — consolidated design specification

**Status:** Consolidated design baseline for review (19 November 2026). All 22 identified governing design files transferred; targeted duplication removed. Known high-impact contradictions reconciled; all retirement destinations and internal navigation checked. Integration with the active firmware review and any residual semantic review are tracked under DOC-01 in the Master Project Plan. The [Design Constitution](DESIGN_CONSTITUTION.md) and [System Requirements](SYSTEM_REQUIREMENTS.md) govern this specification. [Master Project Plan](MASTER_PROJECT_PLAN.md) is the sole delivery/backlog authority, including v1 deferrals and post-v1 candidates. Acceptance tests and dated evidence remain separate. No new design decisions or promotion of post-v1 functionality arise from the consolidation.

**Terminology:** Integrated Display (formerly Taster), Racer (formerly MUG), Race Control / controlling Browser (formerly SMUG/SMUGLAND) are the provisional customer-facing names. Existing canonical firmware identifiers and historic labels remain unchanged.

## Contents

1. [System architecture](#system-architecture)
2. [Race Control and Race Engine](#race-control-and-race-engine)
3. [Inputs and outputs](#inputs-and-outputs)
4. [Communications and system time](#communications-and-system-time)
5. [Memory and storage](#memory-and-storage)
6. [Presentation](#presentation)
7. [Configuration](#configuration)
8. [Diagnostics and recovery](#diagnostics-and-recovery)
9. [Physical product](#physical-product)

### Common design topics

| Subject | Governing location |
|---|---|
| Startup, power loss and device replacement | [System lifecycle](#system-lifecycle--pp-system-lifecycle-and-persistence) |
| Session definition, control and official records | [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design) · [Source / record policy](#race-control-engine-design--41-session-source-and-record-policy) |
| Race rules | [Lap Race](#race-modes-product-spec--2-lap-race) · [Practice](#race-modes-product-spec--3-practice) · [Endurance](#race-modes-product-spec--4-endurance) |
| Future mode designs | [Timed Stage](#race-modes-product-spec--5-timed-stage) · [Drag](#race-modes-product-spec--6-drag) (release scope in the plan) |
| Detectors and controlled outputs | [Inputs](#system-architecture--4-input-architecture) · [Outputs](#system-architecture--10-output-architecture) |
| System clock and Message Bus | [System Time](#system-architecture--8-pp-system-time) · [Message Contract](#message-contract--pp-message-contract) |
| Persistence, details and SD ownership | [Persistent storage](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design) |
| Browser screen appearance and flow | [Visual references](#implementation-spec--approved-visual-references) · [Browser flow and History](#browser-flow-results-history-spec--browser-flow-results-and-history) |
| Local display and controls | [Integrated Display](#pp-taster-spec--pp-taster-specification) · [Physical display constraints](#physical-integrated-display-constraints) |
| Sound | [Shared audio](#product-facilities-spec--3-audio-and-removable-storage) |
| Track, Racer, Car and lane setup | [Configuration and identity](#configuration-and-identity-spec--plonk--play--configuration-and-identity-specification) · [Lane assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour) |
| Maintenance and diagnostics | [Recovery / reset / update](#wp6-recovery-reset-update-contract--wp6-recovery-reset-and-customer-firmware-update-contract) · [Diagnostics](#stage-15-logging-diagnostics-foundation-design--stage-15-logging-and-diagnostics-foundation-design) |

## Authority and reconciliation rules

The design describes **one product**, with shared authority and client-specific restricted views. Current owner-approved decisions take precedence over earlier drafts, historic implementation scope statements or prototype evidence. Each migrated heading retains a stable legacy anchor and original source can be recovered from Git history. Completed work and deferred scope are governed by the master plan, not old phase wording. An unresolved conflict must be recorded there and resolved explicitly, never by silently selecting convenient text. Timing and competition rules belong to Race Control/Engine; local controls and visual presentation belong to Presentation; shared audio is a system facility under Presentation; configuration owns remembered setup and physical installation defaults. The scope of old implementation slices is not a present-day product limit.

### Reconciled authority and precedence

- The [session-source/record policy](#race-control-engine-design--41-session-source-and-record-policy) governs all races: the 18 November approval supersedes older unqualified statements that Lap Race or Endurance results are always official. Source and eligibility freeze at accepted START, not at connection or finish.
- [Integrated Display READY](#pp-taster-spec--ready) owns the local adjustment limit; it never limits the shared positive 32-bit Browser/stored target.
- [Integrated Display factory settings](#pp-taster-spec--factory-taster) and [restoration](#pp-taster-spec--smug-configurable-taster) own the 10-lap LAPS and five-minute Endurance defaults and the protection of physical track, racer assignments and sound.
- [Shared sound](#product-facilities-spec--35-preferences-defaults-and-testing) governs both Browser and standalone; no second preference store.
- [Accepted physical display constraints](#physical-integrated-display-constraints) supersede earlier moving-animation implementation proposals. V18 Browser is owner-approved as the implementation design baseline (19 November 2026), subject to testing and any corrections revealed by testing. Production implementation and acceptance remain open.
- The [Master Project Plan](MASTER_PROJECT_PLAN.md) alone determines current, later-v1 and post-v1 work. No earlier stage exclusion/deferral automatically creates an open task or overrides later owner-approved scope.


## System architecture

<a id="system-architecture--plonk--play-system-architecture"></a>
### Plonk & Play™ System Architecture

**Contract:** System architecture defines responsibilities and boundaries. Detailed software, hardware and customer behaviour belongs in the appropriate sections below.

This document is subordinate to the Design Constitution and System Requirements.

<a id="system-architecture--1-architectural-approach"></a>
#### 1. Architectural approach

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

<a id="system-architecture--2-high-level-responsibilities"></a>
#### 2. High-level responsibilities

The system currently comprises these responsibility areas:

1. Input Module / Input Devices
2. Race Control
3. Race Engine
4. Output Module / Output Devices
5. Presentation and User Interaction
6. Supporting responsibilities/services:
   - Registry / Discovery
   - P&P System Time
   - Memory / Persistence
   - Test / Diagnostics
   - Update / Recovery

Configuration, Race Setup, Session Definition, Registry and authoritative State are information/state rather than extra operational modules merely because other parts of P&P use them. These are architectural responsibilities and information relationships, not a proposed source-code directory structure.

<a id="system-architecture--5-device-failure-and-replacement"></a>
#### 5. Device failure and replacement

The hardware/device layer detects and reports device/capability status to Registry. Registry maintains P&P's current availability information. Neither decides the race consequence of losing a device or capability.

For example:

- loss of an optional sector detector need not stop lap timing;
- loss of a speed trap need not stop a race;
- loss of the only Start/Finish detector may make accurate lap racing impossible.

Race Control decides the consequence according to the active session and available capabilities.

A replacement device identifies itself and its capabilities, not its assumed physical role. Configuration/Discovery may transfer remembered assignments automatically where replacement is unambiguous. Where there is a genuine choice, the user is asked.

<a id="system-architecture--6-registry-and-discovery"></a>
#### 6. Registry and Discovery

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

<a id="system-architecture--61-stable-identity"></a>
##### 6.1 Stable identity

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

<a id="system-architecture--62-replacement-devices"></a>
##### 6.2 Replacement devices

A newly discovered device with similar capabilities to a missing known device must not silently inherit that device's identity.

Where replacement is strongly suggested and unambiguous, P&P may ask a simple human question such as whether the new unit replaces the missing one. Confirmation can then transfer the existing assignments.

Where there is a genuine choice, P&P asks rather than guesses.

<a id="system-architecture--63-simple-directly-connected-hardware"></a>
##### 6.3 Simple directly connected hardware

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

This distinction must not affect race rules. After Input Device abstraction, both sources produce the same standard physical Input Events identified by stable input/capability identity. The active Session Definition, not the Input Module, assigns their session-specific racing roles.

<a id="system-architecture--64-capability-level-availability"></a>
##### 6.4 Capability-level availability

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

<a id="system-architecture--65-transport-independent-discovery"></a>
##### 6.5 Transport-independent discovery

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

<a id="system-architecture--66-power-on-reconciliation-and-readiness"></a>
##### 6.6 Power-on reconciliation and readiness

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

<a id="system-architecture--17-detailed-design-status"></a>
#### 17. Detailed design status

The Race Control / Race Engine responsibility question has been resolved in [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design): Race Control owns session operation/lifecycle, while Race Engine owns competition interpretation, state, rules and calculations. Together they preserve one authoritative P&P race/session authority without duplicate ownership.

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

<a id="system-lifecycle--pp-system-lifecycle-and-persistence"></a>
### System lifecycle and persistence

**Contract:** Governing design within [system architecture](#system-architecture).

<a id="system-lifecycle--1-purpose"></a>
#### 1. Purpose

This document defines the conceptual lifecycle of a P&P controller from power-on through normal operation and shutdown, including persistent configuration, equipment reconciliation, device synchronisation, first-time defaults and recovery after uncontrolled shutdown.

It does not prescribe storage technology, hardware interfaces, clock-synchronisation algorithms or implementation classes.

<a id="system-lifecycle--2-overall-lifecycle"></a>
#### 2. Overall lifecycle

The conceptual lifecycle is:

```text
POWER ON
   ↓
CORE INITIALISATION
   ↓
LOAD LAST-KNOWN-GOOD PERSISTENT STATE
   ↓
DISCOVERY & RECONCILIATION
   ↓
DEVICE INITIALISATION & SYNCHRONISATION
   ↓
SYSTEM ASSEMBLY & AVAILABILITY
   ↓
OPERATIONAL / IDLE
   ↓
SESSION / COMPETITION OPERATION
   ↓
GRACEFUL SHUTDOWN, RESTART OR POWER LOSS
```

A failure in optional equipment must not prevent the controller itself from reaching a state in which it can report the problem and provide unaffected functions where practical.

<a id="system-lifecycle--3-core-initialisation"></a>
#### 3. Core initialisation

Before equipment discovery, the controller must establish the essential P&P environment required to perform discovery and reconciliation.

Conceptually this includes:
- controller/core startup;
- essential supporting services;
- access to persistent storage;
- establishment of P&P System Time;
- communications required for equipment discovery.

The exact boot sequence and implementation mechanisms are later decisions.

<a id="system-lifecycle--4-persistent-state-loaded-into-working-ram-at-startup"></a>
#### 4. Persistent state loaded into working RAM at startup

P&P must not rediscover the installation from a blank sheet at every power-on.

Memory owns the persistent copy of information that must survive power loss. At startup P&P loads the last-known-good information required for operation into working RAM, including as applicable:
- Registry information;
- installation configuration;
- remembered/default device-role assignments/proposals;
- last-used Race Setup;
- saved Race Setup presets where supported;
- user/presentation preferences;
- persistent competition progress;
- other persistent system settings.

The storage mechanism may later use controller flash/NVS, SD card or another technology. The lifecycle design refers only to **persistent storage**.

The last-used Race Setup is restored as the proposed/default setup for the next session. Modules use the applicable working configuration in RAM during normal operation rather than repeatedly consulting persistent storage. Live state from an interrupted individual race is not restored.

**Restore the Race Setup, not the race.**

<a id="system-lifecycle--5-discovery-and-reconciliation"></a>
#### 5. Discovery and reconciliation

After loading remembered persistent information into working RAM, P&P discovers the equipment actually present and reconciles reality with remembered Registry information and the working configuration.

The normal case should require no user interaction:

```text
Load remembered configuration into working RAM
        ↓
Discover expected equipment
        ↓
Restore known remembered/default role proposals
        ↓
Continue startup
```

Expected but missing equipment retains its remembered identity and remembered/default role proposal but is marked unavailable. These remembered proposals are installation/configuration data; the roles actually used by an accepted race are frozen separately into its Session Definition.

Unexpected equipment is identified and processed without unnecessarily preventing the rest of P&P from becoming operational.

<a id="system-lifecycle--6-device-initialisation-and-time-synchronisation"></a>
#### 6. Device initialisation and time synchronisation

Discovery alone does not make an intelligent timing device ready for use.

Where applicable, P&P must:
- establish normal communication;
- confirm identity, capabilities, compatibility and relevant status;
- initialise the device as necessary;
- establish its relationship to P&P System Time;
- restore any required operating configuration;
- confirm that its relevant capabilities are operational.

**Discovered does not necessarily mean synchronised or available.**

Initial time synchronisation is followed by whatever ongoing resynchronisation is required to maintain the timing relationship. The detailed clock-synchronisation algorithm is outside this document.

A device that reconnects must not be treated as timing-ready until any required reinitialisation and resynchronisation have occurred.

Initialisation may include low-level hardware procedures that are not P&P identity. For example, several identical I2C sensors may power up at the same default bus address and be woken/assigned temporary addresses one at a time. Such addresses remain internal to the hardware/device adapter; stable P&P identity or capability identity is established separately.

A capability does not contribute operational events until the initialisation required for that function is complete and it has been declared available.

<a id="system-lifecycle--7-system-assembly-and-availability"></a>
#### 7. System assembly and availability

Once equipment has been reconciled and initialised, P&P determines which configured capabilities are currently available, establishes required safe/default output states and makes the system's current state available to Presentation/User Interaction.

System operational status is distinct from readiness for a particular activity.

P&P may be operational while an optional sector detector is unavailable. Whether a particular session can start depends upon the capabilities that session requires.

**Readiness is capability- and activity-based, not one global READY / NOT READY state.**

<a id="system-lifecycle--8-first-time-startup-and-factory-defaults"></a>
#### 8. First-time startup and factory defaults

If no previous P&P configuration exists, the system enters normal first-time commissioning using factory defaults.

The provisional factory session defaults are:
- two lanes;
- Lap Race;
- 10 laps;
- optional race features off;
- sound enabled if available;
- track power used if available.

The base product may define fixed roles for built-in or directly connected capabilities where their intended purpose is unambiguous. For example, if the base product definition establishes two standard timing inputs as Lane 1 and Lane 2 Start/Finish, P&P need not ask the customer to assign them.

Extra equipment whose physical purpose cannot be determined must not be guessed.

The objective is that a standard new base system can reach a useful two-lane 10-lap race with the minimum possible customer configuration.

<a id="system-lifecycle--9-missing-new-and-replacement-equipment"></a>
#### 9. Missing, new and replacement equipment

<a id="system-lifecycle--expected-equipment-missing"></a>
##### Expected equipment missing

Missing equipment does not erase its remembered/default role proposal.

Unaffected capabilities continue to be available. A session that does not require the missing capability may still be usable.

<a id="system-lifecycle--new-equipment"></a>
##### New equipment

New equipment is discovered, validated, initialised and synchronised automatically where applicable.

If its intended assignment can be inferred unambiguously, P&P may assign it automatically. Otherwise P&P asks only the minimum question necessary.

Optional unassigned equipment does not prevent unaffected functions from operating.

<a id="system-lifecycle--replacement-equipment"></a>
##### Replacement equipment

A new device with a different stable identity does not silently become a missing known device.

Where a replacement is unambiguous, P&P may offer a simple confirmation and then transfer the previous remembered/default role proposals and persist the new relationship.

<a id="system-lifecycle--10-missing-or-unusable-stored-configuration"></a>
#### 10. Missing or unusable stored configuration

P&P should prefer:

1. last-known-good persistent configuration;
2. factory defaults when no usable configuration exists.

A first-ever startup with no configuration is normal.

If stored configuration is corrupt, incomplete or incompatible, P&P must not silently invent physical assignments. It may fall back to safe factory/default behaviour while using discovery to assist controlled recovery/commissioning.

The persistence mechanism should later be designed so that an interrupted configuration write does not unnecessarily destroy the last-known-good configuration.

<a id="system-lifecycle--11-persistence-policy"></a>
#### 11. Persistence policy

Persistence is based on the value and lifecycle of the information, not on shutdown.

<a id="system-lifecycle--configuration-and-system-state"></a>
##### Configuration and system state

Information required to restore P&P itself is persisted when it changes or at an appropriate immediate commit point. Normal restoration must not depend on a future graceful shutdown.

<a id="system-lifecycle--live-individual-race-state"></a>
##### Live individual-race state

Detailed live race state remains in working RAM while the race is active. The fixed Session Definition for an accepted race is also working RAM data; it is not a persistent configuration object or a separate module. It freezes the Race Setup together with the session-specific roles assigned to the input, output and other capabilities required by that session. Later configuration changes cannot silently change those roles during the active race.

Examples include:
- current lap counts;
- current lap/sector timing state;
- live positions;
- current simulated fuel state;
- other transient competition state.

P&P is not required to continuously journal this information to persistent storage.

A controller crash or uncontrolled power loss may therefore lose the active individual race. This is acceptable.

<a id="system-lifecycle--completed-race-data"></a>
##### Completed race data

When an individual race finishes, the race/result data selected for History is written to persistent storage.

History behaviour and formats follow [Results and History](#browser-flow-results-history-spec--results--history) and [persistent storage](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design). Qualification of capacity and compatibility remains in the Master Project Plan.

<a id="system-lifecycle--12-multi-race-competition-progress"></a>
#### 12. Multi-race competition progress

A competition may span several individual races or heats. P&P should preserve enough persistent competition state to continue an unfinished competition after a restart.

For example, with eight competitors and four races/heats, after two have completed P&P may persist:
- competition definition;
- entrants;
- completed races/results;
- current progress;
- the next scheduled race/heat.

This information is persisted at meaningful competition boundaries rather than after every detector event.

If an individual race is interrupted by a failure or shutdown and cannot validly complete, P&P is not required to reconstruct that race. On continuation of the competition, the interrupted race may be restarted.

**Preserve the competition; do not attempt to reconstruct an interrupted race.**

<a id="system-lifecycle--13-unfinished-competition-at-startup"></a>
#### 13. Unfinished competition at startup

After normal startup and hardware reconciliation, if P&P finds a persisted unfinished competition it should offer the Race Director the choice to continue it rather than automatically resuming it.

Conceptually:

```text
Unfinished competition found
        ↓
Continue previous competition?
      YES / NO
```

Continuing restores the competition structure, completed results and next race/heat.

If the previous individual race was interrupted, continuation restarts that race rather than restoring its volatile internal state.

<a id="system-lifecycle--14-graceful-shutdown"></a>
#### 14. Graceful shutdown

Graceful shutdown is primarily verification, safe-state handling and tidying; it is not the normal mechanism by which important configuration is saved.

Where applicable, P&P should:
- stop or resolve active operation in a defined manner;
- check that persistent information which should already have been committed has been committed;
- complete outstanding persistent writes that are worth preserving;
- place controllable outputs into their defined safe states;
- notify intelligent modules of shutdown where genuinely necessary;
- stop services cleanly.

The detailed user experience for shutting down during an active individual race is a later product decision.

<a id="system-lifecycle--15-uncontrolled-shutdown"></a>
#### 15. Uncontrolled shutdown

P&P must assume that a crash, watchdog reset or loss of power may provide no opportunity for shutdown processing.

Therefore:
- restoration-critical configuration must already have been persisted;
- loss of the active individual race is acceptable;
- the next boot follows the normal startup, reconciliation and synchronisation process;
- P&P is not required to reconstruct volatile live race state.

An uncontrolled shutdown must not be treated as a reason to add continuous race journalling or high-availability machinery.

<a id="system-lifecycle--16-device-disappearance-and-reconnection"></a>
#### 16. Device disappearance and reconnection

The architecture supports devices becoming unavailable and subsequently available again where the underlying hardware and transport naturally support it.

A returning intelligent device may require reinitialisation and resynchronisation before its capabilities become available again.

When a capability becomes unavailable, its last reported physical state must not continue to be treated as trustworthy current physical state. On recovery, the capability establishes its current state afresh before becoming operationally available.

Availability transitions whose timing affects interpretation of events are related to P&P System Time. A delayed event is therefore interpreted against the relevant availability/session state at its event timestamp rather than merely the state at packet arrival.

This does **not** create a general requirement for powered physical hot-plugging.

Whether a particular wired interface is safe to connect or disconnect while powered is a hardware-design decision.

It is acceptable for the Race Director to stop P&P, replace failed wired timing hardware and restart the system.

<a id="system-lifecycle--17-hardware-fault-containment-and-proportionality"></a>
#### 17. Hardware fault containment and proportionality

Hardware interfaces should support the architectural principle of local failure where reasonably practical, but P&P does not promise continuous racing through arbitrary hardware faults or replacement.

There is no requirement for:
- wired hot-plug support;
- seamless in-race sensor replacement;
- preservation/restoration of every live lap after controller failure;
- high-availability operation.

Where a hardware failure invalidates an individual race, that race may be restarted. Where it occurs within a larger persisted competition, the competition can continue from that race after repair and restart.

**Engineer in proportion to the product.**

<a id="first-implementation-behaviour--pp-first-implementation-behaviour"></a>
### Foundational customer-visible operation

**Contract:** Foundational customer-visible startup and behaviour. The original prototype was a proving milestone, not the final v1 scope.

<a id="first-implementation-behaviour--1-purpose"></a>
#### 1. Purpose

This document defines the customer-visible and system behaviour required for the first useful P&P implementation.

It does not redefine the P&P architecture and does not prescribe software classes, protocols, packet formats, browser technology, storage mechanisms, hardware pinouts or other implementation details.

The first implementation is deliberately narrow, while its behaviour must remain consistent with the general P&P architecture.

Initial implementation scope:

- two-lane analogue slot-car timing;
- Start/Finish detection;
- basic Lap Race;
- browser-based control and display.

The behaviour described here should use general P&P mechanisms rather than special-case logic that only works for this first product.

**Design broadly. Implement narrowly.**

<a id="first-implementation-behaviour--2-general-startup-behaviour"></a>
#### 2. General startup behaviour

P&P uses the same general startup approach regardless of the amount of equipment installed.

Conceptually:

```text
POWER ON
   ↓
Initialise P&P
   ↓
Discover what is actually available
   ↓
Reconcile discovered equipment with remembered information
   ↓
Apply remembered configuration where still valid
   ↓
Apply defaults where appropriate
   ↓
Determine what configured functions are currently available
   ↓
Present the resulting system to the user
```

Discovery establishes what actually exists.

Remembered configuration and defaults determine what P&P proposes to do with it.

Discovery does not itself imply assignment or use.

<a id="first-implementation-behaviour--3-first-startup-and-defaults"></a>
#### 3. First startup and defaults

On first startup there is no remembered customer configuration.

P&P discovers all supported equipment that is actually available and then applies the factory/default configuration.

The initial base configuration is:

- two lanes;
- Lap Race;
- 10 laps;
- optional race features off;
- the recommended base physical arrangement places each lane's Start/Finish sensor before the Start/Finish line in the direction of travel.

With that recommended arrangement, a car starts before its Start/Finish detector. After GO, the first Start/Finish crossing completes Lap 1, timed from GO. This is the default first-implementation behaviour; alternative physical arrangements remain supported through installation configuration and Race Setup as defined by [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design).

Where the base product or remembered installation makes an assignment unambiguous, P&P may make it automatically. Where more than one valid assignment is possible, P&P must not guess.

For example, if the base product defines two physically distinct standard timing inputs as Lane 1 and Lane 2 Start/Finish, P&P may assign those roles automatically. Discovering several otherwise identical unassigned detectors does not by itself make their roles unambiguous.

Equipment not required by the default configuration remains available but unassigned.

P&P must not invent purposes for equipment merely because it has discovered it.

<a id="first-implementation-behaviour--4-unassigned-equipment"></a>
#### 4. Unassigned equipment

Useful discovered equipment that remains unassigned is brought to the user's attention.

For example:

`2 unassigned detectors found`

The user may choose to configure that equipment or ignore it for now.

Unassigned optional equipment does not prevent normal operation of the configured system.

The objective is to make the user aware of useful additional capability without forcing unnecessary setup before racing.

<a id="first-implementation-behaviour--5-remembered-configuration"></a>
#### 5. Remembered configuration

On subsequent startup, remembered configuration takes precedence over factory defaults where it remains valid.

P&P discovers the equipment actually available and reconciles that reality with the remembered configuration.

A missing known device or capability retains its remembered/default role proposal but is marked unavailable. The active racing role is not owned by the device or Input Module; roles actually used by a race are frozen into its Session Definition when START is accepted.

P&P must not silently move another device into that role merely because it is available.

<a id="first-implementation-behaviour--6-required-and-optional-capabilities"></a>
#### 6. Required and optional capabilities

Whether a capability is required is determined by the selected activity and configuration, not by the hardware itself.

For example, the same compatible detector might be:

- required when assigned as the only Start/Finish detector for an active lane;
- optional when assigned as a sector detector for a Lap Race.

Loss of an optional capability must not unnecessarily prevent the activity from operating.

Loss of a required capability prevents the affected activity from starting or continuing where meaningful operation is no longer possible.

The selected activity and its enabled features declare the capabilities upon which they depend and whether those dependencies are required or optional.

These dependencies must be declarative and extensible.

P&P must not depend on a fixed central firmware table that has to be modified whenever a future supported activity, feature or capability is introduced.

A genuinely new activity may still require new executable race logic. Declarative configuration is not expected to create functionality which P&P does not otherwise implement.

<a id="first-implementation-behaviour--7-missing-equipment-and-customer-recovery"></a>
#### 7. Missing equipment and customer recovery

When configured equipment is unavailable, P&P tells the user:

- what is unavailable;
- what effect that has on the selected activity.

If only optional functionality is unavailable, the activity may still be started.

If required functionality is unavailable, START is unavailable.

P&P may offer:

- Try Again;
- Setup.

Where another currently available capability is known to be compatible with the missing required role, P&P may additionally offer the user a simple reassignment.

For example, an available sector detector might be offered as a replacement for a missing Start/Finish detector if it provides the required compatible detector capability.

P&P must not perform such reassignment silently.

The user confirms the change and the resulting assignment becomes configuration.

This is a general compatibility/reassignment mechanism, not a collection of individually programmed fault scenarios.

<a id="first-implementation-behaviour--8-pre-race-screen"></a>
#### 8. Pre-race screen

The main pre-race screen is primarily a summary of what will happen if the user presses START.

It shows the effective forthcoming **Race Setup** rather than the machinery used to create it.

Frequently changed race settings should be directly accessible where practical.

Less frequent installation configuration, including detector assignments and discovered equipment, is available through Setup.

The screen should not be cluttered with hardware details during normal operation.

Hardware/configuration information becomes prominent when the user needs to know or act upon it.

<a id="first-implementation-behaviour--9-start-and-readiness"></a>
#### 9. START and readiness

P&P does not require a separate prominent READY indicator.

The START control itself communicates readiness.

If all requirements of the selected activity/configuration are satisfied, START is available.

If a required capability is unavailable, START is unavailable and P&P explains why.

Missing optional equipment may be reported without preventing START.

Immediately after START is requested, Race Control performs the final readiness decision using the Registry's authoritative current capability availability, so that a capability lost since the screen was displayed cannot silently invalidate the race. Presentation may display whether START appears available, but it is not the authority that permits the session to start.

No unnecessary confirmation screen is required.

<a id="first-implementation-behaviour--10-fixing-the-session-definition"></a>
#### 10. Fixing the Session Definition

Before START, the SMUG changes the proposed **Race Setup**. The SMUG does not edit a Session Definition.

When START is accepted, P&P validates the Race Setup against the applicable working installation/capability information and creates the fixed **Session Definition in RAM** for that race. It freezes both the race choices and the session-specific roles of every input, output and other capability required for that race.

Normal race operation may change race state, but ordinary configuration changes must not silently alter the active Session Definition.

START begins the start procedure defined for that race.

<a id="first-implementation-behaviour--11-start-procedure"></a>
#### 11. Start procedure

Race Control coordinates the configured start procedure and establishes the authoritative GO time.

Outputs participating in that procedure receive the appropriate logical actions through their defined boundaries.

Where independent components must perform timing-critical actions together, P&P coordinates them sufficiently accurately for their intended purpose.

The mechanism used to achieve this is an implementation decision.

Presentation latency must not determine official race timing.

<a id="first-implementation-behaviour--12-running-race-presentation"></a>
#### 12. Running race presentation

A display receives the authoritative information necessary for its configured purpose.

Race-owning components do not tailor or send information directly to individual displays.

The running presentation shows information relevant to the current race and configured display purpose. Features not in use need not occupy presentation space.

Whether implementation distributes all relevant authoritative information to consumers or filters it according to consumer requirements is an implementation decision and must not affect ownership or race operation.

<a id="first-implementation-behaviour--13-running-race-control"></a>
#### 13. Running race control

The primary Race Director intervention during a running race is PAUSE.

The operation sequence below describes authoritative Race Control semantics;
it is not a promise that every operation is exposed as a live-screen Browser
button. The frozen normal Browser exposes **PAUSE** only while a competitive
race is active. After authoritative PAUSED it exposes **RESUME**, **RESTART
RACE** and confirmed **ABANDON**. The same presentation rule applies to Lap
Race and Endurance. Practice has its separate **PAUSE** then **RESUME** /
**RESTART** / **END PRACTICE** presentation defined in the Browser flow
specification.

The basic control flow is:

```text
RUNNING
   ↓
PAUSE
   ↓
RESUME / RESTART RACE / END RACE
```

RESUME continues the existing race.

RESTART RACE abandons the current attempt and begins the same fixed Session Definition again through its normal start procedure.

END RACE abandons the current race and returns the user to the pre-race state.

In the normal Browser, the paused competitive **ABANDON** control invokes this
authoritative abandonment operation. The distinction is presentation only;
Race Control remains the owner of lifecycle, cleanup and the no-result/no-
History outcome.

<a id="first-implementation-behaviour--131-basic-pause-behaviour"></a>
##### 13.1 Basic PAUSE behaviour

PAUSE suspends competition timing and competition progress without ending the race.

The default restart method is **Honour restart**:

- MUGs stop their cars as soon as reasonably possible when PAUSE/Yellow is called.
- Cars should remain where they stopped. If a car must be handled, it should be returned as close as reasonably possible to that position.
- P&P freezes competition timing while paused. Detector activity during the paused interval does not advance competition progress.
- The interrupted lap is preserved. Time spent paused is excluded from that lap time.
- RESUME uses a short restart countdown and one authoritative scheduled restart instant so all MUGs receive a predictable common restart rather than racing becoming live at the instant the RESUME Request is pressed.

PAUSED is an operational State, not a claim that all pre-pause input has already been interpreted. A clean local detector event timestamped before the authoritative pause instant remains eligible even if its delivery is delayed. A restart therefore waits for a defined local input-settlement fence before it can be accepted.

This is deliberately an honour-system behaviour for the base product. P&P does not claim to know a car's physical position between configured detection points.

An alternative **Grid Restart** may be selected where a more controlled restart is wanted:

- cars are returned to defined Start/Finish grid positions;
- incomplete lap progress at PAUSE is discarded;
- completed laps and other completed competition results remain;
- the race resumes from the defined grid through the restart sequence.

The exact physical grid placement relative to the Start/Finish detectors must be defined by track/setup guidance so placing cars on the grid does not create an unintended competition crossing.

Future optional hardware such as an REU may provide sufficient authoritative position/progress information for a more accurate position-aware restart. The detailed REU mechanism is not defined here and must not be assumed by the base implementation.

Detailed RMS Yellow/Red state transitions, track-power behaviour and other hardware-dependent pause effects remain deliberately deferred.

<a id="first-implementation-behaviour--14-normal-race-completion"></a>
#### 14. Normal race completion

Normal completion is determined by the Race Engine according to the selected race rules.

For a Lap Race, the initial design supports three configurable finish behaviours:

1. **Immediate finish** — when the first competitor completes the configured number of laps, the race ends immediately. Other competitors' finishing positions/state are determined at that point.
2. **Complete current lap** — when the first competitor completes the configured number of laps, each remaining active competitor continues until their next Start/Finish crossing, at which point that competitor is finished.
3. **Complete full race distance** — every active competitor continues until they have completed the configured number of laps.

More sophisticated finish behaviour may be added later without changing the general Race Control/Race Engine responsibility split.

The selected finish rule becomes part of the fixed Session Definition when START is accepted.

<a id="first-implementation-behaviour--15-results"></a>
#### 15. Results

When a race completes normally, P&P presents the authoritative final results appropriate to that race.

See [Results and History](#browser-flow-results-history-spec--results--history), [mode behaviour](#race-modes-product-spec--plonk--play--race-modes-product-specification) and [persistent storage design](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design).

<a id="first-implementation-behaviour--16-implementation-discipline"></a>
#### 16. Implementation discipline

The first implementation must implement the behaviour defined above through the responsibilities and boundaries established by the governing P&P documents. It must not introduce first-product shortcuts that bypass or redefine those boundaries.

Where this document deliberately leaves behaviour or implementation undecided, implementation must not silently turn that omission into a permanent architectural decision.

<a id="first-implementation-behaviour--161-first-software-implementation-milestone"></a>
##### 16.1 First software implementation milestone

The first implementation milestone is a **software/architecture prototype**, not a declaration that the complete first saleable hardware product is finished.

Wokwi may be used as the development test bench for the ESP32-side software, with simulated sensors, buttons and other suitable devices standing in for physical hardware while real browser clients exercise the P&P user/presentation boundary.

Wokwi is **not part of the P&P architecture or product contract**. No architectural responsibility, interface or behaviour may depend on Wokwi-specific facilities. A simulated device must be replaceable by real hardware implementing the same P&P boundary without requiring race/session logic to be redesigned.

The prototype should prove the architectural path and boundaries, including:

- physical/simulated input through Input Devices and the Input Module, producing stable device/capability events without race meaning;
- the common P&P communications bus as the normal inter-component path;
- separate Race Control and Race Engine responsibilities;
- authoritative timing, State and Events/Facts;
- Requests and Request Results;
- scheduled GO coordination;
- browser initial synchronisation and current-State refresh;
- multiple browser clients;
- disconnect/reconnect and recovery to current authoritative State;
- rapid input/state changes without browser/network activity delaying timing-critical race processing;
- configuration and persistence behaviour required by the implemented features;
- logical output boundaries using simulated outputs where real output hardware is not yet present.

Product features should then be added through these same boundaries rather than bypassing them for convenience.

<a id="first-implementation-behaviour--162-prototype-implementation-sequence"></a>
##### 16.2 Prototype implementation sequence

The original staged prototype execution sequence is historical, not an alternative roadmap. [Original sequence and checkpoints](https://github.com/ConnalM/Plonk-and-Play/blob/6d25c9924a4da090a316aaaf70fe1a1f6d5c1a58/docs/PP_DESIGN_SPEC.md#first-implementation-behaviour--162-prototype-implementation-sequence) remain evidence. Design requirements: use the same role-neutral input, Message Bus, authority, timing, output and current-state boundaries, with controlled diagnostics, rapid-input testing and independent Browser reconstruction. Follow the [Master Project Plan](MASTER_PROJECT_PLAN.md) for current delivery order.

<a id="first-implementation-behaviour--17-pre-implementation-consistency-review"></a>
#### 17. Pre-implementation consistency review

The consolidated design must remain consistent across sections, conform to the Constitution/Requirements and use one authoritative definition per requirement. Startup/reconciliation order is governed by [system lifecycle](#system-lifecycle--pp-system-lifecycle-and-persistence); customer-visible behaviour above does not introduce a competing startup sequence. Review and delivery status are managed in the Master Project Plan.

<a id="stage-14a-design--stage-14a-design--variable-race-entry-capacity"></a>
### Variable Race Entry capacity

**Status: ACCEPTED / FROZEN**

**Implementation status: ACCEPTED — automated, regression and real-ESP32 hardware verification passed on 2026-10-05.**

The variable-entry contract generalises the previously fixed two-entry data model and execution paths, supporting variable active Race Entries and preserving shared race semantics. Each mode defines its own activity/finish rules.

<a id="stage-14a-design--product-boundary"></a>
#### Product boundary

P&P has no arbitrary product maximum lane count. Two lanes remain the initial, default and Taster configuration. A particular firmware build may have a bounded implementation capacity because the ESP32 uses deterministic static storage, but that capacity is an implementation/resource limit and must never be presented as a product rule.

The proposed implementation capacity is `PP_MAX_ENTRIES = 8`. This is an engineering default, not a claim that P&P supports only eight lanes. No Race Engine algorithm may depend on the value eight. Increasing the capacity later must require storage/configuration changes and additional validation, rather than a new competition algorithm or product redesign.

The four-entry fixture described below is acceptance evidence only. It is not a product maximum.

<a id="stage-14a-design--existing-behaviour-that-remains-authoritative"></a>
#### Existing behaviour that remains authoritative

The following remain unchanged:

- default/Taster setup is two lanes;
- Lap Race lifecycle, GO/start sequence, false-start policy, Pause settlement, Honour Restart and Grid Restart;
- Relevant-Time ordering and all accepted timing boundaries;
- Race Director and Spectator authority boundaries;
- Race Engine ownership of competition and results;
- Input Module role-neutral event and settlement behaviour;
- Noticeboard as authoritative reconstruction State;
- Results, Details, History, PB and Track Record semantics;
- Stage 1–13 Message Bus numeric values and ownership boundaries;
- Taster, Attract and existing Browser behaviour for the default two-entry configuration.

Stage 13 deferred multi-lane expansion is the subject of this refactor. No other Stage 1–13 requirement is superseded.

<a id="stage-14a-design--proposed-data-model"></a>
#### Proposed data model

<a id="stage-14a-design--proposedracesetup"></a>
##### ProposedRaceSetup

Replace the separate first and second entry fields in `ProposedRaceSetup` with an indexed collection:

```cpp
struct ProposedRaceEntry {
    SessionInputRole startFinish;
    uint32_t mugId;
};

struct ProposedRaceSetup {
    ProposedRaceEntry entries[PP_MAX_ENTRIES];
    uint8_t activeEntryCount;
    // existing Lap Race configuration remains here
};
```

Validation iterates over every active entry. It preserves existing validity rules for input identity, Start/Finish role, lane identity, MUG identity and Lap Race configuration. The default setup populates two entries exactly as it does today.

Temporary first-entry and second-entry accessors may remain as compatibility helpers, but core code must use the indexed collection and count.

<a id="stage-14a-design--sessiondefinition-and-role-mappings"></a>
##### SessionDefinition and role mappings

`SessionDefinition` stores `entryCount`, one `RaceEntryDefinition` per active entry and one role mapping per active entry. Session construction copies all configured entries and assigns stable Race Entry IDs in order. Race Again and other existing setup-copying paths copy the complete collection, not only the first two entries.

`nextRaceEntryId` advances by the active entry count. Existing two-entry IDs and ordering remain unchanged.

<a id="stage-14a-design--race-engine-storage-and-algorithms"></a>
##### Race Engine storage and algorithms

Race Engine entry state, penalties, pending event state and completed-result state use `PP_MAX_ENTRIES` storage and the active entry count. Every loop, lookup and completion operation is indexed by `entryCount`.

The current two-entry completion masks must not remain the algorithmic basis. Completion/observation state should use indexed flags or a capacity-sized bitset whose operations are expressed as entry iteration. No algorithm may assume that eight is the number of entries.

Result sealing must rank all active entries using the existing comparison rules and stable ordering. The current special case that swaps only two result entries becomes a stable multi-entry ordering operation. Existing two-entry tie and completion ordering must remain identical.

The existing `laps()` and `lastLapTime()` convenience accessors may continue to summarize entry zero for compatibility. Indexed accessors are authoritative for multi-entry behaviour.

Any compile-time or development detail-buffer capacity is an engineering
resource bound only. It is not an Endurance or Lap Race sporting maximum.
Authoritative lap totals and classification continue beyond the available
detail buffer; incomplete detail prevents permanent History publication.

<a id="stage-14a-design--noticeboard-state"></a>
##### Noticeboard State

`NoticeboardState` contains `entryCount` and one authoritative `NoticeboardEntryState` for every active entry. State construction copies all active Race Engine entries.

Existing top-level first-entry summary fields may remain as compatibility aliases for Stage 1–13 clients. They must not be used as the complete multi-entry representation.

<a id="stage-14a-design--completedraceresult-and-history"></a>
##### CompletedRaceResult and History

`CompletedRaceResult` contains an indexed summary entry for every active entry,
including rank, laps-behind, completion information and best-lap information.
Detailed lap records are retained by the active/detail storage boundary and
are retrieved on demand for Details; they are not represented by a fixed
product-sized array in the result summary.

The new result and persistence representations must be explicitly versioned. There is no requirement to migrate current development History, PB or Track Record data: this project is pre-production and those existing development formats may be invalidated or reset when the representation changes. Future production data must have a version identity and a migration path.

History reconstruction must decode the new version into the same authoritative Results and Details model without Fact replay or Browser calculation.

The result payload size remains guarded by a compile-time assertion against the History storage bound. The bound must be increased or otherwise sized for `PP_MAX_ENTRIES = 8`.

<a id="stage-14a-design--race-control-and-race-again"></a>
##### Race Control and Race Again

Race Control copies all entries, role mappings, MUG associations and Lap Race configuration when preparing Race Again, Restart Race and other existing setup-copying operations. No operation may silently drop entries three and above.

<a id="stage-14a-design--browser-state-results-and-details"></a>
##### Browser State, Results and Details

The authoritative Browser/API representation uses a dynamic entry collection:

```json
{
  "entryCount": 4,
  "entries": [
    { "raceEntryId": 1, "laps": 2 },
    { "raceEntryId": 2, "laps": 1 },
    { "raceEntryId": 3, "laps": 2 },
    { "raceEntryId": 4, "laps": 0 }
  ]
}
```

Results and Details serialize every active entry and all immutable lap records. Browser reconstruction uses current authoritative State and persisted result data, never fixed two-entry assumptions.

Stage 1–13 top-level fields and Lane 1/Lane 2 records aliases may remain for compatibility. The indexed collection is authoritative.

The human-readable diagnostic Browser may continue to expose only the default two simulated detector controls. Multi-entry acceptance uses the normal API/Input Module boundary and does not require a polished multi-lane control surface.

<a id="stage-14a-design--pb-and-track-records"></a>
##### PB and Track Records

PB and Track Record processing is generalized so every active Race Entry in a session is evaluated independently. This removes the current two-entry assumption from eligibility processing while preserving exactly the Stage 13 eligibility rule.

`PP_MAX_ENTRIES` limits simultaneous active Race Entries in this firmware build. It does not limit the number of persistent MUG identities or PBs that the product may ultimately retain. Stage 14A does not introduce a MUG database or complete MUG/SMUG configuration system. Persistent identity storage remains an implementation boundary to be defined by that future configuration work.

The records API gains an indexed view of the active entries for the current race while retaining Lane 1 and Lane 2 compatibility aliases. Multi-entry acceptance must prove PB/Track Record processing for every active entry without implying one permanent PB slot per Race Entry.

The new persisted record representation is versioned. Existing development records may be cleared or reset during the format change; future production records must be migratable.

<a id="stage-14a-design--input-module-and-acceptance-fixtures"></a>
##### Input Module and acceptance fixtures

The simulated detector/source arrays used by the development and acceptance boundary are generalized to the same implementation capacity. Events still enter through the normal protected Input Module FIFO and Message Bus. Fixtures must not manipulate Race Engine timing or lap state directly.

The default Stage 1–13 simulator remains two-entry. A deterministic four-entry fixture supplies additional input identities and role mappings only for this refactor's acceptance evidence.

<a id="stage-14a-design--capacity-and-resource-rationale"></a>
#### Capacity and resource rationale

Static bounded storage is appropriate for the ESP32 because it provides deterministic memory use, avoids race-time heap fragmentation, keeps persistence sizing explicit and supports compile-time assertions.

Active detail storage grows with the factual records produced by the current
race and reports allocation failure explicitly; it is not sized as a fixed
sporting maximum. A completed summary remains bounded by active-entry count,
while detailed records are encoded/retrieved through the storage boundary.
The development codec workspace is a provisional resource bound and must be
measured/removed as the streamed SD implementation replaces it.

The exact sizes must be measured during implementation. Capacity changes later must be confined to the capacity definition, storage sizing, serializers/codecs and tests. They must not require changes to Lap Race timing, authority, settlement or ranking concepts.

If a setup exceeds the compiled implementation capacity, it may be rejected as unavailable in this firmware build. That message must describe an implementation/resource limit, not a product maximum.

<a id="stage-14a-design--affected-components"></a>
#### Affected components

- `firmware/include/pp/session_definition.h` — setup and Session Definition collections.
- `firmware/include/pp/race_engine.h` — entry storage, completion state, ranking and result storage.
- `firmware/include/pp/noticeboard.h` — variable entry State.
- `firmware/include/pp/race_control.h` — complete setup copying and Race Entry ID allocation.
- `firmware/include/pp/record_store.h` — variable PB storage and versioned record data.
- `firmware/include/pp/history_store.h` — result sizing and versioned persistence codec integration.
- `firmware/include/pp/browser_interface.h` — dynamic State, Results, Details, History and Records serialization.
- `firmware/include/pp/core.h` — shared capacity and generalized simulated Input Module storage.
- `firmware/src/main.cpp` — default two-entry setup remains unchanged; fixtures gain indexed setup support.
- `firmware/tests` and `acceptance` — deterministic multi-entry fixtures and structural/evaluator checks.

Relevant governing constraints are in `docs/SYSTEM_REQUIREMENTS.md`, [System architecture](#system-architecture--plonk--play-system-architecture), [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design), [Message Contract](#message-contract--pp-message-contract), [Integrated Display](#pp-taster-spec--pp-taster-specification), [Shared display](#common-display-spec--plonk--play--common-display-specification) and the accepted Stage 1–13 documents.

<a id="stage-14a-design--risks-and-compatibility"></a>
#### Risks and compatibility

- Fixed-width completion masks may overflow or silently omit entries.
- Two-entry sorting and setup-copying special cases may be hidden in less obvious paths.
- Existing Browser clients may expect Lane 1/Lane 2 aliases.
- Binary result and record formats require explicit version identities for future migration.
- Dynamic Browser responses may exceed current fixed buffers.
- Race Entry ID allocation must remain unique across Race Again and multi-entry sessions.
- Default Taster and two-entry behaviour must not be changed by generalized storage.
- The full Stage 1–13 automated/regression suite must remain passing.

The variable-entry contract applies across supported modes including Open Practice and Endurance; original Stage 14A acceptance remains historical evidence.


## Race Control and Race Engine

<a id="system-architecture--9-race-control"></a>
#### 9. Race Control

Race Control and the Race Engine together form the **single authoritative race/session authority**, with non-overlapping ownership defined in [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design).

<a id="system-architecture--90-normal-live-communication"></a>
##### 9.0 Normal live communication

The common P&P communications bus is the normal route for communication between P&P components, including communication between Race Control and the Race Engine.

This does **not** create a universal decision-making dispatcher or receptionist. The bus transports standard P&P messages; responsibility and authority remain with the component that owns the meaning.

Examples:

- Presentation clients such as the browser or Taster publish authorised operational **Requests**; the P&P responsibility that owns the requested change consumes the Request. Race Control consumes session-operation Requests such as START, PAUSE and RESUME.
- The Input Module publishes clean standard physical Input Events identifying the stable input/capability and event time; the Race Engine interprets relevant events using the device roles frozen in the active Session Definition.
- Race Control may publish an authoritative scheduled GO; the Race Engine and relevant presentation/output consumers may consume the same publication.
- The Race Engine may publish competition completion; Race Control consumes that fact and applies the session-lifecycle consequence.
- A local PAUSE delivery fence, where required, is likewise composed of defined common-bus messages. Race Control does not inspect an Input Module backlog, and the Input Module does not acquire lane, MUG, lifecycle, or competition-rule authority.

Race Control and the Race Engine therefore do not require a privileged private communications path merely because both are contained within the System Controller. Their ownership boundaries remain distinct while their defined inter-component communication uses the same common bus as the rest of P&P.

The **System Controller (SC)** is the enclosing authoritative race-controller platform containing Race Control and the Race Engine. It is not an additional functional module and is not an extra message-routing hop.

Race Control owns session operation and lifecycle, including preparation, start procedure, authoritative GO, Pause/Resume/Stop and coordination of operational logical actions. The Race Engine owns competition interpretation, competition state, rules, calculations, results and competition-completion conditions.

They do not depend on sensor models, GPIOs, wireless addresses, physical gantries, browser implementation or storage media.

<a id="system-architecture--91-race-modes"></a>
##### 9.1 Race modes

Different race forms are treated as replaceable Race Engine rule sets within the common Race Control / Race Engine framework rather than separate hardware/timing systems.

Known customer-facing modes include:

- Lap Race;
- Practice;
- Endurance;
- Timed Stage;
- Drag;
- future modes.

A new mode should ideally add its rules without altering unrelated existing modes.

<a id="system-architecture--92-optional-race-features"></a>
##### 9.2 Optional race features

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

<a id="system-architecture--93-racesession-state-and-calculations"></a>
##### 9.3 Race/session state and calculations

Authoritative live information has one owner according to meaning:

- Race Control owns session lifecycle/operational state such as preparing, starting, running, paused or stopped;
- the Race Engine owns competition state such as current lap, lap/sector times, positions, competition elapsed time, results and competition-affecting feature state.

An incoming competition event changes authoritative competition state once. Where the relative timing of events affects the authoritative competition result, the Race Engine uses their P&P System Time timestamps rather than communication arrival order. The architecture must permit delayed or out-of-order events to be correctly sequenced where necessary. The detailed buffering or sequencing mechanism is an implementation/subsystem-design decision.

Displays, sound and other consumers must not maintain competing calculations of the race.

Derived calculations such as lap time, sector time, reaction time and speed may remain race-domain calculations unless a future requirement demonstrates a genuine need for a separate measurement-processing responsibility.

<a id="system-architecture--94-race-facts-versus-actions"></a>
##### 9.4 Race facts versus actions

Race Control reports **facts/events** independently of how they are presented.

Example:

```text
FASTEST_LAP
lane = 3
time = 5.21
```

A sound system might announce it, a browser might highlight it and history storage might retain it.

Separately, Race Control can request **logical actions**, such as scheduled start lights or track power changes. Output hardware details remain outside Race Control.

<a id="race-control-engine-design--race-control-and-race-engine-detailed-design"></a>
### Race Control and Race Engine Detailed Design

**Contract:** Governing design within [system architecture](#system-architecture).

<a id="race-control-engine-design--1-purpose"></a>
#### 1. Purpose

This document resolves the Race Control / Race Engine responsibility question deliberately left open in System Architecture v1.0. It defines the responsibility boundary sufficiently for later implementation and isolated testing without prescribing C++ classes, files, packet formats or other implementation details.

<a id="race-control-engine-design--2-responsibility-split"></a>
#### 2. Responsibility split

**Race Control owns the operation and lifecycle of a session.**

This includes:
- preparing and starting a session;
- the start procedure;
- establishing the authoritative GO time in P&P System Time;
- PAUSE and RESUME;
- operational STOP;
- RESET as an operational request;
- coordinating operational actions such as start sequences and track power;
- deciding the operational consequence of relevant capability/availability changes.

**Race Engine owns the interpretation, state, rules and calculations of the competition within that session.**

This includes:
- interpreting physical Input Events through the active Session Definition;
- lap, sector and other competition timing;
- lap counts, positions and other competition state;
- race-mode rules;
- optional competition features such as simulated fuel;
- results;
- determining when the competition's rules say it has completed.

Together Race Control and Race Engine remain the single authoritative source of P&P race/session state, with each responsibility having one owner.

Race Control may request/announce defined operations to the Race Engine through standard P&P messages on the common bus but must not manipulate its internal competition state. The Race Engine reports competition facts and completion conditions through the same communications architecture but does not control session lifecycle or physical hardware.

<a id="race-control-engine-design--3-conceptual-boundary"></a>
#### 3. Conceptual boundary

<a id="race-control-engine-design--race-control-to-race-engine"></a>
##### Race Control to Race Engine

The conceptual information/operations crossing the boundary are:

1. **PREPARE**
   - supplies the fixed session definition for the competition, for example mode, race length and enabled optional features.

2. **GO @ P&P time**
   - establishes the authoritative time from which the Race Engine regards competition as active.

3. **SESSION OPERATION / STATE CHANGE @ P&P time**
   - communicates operational changes such as Pause, Resume and Stop where relevant.
   - RESET is an operational request; the Race Engine remains responsible for creating, clearing or reinitialising its own competition state.

4. **COMPETITION INPUT**
   - clean physical Input Events identify the stable input/capability and original P&P event time;
   - the Race Engine interprets each relevant Input ID through the fixed Session Definition to obtain its session role;
   - these events are delivered to the Race Engine through the common P&P communications architecture and need not be mechanically relayed through Race Control merely because Race Control owns session lifecycle.

<a id="race-control-engine-design--race-engine-to-race-control"></a>
##### Race Engine to Race Control

1. **COMPETITION FACTS**
   - examples include lap completed, fastest lap, position changed and pit/refuelling state.

2. **CURRENT COMPETITION STATE**
   - authoritative competition information such as laps, times, positions and enabled-feature state.

3. **COMPLETION**
   - indicates that the active rule set says the competition has completed normally.

4. **PROBLEM / STATUS**
   - reports conditions that prevent or impair correct interpretation of the competition.

These categories are conceptual contracts, not final packet formats, APIs or C++ methods.

<a id="race-control-engine-design--4-session-definition"></a>
#### 4. Session definition

An active session uses a fixed **Session Definition held in working RAM**. It is immutable working data for the session, not a module or bus participant and not something the SMUG edits directly.

Before START, the SMUG edits the proposed **Race Setup**. When START is accepted, P&P validates that setup against the applicable working installation/capability information and creates the Session Definition. The Session Definition freezes only the information required to interpret and execute that session, including the session-specific roles assigned to every required input, output and other capability.

For the first two-lane Lap Race it contains:
- Session ID;
- mode, lap/distance target, finish behaviour, enabled race features and relevant start/first-crossing behaviour;
- one stable Race Entry ID for each competitor;
- for each Race Entry, its lane, stable MUG ID and frozen MUG display name, plus optional stable Car ID and frozen Car display name where Cars are enabled;
- required semantic input roles mapped to stable Input IDs/capabilities;
- required semantic output roles mapped to stable Output IDs/capabilities;
- the trusted session source and record policy: standalone Taster (non-official),
  SMUG-controlled P&P (official under the normal rules), or development
  simulation (non-official);
- only the physical/rule parameters required to interpret or execute the session.

Installed capabilities unused by the session do not need session roles merely because they exist. Hardware implementation details such as GPIOs, transport addresses, detector thresholds and Browser connections do not belong in the Session Definition.

Stable record IDs preserve permanent links. Display values whose later alteration would change the historical meaning of the race are frozen. Thus a MUG renamed after the race does not retrospectively rename that Race Entry in the completed session/result.

Ordinary configuration changes must not silently alter a session already in progress. The definition used by Race Control and the Race Engine remains the definition for that session unless a particular runtime change is explicitly supported by later design.

For the initial product, race-affecting configuration is not changed while an individual race is active. The Race Director changes it after that race has ended. This is a deliberate product simplification, not an architectural prohibition on harmless live presentation preferences or a future explicitly designed runtime feature.

This does not prescribe whether the implementation copies, references, serialises or otherwise represents the definition.

<a id="race-control-engine-design--41-session-source-and-record-policy"></a>
##### 4.1 Session source and record policy

**Owner decision, 18 November 2026.** Official-record eligibility is frozen
in the authoritative `SessionDefinition` at START. A standalone Taster race is
non-official and must not create official History, PB or Track Record entries.
A later SMUG connection cannot change that frozen eligibility. Simulated-race
classification remains independently preserved. The current implementation
must be reconciled and tested against this decision before standalone Taster
official-record behaviour is claimed.

Race Control derives the session source from the trusted control boundary when
START is accepted; it must not accept a Browser-supplied presentation label as
proof of source. The resulting record policy is immutable for the session.

The Race Engine uses that frozen policy at its existing persistence gates:

- standalone Taster sessions seal and present their results in memory but do
  not append official History or update PB/Track Record data;
- SMUG-controlled Lap Race and Endurance sessions retain the normal official
  History, PB and Track Record behaviour;
- development simulation remains non-official and remains distinct from a
  real standalone Taster race.

Control handover during an active session cannot change this policy or
retroactively publish a previously non-official result.

Trusted START ingress and controlling lease must be decided coherently from committed authority at acceptance, never from a client presentation label. Publish changed authority only after successful persistence. A new START after RACE AGAIN/RESTART RACE makes a fresh source decision; resume and in-session restart preserve the active definition. Configuration provenance is not session authority. Exclusion includes official History/sequence, PB/Track Record, official details, HistoryStored and any false pending/fault caused solely by deliberate non-official exclusion. Retain bounded live result/detail presentation and do not promote it on Browser reconstruction. See [acceptance requirements](STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md).

<a id="race-control-engine-design--5-time-and-event-ordering"></a>
#### 5. Time and event ordering

Race Control establishes authoritative session-state boundaries using P&P System Time.

Where timing affects competition interpretation, the Race Engine uses event timestamps relative to those boundaries rather than communication arrival order.

For example, if PAUSE is effective at P&P time 250.000:
- an event timestamped 249.999 belongs before the pause even if it arrives afterwards;
- an event timestamped 250.001 belongs after the pause.

For a local Input Module pause, PAUSED lifecycle State does not by itself mean that pre-pause competition State is final. A clean input accepted locally before the authoritative pause boundary can still be delivered and interpreted afterwards. Before Race Control accepts a restart, the Race Engine confirms through the common Message Bus that it has interpreted every clean input accepted locally before that boundary. This is a local delivery fence, not a distributed-device watermark, clock-synchronisation, or remote-input settlement protocol.

Detailed buffering and sequencing mechanisms remain implementation/subsystem decisions.

If two relevant events have the same effective P&P timestamp, the responsibility interpreting those events defines the consequence of equality. P&P does not require a universal tie-breaking mechanism merely to force an artificial order.

START and GO are distinct concepts:
- START begins the operational start procedure;
- GO is the authoritative P&P timestamp at which competition becomes active.

<a id="race-control-engine-design--6-completion-stop-and-reset"></a>
#### 6. Completion, stop and reset

Normal completion originates from the Race Engine because it owns the competition rules.

Race Control then performs the appropriate session-lifecycle transition and operational consequences.

Manual STOP originates from Race Control and is distinct from normal competition completion. Stopping a race must not manufacture a normal competition-completion event or result. The later product design may decide how a partial stopped race is displayed, retained, discarded or restarted.

RESET of an individual race is requested operationally by Race Control, but Race Control must not reach into Race Engine state and directly zero lap counts, timers or other competition variables. The Race Engine owns the creation and clearing of its own competition state.

An individual-race reset/restart, abandonment/reset of a larger competition and a factory/system reset are distinct operations owned at their appropriate boundaries. They must not be collapsed into a universal `resetEverything()` operation.

Starting a new individual race creates fresh individual-race state within the already-running P&P system. It does not normally reinitialise unrelated services, devices, installation configuration, retained results or longer-lived competition state.

<a id="race-control-engine-design--7-race-modes"></a>
#### 7. Race modes

Race modes are replaceable rule sets within the common Race Engine framework rather than a single growing collection of mode-specific conditions.

Known customer-facing modes include:
- Lap Race;
- Practice;
- Endurance;
- Timed Stage;
- Drag;
- future modes.

Different modes may have different concepts of start, competitor, completion and result while retaining the same Race Control / Race Engine responsibility boundary.

Practice demonstrates that a mode need not have a normal competition finishing condition: Race Control still owns lifecycle while the Race Engine times laps and maintains practice state until operational STOP.

<a id="race-control-engine-design--8-optional-race-features"></a>
#### 8. Optional race features

Optional race features are separate from race modes.

For example, simulated fuel may operate across more than one mode and maintains its own competition-feature state while consuming relevant competition facts/events interpreted through the active Session Definition.

**Race Mode defines the fundamental form and completion rules of the competition. Optional Race Features add behaviours that can operate across more than one race mode.**

The Race Engine must not know detector identities or sensor technologies. A fuel/pit feature may consume an event such as:

`Lane 1 : PIT_ENTRY : ACTIVE : time`

but not `Detector 5` or a particular sensor API.

Physical enforcement of a feature is possible only where installed hardware exposes the required capability.

<a id="race-control-engine-design--81-startfinish-first-crossing-behaviour"></a>
#### 8.1 Start/Finish first-crossing behaviour

Lap-race behaviour must support the physical starting-grid arrangement relative to the Start/Finish detector.

The grid/Start-Finish relationship is installation configuration because it describes the physical track arrangement. When a Lap Race is prepared, the effective first-crossing behaviour derived from that installation configuration becomes part of the fixed session definition for that race.

Where cars start **after** the Start/Finish detector in the direction of travel, they cross the detector shortly after GO. That first post-GO crossing does not complete a lap; it establishes the timing origin for Lap 1. The next Start/Finish crossing completes Lap 1.

Where cars start **before** the Start/Finish detector in the direction of travel, they travel almost a complete circuit before their first post-GO crossing. That first Start/Finish crossing completes Lap 1, timed from GO.

This behaviour is not inferred by the detector or Input Module. The Input Module reports the same stable input identity and physical event in either arrangement. The active Session Definition assigns that input its session-specific Start/Finish role, and the Lap Race rule set interprets the event according to that fixed definition. Another session may assign the same physical input a different role without changing the Input Module.

<a id="race-control-engine-design--9-hardware-availability-and-outputs"></a>
#### 9. Hardware, availability and outputs

Race Engine does not discover hardware failures.

The Registry owns current device/capability availability information. Race Control receives or obtains availability information relevant to the current session and decides its operational consequence.

When a START request is made, Race Control performs the final authoritative readiness check against the Registry's current capability availability before accepting the start. Presentation may indicate apparent readiness, but it does not authorise a session to start.

Loss of an optional capability need not stop a session. Loss of a capability essential to meaningful continuation may require the session to be stopped or restarted.

Race Engine does not manipulate physical outputs.

Race Control resolves required session roles through the active Session Definition and publishes device-level logical operational actions through the established common P&P communications bus. The Output Module/appropriate Output Device performs the hardware-specific action without needing to know the racing role of that output. For example, Race Control may resolve `Lane 2 Track Power` to the stable output capability assigned to that role for this session, while neither Race Control nor Race Engine manipulates GPIO or transport-specific hardware directly.

Detailed false-start behaviour remains a later design decision and depends upon the physical/start arrangement. In particular, a system in which lane power is held off until GO cannot use pre-GO vehicle movement in the same way as a continuously powered arrangement.

<a id="race-control-engine-design--10-information-boundaries"></a>
#### 10. Information boundaries

Each component publishes and receives information only through its defined boundaries. It may publish information for which it is the authoritative owner and consume information made available through its defined interfaces. It must not bypass those boundaries to read, alter or reproduce another component's internal state.

A source reports what happened at its own boundary; it does not need to know or control the eventual consequence. For example, a physical control can report that a button was activated just as a detector reports a state transition. The responsibility receiving that information decides what it means in its own context. Where an interface deliberately defines a genuine operation such as PREPARE, GO or RESET between Race Control and Race Engine, that operation remains part of that defined boundary.

Race Engine consumes physical Input Events and interprets their stable Input IDs through the active Session Definition. It does not require knowledge of:
- sensor model;
- detector transport;
- I2C/GPIO details;
- ESP-NOW/Wi-Fi details;
- browser/client identity.

Similarly, Race Control should receive capability/function availability rather than unnecessary low-level failure details.

Operational consumers receive information in the form appropriate to their boundary and do not depend on upstream implementation or transport. Diagnostics may trace information across boundaries without becoming part of the operational dependency chain.

This boundary should be enforceable structurally during implementation wherever practical.

<a id="race-control-engine-design--101-authoritative-facts-current-state-noticeboard-and-persistence"></a>
#### 10.1 Authoritative facts, current state, Noticeboard and persistence

P&P distinguishes between three information behaviours:

- **facts/events** describe something that happened;
- **current state** describes what is true now;
- **persistent information/configuration** describes information that must remain available across the relevant lifecycle.

Authoritative race facts/events are published once through a common logical interface. Interested consumers may consume them independently. The producer does not require knowledge of those consumers, and consumers must not independently recreate or alter authoritative race state.

The **Noticeboard** is the current authoritative state of every relevant thing needed to understand the session now. It may combine information from several authoritative owners for consumption, but the Noticeboard does not become another owner of that information. Presentation never owns authoritative P&P data; it displays or derives views from information supplied through defined boundaries by the components that own it.

Current fastest lap is an example of Noticeboard State: once established it remains the current fastest lap until superseded or until it ceases to be relevant. It does not disappear merely because later unrelated state changes occur.

A material Noticeboard change may publish the standard `NOTICEBOARD_CHANGED` notification. The notification says only that something material has changed; it does not carry a replacement copy of State. A newly connected or recovering current-state consumer obtains the current authoritative information it needs without replaying every intermediate state change.

The Noticeboard itself does not conceptually require an exposed version number. An implementation may use revision counters, dirty flags, sequence numbers or another internal mechanism where useful without making that mechanism part of the architectural State contract.

Continuous values must not generate streams of authoritative events or `NOTICEBOARD_CHANGED` notifications merely because their displayed value changes with time. Significant transitions may be published as facts/events, while consumers obtain or derive the current value from authoritative state and P&P System Time where appropriate. A race clock can therefore count locally from authoritative timing information without changing the Noticeboard every second.

The authoritative fact/state exists independently of whether a particular consumer successfully receives a live notification. Current-state consumers can resynchronise; obsolete transient notifications may simply be missed; information that P&P has decided must persist is retained by the Memory Module rather than depending on a live consumer receiving a notification.

Consumers may derive information for their own presentation or local use, but this does not create new authoritative P&P state or facts. If derived information must itself be authoritative or shared, it is produced by the responsibility that owns that meaning and exposed through its defined boundary.

Retained history may preserve earlier facts for later retrieval without requiring those facts to remain live current-state information.

<a id="race-control-engine-design--11-testability"></a>
#### 11. Testability

The Race Control / Race Engine boundary must support controlled test implementations in either direction.

Examples:
- Race Control can be tested with a stub Race Engine returning predetermined competition facts, state, completion and status.
- Race Engine can be tested with predetermined physical Input Events plus Session Definitions that assign those stable input identities to session roles, without real sensors, the Input Module or browser clients.
- Input and Output Modules can be tested independently of lanes, MUGs and race modes: stimulate a known input and verify the standard device event, or issue a device-level output action and verify the correct physical output behaviour.

Production and test implementations must use the same defined interfaces; testing must not rely on special shortcuts through the production architecture.

Neither side may reach into the other's internal variables.

<a id="race-control-engine-design--12-proportionality"></a>
#### 12. Proportionality

The design must remain proportionate to a consumer slot-car timing system.

The separation exists to provide clear ownership, replaceability, testability and resistance to accidental architectural coupling. It is not justification for unnecessary layers, distributed-state machinery or high-availability engineering.

**Design broadly. Implement narrowly.**

---

<a id="race-modes-product-spec--plonk--play--race-modes-product-specification"></a>
### Race modes and rules

**Status:** Committed product-design specification  
**Scope:** Agreed behaviour for Lap, Practice, Endurance, Timed Stage and Drag.  
**Relationship:** This is the canonical race-behaviour contract for every P&P
client. The Browser and integrated Taster are presentation/control clients of
the same authority; the Taster is a restricted subset, not a separate racing
product. Detailed RMS implementation remains deferred.

<a id="race-modes-product-spec--1-mode-hierarchy"></a>
#### 1. Mode hierarchy

The customer-facing mode hierarchy is:

- **LAP**
  - Race
  - Practice
- **ENDURANCE**
- **TIMED STAGE**
- **DRAG**

Normal circuit racing is called **Lap**, not Circuit.

<a id="race-modes-product-spec--11-restricted-taster-subset"></a>
##### 1.1 Restricted Taster subset

The Browser exposes available supported modes; the Master Project Plan governs the v1 release boundary. Planned modes are not runnable before implementation. The Taster
exposes only two local choices:

- **LAPS** — the local presentation of `Lap Race`;
- **ENDURANCE** — the local presentation of `Endurance`.

The Taster does not define a separate `TIMED` race mode. `Timed` remains a
Practice option where supported, and **Timed Stage** remains a distinct full
P&P Browser mode. Restricting the Taster's choices must never change the
authoritative mode, timing, finish, pause or result semantics defined here.

<a id="race-modes-product-spec--2-lap-race"></a>
#### 2. Lap Race

<a id="race-modes-product-spec--21-race-definition"></a>
##### 2.1 Race definition

Lap Race is primarily defined by a target number of laps.

The three supported finish behaviours are:

1. **Winner finishes — all stop immediately.**
2. **Winner finishes — all others finish the current lap.**
3. **Winner finishes — all others complete the total race distance.**

These are the three normal Lap Race finish choices.

For `Complete Full Race Distance`, either lane may reach the selected distance
first and is recorded as the winner/first finisher. Every other required lane
continues until it completes that same distance. Each entry's finishing time is
captured and then frozen at its own final eligible crossing. The shared race
clock and race-completion boundary occur when all required entries have
finished; clients must not establish independent lane clocks. A client may
derive and render displayed lane timing from authoritative P&P timing.

<a id="race-modes-product-spec--22-live-information"></a>
##### 2.2 Live information

Typical live information includes:

- position;
- laps completed / target;
- last lap;
- best lap;
- gap ahead/behind;
- sectors where fitted;
- optional live current-lap time.

Sector feedback may briefly show +/- against the competitor's best.

**Fastest Lap** is a separate race statistic regardless of who wins and may generate a live callout.

<a id="race-modes-product-spec--23-starts"></a>
##### 2.3 Starts

Lap Race supports a conventional countdown using either **3 or 5 red lights**.

GO presentation can be:

- **Lights out** — reds illuminate sequentially, then all extinguish.
- **Green** — reds illuminate sequentially, then green indicates GO.

The delay after all reds may be:

- fixed and selectable;
- random within a selectable minimum/maximum;
- immediate where configured.

The interval between red lights may be configurable under Advanced.

The sequence is parameterised rather than hard-coded.

Audio may accompany the start using configured styles such as beeps, different pitches or spoken numeric countdown. A spoken countdown must not announce GO prematurely: numbers complete first, then the configured final delay, then GO.

Where a Power Module is fitted, track power may optionally be held off during the sequence and enabled at GO. Live-power starts remain available.

<a id="race-modes-product-spec--24-false-starts"></a>
##### 2.4 False starts

False-start detection is offered only where the configured physical Start/Finish arrangement can actually detect a pre-GO crossing.

Lap false-start responses may include:

- Off;
- Warning only;
- +1 lap;
- time penalty;
- Power penalty where a Power Module is fitted.

Hardware-dependent options may remain visible but locked/greyed with a short explanation rather than disappearing.

<a id="race-modes-product-spec--3-practice"></a>
#### 3. Practice

Practice is a proper subset of Lap, not a race with a fake winner.

Practice has no normal winner/finish condition. Open Practice offers two SMUG-selectable start options: **Immediate start** (default, no countdown) and **Practice start** (normal five-light starting sequence). Both remain Open Practice and neither creates a competitive result. See the [authoritative lifecycle](#stage-14b-design--mode-and-lifecycle).

Practice has Open and planned Timed variants. Open Practice is the v1 mode; Timed Practice remains post-v1. See [Open Practice](#stage-14b-design--product-boundary) for the binding session/result rules.

Recorded information may include:

- lap count;
- last lap;
- session best (not an official PB update);
- session fastest;
- sectors where fitted;
- best sectors;
- comparison with a previously retained personal best where supported (read-only; no Practice record update);
- theoretical best from the sum of best sectors.

The normal solo path should remain simple: **Practice → lane → Start**.

<a id="race-modes-product-spec--4-endurance"></a>
#### 4. Endurance

<a id="race-modes-product-spec--41-race-definition"></a>
##### 4.1 Race definition

Endurance uses a configured duration rather than a target lap count.

Typical presets may include 5 min, 10 min, 30 min, 1 hour and Custom.

The winner is determined by distance/laps at expiry according to the configured finish behaviour.

<a id="race-modes-product-spec--42-finish-behaviours"></a>
##### 4.2 Finish behaviours

Endurance has two normal finish choices:

1. **Stop at zero** — the race ends when the authoritative remaining timer reaches `0` seconds. Classification uses completed laps and known progress where sector information permits it.
2. **Finish current lap** — when the authoritative remaining timer reaches `0` seconds, each running MUG continues until its next Start/Finish crossing and then finishes.

There is no invented third timed-race finish mode.

For Finish Current Lap, competitors tied on completed laps can be separated by the order in which they complete the final lap.

The expiry boundary is one authoritative shared P&P time for the whole race.
At that boundary, each entry may complete its next eligible crossing under the
Finish Current Lap rules. Every eligible final crossing contributes to that
entry's authoritative result. A lane's finishing offset/overtime is measured
from the same shared expiry point; it is not calculated from a separate lane
clock.

<a id="race-modes-product-spec--43-live-and-result-information"></a>
##### 4.3 Live and result information

Time remaining is prominent.

Typical live information includes:

- time remaining;
- position;
- current lap;
- last lap;
- best lap;
- gap;
- sectors where fitted.

Results include position, completed laps, appropriate total/finishing time, best lap, fastest lap overall, penalties and sectors where available.

<a id="race-modes-product-spec--44-endurance-penalties"></a>
##### 4.4 Endurance penalties

False-start choices may include:

- Off;
- Warning;
- -1 lap in the final result;
- Power penalty where a Power Module is fitted.

A generic post-race +10 second penalty is not appropriate to fixed-duration scoring and should not be offered merely for consistency with other modes.

<a id="race-modes-product-spec--45-countdown-warnings"></a>
##### 4.5 Countdown warnings

Time-expiry warnings are configurable.

The approved audio warning options and factory defaults are defined in [approved shared audio settings](#product-facilities-spec--35-preferences-defaults-and-testing). Any additional visual warning style is a presentation choice, not a second warning schedule.

At zero:

- **Stop at Zero:** announce/display race over.
- **Finish Current Lap:** announce/display time expired/final lap and continue until each competitor finishes.

<a id="race-modes-product-spec--46-cross-client-timing-and-presentation-contract"></a>
##### 4.6 Cross-client timing and presentation contract

Race Control, Race Engine and P&P System Time remain the only authorities for
GO, elapsed/remaining duration, lap timing, expiry, finish times and overtime.
The Browser and Taster consume the same authoritative State and presentation
events/facts; either client may advance or render displayed clock values from
authoritative P&P timing, but neither client independently establishes race
timing, GO, pause, expiry, finish or overtime authority.

PAUSE and every approved restart countdown apply to the whole session. Time
spent paused or in the restart countdown does not add racing time. RESUME
continues from the authoritative timing boundary already established by P&P.

The shared display conventions are below. The physical 800 × 480 Integrated Display deliberately uses the accepted compact MM:SS whole-second live and frozen Lap Race clock instead of Browser whole-second live values / hundredths finished duration. This is a formatting exception only; authoritative duration excludes paused/restart time. Endurance overtime follows the shared rules.

- live elapsed/remaining race timers use whole seconds and remain in seconds
  beyond `3599`; they continue as `3600`, `3601`, and so on without resetting
  or changing units;
- completed race/finish durations use zero-padded `mm:ss:xx`, where `xx` is
  hundredths and the minute field continues beyond `59` (for example,
  `60:01:23` means 60 minutes and 1.23 seconds);
- Endurance post-zero timing uses the one shared expiry point: the live value
  is `+0`, `+1`, `+2` and so on in whole seconds;
- when a lane completes its first eligible final crossing, its overtime offset
  freezes to hundredths as `+mm:ss:xx` (for example, `+00:18:42`). The other
  lane continues to its own final crossing, and both frozen offsets remain
  visible at finish;
- live lap/gap fields retain their common field definition and short-duration
  precision, such as `8.21 s` or `+0.26 s`; these are not alternate race
  clocks.

Changing numeric fields use stable, fixed-width regions. Presentation updates
are event/state driven: a client may animate a genuine active event, but it
must not turn a continuously changing display into a second timing authority
or rebuild unrelated layout regions.

All of these are presentation values derived from authoritative P&P timing,
not separate client timers. Implementations must reserve enough fixed-width
space for unbounded minute values, the `+` overtime prefix and both frozen
lane offsets so changing values never move or truncate adjacent fields.

<a id="race-modes-product-spec--5-timed-stage"></a>
#### 5. Timed Stage

<a id="race-modes-product-spec--51-normal-loop-stage"></a>
##### 5.1 Normal loop stage

For a normal loop/circuit Timed Stage stage:

1. Car is positioned behind the Start/Finish sensor.
2. Start sequence runs.
3. GO makes the stage live but the official stage timer remains at zero.
4. The MUG launches.
5. The first Start/Finish crossing starts the official timer.
6. The car completes the configured number of stage laps.
7. The final required Start/Finish crossing stops the timer.

Reaction time is not included in the stage time.

Crossing the sensor before GO is a false start.

A stage may be one or multiple laps: 1, 2, 3 or Custom.

Individual lap times are recorded automatically as splits, while the official stage result is the total stage time.

<a id="race-modes-product-spec--52-single-and-multi-stage-timed-stage"></a>
##### 5.2 Single and multi-stage Timed Stage

**Single Stage:** one timed stage/run; fastest adjusted time wins.

**Multi-stage Timed Stage:** several stages/runs; lowest cumulative adjusted total wins.

The same physical track may be reused as nominally different stages.

Attempts/runs per stage may be 1, 2, 3 or Custom.

Scoring choices include:

- All runs count;
- Best run counts.

A Drop Worst option is not required unless a real product need emerges.

<a id="race-modes-product-spec--53-point-to-point-and-hill-climb"></a>
##### 5.3 Point-to-point and Hill Climb

P&P also supports an A-to-B timing engine:

**GO arms run → first sensor starts timing → second sensor stops timing.**

Sensor roles can be reversed for another stage, allowing A→B and B→A without moving sensors.

This underlying timing behaviour can support point-to-point Timed Stage, Hill Climb, Sprint and similar A-to-B uses.

Hill Climb is not currently specified as a fake technically distinct timing mode merely for marketing. Presentation/naming may later provide discipline-specific presets where useful.

<a id="race-modes-product-spec--54-dnf-handling"></a>
##### 5.4 DNF handling

DNF scoring is configurable and may include:

- fixed DNF time;
- slowest completed time + configurable penalty;
- exclude from overall classification;
- retry stage.

A DNF does not automatically prevent the MUG from taking part in later stages.

<a id="race-modes-product-spec--55-event-organisation"></a>
##### 5.5 Event organisation

A Timed Stage can contain:

- Timed Stage name;
- MUGs;
- number of stages;
- per-stage settings.

Each stage may differ, for example loop N laps or A→B/B→A.

P&P maintains the cumulative classification.

Supported running orders:

- **Stage order** — all MUGs complete Stage 1, then Stage 2 etc. This is the normal/default organisation.
- **MUG order** — MUG 1 completes all stages, then MUG 2 etc. This is an Advanced option useful where the physical track remains unchanged.

<a id="race-modes-product-spec--56-timed-stage-results"></a>
##### 5.6 Timed Stage results

The main live/default result information is:

**Position | MUG | Stage time | Penalty | Timed Stage total | Gap**

After each stage P&P can show the stage winner/fastest and updated overall classification.

Lap splits remain expandable rather than cluttering the main table.

Final classification is by lowest cumulative adjusted time.

Secondary statistics may include fastest stage, stage wins, best lap/split, penalties, DNFs and retries.

Detailed Results provide a stage-by-stage matrix.

After a stage the primary action is **NEXT STAGE**; at the end it becomes **EVENT RESULTS**.

<a id="race-modes-product-spec--6-drag"></a>
#### 6. Drag

<a id="race-modes-product-spec--61-base-hardware-use"></a>
##### 6.1 Base hardware use

The standard two-sensor P&P package supports:

- ordinary two-lane Lap timing using one Start/Finish sensor per lane;
- **single-lane Drag** by repurposing Sensor 1 as Start and Sensor 2 as Finish.

<a id="race-modes-product-spec--62-base-staging"></a>
##### 6.2 Base staging

Base single-lane Drag uses the Start sensor itself to establish the staging point:

1. MUG moves the car forward until the Start sensor is triggered.
2. **STAGE** illuminates: the beam/start line has been found.
3. MUG rolls back slightly until the sensor clears.
4. **STAGE** extinguishes and the run is armed/ready.
5. The race cannot start while the Start sensor remains blocked.
6. At GO the MUG launches.
7. The car triggers the Start sensor almost immediately.
8. That trigger starts ET.
9. The Finish sensor stops ET.

The rollback amount is deliberately left to the MUG's hand/skill.

<a id="race-modes-product-spec--63-reaction-and-et"></a>
##### 6.3 Reaction and ET

Base reaction time is:

**Start-sensor trigger time - GO time**

This is not a physical first-motion measurement, but is a valid and useful base-system reaction figure.

Reaction time and ET are separate results.

The base display presents reaction and ET to **0.01 s**. Internal timing may be finer. Greater displayed precision may be associated with higher-performance timing hardware.

<a id="race-modes-product-spec--64-false-start"></a>
##### 6.4 False start

Triggering the Start sensor before GO is a false start.

The run may continue so ET is still recorded. Reaction time may therefore be negative.

<a id="race-modes-product-spec--65-start-tree"></a>
##### 6.5 Start tree

Drag supports:

- **Sportsman** — ambers illuminate sequentially;
- **Pro** — ambers illuminate together, followed by green.

<a id="race-modes-product-spec--66-base-results-and-speed"></a>
##### 6.6 Base results and speed

Base Drag results include:

- reaction;
- ET;
- false-start status.

If track length is entered, P&P may calculate whole-run average speed.

This is **not** presented as trap speed.

<a id="race-modes-product-spec--67-future-dual-beam--reu-concept"></a>
##### 6.7 Future dual-beam / REU concept

A future higher-performance dual-beam module is parked, not part of the base design.

Provisional concept:

- two optical beams approximately 10 mm apart;
- at Start: Beam 1 = pre-stage, Beam 2 = stage;
- at Finish: Beam 1 = trap start, Beam 2 = finish/trap end;
- fixed known spacing avoids a separate trap-distance setup.

The 10 mm spacing is provisional and requires prototype validation.

This future option must not derail or complicate the base Drag implementation.

<a id="race-modes-product-spec--68-drag-display"></a>
##### 6.8 Drag display

Drag deliberately does not use the common Lap/Endurance/Timed Stage scoreboard hierarchy and gets its own recognisably drag-racing visual identity.

The display progression is:

**STAGING → TREE → RUN → RESULT**

The start display uses a proper drag-racing **Christmas Tree** rather than reusing the common five-light Race Control Pod.

The full tree layout remains visible with hardware-dependent elements shown in place. On the base single-beam system, **PRE-STAGE is not falsely simulated**: the PRE-STAGE lamp/line remains visible but greyed/dormant because that measurement requires the future dual-beam REU. The normal STAGE indication remains active for the base staging procedure.

This follows the wider P&P product rule that expansion capabilities should normally remain visible but locked/dormant rather than disappear.

During staging and the start sequence, the tree becomes visually dominant and large enough to read while the MUG is concentrating on the car.

The three amber lamps operate according to the selected **Sportsman** or **Pro** sequence. Green indicates a valid GO; red indicates a false start.

After GO, the tree remains visible with the relevant green/red state while reaction time appears as soon as the Start sensor is crossed and ET runs prominently.

At Finish, the display transitions immediately to the run result, including:

- reaction time;
- ET;
- false-start/red-light status where applicable;
- whole-run average speed where track length is known.

Example valid result:

`REACTION 0.23 | ET 6.84 | AVG 42.7 mph`

Example false start:

`RED LIGHT | REACTION -0.08 | ET 6.71`

<a id="race-modes-product-spec--69-base-drag-staging-screen-behaviour"></a>
##### 6.9 Base Drag staging-screen behaviour

The base staging interaction is deliberately simple:

1. **Waiting** — full tree visible; REU-only PRE-STAGE remains greyed/dormant; STAGE is unlit. Display prompts **MOVE FORWARD TO STAGE**.
2. **Start beam blocked** — STAGE illuminates and the prompt becomes **ROLL BACK**.
3. **Beam clears** — STAGE extinguishes because the car is now physically just behind the single beam. A prominent **READY** indication confirms that staging has succeeded and the tree is armed.
4. **Start sequence** — staging instructions disappear and the Christmas Tree becomes visually dominant. The selected Sportsman or Pro sequence runs to GREEN.
5. **Start crossing** — reaction time appears as soon as the car crosses the Start beam and ET begins.
6. **Finish** — the run result takes over, showing reaction, ET, false-start status and whole-run average speed where available. **RACE AGAIN** is the primary post-run action.

If the car rolls forward and blocks the Start beam again after reaching READY but **before the start procedure begins**, P&P simply returns to the **ROLL BACK** staging state. This is not a false start.

Once the start procedure has begun, a Start-sensor crossing before GREEN is a **RED LIGHT / false start** according to the agreed Drag rules.

<a id="race-modes-product-spec--610-base-drag-result-screen"></a>
##### 6.10 Base Drag result screen

The base Drag result screen prioritises the run result and avoids unnecessary controls.

For a valid run, **ET is the dominant figure**, with reaction time and whole-run average speed beneath it where track length is known.

Example: `ET 6.84 | REACTION 0.23 | AVG SPEED 42.7 mph`

For a false start, **RED LIGHT** becomes dominant while reaction and ET remain visible.

Runs are stored automatically; there is no routine SAVE button.

The primary post-run action is **RACE AGAIN**, with **NEW SETUP** secondary.

Achievements such as **NEW BEST ET** or **BEST REACTION** may be shown briefly without another results screen.

The result screen deliberately exposes useful REU-only statistics in a greyed/locked state, including:

`TRAP SPEED — REU required`

Selecting the locked statistic may explain that the REU Drag Sensor Pack adds the additional finish beam required to measure speed through the final speed trap.

This follows the same principle as greyed PRE-STAGE: never fake an unavailable measurement, but show the MUG what additional hardware would provide.

**Base Drag display product design is complete at this level.** Further REU/two-lane competitive Drag display behaviour is deferred until that hardware is designed.

<a id="race-modes-product-spec--7-general-principle"></a>
#### 7. General principle

Where an additional software option costs essentially nothing and does not make the normal experience confusing, P&P should avoid artificial restriction. Less-common choices can live under **Advanced**.

> **Easy to start. Deeply configurable when you want it.**


<a id="race-modes-product-spec--browser-setup-flow-refinements"></a>
#### Browser setup flow refinements

<a id="race-modes-product-spec--timed-stage"></a>
##### Timed Stage
Timed Stage is the generic top-level mode for timed runs such as rally stages, hill climbs and sprints.

Initial stage geometry is:
- **Loop** — same Start/Finish point; first crossing starts timing and the final crossing after the configured lap count finishes it.
- **Point-to-Point** — separate Start and Finish sensors; direction may be A→B or B→A.

Timed Stage may be a **Single Stage** or **Multi-Stage Event**. A Multi-Stage Event layers stage sequencing and cumulative results on the same timing engine.

Use **Runs**, not Attempts, in the user interface.

P&P does not need the MUG to declare whether a point-to-point run is technically a rally stage, hill climb or sprint merely to time it.

<a id="race-modes-product-spec--drag-setup"></a>
##### Drag setup
Drag must work even when course length and scale are unknown. Reaction and ET remain valid.

Optional Drag Course Length and Track Scale add derived information such as actual average speed and scale-equivalent distance/speed.

If useful optional values are missing, SMUG may enter them directly in Drag setup rather than leaving the mode to visit Track Setup. Values entered contextually update the same underlying configuration rather than creating duplicates.

Drag Course Length is distinct from ordinary circuit lap length: it is the physical Start-sensor-to-Finish-sensor distance.

Sportsman and Pro tree choices should remain visibly selectable; Sportsman is the understandable default.

<a id="product-facilities-spec--9-demonstration-and-attract-facilities"></a>
#### 9. Demonstration and Attract facilities

P&P should provide demonstration facilities as a product capability, distinct from normal race operation. These facilities are intended for showroom/exhibition presentation, user familiarisation, development testing and fault diagnosis.

The facilities are deliberately separated into **Attract Mode**, **Automatic Demo Race** and **Manual Demo/Test**, with external automated acceptance testing using the same simulated-input boundary.

**Owner decision — 19 November 2026:** Normal racing receives physical detector crossings; manual simulation receives SMUG-generated crossings; automatic Demo receives crossings from a small internal routine; automated acceptance receives crossings generated by scripts on the PC. All use the same Race Control, Race Engine, timing, rules, presentation, Browser and audio paths. Only the source of detector events differs, apart from the required non-official classification. External test sequences and expected results reside on the PC, not in firmware. Both standalone and SMUG-directed presentation paths must be testable without substituting the wrong control role. The precise test ingress, demo start/exit, and customer controls remain design decisions.

<a id="product-facilities-spec--91-attract-mode"></a>
##### 9.1 Attract Mode

Attract Mode is a presentation facility analogous to the demonstration displays used on retail hi-fi and similar equipment.

It is not intended to be generally available as an ordinary race mode. A SMUG-controlled configuration option enables or disables it.

When enabled, an idle P&P unit may automatically run a short repeating presentation sequence. The sequence may demonstrate the product using lightweight graphics and animation, for example:

- the P&P logo/identity;
- a simple track drawing itself;
- small animated cars;
- start lights;
- lap-count and timing displays;
- fastest-lap callouts;
- a close finish and chequered flag;
- results;
- short visual demonstrations of supported race types and expansion capability.

Attract Mode is presentation, not authoritative race simulation. Its race sequence may therefore be deliberately scripted and choreographed for clarity and visual interest.

The display implementation should favour simple, lightweight graphical primitives, text, icons and small bitmap/sprite assets suitable for an ESP32-class controller. Presentation work must remain subordinate to authoritative race processing and must never compromise race timing or control.

The exact display hardware, resolution, artwork, animation style, sequence length and wake/exit behaviour remain deferred design decisions.

Attract Mode may ultimately be available on the local Taster display and/or connected Browser displays. That presentation scope is deliberately not fixed yet.

<a id="product-facilities-spec--v1-attract-presentation-guidance"></a>
###### V1 Attract presentation guidance

The initial implementation should use the following approximately 35-second loop as a practical starting storyboard:

- **0–3 s:** P&P / PLONK & PLAY identity;
- **3–6 s:** a simple track draws onto the screen, followed by two lane lines and small car sprites;
- **6–9 s:** start-light sequence followed by GO;
- **9–18 s:** a short choreographed race with changing lap/timing information, cars changing relative position and a FASTEST LAP moment;
- **18–22 s:** close finish, chequered flag and winner/gap presentation;
- **22–30 s:** brief feature flashes illustrating **RACE → RALLY → DRAG** and **2 → 4 → 6 → 8 LANES**;
- **30–35 s:** return to P&P / PLONK & PLAY identity, pause briefly, then repeat.

This storyboard is **presentation guidance for V1, not a frozen behavioural contract**. Timing, transitions, wording, sprites, artwork, sound effects and individual beats may be tuned once the sequence is seen on real display hardware. Such presentation tuning does not require an architectural/product-specification change provided the purpose and boundaries of Attract Mode remain unchanged.

A final P&P logo is not a dependency for V1. Until a satisfactory logo is designed, the presentation may use a clean **P&P / PLONK & PLAY** text identity.

<a id="product-facilities-spec--92-automatic-demo-race"></a>
##### 9.2 Automatic Demo Race

Automatic Demo Race is different from Attract Mode: it is a genuine P&P race driven by simulated detector activity.

Virtual detector events must enter at the defined sensor/input boundary so that the normal downstream product path is exercised. Race Control, Race Engine, Message Bus, Noticeboard, displays, audio and results processing should operate as they would for physical detector events.

The simulator may create plausible virtual competitors with slightly varying lap times. Demonstrations may be tuned to produce interesting, reasonably close racing, but the result should be determined by the real race logic rather than by a separately scripted fake result.

Automatic Demo Race may be initiated through SMUG-controlled demonstration facilities. It is not an ordinary Taster race option that a MUG can accidentally select.

<a id="product-facilities-spec--93-manual-demotest"></a>
##### 9.3 Manual Demo/Test

A manual demonstration/test facility may expose virtual versions of the installed detector inputs through SMUG.

For example, a two-lane Lap Race installation may provide virtual controls equivalent to:

- Lane 1 Start/Finish detector;
- Lane 2 Start/Finish detector.

Additional configured detectors may be represented where appropriate.

Activating a virtual detector should inject the corresponding simulated event at the same defined simulation boundary used by Automatic Demo Race, exercising the real downstream system.

This provides a useful diagnostic boundary. If a virtual detector produces the expected race behaviour while the corresponding physical detector does not, investigation can concentrate on the physical sensor/input side rather than the downstream race system.

<a id="product-facilities-spec--94-demo-session-integrity"></a>
##### 9.4 Demo-session integrity

Simulated race sessions must be authoritatively identifiable as **DEMO**, not merely labelled by presentation code.

Demo results must not contaminate genuine sporting records, including personal bests, track records, championship results or equivalent persistent competitive data.

Displays should identify simulated operation clearly enough that a demo cannot reasonably be mistaken for a live race.

The Demo exclusion above is independent of the WP8 standalone-Taster policy:
both are non-official, but a standalone Taster race is a real local race rather
than a simulated/demo session and must remain distinguishable in the frozen
session source.

**No simulated session is recorded as a race.** Manual simulation, internal Demo and externally automated tests must never write official History, Personal Bests, Track Records, championship results or persistent race-detail records. Live timing/results may be presented normally; external acceptance evidence and operational diagnostic logging are not sporting records. Only normal physical-detector racing is potentially record-eligible, subject to the existing official-session source policy (normal standalone Taster races remain non-official).

Physical and simulated detector events must not accidentally mingle within an authoritative race. Exact behaviour when real detector activity occurs during an Automatic Demo Race remains a later design decision; possible policies include cleanly terminating the demo or ignoring physical detector activity for that demo session.

<a id="product-facilities-spec--95-architectural-principle"></a>
##### 9.5 Architectural principle

Demo capability must not create a second fake Race Engine.

Where the purpose is to demonstrate or test real P&P behaviour, simulation occurs at the sensor/input boundary and everything downstream remains normal product behaviour.

Attract Mode is the deliberate exception because it is presentation rather than race simulation.

<a id="stage-13-design--stage-13-design--complete-lap-race"></a>
### Lap Race detailed contract

**Contract:** Lap Race start, false-start, abandonment and record semantics; implementation status is in the Master Project Plan.

Stage 13 completes the ordinary two-lane Lap Race boundary. It adds the authoritative start-light sequence, generic false-start reporting with Lap Race consequences, destructive post-pause abandonment operations, persistent completed-race History, and persistent Lap Race PB/Track Record foundations.

Existing Stage 1–12 authority, timing, Message Bus, Browser identity, settlement, integrity and result boundaries remain in force. Stage 13 introduces no supersession.

<a id="stage-13-design--start-sequence"></a>
#### Start sequence

The factory/default Lap Race start is five red lights, one second between successive red-light instants, a random final delay selected once in the range 0.5–2.0 seconds after the fifth red, and Lights Out as GO. Race Control commits the complete schedule and authoritative GO instant when START is accepted. Browser, audio and physical presentation consume that schedule and never determine GO.

The configured sequence supports three or five reds, Lights Out or Green GO, and Immediate, Fixed or Random final delay. Red-to-red interval is an Advanced setting. The selected random value is committed once per sequence. The implementation may use a deterministic seeded source in acceptance builds, provided product behaviour remains random within the configured range in normal operation.

The sequence completes and GO occurs even if every Browser disconnects or sleeps. A qualifying crossing with Relevant Time `t < GO` is a false start; `t >= GO` is legal, including exactly GO.

<a id="stage-13-design--false-starts"></a>
#### False starts

False-start detection is offered only when the accepted Session Definition contains an installed/configured capability that can authoritatively detect the relevant competitor crossing before GO. The normal base/Taster sensors-before-Start/Finish arrangement does not claim this capability.

The generic event boundary identifies the affected Race Entry and authoritative Relevant Time. Input hardware and the Input Module do not apply race consequences. Lap Race applies one of:

- OFF — the false-start event may be recorded diagnostically, but produces no competitor warning, penalty or change to race outcome;
- WARNING — record and present the false start without changing required distance;
- +1 LAP — increase that entry's required genuine race distance by one lap.

Repeated +1 LAP false starts accumulate. Completed-lap counts are never falsified; a nominal ten-lap race with two penalties requires twelve genuine completed laps. GO is not aborted, rescheduled or restarted by a false start.

The generic event/consequence boundary remains suitable for later Endurance, Timed Stage and Drag policies, which are outside this stage.

<a id="stage-13-design--pause-abandonment-operations"></a>
#### Pause abandonment operations

Restart Race and End Race are Race Director-only operations available only after PAUSE and the existing local pre-pause settlement fence have completed. They are not direct RACING controls.

Opening or cancelling a confirmation has no authoritative effect. Once confirmed and accepted:

- Restart Race abandons the interrupted race, creates no completed History entry, retains/seeds the same proposed setup, returns to READY, and causes the next START to create a fresh Session Definition and fresh Race Entry IDs with competition at zero.
- End Race abandons the interrupted race, creates no completed History entry, and returns to Home.

If a delayed eligible pre-pause event completes the race before acceptance, FINISHED wins and neither operation may destroy the completed race.

<a id="stage-13-design--persistent-history"></a>
#### Persistent History

Each settled completed race eligible for official History under [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy) is stored automatically as a faithful persistent representation of the sealed Result, subject to storage integrity. A persistent sequence identity orders records newest first and remains unique across power cycles, Race Again and rolling deletion. Civil time is optional metadata only when supplied by a trustworthy source; ordering never depends on an RTC.

History contains enough immutable Session Definition, result, ranking, W/F, finish-behaviour and lap-record data to reconstruct Stage 12 Results and Details. Abandoned races are absent. A substantial implementation-selected rolling capacity may evict the oldest record. Clear History is deliberate and separate from record clearing.

Persistent storage failure never changes, invalidates or recalculates authoritative timing or competition. The implementation surfaces a storage warning/fault and retains or retries recoverable pending persistence where possible.

Controller restart recovers persistent History and records but does not reconstruct an interrupted active race, PAUSED State, pending restart or volatile Session Definition.

<a id="stage-13-design--pbs-and-track-records"></a>
#### PBs and Track Records

The factory track/competitor context is DEFAULT TRACK and generic LANE 1 / LANE 2. These identities are not an eligibility grant. An eligible controlling Browser session may use them; a standalone Integrated Display session is never official. See [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy).

A genuine valid completed timed lap in a record-eligible session may update the selected competitor PB and Track Record at lap completion, even if its containing race later ends through End Race. A lap is eligible when its own authoritative timing and validity are sound at the lap-completion boundary. A later race fault does not retrospectively invalidate an already-established eligible PB/Track Record unless that fault establishes that the lap itself was invalid. Honour-Restart timing is eligible; Grid-discarded incomplete progress and demo/acceptance simulated laps are not.

PB/record clearing supports individual MUG PB, Track Record, and all records for a Track. Each requires deliberate confirmation, preserves History, and starts a new persistent record era. Old History cannot resurrect a cleared record automatically. Clearing History does not clear PBs or records.

<a id="stage-13-design--boundaries-and-deferrals"></a>
#### Boundaries and deferrals

The original Stage 13 scope is historical, not a product limit. Current scope and outstanding work are in the [Master Project Plan](MASTER_PROJECT_PLAN.md). Race Control owns lifecycle; Race Engine competition and results; Input Module role-neutral events/settlement. Browser requests and presents; Facts do not become a History database.

<a id="stage-14b-design--stage-14b-design--open-practice"></a>
### Open Practice detailed contract

**Status: ACCEPTED / FROZEN**

**Implementation status: ACCEPTED — automated, regression and real-ESP32 Browser checkpoint passed on 2026-10-05.**

Final implementation/evidence close-out: commits `c49ffbe`, `2770a04`, `870998a` and `d3e9ae6`.

The human checkpoint verified real Browser selection/start, independent multi-lane timing, pause/resume, reconnect reconstruction, and non-official End Session behaviour. No Stage 14B acceptance blockers remain. Stage 14C is not included.

Stage 14B is the next implementation slice after the accepted Stage 14A variable-entry refactor. It implements Open Practice only. It does not implement Timed Practice, Endurance, Timed Stage/Rally, Drag, or any other new mode.

<a id="stage-14b-design--product-boundary"></a>
#### Product boundary

Open Practice is Practice, not an official race. It has no winner, ranking, finishing position, official result, PB update, Track Record update or official History entry. All Practice information is session-local and non-official.

The number of simultaneous Practice cars is the number of configured active entries, subject to the existing firmware implementation capacity. Two lanes remain the default/Taster configuration. The implementation capacity is not a product maximum.

Open Practice has no configured duration and no lap target. It continues until the Race Director ends the session.

<a id="stage-14b-design--existing-architecture-retained"></a>
#### Existing architecture retained

Open Practice uses the existing Race Control, Race Engine, Session Definition, Input Module, Message Bus and Noticeboard boundaries:

- Browser/Taster submits an authorised request through Browser Interface.
- Race Control owns authority and session lifecycle.
- START creates an immutable Practice Session Definition with the configured active entries, role mappings and selected Practice start option.
- Input Module publishes ordinary clean `INPUT_EVENT`s through the Message Bus.
- Race Engine interprets those events using the Session Definition and owns Practice timing/state.
- Noticeboard State is authoritative for reconstruction.
- Browser presence never affects Practice timing.

No parallel Practice timing path, Browser-to-Race-Engine path or Race-Control access to Input Module private state is permitted.

<a id="stage-14b-design--mode-and-lifecycle"></a>
#### Mode and lifecycle

The active lifecycle remains the existing `RACING` value with authoritative `sessionMode = OPEN_PRACTICE`. No new `PRACTISING` lifecycle value is introduced.

The operational flow is:

1. READY: no Practice session exists.
2. START accepted: the Practice Session Definition is created. Immediate start enters `RACING` directly. Practice start uses the existing authoritative `STARTING` lifecycle and normal five-light countdown/GO, then enters `RACING`; presentation must consume the same authoritative signals as circuit racing.
3. `RACING` plus `OPEN_PRACTICE`: normal Practice timing applies in either option. Immediate start has no GO requirement; Practice start admits timing only after GO. No separate Practice-start timing engine or competitive result is introduced.
4. PAUSED: Practice activity is stopped and incomplete timing is discarded.
5. RESUME accepted: Practice becomes active immediately and every lane waits for a new timing origin.
6. END SESSION accepted: Practice ends and the system returns towards READY/Home.

Practice never enters normal Lap Race `FINISHED` and never publishes a normal competition-completion result.

<a id="stage-14b-design--practice-timing"></a>
#### Practice timing

Each active entry independently stores:

- whether it is waiting for a timing origin;
- its timing-origin Relevant Time when established;
- completed lap count;
- last completed lap;
- best completed lap;
- session-fastest information where straightforward.

On entry to `RACING` (immediate start or authoritative GO after the optional Practice start), every lane waits for a timing origin. The first eligible Start/Finish crossing for a lane establishes that lane's timing origin. Each subsequent eligible crossing completes one lap using the Relevant-Time difference and establishes the next timing origin.

Relevant Time remains authoritative over delivery or processing order. Existing Input Module debounce/re-arm and normal Message Bus delivery guarantees remain unchanged.

Sector timing and theoretical-best calculations/presentation are explicitly optional for this slice. Their absence must not fail acceptance. They may be added later through the existing architecture.

<a id="stage-14b-design--pause-and-resume"></a>
#### Pause and Resume

Pause is deliberately simpler than Lap Race Honour/Grid Restart:

- Pause stops Practice activity.
- Completed Practice laps and statistics remain.
- Any incomplete lap is discarded.
- Crossings while paused do not advance Practice timing.
- Resume is immediate.
- After Resume, each lane waits for a new timing-origin crossing.
- The following crossing completes the next timed lap.

The existing Relevant-Time, Message Bus settlement/fence and authoritative event-boundary rules still apply underneath. Any internal boundary needed to classify events is an implementation detail and is not a new user-visible Practice concept.

<a id="stage-14b-design--operations-and-authority"></a>
#### Operations and authority

Use generic `Resume` and `EndSession` with existing `SESSION_OPERATION_REQUEST` / `SESSION_OPERATION` types. Race Control interprets according to active mode. `EndRace` represents competitive abandonment, not Practice completion.

No Practice-specific operation or Message Bus type is introduced. Existing Message Bus numeric values remain unchanged; any appended values are never renumbered.

Only Race Director/SMUG context may start, pause, resume or end Open Practice. Spectator and ordinary MUG/driver contexts cannot end the overall session. Forged Browser role fields cannot change this.

<a id="stage-14b-design--browser-and-taster"></a>
#### Browser and Taster

Open Practice must be selectable and operable through the real Browser/product path on the ESP32. The minimum diagnostic control layer is:

- an Open Practice mode selection;
- START;
- PAUSE;
- RESUME;
- END PRACTICE while Practice is authoritatively PAUSED;
- per-lane Practice state showing lap count, last lap, best lap and session-fastest information where available.

Polished UI is outside this slice.

The Integrated Display exposes no Practice controls; its restricted modes are governed by [standalone controls](#pp-taster-spec--standalone-controls).

The frozen normal Browser exposes **PAUSE** while Open Practice is active. Once
P&P reports authoritative PAUSED it exposes **RESUME**, **RESTART** and **END
PRACTICE**. **END PRACTICE** follows the authoritative Practice completion
path to Practice Summary; Practice does not expose competitive **ABANDON**.
These are Browser controls, not Browser-owned lifecycle transitions.

<a id="stage-14b-design--state-facts-and-presentation"></a>
#### State, Facts and presentation

Authoritative State includes session mode, lifecycle, active entry collection, per-entry lane/entry identity, completed laps, last/best Practice lap where available, pause information and existing integrity/validity fields.

No winner, rank, finish time, official result or result-sealed Practice state is created. Existing common PAUSED and RESUMED Facts may be reused where their semantics remain exact. Any Practice-ended Fact is optional and must use the normal Message Bus path.

The later approved [Practice Summary flow](#browser-flow-results-history-spec--practice-setup-and-control) requires a session-local summary on END SESSION. It is not a competitive CompletedRaceResult, History or record update.

<a id="stage-14b-design--persistence-history-and-records"></a>
#### Persistence, History and records

Open Practice does not create a CompletedRaceResult, write official History, update PBs, update Track Records, alter record eras or introduce a persistent Practice-results schema.

Existing controller-restart rules remain authoritative: an interrupted active Practice session is not reconstructed as an official result. Persistent Race Setup/configuration may continue to follow existing rules.

<a id="stage-14b-design--explicit-exclusions"></a>
#### Explicit exclusions

Open Practice has no winner/rank/competitive finish, official History/records or local Integrated Display mode. Timed Practice and future features follow the Master Project Plan, not historical slice limits.

<a id="stage-14b-design--compatibility-requirement"></a>
#### Compatibility requirement

All applicable Stage 1–14A behaviour and acceptance remains unchanged. The complete Stage 1–14A regression campaign is required for Stage 14B acceptance.

<a id="stage-14c-design--stage-14c-design--endurance"></a>
### Endurance detailed contract

**Status: ACCEPTED / FROZEN**

The Endurance contract extends the shared racing architecture. Current acceptance and release scope are in the Master Project Plan; earlier stages remain evidence.

<a id="stage-14c-design--product-boundary"></a>
#### Product boundary

Endurance is timed competition using the existing Lap Race mechanics. Eligibility for official results and records follows [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy); the mode alone does not confer eligibility. A configured duration replaces the Lap Race lap target. The active competitors are the configured active Race Entries; the implementation capacity is not a product maximum.

Other modes and optional features follow their own design sections and release scope in the Master Project Plan. Interrupted active races are not reconstructed after uncontrolled restart.

<a id="stage-14c-design--duration-and-setup"></a>
#### Duration and setup

- Duration is a whole number of minutes from 1 through 999. Seconds are not entered.
- The Browser duration control supports direct typing, up/down arrows and mouse-wheel editing consistently with the existing Lap numeric control.
- The selected duration is part of the immutable Session Definition.
- Duration is measured in Relevant Racing Time. Time while PAUSED does not consume it.
- The authoritative duration begins at GO.
- The implementation may represent the expiry internally as an authoritative Relevant-Time boundary, accumulated running intervals, or an equivalent representation, provided the observable boundary rules below are preserved.

<a id="stage-14c-design--architecture-and-authority"></a>
#### Architecture and authority

Endurance uses the existing Race Control, Race Engine, Session Definition, Input Module, Message Bus, Noticeboard, Browser, Results and History boundaries.

- Browser and Taster submit authorised setup and operation requests.
- Race Control validates authority, freezes the Session Definition, owns lifecycle and coordinates START, GO, PAUSE, RESUME and END RACE.
- Input Module publishes ordinary role-neutral detector events and settlement markers.
- Race Engine interprets detector events using the immutable Session Definition and owns lap timing, expiry, classification and results.
- Noticeboard exposes current authoritative state for Browser reconstruction.
- Facts remain events and are not a results database.

No Browser-to-Race-Engine shortcut, parallel timing path or mode-specific Message Bus operation is introduced. Existing generic `RESUME` is reused. Endurance manual termination uses the existing `END RACE` abandonment operation; it must not use Practice `END SESSION` semantics.

<a id="stage-14c-design--lifecycle"></a>
#### Lifecycle

Endurance uses the existing lifecycle values. It does not add a new lifecycle enum.

1. READY: setup can select Endurance, duration and finish policy.
2. START accepted: an immutable Endurance Session Definition is created.
3. STARTING: the normal configured countdown/start lights run.
4. GO: the authoritative Endurance duration clock starts.
5. RACING: lap timing and remaining duration are active.
6. PAUSED: the existing settlement fence applies and duration stops consuming Relevant Racing Time.
7. RESUME: the generic operation resumes the same race with its remaining duration.
8. FINISHED: only normal duration completion reaches official results.
9. END RACE: manual premature termination abandons the race and creates no normal completed Endurance result.

<a id="stage-14c-design--false-starts"></a>
#### False starts

The initial Endurance choices are:

- OFF — a diagnostic false-start event may be recorded, but it produces no warning, penalty or outcome change.
- WARNING — the event is recorded and presented without changing classified distance.
- -1 LAP — the penalty is applied to the final classified lap count. The factual completed-lap count remains unchanged, the classified count is `max(0, completed laps - 1)`, and Results/History preserve enough information to show both counts and the applied penalty.

The Lap Race `+1 LAP` required-distance rule is not reused for Endurance. Stop/Go is reserved for a later slice shared by Lap and Endurance.

<a id="stage-14c-design--lap-timing-and-retention"></a>
#### Lap timing and retention

Entries use the existing independent Start/Finish crossing and Relevant-Time timing machinery. Crossings are ordered by Relevant Time, never by delivery order. The authoritative practical lap count and required statistics are independent of detailed lap-record retention.

Endurance must not have a 16-lap product limit. The current bounded detailed-record array is an implementation constraint only. The proposed implementation retains a bounded recent/detail set sufficient for Results/Details and persistence resource limits, while maintaining authoritative lap totals, last/best/fastest values and classification independently. The chosen retention policy and measured resource cost must be documented in the implementation evidence; it must never silently change the classified lap count.

<a id="stage-14c-design--expiry-and-finish-policies"></a>
#### Expiry and finish policies

Endurance has exactly two normal finish policies.

<a id="stage-14c-design--stop-at-zero"></a>
##### Stop at Zero

At the authoritative expiry boundary, the race ends immediately. Only laps whose completion Relevant Time is less than or equal to expiry count. Classification is by final classified lap count; equal totals are ordered by the earlier Relevant Time of the last counted Start/Finish crossing.

<a id="stage-14c-design--finish-current-lap"></a>
##### Finish Current Lap

Eligibility is determined at the expiry boundary: the entry must already have a legitimate timing origin and be on that lap before expiry. A crossing exactly at expiry may complete that already-in-progress lap, but cannot establish a new timing origin or grant another post-expiry completion.

At expiry, an entry that had established a legitimate timing origin and was already on a lap may complete that lap at its first eligible post-expiry Start/Finish crossing. It cannot begin another counted lap. An entry without a legitimate pre-expiry timing origin/lap receives no post-expiry lap. Different entries may finish with different totals. Classification is by final classified lap count; equal totals are ordered by post-expiry finishing Relevant Time.

The main duration display reaches and remains at `0` seconds. Finish Current
Lap also exposes live authoritative overtime as `+0`, `+1`, `+2` and so on from
the one shared expiry point. Each lane's first eligible final crossing freezes
its offset to hundredths as `+mm:ss:xx`; the other lane continues to its own
crossing and both offsets remain visible at finish.

<a id="stage-14c-design--exact-relevant-time-boundaries"></a>
#### Exact Relevant-Time boundaries

- A crossing with Relevant Time before expiry is normal-time input.
- A crossing exactly at expiry completes a normal-time lap.
- A crossing exactly at expiry may complete the lap that was already legitimately in progress before expiry, but does not begin a new post-expiry lap or establish a new timing origin for one.
- A crossing delivered after expiry but carrying Relevant Time less than or equal to expiry is classified by that Relevant Time.
- A crossing with Relevant Time greater than expiry is post-expiry input.
- Each eligible entry receives at most one post-expiry completion crossing under Finish Current Lap, and eligibility requires a legitimate timing origin/lap established before expiry.
- PAUSE settlement and all existing event-boundary rules remain authoritative.
- Paused time does not advance the Endurance duration or overtime clock.

<a id="stage-14c-design--pause-resume-and-end-race"></a>
#### PAUSE, RESUME and END RACE

Endurance reuses the frozen LAP PAUSE settlement/fence and Relevant-Time timing semantics, including preservation of interrupted-lap timing across the normal resume path. Completed laps and valid statistics remain. The duration clock stops while PAUSED and resumes with the remaining duration after generic RESUME. Endurance does not import Open Practice's discard-partial-lap rule. An END RACE request is a manual abandonment and does not manufacture a normal result or History entry.

<a id="stage-14c-design--state-browser-and-taster"></a>
#### State, Browser and Taster

Authoritative State must expose enough information to reconstruct Endurance, including session mode, duration, finish policy, remaining duration, expiry state, overtime where applicable, lifecycle, per-entry identities, laps, last/best lap, integrity and validity.

The Browser provides the smallest real product path for selecting Endurance,
editing duration, selecting the two finish policies, START, PAUSE, RESUME,
RESTART and confirmed ABANDON, and observing remaining time, positions, laps
and relevant lap information. The normal Browser exposes PAUSE only while an
Endurance race is active; RESUME/RESTART/ABANDON are exposed after
authoritative PAUSED. Confirmed ABANDON invokes the existing END RACE
abandonment path. Presentation is derived from authoritative State and must
not calculate results.

The Taster exposes the restricted **LAPS** and **ENDURANCE** choices from
[Integrated Display](#pp-taster-spec--pp-taster-specification). Its ENDURANCE choice maps to this canonical Endurance
contract; it is not a separate TIMED race mode. The Taster preserves the
simple interaction model and does not add unrelated controls.

Warnings consume authoritative remaining time and the shared zero boundary. Configurable audio warnings follow [approved shared audio settings](#product-facilities-spec--35-preferences-defaults-and-testing), which superseded the original Stage 14C presentation limitation.

<a id="stage-14c-design--results-history-and-records"></a>
#### Results, History and records

Normally completed Endurance sessions seal an authoritative Result. Only record-eligible sessions under [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy) enter official History, subject to [persistent storage](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design). Stored results retain duration, finish policy, lap totals, ranking, finish/overtime information, penalties and valid lap details for reconstruction.

Manual END RACE produces no normal completed result or History record. Interrupted active Endurance remains volatile under the accepted Stage 11–13 controller-restart rules.

In an eligible session, valid completed Endurance laps may update PB and Track Record, including the valid final Finish Current Lap crossing. Partial/incomplete laps and non-official sessions are excluded; validity and integrity rules apply.

<a id="stage-14c-design--compatibility-and-open-implementation-choices"></a>
#### Compatibility and open implementation choices

No contradiction with the frozen Stage 1–14B architecture has been found. The following are implementation choices intentionally left open by this proposal, rather than product changes:

- exact internal duration/expiry representation;
- versioned result/persistence field layout;
- bounded detailed-lap retention strategy and measured capacity;
- exact Browser/Taster control layout within the required operations;
- whether an optional non-authoritative overtime summary is shown in every result view.

These choices must preserve every normative behaviour in this document and be covered by acceptance evidence. They must not introduce a product maximum or an Endurance-specific timing architecture.

<a id="stage-14c-design--cumulative-regression"></a>
#### Cumulative regression

Stage 14C acceptance requires the complete applicable Stage 1–14B automated, structural, shared-path, Wokwi and Browser regression campaigns, plus the deliberate evaluator-failure/recovery proof and a real ESP32 Browser/Taster human checkpoint. Stage 14C must not be marked accepted until those checks and the human checkpoint pass.


## Inputs and outputs

<a id="system-architecture--4-input-architecture"></a>
#### 4. Input architecture

<a id="system-architecture--41-input-module-and-input-devices"></a>
##### 4.1 Input Module and Input Devices

The **Input Module** is the P&P subsystem boundary through which physical, simulated or test competition input becomes clean standard P&P physical Input Events. It may contain any number of **Input Devices**. It owns the hardware/source-facing work required to identify which stable input capability produced a clean trigger and when. It does **not** assign lane, Start/Finish, sector, drag, speed-trap or other competition meaning to that input. The resulting physical Input Events are published onto the common P&P communications bus; the Race Engine interprets the events relevant to the active session using the fixed Session Definition.

An **Input Device** is one complete working source of physical input. It owns everything specific to that source that is required to produce a clean standard physical Input Event. For a ToF detector this can include sensor reading, thresholds, filtering, debounce/hysteresis, re-arming and device-specific protocol handling. A keyboard or test-harness Input Device performs its own equivalent source-specific handling.

Each Input Device translates its own native behaviour into the standard physical P&P input contract. Adding a new Input Device type must not require a central translator to be modified merely to understand that device's native output.

An intelligent remote device may perform some or all of this source-specific interpretation locally before the Input Module publishes the standard physical event. Physical distribution does not change the logical ownership: Input Devices are components of the Input Module, and their internal communication is not a separate P&P message merely because a particular implementation uses a remote or wireless device.

The Input Module publishes the clean physical Input Event with the stable input/capability identity and original event time. It does not convert that identity into a racing role. The Race Engine uses the active Session Definition to interpret the input identity for the current session. It therefore does not need to know whether the event originated from ToF hardware, a keyboard, a test harness or another future source, nor which physical transport carried it.

<a id="system-architecture--42-detector-events"></a>
##### 4.2 Detector events

For the first detector-style input contract, the standard P&P event is a clean trigger with a stable input/capability identity and timestamp, conceptually:

```text
INPUT ID : TIMESTAMP
```

The timestamp represents when the clean physical trigger occurred as closely as practical, not when a communications packet happened to arrive at Race Control.

Signal conditioning, debounce, hysteresis, clearing and re-arming belong close to the detector. **Raw sensor samples, interrupt chatter and pre-conditioning transitions are internal to the Input Device and are not published onto the P&P Message Bus.** Only after the Input Device has recognised a clean meaningful trigger does the Input Module publish the standard physical `INPUT_EVENT`. The Input Device may need to observe both detection and clearing internally, but the first P&P detector contract does not publish a subsequent INACTIVE/clear event merely to report that re-arming has occurred. Once the meaningful trigger has been published, that event's work is done. Race meaning remains outside the Input Module.

A future input type may define different event/state semantics where they are genuinely required.

A race-domain plausibility rule such as rejecting an impossibly short lap is not detector debounce or signal conditioning; it belongs to the race/timing logic interpreting otherwise valid clean detector events.

Detector-specific conditioning parameters are owned by the detector/adapter. They need only become P&P installation configuration if the product requires them to be user- or installation-configurable; otherwise they may remain implementation details of that detector/adapter.

<a id="system-architecture--43-extensible-events"></a>
##### 4.3 Extensible events

The higher-level event boundary must not assume that every future input is necessarily a detector state or lane event.

Conceptually an event can carry:

```text
source
event type
timestamp
event data
```

This allows future event types to be added without redefining unrelated existing components.

<a id="system-architecture--44-device-roles-and-session-interpretation"></a>
##### 4.4 Device roles and session interpretation

Source-specific translation is already complete inside the Input Device. Assignment of a stable Input Device/capability identity to a racing purpose is **session-role information**, not an Input Module responsibility and not a separate module or bus participant.

Before START, working configuration and Race Setup may propose how available capabilities will be used. When START is accepted, the roles required for that session are frozen into the Session Definition. During the session the Input Module continues to publish the same device/capability event regardless of its assigned racing role; the Race Engine interprets that identity using the active Session Definition.

The Session Definition therefore translates a stable input capability into its role for this particular session.

Example:

```text
ABC123:1 : 123.456
        ↓
Lane 1 : START_FINISH : 123.456
```

The Input Module does not know this assignment. For example, `ABC123:1 : 123.456` remains the same physical Input Event whether the active Session Definition assigns `ABC123:1` as Lane 1 Start/Finish, a sector detector, a drag finish or a speed-trap input.

The Race Engine combines the physical Input Event with the active Session Definition and current competition state to determine its racing significance. Lane identity is therefore not required in the Input Module event.

The same physical detector can be reassigned to a different role for a later session without becoming a different kind of sensor or changing the Input Module contract.

<a id="system-architecture--45-transport-independence-and-transport-adapters"></a>
##### 4.5 Transport independence and Transport Adapters

An Input Device is defined by the standard P&P Input Events it produces, not by whether it is local, wired or wireless.

A local detector, a wired remote detector and a wireless intelligent detector may therefore all publish the same standard detector event. Their different communications mechanisms are handled by **Transport Adapters** at the edge of the common P&P communications bus.

A Transport Adapter bridges a physical or software communications mechanism to the logical P&P bus. It may support, for example, an in-process connection, GPIO/I2C-connected hardware, ESP-NOW, Wi-Fi/WebSocket or a future wired remote transport. It has no race meaning and must not reinterpret an Input Event as a lap, false start or other competition fact.

Transport Adapters may be bidirectional where required for discovery, configuration, status, time synchronisation or updates. Remote timestamping must remain reliably related to P&P System Time, and remote-device availability is represented through the normal Discovery/Registry responsibilities.

> **Transport changes how a P&P message travels, not what the message means.**

<a id="system-architecture--46-separation-of-knowledge"></a>
##### 4.6 Separation of knowledge

The intended separation is:

> **Each Input Device knows how to produce a clean standard physical P&P Input Event.**  
> **The Input Module publishes which stable input/capability produced the clean event, and when; it does not assign race meaning.**  
> **The active Session Definition assigns session-specific roles; the Race Engine interprets input events against those frozen roles.**  
> **Race Control owns session operation; the Race Engine owns competition interpretation.**

<a id="system-architecture--10-output-architecture"></a>
#### 10. Output architecture

The output path mirrors the input separation:

```text
Race Control / other authorised producer
     ↓
Session/race responsibility resolves required role using Session Definition
     ↓
Logical action addressed to stable Output Device/capability on the P&P Message Bus
     ↓
Output Module / Output Device hardware abstraction
     ↓
Physical hardware
```

**Output role assignment is session data, not a separate module.** When START is accepted, the relevant output roles are frozen into the Session Definition. A race/session responsibility can therefore resolve a semantic need such as `Lane 2 Track Power` to the stable output capability assigned to that role for this session, then publish the resulting device-level logical action. The Output Module does not need to know what racing role that output is serving.

Each Output Device owns the hardware-specific implementation required to carry out its standard P&P device action. Race Control and the Race Engine may know the stable output identity selected by the Session Definition, but they do not know GPIOs, addresses or device-specific command protocols.

<a id="system-architecture--101-time-critical-outputs"></a>
##### 10.1 Time-critical outputs

Where timing matters, outputs should be scheduled against P&P System Time rather than relying on command-arrival time.

A start sequence may therefore be transmitted ahead of time with authoritative execution times. A capable local output device can queue and execute those actions against its synchronised clock.

<a id="system-architecture--102-failure-behaviour"></a>
##### 10.2 Failure behaviour

Every output that materially affects racing requires a defined behaviour if its controlling device fails.

The precise failure behaviour of particular products, including track-power hardware, is a later hardware/product decision.

<a id="system-architecture--103-track-power"></a>
##### 10.3 Track power

Track power is architecturally supported as a logical output capability.

It only qualifies as a Plonk & Play™ product feature where the physical installation can itself be made genuinely simple, for example through a suitable plug-in power-control accessory rather than requiring the customer to cut and splice track wiring.

<a id="product-facilities-spec--2-power-module"></a>
#### 2. Power Module

Track-power control is an optional hardware capability.

Without a Power Module, P&P can still detect, report and score behaviours such as false starts and penalties, but it cannot physically cut, hold or otherwise control lane power.

Where a Power Module is fitted, modes may offer appropriate power-control responses.

Hardware-dependent options should normally remain visible but locked/greyed with a concise explanation such as **Power Module required**, rather than disappearing and concealing the system's expandable capability.

Reduced-power Yellow operation is only a valid product option if the eventual Power Module hardware genuinely supports controlled reduced power.


## Communications and system time

<a id="system-architecture--8-pp-system-time"></a>
#### 8. P&P System Time

P&P System Time is a **shared service**, not an operational module. It provides the common authoritative time domain used by modules and devices that need to describe, compare or schedule instants.

The common P&P communications bus transports messages that may contain timestamps or scheduled instants; System Time gives those values their common meaning. The bus does not itself establish timing authority.

The System Controller is the authority for the common P&P time domain. P&P System Time defines common instants; it does not require every distributed local clock to display the same numerical value.

Intelligent devices may use local high-resolution clocks. Where timing information must be compared across devices, local timestamps must be reliably relatable or convertible to P&P System Time with sufficient accuracy for the function.

Important input events should be timestamped at or near their source so communications latency does not determine official timing. Local intervals may be measured using a device's own stable clock.

Straightforward synchronisation is preferred where practical. A known clock relationship is also acceptable, including measured offset and drift correction or rechecking where required. Approximate clocks whose effective timing reference is packet arrival are not acceptable.

The specific synchronisation algorithm is deliberately not chosen. This requirement is not justification for unnecessarily elaborate clock-synchronisation machinery. **In a single-System-Controller implementation, the controller's suitable high-resolution monotonic clock may directly provide P&P System Time; no clock-synchronisation protocol is required merely to satisfy the abstraction.** Offset/drift synchronisation is required only where another timing source has an independent clock whose timestamps must share the P&P time domain.

Across defined P&P boundaries, timestamps must use one defined P&P representation with sufficient resolution for the supported timing functions and sufficient range that representation rollover cannot occur during any supported session. Local hardware clocks may use different native units, widths or rollover behaviour; conversion into or out of the P&P representation belongs at the appropriate boundary. The exact P&P unit and integer representation are selected during implementation design, but must be common at those boundaries.

The same common time domain supports scheduled time-critical outputs. A scheduled action such as GO at a specified P&P time denotes one common instant; a remote module may convert that instant to its local clock and execute locally.

<a id="system-architecture--133-identity-and-low-level-addressing"></a>
#### 13.3 Identity and low-level addressing

Anything P&P needs to distinguish must have unambiguous stable identity within P&P. Human-readable names are labels, not identities.

Intelligent devices use stable identity independent of transport or temporary communications addresses. Simple devices that cannot identify themselves may instead be represented by the stable System Controller capability/connection through which they are used.

Low-level addresses assigned during hardware initialisation are hardware/device-adapter details and must not become P&P identity. Identical I2C sensors, for example, may receive temporary per-boot bus addresses while their stable P&P meaning remains attached to controller detector ports/capabilities.

Assignments belong to configuration, not to a temporary address. A confirmed replacement intelligent device retains its own identity while configuration may transfer the previous assignment to it.

P&P restores and discovers what the hardware can actually establish. It must not pretend to detect an otherwise unobservable physical swap of indistinguishable simple devices.

<a id="system-architecture--134-system-controller-and-communication-topology"></a>
#### 13.4 System Controller and communication topology

The **System Controller (SC)** is the central P&P controller platform containing the authoritative Race Control and Race Engine responsibilities. Race Control is a distinct responsibility within the SC, not another name for the SC.

P&P uses the common communications bus as the normal path for inter-component communication. Mapped information and other messages are consumed by the responsibility that owns their meaning; they are not mechanically forwarded through Race Control or through the SC merely for routing.

A private point-to-point inter-component path is therefore not part of the normal architecture. If implementation later demonstrates a genuine technical need for one, it is an explicit exception and must preserve the same message meaning, authority and observable system state rather than becoming a hidden alternative architecture.

<a id="system-architecture--135-pp-communications-bus"></a>
#### 13.5 P&P communications bus

P&P components communicate through a common logical message backbone: the **P&P Message Bus**. This is an architectural and programming concept, not necessarily one physical electrical bus or one transport protocol.

The default architectural rule is:

> **If P&P components need to communicate with one another, they do so through the P&P Message Bus.**

Modules, devices and human interfaces attach to the common bus and exchange standard P&P messages without requiring direct knowledge of each other's implementation. Local software participants may attach in-process; remote participants reach the same logical bus through appropriate Transport Adapters.

The bus may carry Requests, Request Results, standard Input Events, Actions, Events/Facts, `NOTICEBOARD_CHANGED` notifications and other defined P&P messages.

<a id="system-architecture--1351-authority-and-message-policing"></a>
##### 13.5.1 Authority and message policing

Connection to the bus grants **connectivity, not authority**. The same rule
applies to association with the factory P&P Wi-Fi access point: reaching the
Browser or another transport does not create or upgrade the Master,
controlling-SMUG or Race Director lease.

Each standard P&P message contract defines:

- what the message means and the data it carries;
- which participant or responsibility is permitted to publish/originate it;
- which participant(s) or responsibilities are permitted or expected to consume it.

A component must not acquire authority merely because it can technically place a message on the bus. For example, an Input Device may originate a valid detector event but cannot authoritatively declare that a lap has been completed; that interpretation belongs to the Race Engine. A browser may originate an authorised START Request but cannot publish an authoritative GO or mutate competition state.

Consumers subscribe to or receive only the message types appropriate to their responsibilities. This replaces architectural policing by dedicated point-to-point pathways while preserving the same responsibility boundaries.

The bus itself does not decide whether a message is true, valid or operationally permitted. Validation and consequence remain with the authoritative responsibility defined for that message.

<a id="system-architecture--1352-one-message-one-or-many-consumers"></a>
##### 13.5.2 One message, one or many consumers

A message may have one intended consumer or several. A START Request is handled by Race Control. A competition detector event is handled by the Race Engine. An authoritative scheduled start publication may be consumed by the Race Engine, lights, browser presentation, Taster, audio and test equipment.

This allows a new consumer, such as a future scoreboard, to subscribe to existing standard P&P information without requiring Race Control or the Race Engine to be modified merely to know that the new consumer exists.

Race Control and the Race Engine use this same bus for their own defined communication. They do not require a special private link.

<a id="system-architecture--1353-state-notification-and-resynchronisation"></a>
##### 13.5.3 State notification and resynchronisation

The P&P Message Bus does not replace authoritative P&P State.

Authoritative State remains owned by the relevant P&P responsibilities. A `NOTICEBOARD_CHANGED` notification on the bus says that something material on the authoritative Noticeboard has changed. A browser or other current-state consumer then refreshes the current authoritative State it needs.

Rapid successive changes may therefore collapse naturally from the consumer's point of view. A browser may receive one or more such notifications and simply refresh to what is true now rather than reconstructing every intermediate State. Events/Facts that matter individually remain separate bus messages where their individual occurrence is relevant.

<a id="system-architecture--1354-transport-adapters"></a>
##### 13.5.4 Transport Adapters

The P&P Message Bus is a logical topology. It need not be one physical network and does not require every message to be physically broadcast to every participant.

A **Transport Adapter** connects a particular communications mechanism to the logical bus while preserving standard P&P message meaning and authority. Examples may include local in-process delivery, wired remote communication, ESP-NOW and Wi-Fi/WebSocket. The exact transports and software mechanisms remain implementation decisions.

A Transport Adapter may filter/deliver only messages relevant to participants reachable through it. A slow or disconnected remote participant must not block timing-critical local processing.

The logical topology may therefore be pictured similarly to a shared Ethernet backbone: Input Devices, SC responsibilities, Memory, human interfaces, physical outputs, test tooling and future devices are participants attached to a common communications system. Direction and authority are properties of individual messages and responsibilities, not of the bus itself.

Human-interface participants such as the Browser and Taster are two-way: they consume P&P information for presentation and may originate authorised Requests. One-way output participants such as start lights may simply consume the message types they require.

<a id="message-contract--pp-message-contract"></a>
### P&P Message Contract

**Contract:** Generic communication rules and agreed Message Types for the common **P&P Message Bus**. This document defines message semantics and authority, not packet encoding, C++ structures or transport protocols.

This document is subordinate to the Design Constitution, System Requirements and System Architecture.

<a id="message-contract--1-definition"></a>
#### 1. Definition

A **P&P Message** is communication sent by one authorised P&P component over the common P&P communications bus for consumption by one or more permitted P&P components.

The bus provides communication. It does not grant authority.

For every defined Message Type, its contract must state:

- what the message means;
- the payload required by that meaning;
- which component(s) may publish it;
- which component(s) may consume it;
- any timing or delivery semantics required by that Message Type.

<a id="message-contract--2-common-message-envelope"></a>
#### 2. Common message envelope

The generic conceptual envelope is:

- **Message Type** — identifies the defined message contract.
- **Source** — the originating P&P component instance.
- **Relevant Time** — where the message meaning requires time, the P&P System Time at which the event happened or the scheduled action is to become authoritative.
- **Payload** — only the information required by that Message Type.
- **Correlation ID** — used by Requests and their Request Results; may be used elsewhere only where a defined contract requires it.
- **Message/Event ID** — optional; used where a Message Type requires duplicate detection or another explicit identity requirement.

The exact binary/text representation is deliberately not specified.

A message must not duplicate authoritative State or configuration merely for convenience.

<a id="message-contract--3-message-classes"></a>
#### 3. Message classes

The current semantic classes are:

1. **Request** — asks the owning P&P responsibility to do something.
2. **Request Result** — reports that a Request was accepted or rejected, including a useful reason where rejected.
3. **Input Event** — reports a clean physical/system input trigger.
4. **Fact/Event** — reports something authoritative P&P has determined happened.
5. **Action** — requires an authorised P&P component/device to perform a defined action.
6. **Noticeboard-change notification** — indicates that something material on the authoritative Noticeboard has changed.

A Noticeboard-change notification is a notification to look again at current truth; it is not a replacement copy of that truth.

<a id="message-contract--4-publication-subscription-and-authority"></a>
#### 4. Publication, subscription and authority

Messages are normally **published rather than individually addressed**.

A component subscribes to the Message Types it needs. Subscriptions determine ordinary delivery; the Message Contract determines permission and authority.

Connection to the bus or subscription to a Message Type does not authorise a component to originate that message.

The bus must not require publishers to know every consumer. Adding a new permitted consumer should not require an existing publisher to send a special additional copy.

<a id="message-contract--5-source-identity"></a>
#### 5. Source identity

Source identifies the P&P component instance that originated the message sufficiently to establish provenance and apply the Message Contract.

A Transport Adapter preserves the originating Source; it does not replace the Source merely because it carried the message.

Transport changes how a message travels, not what it means or who originated it.

<a id="message-contract--6-time-semantics"></a>
#### 6. Time semantics

Where a Message Type has race-relevant time, **Relevant Time means the time relevant to the message's meaning**, not the time at which the message happened to arrive at a consumer.

Examples:

- an Input Event carries the time the clean physical trigger occurred;
- a scheduled GO carries the future P&P System Time at which GO becomes authoritative.

Transmission, receive and diagnostic timestamps may exist as implementation metadata but must not redefine authoritative race timing.

<a id="message-contract--7-input-events-and-device-meaning"></a>
#### 7. Input events and device meaning

Input Devices are components of the Input Module. Their source-specific work is internal to that module and is not a separate P&P Message Bus exchange merely because a particular physical implementation is remote.

For the first detector-style input contract, the Input Device owns detection, filtering, debounce/hysteresis, clearing and re-arming. Once it has recognised a clean trigger, the Input Module publishes an Input Event identifying:

- the stable input/device capability that produced the trigger;
- the Relevant Time at which that trigger occurred.

The bus does not require a subsequent INACTIVE/clear event merely to tell the Race Engine that the detector has re-armed. Re-arming remains an Input Device responsibility. A future input type may define different semantics if it genuinely requires them.

The Input Module does **not** publish lane, Start/Finish, sector, drag, speed-trap or MUG meaning.

The active Session Definition freezes the role assigned to each input capability required by that session. The Race Engine interprets the physical Input Event against that fixed Session Definition.

Therefore the same Input Module message can acquire different competition meaning in different sessions without changing the Input Module.

<a id="message-contract--8-output-actions-and-device-meaning"></a>
#### 8. Output actions and device meaning

Output Devices are owned by the Output Module/device boundary and remain ignorant of race mode, lane and MUG meaning.

For an active session, the relevant race/session responsibility uses the fixed Session Definition to resolve a semantic requirement to the stable output capability assigned that role. The resulting device-level logical Action is published on the bus.

The Output Module/Output Device carries out that action using its hardware-specific implementation. It does not need to know why the output is being operated.

The first exact OUTPUT_ACTION payload is deliberately deferred until the output prototype stage requires it.

<a id="message-contract--9-browser-clients-and-human-interfaces"></a>
#### 9. Browser clients and Human Interfaces

The Browser interface is the P&P bus participant. Individual connected browsers are clients behind that interface rather than independent full P&P Message Bus participants.

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

The integrated Taster is also a controller-local trusted control source. It is
not an HTTP Browser identity and cannot claim Race Director authority by
putting a role in a Request. The controller's existing control-lease boundary
determines whether the local Taster is controlling or subordinate:

- when no SMUGLAND controller is connected, the Taster may issue its supported
  setup and session Requests;
- when SMUGLAND connects and becomes the controlling P&P client, the controller
  automatically supersedes Taster; Taster Requests are rejected or disabled
  and the Taster consumes the authoritative Noticeboard as a subordinate
  display;
- a spectator connection never acquires control or causes this handover;
- when the controlling SMUGLAND connection is lost, the controller may restore
  Taster control through the same lease boundary.

This handover changes only which presentation client may originate an
authorised Request. Race Control remains the sole lifecycle authority, and the
active Session Definition and its record policy remain frozen for the life of
the session.

<a id="message-contract--10-the-noticeboard-and-change-notification"></a>
#### 10. The Noticeboard and change notification

The **Noticeboard** is P&P's authoritative current externally presentable view of itself. It contains everything relevant and current needed to understand P&P or an active session, plus any current information that P&P makes available to a Browser, Taster or other presentation interface. It is information/state, not another operational module and not another owner of the underlying information.

Examples include current session lifecycle state, relevant Session Definition information, current lap counts, positions and the current fastest lap. Outside an active race it can also expose current Race Setup, available modes/options, current capability/availability information, saved identities offered for selection and user-facing status/fault information where a presentation interface needs them. If a fastest lap was established several events ago and is still the fastest lap, that remains the current authoritative fastest-lap state on the Noticeboard.

The Noticeboard is not History and does not contain every past event merely because it happened. Nor does it expose private hardware or implementation detail merely because that detail exists. The practical test is: **if a Browser or Taster needs to know something about P&P now, it obtains that current externally presentable information through the Noticeboard rather than rummaging inside owning modules.**

The standard notification that something material on the Noticeboard has changed is **`NOTICEBOARD_CHANGED`**.

NOTICEBOARD_CHANGED does not carry a replacement copy of the Noticeboard. Interested consumers obtain the current authoritative information they need.

The architecture does **not** require the Noticeboard itself to expose a version number. An implementation may use revision counters, dirty flags, sequence numbers or another internal mechanism if useful, but that mechanism is not part of the conceptual P&P contract unless a later concrete requirement makes it necessary.

A `NOTICEBOARD_CHANGED` notification is not generated merely because a derived displayed value changes with the passage of time. For example, if authoritative State records the GO/start time and duration, a Browser or Taster can display a counting clock from P&P System Time without P&P changing the Noticeboard every second.

Facts/Events and Noticeboard State remain distinct:

- a Fact/Event says that something happened;
- the Noticeboard says what is true now as a result.

A recovering or newly connected consumer can obtain what is true now from the Noticeboard without replaying every intermediate state change. Where a consumer must react to a particular Fact/Event as it happens, that event uses its own Message Type and delivery semantics.

<a id="message-contract--11-delivery-semantics"></a>
#### 11. Delivery semantics

Different Message Types may require different delivery behaviour. The Message Contract for each type must state what is required rather than relying on one universal reliability rule.

<a id="message-contract--111-local-pause-settlement"></a>
##### 11.1 Local Pause settlement

An accepted local PAUSE may establish a standard Message Bus delivery fence. Race Control publishes the pause operation to the Race Engine and Input Module; the Input Module publishes one settlement marker after all clean inputs it accepted locally before the pause boundary; and the Race Engine acknowledges settlement only after interpreting those preceding inputs. The fence uses normal component endpoints and delivery semantics: it does not create a private Input-to-Race-Engine route, give the Input Module race-rule meaning, or define a distributed-input watermark, clock protocol, or remote-device settlement mechanism.

Current categories include:

- **race-critical event** — must not silently disappear where loss would change authoritative competition behaviour;
- **Noticeboard-change notification** — may be combined or superseded because the consumer can look again at current authoritative State;
- **ephemeral presentation** — may be best-effort where loss cannot affect authoritative operation.

Where duplicate delivery could change authoritative state, the Message Type must provide sufficient identity/handling to prevent an accidental duplicate from being interpreted twice.

Transport Adapters must satisfy the required semantics without changing message meaning or authority.

<a id="message-contract--12-priority-and-scheduling"></a>
#### 12. Priority and scheduling

There is no sender-selected universal Priority field in the generic envelope.

Timing-critical requirements belong to the Message Contract and bus implementation. Browser, storage, audio or other slow/non-critical work must not block timing-critical race processing.

<a id="message-contract--13-first-concrete-message-types"></a>
#### 13. First concrete Message Types

These are semantic contracts. Names are fixed for the first implementation; exact packet/structure encoding remains deferred.

<a id="message-contract--131-input_event"></a>
##### 13.1 INPUT_EVENT

**Class:** Input Event  
**Publisher:** Input Module  
**Permitted consumers:** Race Engine; authorised Test/Diagnostics where required.

**Payload:**
- stable Input ID/capability identity.

**Relevant Time:** the P&P System Time at which the clean trigger occurred.

**Meaning:** "This input was triggered at this time."

For the first detector-style input, there is no ACTIVE/INACTIVE field. Clearing and re-arming are internal Input Device work.

The message carries no lane, MUG, Start/Finish, sector, drag, speed-trap or lap meaning. The Race Engine obtains the session meaning from the fixed Session Definition.

<a id="message-contract--132-start_request"></a>
##### 13.2 START_REQUEST

**Class:** Request  
**Publisher:** authorised Human Interface, including Browser interface or Taster.  
**Consumer:** Race Control.

**Payload:** no Race Setup copy is required.

**Correlation ID:** required.

**Meaning:** "Please start the race described by the current authoritative Race Setup."

Race Control checks permission, validates current Race Setup and required capabilities, creates the immutable Session Definition when the Request is accepted, and begins the start procedure. The requesting interface does not supply its own competing copy of Race Setup.

<a id="message-contract--133-request_result"></a>
##### 13.3 REQUEST_RESULT

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

<a id="message-contract--134-go_scheduled"></a>
##### 13.4 GO_SCHEDULED

**Class:** Fact/Event  
**Publisher:** Race Control.  
**Permitted consumers:** Race Engine; Browser interface; Taster; authorised output/presentation responsibilities and Test Harness where required.

**Payload:**
- authoritative future GO time in P&P System Time.

**Meaning:** "GO for the active session occurs at this exact System Time."

This is not "GO now". Consumers can prepare presentation/behaviour in advance against the same authoritative instant. It does not require a Correlation ID merely because START_REQUEST preceded it.

<a id="message-contract--135-lap_completed"></a>
##### 13.5 LAP_COMPLETED

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

<a id="message-contract--136-competition_complete"></a>
##### 13.6 COMPETITION_COMPLETE

**Class:** Fact/Event  
**Publisher:** Race Engine.  
**Consumer:** Race Control; other permitted interested consumers may subscribe where useful.

**Payload:** none required for v1.

**Relevant Time:** P&P System Time at which the Race Engine determined that the active competition's completion rules had been satisfied.

**Meaning:** "Under the rules of the active Session Definition, this competition is now complete."

Race Engine reports the fact. Race Control owns the resulting session-lifecycle and operational consequences. Winner/results/final positions remain authoritative competition State rather than being duplicated into this completion message.

<a id="message-contract--137-noticeboard_changed"></a>
##### 13.7 NOTICEBOARD_CHANGED

**Class:** Noticeboard-change notification  
**Publisher:** the P&P responsibility through which a material authoritative Noticeboard change is exposed. Exact internal aggregation/notification mechanics are an implementation decision and must not create a new owner of State.  
**Consumers:** Browser interface, Taster and other consumers that need to refresh current authoritative information.

**Payload:** none conceptually required.

**Meaning:** "Something material on the authoritative Noticeboard has changed; look again if you need current State."

This message does not require a Noticeboard version number and does not itself carry the changed State.

<a id="message-contract--14-session-definition-v1-information-required-by-these-contracts"></a>
#### 14. Session Definition v1 information required by these contracts

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

<a id="message-contract--15-implementation-boundary"></a>
#### 15. Implementation boundary

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


<a id="message-contract--stage-12-result-messages-frozen-design-implementation-not-yet-authorised"></a>
#### Stage 12 result messages (settled message contract)

Stage 12 proposes appending, without renumbering existing types: `FINISH_SETTLEMENT=18` (Race Engine requests an Input Module FIFO fence through the normal Message Bus) and `FINISH_SETTLED=19` (Input Module acknowledges delivery of locally accepted input covered by the fence). Race Engine remains the sole publisher of authoritative competition result data; no separate Browser result message is proposed. The proposed payloads carry candidate/settled Relevant Time F, finish behaviour and result identity; Race Engine remains the sole publisher of authoritative competition result data. Exact names, numeric values, payload fields, reliability and settlement ownership are constrained by the frozen Stage 12 design and acceptance plan; implementation remains unauthorised until separately approved.

<a id="system-architecture--132-event-ordering-duplication-and-availability"></a>
#### 13.2 Event ordering, duplication and availability

Where authoritative timestamps establish order, P&P uses those timestamps rather than communication arrival order. Where timestamps genuinely do not establish an order, arrival order must not manufacture one; the relevant race/mode rules determine how simultaneous events are treated.

A single source event must not affect authoritative state more than once. The mechanism is an implementation decision and should be placed at the lowest sensible boundary rather than forcing Race Engine to compensate for avoidable transport duplication.

There is no universal age at which a delayed event becomes invalid. Its timestamp is preserved and the responsibility owning its meaning decides whether it remains relevant. Where capability availability changes affect event interpretation, those changes are related to P&P System Time so that an event is interpreted against the state applicable at its event time rather than merely the state when it arrived.

Capability availability is authoritative at the boundary of the responsibility that owns or monitors that capability. That responsibility may use acknowledgements, heartbeat, timeout, self-test, connection state or another suitable mechanism internally. Race Control consumes the functional status needed for race decisions, not routine low-level communications or detailed diagnostics.

Communication status, capability status, event publication/delivery and authoritative processing are distinct. Successful communication is not proof of downstream physical operation or successful event processing unless the relevant boundary explicitly provides that information.

Loss of a capability invalidates any stale claim that its last reported physical state is still current. On recovery it establishes current state afresh and is not operationally available until required initialisation, compatibility and timing conditions are satisfied.


## Memory and storage

<a id="system-architecture--13-memory-module"></a>
#### 13. Memory Module

The **Memory Module** owns information that P&P deliberately retains across power cycles. Its internal storage technology may be flash, SD card, files, a database or another suitable implementation; other modules do not depend on that choice.

Persistent information may include installation/configuration data, remembered setup and preferences, MUG/car records where supported, race/session history, lap data, personal-best summaries and overall track-record summaries.

Memory has three distinct relationships with the rest of P&P:

1. **Persistent information** — Memory owns the persistent copy of installation configuration, preferences, remembered Race Setup, identities, history, records and similar information that must survive power loss.
2. **Working information** — at startup, and when an authorised persistent change is accepted, the applicable information is made available in working RAM to the modules/responsibilities that need it. Normal operation then uses that working data locally. A sensor crossing, output action or race calculation must not require a fresh persistent-storage lookup merely to obtain configuration already loaded for operation.
3. **Session preparation and recording** — the proposed Race Setup plus relevant working installation/capability information contribute to creation of a fixed **Session Definition in RAM** when START is accepted. The Session Definition freezes the roles of every input, output and other capability required by that session, so those meanings cannot change underneath the race. Authoritative race/session information worth retaining is written to Memory as appropriate during or after a session.

The Session Definition is therefore not a gateway to Memory and is not a module. It is frozen working data that isolates an active session from later changes to persistent configuration.

The P&P Message Bus is used when components communicate changes, Requests, facts or state to one another. A module reading configuration/state already present in its own working RAM is local data access, not a P&P Message Bus transaction.

Memory must not become a shadow Race Engine by reconstructing competition state from every live event. The Race Engine remains authoritative for competition interpretation and live competition state; Memory retains the authoritative results/history information provided for persistence.

The initial product should retain a deliberately modest useful result/history record rather than logging everything simply because storage is available.

History follows [the storage contract](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design) and [Browser History](#browser-flow-results-history-spec--results--history). Wall-clock date/time is not required for retained race history.

The storage model must be extensible so that substantially richer information can be retained later without redesigning Race Control or making older records unusable.

<a id="system-architecture--130-physical-storage-resource-ownership"></a>
##### 13.0 Physical storage resource ownership

The P&P Memory/storage responsibilities may use one physical SD card for
History/Records, active/detail persistence and observation-only Diagnostics,
but those logical consumers do not own the physical device independently.
Exactly one P&P physical-storage owner owns FSPI initialisation, the
`SD.begin()`/mount lifecycle, FAT/filesystem access and physical recovery.

The owner coordinates and serializes actual filesystem operations inside the
storage boundary. Logical consumers submit bounded work through their defined
interfaces and namespaces; this serialization must never put SD/FAT latency
into Race Engine timing, input processing or other timing-critical authority.
Storage failure, queueing and slow media therefore remain isolated from
authoritative racing and HTTP operation.

A successful physical mount is shared. Consumers do not create independent
SPI objects, call `SD.begin()` or opportunistically remount. Physical-device
health and logical consumer health are separate: History and Diagnostics may
report independent logical outcomes, but neither may claim durable SD-backed
operation while the shared physical device is unavailable. Diagnostics may use
its existing bounded RAM-only/degraded mode.

On reboot, or at an explicit owner-controlled recovery boundary in an eligible
non-active lifecycle state, the owner mounts once and logical consumers
reconstruct their own namespaces through it. History commits, active-detail
streams, diagnostic appends, rolling and export reads use bounded arbitration
so one consumer cannot monopolize the medium.

Physical recovery is prohibited while the authoritative session lifecycle is
`STARTING`, `RACING`, `PAUSED` or `RESTARTING`, and while completion settlement
or finishing has not yet reached `FINISHED`. It is eligible in `READY` and in
`FINISHED` after normal completion settlement. During a prohibited state,
racing continues with the existing failure-isolation rules and recovery is
deferred. Failed recovery leaves the physical and logical capabilities
unavailable/degraded without affecting racing or HTTP; logical consumers never
perform an independent remount.

<a id="system-architecture--131-information-ownership-and-delivery"></a>
#### 13.1 Information ownership and delivery

P&P may distribute information widely, but it must not distribute ownership. For each authoritative fact or state there is one owner and one truth. Other components may hold views, snapshots or derived presentation data without becoming another authority.

Architectural modules own the internal state required to perform their responsibility. Other modules consume defined outputs, facts, state or requests through the owning module's boundary; they do not depend on its internal workings. In particular, Race Control does not need to know the Race Engine's internal lap, position or calculation process. It consumes only defined Race Engine outputs that matter to session operation, such as competition completion. The architecture therefore does not require a universal central State module.

A component publishes and receives information only through its defined boundaries. Publication means information has been made available; it does not prove every consumer received it and does not by itself imply persistence.

Facts/events describe what happened. Current state describes what is true now. Persistent information describes what must remain available across the relevant lifecycle. A recovering consumer resynchronises from the authoritative current state it needs and then consumes new relevant facts/events; it does not require replay of every earlier transient event.

Where another component's resulting state matters, message delivery alone is not proof that the requested action occurred. The resulting state/status must be available through that component's boundary where the function requires it.

A physical output has one authoritative owner of commanded state. Other functions provide information or request action through boundaries; they do not independently drive it.

A component receives only the information required for its responsibility. The existence of complete authoritative P&P state does not justify a universal mutable state object shared across unrelated components.

<a id="persistent-storage-foundation-design--pp-persistent-storage-foundation-design"></a>
### Persistent storage design

**Status: ACCEPTED / FROZEN**

<a id="persistent-storage-foundation-design--purpose"></a>
#### Purpose

This foundation defines the persistent-storage boundary required by the accepted P&P race architecture. Production SD storage is authoritative for completed-race data; NVS remains for small essential controller state.

It does not change race timing, authority, session, Results, History, Practice, Endurance, PB or Track Record semantics.

<a id="persistent-storage-foundation-design--storage-responsibilities"></a>
#### Storage responsibilities

<a id="persistent-storage-foundation-design--nvs"></a>
##### NVS

NVS stores only small essential controller state, including configuration,
remembered setup, the ESP-IDF/Arduino Wi-Fi driver's persisted STA
configuration for the one remembered external network, retained Browser/Race
Director authority, storage-format or migration markers, and small indexes or
generations where useful. The driver-managed flash-backed STA configuration is
the sole persistent credential store for optional AP+STA reconnection; P&P does
not maintain a duplicate SSID/password record. Network credentials remain
outside History, Results, diagnostic storage and support exports.

NVS is not the production backend for completed-race History or detailed Results/Details. The current Wokwi NVS `NOT_ENOUGH_SPACE` evidence remains valid and is a development failure-boundary case.

<a id="persistent-storage-foundation-design--sd"></a>
##### SD

The production SD card is canonical for completed official race History, persisted Results and Details, PBs and Track Records, Record Era state, and future persistent MUG data.

The number of active entries (`PP_MAX_ENTRIES`) is independent of the number of persistent MUG identities, History capacity, and the number of records displayed by a Browser. History capacity is a storage policy independent of UI display count.

<a id="persistent-storage-foundation-design--physical-sd-ownership"></a>
##### Physical SD ownership

When one physical SD card serves more than one P&P storage function, exactly
one P&P physical-storage owner owns that device's FSPI initialisation,
`SD.begin()`/mount, FAT/filesystem lifecycle and recovery. History/Records,
active/detail persistence and Diagnostics are logical consumers. They use
subordinate storage interfaces and namespaces supplied by the physical owner;
they do not create their own `SPIClass`, initialise FSPI, call `SD.begin()` or
independently remount the card.

All actual filesystem access is coordinated and serialized inside the physical
storage boundary. Consumers submit bounded work through their defined storage
boundaries; filesystem latency, queueing or failure must not block
timing-critical Race Engine processing or alter authoritative racing. The
owner may schedule bounded recovery work, but a consumer may not retry an
impossible filesystem operation opportunistically or on every race tick.

There is one mount attempt for each physical-storage lifecycle or explicit
owner-controlled recovery attempt. A successful mount is shared by all logical
consumers. The owner reports physical availability separately from each
consumer's logical health. A logical consumer may be degraded or faulted while
another remains usable, but no consumer may report durable SD-backed operation
while the shared physical device is unavailable. Diagnostics may use its
existing bounded RAM-only/degraded path when durable SD is unavailable.

An SD failure does not stop racing or HTTP. History does not publish a result
as durable unless its existing commit contract succeeds; diagnostic failure
follows the bounded degraded/RAM-only rules. A failure in one namespace does
not imply failure in another unless the common physical layer is the cause.

On reboot, or at an explicit owner-controlled recovery boundary in an eligible
non-active lifecycle state, the physical owner establishes the device once and
then each logical consumer scans/reconstructs its own namespace through its
subordinate interface. Committed History, detail, diagnostic evidence and
their existing sequence/retention semantics are preserved.

History writes, active-detail streaming, diagnostic appends, diagnostic
rolling and diagnostic export reads share the owner's bounded arbitration. No
one consumer may monopolise filesystem work or make another consumer's
timing-critical authority path wait on SD latency.

Physical recovery is prohibited while the authoritative session lifecycle is
`STARTING`, `RACING`, `PAUSED` or `RESTARTING`, and while the completion
settlement/finishing boundary has not yet reached `FINISHED`. Racing continues
under the existing failure-isolation rules while recovery is deferred. Recovery
is eligible in `READY` and in `FINISHED` after the normal completion settlement
has reached that state. A failed recovery leaves physical and logical health
unavailable/degraded and does not affect racing or HTTP. No logical consumer
may independently retry or remount at any lifecycle state.

<a id="persistent-storage-foundation-design--authority-and-storage-boundary"></a>
#### Authority and storage boundary

Race Engine remains authoritative for timing, laps, classification, integrity and completed results. It depends only on a persistence/storage boundary and not directly on SD, FATFS, NVS, filenames or filesystem layout.

The boundary supports append/load/enumerate committed History, clear History, PB/Track Record persistence, and Record Era persistence, with explicit status and fault reporting. Production, development and acceptance backends implement the same boundary.

<a id="persistent-storage-foundation-design--serialized-formats"></a>
#### Serialized formats

Persistent data uses explicit serialized formats; raw C++ structure dumps are prohibited. Each record contains a magic identifier, record type, format version, sequence or generation identity, payload length, fixed-width encoded fields, checksum, and a commit state or equivalent validity marker.

The format preserves the immutable result information required by the frozen Results, Details and History contracts: Session Definition and mode, entry identities and lane mappings, factual and classified laps, penalties, ranks, ties and laps behind, finish/winning/expiry/overtime data, best/last/fastest lap information, retained valid lap details, and persistent sequence identity.

Detailed retention remains bounded by the available production storage, not by an Endurance product lap ceiling. Authoritative counts and statistics never depend on retained-detail count. The current WP2C development representation uses a provisional bounded dynamic active-detail workspace per entry; its byte budget is an engineering test value, not a product lap limit, and is replaced by streamed per-race SD detail in the production representation.

The active-race implementation separates sporting truth from detail retention.
Race Engine records factual crossings and authoritative totals immediately;
an Active Lap-Detail Store accepts records into a bounded queue and drains them
in order to a temporary per-race SD stream on the production backend.
`CompletedRaceResult` is a summary plus detail association/status, not a fixed
lap-array product limit.
If the detail queue/stream or storage transaction cannot complete, the sealed
Result remains authoritative in RAM but the race is not published as committed
History. A summary-only or falsely-complete History entry is forbidden.

The Race Engine never waits for SD while accepting a lap. At completion the
temporary stream is drained, finalized and verified, then the summary and
detail are encoded and published through one linked transactional History
commit. Temporary streams are never enumerated as History.

Storage enforces [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy) at the Race Engine persistence gate, including exclusion from official streams, History and records. Abandoned races and Practice create no official History. A non-official sealed Result remains available in bounded live state without an official storage transaction.

<a id="persistent-storage-foundation-design--atomic-persistence"></a>
#### Atomic persistence

Completed results use a two-phase persistence model: serialize and checksum the payload; write it to a temporary or uncommitted location; flush/sync it; verify it; write the commit/index record; flush/sync that record; and expose the result only after a valid committed pair exists.

Recovery reconstructs only committed, checksum-valid records. Incomplete temporary data is ignored or reclaimed, and an uncommitted payload cannot appear in History. The same principle applies to PB/Track Record and Record Era updates.

<a id="persistent-storage-foundation-design--persistence-failures"></a>
#### Persistence failures

A persistence failure never invalidates an authoritative result held in RAM. For a failed History write, the result remains sealed and valid, `persistencePending` remains true, `persistenceFault` becomes true, one storage-fault event/report is emitted, no `HistoryStored` event is emitted, and the same impossible operation is not retried on every Race Engine tick. A retry may occur only through an explicit controlled storage-recovery boundary.

An active-detail storage interruption is distinguished from actual detail loss. While required detail remains in the bounded queue, storage may be recovered through the explicit owner-controlled recovery boundary once the lifecycle is eligible, and the normal drain/finalize/verify/transaction sequence may still publish the race. Physical recovery is not attempted during `STARTING`, `RACING`, `PAUSED`, `RESTARTING` or unfinished completion settlement. Once a required detail record is dropped or the stream is proven incomplete, `detailIncomplete` is permanent for that race; later recovery cannot make it eligible for History. Active temporary streams are disposable uncommitted debris after restart: they are never History, are never attached to another session, and may be deterministically reclaimed without deleting committed records.

PB/Track Record failures are explicit and latched; their write result is not silently ignored. A failed record write does not alter race validity or lap eligibility.

<a id="persistent-storage-foundation-design--sd-failure-cases"></a>
#### SD failure cases

The implementation defines observable behaviour for SD absence, startup unavailability, full storage, payload failure, commit/index failure, corrupt payload or index, unsupported format version, power loss at commit boundaries, and media removal or remount failure.

In every case authoritative in-RAM state is unchanged, invalid or uncommitted data is not presented as official History, faults are visible through persistence status and diagnostics, and valid older committed records remain available where possible.

Unsupported old development formats may be reset under the pre-production migration policy. Future production formats require explicit migration or a safe read-only incompatibility result.

<a id="persistent-storage-foundation-design--history-retention"></a>
#### History retention

History is a bounded rolling collection of the **most recent 100 completed official sessions**. It is not tied to `PP_MAX_ENTRIES` or to the number of records displayed by a Browser. When race 101 is durably committed, the oldest retained completed race is evicted only after the replacement commit is valid. Each retained race is intended to preserve its complete available lap-by-lap detail. The current four-race / 8,192-byte development `HistoryStore` and the provisional bounded active-detail workspace are implementation bounds only; they are not the production retention policy.

<a id="persistent-storage-foundation-design--development-and-acceptance-backends"></a>
#### Development and acceptance backends

Before final SD hardware is available, the same boundary is exercised with a file-backed development backend, failure-injection controls, an acceptance fake, and the existing bounded NVS backend where necessary to reproduce development limitations. The Wokwi `slot-10 NOT_ENOUGH_SPACE` result is retained as a deliberate development failure-boundary case, not a production capacity requirement.

The production backend remains an ESP32-S3 SD/FATFS implementation and requires a later real-hardware checkpoint. It must store per-race detail without requiring all retained History, or arbitrarily large lap arrays, to remain in RAM; detail loading/streaming and the rolling-100 index are part of that later implementation.

The established Phase 8 hardware record is the ESP32-8048S070 7-inch
800x480 ESP32-S3 board with 16 MB flash, on-board PSRAM and microSD. Its
provisional SPI microSD mapping for the diagnostic checkpoint is CS GPIO10,
MOSI GPIO11, SCK GPIO12 and MISO GPIO13. This identifies the test hardware
only; it does not define the eventual production SD filesystem or wiring
contract beyond these verified board signals.

<a id="persistent-storage-foundation-design--interim-containment"></a>
#### Interim containment

Before the SD backend is implemented, the current development History path must latch the first failed persistence operation, expose `persistencePending` and `persistenceFault`, publish/report the storage fault once, and stop retrying the same failed operation on every Race Engine tick. NVS preflight prediction is not a product requirement, and NVS is not the production History backend.

<a id="persistent-storage-foundation-design--wp2e-housekeeping-policy"></a>
#### WP2E housekeeping policy

The storage boundary exposes advisory backend capacity and health information
(`healthy`, `low`, `insufficient`, `unavailable`, `corrupt` or `unsupported`).
These values describe storage only; they never gate or replace Race Engine
authority. A backend may reject creation of a temporary payload when the
transaction cannot fit. The sealed in-RAM result remains authoritative and the
existing pending/fault boundary applies.

The production History policy remains a rolling 100 committed races. The
current SD implementation uses a fixed bounded set of payload slots. When a
replacement is committed, metadata is published first; only then may the
evicted slot be reclaimed. A failed replacement or failed reclamation cannot
invalidate previously committed metadata. This is a storage bound, not a
sporting or lap limit.

At mount/recovery, only narrowly identified uncommitted transaction temporary
objects are removed: slot payload `.tmp` files and active-detail `.tmp`/`.ok`
objects. Committed payloads and metadata are never removed by age. The fixed
slot set prevents superseded payload growth without requiring an unbounded
index.

Active-detail append and seal operations use the same advisory free-space
check before creating/appending temporary data. A failed check leaves the
authoritative race running and enters the existing pending/fault boundary; it
does not invent a lap or race limit.

The current format matrix is deliberately small:

| Data | Current | Readable legacy | Unsupported behaviour |
|---|---:|---:|---|
| Versioned History/result/detail payload | PSF container v3, result schema v3 | PSF v2/result v2 | reject checksum/schema/version; retain prior committed data |
| History metadata/index | slot metadata v1; legacy metadata reader retained | legacy metadata form | ignore invalid generation and use older valid copy |
| PB/Track Record/Record Era | record format v2 | no migration required | ignore invalid/unsupported record and keep defaults; do not reinterpret |
| NVS configuration | existing dual-copy configuration format | existing readable copy | report invalid/missing and use existing defaults path |

No migration is required for the currently supported legacy data: it is read
directly. No rewrite-on-boot migration is introduced. A future migration must
write and verify a new copy before retiring the old one.

Normal writes are event-driven: active detail is appended as accepted detail
records are drained; one History payload and one metadata generation are
published per completed race; PB/Track Record writes occur only when a record
actually improves or a clear changes it; unchanged values and the main loop do
not cause persistence writes. A failed operation is latched and is not retried
from every tick.

The provisional low-free-space threshold used by the current S3 development
backend is an engineering/test value only. Final capacities, FAT allocation,
wear policy and near-full-card behaviour remain Phase 8B hardware evidence.

<a id="persistent-storage-foundation-design--scope-exclusions"></a>
#### Scope exclusions

This foundation introduces no active-race persistence or recovery, new race
modes, a second persistence authority, new Practice or Endurance behaviour, or
product maximum based on implementation capacity. The WP8 session-source
record-policy boundary is defined by the Race Control/Session Definition
contract above; its implementation and physical presentation remain WP8 work.

<a id="system-architecture--77-authoritative-noticeboard"></a>
#### 7.7 Authoritative Noticeboard

The **Noticeboard** is P&P's authoritative current externally presentable view of itself. It contains everything relevant and current needed to understand P&P or an active session, plus any current information made available to a Browser, Taster or other presentation interface. It is information/state assembled from the responsibilities that own that truth; it is not another operational module and does not become another owner of the underlying information.

During a session this includes the relevant Session Definition information as well as live session/competition state. Outside a race it can include current Race Setup, available options, capability/availability information, selectable saved identities and user-facing status/fault information where an interface needs them. The Noticeboard is not History and does not expose private hardware/implementation detail merely because it exists. A Browser/Taster obtains current externally presentable P&P information through the Noticeboard rather than directly interrogating owning modules. **The Noticeboard is read-only as an architectural view: no consumer writes through it to alter Race Control, Race Engine, configuration, Registry or any other authoritative owner.** Changes originate with the responsibility that owns the truth; the Noticeboard merely exposes the resulting current externally presentable state.

A material change to the Noticeboard can cause a standard **`NOTICEBOARD_CHANGED`** notification. The notification does not carry a replacement copy of State; interested consumers look again at the current authoritative information they need.

For WP8 the Noticeboard must also make the current Taster/SMUG control role,
startup race-readiness, selected lane graphics, frozen racer identities and
the live per-lane completion/winner information available to presentation
clients where those values are relevant. These remain views assembled from
their owning authorities; the Taster and Browser do not publish them back as
state.

The Noticeboard does not conceptually require a public version number. Revision counters, dirty flags or sequence numbers may be used internally if useful, but are implementation mechanisms unless a later concrete requirement says otherwise.

The passage of time alone does not require repeated Noticeboard changes. For example, a display can derive a counting race clock from authoritative start/GO time, duration and P&P System Time without generating a `NOTICEBOARD_CHANGED` notification every second.

Facts/Events describe things that happened. The Noticeboard describes what is true now as a result. A current fact such as the fastest lap remains represented as current State until it is superseded or ceases to be relevant.

<a id="stage-12-design--stage-12-design--completed-results-and-post-race-flow"></a>
### Completed Results and post-race flow

**Contract:** Completed-result design (original Stage 12); current delivery and acceptance status are in the Master Project Plan.

The authoritative in-RAM completed-result boundary underpins Results, Details, Home and Race Again. History extends it through [persistent storage](#persistent-storage-foundation-design--pp-persistent-storage-foundation-design); an uncontrolled restart does not reconstruct an interrupted active race.

<a id="stage-12-design--ownership-and-lifetime"></a>
#### Ownership and lifetime

Race Engine owns a `CompletedRaceResult` summary created only after completion settlement. It contains the immutable Session Definition reference, W/F, finish behaviour, per-entry completed progress, completion times where measured, finishing order/dead heats, Best Lap and overall Fastest Lap, detail-completeness/association status and a sealed flag. Individual lap records belong to the active/detail storage boundary and are retrieved on demand for Details; they are not a fixed product-sized array in the summary. It remains available in RAM through Results and Browser reconnect until the next accepted START, fixture reset, or controller restart. Browser and Noticeboard expose it; neither calculates authoritative results.

START after Race Again creates a new Session Definition and fresh Race Entry IDs. Editing the proposed setup cannot mutate the sealed previous result.

<a id="stage-12-design--w-f-and-completion-settlement"></a>
#### W, F and completion settlement

Immediate Finish has W=F at the first target-distance crossing. Complete Current Lap and Complete Full Race Distance retain required post-W crossings; F is the Relevant Time of the last required crossing. A candidate F starts an explicit Message Bus settlement: `FINISH_SETTLEMENT(F)` reaches the local Input Module, which settles all locally accepted eligible input through F; `FINISH_SETTLED(F)` returns through the normal bus. Race Engine then orders and interprets the settled inventory, seals the result, and reports completion to Race Control. Input with Relevant Time after F cannot mutate the sealed result.

The exact finish-rule algorithms and settlement-pending rejection reason are frozen by the acceptance plan. No generalized distributed watermark or persistence protocol is introduced.

<a id="stage-12-design--browser-flow"></a>
#### Browser flow

FINISHED offers Results and read-only Details of genuine retained lap records. BACK returns to Results. Spectators may view both. Race Again requires Race Director authority and restores completed setup as proposed without starting. HOME is Browser-local navigation; it does not change FINISHED or factory-reset remembered setup.

<a id="stage-12-design--explicit-deferrals"></a>
#### Explicit deferrals

Historical Stage 12 deferrals are not current tasks. Later accepted History, records, pause/abandon and modes supersede the slice limits. Outstanding work and post-v1 candidates are in the [Master Project Plan](MASTER_PROJECT_PLAN.md).

<a id="stage-12-design--decisions-1421-incorporated"></a>
#### Decisions 14�21 incorporated

**W/F:** Immediate Finish uses W=F. Complete Current Lap sets W at the winner crossing and requires exactly one valid post-W crossing from every other active competitor; F is the last such crossing. Complete Full Race Distance keeps required competitors active to target distance; F is the last target crossing.

Complete Current Lap ranking is completed progress first, then finishing Relevant Time, with equality only when both are equal. Immediate Finish non-winners use completed progress only; no physical position or finish time is inferred. Results use explicit normal competition ranks (`1, =2, =2, 4`) and joint first where applicable.

**HOME and Race Again:** HOME is Browser-local navigation and never changes FINISHED or requires Race Director authority. Race Again is Race Director-only and seeds a new editable Proposed Race Setup from the completed Session Definition, then validates current capabilities when START creates a fresh Session Definition and fresh entry IDs.

**CompletedRaceResult:** Race Engine owns one sealed in-RAM summary. Results is served from the summary; Details retrieves the linked detail records through the active/detail storage boundary or committed History representation. Facts remain events and are never used as a history database. The summary ends at the next accepted START, full fixture reset or controller restart.

**Settlement:** Proposed appended types are `FINISH_SETTLEMENT=18` and `FINISH_SETTLED=19`. Race Engine owns candidate F and finish-rule interpretation; Input Module only fences its locally accepted FIFO. Race Engine seals only after FINISH_SETTLED, and later Relevant Time values cannot mutate the result. Settlement flags reset for every new race.


<a id="stage-12-design--decision-22--active-competitor-at-w"></a>
#### Decision 22 � active competitor at W

An active competitor at W is a race entry in the current Session Definition which has not ceased participation under an existing authoritative race rule before W. Stage 12 introduces no retirement, withdrawal, DNF, disqualification, pit/inactive state or Browser-dependent participation. In this implementation every valid entry remains active except an entry already finished as part of the winning group. Browser presence never affects activity. Complete Current Lap therefore requires exactly one further valid crossing from every other active entry; Complete Full Race Distance continues every other active entry to target distance. If all active entries satisfy the winning condition at the same Relevant Time they may be joint winners and no artificial crossing is added.

Completed-result Noticeboard and Browser field names/serialization are implementation details, constrained by the semantic requirements and existing boundaries. They are not remaining product-design decisions and must not turn State into persistent History.


## Presentation

<a id="common-display-spec--plonk--play--common-display-specification"></a>
### Shared display behaviour

**Status:** Committed product-design specification  
**Applies to:** Lap, Endurance and Timed Stage on the Browser and integrated
P&P displays.
**Exception:** Drag uses its own display design. The integrated Taster is a
restricted client of this same display/race authority; it may omit modes and
controls, but it does not define separate racing or timing behaviour.

<a id="common-display-spec--1-design-principle"></a>
#### 1. Design principle

P&P should scale from a solo MUG using one browser to a full competition using a main display, individual MUG screens and a separate Race Director screen.

The simple/default experience must remain non-threatening. Advanced functionality may be available without cluttering normal operation.

<a id="common-display-spec--2-full-browser-race-display"></a>
#### 2. Full Browser Race Display

The main display is intended for a monitor, TV, projector, tablet or full-screen browser.

The race display remains fundamentally one screen with changing states rather than changing pages during a race:

`READY → STARTING → RACING → FINISHING → FINISHED / RESULTS`

The start sequence does not replace the scoreboard with a separate start screen.

<a id="common-display-spec--21-shared-client-authority-and-timing"></a>
##### 2.1 Shared client authority and timing

The Browser and Taster are presentation/control clients of the same Race
Control, Race Engine and P&P System Time authority. See [Integrated Display standalone controls](#pp-taster-spec--standalone-controls) for its restricted local mode choices; the same authoritative race-mode contract applies.

Both clients consume the same authoritative state and presentation
events/facts. Neither client invents the GO instant, elapsed/remaining clock,
Endurance expiry, finish classification, finish time or overtime. Paused time
and an approved restart countdown do not add racing time.

Mode-specific finish policy and the shared timing/format definitions are
defined only in [Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) §2, §4 and §4.6. The display
consumes those values; it does not restate or reinterpret the policies.

Presentation updates are event/state driven. A client may advance or render a
displayed clock from authoritative P&P timing and may animate a genuine active
event, but a redraw timer or local calculation must not establish race timing
authority or rebuild unrelated layout regions. Numeric fields and lane rows
retain stable fixed regions while values change.

<a id="common-display-spec--3-race-control-pod"></a>
#### 3. Race Control Pod

A permanent segmented area sits across the top of the full display, envisaged as a shallow/squashed half-hexagonal pod.

It contains five light positions, which remain visible but greyed when inactive.

During a start these become the race start lights. The system can use either three or five according to the selected start configuration.

The pod has five physical light positions; there is no separate sixth green lamp. The authoritative countdown uses either three or five red lights according to the configured start. The authoritative GO style is either **Lights Out** or **Green**, as selected in the Session Definition. With Lights Out, the configured red lights extinguish at GO and all five physical positions are grey/off and dormant during ordinary racing. With Green, all five physical positions turn green simultaneously at authoritative GO for exactly one second, then all five become grey/off and dormant during ordinary racing. A configured three-light start changes only the number of red countdown positions; Green still illuminates all five physical positions. Browser and physical presentation consume this authoritative state and do not create a sixth lamp or independently establish GO authority; they may render the authoritative countdown and GO state.

During normal racing the lights are dormant but can be repurposed for race-control signals:

- **Flashing Yellow lights** — Track Call / Yellow active.
- **Flashing Red lights** — Race stopped / Red active.

The surrounding pod area is an independent display channel. For example, if a MUG requests a Yellow rather than calling one directly, the surround can pulse yellow while the five lights remain in their normal state.

Similarly, a false start can illuminate the surround without interfering with a start-light sequence already in progress.

Start/race-control lighting always takes priority over ordinary informational messages.

The pod may also display temporary messages such as:

- `FASTEST LAP`
- `FINAL LAP`
- `PENALTY`
- `FINISHED`

Exact visual treatments remain for UI prototyping.

<a id="common-display-spec--4-main-scoreboard"></a>
#### 4. Main Scoreboard

The main body consists of one row for each active lane/MUG.

Live race rows remain in fixed lane order:

- Lane 1
- Lane 2
- Lane 3
- Lane 4
- and so on if expansion hardware supports additional lanes.

Rows do not move when race positions change. Instead, the MUG's current `P1`, `P2`, etc. changes prominently within the permanent lane row.

This lets a MUG glance repeatedly at the same physical part of the display.

A typical Lap row contains:

**Position | Lane/MUG | Race Progress | Last Lap | Best Lap | Gap**

Race progress for a 25-lap race might display `17 / 25`.

A thin graphical progress indicator may also show race completion visually. This should be prototyped rather than considered mandatory at this stage.

Endurance and Timed Stage use the same visual architecture with mode-appropriate information. Mode-specific field meaning and finish semantics are defined in [Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification); this specification governs their shared visual meaning and geometry.

<a id="common-display-spec--5-temporary-row-states"></a>
#### 5. Temporary Row States

The geometry of a lane row remains stable, but information can temporarily change emphasis.

Examples include:

- **Position change** — new position briefly emphasised.
- **Fastest lap** — relevant timing briefly highlighted.
- **Penalty** — warning treatment followed by a persistent penalty indicator where appropriate.
- **Final lap** — progress area receives a distinctive treatment.
- **Finished** — row changes into its finished state.

A finished MUG remains in its original lane row while other MUGs continue racing.

Example:

`LANE 2 | CONNAL | FINISHED | P1 | 25/25 | 03:42:18 | BEST 8.19`

This is necessary because some Lap finish rules allow other MUGs to continue after the winner has finished.

<a id="common-display-spec--6-track-length-scale-and-speed"></a>
#### 6. Track Length, Scale and Speed

Track Setup can optionally store:

- **Track length** — metric or imperial.
- **Track scale** — common preset scales plus Custom.

These settings are optional and stored with the track rather than entered for every race.

Where available, P&P can calculate:

- actual model average speed;
- scale-equivalent average speed.

Speed can be calculated for individual laps, stages and complete races where meaningful.

It need not clutter the default live scoreboard. It can instead appear in results, detailed timing and selected temporary announcements such as:

`FASTEST LAP — 8.19 — 137 SCALE MPH`

<a id="common-display-spec--7-finish-behaviour-and-transition-to-results"></a>
#### 7. Finish Behaviour and Transition to Results

See [Competitive Finish Display](#implementation-spec--competitive-finish-display) for Browser-only timed Finish Display, default five seconds (zero supported), authorised skip and reconstruction. The Integrated Display keeps its fixed finished-state behaviour until RACE AGAIN. Winner/chequered treatment is presentation, not a new timing authority.

<a id="common-display-spec--8-results-screen"></a>
#### 8. Results Screen

Unlike the live scoreboard, Results are displayed in finishing order, not lane order.

The default Results screen should answer **who won and what happened?** immediately rather than overwhelming the MUG.

Typical information:

**Position | Lane/MUG | Progress | Time | Best Lap | Gap**

For Lap Race Results, progress and finishing information are separate fields.
Progress shows authoritative classified race distance against the configured
target where applicable. Time shows an entry's authoritative race-relative elapsed duration at its
permitted final crossing, or at its last completed lap when it was stopped
before a final crossing. An entry with no completed lap shows an em dash. Gap shows only information
supplied by P&P, such as authoritative laps behind. The Browser does not
replace one field with another or calculate classification, finish times or
time gaps.

Secondary information can include fastest lap, penalties and speed information where track data is available.

A valid finished Results screen presents the status context `RESULTS`, even though the finished session is not startable. This is presentation context and does not change authoritative startability. After **RACE AGAIN** returns P&P to READY, normal READY/startability presentation resumes.

A **Details / Analysis** view provides individual lap times, sectors, speeds, penalties and other recorded timing data.

Graphs may eventually live here but are not part of the default Results display.

<a id="common-display-spec--9-post-race-navigation"></a>
#### 9. Post-Race Navigation

The primary action is contextual:

- **RACE AGAIN** — standalone race; same setup again.
- **CONTINUE** — competition/event; advance to next scheduled race/heat/round.
- **NEXT STAGE** — Timed Stage multi-stage event.
- **EVENT RESULTS** — event has reached its conclusion.

Secondary actions such as **NEW RACE** and **HOME** are available where appropriate.

<a id="common-display-spec--10-personal-mug-display"></a>
#### 10. Personal MUG Display

Any connected browser can be assigned to a particular lane.

The browser should remember an assignment such as **I'M LANE 3** rather than requiring selection before every race. During organised competition, P&P may assign the lane automatically.

The personal screen prioritises information that can be read at a glance while driving.

**Position** and **race progress** dominate.

Secondary information includes:

**Last Lap | Best Lap | Gap**

It also carries a reduced version of the same Race Control Pod so the MUG sees the same start, Yellow and Red signals as the main display.

Personal alerts can temporarily dominate the screen:

- `FASTEST LAP`
- `FINAL LAP`
- `PENALTY +1 LAP`
- `YELLOW REQUESTED`
- `FINISHED — P2`

Browser audio can likewise provide MUG-specific alerts. Personalised spoken
announcements using MUG names are deferred until after v1.

<a id="common-display-spec--11-race-director-and-solo-operation"></a>
#### 11. Race Director and Solo Operation

P&P does not assume a dedicated Race Director.

It must support:

1. Dedicated Race Director.
2. Race Director who is also a racing MUG.
3. Solo MUG.

A dedicated RD can use a separate phone/tablet/PC with denser timing information and race-control functions.

The main full-screen display can also expose controls where appropriate, although **Display Only** should be the normal TV/projector presentation.

For a solo MUG or racing RD, pre-race controls such as **START** can appear prominently and then disappear once the race begins. Normal race operation should then be automatic.

Core rule:

> **Every normal race/session must be capable of being run by one MUG without requiring a separate Race Director.**

<a id="common-display-spec--12-multiple-simultaneous-displays"></a>
#### 12. Multiple Simultaneous Displays

Displays are clients, not timing authorities.

A four-MUG event could therefore have:

- 1 × Main Scoreboard
- 4 × Personal MUG displays
- 1 × Race Director display

or any smaller combination.

The System Controller remains responsible for timing, race logic and authoritative race state. Browser and Taster perform their own rendering, animations and audio as presentation responses; neither becomes a timing authority.

The [Product Facilities audio specification](#product-facilities-spec--3-audio-and-removable-storage)
governs destination fallback, priorities and playback reliability. Audio
must never delay racing. Alternating yellow PAUSED lights are silent.

Loss of a browser connection must never affect race timing or race state.

<a id="common-display-spec--13-yellow-and-red--display-principles"></a>
#### 13. Yellow and Red — Display Principles

Detailed RMS logic is deliberately deferred until software design.

The principles already agreed are:

Yellow / Track Call is a session option with three settings:

- **Off** — Yellow / Track Call is disabled for that session.
- **Request** — a MUG may request Yellow; the race continues while the requesting MUG and RD are notified and the RD accepts or dismisses the request.
- **Direct** — a permitted MUG Yellow input immediately calls Yellow / Track Call.

**Yellow Off does not disable the Race Director's basic PAUSE control.** PAUSE remains a core race-control intervention independently of whether the session uses a Yellow / Track Call procedure.

In Request mode, the Race Control Pod surround can pulse yellow while the five lights remain in their normal state.

**Yellow active:** five lights flash yellow. This represents the normal temporary track-call/interruption state and normally leads to **RESUME**.

**Red active:** five lights flash red. This represents a more serious Race Stopped state.

From Red, the Race Director may ultimately:

- **RESUME RACE** — preserve existing progress and continue.
- **RESTART RACE** — discard progress and begin the race again from zero, with confirmation.
- **ABANDON RACE** — terminate the race.

The precise timing/power behaviour and every possible transition are RMS implementation details to be specified later rather than allowed to derail the display design now.

---

**Product principle:** Easy to start. Deeply configurable when you want it.


<a id="common-display-spec--browser-ui-rules"></a>
#### Browser UI rules

<a id="common-display-spec--numeric-stability"></a>
##### Numeric stability
Numeric fields and live numeric display regions must have fixed dimensions appropriate to their expected values. Changing digits must not resize controls or move adjacent UI.

Use tabular numerals where appropriate for clocks, lap times, gaps, counters and similar changing race data.

General rule:

> **No UI jitter caused by changing race data.**

<a id="common-display-spec--numeric-adjustment"></a>
##### Numeric adjustment
Where a numeric value is commonly adjusted, use decrement / editable value / increment controls. Direct editing must remain available so large changes do not require repeated button presses.

<a id="common-display-spec--contextual-help"></a>
##### Contextual help
Use a consistent information control for terminology or consequences that may not be obvious.

- Desktop: hover may reveal the help.
- Touch: tap reveals the same help.
- Keep help concise, normally one or two sentences.
- Do not add help icons to self-evident controls merely for consistency.

Typical candidates include Drag Course Length, Timed Stage, Loop, Point-to-Point, finish behaviour, scale speed and false-start behaviour.

If a control requires a paragraph before a MUG can choose it, reconsider the control design rather than relying on a tooltip.

---

<a id="presentation-design-spec--plonk--play--presentation-design-specification"></a>
### Presentation and visual language

**Status:** Accepted visual-direction specification  
**Applies to:** Browser presentation and integrated P&P display  
**Authority:** Product behaviour remains governed by the relevant product specifications. This document governs visual language and presentation consistency, not race logic. Browser and Taster share the same presentation meanings even where their layouts and controls differ.

<a id="presentation-design-spec--1-purpose"></a>
#### 1. Purpose

P&P must look like one product everywhere.

The integrated display, browser Race Control, Personal MUG display, spectator display, Results and configuration screens must share a recognisable visual language. Different devices may arrange and prioritise information differently, but they must not look like unrelated applications.

The presentation should feel like a professionally designed motorsport timing product rather than a generic web dashboard, hobby project or computer game.

Core principle:

> **One P&P look. Different screens. The information that matters now dominates.**

<a id="presentation-design-spec--11-cross-client-product-boundary"></a>
##### 1.1 Cross-client product boundary

See [shared client authority and timing](#common-display-spec--21-shared-client-authority-and-timing) and [Integrated Display standalone controls](#pp-taster-spec--standalone-controls). Visual design may vary with the device, never the authoritative race semantics.

<a id="presentation-design-spec--2-agreed-visual-direction"></a>
#### 2. Agreed visual direction

The accepted visual direction is:

- dark navy/charcoal primary surfaces;
- restrained P&P yellow as the principal brand/accent colour;
- clear white/off-white primary information;
- lane colours used where they communicate lane/competitor identity, not as general decoration;
- large, highly legible timing and race-state typography;
- clean panels with restrained rounding and borders;
- generous spacing and strong visual hierarchy;
- simple, consistent iconography;
- restrained motorsport cues rather than decorative racing clichés;
- presentation theatre used for genuine race events, not continuously.

The visual direction is intentionally **not**:

- carbon-fibre wallpaper;
- fake brushed metal;
- permanent chequered-flag decoration;
- seven-segment typography everywhere;
- neon/glowing controls;
- gratuitous animated cars;
- a dense telemetry dashboard;
- a collection of differently styled pages.

P&P should remain recognisably P&P even when all decorative imagery is removed.

<a id="presentation-design-spec--3-relationship-to-the-physical-product"></a>
#### 3. Relationship to the physical product

The browser and integrated P&P display should visually belong to the same physical product.

Where practical they should share:

- P&P mark/wordmark treatment;
- colour palette;
- typography hierarchy;
- panel geometry;
- timing-number treatment;
- status colours and meanings;
- Race Control Pod language;
- icons;
- event treatments such as Fastest Lap, Final Lap and Finish.

They do not need identical layouts. The integrated display, phone, tablet, laptop and TV have different jobs and available space.

The integrated display may be self-contained when no controlling SMUG is
connected, but its controls still submit to the same Race Control authority as
the Browser. A connected controlling SMUG may take precedence under the
existing lease/permission contract; this changes who may request an operation,
not what the operation means.

<a id="presentation-design-spec--4-information-hierarchy"></a>
#### 4. Information hierarchy

P&P must show **what matters now** rather than everything it knows.

During a race the primary hierarchy is generally:

1. race-control state and safety/control signals;
2. position and race progress;
3. immediately useful timing information;
4. secondary analysis/detail.

Secondary information must not compete visually with the current race state.

Detailed analysis belongs in Results/Details rather than being forced onto the live race display.

<a id="presentation-design-spec--5-shared-presentation-components"></a>
#### 5. Shared presentation components

Browser and integrated-display implementations should be assembled from a common conceptual component vocabulary wherever applicable.

Initial component set:

- **P&P Header / Identity Mark**
- **Race Control Pod**
- **Status Banner**
- **Mode Card**
- **Lane Row / Lane Card**
- **Position Indicator**
- **Race Progress**
- **Timing Value**
- **Gap Value**
- **Fastest Lap treatment**
- **Final Lap treatment**
- **Penalty treatment**
- **Finished treatment**
- **Result Row**
- **Primary Action**
- **Secondary Action**
- **Information / Help control**
- **Numeric adjustment control**

A component should retain the same meaning and basic visual identity wherever it appears, while being allowed to scale or simplify for the available display.

This component vocabulary is also the intended foundation for any future configurable Screen Designer. A future SMUG should rearrange supported P&P components rather than hand-author arbitrary HTML/CSS.

<a id="presentation-design-spec--6-colour-rules"></a>
#### 6. Colour rules

<a id="presentation-design-spec--61-brand-colour"></a>
##### 6.1 Brand colour

P&P yellow is the principal accent and selection colour. It should be distinctive but restrained; covering large portions of every screen in yellow would weaken it.

<a id="presentation-design-spec--62-lane-colours"></a>
##### 6.2 Lane colours

Lane colours communicate lane identity and related race information. They should not become a general page theme.

A lane colour may be used for items such as:

- lane number/identity marker;
- a narrow row accent;
- a relevant event highlight;
- Personal MUG identity.

Large saturated lane-colour backgrounds should be avoided where they reduce readability or make multi-lane displays visually noisy.

<a id="presentation-design-spec--63-status-colours"></a>
##### 6.3 Status colours

Status colour must have consistent meaning across screens. Yellow/Red race-control presentation remains governed by [Shared display](#common-display-spec--plonk--play--common-display-specification).

Colour must not be the only means of communicating a critical state; text, iconography or light state should also make the meaning clear.

<a id="presentation-design-spec--7-typography-and-numeric-presentation"></a>
#### 7. Typography and numeric presentation

Race information must remain readable at a glance and at the expected viewing distance.

Use a compact, modern sans-serif family suitable for screen display. Exact production fonts are an implementation choice until separately frozen, but browser and integrated display should use visually compatible typography.

Typography should distinguish clearly between:

- race state;
- position;
- race progress;
- live timing;
- labels/secondary information;
- configuration/help text.

Changing race numbers must use tabular/fixed-width numeral behaviour where appropriate.

Existing rule from [Shared display](#common-display-spec--plonk--play--common-display-specification) remains mandatory:

> **No UI jitter caused by changing race data.**

Timing-field definitions and number formats are common across Browser and
Taster. Live elapsed/remaining race timers use whole seconds and continue
beyond 3599 without changing units. Completed race/finish durations use
unbounded-minute `mm:ss:xx` with hundredths; Endurance live overtime uses
`+0`, `+1`, `+2` and so on; and a lane's frozen final offset uses
`+mm:ss:xx`, such as `+00:18:42`. The Taster may omit fields outside its
restricted mode set but may not reinterpret or reformat their authoritative
meaning. Stable regions must reserve sufficient width for these values.

Decorative motorsport fonts may be used sparingly for branding if appropriate, but not for dense timing information.

<a id="presentation-design-spec--8-live-race-display"></a>
#### 8. Live race display

The existing Common Display Specification remains authoritative for live race content and behaviour.

Visually:

- the Race Control Pod is clearly identifiable and remains in a stable location;
- lane rows remain geometrically stable during live racing;
- position changes within a lane row rather than moving the row;
- position and progress are visually dominant;
- Last, Best and Gap are clearly secondary;
- temporary events alter emphasis without rebuilding the whole screen;
- animation must not interfere with reading current race information.

Browser and Taster update from authoritative state changes and event/fact
notifications. They may advance or render displayed clock values from
authoritative P&P timing. Stable lane rows and numeric regions remain in place;
only the affected regions change. Frequent redraw is reserved for an active
event animation and must restore the background beneath moving content. A
client must not independently establish race timing, GO, pause, expiry, finish
or overtime authority, or use presentation refresh as a substitute for it.

Race theatre is appropriate for meaningful moments such as:

- start sequence;
- position change;
- fastest lap;
- final lap;
- finish;
- Yellow/Red race control.

Continuous animation for its own sake is not part of the P&P visual language.

<a id="presentation-design-spec--9-personal-mug-display"></a>
#### 9. Personal MUG display

The Personal MUG display is a glanceable racing instrument, not a miniature copy of the full scoreboard.

Position and race progress dominate.

Last Lap, Best Lap and Gap remain secondary. Personal alerts may temporarily dominate as already specified in [Shared display](#common-display-spec--plonk--play--common-display-specification).

A phone held or positioned several feet away should remain useful without requiring the MUG to read small labels while driving.

Portrait and landscape presentations may arrange the same information differently while retaining the same hierarchy and visual identity.

<a id="presentation-design-spec--10-results"></a>
#### 10. Results

Results switch from live fixed-lane presentation to finishing order as already specified.

The visual hierarchy should answer immediately:

> **Who won, and what happened?**

The winner should be obvious without excessive celebration or obscuring the other results.

Detailed lap, sector, speed and penalty analysis remains secondary and may live behind a Details / Analysis view.

Controls shown in visual prototypes are not automatically product requirements. In particular, History is automatically recorded according to the governing history specification; a mock-up button such as `SAVE RESULTS` must not create a new manual-save requirement.

<a id="presentation-design-spec--11-home-and-mode-selection"></a>
#### 11. Home and mode selection

Home should present the supported race modes clearly using the common Mode Card component.

Lap Race remains the normal/dominant starting choice in accordance with the browser-flow specification. Other supported modes remain visible without making the Home screen feel like a configuration menu.

The Taster may restrict this choice surface to **LAPS** and **ENDURANCE**.
That restriction changes available controls only; it does not rename or alter
the canonical Browser race modes or finish policies.

Mode cards may use simple icons and one short explanatory phrase. They should not contain paragraphs of help.

<a id="presentation-design-spec--12-configuration-screens"></a>
#### 12. Configuration screens

Configuration uses the same P&P visual language but may be denser than racing screens.

Configuration should favour conventional, understandable controls over decorative novelty.

Advanced settings must not visually overwhelm normal setup. Contextual help and numeric controls follow the rules in [Shared display](#common-display-spec--plonk--play--common-display-specification).

The fact that a configuration page can fit more information on a laptop does not justify displaying every available option simultaneously.

<a id="presentation-design-spec--13-responsive-presentation"></a>
#### 13. Responsive presentation

P&P is one presentation system, not separate unrelated applications for each screen size.

Responsive layouts should adapt the shared components and hierarchy approximately as follows:

- **Phone portrait:** Personal MUG / focused single-user presentation; very large glanceable values.
- **Phone landscape:** compact Personal MUG or compact race presentation.
- **Tablet:** full Race Control and normal setup/results.
- **Laptop/desktop:** full Race Control, configuration and detailed analysis.
- **TV/projector:** spectator-first scoreboard with controls normally absent.
- **Integrated P&P display:** local self-contained presentation optimised for its physical screen and controls.

These are presentation priorities, not hard device restrictions.

<a id="presentation-design-spec--14-multiple-simultaneous-displays"></a>
#### 14. Multiple simultaneous displays

Different connected displays may legitimately show different views of the same authoritative race.

For example, at the same moment P&P may show:

- full scoreboard on a TV;
- Race Control on a tablet;
- Personal MUG views on phones;
- local race state on the integrated display.

All should unmistakably belong to the same P&P system.

The browser remains a presentation client. Visual effects, layout and animations must never become timing authority or alter authoritative race state.

<a id="presentation-design-spec--15-visual-prototypes"></a>
#### 15. Visual prototypes

Visual mock-ups are design references, not behavioural specifications.

A mock-up may contain invented example names, cars, numbers, controls or layouts solely to communicate visual direction. Such content must not silently override the governing product specifications.

Before implementing functionality inferred from a mock-up, check the relevant product specification.

The currently accepted design-board direction demonstrates:

- dark P&P visual language;
- yellow brand accents;
- clean mode cards;
- stable multi-lane timing rows;
- strong Personal MUG hierarchy;
- coherent Race Control, Results and spectator presentations;
- shared component styling across different device formats.

It does **not** freeze exact pixels, exact fonts, example data, decorative car graphics or any invented controls shown by the concept artwork.

<a id="presentation-design-spec--16-accessibility-and-practical-use"></a>
#### 16. Accessibility and practical use

P&P screens are used while people are watching and driving slot cars, often at a distance and in imperfect lighting.

Therefore:

- important text and numbers require strong contrast;
- critical information must not rely on colour alone;
- touch targets must be comfortably usable;
- important race values should not require close reading;
- decorative elements must never obscure timing information;
- motion should be brief and purposeful;
- layouts must tolerate longer MUG names and realistic numeric values without collapsing.

<a id="presentation-design-spec--17-implementation-boundary"></a>
#### 17. Implementation boundary

This specification deliberately freezes the **visual direction and consistency rules**, not a pixel-perfect final UI.

Exact spacing tokens, colour values, fonts, icon set, animation timings and component dimensions should be established through implementation/prototyping and then recorded once proven.

Do not delay race-engine or product-behaviour work merely to perfect cosmetic detail.

Equally, do not allow temporary development UI to become the product appearance by accident.

---

**Presentation principle:** Professional enough to look like a product; simple enough to remain Plonk & Play™.

---

<a id="pp-taster-spec--pp-taster-specification"></a>
### Integrated Display (Taster)

<a id="pp-taster-spec--purpose"></a>
#### Purpose

P&P Taster is the built-in, restricted client of the normal P&P Browser/Race
Control experience. It is not a separate racing product. It is deliberately
simpler than full browser-based P&P Race Control: its purpose is to let a MUG
switch on the standard base product and run the supported subset without first
connecting a phone, tablet or computer.

The Taster uses the integrated 7-inch, 800 × 480 landscape colour touchscreen.
It should be capable of attractive fonts, colours, graphics and the P&P visual
language. It must not look or behave like a crude segment display. No rotary
encoder or physical control buttons are required; local operation is through
the touchscreen.

<a id="pp-taster-spec--cross-client-contract-alignment"></a>
#### Cross-client contract alignment

The Taster is a controller-local presentation and control client over the
normal P&P authority. It does not create a second Race Control, Race Engine,
timing, results or persistence authority. The canonical race behaviour is
defined in [Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) and the shared presentation rules are
defined in [Shared display](#common-display-spec--plonk--play--common-display-specification) and [Presentation design](#presentation-design-spec--plonk--play--presentation-design-specification); this
document defines only the Taster's restricted scope, controls and layout.

Where this document is silent, those canonical contracts apply. A different
screen size or control surface may omit features, but it may not reinterpret
race state, timing, finish policy, result meaning or event presentation.

The Taster leaves its Welcome state only after configuration has loaded and
P&P is genuinely capable of racing for the selected installation. Optional
services, including Browser connectivity and official record storage, must not
block a standalone Taster race when the required race capabilities are healthy.

<a id="pp-taster-spec--factory-taster"></a>
#### Factory Taster

The factory/default Taster uses the standard two-sensor, two-lane base installation in [Out-of-box base configuration](#configuration-and-identity-spec--14-out-of-box-base-configuration). This defines the sensor roles, physical arrangement and capability limits. On the compatible standard installation no sensor-role, lane-count or race-mode setup is required.

Factory remembered Race Setup (approved 18 November 2026):
- LAPS selected;
- target: 10 laps;
- Endurance duration: 5 minutes.

<a id="pp-taster-spec--standalone-controls"></a>
#### Standalone controls

Taster should remain comparable in complexity to a basic Scalextric-style lap counter.

The intended local controls are deliberately simple. Taster must not require the SMUG merely to choose an ordinary race distance or duration.

The local touchscreen is the only Taster control surface. It provides the
simple two-lane experience directly without a rotary encoder, physical control
buttons or a browser.

<a id="pp-taster-spec--ready"></a>
##### READY

Taster provides exactly two locally selectable race types:

- **LAPS** — local distance adjustments select 1 to 999 laps;
- **ENDURANCE** — 1 to 999 minutes.

These are the only Taster race modes. Open Practice and Timed Stage are not
offered on the local touchscreen.

On first/factory use, READY uses the [factory settings](#pp-taster-spec--factory-taster).

Thereafter the last selected race type, the last selected lap distance and the
last selected Endurance duration are retained through the existing Memory/Store
boundary and restored after power-off/restart.

At READY, the touchscreen provides:

- a LAPS/ENDURANCE selector;
- an adjustment control for the selected value, wrapping between 1 and 999.
  Ten-step repeats reach the boundary before wrapping on the following step;
- a START control for the displayed race.

Selecting LAPS adjusts laps; selecting ENDURANCE adjusts minutes.

START is not a second Taster-specific race mechanism. Once requested, the selected Taster setup is submitted to the normal P&P start procedure and normal Race Control/session machinery.

The Taster shows a shared Lap Race target above 999 as supplied by the
Browser or restored from storage, without altering it. Opening the screen,
selecting a mode or adjusting Endurance duration leaves that lap target intact.
However, standalone Taster START must reject a displayed Lap Race target above
999 and require an explicit local distance adjustment before it can start.
The first explicit local distance adjustment enters the local range at 999.
The existing hold-to-adjust flow submits the new value on release. Browser-
controlled races may use the shared positive 32-bit lap-target range; this
Taster-only start boundary must never rewrite the shared setting silently.

Taster setup is translated into the normal race modes and finish policies:

- LAPS uses `LapRace` with `CompleteFullRaceDistance`;
- ENDURANCE uses `Endurance` with `CompleteCurrentLap`.

The Browser may expose additional modes, but the Taster never relabels or
redefines them. Its ENDURANCE choice is the normal P&P `Endurance` mode with
the canonical Finish Current Lap policy.

<a id="pp-taster-spec--during-a-race"></a>
##### During a race

RACING includes one small, unobtrusive interruption control. Its visual
treatment belongs to the Taster presentation design; it is not a second race
operation.

The control recognises a deliberate 700 ms touch-and-hold on release. A short
touch, or a hold released before 700 ms, has no race-control effect. A hold at
or beyond 700 ms submits the normal authoritative `PAUSE` operation once.

Once the pause is authoritative, the Taster offers only:

- **RESUME** — use the normal P&P restart countdown and resume the preserved
  race;
- **ABANDON RACE** — requires explicit confirmation, then returns to READY and
  creates no completed result.

The shared pause/restart timing rule in [Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) applies:
paused time and an authoritative restart countdown do not add racing time.
RESUME continues from the shared P&P timing boundary. The Taster may advance
or render pause/countdown values from authoritative P&P timing, but it does not
independently establish pause or countdown authority.

The Taster does not offer **RESTART**. Race Control remains responsible for
pause settlement, resume timing, abandonment and the no-result/no-History
outcome.

<a id="pp-taster-spec--finished"></a>
##### FINISHED

The final result remains displayed indefinitely until the MUG acts.

- **RACE AGAIN** returns to READY with the previous mode and value retained;
- a separate **START** selection is required to begin the next countdown and
  race.

The standalone Taster result is a sealed in-memory result view, not a local
History screen. **RACE AGAIN** discards that displayed standalone result when
the authoritative operation completes and READY is restored; it does not
automatically start another race.

Taster must not grow into a general menu-driven recreation of the browser interface. If configuration becomes menu-heavy or requires navigating levels, it belongs on the SMUG.

<a id="pp-taster-spec--what-the-mug-sees"></a>
#### What the MUG sees

The Taster preserves the original Wokwi screen composition: the P&P graphical
layout, five start lights and two stable lane rows. Moving cars and animated
flags remain deferred; the static presentation below supersedes the earlier
lane-animation restart proposal. The Taster uses
the bottom message strip. READY, RACING and FINISHED use the same basic
two-lane layout; there is no separate Results screen and no developer or
simulation control surface.

Before the race, the local screen shows the Taster race type/distance and the two competitors as READY.

During a race it provides one stable display row for each of the two lanes/competitors. Each row shows the useful live information appropriate to the race type, including:

- lane number or configured MUG name;
- lap count/progress;
- latest lap time;
- race time/countdown information where appropriate for ENDURANCE.

The Browser and Taster consume the same authoritative state and presentation
events/facts. The Taster may advance or render displayed clock values from
authoritative P&P timing, but never independently establishes the GO instant,
race timing, expiry, finish, overtime, classification or result authority.
Numeric fields and lane rows remain in fixed regions; changing values do not
move unrelated layout. A car or finish effect is a response to its
authoritative event and is active only for that event.

The accepted physical Taster timer presentation is fixed-position `MM:SS`:
two-minute digits, a stationary colon and two-second digits. Digits are drawn
into fixed cells so changing values never shift horizontally. This is a local
presentation convention; the authoritative time remains the shared P&P value.

An authoritative lap or finish event may give only the affected lane a 450 ms
brightness pulse. Where brightness cannot be applied, use its perimeter
highlight instead. The other lane remains undisturbed. A finished winner keeps
a static wreath; it is not an animation. Graphics and sound are Presentation
responses to authoritative race events only and have no timing or Race Engine
authority.

On the winning/final lap, the appropriate lane may use a chequered-flag/finish treatment rather than the ordinary lap animation.

For Taster LAPS races, the canonical `CompleteFullRaceDistance` contract in
[Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) applies: either lane may be first to the selected
distance, each lane's final time freezes at its own eligible crossing, and the
shared race completes only when the required entries have finished. Taster
exposes no alternative finish-policy setting.

For Taster ENDURANCE races, the canonical `FinishCurrentLap` contract in
[Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) applies: the next eligible crossing after the one
shared expiry point contributes to each lane's result, with any finishing
offset/overtime derived from that same authority.

Timing fields and number formats are shared with the Browser exactly as
defined in [Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) §4.6. Live elapsed/remaining race
timers use whole seconds beyond 3599 without changing units; completed
race/finish durations use unbounded-minute `mm:ss:xx` with hundredths; live
Endurance overtime uses `+0`, `+1`, `+2` and so on; and each lane's frozen
final offset uses `+mm:ss:xx`, such as `+00:18:42`. The Taster does not
redefine those formats and reserves stable width for their largest values.

At finish it shows the finishing/result information, including finish time as appropriate, followed by the simple result. The result remains on screen until the MUG chooses Race Again.

If SMUG has configured MUG names, those names may replace generic LANE 1 / LANE 2 labels on the Taster display rather than adding unnecessary extra clutter.

The local display uses the authoritative five-light countdown: one through
five red lights progress before GO, then all five lights turn green together.
The Taster never calculates GO or race timing locally.

<a id="pp-taster-spec--smug-configurable-taster"></a>
#### SMUG-configurable Taster

The Factory Taster is only the out-of-box default.

Using the browser, a SMUG may configure a different supported Taster for subsequent standalone use. This allows the full interface to perform setup once while leaving later operation simple enough for MUGs or MINIMUGs without supervision.

The controlling Browser's Integrated Display settings allow editing the remembered mode, lap target and Endurance duration directly. They use the shared remembered Race Setup, not an independent conflicting preset store. Lane display names and built-in graphics can be set independently; blank names use generic lane labels. A configured Taster must remain self-contained: the browser must
not allow a Taster configuration that subsequently requires a browser merely
to run it.

RESTORE FACTORY TASTER restores the [factory remembered Race Setup](#pp-taster-spec--factory-taster), factory lane labels and graphics. It is distinct from system factory reset. It preserves shared
audio preferences, existing track configuration and saved racer assignments.
Standalone operation still requires a compatible two-lane installation.

<a id="pp-taster-spec--factory-car-graphics"></a>
##### Factory car graphics

The factory Taster provides the original green MGB and red E-Type side-profile
graphics, assigned independently to Lane 1 and Lane 2. SMUGLAND may select a
different graphic for either lane only from P&P's built-in graphic set. Custom
image upload is not part of this contract. Lane graphic selections are
persistent Taster configuration and are restored after restart; they are not
racer identities and are not assigned through the MUG catalog. They do not alter lane assignments or race timing.

<a id="pp-taster-spec--control-hierarchy-and-records"></a>
##### Control hierarchy and records

The Integrated Display controls supported requests when no controlling Browser is connected. A connected controlling Browser takes precedence automatically; the local display remains subordinate and its control requests are disabled. On disconnection, control may return through the committed lease boundary, without a second authority.

For official-record eligibility, source freezing and handover see [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy).

<a id="pp-taster-spec--minimug-principle"></a>
#### MINIMUG principle

A SMUG should be able to configure the Taster in the browser and then leave MINIMUGs to use the resulting standalone race without needing the browser or supervision for normal operation.

The usability test is intentionally simple:

> A MINIMUG unfamiliar with P&P should be able to switch it on, understand the local display, start the race and race again without needing to learn a menu system.

<a id="pp-taster-spec--browser-boundary"></a>
#### Browser boundary

Taster is the standard two-sensor/two-lane standalone experience.

If the installation uses four lanes or otherwise uses additional sensors/hardware beyond the standard two-sensor Taster arrangement, configuration and race operation use the browser-based full P&P interface.

The local display does not need to become a general-purpose interface for arbitrary expanded hardware.

<a id="pp-taster-spec--connection-guide"></a>
#### Connection guide

The local display also helps the MUG reach full P&P.

P&P provides its own factory local Wi-Fi access point automatically. The
local display should clearly show the P&P network name and Browser address so
that a phone, tablet or computer can connect without house/club Wi-Fi or
Internet. The factory network is deliberately open; joining it provides
connectivity but does not grant SMUG/Race Director control. A QR connection aid
may be added later.

Optional external house/club Wi-Fi is not required for racing. Its sanitized
status may be presented alongside the factory AP status, including the assigned
LAN address or `plonk-and-play.local` when available. The external-network
password is controller-private and must not appear in the Taster, logs,
diagnostics, History, Results or support downloads. Loss of that external
network must not affect the P&P access point or racing. Network configuration
authority remains the Browser Advanced/Diagnostics Network section and the
existing controlling-SMUG lease; this specification does not create a second
authority mechanism.

Thus the local screen has two complementary purposes:

1. provide the complete simple Taster experience without a browser;
2. lead the MUG into full browser-based P&P when more capability is wanted.

The printed MUGGLE Guide remains the quick physical-installation guide; P&P itself should help explain the browser connection once powered.

---

<a id="browser-flow-results-history-spec--browser-flow-results-and-history"></a>
### Browser Flow, Results and History

<a id="browser-flow-results-history-spec--home"></a>
#### Home

The browser Home screen presents the five principal modes as visible selectable cards/tiles:

- **Lap Race** — visually dominant/default;
- **Practice**;
- **Endurance**;
- **Timed Stage**;
- **Drag**.

Do not hide modes in a dropdown merely to save space.

Secondary Home actions include **Race Again**, **Results & History** and **Setup**.

<a id="browser-flow-results-history-spec--browser-presentation-architecture"></a>
#### Browser presentation architecture

P&P ships with a small number of good **out-of-the-box (OOTB) race-screen designs** rather than requiring a MUG or SMUG to design a screen before racing. The four retained Browser presets for owner review are **Classic, Pit Wall, Grandstand and Timing Tower**; see [visual reference handover](#implementation-spec--approved-visual-references). Production implementation retains the shared reusable model, not four race engines.

The standard Browser race displays shall be constructed from **reusable presentation components** using the same declarative screen-layout model intended for future SMUG-editable screens. OOTB screen designs are presets, not separately hard-coded interfaces.

The presentation component set may include, as appropriate to the active mode and installed capabilities, items such as MUG name, car name, lane, position, lap count, last lap, best lap, gap, race clock, sector information, fuel, reaction time, start lights and race/flag status.

The layout model shall support three display classes:

- **Mobile**;
- **Tablet**;
- **PC / large display**.

A screen design may share presentation choices and data bindings across those classes while allowing component position and size to differ by display class. A SMUG should therefore be able to customise a screen without requiring a PC layout to be mechanically squeezed onto a phone.

The future **Screen Designer** shall edit this declarative presentation model rather than HTML or executable application code. Its intended interaction is a constrained visual editor: choose P&P components/fields from a palette, position and resize them, and adjust permitted presentation properties such as typography, colours, backgrounds, lights and simple effects.

A SMUG shall be able to duplicate an OOTB preset and customise the copy. OOTB presets remain available for straightforward recovery and use.

This does **not** make the Screen Designer a v1 implementation requirement. The architectural requirement for v1 is that ordinary Browser race screens are built from reusable components and a declarative layout/preset model so that later SMUG editing extends the same system rather than requiring the Browser UI to be rewritten.

Custom screens remain presentation only. Their available live data comes from P&P's externally presentable state/Noticeboard and standard presentation interfaces. A custom layout does not acquire authority over Race Control, Race Engine or other operational state merely because it displays their information.

<a id="browser-flow-results-history-spec--lap-race-setup"></a>
#### Lap Race setup

The normal setup presents:
- lane/MUG assignment;
- optional car only when car recording is enabled;
- ROTATE LANES, Reverse direction, automatic rotation and direct entry rearrangement (see [Race Entry assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour));
- target laps using decrement / editable value / increment;
- finish behaviour;
- MORE RACE OPTIONS;
- a visually dominant START control.

More Race Options expands without requiring a separate page and may contain start lights, GO style, final delay, false-start behaviour and audio. Advanced contains less commonly adjusted timing details.

Settings auto-save where intention is unambiguous.

<a id="browser-flow-results-history-spec--practice-setup-and-control"></a>
#### Practice setup and control

Practice exposes Open Practice in v1. The planned Timed Practice variant may be shown as unavailable; it must not be startable until authorised and implemented. SMUG selects Immediate start (default) or Practice start (normal five-light sequence) in Practice Setup. The auxiliary Integrated Display uses full-screen start lights only for Practice start, otherwise goes straight to Practice information. This is an approved addition to the V18 baseline, not a separate race mode. Detailed repeat-start workflow and false-start handling in Practice remain to be agreed; existing immediate Resume and non-competitive Practice timing/record rules are unchanged.

Open Practice continues until SMUG/Race Control selects **END PRACTICE** in
the normal Browser. This presentation action invokes the authoritative
Practice **END SESSION** operation.

When implemented post-v1, Timed Practice ends automatically at its configured duration; Race Control may also end it early.

Session-level controls belong to SMUG/Race Control, not ordinary MUG displays.

<a id="browser-flow-results-history-spec--endurance-setup"></a>
#### Endurance setup

Endurance setup includes:
- MUG/lane assignment;
- editable duration;
- the two established finish behaviours: **Stop at zero** or **Finish current lap**;
- More Race Options.

Time remaining is dominant during the race.

<a id="browser-flow-results-history-spec--active-session-controls-frozen"></a>
#### Active-session controls (frozen)

The normal Browser exposes only the controls appropriate to the authoritative
session lifecycle and mode. P&P remains responsible for validating and
executing every operation; these rules define the normal Browser presentation
only.

| Authoritative session | Normal Browser controls |
| --- | --- |
| Active Lap Race or Endurance | **PAUSE** only |
| Paused Lap Race or Endurance | **RESUME**, **RESTART**, **ABANDON** |
| Active Open Practice | **PAUSE** only |
| Paused Open Practice | **RESUME**, **RESTART**, **END PRACTICE** |

Competitive **ABANDON** is shown only while a Lap Race or Endurance session is
authoritatively paused. It requires confirmation and uses the existing
authoritative P&P abandonment path. It creates no official Result, History
entry or Finish Display, and returns through authoritative state to READY/Home.
At the P&P operation boundary this existing competitive abandonment may retain
the operation name **END RACE**; **ABANDON** is the normal Browser control
label.

**END PRACTICE** is shown only while Open Practice is authoritatively paused.
It uses the existing authoritative Practice completion semantics and proceeds
to Practice Summary. Practice does not show **ABANDON**.

Practice Summary is the completed summary of that Practice session. It offers
**PRACTICE AGAIN** as the primary action and **HOME** as the secondary action.
PRACTICE AGAIN asks P&P to prepare a new Open Practice using the retained
relevant setup, racers and lanes; it does not resume or resurrect the ended
Practice and does not start automatically. HOME leaves the summary and returns
to the normal READY/Home experience without creating another Practice.

The Browser must not implement these transitions as local state. A reload,
reconnect or second client renders the controls from the current authoritative
P&P lifecycle and mode.

<a id="browser-flow-results-history-spec--timed-stage-setup"></a>
#### Timed Stage setup

Timed Stage first offers two clear geometry choices:
- **Loop**;
- **Point-to-Point**.

It then supports **Single Stage** or **Multi-Stage Event** without requiring a motorsport-discipline label.

Loop setup includes MUG, stage laps and Runs.

Point-to-Point includes MUG, A→B/B→A direction and Runs.

Where multiple Runs are configured, established scoring choices such as All Runs Count / Best Run Counts may be offered.

A Multi-Stage Event is a lightweight collection of Loop and/or Point-to-Point stages with event progress and cumulative results. Do not expand v1 into a general competition-management system.

<a id="browser-flow-results-history-spec--drag-setup"></a>
#### Drag setup

Drag setup includes:
- MUG;
- Sportsman or Pro start tree;
- optional contextual course length and scale information;
- STAGE as the primary action.

Selecting Drag proposes the standard two-sensor base roles contextually: Sensor 1 Start, Sensor 2 Finish. Do not ask SMUG to reassign those sensors unnecessarily. When START is accepted, those Drag roles are frozen into the Session Definition for that session; the Input Module itself does not acquire Start/Finish meaning.

Drag remains usable with no length or scale configured. Missing optional values may be added directly from Drag setup.

<a id="browser-flow-results-history-spec--immediate-lap-race-result"></a>
#### Immediate Lap Race result

The primary Results screen should answer who won without becoming an analysis dashboard.

Lap Race Results use the columns **POSITION | LANE/RACER | PROGRESS | TIME |
BEST | GAP**. PROGRESS is authoritative classified race progress against the
configured target where applicable. TIME is the authoritative per-entry
race-relative elapsed duration at that entry's permitted final crossing, or at
its last completed lap when the race stopped that entry before a final crossing.
A racer with no completed lap has no TIME and shows an em dash. The Browser
never derives this value. GAP uses only authoritative gap information supplied by P&P;
the Browser does not calculate classification or substitute a finish time for
progress or gap.

Show:
- finishing position;
- MUG or lane;
- total result/finish time or gap as appropriate;
- each MUG's Best Lap;
- overall Fastest Lap;
- RACE AGAIN;
- HOME;
- DETAILS.

DETAILS exposes the lap-by-lap data and other available information such as penalties, sectors or speed where relevant.

Record-eligible completed results enter History automatically under [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy) and the storage contract. There is no routine Save Results action; a live non-official result does not become History.

<a id="browser-flow-results-history-spec--results--history"></a>
#### Results & History

History is a simple newest-first list of completed **official** sessions, as determined by [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy) and storage integrity.

A basic mode filter may be provided:
**All | Lap | Endurance** in v1; future official modes may add filters when implemented. Open Practice has no official History.

Selecting a history item reopens its result/details.

Do not turn v1 History into a general statistics/database-management interface.

<a id="browser-flow-results-history-spec--rolling-history-buffer"></a>
##### Rolling history buffer

History is a fixed-size rolling buffer.

When the buffer is full, storing a new completed session automatically discards the oldest retained session. The MUG is not required to manage storage or respond to capacity warnings during normal use.

The exact factory buffer size is an implementation choice to be set after realistic session-size/storage testing.

Persistent record summaries are independent of the rolling session buffer. Expiry of an old session must not make a valid PB or track record revert merely because its originating session has fallen out of History.

The Browser displays committed History and records; it never infers eligibility. See [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy).

<a id="browser-flow-results-history-spec--v1-records"></a>
##### v1 records

Keep records deliberately modest:
- personal fastest lap per MUG;
- overall fastest lap / track record.

Store useful underlying race data so richer statistics can be added later without making them v1 requirements.

<a id="browser-flow-results-history-spec--storage-implementation-boundary"></a>
#### Storage implementation boundary

The product requirement is **persistent structured history**, not a particular storage engine.

SQLite, structured files or another suitable implementation may be evaluated during implementation/prototyping. Browser behaviour must not depend on that choice.

General principle:

> **Design broadly. Implement narrowly.**

<a id="system-architecture--11-sound"></a>
#### 11. Sound

Sound is an output/presentation capability, not part of Race Control.

The authoritative race responsibility may publish a fact such as `FASTEST_LAP`; the sound capability decides how that fact is rendered as speech/audio.

Sound may also be requested as a scheduled logical action where timing matters, such as start-sequence beeps. Such actions use P&P System Time in the same way as other time-critical outputs rather than depending on message-arrival time.

The agreed audio facilities use firmware tones and P&P-provided removable-storage speech/effects, as specified in [shared audio](#product-facilities-spec--3-audio-and-removable-storage). Exact hardware/format choices must satisfy that contract.

Sound hardware and persistent storage may eventually share physical storage if suitable hardware permits it, but the architecture must not depend on that physical arrangement.

<a id="system-architecture--12-presentation-and-user-interaction"></a>
#### 12. Presentation and user interaction

Presentation observes authoritative P&P state. It does not create or maintain a competing race state.

A newly connected or reconnected display must be able to obtain the complete current state required to present the session rather than replaying every event it missed.

Multiple clients may simultaneously present different views of the same race, for example:

- full Race Director view;
- individual lane/driver view;
- spectator display;
- future trackside display.

Loss, sleep or disconnection of a browser must not stop the race.

<a id="system-architecture--120-local-browser-access"></a>
##### 12.0 Local Browser access

P&P provides its own local Wi-Fi access point automatically out of the box.
The factory access point is deliberately open, without a Wi-Fi password.
SMUGs and MUGs connect their phone, tablet or computer to that local network
and use the P&P Browser address presented by P&P. Normal racing does not
require house/club Wi-Fi or Internet access.

Wi-Fi association grants connectivity only; it never grants Master,
controlling-SMUG or Race Director authority. Those remain governed by the
single P&P control lease. P&P may additionally operate as an AP+STA device:
one remembered external house/club Wi-Fi network may be configured from
**Settings → Advanced → Diagnostics → Network**. The P&P HTTP service becomes
available as soon as the factory AP is ready and is never gated by station
connection. The same P&P Browser/API/SSE authority is available through the
factory AP and, when connected, the assigned external-network address.

Network status and an explicit Wi-Fi scan are observational and may be viewed
or requested by any connected Browser. Selecting an SSID is local Browser UI
state until submitted. When a remembered network is disconnected, the
controlling SMUG may issue **RECONNECT**; this uses the driver's stored STA
configuration, has no SSID/password payload and cannot overwrite credentials.
**RECONFIGURE** is the separate credential-writing operation for changing the
remembered network. Reconnect, Disconnect, Reconfigure and Forget Network are
authoritative mutations and require the current Master/Race
Director/controlling-SMUG lease. A non-controlling mutation is rejected by
P&P. Network association never grants or transfers that lease.

The factory AP remains automatic, deliberately open and independent of the
external network. External connection/reconnection must be non-blocking;
failure or loss of the external network must not affect the AP, HTTP service,
racing, timing, results or authority. `plonk-and-play.local` is the preferred
LAN discovery name when mDNS is available; the assigned LAN IP remains the
fallback and is presented as current network status.

The external-network password may be accepted for provisioning and persisted
only as required for reconnection. The ESP-IDF/Arduino Wi-Fi driver's
flash-backed STA configuration is the sole persistent credential store; P&P
does not maintain a duplicate SSID/password record. It must never subsequently
be returned, displayed, logged, diagnosed, exported, or included in History or
Results. An explicit Disconnect retains that driver configuration while
suppressing automatic reconnection for the remainder of the boot. Explicit
Reconnect clears suppression without calling the credential-writing path;
explicit Reconfigure clears suppression and may replace the driver's stored
configuration. Reboot clears suppression and permits automatic reconnection.
Forget Network erases the driver's persisted STA configuration and P&P's configured
state, with authoritative failure if that operation cannot be completed.
Network diagnostics expose only sanitized AP/STA state, addresses, signal and
failure information. Scan/retry/backoff constants remain implementation
choices subject to bounded, non-blocking behaviour and real-S3 acceptance.

<a id="system-architecture--121-context-sensitive-controls"></a>
##### 12.1 Context-sensitive controls

Race Control determines which session-operation Requests are valid in its current state. Other authoritative responsibilities validate Requests that belong to them. Presentation exposes only controls appropriate to the currently valid Requests.

For example, **Pause** is relevant only once a race is running, while **Resume** is relevant only when paused.

Hiding an invalid control is a usability measure, not the security/validity mechanism: Race Control must still reject session-operation Requests that are invalid for its current state.

<a id="system-architecture--122-one-master--race-director"></a>
##### 12.2 One Master / Race Director

There is one control lease at a time. The **Master**, **Race Director** and
controlling **SMUG** are the same authority and are different names for the
client holding that lease. SMUG is not a second ownership system or an
account identity.

The controller-side Presentation / User Interaction responsibility owns client/session identity, which client currently holds Race Director authority, and deliberate transfer or recovery of that authority. It determines whether a particular client is permitted to submit Race Director Requests.

Race Control does not need to know browser, connection or transport details. It receives authorised logical session-operation Requests and separately determines whether each Request is valid in the current race/session state and what consequence it has.

Only the Master may issue Race Director Requests such as:

- start;
- pause;
- resume;
- end/abort;
- configure race length/rules.

Connecting another browser must never accidentally create another Race Director.

Master authority belongs to a client/browser session, not to the human identity of the Race Director. When no Master binding exists, a client becomes suitable for initial Race Director assignment only by performing the deliberate bootstrap action. Opening, polling, State retrieval, connection order and Race Director Requests never make a client suitable or assign Master authority.

A second control client may view the control screen but remains non-Master. It
may deliberately choose **Take Control**. P&P owns the takeover request and
there may be only one pending request. If the current controller is connected,
P&P presents that controller with an authoritative request — **ALLOW** or
**KEEP CONTROL**. ALLOW atomically transfers the single Master/Race
Director/SMUG lease to the requester and the former controller immediately
loses the privilege. KEEP CONTROL leaves ownership unchanged and informs the
requester that the request was refused. A connected controller's failure to
answer never transfers control after an arbitrary timeout.

If P&P authoritatively establishes that the controlling client has genuinely
disconnected, another client may deliberately acquire the lease without that
client's approval. The technical disconnection test is an implementation
matter, but it must be based on an authoritative client/session boundary, not
an arbitrary inactivity timeout. A pending request remains P&P state: a
controller that reloads or reconnects with the same valid client identity
reconstructs the outstanding prompt. A controller returning after a completed
transfer has no automatic right to regain the lease and must use Take Control
normally. Only one pending request is accepted; concurrent requests are
rejected as busy/already pending.

This is deliberately a trust model rather than an account/security system. P&P enforces exactly one Master but does not require user accounts, passwords or PINs merely to arbitrate control around a home slot-car track.

Loss, sleep or disconnection of the Race Director client does not affect Race
Control or the running race. Transfer changes only which client may issue
Race Director Requests; it never interrupts a session, changes timing,
classification or results, and Race Control itself never moves into the
browser.

<a id="system-architecture--123-non-master-clients"></a>
##### 12.3 Non-Master clients

Other clients may observe the same or different presentation views but cannot take authoritative race control.

They may submit specifically permitted race inputs/requests. An example is a driver's **CAR OFF / re-call** button.

A lane-associated client can report that its car is off the track without becoming a controller.

The active Session Definition can determine what a **CAR OFF** input means. It may, for example, be disabled, notify/request action from the Race Director or automatically invoke the configured track-call behaviour.

The client always reports the same semantic event — **CAR OFF** — rather than directly commanding a pause or power change. Race Control interprets the event according to the active Session Definition.

<a id="system-architecture--124-requests-and-browser-synchronisation"></a>
##### 12.4 Requests and browser synchronisation

User interfaces send logical **requests**, not direct mutations of authoritative P&P variables. A request asks the appropriate authoritative responsibility to perform an action or change configuration. The request is accepted or rejected, with a reason where useful; the browser must not assume that submitting a request means the requested change occurred.

Physical controls enter through the appropriate hardware/input path. Depending on the device, that path may map a simple physical detector state into a semantic race input, or an intelligent device may report an appropriate standard semantic input directly.

Both physical controls and user interfaces ultimately present the appropriate P&P responsibility with defined semantic inputs or requests which are validated and interpreted according to current state and configuration. Race Director requests such as START, PAUSE and RESUME are validated against current state. Other permitted inputs, such as CAR OFF or TRACK_CALL_REQUEST, are interpreted according to the active Session Definition and current authoritative state and may result in an authoritative action.

Browser-facing communication distinguishes five semantic kinds of information:

- **Request** — browser to P&P: asks P&P to do something.
- **Request Result** — P&P to the requesting browser: reports whether that request was accepted or rejected, with a reason where useful.
- **State** — authoritative information maintained by P&P describing what is true now.
- **NOTICEBOARD_CHANGED** — P&P to browser: a notification indicating that something material on the authoritative Noticeboard has changed.
- **Event/Fact** — P&P to browser: notification that something has happened, useful for presentation such as animation, sound or temporary messages.

P&P owns and maintains authoritative live State. Browsers decide which State they need for the view they are presenting; P&P does not need knowledge of individual browser screens or to construct a screen-specific authoritative State model for each connected browser.

On initial connection or reconnection, a browser obtains authoritative State sufficient to construct the correct display from nothing. It need not replay every event that occurred before or while it was disconnected.

Every material change to the authoritative Noticeboard can cause a `NOTICEBOARD_CHANGED` notification. The notification need not identify the fields that changed or contain their new values; its purpose is to tell a browser to look again at the current authoritative information it needs.

On receiving such a notification, a browser refreshes the authoritative State needed by its current view. Further `NOTICEBOARD_CHANGED` notifications received while that refresh is already in progress do not require parallel refreshes. The refresh process must leave the browser with internally consistent current authoritative State, including any further changes that occurred while an earlier refresh was being initiated or completed.

Rapid authoritative State changes therefore do not require a browser to observe or render every intermediate State. Superseded intermediate State may be skipped; the browser ultimately renders what is currently true. Where individual occurrences themselves matter for presentation, their Events/Facts remain distinct from current State.

A browser may retain previously obtained State while it remains synchronised and no `NOTICEBOARD_CHANGED` notification has been received. If connection/synchronisation is lost, it must no longer assume that cached State is current. Recovery requires obtaining current authoritative State again.

A browser may issue operational or configuration Requests only while it is synchronised with current authoritative P&P State. If synchronisation is lost, browser controls that would issue such Requests remain unavailable until full synchronisation has completed again. This client-side restriction does not replace P&P validation: P&P remains responsible for validating every Request against current authoritative State and control authority before accepting it.

A missed Event/Fact must never leave a browser permanently wrong. Events/Facts are transient aids for timely presentation such as sounds, flashes, animations or temporary messages; ordinary live Events/Facts do not require guaranteed delivery, acknowledgement, queuing or replay to a browser. If information must remain valid after the moment has passed, it must be represented in authoritative State independently of any Event/Fact. Thus an Event/Fact such as LAP_COMPLETED or NEW_FASTEST_LAP may enhance immediate presentation, while the resulting lap count, last/best lap or current overall fastest lap remain authoritative State.

Persistent results/history are separate from browser Event/Fact delivery. Information that P&P must retain as a lasting record is stored by P&P according to the results/history model and does not depend on any browser having received a live Event/Fact.

Timing-critical physical event capture, ordering, race interpretation and authoritative State maintenance must never wait for browser reads, browser rendering, network delivery or other presentation activity. Slow or disconnected clients must not delay or compromise race timing or authoritative race processing.

The implementation used to determine what must be refreshed, combine rapid changes, and transport notifications or State remains an implementation/prototyping decision. Possible mechanisms such as internal revisions, dirty flags, logical State blocks, deltas or broader snapshots are not prescribed by this architecture and do not make a Noticeboard version part of the conceptual contract.

For time-critical coordinated actions, current State alone is not sufficient. Once a START Request has been accepted, P&P determines the authoritative future **GO instant** using its own high-resolution timing clock. Connected presentation/output devices are given sufficient advance information to schedule their local start-sequence presentation against that same authoritative GO instant rather than waiting for a network message sent at GO.

This allows browser lights, local displays, sounds and other outputs to appear to reach GO together despite ordinary communication latency. P&P itself judges detector events, reaction timing and false starts against the same authoritative GO instant. Presentation latency must therefore not redefine when GO actually occurred.

The mechanism used to relate a browser or output device's local scheduling to P&P's authoritative timing clock is an implementation/prototyping decision. The architectural requirement is common scheduling against a P&P-dictated future instant, not a particular clock-synchronisation protocol.

<a id="system-architecture--125-browser-clockdisplay-updates"></a>
##### 12.5 Browser clock/display updates

P&P remains authoritative for race timing. Timing calculations use the required high-resolution P&P timing representation independently of the coarser resolution chosen for normal display.

Where a normal running clock or countdown is displayed in whole seconds, P&P sends the browser an authoritative time/state update once per displayed second while that value is changing. A meaningful state change or event between those regular ticks causes an immediate appropriate update rather than waiting for the next one-second tick.

Thus display update cadence does not determine timing accuracy: a detector event may be timestamped and processed at high resolution even though the ordinary running clock changes only once per second.

The browser need not run an independent authoritative race clock. This keeps multiple displays tied to P&P's authoritative timing while avoiding unnecessary continuous browser updates.

Separately from race timing, P&P may maintain a low-stakes human-readable clock for browser display and connection indication. P&P does not require an RTC, Internet time or manually entered wall-clock time for racing. When a browser connects, it may supply its own approximate local time to initialise this display clock. Accuracy to real-world time is not important to race operation.

Once initialised, P&P maintains this display clock and sends its current value to connected browsers once per second. This is a dedicated time/display synchronisation stream, not a Noticeboard mutation: its once-per-second updates do not generate `NOTICEBOARD_CHANGED` notifications. Browsers display the P&P-supplied value rather than independently advancing their own copy. These regular clock updates therefore also provide a simple connection/health indication without requiring a separate routine heartbeat.

A single missed update need not imply failure. If updates cease beyond an implementation-defined tolerance, the browser treats itself as unsynchronised, freezes rather than locally advancing the P&P display clock, and indicates connection loss. Recovery requires a fresh full authoritative state snapshot before normal delta processing resumes.

The display clock is not an authoritative source for lap times, elapsed/remaining race timing, reaction times, scheduled race actions or other competition timing. Those remain based on P&P's high-resolution monotonic/system timing. History need not retain wall-clock/date information merely because the display clock exists; retained race timing concerns durations and results.

The transport, protocol and payload representation used to carry Requests, Request Results, State and Events/Facts remain implementation decisions.

<a id="system-architecture--136-manual-user-gateway-mug"></a>
#### 13.6 Manual User Gateway (MUG)

A **Manual User Gateway (MUG)** is a phone, tablet, computer or other browser host acting as a human-facing P&P interface. The integrated physical Taster is another P&P human-interface participant, not a second controller. When no SMUGLAND controller is connected, the Taster may control the supported standalone operations. A connected SMUGLAND controller automatically supersedes Taster control; when that connection is lost, the controller may restore control to Taster through the same control-lease boundary. Race Control remains the sole lifecycle authority.

Multiple MUGs may be connected simultaneously. One MUG may hold Race Director authority while others operate as permitted driver, display or spectator clients. A MUG does not own race state and its disconnection must not stop the race.

MUG is internal/technical vocabulary; customer-facing interfaces may use ordinary terms such as Race Director, driver or display.

<a id="product-facilities-spec--3-audio-and-removable-storage"></a>
#### 3. Audio and removable storage

<a id="product-facilities-spec--31-base-requirement"></a>
##### 3.1 Base requirement

Every normal P&P System Controller should include:

- persistent microSD/removable storage capability;
- onboard audio capability;
- a small internal speaker sufficient for clear race-control tones and speech.

This is a standard base capability, not an upgrade.

The exact hardware implementation is not yet fixed.

<a id="product-facilities-spec--32-storage-uses"></a>
##### 3.2 Storage uses

Persistent removable storage may retain:

- race/session results;
- lap-by-lap data;
- Timed Stage results;
- personal bests and records;
- track records;
- MUG profiles;
- event history;
- CSV exports/backups;
- configuration backup;
- audio/voice files.

The detailed record schema remains implementation design.

<a id="product-facilities-spec--33-audio-implementation"></a>
##### 3.3 Audio implementation

Essential tones reside in firmware. Prerecorded speech and effects are stored on SD.
P&P supplies the audio packs. Customer-installed packs are outside the product scope.
Launch packs provide British male and female voices. The audio pack design must
support future languages.

The implementation should favour a small, reliable, low-BOM solution with sane file/folder handling.

Options to compare before hardware freeze include:

1. off-the-shelf audio + SD module;
2. separate microSD plus audio module/amplifier;
3. audio/SD circuitry integrated into P&P's own PCB.

Previous awkward module folder/naming conventions must not dictate the new architecture.

High audio power is not a goal in itself. Clear internal race-control sound matters more than 5 W versus 10 W; a few watts may be sufficient.

<a id="product-facilities-spec--34-audio-destinations"></a>
##### 3.4 Audio destinations

Audio event/type and audio destination are separate choices.

Selectable destinations:

- P&P unit;
- Browser;
- Both;
- Mute.

In standalone operation, Browser-only falls back to P&P's internal speaker.
Mute is always respected, including during destination fallback. Browser audio
can deliver individual MUG alerts. Personalised spoken announcements using
MUG names are deferred until after v1.

<a id="product-facilities-spec--35-preferences-defaults-and-testing"></a>
##### 3.5 Preferences, defaults and testing

P&P uses one shared set of sound preferences, including standalone operation. The controlling Browser's System sound settings allow individual event configuration, including countdown beeps or spoken countdown, race announcements and spoken lap times. Preferences survive restart; system factory reset restores the defaults, but restoring the Integrated Display defaults does not reset sound.

| Factory setting | Default |
|---|---|
| Enabled events | Welcome sound, countdown beeps, lap-completion sound, final-lap announcement, time warnings, pause/resume, winner announcement, victory fanfare |
| Disabled events | Race setup announcement, spoken lap times, fastest-lap announcement, halfway announcement |
| Destination and volume | P&P internal speaker, 60% |

Endurance warnings are individually configurable.

| Endurance event | Factory default |
|---|---|
| Five minutes remaining | Off |
| One minute remaining | On |
| Thirty seconds remaining | Off |
| Ten seconds remaining | On |
| Time expired | On |
| Finish current lap | On |

Provide audio preview and independent tests for the P&P speaker and Browser
audio. Disable previews and both tests during active races.

<a id="product-facilities-spec--36-playback-priority-and-reliability"></a>
##### 3.6 Playback priority and reliability

Playback priority, highest first:

1. Countdown and GO.
2. Race interruption and completion.
3. Winner announcement.
4. Final lap and time remaining.
5. Fastest lap.
6. Ordinary lap sounds and spoken lap times.

Higher priorities may interrupt lower priorities. Discard obsolete
announcements, including stale cues on PAUSE and race restart. Play the pause
announcement once per pause. Alternating yellow warning lights remain silent.

Audio must never delay racing and must work without an internet dependency.
If a file is missing or corrupt, use an appropriate basic tone. Report audio
failures through diagnostics.

<a id="product-facilities-spec--4-browser-and-system-controller-responsibilities"></a>
#### 4. Browser and System Controller responsibilities

The System Controller owns:

- authoritative timing;
- race/session logic;
- authoritative state;
- small race facts/events and state updates.

Browsers own:

- graphics;
- animation;
- formatting;
- browser audio;
- presentation appropriate to their role.

Official timing must never depend on browser latency.

The architecture should support several simultaneous browser clients. A representative four-MUG setup may involve six browsers:

- one main scoreboard;
- four personal MUG displays;
- one Race Director display.

This scale should be prototype-tested under realistic worst-case conditions before hardware is frozen.

<a id="product-facilities-spec--41-local-taster-and-smug-control-hierarchy"></a>
##### 4.1 Local Taster and SMUG control hierarchy

See [Integrated Display control hierarchy and records](#pp-taster-spec--control-hierarchy-and-records) and [factory car graphics](#pp-taster-spec--factory-car-graphics). The Integrated Display is not an independent racing product.

<a id="product-facilities-spec--6-start-presentation-and-audio"></a>
#### 6. Start presentation and audio

Start behaviour is a race-mode concern, but presentation facilities should support parameterised sequences rather than hard-coded animations.

For normal Lap racing this includes three- or five-light sequences, configurable red-light interval and configurable final delay.

The same authoritative GO time must govern race timing, display and any scheduled physical/audio outputs. Presentation may be theatrical; timing remains boring and reliable.

<a id="product-facilities-spec--7-solo-and-multi-user-product-behaviour"></a>
#### 7. Solo and multi-user product behaviour

P&P must remain usable without a dedicated Race Director.

Supported operating patterns include:

- dedicated Race Director;
- Race Director who is also racing;
- solo MUG.

Every normal race/session must be runnable by one MUG.

At the other end, P&P may scale to a main display, several personal MUG displays and a separate Race Director display without changing the fundamental race engine.

<a id="implementation-spec--pp-normal-browser-----implementation-handover"></a>
### Browser design and implementation contract

<a id="implementation-spec--status-and-priority"></a>
#### Status and priority

This package contains the current approved visual references and agreed
normal-Browser UX.

If anything conflicts, use this priority:

1.  P&P authority/product specification
2.  This implementation handover
3.  Approved visual HTML references
4.  Mock-up behaviour/scripts

The HTML files are visual references, not production code to paste
blindly. Existing authoritative P&P behaviour and API boundaries must be
retained. Do not resurrect older/intermediate mock-ups.

<a id="implementation-spec--authority-rule"></a>
#### Authority rule

P&P is authoritative. The Browser is presentation and control.

The Browser may present authoritative state, submit requests,
temporarily display a locally requested setup value while it settles,
and add presentation-only effects.

The Browser must not determine laps, timing, position, gaps, winner,
finishing state, classification, readiness or acceptance of
configuration. Reload/reconnect reconstructs the correct presentation
from P&P.

<a id="implementation-spec--visual-language"></a>
#### Visual language

Use dark navy/charcoal, restrained P&P yellow, white/off-white primary
information and lane colours for lane identity. Keep the
race-equipment/instrument appearance rather than game styling. Retain
the P&P logo, pictorial top navigation, yellow selected-nav treatment,
common Race Control Pod and fixed lane-row language. Racer and Car
remain separate identities.

Normal navigation: Race \| Results \| Track Setup \| Racers \| Settings.

<a id="implementation-spec--race--ready"></a>
#### Race / READY

The page answers: "What is P&P ready to do if I press START?"

Show Race Entries, racer/car/lane assignments, Race Setup, authoritative
mode/configuration, Track Status, START and Latest Result.

Mode selector: LAP RACE \| PRACTICE \| ENDURANCE \| TIMED STAGE \| DRAG

Unavailable modes are shown unavailable only when P&P reports that
state.

There is no Save/Apply. Setup controls request changes. START starts
P&P's currently authoritative proposal; it never submits Browser-local
pending values.

<a id="implementation-spec--lap-race-setup"></a>
#### Lap Race setup

Target laps use the [shared positive 32-bit configuration boundary](#configuration-and-identity-spec--121-normal-race-screen); the 1–999 local adjustment limit applies only to standalone Integrated Display. Race Entries and lane controls follow [Race Entry assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour). The known extreme lap-target-plus-penalty arithmetic defect remains a delivery task, not a changed product limit.

Exactly three finish behaviours:

1.  All stop when winner finishes.
2.  Winner finishes --- all others finish current lap.
3.  Winner finishes --- all others complete total race distance.

Do not show an empty Advanced/More Race Options panel. Add it only when
P&P has genuine authoritative secondary options.

<a id="implementation-spec--endurance-setup"></a>
#### Endurance setup

Duration 1–999 whole minutes, with the shared remembered default defined in [Integrated Display factory settings](#pp-taster-spec--factory-taster); Race Entries and lane controls follow [Race Entry assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour).

Exactly two finish behaviours: STOP AT ZERO or FINISH CURRENT LAP.

<a id="implementation-spec--practice-setup"></a>
#### Practice setup

Initial implementation is OPEN PRACTICE. Timed Practice remains
unavailable until P&P has the specified authoritative mechanics.

Open Practice has no race distance, winner, finish policy or competitive
position. Provide the two start options defined in [Practice setup and control](#browser-flow-results-history-spec--practice-setup-and-control).

<a id="implementation-spec--numeric-controls"></a>
#### Numeric controls

Support direct typing, decrement/increment and desktop hover + mouse
wheel.

Wheel over the numeric field changes by ±1 within bounds and consumes
the wheel event so the page does not also scroll. Moving the pointer
away restores normal page scrolling.

Wheel/hover is a convenience only. Touch must have equivalent usable +/-
and direct-entry operation.

Typing uses an edit buffer. Typing 120 must not generate intentions 1,
12 and 120. Commit on Enter, blur or a short settle interval.

<a id="implementation-spec--pending-setup"></a>
#### Pending setup

The Browser may immediately show the latest desired value, but that is
not authoritative.

While unresolved, retain the latest desired value, do not let older P&P
readback bounce it backwards, use only subtle pending indication if
delay is perceptible, and make START unavailable.

On acceptance reconcile normally. On rejection restore authoritative
value and explain briefly.

Rapid changes such as 10→4→2→1 must leave 1 displayed while machinery
catches up.

Reload/reconnect discards local pending state and reconstructs from P&P.

<a id="implementation-spec--starting"></a>
#### STARTING

STARTING uses the stable race screen.

There are five physical lamp positions. P&P supplies authoritative start
presentation, including configured 3-red or 5-red sequence, LIGHTS OUT
and GO.

At GO all five positions are green for one second, then extinguish.
Green is not left on throughout RACING.

The Browser must not recreate the start state machine from its own
elapsed time.

Endurance must show the configured authoritative duration before GO.

<a id="implementation-spec--lap-race-live-screen"></a>
#### Lap Race live screen

Rows remain in physical lane order throughout the race.

Columns: POSITION \| LANE/RACER \| PROGRESS \| LAST \| BEST \| GAP

Progress example: 6 / 10.

Show mode/race distance, Fastest Lap, Track Record, Race Control Pod,
elapsed RACE TIME and PAUSE.

RACE TIME counts upward from authoritative race timing in whole seconds,
continuing past `3599` without resetting or changing units. Pause freezes it;
Resume continues it.

<a id="implementation-spec--endurance-live-screen"></a>
#### Endurance live screen

Same visual family as Lap Race.

Columns: POSITION \| LANE/RACER \| LAPS \| LAST \| BEST \| GAP

Show prominent TIME REMAINING counting down from authoritative timing.

Clearly show ENDURANCE · STOP AT ZERO or ENDURANCE · FINISH CURRENT LAP.

For FINISH CURRENT LAP after expiry, TIME REMAINING remains `0` seconds and
OVERTIME appears where applicable, counting upward from the same authoritative
expiry point as `+0`, `+1`, `+2` and so on. Each lane freezes its first
eligible final-crossing offset as `+mm:ss:xx` to hundredths; both offsets stay
visible when the race finishes. These values follow the common contract in
[Race modes](#race-modes-product-spec--plonk--play--race-modes-product-specification) §4.6.

P&P determines whether overtime applies and supplies authoritative
timing. Browser does not infer expiry/overtime.

<a id="implementation-spec--lane-density"></a>
#### Lane density

The same scoreboard works from 1--8 lanes: 1--2 generous, 3--4 medium,
5--6 compact, 7--8 dense.

Do not make one lane absurdly tall. Preserve column geometry. Optional
car imagery scales with row density and remains secondary.

Eight lanes should be visible without vertical scrolling at the intended
primary desktop/tablet Race Control viewport.

<a id="implementation-spec--lap-and-finish-animation"></a>
#### Lap and finish animation

Normal lap: car accelerates left→right and exits.

Finishing lap: the same-size car accelerates left→right, pulls an
expanding chequered band, exits, and the band briefly ripples/settles
then clears.

The car does not grow during the animation.

If one racer has finished while others continue, show a small chequered
flag in that lane's fixed reserved flag slot. Do not alter row geometry
and do not add redundant FIN text.

P&P supplies the authoritative lap/finish event. Animation never gates
or determines sporting state.

<a id="implementation-spec--pause-first-session-management"></a>
#### PAUSE-first session management

While actively running, the only deliberate session-management action is
PAUSE.

Competitive Lap Race / Endurance after authoritative PAUSED: RACE PAUSED
--- RESUME \| RESTART \| ABANDON

ABANDON is destructive and requires confirmation. Confirmed abandonment
returns to READY with no Result, History entry or Finish Display.

Practice after authoritative PAUSED: PRACTICE PAUSED --- RESUME \|
RESTART \| END PRACTICE

Practice does not use ABANDON.

<a id="implementation-spec--resume"></a>
#### Resume

Lap Race / Endurance: RESUME → authoritative STARTING → start
presentation → GO → RACING.

Endurance timing remains frozen until authoritative GO.

Open Practice Resume returns directly to active Practice with no fake
STARTING/countdown/GO. Completed laps remain; partial-lap handling
follows authoritative P&P Practice semantics.

<a id="implementation-spec--competitive-finish-display"></a>
#### Competitive Finish Display

P&P owns whether Finish Display is active, its timing and when it ends.
Browser owns appearance.

The Browser default Finish Display duration is 5 seconds, configurable including zero (zero immediately proceeds to Results).

Final live scoreboard freezes. Browser must not independently start its
own five-second timer.

Reload during Finish Display reconstructs the current authoritative
point; it does not restart the delay.

The controlling Race Director may request skipping the remaining Finish Display; an observation-only client may not mutate this state. The authoritative state and deadline determine the transition.

<a id="implementation-spec--results"></a>
#### Results

Results deliberately resemble the live race display.

Live race rows = physical lane order. Results rows = authoritative
classification/finishing order.

Lane number/colour remains attached to racer.

Retain familiar scoreboard geometry, Fastest Lap and Track Record. Use
restrained first-place emphasis; no separate giant winner card.

Bottom actions: HISTORY \| DETAILS \| RACE AGAIN

RACE AGAIN requests the next proposal and returns to READY; it never
immediately starts a race.

<a id="implementation-spec--practice-completion--summary"></a>
#### Practice completion / summary

Flow: Active Practice → PAUSE → END PRACTICE → Practice Summary.

No competitive Finish Display or winner theatre.

Practice Summary uses the same visual family but is non-competitive.
Show relevant authoritative information such as LANE \| RACER/CAR \|
LAPS \| BEST LAP, plus Fastest Lap / Track Record where P&P supplies
them.

Do not show position, gap, winner, finishing order, leader highlighting
or competitive chequered treatment.

The lifecycle decision is explicit: an ended Practice cannot be resumed.
Practice Summary presents PRACTICE AGAIN (primary) and HOME (secondary).
PRACTICE AGAIN is an authoritative request to prepare a new Open Practice with
the retained relevant setup; START remains a separate action.

<a id="implementation-spec--individual-racer-finish"></a>
#### Individual racer finish

If one racer finishes while others continue, the live race remains, the
racer stays in the physical lane row, finished state is clear, the small
chequered flag appears in the reserved slot and authoritative
progress/classification is shown.

Browser never infers finished state from lap counts.

<a id="implementation-spec--devices"></a>
#### Devices

PC/laptop: full SMUG/Race Control experience.

Tablet: full SMUG/Race Control experience. Landscape is
preferred/optimised, not mandatory.

Phone: Personal MUG is the default intended experience rather than a
miniature SMUG. Do not create an artificial architectural prohibition
against Race Control on a phone unless separately decided.

All devices remain clients of the same authoritative P&P state.

<a id="implementation-spec--browser-compatibility"></a>
#### Browser compatibility

Target current mainstream Chrome, Edge, Firefox and Safari.

Core operation must not depend on Windows, Chrome-specific behaviour,
hover, mouse wheel or right-click.

Mouse, keyboard and touch may provide different conveniences, but
equivalent core functionality must remain available.

No browser version numbers are frozen.

<a id="implementation-spec--multiple-browsers"></a>
#### Multiple Browsers

Several Browsers may connect simultaneously, including full Race Control
and Personal MUG presentations. Different presentation/device layouts
must never create different race truth.

<a id="implementation-spec--deliberately-outside-this-handover"></a>
#### Deliberately outside this handover

The Master Project Plan determines current v1 scope, including History/Details, diagnostics and advanced settings. Do not infer future-mode requirements from mockups or resurrect old handover exclusions that conflict with the approved v1 boundary.

<a id="implementation-spec--approved-visual-references"></a>
#### Approved visual references

-   01_RACE_READY.html --- approved Race/READY visual baseline.
-   02_LAP_RACE.html --- Lap Race live baseline with elapsed Race Time.
-   03_ENDURANCE.html --- Endurance live baseline with Time Remaining
    and conditional Overtime example. Bottom test buttons are not
    production UI.
-   04_RESULTS.html --- Results baseline: live-race family,
    authoritative result order.
-   05_PRACTICE_SUMMARY.html --- current Practice Summary baseline; note
    the RESUME authority check above.
-   06_EIGHT_LANE_DENSITY.html --- approved density/layout reference;
    placeholder car graphics are not approved assets.
-   07_LAP_FINISH_ANIMATION.html --- lap/finish animation and fixed
    finished-lane flag-slot reference.

These files preserve original visual references. The later consolidated [V18 Browser design](https://github.com/ConnalM/Plonk-and-Play/blob/ec46ff5b8df07d1477fa40543423509c778a22d1/design/smugland/PP_Browser_v18_navigation.html) is the **owner-approved design baseline** (19 November 2026), subject to testing. Gina's current design assignment is complete. Changes revealed by actual testing may refine the design through normal scope control; this is not production acceptance. V17/V18 source review confirms the corrected lane model, complete-entry rotation, finish-display handoff, four preset layouts and retained factory restoration. Only agreed governing semantics may be implemented. Representative and future-mode fixtures must not become race logic or scope commitments. The Master Project Plan records the approval/production acceptance gate.

<a id="physical-integrated-display-constraints"></a>
### Physical Integrated Display constraints

Preserve the accepted 800 × 480 RGB ESP32-S3 single-framebuffer/20-line bounce-buffer design (one framebuffer in PSRAM, 64,000 bytes total internal DMA SRAM), 12 MHz pixel clock, Octal PSRAM 80 MHz XIP, VSYNC restart and RGB-driver-only -Os workaround. Pin mapping: DE 41, VSYNC 40, HSYNC 39, PCLK 42; R={14,21,47,48,45}, G={9,46,3,8,16,1}, B={15,7,6,5,4}; backlight GPIO2. Operational commands remain in [XIP build reference](WP7_XIP_APPLICATION_BUILD_REFERENCE.md).

The accepted static header uses the 145 × 55 logo, five centred 60 × 60 start lamps/static bezel, one-second green GO, amber RELEASE feedback after 700 ms hold and alternating yellow/OFF warning lamps every 500 ms while PAUSED. Active heading left aligned, lane counters blank before GO, stable MM:SS timer cells. Preserve the 450 ms lane-only brightness pulse (perimeter fallback), static winner wreath and original per-lane artwork. Use small dirty regions; preserve genuine full-frame fallback with logged reason, not unnecessary redraws on setup acknowledgement. Normal NVS persistence and touch/timing responsiveness must remain intact.

Moving cars, animated chequered flags and production double-buffer experiments are deferred. When explicitly resumed, restore only the affected lane background, arm from authoritative events, prove one lane before both and do not confuse isolated double-buffer diagnostic success with integrated proof. The detailed original [physical checkpoint, artifact hashes, failure history, source references and staged experiment](<https://github.com/ConnalM/Plonk-and-Play/blob/13662fbc5e42fbc83204b7662643a2c43fe8fefb/docs/MASTER_PROJECT_PLAN.md#superseded-animation-handover--static-taster-presentation>) is preserved as historical evidence.



## Configuration

<a id="configuration-and-identity-spec--plonk--play--configuration-and-identity-specification"></a>
### Configuration, Racer and Car identity

**Status:** Committed product-design specification  
**Scope:** Basic v1 MUG, car and lane configuration.  
**Principle:** Identity enhances P&P; it must never be required to use P&P.

<a id="configuration-and-identity-spec--1-immediate-use-without-setup"></a>
#### 1. Immediate use without setup

P&P must remain genuinely Plonk & Play.

A new/default two-lane system can operate as:

- **Lane 1 — MUG 1**
- **Lane 2 — MUG 2**

The user does not have to create profiles, cars or a database before racing.

<a id="configuration-and-identity-spec--2-persistent-mug-identity"></a>
#### 2. Persistent MUG identity

A saved MUG has:

- a permanent internal ID;
- a user-visible name.

For v1, **Name is the only required profile field**.

The internal ID, not the displayed name, owns historical identity. Renaming a MUG must therefore not orphan or split their previous results.

Richer profile information may be added later without changing this identity model.

<a id="configuration-and-identity-spec--3-selecting-mugs"></a>
#### 3. Selecting MUGs

Tapping a lane's MUG name opens a simple selection list/dropdown containing saved MUG names plus appropriate temporary/default choices.

Example:

- Connal
- Hazel
- Baird
- Freya
- Guest
- No MUG / Clear
- **+ New MUG**

A MUG already assigned to another active lane remains visible but is greyed/unavailable rather than silently disappearing.

Selecting **No MUG / Clear** returns the lane to its anonymous/default identity such as **MUG 1**.

<a id="configuration-and-identity-spec--4-text-entry-clear-control"></a>
#### 4. Text-entry Clear control

Where clearing an editable text value is a sensible operation, P&P should provide a one-action **Clear** control rather than requiring repeated backspace/delete actions.

This applies to MUG-name entry and should be treated as a general UI principle.

<a id="configuration-and-identity-spec--5-guest-mugs"></a>
#### 5. Guest MUGs

A Guest can race without being added permanently to the saved MUG list.

Guest name is optional:

- blank → display **Guest**;
- entered, e.g. **Dave** → use Dave for the current race/session without automatically creating a permanent profile.

A temporary named Guest can be promoted directly to a permanent saved MUG without re-entering the name.

This may be offered while entering the Guest name (for example **Remember Dave**) and/or after the race (**Remember this MUG**).

General principle:

> Temporary identity data should be promotable to permanent identity without requiring re-entry.

<a id="configuration-and-identity-spec--6-cars"></a>
#### 6. Cars

Car recording is optional and should not clutter the normal setup screen by default.

Cars are independent entities. A car does **not** belong permanently to a particular MUG.

When car recording is enabled, a race entry may reference an optional saved car.

Therefore the conceptual relationship is:

**Race Entry = MUG + Lane + optional Car**

The same car may be driven by different MUGs in different races/heats.

This supports shared cars, including groups of younger/informal racers, without distorting the data model.

For v1, if car profiles are exposed, a simple user-visible car name is sufficient. Manufacturer/model/year/catalogue-style data is not required.

<a id="configuration-and-identity-spec--7-lane-assignment"></a>
#### 7. Lane assignment

For a normal two-lane Lap setup, the presentation should remain approximately:

**LAP RACE — 10 laps**

- **Lane 1 — Connal**
- **Lane 2 — Hazel**

**START**

Changing a MUG should normally require only selecting a different name from the lane dropdown.

<a id="configuration-and-identity-spec--8-swap-lanes"></a>
#### 8. Swap Lanes

The former SWAP LANES label is superseded by ROTATE LANES (two lanes necessarily exchange). See [Race Entry assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour) for the governing contract: fixed physical lanes, complete pending Race Entries including optional Car move together.

<a id="configuration-and-identity-spec--9-remembering-the-last-setup"></a>
#### 9. Remembering the last setup

P&P automatically remembers the most recently used straightforward race setup, including MUG/lane assignment and normal race settings.

Returning to the same mode should therefore normally present the previous useful setup, allowing regular users to reach START with minimal interaction.

A separate Save Setup / Load Setup system is not required for v1; named presets are a later candidate in the Master Project Plan.

<a id="configuration-and-identity-spec--10-future-compatibility-without-v1-scope-creep"></a>
#### 10. Future compatibility without v1 scope creep

V1 provides straightforward Racer/Car identity and pending setup, including the agreed [automatic lane rotation](#race-setup-lane-assignment-spec--6-automatic-rotation). The data model must permit richer Racer/Car profiles and statistics, multi-race competitions, heats/rounds and other event structures without pre-implementing them. Their release disposition is the Master Project Plan.

The design must not assume that:

- one MUG permanently belongs to one lane;
- one car permanently belongs to one MUG;
- a race must always be completely standalone.

> **Design broadly. Implement narrowly.**


<a id="configuration-and-identity-spec--11-track-configuration"></a>
#### 11. Track configuration

The v1 product assumes one remembered physical Track Configuration rather than requiring a named multi-track database.

Normal use must not require the MUG to create or name a track before racing.

The Track Configuration contains information P&P actually uses, including:

- configured lane count;
- remembered/default device-role proposals used when forming a Race Setup and, on accepted START, the Session Definition;
- Start/Finish arrangement;
- optional track length;
- optional track scale.

Track length enables appropriate speed calculations. Track scale enables scale-equivalent speed calculations.

The underlying design must not unnecessarily prevent multiple saved physical Track Configurations being added later.

<a id="configuration-and-identity-spec--111-rally-stages-are-not-tracks"></a>
##### 11.1 Rally stages are not tracks

A Rally Stage is an event/run definition layered on top of the current physical Track Configuration; it is not another physical track record.

Stages may differ in how the installation is used, for example:

- loop, 2 laps;
- loop, 3 laps;
- A→B;
- B→A.

Stages may optionally be given user-visible names.

<a id="configuration-and-identity-spec--12-configuration-presentation"></a>
#### 12. Configuration presentation

P&P separates **what are we doing now?** from **what is this installation?**

<a id="configuration-and-identity-spec--121-normal-race-screen"></a>
##### 12.1 Normal race screen

The ordinary race screen contains the small number of settings commonly needed for the imminent race.

For Lap Race this may include:

- MUG/lane assignments;
- target laps;
- finish behaviour;
- ROTATE LANES, Reverse direction and automatic rotation; direct Race Entry rearrangement (see [Race Entry assignment and rotation](#race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour));
- START.

It also provides unobtrusive access to **More Race Options** and **Setup**.

See [Integrated Display configuration and restoration](#pp-taster-spec--smug-configurable-taster) for the local setup, factory values and reset boundaries.

The shared configuration accepts positive 32-bit lap targets. Loading a
historical configuration preserves its stored lap target and unrelated
customer settings. The standalone adjustment/START restriction is governed by [Integrated Display READY](#pp-taster-spec--ready); it does not limit Browser race distances. Corrupt or invalid records continue through the existing storage recovery rules.

<a id="configuration-and-identity-spec--122-more-race-options"></a>
##### 12.2 More Race Options

More Race Options contains settings that may genuinely vary from race to race but need not clutter the normal screen.

Examples include:

- start-light count;
- GO style;
- final-delay style;
- false-start response;
- audio event choices and destination.

See [shared audio preferences, defaults and testing](#product-facilities-spec--35-preferences-defaults-and-testing).

An **Advanced** area may contain less-common detailed parameters such as exact random-delay limits or red-light interval.

<a id="configuration-and-identity-spec--123-setup"></a>
##### 12.3 Setup

Setup describes the P&P installation rather than today's race.

Logical areas include:

- **Track** — length, scale, Start/Finish arrangement;
- **Hardware** — detected sensors/modules, remembered/default role proposals and optional hardware;
- **MUGs** — management of saved identities;
- **Cars** — only where optional car recording is enabled;
- **Sound & Display**;
- **System** — networking, update, backup/restore, diagnostics and similar system functions. External Wi-Fi configuration belongs in the **Network** section of **Settings → Advanced → Diagnostics**; it is not a separate authority or setup area.

Normal MUG creation/selection remains available directly from the race screen; a MUG-management area must not become a prerequisite to racing.

<a id="configuration-and-identity-spec--13-save-and-confirmation-behaviour"></a>
#### 13. Save and confirmation behaviour

P&P should not require routine SAVE/APPLY interaction when the user's intention is already unambiguous.

Race settings become the current/remembered **Race Setup** as they are changed. The SMUG edits Race Setup; P&P creates the fixed internal Session Definition only when START is accepted.

Installation settings are stored when changed/confirmed.

Confirmation is reserved for actions that are destructive, disruptive or genuinely ambiguous, such as deleting persistent information, materially reassigning hardware or performing a factory reset.

> **Save automatically where intention is unambiguous. Confirm where an action is destructive, disruptive or ambiguous.**


<a id="configuration-and-identity-spec--14-out-of-box-base-configuration"></a>
#### 14. Out-of-box base configuration

The base product should require **zero software configuration** when installed according to the recommended quick-start arrangement.

Factory/default base role proposals are:

- Sensor 1 → Lane 1 Start/Finish;
- Sensor 2 → Lane 2 Start/Finish;

These are remembered/default installation proposals, not racing meaning owned by the Input Module. When START is accepted, the roles actually required for that race are frozen into the Session Definition. The same physical input may therefore serve a different role in another session without changing the Input Module.
- recommended/default physical sensor arrangement → sensor before the Start/Finish line.

The Quick Setup documentation must show the matching physical arrangement clearly.

With this default physical arrangement, straightforward lap counting works immediately. Capabilities that require a different sensor relationship, including applicable false-start detection and reaction timing, are not available merely by pretending the default installation can measure them.

Those unavailable capabilities may remain visible but greyed/locked with an explanation of the physical/configuration change required to enable them.

A MUG who installs the sensors differently can change the remembered arrangement later under Track Setup. A clear diagram should be used rather than relying only on technical wording.

Optional information such as track length, track scale and MUG names must not become first-boot requirements.

<a id="configuration-and-identity-spec--15-muggle-quick-setup-guide"></a>
#### 15. MUGGLE quick-setup guide

The base product requires a short, highly visual **MUGGLE Guide** for first use.

Its purpose is not to replace the full manual. Its job is to get an ordinary MUG from unopened/unconfigured product to a working basic race with the fewest possible decisions.

The intended flow is approximately:

1. **PLONK** — position the two sensors exactly as illustrated in the recommended/default arrangement.
2. **PLUG** — connect Sensor 1/Lane 1 and Sensor 2/Lane 2 and power P&P.
3. **CONNECT** — connect a phone/tablet/computer to P&P's factory local
   Wi-Fi access point and open the Browser address shown by P&P.
4. **PLAY** — choose the basic race setting such as laps and press START.

<a id="configuration-and-identity-spec--151-factory-local-network"></a>
##### 15.1 Factory local network

P&P provides its own local Wi-Fi access point automatically from first boot.
Normal racing is local and does not require a house/club Wi-Fi network or
Internet access. The factory access point is deliberately open, with no
Wi-Fi password; the Taster or other local P&P presentation shows the network
name and Browser address needed for connection. A QR connection aid may be
added later.

Wi-Fi association is only transport connectivity. It does not assign SMUG,
Master or Race Director authority. The existing P&P control-lease rules decide
which Browser/client may issue control requests.

Optional external house/club Wi-Fi is available through the Network section of
Settings → Advanced → Diagnostics and is never required for racing. One
external network may be remembered for reconnection. Any connected Browser may
view sanitized AP/STA status and request a scan; choosing an SSID remains local
presentation state until submission. When a remembered network is disconnected,
the controlling SMUG may issue **RECONNECT**, which uses the ESP32 driver's
already-stored STA configuration and sends no SSID or password. **RECONFIGURE**
is the separate credential-writing operation for changing the remembered
network. Connect/Reconfigure, Reconnect, Disconnect and Forget Network require
the current controlling SMUG/Race Director lease and are rejected
authoritatively for other clients. Association with either the factory AP or
the external network grants connectivity only and never grants control.

The factory AP remains automatic, open and independent. HTTP and racing remain
available as soon as the AP is ready, regardless of station failure or loss.
The external password may be accepted for provisioning and persisted only as
needed for reconnection, but must never subsequently be returned, displayed,
logged, diagnosed, exported, or included in History/Results or support data.
The ESP-IDF/Arduino Wi-Fi driver's flash-backed STA configuration is the sole
persistent credential store for that remembered network; P&P does not maintain
a duplicate SSID/password record. Disconnect retains the driver configuration
but suppresses automatic reconnection for the remainder of the boot. Explicit
Reconnect clears that suppression without writing credentials. Explicit
Reconfigure clears that suppression and may replace the driver's stored
configuration. Reboot clears suppression and permits automatic reconnection.
Forget Network erases the driver's persisted STA configuration and P&P's configured
state, with authoritative failure if the operation cannot be completed.
Network status is sanitized and may include AP/STA state, addresses, signal,
failure information and mDNS status. `plonk-and-play.local` is preferred when
available, with the assigned LAN IP as fallback. Connection, retry and scan
work must remain non-blocking; exact timing constants are not product-fixed.

A small **Want more?** section may point towards optional MUG names, track length/scale, alternative sensor arrangements, enhanced start/false-start functions, Rally, Drag and other capabilities.

Product requirement:

> **If the MUG follows the MUGGLE Guide's recommended physical installation, the software defaults must match it and no Setup step should be required before basic racing.**

---

<a id="race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour"></a>
### Race Setup — Lane Assignment and Rotation Behaviour

**Status:** Accepted product/UI behaviour for implementation  
**Scope:** Race Setup presentation and ownership of pending lane assignments for configured multi-lane tracks.

<a id="race-setup-lane-assignment-spec--1-physical-lanes-and-race-entries"></a>
#### 1. Physical lanes and Race Entries

The current Track Configuration determines the physical lane count shown by Race Setup, up to the supported maximum.

Physical lanes remain fixed. What moves between lanes is the complete **Race Entry**: Racer plus optional Car and any other entry-owned identity required by the pending race setup.

A lane-rearrangement operation therefore changes the pending Race Entry-to-Lane assignment; it does not renumber or redefine the physical track lanes.

<a id="race-setup-lane-assignment-spec--2-race-entry-panel-scaling"></a>
#### 2. Race Entry panel scaling

Race Setup must remain pleasant for a normal two-lane track while also accommodating larger configured tracks, including up to eight lanes where supported.

Do not permanently squash two-lane rows merely to reserve space for eight lanes.

The Race Entries panel should use available vertical space intelligently:

- few lanes: generous row height;
- increasing lane count: progressively more compact rows;
- once a sensible minimum row height is reached: keep that row height and make the Race Entries area internally scrollable if the available viewport still cannot show all configured lanes.

The intent is adaptive row height first, scrolling only when useful. Exact pixel thresholds are presentation implementation details, not product semantics.

Only configured physical lanes are shown. A two-lane Track Configuration shows two lane rows, not empty rows for lanes 3–8.

<a id="race-setup-lane-assignment-spec--3-front-screen-lane-controls"></a>
#### 3. Front-screen lane controls

Lane rearrangement is performed directly on the main Race Setup screen. It does **not** require a separate lane-swap pop-up or modal.

Above the track/lane list, Race Setup provides:

- **ROTATE LANES**;
- a nearby **Reverse direction** toggle;
- **Automatically rotate after each race**.

If an icon accompanies ROTATE LANES, a vertical double-arrow treatment is appropriate because the visible entries move up/down through the lane list. Do not use a horizontal double-arrow that suggests left/right swapping.

The track heading (for example **2-LANE TRACK** or **8-LANE TRACK**) and configured lane rows appear below these controls.

<a id="race-setup-lane-assignment-spec--4-manual-drag-and-drop"></a>
#### 4. Manual drag and drop

Race Entries remain directly draggable on the main Race Setup screen.

- the user may drag a Racer/Race Entry from one lane to another;
- dropping onto an occupied lane exchanges/rearranges the complete entries;
- the operation must not create duplicate lane ownership;
- optional Car identity moves with its Race Entry.

Drag and drop provides arbitrary manual rearrangement. It coexists with ROTATE LANES rather than being replaced by it.

A separate **CHANGE** button on every lane row is not required. The Racer/Car area itself is the natural entry point for selecting or changing that Race Entry.

<a id="race-setup-lane-assignment-spec--5-manual-rotation-and-direction"></a>
#### 5. Manual rotation and direction

**ROTATE LANES** quickly moves all current Race Entries by one physical lane.

For N configured lanes, normal rotation is conceptually:

`1 → 2 → 3 → ... → N → 1`

**Reverse direction** reverses that mapping:

`1 → N → ... → 3 → 2 → 1`

For two lanes, either direction necessarily exchanges the two entries.

The direction is one P&P setting used consistently by both manual ROTATE LANES and automatic post-race rotation. Do not maintain contradictory browser-local direction settings.

Terminology matters:

- **drag/rearrange** describes arbitrary manual reassignment;
- **rotate/rotation** describes systematic movement of every Race Entry by one lane.

Do not call the multi-lane rotation behaviour “swap lanes”.

<a id="race-setup-lane-assignment-spec--6-automatic-rotation"></a>
#### 6. Automatic rotation

**Automatically rotate after each race** is race behaviour, not part of an individual lane row.

When enabled, after a properly completed race P&P rotates every Race Entry one physical lane when preparing the next pending race setup, using the same direction setting as manual ROTATE LANES.

An abandoned/restarted race does not trigger automatic rotation.

The completed race's Results continue to show the lanes actually used in that race. Rotation affects the pending setup for the next race; it does not rewrite the completed result.

The browser must not independently perform an automatic rotation merely because it is open.

<a id="race-setup-lane-assignment-spec--7-numeric-race-controls"></a>
#### 7. Numeric race controls

Numeric Race Setup controls such as target laps should support efficient small and large changes without requiring repeated individual clicks.

For target laps:

- the Browser uses the shared positive 32-bit target range in [configuration](#configuration-and-identity-spec--121-normal-race-screen); only the local Integrated Display adjustment range is 1–999;
- direct text entry is supported;
- keyboard Up/Down may increment/decrement;
- mouse wheel over the numeric control increments/decrements;
- a single press of `+` or `−` changes the value by one;
- holding `+` or `−` repeats the change;
- continued holding progressively accelerates the rate of change.

Acceleration should feel progressive rather than immediately jumping to a large step. Exact timing/rate curves are presentation implementation details and should be tuned by use.

This is a reusable P&P numeric-control behaviour. Other suitable numeric settings such as durations or run counts should use the same interaction pattern where appropriate.

<a id="race-setup-lane-assignment-spec--8-ownership-and-authority"></a>
#### 8. Ownership and authority

The browser is the interface for choosing these options; it is not their authority.

Lane assignments, rotation direction and automatic-rotation state belong to P&P's authoritative **pending Race Setup / pending session proposal**.

Therefore:

- a browser requests a lane-assignment, rotation or setting change;
- P&P validates and owns the resulting pending configuration;
- the browser reads back and presents that authoritative pending state;
- multiple connected browsers must converge on the same pending assignments/settings rather than maintaining private local versions;
- reload/reconnect must reconstruct the same pending state from P&P;
- START validates and commits that single authoritative proposal into the immutable Session Definition.

Once START is accepted, the active Session Definition's lane assignments are frozen for that session. A browser must not casually mutate the active race by editing the next/pending setup.

This requirement is intentionally consistent with the pending-session ownership cleanup: lane-management UI must not recreate Browser-local proposal state.

<a id="race-setup-lane-assignment-spec--9-availability-and-disabling"></a>
#### 9. Availability and disabling

Lane rearrangement and rotation controls are available only when P&P says the pending setup can be edited.

If the relevant configuration is frozen or changing it is not currently permitted, the browser disables the controls rather than making a local change that Race Control will later reject.

<a id="race-setup-lane-assignment-spec--10-presentation-principle"></a>
#### 10. Presentation principle

The ordinary Race Setup remains simple and direct:

- ROTATE LANES, Reverse direction and automatic rotation are visible above the configured track/lane list;
- configured lane rows show Racer and optional Car;
- entries can be dragged directly between fixed physical lanes;
- race-defining settings such as laps and finish behaviour remain alongside the entries;
- More Race Options is reserved for other secondary race behaviour;
- the permanent READY / START contract remains intact.

> **The lane stays put; the Race Entry moves. The browser requests; P&P owns.**

<a id="system-architecture--7-configuration-race-setup-and-working-ram"></a>
#### 7. Configuration, Race Setup and working RAM

Configuration answers:

> **What have we decided to use this equipment and system for?**

Configuration is information, not a standalone operational module. Persistent configuration is owned by Memory. At startup, P&P loads the configuration needed for operation into working RAM; the appropriate modules then use their working configuration locally. They do not turn ordinary local configuration lookups into P&P messages or repeatedly consult persistent storage for timing-critical work.

The P&P Message Bus remains the normal route when one P&P component needs to communicate with another. Local access by a module to configuration/state already available to that module in working RAM is not inter-component communication and does not require the bus.

Configuration is distinct from Registry / Discovery and from live Race Control state.

<a id="system-architecture--71-installation-configuration"></a>
##### 7.1 Installation configuration

Persistent installation information includes device identities, capabilities, remembered/default role proposals and physical information such as:

```text
Device ABC123 = "Main gantry"
ABC123:1 = available detector capability
ABC123:2 = available detector capability
Remembered/default proposal: ABC123:1 → Lane 1 : Start/Finish
Remembered/default proposal: ABC123:2 → Lane 2 : Start/Finish

Device DEF456 = "Back straight"
DEF456:1 → Lane 1 : Sector 1
DEF456:2 → Lane 2 : Sector 1
```

This survives between races and power cycles.

Installation configuration may also contain physical parameters required to interpret configured functions, such as the distance between detection points forming a speed trap.

<a id="system-architecture--72-race-setup"></a>
##### 7.2 Race Setup

Before START, the SMUG edits **Race Setup**: the mutable working choices for the proposed race/session, for example:

```text
Mode = Lap race
Length = 20 laps
Fuel = On
```

Race Setup is working data, not a module. P&P may remember the last useful Race Setup or future named presets in Memory, but the SMUG does not edit a Session Definition directly.

<a id="system-architecture--73-user--presentation-preferences"></a>
##### 7.3 User / presentation preferences

Preferences such as display choices, units, sounds and similar user choices are distinct from both installation configuration and live race state.

<a id="system-architecture--74-configuration-versus-live-state"></a>
##### 7.4 Configuration versus live state

Configuration describes **how P&P should behave**.

Live race/session state describes **what is happening or has happened as a result**, with ownership divided between Race Control session state and Race Engine competition state as defined in [Race Control and Engine](#race-control-engine-design--race-control-and-race-engine-detailed-design).

For example, current lap, current fuel level, race running/paused state and current position are not Configuration.

<a id="system-architecture--75-startup-reconciliation"></a>
##### 7.5 Startup reconciliation

At startup P&P compares remembered configuration with currently discovered hardware:

- **known + present** — restore and continue;
- **known + missing** — retain its assignments/configuration but mark it unavailable;
- **new + unambiguous** — infer/restore an appropriate assignment where safe;
- **new + ambiguous** — ask the user.

Principle:

> **Restore what is known. Infer what is unambiguous. Ask only when there is a genuine choice.**

A startup summary may show the interpreted current configuration briefly so the user can verify it or choose to change setup. Exact UI behaviour is not yet specified.

<a id="system-architecture--76-configuration-changes-during-operation"></a>
##### 7.6 Configuration changes during operation

Configuration changes must be controlled according to their effect on the live system.

Some changes may be safe at any time, some only while idle and some must not change underneath an active race.

When START is accepted, P&P validates the proposed Race Setup against the applicable installation/capability information and creates the fixed **Session Definition** in working RAM. The Session Definition is immutable working data for that session, not a module or P&P Message Bus participant and not something the SMUG edits directly.

For the first Lap Race, the Session Definition contains only what is required to interpret and execute that session: session identity; race rules; Race Entries; the required input-role and output-role assignments; and any physical/rule parameters needed by those rules. A Race Entry has its own stable ID for the session and retains the stable MUG ID plus the MUG display name frozen when START was accepted; optional Car identity follows the same principle when Cars are enabled. Installed capabilities unused by the session need not be copied into the Session Definition. Hardware implementation details such as GPIOs, transport addresses and detector thresholds do not belong in it.

The Session Definition contains the Race Setup **and the session-specific roles assigned to every input, output and other capability required to interpret and execute that session**. Only relevant capabilities need a session role. For example, the same physical detector may be Lane 2 Start/Finish in one session and a Drag finish or speed-trap input in another. Its hardware identity and Input Module behaviour do not change.

Session Definition freezes trusted source and record eligibility at accepted START, governed by [Session source and record policy](#race-control-engine-design--41-session-source-and-record-policy).

Race Control and the Race Engine operate from that fixed Session Definition rather than repeatedly reading mutable configuration and discovering that operation, device roles or rules have silently changed. **PAUSE does not unlock race-affecting configuration:** the same immutable Session Definition remains in force through PAUSE and RESUME. A change that would alter the Session Definition requires the active session to be ended/abandoned and a new session to be started from a newly accepted Race Setup.

<a id="system-architecture--77-optional-drivercar-data"></a>
##### 7.7 Optional driver/car data

A small persistent list of drivers and/or cars is a possible product feature, particularly for more competitors than available lanes. It is not currently considered a fundamental architectural issue and is not committed as a feature.

<a id="product-facilities-spec--5-track-setup"></a>
#### 5. Track Setup

Track Setup may optionally store information that normally changes rarely:

<a id="product-facilities-spec--51-track-length"></a>
##### 5.1 Track length

- metres or feet;
- Unknown / Not entered allowed.

<a id="product-facilities-spec--52-track-scale"></a>
##### 5.2 Track scale

Preset examples include:

- 1:32;
- 1:24;
- 1:43;
- 1:64;
- Custom;
- Unknown / Not entered.

From track length and timing P&P can calculate actual model average speed.

Where scale is known it can also calculate scale-equivalent speed.

Speed may be calculated per lap, for fastest lap, stage or whole race where meaningful.

Speed does not need to occupy the default live scoreboard. It may appear in results, analysis or event callouts.

<a id="product-facilities-spec--8-product-option-philosophy"></a>
#### 8. Product-option philosophy

P&P should be **simple by default, sophisticated when wanted**.

Software capability should not be artificially withheld merely to manufacture product tiers when exposing it costs essentially nothing.

Advanced or uncommon choices may be placed under **Advanced** so the ordinary MUG is not confronted with unnecessary complexity.

Hardware-dependent premium/expansion capabilities can remain visible but locked with a clear requirement explanation.


## Diagnostics and recovery

<a id="system-architecture--14-test-and-diagnostics"></a>
#### 14. Test and diagnostics

Testability is a permanent architectural responsibility.

Real wired detectors, wireless devices, future devices and simulated inputs should use the same standard P&P message contracts and common communications bus. Test tooling may publish permitted synthetic/test messages and subscribe to relevant outputs at the same architectural boundaries used by real components. **The injection point depends on what is under test:** an Input Device/Input Module test stimulates the source-facing side so filtering and event production are exercised; a downstream race/session test may impersonate the Input Module and publish an authorised standard `INPUT_EVENT` onto the P&P Message Bus. The latter deliberately bypasses physical sensing because that hardware boundary is not the subject of that test; it does not create a private production path around the P&P Message Bus.

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

<a id="system-architecture--15-failure-and-recovery"></a>
#### 15. Failure and recovery

Failure handling is based on the **function/capability lost**, not merely the component that failed.

A fault should propagate only as far as the functions that depend upon it. Loss of an optional sector detector, display or sound capability should not unnecessarily stop unaffected racing functions. Loss of a capability essential to the active session may require the session to pause, stop or otherwise be treated as degraded.

Registry reports availability; Race Control determines the consequence for the current session.

When a failed component returns, technical reconnection does not imply that information missed during its absence can be reconstructed. A returning component rejoins from authoritative current P&P state rather than blindly resuming stale pre-failure state.

Recovery engineering must remain proportionate to the product. P&P is not required to reconstruct a live race after every conceivable controller crash or power failure. Persistent configuration should survive and the system should restart cleanly; detailed live-race crash recovery is a later product decision and should only be implemented where its benefit justifies the complexity.

<a id="system-architecture--16-updates-and-compatibility"></a>
#### 16. Updates and compatibility

The P&P Controller is the customer-facing update authority.

Controller firmware should support customer-initiated OTA updating through the normal P&P interface when the customer chooses to provide Internet access. Ordinary racing must remain independent of Internet availability.

The controller update mechanism must be recoverable: installing a replacement must not deliberately destroy the only known-working firmware before the new firmware has been successfully installed and validated. The exact mechanism is an implementation decision.

Intelligent modules expose sufficient version and capability information for P&P to determine compatibility.

The architecture permits intelligent modules to be updated through the P&P controller where appropriate, without requiring the customer to connect each module to development tools or separately configure it for Internet access. **Remote updating is optional for a module**; simple or inexpensive modules are not required to implement OTA merely to qualify as P&P-compatible.

An incompatible or outdated optional component should be identified clearly without unnecessarily preventing compatible functions from operating.

<a id="wp6-recovery-reset-update-contract--wp6-recovery-reset-and-customer-firmware-update-contract"></a>
### WP6 Recovery, Reset and Customer Firmware Update Contract

**Status:** APPROVED PRODUCT CONTRACT — SIGNED-ENVELOPE IMPLEMENTED; PRODUCTION KEY PROVISIONED OUTSIDE THE REPOSITORY

**Purpose:** define the customer-safe recovery, reset, factory-reset and firmware-update
boundary for WP6 without changing race, authority or persistent-record semantics.

<a id="wp6-recovery-reset-update-contract--1-terms-and-fixed-behaviour"></a>
#### 1. Terms and fixed behaviour

<a id="wp6-recovery-reset-update-contract--restart"></a>
##### Restart

Restart is an orderly or externally requested controller reboot into the same
installed firmware. It may be triggered by an authorised support action, a
software health action, or an ordinary hardware reset/reboot boundary. Restart:

- preserves committed configuration, network credentials, remembered authority,
  History, Results/Details, PB, Track Record and Record Era;
- does not restore the volatile state of an interrupted individual race;
- reconstructs the last-known-good persistent state and returns to the normal
  startup/readiness flow;
- treats any incomplete transaction as uncommitted debris, never as official
  History.

These rules follow [System lifecycle](#system-lifecycle--pp-system-lifecycle-and-persistence) and the accepted Persistent Storage
Foundation design.

<a id="wp6-recovery-reset-update-contract--reset-settings"></a>
##### Reset settings

Reset settings restores installation, configuration and settings to factory
defaults while retaining committed race History/Results/Details, PB, Track
Record and Record Era. It requires controlling authority and explicit
confirmation. It is not a reboot and it is not a full customer-data wipe.

<a id="wp6-recovery-reset-update-contract--erase-everything"></a>
##### Erase everything

Erase everything is the separate, genuinely destructive action intended for
resale or a completely fresh start. It erases configuration/settings and all
customer race/record data. It requires controlling authority and substantially
stronger explicit confirmation, then returns to the safe first-boot/
commissioning state. “Factory Reset” must not be used as an ambiguous label for
both actions.

<a id="wp6-recovery-reset-update-contract--2-customer-firmware-update"></a>
#### 2. Customer firmware update

The controller is the customer-facing update authority. Firmware update is a
Browser-only function; the Taster is deliberately a MINIMUG-style race
interface, not an alternative P&P administration, maintenance, Settings or
diagnostics interface. A customer-initiated update must be available through
the normal P&P Browser without development tools. Both supported sources use
the same update boundary:

```text
authorised request
  -> receive/stage candidate
  -> validate image identity, integrity, compatibility and version policy
  -> mark candidate pending
  -> install through recoverable boot/update mechanism
  -> reboot into candidate
  -> perform boot/health confirmation
  -> commit candidate as active, or roll back to known-good
```

The replacement must never destroy the only bootable known-good firmware before
the candidate is validated. The Browser supports both normal online P&P update
and local firmware-file upload for offline/support/recovery use. Both paths
must authenticate and validate legitimate, compatible P&P firmware. Ordinary
customer operation rejects downgrades; a separately controlled support/recovery
mechanism may permit an intentional downgrade when genuinely required. Normal
racing remains independent of Internet availability.

<a id="wp6-recovery-reset-update-contract--signed-pp-update-envelope"></a>
##### Signed P&P update envelope

Every local or online candidate is a versioned `PPU1` package containing a
canonical manifest followed by the exact firmware image. The manifest binds the
P&P product identity, ESP32-8048S070 target, firmware version/build identity,
declared image size and SHA-256 digest. An asymmetric signature covers that
manifest. The controller verifies the envelope structure, manifest fields,
payload size/digest and signature before `CandidateReady`; only then may the
candidate be marked authenticated and passed to the existing compatibility and
downgrade policy. `esp_ota_end()` remains the ESP-IDF image-format/integrity
boundary, while the envelope is the product-authenticity boundary.

Production firmware contains only a separately supplied verification public key
(`PP_UPDATE_PUBLIC_KEY_HEX`). A missing production key is a build-stopping
configuration error. The repository has no production private key. A dedicated
non-production Wokwi/acceptance build may use an explicitly marked test public
key; its private signing material is generated ephemerally by test tooling and
is never committed, embedded or shipped.

<a id="wp6-recovery-reset-update-contract--3-failure-and-recovery-contract"></a>
#### 3. Failure and recovery contract

The following behaviours are fixed:

- invalid, incompatible, truncated or corrupt images are rejected before
  installation;
- insufficient staging space is an explicit bounded failure and leaves the
  active firmware/data unchanged;
- power loss during upload or before installation leaves the active firmware
  and committed customer data unchanged;
- power loss during installation must boot the previous known-good image or a
  bootloader recovery path;
- a candidate that fails to boot or fails post-boot health confirmation is not
  committed as active; automatic rollback to the previous known-good firmware
  is attempted first;
- repeated failed attempts are bounded and retain a bootable known-good path;
- recovery/rollback must not fabricate, delete or rewrite committed race data.

If automatic rollback cannot restore normal operation, P&P enters a
customer-visible recovery/support mode. A failed update reports that the
previous version was restored whenever rollback succeeds.

<a id="wp6-recovery-reset-update-contract--4-authority-and-safety"></a>
#### 4. Authority and safety

- Read-only status may be viewed by any connected Browser.
- Restart, Reset settings, Erase everything and firmware update require the
  controlling Race Director/Master or an explicitly authorised support path.
- Reset settings, Erase everything and firmware update require controlling
  authority, explicit confirmation and a settled non-racing lifecycle. They are prohibited during
  `STARTING`, `RACING`, `PAUSED`, `RESTARTING` and unfinished settlement.
- Browser/UI code requests operations; it does not own reboot, update, storage,
  rollback or authority state.
- A failed update must not change Race Director ownership or make a Browser
  authoritative.

<a id="wp6-recovery-reset-update-contract--5-data-retention-matrix"></a>
#### 5. Data-retention matrix

| Data | Restart P&P | Reset settings | Firmware update | Failed/interrupted update | Recovery/rollback | Erase everything |
|---|---|---|---|---|---|---|
| Installed known-good firmware | retained | retained | replaced only after validation | retained | restored/retained | retained until the erase completes |
| Network credentials | retained | factory defaults | retained | retained | retained | erased |
| Remembered setup/configuration | retained | factory defaults | retained | retained | retained | erased |
| Committed History/Results/Details | retained | retained | retained | retained | retained | erased |
| PB / Track Record / Record Era | retained | retained | retained | retained | retained | erased |
| Active individual race | not restored after uncontrolled interruption | same | same | same | same | not applicable |
| Incomplete temporary transactions | ignored/reclaimed under PSF rules | same | same | same | same | not applicable |

<a id="wp6-recovery-reset-update-contract--6-fixed-first-bootrecovery-outcome"></a>
#### 6. Fixed first-boot/recovery outcome

After a successful restart, update commit or rollback, P&P loads last-known-good
configuration, reconstructs committed storage, reconciles equipment and returns
to normal readiness. Reset settings uses the same first-boot/default-settings
path while retaining customer records. Erase everything enters the existing
factory-default commissioning path after the confirmed wipe. An interrupted
individual race is not resumed automatically; an unfinished competition may be
offered for continuation under [System lifecycle](#system-lifecycle--pp-system-lifecycle-and-persistence).

No product owner decisions remain open in this contract. Implementation must
still define measurable image-authentication, compatibility and post-boot
health checks within these boundaries and submit those details for technical
review; they are not permission to change the product decisions above.

<a id="stage-15-logging-diagnostics-foundation-design--stage-15-logging-and-diagnostics-foundation-design"></a>
### Logging and diagnostics design

**Status:** Foundation Design ACCEPTED/FROZEN for implementation. Final Stage 15 acceptance/freeze is not granted.

<a id="stage-15-logging-diagnostics-foundation-design--1-scope"></a>
#### 1. Scope

This foundation records observations about Browser actions, P&P requests and decisions, authoritative state changes, notifications, rendering, storage, and system health. It does not become product state, race authority, timing authority, classification authority, Browser configuration authority, or product persistence.

The implementation must preserve all Stage 1–14C semantics and the preserved Stage 14C candidate.

<a id="stage-15-logging-diagnostics-foundation-design--1a-diagnostic-privacy-and-access-contract"></a>
#### 1A. Diagnostic privacy and access contract

Diagnostic recording is for technical and product diagnosis only. P&P may
record structured technical information needed to diagnose behaviour,
including controller/build identity, lifecycle and race state, race
configuration, lane information, timings, requests/results, correlation and
trace identities, detector/input events, persistence/storage events,
resource/health information, Browser/Taster observations, errors, drops and
diagnostic health.

Racer/MUG names or identifiers may appear when they are genuinely part of the
product state being diagnosed. They must not be duplicated merely for
logging. Diagnostics must never deliberately record Wi-Fi passwords,
passwords, secrets, authentication tokens, private/signing keys,
cookies/session credentials, complete HTTP headers for convenience, arbitrary
Browser form contents, or unrelated user data. Free-form or unknown external
data is not copied automatically; records use defined structured fields.

Normal diagnostics may run automatically during ordinary P&P operation.
Development diagnostics are an explicit troubleshooting facility under the
advanced diagnostic facilities and require the same P&P
Master/Race-Director/controlling-SMUG control lease used by other advanced
controls. Starting and stopping Development diagnostic sessions require that
same lease. Downloading a diagnostic artifact is an observation-only action
available to any connected Browser with normal P&P access; it does not grant
control and must not offer TAKE CONTROL merely to download. Status and
diagnostic viewing are likewise observation-only unless a narrower rule is
explicitly required by the product UI. No separate SMUG capability, account,
password, PIN or authentication system is introduced. Diagnostic access never
grants Race Control or Race Engine authority, and diagnostic, access or export
failure must not alter product/race operation.

No elaborate account/login system is introduced solely for Stage 15. The
existing/frozen P&P configuration and access model must enforce the
Development boundary at the request boundary. The production implementation contains the controlling-lease check for Development start/stop, and status/retrieval/download remain observation-only. These checks are request-boundary rules, not permissions inferred from Browser presentation.

<a id="stage-15-logging-diagnostics-foundation-design--1b-wp3a-product-support-contract"></a>
#### 1B. WP3A product support contract

WP3A freezes the customer-facing diagnostic contract and design direction. It
does not claim that the production recorder, export route or final Browser UI
is implemented. WP3B is the subsequent implementation package.

<a id="stage-15-logging-diagnostics-foundation-design--customer-workflow"></a>
##### Customer workflow

The supported workflow is:

`Settings → Advanced → Diagnostics → DOWNLOAD DIAGNOSTICS → send one item to support`.

The customer receives one P&P-generated artifact. They do not select raw logs,
sessions, cursors, sinks, files or endpoints. A download may be requested while
recording is active. P&P owns the snapshot boundary and recording continues
after the snapshot. The Browser displays the authoritative export capability
and result; it does not implement a snapshot or recovery state machine.

<a id="stage-15-logging-diagnostics-foundation-design--artifact-representation-and-contents"></a>
##### Artifact representation and contents

The WP3A support artifact is a versioned UTF-8 newline-delimited JSON file
(`.ndjson`), with one self-identifying header record, structured diagnostic
records in deterministic sequence order, and a summary/footer record. The
header identifies the product, firmware/build, controller epoch, export
schema, snapshot boundary, storage-health and capability state. Records retain
the structured envelope, timestamps, client/session identity where applicable,
correlations, event kind and allow-listed payload. The summary reports record
count, drops and whether evidence is incomplete, volatile or degraded.

The artifact must contain enough retained Normal material and Development
material up to the snapshot to reconstruct chronology, reboot/session
boundaries, health/storage faults, controller/network context, race/session
state, requests/results and input context. It must remain inspectable and
deterministic for tests. The current `/diag/records` hexadecimal response is a
development inspection view, not this support artifact. Compression, byte
limits and final SD placement remain implementation/physical-validation
details and are not frozen here.

<a id="stage-15-logging-diagnostics-foundation-design--recording-scope"></a>
##### Recording scope

Normal diagnostics are always-on, quiet, bounded and rolling. They capture the
allow-listed Stage 15 taxonomy needed to explain ordinary customer faults:
boot/controller/build identity, session/reconstruction boundaries, material
request acceptance/rejection, authoritative lifecycle/state changes,
persistence and storage-health transitions, resource-health faults/recovery,
and relevant notification/transport-health transitions. They do not record
unchanged loops, raw traffic or arbitrary form data. Detailed detector/input,
Browser request/result, notification, collection and render observations are
Development-level evidence, with Normal retaining only material fault or
transition observations needed for support.

Development diagnostics include Normal scope plus the deeper structured
request, input/detector, Message Bus/Race decision, Browser notification,
authoritative collection, render, persistence, resource, fault, drop and
reconstruction observations already defined by the Stage 15 taxonomy. Both
scopes use an allow-list; neither copies raw headers, secrets, cookies,
arbitrary form contents or unrelated personal data.

<a id="stage-15-logging-diagnostics-foundation-design--retention-storage-health-and-failure"></a>
##### Retention, storage health and failure

Normal retention is a bounded rolling window; the oldest diagnostic evidence
is discarded when its bound is reached, without outranking authoritative
History/Results data. Development retention is bounded and reports explicit
drops/loss. Temporary export material is bounded, reclaimable and leaves no
uncontrolled debris. Writes must not occur per loop or retry unchanged status
indefinitely; exact capacities, write timing, SD throughput and wear tuning
remain provisional until WP4 hardware evidence.

Export capability is P&P-owned and reports one of `AVAILABLE`, `LIMITED` or
`UNAVAILABLE`. `LIMITED` means a useful artifact can be produced from retained
RAM/current status/build/health despite degraded or missing durable storage;
`UNAVAILABLE` means no useful artifact can be produced. SD unavailability does
not disable diagnostics or racing, and diagnostic failures never alter product
authority. Existing storage-health state is observed; no second diagnostic
recovery state machine is introduced.

<a id="stage-15-logging-diagnostics-foundation-design--browser-hierarchy"></a>
##### Browser hierarchy

The final Browser hierarchy is Settings → Advanced → Diagnostics. Availability
and DOWNLOAD DIAGNOSTICS are primary. Firmware/build, System Health,
Diagnostic Storage and Normal Diagnostics state are secondary. TECHNICAL
DETAILS and DEVELOPMENT DIAGNOSTICS are tertiary. Raw logs, files, cursors,
JSON, API endpoints, addresses, Message Bus/SD paths, raw HTTP, live console
and CLEAR DIAGNOSTICS are not customer controls. Normal diagnostics has no
customer Start/Stop. Development START/STOP is offered to the controlling
SMUG. Any connected Browser may request the observation-only download.

The same Advanced/Diagnostics area contains a **Network** section. Any
connected Browser may view sanitized AP/STA status and request a Wi-Fi scan.
Selecting an SSID is local presentation state. For a remembered but
disconnected network, **RECONNECT** is an authoritative controlling-SMUG
operation that uses the driver's existing configuration without accepting an
SSID/password payload. **RECONFIGURE** is the separate credential-writing
operation. Reconnect, Disconnect, Reconfigure and Forget Network require the
controlling SMUG/Race Director lease; no separate diagnostic or network
authority exists. Network credentials are never returned, displayed after
submission, logged, diagnosed, exported, or placed in History/Results.

<a id="stage-15-logging-diagnostics-foundation-design--wp3a-status-boundary"></a>
##### WP3A status boundary

This section records the **WP3A Foundation/Product Design Freeze**. WP3B–WP3E
software implementation is complete for review, including the production
recorder integration, Browser workflow, export route/encoding and support
artifact. This does not grant final Stage 15 acceptance. Final acceptance also
requires the resource, policy, real ESP32-S3/SD, recovery, housekeeping and
render-evidence gates in Section 7.

<a id="stage-15-logging-diagnostics-foundation-design--development-duration-and-reboot-volatility"></a>
##### Development duration and reboot volatility

Development records already retained or durably written are not deleted merely
because the mode expires and remain available under the normal bounded
retention policy.

The enabled/active Development-logging mode is volatile and is not persisted.
A reboot, reset or power loss ends Development logging. P&P always starts or
restarts in Normal diagnostics only. There is no automatic resume and no
resume prompt. Previously recorded diagnostic material remains available
subject to bounded retention. If further Development detail is required, the
controlling SMUG deliberately starts a new Development session. This
volatility applies to the mode/enabled state, not to already-recorded evidence.

<a id="stage-15-logging-diagnostics-foundation-design--2-session-and-storage-health-state"></a>
#### 2. Session and storage-health state

The diagnostic session lifecycle is exactly:

`DISABLED`, `STARTING`, `ACTIVE`, `STOPPING`, `COMPLETED`, `INTERRUPTED`.

Storage health is a separate dimension. The foundation contract uses:

`DURABLE_SD`, `RAM_ONLY`, `DEGRADED`, `FAULT`.

`DEGRADED_RAM_ONLY` is not a lifecycle state. An `ACTIVE` session may be `RAM_ONLY` or `DEGRADED`.

Starting without durable storage is permitted. The session starts in clearly identified RAM-only/degraded operation with an explicit warning that evidence is volatile, may be incomplete, and is lost on controller restart unless committed elsewhere.

<a id="stage-15-logging-diagnostics-foundation-design--3-ownership"></a>
#### 3. Ownership

There is one active Development diagnostic session per P&P controller. A request to start another session returns `ALREADY_ACTIVE` together with the existing session status and identity. It never creates, replaces, merges, or silently joins a session. Access to these session controls is checked against the authoritative single Master/Race Director/controlling-SMUG lease; Browser presentation flags are not an authorization source.

Other Browser clients may observe or contribute to the existing session. Each client has an independent client identity recorded in its diagnostic events.

<a id="stage-15-logging-diagnostics-foundation-design--4-structured-records"></a>
#### 4. Structured records

Every record has a versioned envelope containing controller epoch, session identity, client identity where applicable, sequence, diagnostic timestamp, event kind, severity, correlation fields, payload type, and structured payload. For durable diagnostic storage, sequence is monotonically increasing and unique within the retained durable diagnostic stream/storage generation, including across controller restarts; `controllerEpoch` remains separate provenance identifying the boot that produced a record. A volatile RAM-only stream may begin a new sequence after reboot because its prior stream no longer exists. A failed or ambiguous append consumes its reserved sequence; a resulting gap is reported as incomplete evidence rather than reusing the candidate.

The correlation chain is:

`USER ACTION → Browser request → interface request → Message Bus → P&P decision → authoritative change → notification → Browser collection → render observation`.

Message Bus facts and Noticeboard/SSE notifications remain separate from diagnostic records. Missed diagnostic events must not affect product behavior.

<a id="stage-15-logging-diagnostics-foundation-design--5-recorder-and-backend-boundary"></a>
#### 5. Recorder and backend boundary

The `DiagnosticRecorder` owns session lifecycle, record sequencing, attribution, status, and fault reporting. The `DiagnosticStorage` boundary abstracts RAM, host/file-backed development storage, Wokwi-compatible development storage, failure-injection storage, and the future ESP32-S3 SD/FATFS backend.

The Phase 1 interfaces do not freeze capacities, queue sizes, file sizes, export encoding, SD layout, or performance thresholds.

Recorder operations must be bounded and non-blocking from timing-critical product code. A diagnostic failure may drop or mark evidence, but must not block or alter racing, timing, classification, persistence authority, or Browser authority.

<a id="stage-15-logging-diagnostics-foundation-design--6-restart-and-recovery"></a>
#### 6. Restart and recovery

Each controller boot creates a new controller epoch. Durable diagnostic evidence follows the selected backend recovery rules. A RAM-only active session cannot be reconstructed after restart and is reported as interrupted/lost rather than completed.

Browser reload/reconnect reconstructs diagnostic status from P&P authority. No active-race recovery is introduced.

<a id="stage-15-logging-diagnostics-foundation-design--7-final-acceptance-boundary"></a>
#### 7. Final acceptance boundary

Foundation Design Freeze and the WP3A product design freeze permit
implementation. Final Stage 15 acceptance/freeze additionally requires
resource measurement, production ESP32-S3 plus SD/FATFS evidence,
permission/security, privacy/redaction, final Browser UX, export encoding, SD
housekeeping/wear policy, exact render-evidence rules, and
implementation-informed storage-health recovery details. WP3A itself is not
complete merely because this design is frozen.

<a id="stage-15-logging-diagnostics-foundation-design--8-phase-2-development-implementation"></a>
#### 8. Phase 2 development implementation

Phase 2 provides an in-memory recorder and backend solely to exercise the contract. It uses an explicitly named provisional test bound and retains records in insertion order. When the bound is reached, new records are dropped, loss is counted, and storage health becomes `DEGRADED`; records are not silently overwritten. Backend failures are latched as `FAULT`, subsequent submissions do not retry the failed backend operation, and the diagnostic session remains separate from product authority.

The Phase 2 bound is a test/development implementation detail. It is not a product retention capacity, SD capacity, or final performance threshold.

<a id="stage-15-logging-diagnostics-foundation-design--9-phase-3-development-storage"></a>
#### 9. Phase 3 development storage

Phase 3 adds a host/file-backed development backend and a deterministic failure-injection wrapper. The file representation is explicitly development/test-only, versioned, checksummed, session-scoped, and capable of recognizing finalized, unfinalized, corrupt, and unsupported-version data. It is not the production SD layout or final export encoding.

The in-memory backend remains the firmware-compatible development backend for Wokwi tests. No Wokwi firmware access to host files is required, and no separate Wokwi storage architecture is introduced in Phase 3.

<a id="stage-15-logging-diagnostics-foundation-design--10-phase-4-controller-side-operation-trace"></a>
#### 10. Phase 4 controller-side operation trace

Phase 4 adds one observation-only controller trace spine for an existing named
operation, initially `START`. The development/demo controller observes the
existing `StartRequest`, `RequestResult`, authoritative state transition, and
generic `NoticeboardChanged` messages through a separate Diagnostics bus
observer. It records the operation name, existing request correlation, event
ordering, and the observed authoritative lifecycle snapshot in the existing
bounded in-memory recorder.

There is no new product request path and no new product message contract. The
current `RequestResult` is the available Race Control decision/result boundary;
there is no separate downstream decision message to correlate. A generic
`NoticeboardChanged` has no request correlation, so the trace associates it with
the pending accepted START observation only and records that limitation rather
than changing the frozen Noticeboard contract.

Accepted and rejected START requests are both observable. A rejected request
must not produce a diagnostic state-transition record. Recorder/backend failure
may lose or mark diagnostic evidence but cannot affect request acceptance,
authoritative state, timing, or Noticeboard publication. Phase 4 does not add
Browser click/render tracing, detector/lap tracing, persistence tracing, SD,
SSE transport tracing, or final diagnostic UX.

<a id="stage-15-logging-diagnostics-foundation-design--11-phase-5-browserhttp-request-trace"></a>
#### 11. Phase 5 Browser/HTTP request trace

Phase 5 extends the observation spine to the Browser request boundary for the
development/diagnostic Browser path. A Browser action observation and the
corresponding HTTP/interface receipt carry the server-attributed Browser
client identity, the Browser-visible product correlation, and a separate
diagnostic trace identity. The existing internal P&P correlation remains
unchanged and is recorded separately, so diagnostic metadata cannot become a
permission, sporting, timing, configuration, or result authority.

The trace distinguishes an observed Browser action from a request received at
the HTTP boundary and from the existing Message Bus request/result records.
An action record without a request record means that the request was not
observed at the controller boundary; a request without a later bus/result
record remains an explicit transport/interface gap. Malformed or absent
optional diagnostic metadata is ignored by product request parsing and never
changes request acceptance.

The Browser-side observation is fire-and-forget and is never a prerequisite
for dispatching the real product request. It does not retry, suppress, or
duplicate a product request. Multiple Browser clients contribute to the one
active diagnostic session using their server-attributed client identities.
The bounded recorder may drop diagnostic records or latch a fault without
changing product operation behaviour.

Phase 5 does not define final Browser diagnostic UX, security/permission or
privacy policy, export encoding, storage capacities, SD layout, SSE/state
tracing, or render-evidence semantics.

<a id="stage-15-logging-diagnostics-foundation-design--12-phase-6-authoritative-state-to-browser-render-trace"></a>
#### 12. Phase 6 authoritative state to Browser render trace

Phase 6 observes the existing presentation path only:

`authoritative P&P change → NoticeboardChanged → existing notification → Browser receipt → authoritative /state collection → state application → render evidence`.

The generic NoticeboardChanged contract remains unchanged and carries no new
product correlation. Diagnostic records may associate a notification with a
trace only where the existing metadata makes that association legitimate; a
generic notification without proven causation is recorded as such. Browser
notification receipt is attributed to the server-assigned Browser client and
preserves any available diagnostic trace, visible correlation, revision and
event identifiers separately.

State-fetch start/receipt, state application and render evidence are separate
observation records. They carry compact lifecycle/session information and the
existing P&P revision/epoch identifiers where available; they do not duplicate
full State or invent a Browser-owned revision. Render evidence is a narrow
observation of the existing DOM/presentation update, not a screenshot system,
renderer duplicate or authority source. Diagnostic submission is fire-and-
forget and must never be required for notification handling, state collection,
state application or rendering.

Initial connection, reload, reconnect and controller-epoch reconstruction are
marked as reconstruction observations. They do not manufacture a START trace.
Multiple Browser clients remain independently attributable while observing the
same authoritative state. The trace must make it possible to distinguish a
missing notification, incomplete state acquisition, state received but not
applied, applied but not rendered, and the complete chain. Phase 6 does not
add diagnostic state delivery, SSE/polling, a Browser product state machine,
general screen instrumentation, detector/lap tracing or persistence tracing.

<a id="stage-15-logging-diagnostics-foundation-design--13-phase-7-resource-retrieval-and-robustness-foundation"></a>
#### 13. Phase 7 resource, retrieval and robustness foundation

Phase 7 measures the bounded development implementations and exposes the
existing diagnostic session through observation-only status and cursor-based
retrieval. `GET /diag/status` reports session identity/lifecycle, storage
health/backend/failure, retained record and payload-byte occupancy, dropped
counts, and volatility or incompleteness. `GET /diag/records?cursor=N` returns one current record at a
time with its envelope provenance and a provisional hexadecimal payload view.
This transport representation is development inspection only; it does not
freeze the final export encoding, permission model, privacy policy, SD layout,
or product retention capacity. It is not the WP3A support artifact; the
customer download path uses the versioned NDJSON contract in Section 1B.

Retrieval is a bounded moving view. Each request reads the current retained
count at its cursor; records appended after a read are visible from a later
cursor, and reads do not clear or mutate evidence. End-of-view, read failure,
and incomplete evidence are explicit in the response. The implementation uses
bounded per-request buffers and no new diagnostic task, queue, or SD I/O path.
Status/retrieval failures remain observation-only and cannot affect product
authority or timing.

Phase 7's host/file and in-memory limits remain provisional development/test
values. The resource campaign records measured sizes, latency and loss
behavior for later capacity and performance decisions. Production ESP32-S3 SD
storage and final recovery/housekeeping remain outside this phase.

<a id="stage-15-logging-diagnostics-foundation-design--14-phase-7-measured-baseline"></a>
#### 14. Phase 7 measured baseline

[Historic Phase 7 measurements](https://github.com/ConnalM/Plonk-and-Play/blob/6d25c9924a4da090a316aaaf70fe1a1f6d5c1a58/docs/PP_DESIGN_SPEC.md#stage-15-logging-diagnostics-foundation-design--14-phase-7-measured-baseline) preserve envelope/chunk sizes, bounded development payloads and ESP32 demo/acceptance resource figures. They are not production capacities or acceptance thresholds. Current resource limits and qualification are tracked in the Master Project Plan and acceptance evidence.

<a id="stage-15-logging-diagnostics-foundation-design--15-phase-8-hardware-baseline"></a>
#### 15. Phase 8 hardware baseline

The Phase 8 real-hardware checkpoint targets the established ESP32-8048S070
7-inch 800x480 ESP32-S3 display board. The board record identifies 16 MB flash,
on-board PSRAM, and an on-board microSD socket. The checkpoint uses the
repository's `esp32-8048s070` PlatformIO board definition, derived from the
previously used `esp32-s3-devkitc-1` target and corrected for the N16R8
16 MB/PSRAM hardware. It does not claim that display peripherals are part of
the diagnostic firmware.

The board microSD is exercised in SPI mode with the verified signal mapping:
CS GPIO10, MOSI GPIO11, SCK GPIO12, and MISO GPIO13. No card-detect,
write-protect, display, touch, or other electrical details are assumed. Runtime
firmware must report the detected card, filesystem and capacity rather than
assuming them.

This is a Phase 8 hardware-test definition only. It does not freeze the
production diagnostic directory layout, export encoding, capacity, wear policy,
or final SD integration architecture.


## Physical product

<a id="product-facilities-spec--plonk--play--product-facilities-specification"></a>
### Base product hardware and facilities

**Status:** Committed product-design specification  
**Scope:** Cross-mode facilities and product decisions agreed during Lap Counter product design.

<a id="product-facilities-spec--1-base-product-hardware-direction"></a>
#### 1. Base product hardware direction

The base product has two standard optical timing sensors.

The same sensors are repurposed according to mode rather than being permanently tied to one race form.

The System Controller is authoritative for timing, race logic and state. Browser clients are presentation/control clients and must not become timing authorities.

<a id="product-facilities-spec--10-deferred-decisions"></a>
#### 10. Deferred decisions

All outstanding facility design decisions and optional scope are tracked in the [Master Project Plan](MASTER_PROJECT_PLAN.md). No speculative feature is promoted to v1 by migration.

<a id="system-architecture--3-devices-capabilities-and-physical-mounting"></a>
#### 3. Devices, capabilities and physical mounting

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

