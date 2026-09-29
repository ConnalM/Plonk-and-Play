# P&P System Lifecycle and Persistence

**Status:** Draft v0.1  
**Parent architecture:** SYSTEM_ARCHITECTURE.md v1.0

## 1. Purpose

This document defines the conceptual lifecycle of a P&P controller from power-on through normal operation and shutdown, including persistent configuration, equipment reconciliation, device synchronisation, first-time defaults and recovery after uncontrolled shutdown.

It does not prescribe storage technology, hardware interfaces, clock-synchronisation algorithms or implementation classes.

## 2. Overall lifecycle

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

## 3. Core initialisation

Before equipment discovery, the controller must establish the essential P&P environment required to perform discovery and reconciliation.

Conceptually this includes:
- controller/core startup;
- essential supporting services;
- access to persistent storage;
- establishment of P&P System Time;
- communications required for equipment discovery.

The exact boot sequence and implementation mechanisms are later decisions.

## 4. Persistent state loaded into working RAM at startup

P&P must not rediscover the installation from a blank sheet at every power-on.

Memory owns the persistent copy of information that must survive power loss. At startup P&P loads the last-known-good information required for operation into working RAM, including as applicable:
- Registry information;
- installation configuration;
- device/capability assignments;
- last-used Race Setup;
- saved Race Setup presets where supported;
- user/presentation preferences;
- persistent competition progress;
- other persistent system settings.

The storage mechanism may later use controller flash/NVS, SD card or another technology. The lifecycle design refers only to **persistent storage**.

The last-used Race Setup is restored as the proposed/default setup for the next session. Modules use the applicable working configuration in RAM during normal operation rather than repeatedly consulting persistent storage. Live state from an interrupted individual race is not restored.

**Restore the Race Setup, not the race.**

## 5. Discovery and reconciliation

After loading remembered persistent information into working RAM, P&P discovers the equipment actually present and reconciles reality with remembered Registry information and the working configuration.

The normal case should require no user interaction:

```text
Load remembered configuration into working RAM
        ↓
Discover expected equipment
        ↓
Restore known assignments
        ↓
Continue startup
```

Expected but missing equipment retains its remembered identity/assignment but is marked unavailable.

Unexpected equipment is identified and processed without unnecessarily preventing the rest of P&P from becoming operational.

## 6. Device initialisation and time synchronisation

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

## 7. System assembly and availability

Once equipment has been reconciled and initialised, P&P determines which configured capabilities are currently available, establishes required safe/default output states and makes the system's current state available to Presentation/User Interaction.

System operational status is distinct from readiness for a particular activity.

P&P may be operational while an optional sector detector is unavailable. Whether a particular session can start depends upon the capabilities that session requires.

**Readiness is capability- and activity-based, not one global READY / NOT READY state.**

## 8. First-time startup and factory defaults

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

## 9. Missing, new and replacement equipment

### Expected equipment missing

Missing equipment does not erase its remembered assignment.

Unaffected capabilities continue to be available. A session that does not require the missing capability may still be usable.

### New equipment

New equipment is discovered, validated, initialised and synchronised automatically where applicable.

If its intended assignment can be inferred unambiguously, P&P may assign it automatically. Otherwise P&P asks only the minimum question necessary.

Optional unassigned equipment does not prevent unaffected functions from operating.

### Replacement equipment

A new device with a different stable identity does not silently become a missing known device.

Where a replacement is unambiguous, P&P may offer a simple confirmation and then transfer the previous assignments and persist the new relationship.

## 10. Missing or unusable stored configuration

P&P should prefer:

1. last-known-good persistent configuration;
2. factory defaults when no usable configuration exists.

A first-ever startup with no configuration is normal.

If stored configuration is corrupt, incomplete or incompatible, P&P must not silently invent physical assignments. It may fall back to safe factory/default behaviour while using discovery to assist controlled recovery/commissioning.

The persistence mechanism should later be designed so that an interrupted configuration write does not unnecessarily destroy the last-known-good configuration.

## 11. Persistence policy

Persistence is based on the value and lifecycle of the information, not on shutdown.

### Configuration and system state

Information required to restore P&P itself is persisted when it changes or at an appropriate immediate commit point. Normal restoration must not depend on a future graceful shutdown.

### Live individual-race state

Detailed live race state remains in working RAM while the race is active. The fixed Session Definition for an accepted race is also working RAM data; it is not a persistent configuration object or a separate module.

Examples include:
- current lap counts;
- current lap/sector timing state;
- live positions;
- current simulated fuel state;
- other transient competition state.

P&P is not required to continuously journal this information to persistent storage.

A controller crash or uncontrolled power loss may therefore lose the active individual race. This is acceptable.

### Completed race data

When an individual race finishes, the race/result data selected for History is written to persistent storage.

The product-level retained-History behaviour is defined in `BROWSER_FLOW_RESULTS_HISTORY_SPEC.md`. The detailed persistence schema, factory capacity selected from realistic storage testing and storage format remain implementation decisions.

## 12. Multi-race competition progress

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

## 13. Unfinished competition at startup

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

## 14. Graceful shutdown

Graceful shutdown is primarily verification, safe-state handling and tidying; it is not the normal mechanism by which important configuration is saved.

Where applicable, P&P should:
- stop or resolve active operation in a defined manner;
- check that persistent information which should already have been committed has been committed;
- complete outstanding persistent writes that are worth preserving;
- place controllable outputs into their defined safe states;
- notify intelligent modules of shutdown where genuinely necessary;
- stop services cleanly.

The detailed user experience for shutting down during an active individual race is a later product decision.

## 15. Uncontrolled shutdown

P&P must assume that a crash, watchdog reset or loss of power may provide no opportunity for shutdown processing.

Therefore:
- restoration-critical configuration must already have been persisted;
- loss of the active individual race is acceptable;
- the next boot follows the normal startup, reconciliation and synchronisation process;
- P&P is not required to reconstruct volatile live race state.

An uncontrolled shutdown must not be treated as a reason to add continuous race journalling or high-availability machinery.

## 16. Device disappearance and reconnection

The architecture supports devices becoming unavailable and subsequently available again where the underlying hardware and transport naturally support it.

A returning intelligent device may require reinitialisation and resynchronisation before its capabilities become available again.

When a capability becomes unavailable, its last reported physical state must not continue to be treated as trustworthy current physical state. On recovery, the capability establishes its current state afresh before becoming operationally available.

Availability transitions whose timing affects interpretation of events are related to P&P System Time. A delayed event is therefore interpreted against the relevant availability/session state at its event timestamp rather than merely the state at packet arrival.

This does **not** create a general requirement for powered physical hot-plugging.

Whether a particular wired interface is safe to connect or disconnect while powered is a hardware-design decision.

It is acceptable for the Race Director to stop P&P, replace failed wired timing hardware and restart the system.

## 17. Hardware fault containment and proportionality

Hardware interfaces should support the architectural principle of local failure where reasonably practical, but P&P does not promise continuous racing through arbitrary hardware faults or replacement.

There is no requirement for:
- wired hot-plug support;
- seamless in-race sensor replacement;
- preservation/restoration of every live lap after controller failure;
- high-availability operation.

Where a hardware failure invalidates an individual race, that race may be restarted. Where it occurs within a larger persisted competition, the competition can continue from that race after repair and restart.

**Engineer in proportion to the product.**
