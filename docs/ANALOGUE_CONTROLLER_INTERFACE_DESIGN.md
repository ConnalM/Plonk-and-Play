# P&P Analogue Controller Interface Design

**Status:** Recovered design record — current product-owner decisions and previously agreed interface principles  
**Scope:** P&P analogue controller/powerbase interface only. REU integration is deliberately excluded until this baseline is recorded and reviewed.  
**Date recovered:** 22 October 2026

## 1. Purpose

This document records the analogue controller-interface decisions made during P&P design discussions but not previously captured in the repository.

It distinguishes:

- **DECIDED** — accepted product/design decision.
- **WORKING ASSUMPTION** — design direction used in discussion but not yet sufficiently specified to freeze implementation.
- **OPEN** — intentionally unresolved and must not be invented by implementation.

This is the baseline to be understood before designing REU integration.

## 2. Product principle

### DECIDED

P&P is intended to work with ordinary analogue slot-car equipment while retaining the user's existing controllers, powerbase/track power supply and cars wherever practical.

P&P must not require a user to open or modify a powerbase merely to use the system.

The analogue controller interface must support systems that already use the P&P standard connector directly and systems that require a simple conversion/adaptor arrangement.

P&P's current product design includes an analogue controller/powerbase interface as part of standalone P&P operation. The later REU concept must not make that existing P&P function dependent on an REU.

## 3. P&P standard physical connector

### DECIDED

The P&P standard analogue controller interface is **3.5 mm TRS**.

For the supported native three-wire TRS families investigated:

- **Ring = TRACK**.
- **Tip and Sleeve = POWER and BRAKE/0 V**, with the orientation of those two contacts varying between manufacturers/systems.

The user must not normally be expected to know or select the manufacturer's Tip/Sleeve polarity convention.

Automatic identification of the two outer-contact functions/polarity is the accepted design direction for supported TRS families.

Examples investigated as native/compatible TRS-type systems include Scalextric Sport and Ninco. Policar has also been treated as an out-of-box TRS candidate in the hardware discussions. Final production compatibility must be verified electrically rather than inferred merely from connector shape.

## 4. Three wires do not always mean the same thing

### DECIDED

A three-contact connector must not automatically be interpreted as POWER/TRACK/BRAKE merely because it has three conductors.

For example, Carrera GO was identified as a case in which the additional controller function can be **Turbo**, rather than a conventional brake function.

Therefore connector shape and conductor count alone are not a sufficient compatibility definition.

## 5. Two-wire controllers

### DECIDED

Two-wire analogue controllers are a separate supported interface family in principle.

They provide the equivalent of:

- POWER
- TRACK

They do **not** acquire a genuine third-wire braking function merely by being adapted to a TRS connector.

Where a two-wire controller is presented through the P&P standard TRS physical interface, the unused contact remains functionally absent. P&P must respect the capabilities actually present rather than assume three-wire braking.

### OPEN

The exact production electrical detection/identification circuit for distinguishing supported two-wire arrangements from three-wire arrangements is not frozen in this document.

## 6. Non-TRS controllers — agreed cable-conversion method

### DECIDED

Where a controller/powerbase uses a proprietary or otherwise unsuitable connector and no clean plug-in interposer is available, the agreed retrofit method is a **single cut in the controller cable**, creating a standard P&P insertion point.

Original arrangement:

```text
Controller ---------------- original cable ---------------- original plug -> Powerbase
```

After conversion:

```text
Controller -- cable -- [bespoke TRS male]
                              |
                         direct mating
                              |
                     [bespoke TRS female] -- cable -- original plug -> Powerbase
```

The male and female are deliberately complementary so that, with P&P absent, they plug directly into one another and restore the original controller-to-powerbase circuit.

With P&P inserted, the standardised connection becomes conceptually:

```text
Controller -- converted TRS interface -> P&P -> converted TRS interface -- original plug -> Powerbase
```

This means the one-time cable conversion does **not** make the controller dependent on P&P. Remove P&P, reconnect the complementary male/female pair, and the controller/powerbase works in its original form.

## 7. Bespoke conversion terminations

### DECIDED / DESIGN DIRECTION

The non-TRS conversion was not intended to be a loose DIY splice or require the user to solder miniature jack contacts.

The discussed solution is a **bespoke covered male/female TRS termination** incorporating:

- labelled conductor positions;
- push-in/spring-clamp style conductor termination;
- proper insulation/covering;
- cable strain relief;
- complementary male/female physical interfaces.

The user would make the single cable cut, strip the required conductors and secure them into the labelled terminals.

No satisfactory off-the-shelf part matching the desired arrangement was identified in the discussion; a manufacturable bespoke termination was therefore the working direction.

