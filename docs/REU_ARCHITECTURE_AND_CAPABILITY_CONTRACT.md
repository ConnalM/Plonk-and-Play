# REU Architecture and Capability Contract

**Status:** Draft for Rex review — architectural baseline, not implementation authorisation  
**Owner:** P&P System Architecture  
**Date:** 22 October 2026  
**Scope:** Defines the architectural responsibilities, capability model, authority rules, state semantics and extensibility contract for the Race Enhancement Unit (REU), both standalone and when integrated with Plonk & Play™ (P&P).

This document is intentionally above circuitry, firmware classes, packet formats, transports and Browser layouts. Rex should review it against electrical reality before it is promoted from draft to authoritative baseline. Al must not treat this draft as implementation authorisation.

It is consistent with the existing P&P principle that hardware describes what it is and can do, configuration describes how it is used, and transport changes how meaning travels rather than defining that meaning.

---

## 1. Product boundary

### 1.1 Fundamental rule

> **P&P measures cars and understands the race. REU electrically influences cars.**

P&P owns race truth, race rules, session state, timing, classification and race-derived requirements.

REU owns the analogue controller/powerbase electrical interface, electrical interpretation, electrical execution, protection and REU-owned configuration.

More specifically:

> **P&P decides WHAT electrical behaviour the race requires. REU decides HOW to produce that behaviour safely.**

P&P must not need to understand controller voltages, polarity, PWM topology, MOSFET behaviour, brake wiring or other REU-specific electrical implementation in order to request a meaningful electrical outcome.

REU must not need to understand why Race Control wants an outcome. P&P should request an electrical requirement such as a permitted-demand ceiling or drive prohibition, not send a race-semantic instruction such as `YELLOW_FLAG` and make REU interpret the racing rule.

### 1.2 First-class product configurations

All three are valid product configurations:

1. **P&P only** — complete sensor-based lap/race system with no controller/powerbase electrical connection.
2. **REU only** — complete standalone analogue controller enhancement system.
3. **P&P + REU** — P&P supplies race understanding and may impose temporary electrical requirements; REU supplies electrical execution and telemetry.

Neither product may become dependent on the other merely because combined operation exists.

### 1.3 Analogue interface ownership

All controller/powerbase analogue interface work belongs to REU, including where applicable:

- native TRS interfaces;
- two-wire interfaces;
- controller identification/calibration;
- polarity/function identification;
- manufacturer-specific adaptors/interposers;
- complementary cable-conversion interfaces;
- passive bypass/pass-through;
- mediated output control;
- braking circuitry;
- electrical sensing and protection;
- future motor-feedback circuitry.

The core P&P unit has no analogue controller/powerbase interface requirement.

---

## 2. Design objectives

REU architecture shall preserve these objectives:

1. **Standalone completeness.** REU's useful standalone functions must not require P&P.
2. **P&P independence.** P&P remains a complete race/lap system without REU.
3. **Passive ordinary operation.** Where the supported electrical interface permits it, loss of REU power should leave the ordinary controller/powerbase usable through a passive/fail-through path.
4. **Electrical safety remains local.** REU never delegates electrical protection to P&P or a network round trip.
5. **One underlying capability model.** Standalone REU UI and combined P&P UI consume the same REU-owned state and configuration; they do not maintain competing copies.
6. **Explicit capability discovery.** Consumers use advertised capability, not assumptions based on product name/version.
7. **Extensibility.** Future REU hardware can add capabilities without redefining unrelated existing capabilities or requiring a new overall architecture.
8. **Truthful state.** Unsupported, unavailable, invalid, stale and zero are distinct meanings.
9. **Explainability.** Where multiple transformations/constraints affect output, sufficient provenance is retained to explain the effective result.
10. **Architecture before implementation.** This contract defines product/system meaning. Circuitry, firmware structures, packets and UI layouts implement it rather than silently redefine it.

---

## 3. Responsibility and design ownership

### 3.1 Product Owner — Connal

The Product Owner decides desired product behaviour, scope and acceptance.

### 3.2 System Architecture

System Architecture owns:

- the P&P↔REU boundary;
- canonical capability meanings;
- authority and ownership;
- composition/order of transformations, constraints, vetoes and safety overrides;
- persistent configuration versus temporary requirements;
- capability and availability semantics;
- external-authority lifecycle;
- provenance requirements;
- shared vocabulary and units;
- transport-independent communications meaning;
- extensibility and compatibility rules.

