# REU Architecture and Capability Contract

**Status:** Authoritative architectural baseline — implementation still requires explicit Product Owner authorisation  
**Owner:** P&P System Architecture  
**Date:** 22 October 2026  
**Scope:** Architectural responsibilities, capability model, authority rules, state semantics and extensibility contract for the Race Enhancement Unit (REU/MUG), standalone and integrated with Plonk & Play™ (P&P).

This baseline consolidates the original architecture draft and Rex's accepted electrical-reality review. It sits above circuitry, firmware classes, packet formats, transports and Browser layouts. Hardware, software and UX must implement these meanings rather than silently redefine them.

---

## 1. Fundamental product boundary

> **P&P measures cars and understands the race. REU electrically influences cars.**

P&P owns race truth, race rules, session state, timing, classification and race-derived requirements.

REU owns the analogue controller/powerbase electrical interface, electrical interpretation, electrical execution, protection and REU-owned configuration.

> **P&P decides WHAT electrical behaviour the race requires. REU decides HOW to produce that behaviour safely.**

P&P must not need to understand controller voltages, polarity, PWM topology, MOSFET behaviour, brake wiring or other REU electrical implementation. REU must not need to interpret race semantics such as `YELLOW_FLAG`; P&P requests the resulting electrical requirement.

All three configurations are first-class:

1. **P&P only** — complete sensor-based lap/race system with no controller/powerbase electrical connection.
2. **REU only** — standalone analogue controller enhancement system.
3. **P&P + REU** — P&P supplies race understanding and temporary electrical requirements; REU supplies electrical execution and telemetry.

Neither product becomes dependent on the other merely because combined operation exists.

All analogue controller/powerbase work belongs to REU, including TRS/two-wire/proprietary interfaces, identification/calibration, polarity/function identification, adaptors, cable-conversion interfaces, pass-through, mediated output, braking, protection and future motor observation. Core P&P has no analogue controller interface requirement.

---

## 2. Design objectives

REU architecture shall preserve:

1. **Standalone completeness.** Useful standalone REU functions do not require P&P.
2. **P&P independence.** P&P remains complete without REU.
3. **Passive ordinary operation.** For each verified supported interface where electrically practical, loss of REU power should leave a passive path functionally equivalent to ordinary controller→powerbase operation. This is not a promise of literally zero added resistance/capacitance and is not guaranteed for arbitrary analogue hardware.
4. **Local electrical safety.** REU never delegates electrical protection to P&P or a network round trip.
5. **One capability/state model.** Standalone REU UI and combined P&P UI consume the same REU-owned state/configuration.
6. **Explicit capability discovery.** Consumers use advertised capability, not product-name/version assumptions.
7. **Extensibility.** Mini/V2/Pro/revised hardware can add or omit capabilities without a new architecture.
8. **Truthful state.** Unsupported, unavailable, invalid, stale and numerical zero are distinct.
9. **Explainability.** Effective output retains enough provenance to explain which transformation, constraint, veto or protection state determined it.
10. **Architecture before implementation.** Circuitry, firmware, protocol and UX implement this contract rather than define product meaning by accident.

---

## 3. Responsibility and ownership

- **Connal / Product Owner:** desired behaviour, product scope and acceptance.
- **System Architecture:** P&P↔REU boundary; canonical meanings; ownership; authority; transformation/constraint ordering; configuration versus temporary requirements; availability/validity semantics; provenance; external-authority lifecycle; extensibility; shared vocabulary and transport-independent communication meaning.
- **Rex:** electrical/hardware means: controller sensing, identification, pass-through, switching, output generation, braking, protection and future motor-observation hardware. Electrical conflicts return to Architecture/Product Owner rather than silently changing semantics.
- **Al:** software implementation in P&P, shared communications/capability mechanisms and REU firmware. Implementation convenience does not authorise semantic changes.
- **Gina:** standalone REU and combined P&P+REU Browser/UX after capability meanings/interfaces are stable. Presentation may simplify language but must not create competing concepts.

---

## 4. Underlying capability model

REU is not fundamentally a list of unrelated features. User features are built from these responsibilities:

1. **Controller Interface** — interpret a supported human controller request.
2. **Control Authority** — establish who/what currently has electrical drive authority and whether it was safely achieved.
3. **Demand Transformation** — transform human/autonomous demand according to REU-owned response behaviour.
4. **Constraint and Permission Management** — apply REU-owned limits and temporary external requirements.
5. **Lane Output Control** — electrically produce DRIVE/COAST/BRAKE/SAFE behaviour as supported.
6. **Electrical Observation and Protection** — protect hardware locally and expose meaningful health/state.
7. **Configuration and Profiles** — persist reusable REU-owned behaviour/configuration.
8. **External Requirements and Autonomous Demand** — accept temporary requirements/demand while retaining REU electrical responsibility.
9. **Motor/Vehicle Observation** — optional/future BEMF, RPM and derived estimates.

MINIMUG Assist, handicapping, equalisation, throttle response and future traction assistance use these primitives rather than creating independent electrical paths.

---

## 5. Canonical demand and electrical vocabulary

The word **power** must not be used internally as a catch-all.

### 5.1 Controller Demand

> **REU's normalised interpretation of the connected controller's requested propulsion, where that interpretation is supported and currently valid.**

Typical conceptual range: 0–100%. It is not physical trigger position, watts, volts, current or necessarily PWM duty. A controller may work transparently while valid Controller Demand cannot be derived.

### 5.2 Transformed Demand

Controller Demand after REU-owned response transformations such as response curve, rise/fall rate or smoothing.

### 5.3 Permitted Demand

Demand permitted after applicable normal operating and temporary external constraints are composed.

### 5.4 Effective Demand

Demand remaining after transformations, constraints and permission/veto semantics have been resolved, before/as input to lane-output generation.

### 5.5 Output Command

What REU commands the electrical output hardware to produce. It may map to PWM or another mechanism; it is not measured electrical power.

### 5.6 Electrical measurements

Actual voltage/current/watts or other physical quantities exist only where genuinely measured. PWM/demand percentages must never be presented internally as measured electrical power.

### 5.7 Electrical output state

Numerical demand is insufficient to describe lane electrical state. The model preserves, where supported:

- **DRIVE** — propulsion output produced;
- **COAST** — propulsion absent with appropriate high-impedance/free-running state;
- **BRAKE** — deliberate electrically supported braking condition;
- **SAFE / ISOLATED** — deliberate no-drive electrical state for transition, protection or fault handling.

`0% + COAST` and `0% + BRAKE` are not equivalent.

A friendly UI term such as **Power Limit** may be used, but its canonical underlying meaning is a limit on normalised drive demand, not watts.

---

## 6. Human-drive processing and composition