### OPEN

The exact mechanical design, enclosure, spring terminal part and production assembly are not yet frozen.

## 8. Manufacturer-specific adaptors/interposers

### DECIDED

Cable cutting is not mandatory where a cleaner reversible adaptor can be provided.

For systems whose existing connectors permit it, manufacturer/system-specific adaptor or interposer leads may convert the existing controller connection to the standard P&P interface without modifying the user's equipment.

The preference is:

1. native standard TRS where electrically compatible;
2. reversible manufacturer-specific adaptor/interposer where practical;
3. the agreed one-cut complementary male/female conversion where necessary.

The objective is compatibility without requiring modification of the powerbase or replacement of the user's controller.

## 9. Electrical identification and polarity

### DECIDED

For supported native three-wire TRS families:

- TRACK is treated as the known Ring function;
- the remaining two contacts must be identified as POWER versus BRAKE/0 V without asking the ordinary user to understand manufacturer wiring conventions.

Direction reversal of the slot-car system was explicitly outside the immediate interface design scope.

### WORKING ASSUMPTION

Automatic electrical identification is preferable to a manufacturer selector in the user interface.

### OPEN

The final sensing/protection circuit and complete list of certified wiring families remain hardware-design work. Arbitrary unknown wire orders must not be assumed safely auto-identifiable merely because known supported families can be identified.

## 10. Pass-through and independence

### DECIDED

The standardised controller connection must preserve ordinary controller use when P&P is removed.

For a converted non-TRS controller, this is achieved physically by reconnecting the complementary male and female conversion terminations.

P&P must not turn a converted controller into proprietary equipment that can only be used with P&P attached.

## 11. P&P timing and controller interface

### DECIDED

P&P's authoritative lap/sector crossing information comes from its timing detectors (for example the per-lane ToF start/finish sensors in the current design).

The analogue controller/powerbase interface is nevertheless part of the current standalone P&P product design and must be preserved as such; REU must not become a prerequisite for it.

An earlier September design assumption described electrical track-power control as an optional Power Module while timing detectors alone performed lap counting. Subsequent controller-interface design work and the product owner's current clarification supersede treating the analogue interface as something that may simply be designed out of the standalone P&P hardware.

### OPEN

This recovered record does **not** invent the final P&P analogue sensing/switching schematic. The exact electrical functions performed by the current P&P main-unit interface, beyond the accepted physical/interface requirements above, need to be reconciled with the hardware design before schematic freeze.

## 12. What is deliberately NOT decided here

This document does not decide:

- REU placement relative to P&P;
- P&P-to-REU communications transport;
- REU PWM/MOSFET topology;
- BEMF sensing;
- future Ghost electrical control;
- the final P&P analogue-interface circuit;
- production protection/hot-plug components;
- a universal automatic mapping algorithm for arbitrary unknown controllers.

Those questions must be considered only after this existing P&P interface baseline is accepted.

## 13. Compatibility rule

A system is not declared compatible solely because a plug physically fits.

Production compatibility must account for at least:

- conductor functions;
- voltage and polarity;
- two-wire versus three-wire behaviour;
- braking/turbo/other third-wire semantics;
- controller technology;
- current requirements;
- safe pass-through and disconnection behaviour where applicable.

## 14. Baseline diagrams

### Native compatible TRS system

```text
Controller -- 3.5 mm TRS --> P&P -- 3.5 mm TRS --> existing powerbase
```

### Converted non-TRS system, P&P absent

```text
Controller -- cable -- [TRS male] <-> [TRS female] -- cable -- original plug --> Powerbase
```

### Converted non-TRS system, P&P present

```text
Controller -- cable -- [TRS male] --> P&P --> [TRS female] -- cable -- original plug --> Powerbase
```

### Two-wire controller

```text
POWER -------------------------------
TRACK -------------------------------
third TRS contact: no invented brake function
```

## 15. Design guardrail for subsequent REU work

Before choosing how P&P and REU are physically chained or how they communicate, the combined design must preserve all of the following:

1. P&P remains useful and complete without REU.
2. The existing P&P analogue interface is not silently migrated into REU.
3. REU remains independently useful without P&P.
4. Native compatible controllers remain simple to connect.
5. Converted non-TRS controllers remain usable with neither P&P nor REU present by reconnecting their complementary conversion pair.
6. Two-wire controllers are never falsely treated as three-wire braking controllers.
7. Manufacturer wiring differences are handled by hardware/interface design, not by requiring the ordinary user to understand pin assignments.

Only after these constraints are preserved should the combined P&P + REU topology be selected.