### 3.3 Rex — REU electrical/hardware specialist

Rex owns the electrical means by which REU satisfies this contract, including controller sensing, identification, pass-through, switching, output generation, braking, protection and future motor-observation hardware.

Rex must report any electrical reality that makes an architectural requirement impossible, misleading or disproportionately expensive. Such a conflict returns to System Architecture/Product Owner; it is not silently resolved by changing the meaning in hardware or firmware.

### 3.4 Al — software implementation

Al implements agreed architecture in P&P software, shared communications/capability mechanisms and REU firmware/software. Implementation must not invent conflicting product semantics merely because a local representation is convenient.

### 3.5 Gina — Browser/UX specification

Gina specifies standalone REU and combined P&P+REU presentation after the underlying capability meanings and interfaces are stable enough to consume. Presentation may simplify terminology for users but must not create competing underlying concepts.

---

## 4. Underlying REU capability model

REU is not fundamentally a collection of unrelated named features. Its user-facing functions are constructed from a smaller set of underlying responsibilities.

The architectural model is:

1. **Controller Interface** — interpret what the connected human controller is requesting.
2. **Control Authority** — establish who/what currently has electrical control and whether that authority was safely achieved.
3. **Demand Transformation** — transform human or autonomous demand according to REU-owned response behaviour.
4. **Constraint and Permission Management** — apply REU-owned limits and temporary external requirements without confusing them with persistent configuration.
5. **Lane Output Control** — electrically produce drive/coast/brake behaviour.
6. **Electrical Observation and Protection** — protect hardware locally and expose meaningful electrical health/state.
7. **Configuration and Profiles** — persist reusable REU-owned behaviour/configuration.
8. **External Requirements and Autonomous Demand** — accept temporary requirements/demand from P&P or another authorised source while retaining REU electrical responsibility.
9. **Motor/Vehicle Observation** — optional/future observation such as BEMF, RPM and estimates derived from it.

Named features such as MINIMUG Assist, manual handicapping, car equalisation, throttle response and future traction assistance should use these primitives rather than create independent electrical control paths.

---

## 5. Canonical demand/output vocabulary

The word **power** must not be used internally as a catch-all for unrelated quantities.

The following conceptual distinctions are mandatory. Exact implementation field names are not specified here.

### 5.1 Controller Demand

A normalised representation of what the human controller is requesting.

Typical conceptual range: 0–100%.

It is not watts, volts, current or necessarily PWM duty.

### 5.2 Transformed Demand

Controller Demand after REU-owned response transformations such as response curve, rise/fall rate or smoothing, but before all ceilings/vetoes/protection have necessarily been applied.

### 5.3 Permitted Demand

The maximum/current demand permitted after applicable normal operating constraints and temporary external constraints have been composed.

### 5.4 Effective Demand

The demand that remains after applicable transformation, constraints and permission/veto semantics have been resolved, before or as input to lane-output generation.

### 5.5 Output Command

What REU commands its electrical lane-output hardware to produce. This may map to PWM or another electrical mechanism, but its definition must not be confused with measured electrical power.

### 5.6 Electrical measurements

Where hardware genuinely measures them, voltage, current, watts or other electrical quantities are separate measurements with physical units.

A percentage of PWM duty must never be labelled internally as measured electrical power.

### 5.7 User-facing terminology

A user-facing term such as **Power Limit** may remain appropriate if user testing/product design prefers it. If used, its canonical underlying meaning must be explicitly defined as a limit on normalised drive demand, not a claim about measured watts.

---

## 6. Human-drive processing model

The conceptual human-drive path is:

```text
Physical controller
      ↓
Controller Interface
      ↓
Controller Demand
      ↓
REU-owned Demand Transformations
      ↓
REU-owned operating constraints
      ↓
Temporary external/P&P constraints
      ↓
Drive Permission / veto
      ↓
Effective Demand
      ↓
Electrical Protection override
      ↓
Lane Output Control
      ↓
Physical controller/powerbase/track interface
```

This is a semantic ordering, not a required software call graph.

### 6.1 Transformations are not constraints

A response curve or acceleration/rise-rate limit transforms demand. It must not be modelled merely as another maximum ceiling if its actual meaning differs.

