# Plonk & Play™ System Requirements

**Status:** Draft v0.1  
**Purpose:** Define what the P&P platform must be capable of supporting before detailed architecture or implementation begins.

These are system requirements, not a commitment to implement every capability in the first product.

## Guiding scope

P&P should **design broadly and implement narrowly**.

The architecture must not make unnecessary assumptions that close off sensible future capabilities. This does not mean speculative features must be built before they are needed.

The first product may be a simple two-lane lap counter, but the underlying platform must be capable of growing without repeatedly redesigning its foundations.

## Core requirements

### 1. Universal analogue operation

- Work with normal analogue slot-car systems without depending on a particular track manufacturer, scale or proprietary control system.
- Avoid requiring permanent track modification for basic operation.

### 2. Lanes and competitors

- Support two lanes in the initial product.
- Do not architect the platform around an assumption that there can only ever be two lanes.
- Allow expansion to additional lanes without redesigning the Race Engine.

### 3. Timing and detection

- Provide lap timing to 1/100th of a second for the base system.
- Support multiple independent detection points.
- Detection points may include Start/Finish, sectors, speed traps, pit entry/exit, drag staging and other future purposes.
- Sensor technology must be replaceable without requiring changes to race logic.
- Events originating from different modules must be expressible in a common P&P time domain where their timing must be compared.
- Communication latency must not determine official race timing.

### 4. Race and session modes

The platform must be capable of supporting:

- Lap Race
- Practice
- Endurance
- Timed Stage
- Drag

Modes may have substantially different concepts of starts, finishes, competitors and timing. The architecture must not assume that every session is a conventional simultaneous-start circuit race.

### 5. Race simulation

- Support optional simulated fuel management, including fuel consumption, fuel level, pit/refuelling detection and configurable refuelling behaviour.
- Permit future optional race simulation variables such as tyres, track/weather conditions and similar effects.
- Fuel is a planned capability; tyres, weather and similar variables are not currently committed features.
- The architecture must not assume that fuel is the only race variable that may affect a competitor.

### 6. Browser-based operation

- Provide control and presentation through an ordinary web browser.
- Require no dedicated PC application for normal use.
- Support phones, tablets and computers as control/display devices.
- Permit multiple simultaneous browser/display clients.
- The controller, not the browser, owns official race state and timing.
- A race must continue if a browser sleeps, closes or loses its connection.
- A reconnecting client must be able to obtain the current authoritative state.

### 7. Offline operation

- Normal racing must not require Internet access.
- Local networking may be used.
- Internet access may provide optional services such as software updates, but loss of Internet access must not prevent normal race operation.

### 8. Modular expansion

The platform must be capable of supporting P&P modules including, but not limited to:

- Additional timing sensors
- Sector and speed-trap sensors
- Pit entry/exit modules
- Start lights / gantries
- False-start detection
- Track-power control
- Physical start, stop and track-call controls
- Trackside displays
- Sound / announcements
- Drag-racing equipment
- Higher-performance timing sensors
- Future supported modules not yet conceived

Adding an optional capability must not require unrelated parts of the system to be redesigned.

### 9. Replaceable and failure-isolated modules

- Modules must have defined interfaces/contracts.
- A module should be independently replaceable, reimplemented or upgraded where practical without requiring unrelated system components to change, provided its contract is preserved.
- Failure of a non-essential module should not unnecessarily stop unrelated functions.
- For example, loss of a sector sensor should not inherently stop Start/Finish lap timing, and loss of a display should not stop a race.
- Internal modularity must provide practical isolation, not merely divide source code into files.

### 10. Wired and wireless communication

- Support wired modules for simple, reliable base installations.
- Support wireless P&P modules as a first-class system capability.
- Wired and wireless versions of the same logical function must be capable of presenting the same standard interface to the rest of the system.
- Transport mechanism must not define module identity or race behaviour.
- Intelligent devices must have stable, unambiguous P&P identity independent of temporary addresses or transport. Simple non-identifying devices may instead be identified by the stable controller capability/connection through which P&P sees them. Human-readable labels are not machine identity.

### 11. Discovery and configuration

