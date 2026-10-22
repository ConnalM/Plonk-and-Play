# REU Architecture — Rex Review Reconciliation

**Status:** Accepted architectural reconciliation  
**Owner:** P&P System Architecture  
**Date:** 22 October 2026  
**Applies to:** `docs/REU_ARCHITECTURE_AND_CAPABILITY_CONTRACT.md`  

This document records the architectural disposition of Rex's first electrical-reality review of the REU Architecture and Capability Contract. These points are accepted architecture and must be read with the draft contract. Where wording in the draft contract is less precise, this reconciliation governs until the material is consolidated into the baseline contract.

No circuitry, firmware implementation, packet format or V1 product scope is authorised by this reconciliation.

---

## 1. Rex review disposition

Rex's verdict was **APPROVE WITH CAVEATS**. System Architecture accepts the caveats. There is no electrical architectural blocker.

The architecture is therefore retained, with the refinements below.

---

## 2. Electrical output state is distinct from demand

A numerical demand value is not sufficient to describe the electrical state of a lane.

The architecture shall preserve distinct electrical output states where supported, including conceptually:

- **DRIVE** — propulsion output is being produced;
- **COAST** — propulsion is absent and the output is in the appropriate high-impedance/free-running state;
- **BRAKE** — propulsion is absent and an electrically supported braking condition is deliberately applied;
- **SAFE / ISOLATED** — the lane is deliberately placed in a no-drive electrical condition for transition, protection or fault handling.

Exact implementation names are not frozen here.

A demand of `0%` must not be used as a substitute for this distinction. In particular, `0% + COAST` and `0% + BRAKE` may have materially different electrical and vehicle behaviour.

This distinction also preserves a clean route to future BEMF observation, which may require genuine coast/high-impedance intervals.

---

## 3. Safe / no-authority condition

The Control Authority model shall permit a condition in which **no human or autonomous source currently has electrical drive authority**.

This is required for truthful representation of states such as:

- transition between Transparent Human and Mediated Human;
- transition into or out of Autonomous authority;
- human path disconnected while mediated output is not yet safely enabled;
- protection trip;
- deliberate electrical isolation;
- fault handling.

Conceptually the authority model therefore includes at least:

- Transparent Human;
- Mediated Human;
- Autonomous;
- Safe / No Authority.

Exact implementation/UI names remain open.

Requested authority and achieved authority remain distinct. REU reports achieved electrical reality.

---

## 4. Controller Demand definition

`Controller Demand` must not be defined as literal physical trigger position.

Canonical meaning:

> **Controller Demand is REU's normalised interpretation of the connected controller's requested propulsion, where that interpretation is supported and currently valid.**

Typical conceptual range remains 0–100%, but it is not a claim that REU directly measures trigger travel.

For some controller technologies, reliable interpretation may depend on electrical loading or powerbase behaviour. Controller Demand may therefore be unsupported or unavailable even though transparent controller operation remains possible.

---

## 5. Compatibility is capability-specific

A controller/powerbase installation is not simply `compatible` or `incompatible` with every REU function.

In particular:

> **Transparent compatibility does not imply Mediated Human compatibility.**

A controller/interface may work normally through REU's transparent path while REU cannot reliably derive a valid Controller Demand or mediate the lane.

Likewise, successful mediated throttle control does not imply that braking, telemetry, motor observation or every other REU capability is available.

Compatibility shall therefore be represented in terms of the capabilities actually supported and available for the connected REU + controller/powerbase combination.

Conceptually, one installation might provide:

```text
Transparent operation       supported
Mediated human control      supported
Demand limiting             supported
Dynamic braking             unavailable/unsupported
Variable braking            unavailable/unsupported
```

Another might provide:

```text
Transparent operation       supported
Mediated human control      unsupported
```

Both can legitimately be usable REU installations; they simply expose different feature sets.

---

## 6. MUG feature-set principle

No MUG/REU installation is required to provide every capability defined by the REU architecture.

Formal product principle:

> **The features available in a MUG/REU installation are determined by the capabilities of the REU hardware together with the capabilities of the connected controller/powerbase interface. Capability must be discovered and reported explicitly; it must not be inferred merely from the MUG/REU product name, generation, connector type or the fact that the installation is generally compatible.**

Consequences:

- a two-wire controller is not inherently restricted to transparent/basic operation;
- a two-wire controller may support mediated throttle, demand limiting, response curves, acceleration limiting, smoothing, MINIMUG Assist, P&P-imposed drive constraints and future autonomous demand if REU can reliably interpret and mediate that interface;
- a separate third conductor is relevant to conventional three-wire braking, but is not a prerequisite for the ordinary mediated-throttle/MINIMUG feature family;
- a sophisticated three-wire electronic controller may in practice expose fewer mediated features than a simple two-wire resistive controller if its demand cannot be reliably interpreted;
- a future Mini, standard, Pro, revised board or other variant may expose different capability sets without changing the architecture;
- marketing/support terminology such as `MUG compatible` must not be taken to mean `every MUG feature supported`.