### 6.2 Constraints are not transformations

A maximum permitted demand constrains the result; it does not redefine the controller's response curve.

### 6.3 Drive prohibition is not zero percent

`Drive not permitted` is semantically distinct from `maximum permitted demand = 0%`, even if both can result in no propulsion at an instant.

Drive Permission/Inhibit is therefore a separate veto/permission concept.

### 6.4 Electrical protection outranks ordinary commands

Electrical protection may override human demand, REU configuration, P&P requirements and autonomous demand as necessary to protect the system.

P&P cannot command REU to bypass its local electrical protection.

---

## 7. Constraint composition

The firmware must not accidentally define product behaviour by choosing an arbitrary arithmetic rule.

### 7.1 Ceiling constraints

Where several independent requirements are genuinely upper ceilings on the same normalised demand quantity, the effective ceiling is the most restrictive valid ceiling.

Example:

```text
REU base maximum        80%
P&P temporary maximum   60%
Effective ceiling       60%
```

A less restrictive external ceiling does not raise a more restrictive REU-owned base ceiling.

### 7.2 Transformations

Response curves, rise/fall rates and smoothing are transformations with their own semantics. They are not automatically multiplied together as percentages unless the specific capability definition deliberately says so.

### 7.3 Vetoes

A valid Drive Inhibit/Drive Permission state can veto propulsion regardless of numerical demand ceilings.

### 7.4 Safety override

Electrical protection has final local authority to reduce or remove output regardless of ordinary transformations, ceilings or permissions.

### 7.5 Provenance

REU shall retain enough state to identify the effective limiting/vetoing reason where useful.

Conceptually:

```text
Driver demand       92%
REU base ceiling    80%
P&P ceiling         60%
Protection ceiling  45%
Effective result    45%
Limiting reason     ELECTRICAL_PROTECTION
```

This provenance is part of the model even if the ordinary UI presents a simpler explanation.

---

## 8. Configuration versus temporary external requirements

This distinction is fundamental.

### 8.1 REU-owned persistent configuration

Examples include a user's standalone drive-response settings, base demand ceiling, calibration and saved profile information.

REU owns and persists these values.

### 8.2 User configuration through the P&P Browser

When the combined P&P Browser deliberately edits an REU-owned persistent setting, it acts as a **configuration client of REU**.

Conceptually:

```text
User → P&P Browser → REU configuration operation → REU owns/persists result
```

P&P presenting the control does not transfer ownership of the setting to P&P.

### 8.3 P&P race requirements

When Race Control requires temporary electrical behaviour, it imposes an **external requirement**, not a configuration edit.

Conceptually:

```text
P&P Race Control → temporary electrical requirement → REU applies while valid
```

Such a requirement must not silently overwrite the user's persistent REU configuration.

Example:

```text
REU base ceiling        80%   (persistent)
P&P race ceiling        60%   (temporary)
Effective ceiling       60%
```

When the valid P&P requirement ends, REU can return to its base configuration according to the defined lifecycle/failover rules.

---

## 9. Control authority

REU owns the truth about whether it has actually achieved the electrical state required to control the lane.

Conceptual authority states include at least:

- **Transparent Human** — original controller path has direct electrical authority; REU cannot claim enforcement of mediated constraints it cannot physically impose.
- **Mediated Human** — human Controller Demand is interpreted/transformed by REU and REU controls lane output.
- **Autonomous** — an authorised autonomous demand source controls requested lane demand through REU's normal constraint/protection machinery.

These names need not be final UI labels.

REU must never report mediated/autonomous authority merely because software requested it. The reported state must reflect safely achieved electrical reality.

Loss of P&P does not inherently require loss of Mediated Human control; standalone REU behaviour may continue.

---

## 10. Passive/transparent operation

Passive ordinary-controller operation is a fundamental REU design objective.

Where the supported interface permits it, REU power loss should result in an electrical path equivalent to ordinary controller→powerbase operation without requiring firmware to act.

Transparent operation has an important semantic consequence:

> P&P must not assume an REU can enforce mediated electrical requirements while the lane is genuinely in a passive transparent state.

REU therefore reports actual authority/availability honestly.

The precise relay/switching implementation remains Rex's electrical design responsibility.

---

## 11. Capability model and extensibility