Conceptually:

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
DRIVE / COAST / BRAKE / SAFE as supported
```

This is semantic ordering, not a required software call graph.

- Response curves/rise/fall/smoothing are **transformations**, not automatically numerical ceilings.
- Maximum permitted demand values are **constraints**, not transformations.
- **Drive Inhibit** is a veto and is semantically different from a 0% ceiling.
- **Electrical protection** has final local authority and may reduce/remove output regardless of human demand, REU configuration, P&P requirements or autonomous demand.

Where several valid requirements are genuine upper ceilings on the same normalised quantity, the most restrictive ceiling wins. A less restrictive P&P ceiling cannot raise a stricter REU base ceiling.

REU retains useful provenance. Example:

```text
Driver demand       92%
REU base ceiling    80%
P&P ceiling         60%
Protection ceiling  45%
Effective result    45%
Limiting reason     ELECTRICAL_PROTECTION
```

Protection state remains distinct from the numerical result it causes.

---

## 7. Persistent configuration versus temporary requirements

This distinction is constitutional.

**REU-owned persistent configuration** includes base response, base demand ceiling, calibration and saved REU configuration/profile information.

When a user changes such a setting through the combined P&P Browser, P&P is acting as a **configuration client of REU**. REU remains owner and persists the result.

A P&P race requirement is different:

```text
REU base ceiling        80%   persistent
P&P race ceiling        60%   temporary
Effective ceiling       60%
```

The temporary requirement does not overwrite 80%. When it expires, REU returns according to the defined lifecycle/failover rules.

External requirements must have explicit validity and expiry semantics. There must be no accidental zombie constraints after communications loss, session change or reboot.

---

## 8. Control authority

REU owns the truth about **achieved** electrical authority. Requested authority is not achieved authority.

Conceptual states include at least:

- **Transparent Human** — ordinary controller path has direct electrical authority; REU cannot claim enforcement of mediated constraints it cannot physically impose.
- **Mediated Human** — REU interprets human Controller Demand and controls lane output.
- **Autonomous** — an authorised non-human demand source controls requested lane demand through normal REU constraints/protection.
- **Safe / No Authority** — no human or autonomous source currently has electrical drive authority.

Safe/No Authority covers switching transitions, protection trips, deliberate isolation and fault states. REU must never report mediated/autonomous authority merely because software requested it.

Loss of P&P does not inherently require loss of standalone Mediated Human operation.

---

## 9. Compatibility is capability-specific

A controller/powerbase installation is not simply `compatible` or `incompatible` with every REU feature.

> **Transparent compatibility does not imply Mediated Human compatibility.**

A controller may operate normally through the transparent path while REU cannot reliably derive valid Controller Demand. Conversely, mediated throttle support does not imply braking, telemetry or motor observation.

A simple two-wire resistive controller may support more mediated features than a sophisticated three-wire electronic controller if the latter cannot be interpreted reliably.

A two-wire controller is **not** inherently basic-only. Where REU can interpret and mediate it, it may support demand limiting, response curves, acceleration limiting, smoothing, MINIMUG Assist, P&P constraints and future autonomous demand. A separate third conductor is relevant to conventional three-wire braking, not to the ordinary mediated-throttle feature family.

### 9.1 MUG feature-set principle

> **No MUG/REU installation is required to provide every capability defined by this architecture. The available features are determined by the capabilities of the REU hardware together with the connected controller/powerbase interface. Capabilities must be discovered and reported explicitly; they must not be inferred merely from product name, generation, connector type or general compatibility.**

Therefore `MUG compatible` must never automatically mean `every MUG feature supported`.

The Browser presents what the current installation can actually do, not a fixed generic MUG feature list.

### 9.2 Capability dependencies

Capabilities may depend on other capabilities/states. Examples:

- MINIMUG-style assistance requires valid Controller Demand, Mediated Human authority and controllable lane output;
- P&P demand ceilings require authority capable of enforcing them;
- conventional dynamic braking requires a verified braking-capable interface;
- future RPM requires valid motor-observation capability;
- future traction assistance requires sufficiently credible observation plus mediated output authority.

Runtime loss of a dependency can therefore make a supported capability currently unavailable.

---

## 10. Passive/transparent operation

Passive ordinary-controller operation is a strong design objective, verified interface by interface.

Where a verified supported interface permits it, REU power loss should leave a passive electrical path **functionally equivalent** to ordinary controller→powerbase operation without firmware action. `Equivalent` does not promise literally zero effect from contacts, connectors, PCB tracks or protection.

Transparent operation also means P&P must not assume REU can enforce mediated requirements while the lane is genuinely passive/transparent.

REU may expose **bypass availability** separately from current Control Authority.

---

## 11. Capability discovery, variants and expansion

### 11.1 Constitutional discovery rule

> **No consumer may infer a device's capabilities from its product version, hardware revision or device type when those capabilities can be explicitly discovered.**

A future V2 is not a special architectural case; it is an REU exposing a richer capability set.

An older P&P must be able to ignore unknown future capabilities without breaking known operation. A newer P&P must degrade according to the capabilities actually advertised by older/cheaper hardware.

### 11.2 Product variant does not define capability

The architecture permits Mini, standard, Pro, revised boards and optional expansion without hard-coded product-name→feature assumptions.

### 11.3 Lane count is not REU identity

`REU` does not mean `two lanes`. The first product may be two-lane, but discovery must permit one-lane Mini, two-lane standard, four-lane integrated or future expandable/chained arrangements. Consumers discover lane resources actually exposed.

### 11.4 Capability evolution

A richer future capability must not silently redefine an older one. For example, variable braking may extend or sit alongside an older brake/coast capability rather than changing the old meaning underneath existing consumers.

### 11.5 Architecture versus product scope

Every capability distinguishes:

- **Architectural foundation** — architecture must support the concept;
- **V1 implementation requirement** — must actually ship in first production REU;
- **Future/optional capability** — architecture accommodates it but V1 need not implement it.

Leaving room for Ghost, BEMF or V2 must not smuggle those features into V1.

---

## 12. Support, availability, validity and interface state

These meanings must remain distinct:

- **Supported** — installed hardware/interface can provide the capability in principle.
- **Unsupported** — it cannot.
- **Available** — supported and currently able to operate.
- **Unavailable** — supported but presently unusable because of connection, interface, authority, fault or dependency state.
- **Valid / Invalid / Stale** — data-state semantics separate from numerical values.

Zero RPM is not RPM unavailable. Zero current is not current measurement unsupported.

Capability availability may change at runtime.

Where applicable REU may expose **Interface Identification State**, conceptually `known / unknown / incompatible / ambiguous`.

Protection state is distinct from resulting demand/output, conceptually `normal / limited / tripped-faulted`.

---

## 13. Capability catalogue

This catalogue defines architectural concepts; it does not by itself make each a V1 shipping feature.

### 13.1 Controller Input
Converts supported controller behaviour into valid normalised Controller Demand. REU owns interpretation, identification/calibration and validity. Failure to interpret must never invent demand. Transparent operation may remain possible even where mediated Controller Demand is unsupported.

### 13.2 Transparent Control
Preserves ordinary controller→powerbase operation when REU is not mediating. REU owns actual bypass/pass-through truth. Passive fail-through is verified per supported interface.

### 13.3 Lane Authority
Reports achieved Transparent Human, Mediated Human, Autonomous or Safe/No Authority state and relevant failure reason. Live authority is re-established after boot, not blindly restored.

### 13.4 Drive Response
Transforms demand through configured response curve, rise/fall rate, smoothing/damping or other deliberately defined transformations. Exact V1 set remains Product Owner scope.

### 13.5 Demand Ceiling / user-facing Power Limit
Limits normalised permitted drive demand. REU owns base/persistent ceiling; P&P may impose a temporary stricter ceiling. Base, external and effective values plus provenance remain distinguishable.

### 13.6 Driver Assist / MINIMUG Assist™
A user-oriented feature built from Drive Response and demand-ceiling primitives. REU owns persistent assistance configuration. P&P may present/configure it, but race logic imposes electrical outcomes rather than manipulating a marketing assistance level.

### 13.7 Drive Permission / Inhibit
Explicit propulsion permission/veto independent of numerical ceilings. Temporary external inhibit does not persist as an unexplained stale restriction. Communications-loss behaviour must be deliberately defined.

### 13.8 Lane Output State and Control
Produces supported DRIVE, COAST, BRAKE and SAFE/ISOLATED states. Demand percentage alone does not replace electrical-state truth.

### 13.9 Brake capability
Braking is advertised only where electrically verified. A verified POWER/TRACK/BRAKE interface may support dynamic braking. A two-wire interface does not magically gain conventional three-wire braking. Unknown/proprietary interfaces make no braking claim until verified. Future discovery may distinguish coast, dynamic brake and variable brake support.

### 13.10 Autonomous Demand
Allows an authorised non-human source to request lane demand through normal constraints/permission/protection. Active autonomous demand never resumes from stale persisted state. Implementation requires bounded freshness/watchdog/lease semantics. Architectural provision does not make this a V1 shipping requirement.

### 13.11 Commanded Output
Reports what REU commands output hardware after applicable logic. When REU is not controlling output, unavailable/invalid may be more truthful than zero.

### 13.12 Delivered Electrical Measurements
Actual measured voltage/current/watts only where sensing exists. Optional/capability-dependent and never faked from PWM percentage.

### 13.13 Electrical Protection and Status
Electrical protection is **mandatory wherever REU actively drives a lane** and outranks all ordinary commands. The architecture does not prescribe whether this is achieved by switching-device protection, fuse/polyfuse, dedicated current limiting, measured current, thermal protection or another appropriate combination.

> **Protection mandatory; rich precision telemetry capability-dependent.**

Precision voltage/current/watt telemetry is not automatically a V1 requirement.

### 13.14 Profiles / reusable configuration
Architecture permits reusable REU-owned configuration. Driver/car/combined profile product behaviour is not automatically a V1 requirement.

### 13.15 Performance Adjustment / Equalisation / Handicap
An application/policy built from transformations, ceilings and configuration, not a separate electrical primitive. P&P owns race/timing-derived reasoning; REU executes resulting electrical requirements.

### 13.16 Identity and Capability Discovery
REU exposes stable identity, lane resources, supported capabilities and current availability. P&P consumes these through its normal Registry/Discovery concepts.

### 13.17 External Requirement Management
Keeps temporary P&P requirements distinct from persistent REU configuration and manages validity/lifetime.

### 13.18 Communications Health
Represents peer connection/freshness and external-requirement validity. Pairing/identity may persist; live connection validity does not.

### 13.19 Diagnostics / Service Data
Exposes sufficient internal information for calibration/troubleshooting/development. P&P may surface meaningful summaries without duplicating the complete engineering dashboard.

---

## 14. Future motor/vehicle observation

BEMF, RPM, vehicle-speed estimation and traction assistance remain future/optional unless explicitly promoted by the Product Owner.

Where negligible in cost/complexity, V1 design should avoid unnecessarily designing out future provision such as:

- ADC-capable MCU input per lane;
- PCB pads/space for protected TRACK-to-BRAKE/return sensing;
- access to relevant TRACK and BRAKE/return points;
- useful development test points;
- output topology capable of genuine COAST/high-impedance intervals.

This does **not** authorise a precision analogue front end or BEMF-specific components merely for possible V2 use.

Potential future underlying values remain provenance-specific:

- **Motor RPM** — REU-derived estimate;
- **REU Local Vehicle Speed Estimate** — local estimate from motor observation/calibration;
- **P&P Vehicle Speed Estimate** — potentially fused using REU observation plus P&P track/sensor knowledge.

A generic underlying `Speed` value is prohibited until provenance/ownership is defined. Presentation may later show the best available value without erasing provenance.

---

## 15. External requirements and failure lifecycle

Temporary P&P requirements do not become persistent REU configuration. They require explicit validity and expiry; stale requirements must not remain indefinitely by accident.

Autonomous demand requires stronger freshness semantics than static configuration. Lost/stale commands must cause a predetermined safe transition rather than leave a car indefinitely running at the last demand.

Before affected implementation is authorised, Architecture/Product Owner must deliberately freeze policy for:

- P&P loss while a temporary demand ceiling is active;
- P&P loss while Drive Inhibit is active;
- P&P loss while Autonomous Demand is active;
- reconnection after requirements expire;
- transitions among Transparent Human, Mediated Human, Autonomous and Safe/No Authority.

Rex and Al identify constraints; neither independently invents these product behaviours.

---

## 16. P&P integration and presentation

Standalone:

```text
Phone/tablet ↔ REU Browser/UI ↔ REU capability/state model ↔ REU electrical hardware
```

Combined:

```text
P&P Browser ↔ P&P ↔ capability/requirements contract ↔ REU capability/state model ↔ REU electrical hardware
```

P&P does not scrape the REU web page. It consumes the same underlying REU information/control model used by the standalone web server.

There is one REU-owned setting/state, not a P&P copy plus an REU copy.

The combined P&P Browser is the normal combined cockpit. It presents P&P-native race information alongside discovered REU capabilities. Capabilities not present/available should not clutter P&P-only operation with dead controls.

Authoritative P&P timing remains distinct from future REU-derived estimates such as RPM/speed.

---

## 17. Registry, discovery and transport

REU integration uses existing P&P principles: stable intelligent-device identity, explicit capabilities, capability-level availability, transport-independent discovery and activity-based readiness.

Physical P&P↔REU transport is **not frozen**. ESP-NOW is attractive; wired alternatives remain possible.

> **Transport changes how a requirement/state travels, not what it means.**

Current internal P&P Message Bus structs are not automatically the external REU protocol.

Conceptually P&P→REU may carry temporary demand ceilings, drive permission/inhibit, braking requirements where supported, autonomous demand where implemented and possibly scheduled requirements.

REU→P&P may carry identity/capabilities, availability changes, authority/output/protection state, acknowledgements/rejections, faults/diagnostics, Controller Demand, Effective Demand, Commanded Output, measured telemetry where supported and future motor observations.

This is not a packet definition.

---

## 18. Failure principles

1. **P&P absent:** REU remains usable for standalone functions.
2. **REU absent:** P&P remains a complete race/lap system without electrical enhancement.
3. **REU power loss:** passive ordinary-controller path is the objective for each verified interface where electrically feasible.
4. **P&P communications loss:** REU remains electrically safe and follows deliberately defined requirement-expiry/failover policy.
5. **Electrical fault:** local protection outranks commands and reports degraded/unavailable state.
6. **Unsupported request:** reject/report; never approximate with an unrelated electrical action.
7. **Invalid telemetry:** report invalid/unavailable/stale; never silently substitute zero.
8. **Capability loss while running:** REU reports changed availability; the responsible consumer decides product/race consequence.
9. **Reboot:** live authority, stale race constraints and autonomous throttle commands are not blindly restored.

---

## 19. V1 scope and electrical proof

The architecture must support the foundation without automatically shipping every possible feature.

Likely foundations include Controller Input; Transparent/Mediated/Safe authority distinction; Demand Transformation; demand ceilings; Drive Permission/Inhibit semantics; Lane Output Control and output-state truth; local Electrical Protection/Status; Identity/Capability Discovery; External Requirement Management; Communications Health; support/availability/validity semantics; provenance; and extensibility.

This baseline does **not** automatically require V1 to ship sophisticated profiles, every form of braking, Ghost/pacer algorithms, external autonomous demand, automatic equalisation, precision electrical telemetry, BEMF, RPM, vehicle-speed estimation or traction assistance.

The eventual V1 compatibility catalogue must be established by interface-specific electrical proof. Rex identified the two most important proof areas as:

1. reliable Controller Demand sensing/normalisation across intended controller families;
2. truthful braking behaviour across intended interfaces.

V1 scope must be chosen from verified capability combinations rather than attempting universal analogue-controller support.

---

## 20. Open decisions before affected implementation

These remain deliberately open until resolved by Architecture/Product Owner with Rex/Al input where appropriate:

1. Exact external-requirement expiry/failover semantics.
2. Drive Inhibit behaviour on P&P communications loss.
3. Autonomous-demand freshness/watchdog/lease semantics if/when implemented.
4. Final user-facing name for Demand Ceiling (`Power Limit` remains a candidate).
5. Exact V1 transformation set.
6. V1 profile scope.
7. V1 supported controller/interface families after electrical verification.
8. Exact braking capabilities per verified interface.
9. Exact protection implementation and whether any sensing used for protection also becomes exposed telemetry.
10. Ownership/naming of future fused vehicle-speed estimates.
11. Physical P&P↔REU transport and protocol representation.
12. Exact first-production V1 feature cut.

Open items are not permission for implementation to choose arbitrary behaviour.

---

## 21. Constitutional summary

> **P&P owns race truth. REU owns electrical truth.**

> **P&P requests meaningful electrical outcomes. REU owns the safe electrical means.**

> **Persistent REU configuration and temporary P&P race requirements are different things.**

> **One underlying REU capability/state model serves standalone and combined presentation.**

> **Controller Demand is REU's valid normalised interpretation of requested propulsion, not literal trigger position.**

> **DRIVE, COAST, BRAKE and SAFE are electrical states; zero demand does not replace them.**

> **Transparent compatibility does not guarantee mediated-control compatibility.**

> **No MUG is required to provide every MUG capability. Features are discovered from the actual REU + controller/powerbase installation.**

> **A two-wire controller is not inherently denied mediated/MINIMUG functions; braking and other features remain capability-specific.**

> **Electrical protection remains local and mandatory for active lane drive; precision telemetry remains capability-dependent.**

> **Capabilities are explicitly discovered, may have dependencies and may become unavailable dynamically.**

> **REU identity does not imply a product generation, feature set or lane count.**

> **Future capability is accommodated by extensible architecture, not smuggled into V1 scope.**

> **If electrical reality, implementation convenience or UI design conflicts with this contract, the conflict returns for deliberate architectural/product resolution rather than silently changing meaning.**