The Browser must present the capabilities actually available in the current installation rather than a fixed generic MUG feature list.

---

## 7. Capability dependencies

Capabilities may depend on other capabilities or valid states. Consumers must not infer those dependencies from product names.

Examples:

- MINIMUG-style mediated assistance requires valid Controller Demand, Mediated Human authority and controllable lane output;
- P&P temporary demand ceilings require REU to have electrical authority capable of enforcing them;
- conventional dynamic braking requires a verified electrical interface capable of producing that braking state;
- future RPM requires suitable motor-observation capability and valid observation data;
- future traction assistance would require sufficiently credible motor/vehicle observation as well as mediated output authority.

Capability discovery/availability must therefore be able to express the practical result of dependencies becoming unavailable at runtime.

---

## 8. Braking capability

Braking must remain truthful to the connected electrical interface.

- A verified three-wire POWER/TRACK/BRAKE interface may support genuine dynamic braking.
- A two-wire interface must not be advertised as supporting conventional three-wire dynamic braking unless that specific system provides another verified means.
- An unknown/proprietary interface carries no braking claim until electrically verified.

Future capability refinement may distinguish, for example:

- coast support;
- dynamic brake support;
- variable brake support.

A richer future braking capability must not silently redefine the meaning of an older simpler capability.

---

## 9. Electrical protection versus telemetry

Electrical protection is mandatory wherever REU actively drives a lane.

The architecture does **not** prescribe the protection mechanism. It may ultimately involve switching-device protection, fuse/polyfuse, dedicated current limiting, measured current, temperature protection or another electrically appropriate combination.

Precision telemetry is separate.

The architecture does not make precision voltage, current or watt measurement a mandatory V1 feature merely because local electrical protection is mandatory.

Therefore:

> **Protection mandatory; rich electrical telemetry capability-dependent.**

---

## 10. Passive fail-through

Passive ordinary-controller operation remains a strong design objective.

Where a **verified supported interface** permits it, loss of REU power should leave an electrical path functionally equivalent to ordinary controller→powerbase operation without firmware intervention.

The word `equivalent` is intentional. The architecture does not promise literally zero electrical effect from connectors, contacts, PCB tracks or protection components.

Passive fail-through must be verified interface by interface. The architecture does not promise electrically transparent fail-through for arbitrary analogue hardware merely because it can be physically connected.

---

## 11. Interface identification and protection state

The architecture shall permit REU to expose electrical truths including, where applicable:

### Interface identification state

Conceptually:

- known;
- unknown;
- incompatible;
- ambiguous.

### Bypass availability

REU may need to report whether passive bypass is actually electrically available, separately from current Control Authority.

### Protection state

Protection state is distinct from the demand/result it causes. Conceptually it may include:

- normal;
- limited;
- tripped/faulted.

A protection-imposed demand ceiling or no-drive result must not erase the fact that electrical protection caused it.

These are architectural electrical truths, not necessarily ordinary user-facing controls.

---

## 12. Lane count is not REU identity

The architecture must not equate `REU` with a fixed lane count.

The first production unit may have a chosen physical lane count, but capability/identity architecture must permit future devices such as:

- a one-lane Mini;
- a two-lane standard unit;
- a four-lane integrated unit;
- a future expandable/chained arrangement;

without redefining what an REU is.

Consumers discover the lane capabilities/resources actually exposed by the device.

---

## 13. Future BEMF/RPM provision

BEMF, RPM, estimated vehicle speed and traction assistance remain future/optional capabilities, not V1 product requirements.

Where effectively negligible in cost and complexity, V1 electrical design should avoid unnecessarily designing out future provision such as:

- ADC-capable MCU input per lane;
- PCB pads/space for protected TRACK-to-BRAKE/return sensing;
- access to relevant TRACK and BRAKE/return points;
- useful development test point;
- an output topology capable of producing a genuine COAST/high-impedance interval.

This is provision, not authorisation to add a precision analogue front end or BEMF-specific components to V1 merely for possible future use.

---

## 14. V1 compatibility proof

The architecture remains electrically plausible, but the eventual V1 compatibility catalogue must be established by interface-specific electrical proof.

The two most important proof areas identified by Rex are:

1. reliable Controller Demand sensing/normalisation across intended controller families;
2. truthful braking behaviour across intended interfaces.

V1 scope must therefore be chosen from electrically verified capability combinations rather than by attempting universal analogue-controller support.

---

## 15. Architectural conclusion

Rex's review does not alter the fundamental boundary:

> **P&P owns race meaning and race requirements. REU owns electrical truth and electrical execution.**

It strengthens the capability-driven model:

> **A MUG/REU is not defined by possessing every possible REU feature. Each installation truthfully exposes what its hardware and connected controller/powerbase combination can actually do.**

This preserves expansion to future REU/MUG variants without forcing V1 to implement future features and without pretending that every controller technology can support every function.