### 11.1 Explicit discovery

REU exposes what capabilities it supports. Consumers must not infer capability from model name, product generation or hardware revision when the capability can be explicitly discovered.

Constitutional rule:

> **No consumer may infer a device's capabilities from its product version, hardware revision or device type when those capabilities can be explicitly discovered.**

Thus a future REU V2 is not architecturally a special case. It is an REU exposing a richer capability set.

### 11.2 Unknown future capabilities

An older P&P implementation encountering an unknown future REU capability must be able to ignore that capability without breaking unrelated known operation.

A newer P&P encountering an older REU must degrade according to the capabilities actually advertised rather than assume missing V2 functions.

### 11.3 Hardware identity does not define capability

The architecture must permit variants such as a future Mini, standard, Pro, revised board or optional daughterboard without hard-coding product-name→feature assumptions.

### 11.4 Capability evolution

If a future capability is semantically richer than an existing one, it must not silently change the meaning of the old capability.

For example, a future continuously variable braking capability must not redefine an older binary brake/coast capability underneath existing consumers. It may extend/version the capability model or advertise a distinct richer capability as appropriate.

### 11.5 Architecture versus implementation scope

Every capability/status in design work shall distinguish:

- **Architectural foundation** — the architecture must support the concept because other/future behaviour depends on it.
- **V1 implementation requirement** — must actually be implemented in the first production REU.
- **Future/optional capability** — architecture accommodates it but V1 need not implement it.

Architectural provision must not automatically expand V1 product scope.

---

## 12. Capability support, availability and validity

These states must not be conflated.

### 12.1 Supported

The REU design/installed hardware supports the capability in principle.

### 12.2 Unsupported

The REU does not provide the capability. Consumers must not attempt to use it.

### 12.3 Currently available

A supported capability is presently able to perform its function.

### 12.4 Currently unavailable

A capability may be supported by the hardware but temporarily unusable because of connection state, detected interface type, fault, authority state or another legitimate condition.

Example: brake control may be supported by the REU hardware but unavailable with a two-wire controller interface or after an electrical fault.

### 12.5 Valid / invalid / stale data

For measurements/state, `invalid`, `unavailable` and `stale` must be representable separately from a valid numerical zero.

Zero RPM is not the same as RPM unavailable. Zero current is not the same as current measurement unsupported.

### 12.6 Dynamic availability

Capability discovery is not necessarily a once-at-boot static list. REU may report changes in current capability availability during operation.

P&P/Browser consequences belong to the consumer using the capability; REU reports the electrical/capability truth.

---

## 13. Core capability catalogue

The following catalogue defines architectural concepts. It does **not** by itself declare every item mandatory for first production V1.

### 13.1 Controller Input

**Purpose:** Convert supported physical controller behaviour into normalised Controller Demand.

**REU ownership:** Electrical interpretation, identification/calibration and live Controller Demand.

**Persistent:** Interface/controller calibration and configuration where required.

**Live:** Connection state, Controller Demand, identification/calibration status and service diagnostics.

**P&P exposure:** Normalised demand and meaningful status; raw voltages are diagnostic/electrical detail unless a defined capability requires them.

**Failure principle:** Failure to interpret a controller must never cause REU to invent a demand. Safe mediated output ceases; passive/manual fallback is preferred where electrically possible.

**Architecture status:** Fundamental.

### 13.2 Transparent Control

**Purpose:** Preserve ordinary controller→powerbase operation when REU is not electrically mediating the lane.

**REU ownership:** Actual bypass/pass-through state.

**Persistent:** No live authority state is persisted as truth across reboot.

**P&P exposure:** Whether REU currently has authority to enforce mediated requirements.

**Failure principle:** Passive/fail-through behaviour is preferred where supported; never claim enforceable mediated control while genuinely transparent.

**Architecture status:** Fundamental safety/product objective; V1 electrical feasibility to be confirmed by Rex per supported interface.

### 13.3 Lane Authority

**Purpose:** Represent actual electrical control authority: transparent human, mediated human or autonomous.

**REU ownership:** Actual achieved authority state and failure reason.

**Persistent:** Preferred standalone mode may persist; achieved authority is live and must be re-established after boot.

**P&P exposure:** Required for truthful integrated behaviour.

**Failure principle:** Requested authority is not achieved authority.