- Supported P&P equipment should identify itself and its capabilities automatically wherever practical.
- The system should discover newly connected supported modules with minimal customer configuration.
- Ask the customer only for information that cannot reasonably be discovered, such as the physical role or location of otherwise identical sensors.
- Remember known modules, assignments and configuration across power cycles.
- Reconnect known modules automatically where practical.
- Race-affecting configuration need not be changeable while an individual race is active; the initial product may defer such changes until the race has ended.
- Before START, the user changes a proposed Race Setup. When START is accepted, P&P creates a fixed Session Definition for the race; the user does not directly edit that Session Definition.
- The Session Definition must freeze the session-specific roles of the input, output and other capabilities required for that session. A hardware module must not acquire race-mode, lane or MUG knowledge merely to operate its devices.
- Normal setup must not require customers to enter IP addresses, MAC addresses, edit configuration files or manually flash devices.

### 12. Common system time

- The System Controller is the authority for the P&P system time domain.
- P&P System Time defines common instants; distributed devices do not have to maintain numerically identical local clock readings.
- Intelligent distributed modules may use local high-resolution clocks but, where information must be compared across devices, their timestamps must be reliably relatable or convertible to P&P System Time with sufficient accuracy for the function.
- Events should be timestamped at or close to detection where practical.
- The design must permit measurement/correction of clock offset and drift where required.
- Straightforward clock synchronisation is preferred where practical; a known and maintained clock relationship is also acceptable. Communication latency must not become the timing reference.
- The exact synchronisation algorithm is an implementation decision, not a requirement at this stage.

### 13. Persistent state and working configuration

- Configuration, module identities, module assignments and relevant user preferences must survive power loss and restart.
- Persistent configuration must be loaded into suitable working RAM/state for normal operation; timing-critical input, output and race processing must not depend on repeated persistent-storage lookups for configuration already available to the running system.
- Remembered/default device-role assignments are configuration data. The roles actually used by an accepted session are frozen into its Session Definition; Input and Output Modules remain responsible for device operation rather than race meaning.
- Local use of a module's working configuration/state is not inter-component communication and does not require a message-bus transaction.
- Persistence technology is not specified at requirements stage.

### 14. Test and diagnostics

- Testability must be part of the production architecture.
- Important race functions must be testable without requiring physical cars and sensors.
- Simulated inputs must use the same interfaces as real inputs.
- Support manual generation of test events.
- Support scripted race/session scenarios.
- Permit useful fault simulation, including delayed or missing events/modules, duplicate events, clock drift, out-of-order events and communication loss.
- The first implementation need only build the test and fault-injection facilities required for the capabilities it actually implements. Future transport- or module-specific fault cases are added when those capabilities are implemented; this requirement does not mandate a complete future-system simulator in v1.
- Customer-facing diagnostics may reuse the same underlying test facilities.
- Test infrastructure must not become a separate alternative Race Engine or bypass the normal production interfaces.

### 15. Updates and recovery

- The controller must be capable of software/firmware updates without development tools or manual reprogramming.
- Intelligent P&P modules must be capable of an appropriate update mechanism where required.
- A capability must not be declared available for a function unless P&P has established that it is sufficiently compatible and operational to provide that function correctly.
- Compatibility and recovery from interrupted or failed updates must be considered in the architecture.
- Updating one component should not unnecessarily require replacement or reprogramming of unrelated components.

### 16. Outputs and accessories

The platform must permit the authoritative race/session responsibilities to cause physical or presentation actions including, where appropriate:

- Start lights
- Track power enable/disable
- False-start response
- Race-finish response
- Track calls
- Pit/refuelling behaviour
- Displays
- Sound

Race Control coordinates operational logical actions through the common P&P communications architecture. For an active session, the appropriate race/session responsibility resolves semantic output roles through the fixed Session Definition and addresses a stable output capability; the Output Module/Output Device owns the hardware-specific implementation and does not need to know the racing role being served. The Race Engine reports competition facts, state and completion conditions through its defined boundary and does not directly control physical hardware. Neither Race Control nor Race Engine needs to know GPIOs, addresses or transport-specific implementation details.

## Explicitly not fixed by this document

This requirements document does **not** yet specify:

- Exact ESP32/controller model
- Connector family or pinout
- Exact sensor model
- Number or arrangement of physical sockets
- Wireless protocol details
- Clock-synchronisation algorithm
- Persistent-storage technology
- Web framework
- Packet formats
- C++ classes
- Source-file structure
- Detailed UI design

Those decisions belong to later architecture and implementation work.

## Working principle

> **Design broadly. Implement narrowly.**

Build only what is needed, but avoid unnecessary design decisions that make sensible future expansion difficult or require the foundations to be repeatedly rewritten.