**Architecture status:** Fundamental.

### 13.4 Drive Response

**Purpose:** Transform input demand into desired drive response.

Potential elements include response curve, rise rate, fall rate and smoothing/damping.

**REU ownership:** Persistent standalone/base transformation settings and execution.

**Live:** Input demand, transformed demand and relevant active transformation state.

**P&P exposure:** Configuration may be presented/edited through combined UI; race logic should normally impose outcomes/constraints rather than reproduce REU algorithms.

**Architecture status:** Fundamental to mediated REU value; exact V1 feature set is Product Owner scope.

### 13.5 Demand Ceiling (user-facing name may be Power Limit)

**Purpose:** Limit normalised permitted drive demand.

**REU ownership:** Base/persistent ceiling.

**P&P role:** May impose a temporary upper ceiling without overwriting base configuration.

**Live:** Base ceiling, external ceiling(s), effective ceiling and limiting provenance.

**Failure principle:** A less restrictive external ceiling cannot raise a stricter REU base ceiling.

**Architecture status:** Fundamental.

### 13.6 Driver Assist / MINIMUG Assist™

**Purpose:** Package Drive Response and demand-ceiling primitives into a simple user-oriented assistance feature.

**REU ownership:** Persistent assistance preference/configuration.

**P&P role:** May present/configure the REU-owned feature. Race logic should not manipulate a marketing/user-assistance level when it actually means to impose a race electrical requirement.

**Architecture status:** Product feature built from primitives; V1 inclusion is a Product Owner decision.

### 13.7 Drive Permission / Inhibit

**Purpose:** Explicitly permit or prohibit propulsion independently of numerical demand ceilings.

**REU ownership:** Execution and actual state.

**P&P role:** May impose temporary drive prohibition for race-derived purposes without sending the race semantic reason as an electrical command.

**Persistent:** Temporary external inhibit must not survive reboot as an unexplained stale restriction.

**Failure principle:** Communication-loss behaviour while inhibited requires an explicit lifecycle policy; firmware must not invent one.

**Architecture status:** Fundamental if integrated P&P is to enforce starts/other drive prohibitions; exact V1 use is product scope.

### 13.8 Brake Control

**Purpose:** Provide coast/brake or richer braking only where the actual electrical interface supports it.

**REU ownership:** Electrical capability, base configuration and execution.

**P&P role:** May request a temporary braking requirement only if advertised and currently available.

**Failure principle:** Unsupported/unavailable braking is reported/rejected; it is never approximated by an unrelated action.

**Architecture status:** Optional by electrical capability. V1 support depends on proven supported interfaces and Product Owner scope.

### 13.9 Autonomous Demand

**Purpose:** Permit an authorised non-human source to request lane demand through REU's normal constraints, permission and protection machinery.

**REU ownership:** Electrical execution, local safety and freshness enforcement.

**Persistent:** An active autonomous demand never resumes from persisted stale state after reboot.

**Failure principle:** Stale/lost commands must not leave a car indefinitely running at the last demand. A bounded validity/watchdog/lease semantic is required before autonomous operation is implemented.

**Architecture status:** Architectural provision for future Ghost/pacer behaviour. This document does **not** automatically require V1 implementation.

### 13.10 Commanded Output

**Purpose:** Report what REU is commanding its lane-output hardware after applicable transformation/constraint logic.

**REU ownership:** Live state.

**P&P role:** Optional display/logging/diagnostics consumer.

**Validity:** If REU is not controlling the output, unavailable/invalid may be more truthful than numerical zero.

**Architecture status:** Fundamental telemetry for mediated operation.

### 13.11 Delivered Electrical Measurements

**Purpose:** Report actual measured electrical quantities where genuine sensing exists.

Potential values include voltage, current and watts.

**REU ownership:** Measurement and validity.

**P&P role:** Telemetry/diagnostics consumer.

**Architecture status:** Optional according to fitted sensing; must never be faked from PWM percentage and labelled measured power.

### 13.12 Lane Electrical Status and Protection

**Purpose:** Protect REU/controller/powerbase/car and expose meaningful electrical health.

Potential state includes supply, driver state, overcurrent, short/fault, thermal state, protection trip and degraded capability.

**REU ownership:** Entirely local electrical protection and state truth.

**P&P role:** Receive meaningful faults/capability changes; never make the fast electrical protection decision.

**Failure principle:** Protection outranks all ordinary commands.

**Architecture status:** Fundamental; telemetry richness may expand over time.

### 13.13 Profiles / reusable configuration

**Purpose:** Store reusable groups of REU-owned configuration.

Possible future distinctions include driver, car and combined profiles.

**REU ownership:** Standalone REU must retain enough configuration ownership to remain independent of P&P.

**P&P role:** May select/associate/present profiles without duplicating their underlying settings.

**Architecture status:** Architecture shall permit profiles. Full profile product behaviour is not automatically a V1 requirement.

### 13.14 Performance Adjustment / Equalisation / Handicap

**Purpose:** Reduce permitted performance to achieve a user/product objective.

This is an application/policy built from demand transformation, ceilings and configuration, not a separate electrical primitive.

**REU ownership:** Persistent/manual REU adjustment if configured standalone.

**P&P role:** P&P owns any race/timing-derived reasoning and may impose a temporary resulting electrical constraint.

**Architecture status:** Manual/product feature may be V1; automatic P&P equalisation can arrive later without changing the primitive architecture.

### 13.15 Identity and Capability Discovery

**Purpose:** State what this REU is, what capabilities it supports and what is currently available.

**REU ownership:** Stable identity, capability declarations and actual availability/status.

**P&P role:** Consume discovery into normal P&P Registry/Discovery concepts.

**Architecture status:** Fundamental.

### 13.16 External Requirement Management

**Purpose:** Keep temporary P&P requirements distinct from persistent REU configuration and manage their validity/lifetime.

**REU ownership:** Active application and validity of accepted external requirements.

**Persistent:** External race requirements are not persistent REU base configuration.

**Standalone presentation:** May show that an external requirement is active so a user can understand why effective behaviour differs from base settings.

**Architecture status:** Fundamental for integrated operation.

### 13.17 Communications Health

**Purpose:** Represent whether an external authority/peer is connected and whether its requirements remain valid.

**REU ownership:** Actual communications/freshness state.

**Persistent:** Pairing/identity may persist; live connection validity does not.

**Failure principle:** Drives predetermined external-requirement expiry and autonomous-demand safety behaviour.

**Architecture status:** Fundamental for integrated operation; physical transport remains open.

### 13.18 Diagnostics / Service Data

**Purpose:** Expose sufficient internal information for calibration, troubleshooting and development without forcing ordinary users to consume engineering detail.

**REU ownership:** REU-specific raw/service data.

**P&P role:** Surface meaningful fault summaries and selected diagnostics, not necessarily duplicate the entire REU engineering dashboard.

**Architecture status:** Fundamental framework; content capability-dependent.

---

## 14. Future motor/vehicle observation

These capabilities are deliberately outside V1 requirements unless separately promoted by the Product Owner.

### 14.1 Motor Feedback / BEMF

Potential future protected sensing of motor-generated voltage during valid high-impedance/coast periods.

Invalid/unavailable measurement is not zero.

Architecture may preserve inexpensive future provision, but V1 operation must not depend on BEMF.

### 14.2 Motor RPM

Potential REU-derived estimate from Motor Feedback or another future mechanism.

If provided, it should carry sufficient validity semantics to distinguish a credible estimate from unavailable/invalid data.

### 14.3 Vehicle speed

A generic unqualified `Speed` value is prohibited until ownership/provenance is defined.

Potential distinct concepts are:

- **REU Local Vehicle Speed Estimate** — derived locally from motor observation/calibration;
- **P&P Vehicle Speed Estimate** — potentially fused using REU observation plus authoritative track timing/sensors.

Presentation may later choose the best available value for a user-facing speedometer, but the underlying provenance must remain distinguishable.

### 14.4 Traction assistance

Future traction assistance would be an application using credible motor/vehicle observation and REU-local fast intervention. It must not become a V1 dependency merely because the architecture leaves room for it.

---

## 15. External requirements and lifecycle

The exact wire protocol is deliberately not defined here, but the semantics must satisfy the following.

### 15.1 Temporary scope

P&P race requirements are temporary and must not silently become persistent REU configuration.

### 15.2 Explicit validity

External requirements must have a defined validity lifecycle. The implementation must be able to distinguish a currently valid requirement from a stale requirement left behind by lost communications or a previous session.

### 15.3 No zombie constraints

A stale/disconnected P&P requirement must not remain indefinitely active by accident.

### 15.4 No runaway autonomous demand

Autonomous demand requires stronger freshness semantics than an ordinary static configuration value. Loss/staleness must cause a predetermined safe transition rather than indefinite continuation of the last demand.

### 15.5 Policy still to freeze

Before implementation, System Architecture/Product Owner must explicitly define the exact failover policy for at least:

- loss of P&P while an ordinary temporary demand ceiling is active;
- loss of P&P while Drive Inhibit is active;
- loss of P&P while an autonomous demand is active;
- reconnection after stale requirements have expired;
- transition between transparent, mediated and autonomous authority.

Rex and Al should identify constraints, but neither should invent these product behaviours independently.

---

## 16. P&P integration model

### 16.1 P&P consumes REU capabilities, not the REU web page

Standalone:

```text
Phone/tablet ↔ REU Browser/UI ↔ REU capability/state model ↔ REU electrical hardware
```

Combined:

```text
P&P Browser ↔ P&P ↔ capability/requirements contract ↔ REU capability/state model ↔ REU electrical hardware
```

P&P does not scrape or depend on the standalone REU web presentation.

### 16.2 One underlying setting/state

There is one REU-owned base setting for a concept such as a demand ceiling. It may be viewed/edited through REU's own UI or, deliberately, through the combined P&P UI. It does not become two independent settings.

### 16.3 Combined cockpit

When P&P+REU are present, the normal combined Browser may present P&P-native race information alongside discovered REU capabilities.

Unavailable/nonexistent REU features should not clutter P&P-only operation with dead controls.

### 16.4 Authoritative versus estimated data

P&P timing detector events remain authoritative for lap/sector timing. Future REU-derived RPM/speed values may be measurements or estimates with different provenance. Presentation must not erase that distinction in the underlying model.

---

## 17. P&P Registry/Discovery relationship

REU integration shall fit the existing P&P Registry/Discovery principles:

- intelligent devices have stable identity;
- devices expose capabilities;
- capability-level availability can change independently of whole-device presence;
- Registry reports availability but does not decide race consequences;
- transport-specific discovery feeds the same logical Registry;
- readiness is capability/activity based rather than one global hardware-ready flag.

REU-specific capability meaning may be richer than a simple input/output, but it must not create a parallel incompatible discovery universe.

---

## 18. Communications and transport

Physical P&P↔REU transport is intentionally **not frozen**.

ESP-NOW is attractive because both products use ESP32-class intelligence and it may avoid additional P&P hardware. Wired alternatives remain possible.

The contract must remain transport-independent.

> **Transport changes how a requirement/state travels, not what that requirement/state means.**

Current internal P&P Message Bus structs are not automatically the external REU protocol.

Conceptually, future communications may include:

**P&P → REU**
- temporary demand ceiling/constraint;
- drive permission/inhibit;
- braking requirement where supported;
- autonomous demand where implemented;
- potentially scheduled requirements against P&P System Time.

**REU → P&P**
- identity/capabilities;
- support/availability changes;
- authority state;
- acknowledgements/rejections;
- faults/diagnostics;
- Controller Demand;
- Effective Demand/Commanded Output;
- measured electrical telemetry where supported;
- future RPM/motor observations where supported.

This list is conceptual, not a packet definition.

---

## 19. Failure principles

1. **P&P absent:** REU remains usable for its standalone functions.
2. **REU absent:** P&P remains usable as a complete race/lap system without electrical enhancement.
3. **REU power loss:** passive ordinary-controller path is the design objective where electrically feasible.
4. **P&P communications loss:** REU remains electrically safe and follows explicitly defined external-requirement expiry/failover policy.
5. **Electrical fault:** REU protection outranks user/P&P commands and reports meaningful degraded/unavailable state.
6. **Unsupported request:** reject/report; never approximate with an unrelated electrical action.
7. **Invalid telemetry:** report invalid/unavailable/stale; never silently substitute numerical zero.
8. **Capability loss while running:** report changed availability; consumer decides product/race consequence according to its responsibility.
9. **Reboot:** live authority, stale race constraints and autonomous throttle commands are not blindly restored from persistence.

---

## 20. V1 scope discipline

This document intentionally distinguishes architectural necessity from first-production scope.

### 20.1 Likely architectural foundations

The architecture should support from the beginning:

- Controller Input;
- Transparent/Mediated authority distinction;
- Demand Transformation;
- demand ceilings;
- Drive Permission/Inhibit semantics;
- Lane Output Control;
- local Electrical Protection/Status;
- Identity/Capability Discovery;
- External Requirement Management;
- Communications Health;
- support/availability/validity semantics;
- provenance of effective limitation;
- extensibility to future autonomous and observation capabilities.

### 20.2 V1 implementation is a separate Product Owner decision

The presence of an architectural concept here does not by itself require a complete V1 user feature.

In particular, this draft does not automatically require first production V1 to ship:

- sophisticated Profiles;
- every form of braking;
- Ghost/pacer algorithms;
- external Autonomous Demand;
- automatic equalisation;
- current/voltage/watt telemetry beyond what protection requires;
- BEMF;
- RPM;
- vehicle-speed estimation;
- traction assistance.

The architecture must avoid making these unnecessarily difficult later without building them prematurely now.

---

## 21. Open architectural decisions before implementation freeze

The following remain intentionally open and must be resolved deliberately before affected implementation is authorised:

1. Exact external-requirement expiry/failover semantics.
2. Drive Inhibit behaviour on P&P communications loss.
3. Autonomous-demand freshness/watchdog/lease semantics if/when implemented.
4. Exact canonical user-facing name for Demand Ceiling (`Power Limit` remains a candidate).
5. Exact V1 transformation set: curve, rise/fall rates, smoothing etc.
6. V1 profile scope and whether driver/car/combined identity belongs in REU or higher-level P&P association.
7. V1 supported controller/interface families after Rex's electrical verification.
8. Exact braking capabilities achievable per supported interface.
9. Whether current/voltage sensing is mandatory for V1 protection/diagnostics or optional telemetry.
10. Ownership and naming of future fused vehicle-speed estimates.
11. Physical P&P↔REU transport and protocol representation.
12. Exact first-production V1 feature cut.

Open items are not permission for implementation to choose arbitrary behaviour.

---

## 22. Review required from Rex

Rex should review this draft as the electrical/hardware specialist and report, rather than implement.

Please identify:

1. Any requirement that is electrically impossible or misleading.
2. Any requirement that is technically possible but disproportionately expensive/complex for the product.
3. Any place where the semantic distinction does not match electrical reality.
4. Any missing electrical state/capability the architecture must know about.
5. Whether passive unpowered pass-through is realistic across the intended V1 interface families.
6. Whether Controller Demand can be normalised credibly across the intended V1 controller families and what architectural caveats are needed.
7. What braking capability can truthfully be advertised for each likely interface family.
8. Whether any protection requirement requires current/voltage/temperature sensing to become mandatory rather than optional.
9. Whether the proposed authority states are sufficient to describe actual switching/electrical states.
10. Whether future BEMF/RPM provision requires any inexpensive V1 hardware hooks that should be preserved now without making BEMF a V1 dependency.
11. Any assumption in this document that accidentally prevents a future REU V2, Mini, Pro or revised hardware from exposing richer capabilities.

Do not choose packet formats, component values or firmware structures as part of this review unless needed to demonstrate an architectural problem.

Return disagreements and caveats to System Architecture before detailed design proceeds.

---

## 23. Constitutional summary

The REU architecture can be reduced to these guardrails:

> **P&P owns race truth. REU owns electrical truth.**

> **P&P requests meaningful electrical outcomes. REU owns the safe electrical means.**

> **Persistent REU configuration and temporary P&P race requirements are different things.**

> **One underlying REU capability/state model serves both standalone and combined presentation.**

> **Transformations, constraints, vetoes and safety overrides have distinct meanings and must not be collapsed into arbitrary percentages.**

> **Electrical protection always remains local to REU and outranks ordinary commands.**

> **Capabilities are explicitly discovered; they are not inferred from REU version or product name.**

> **Supported, available, valid, stale and zero are different states.**

> **Future capability is accommodated by extensible architecture, not smuggled into V1 scope.**

> **If electrical reality, implementation convenience or UI design conflicts with this contract, the conflict is brought back for deliberate architectural/product resolution rather than silently changing the meaning.**
