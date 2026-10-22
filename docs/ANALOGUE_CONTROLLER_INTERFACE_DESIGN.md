# REU Analogue Controller Interface Design

**Status:** Recovered design record — current product-owner decisions and previously agreed interface principles  
**Scope:** Analogue controller/powerbase interface for the optional REU. The core P&P unit does not require this interface.  
**Date recovered/corrected:** 22 October 2026

## 1. Purpose

This document records the analogue controller-interface work developed for the optional electrical-control module that evolved into the REU.

A design-history correction is important: during discussion, the interface was sometimes described loosely as a "P&P interface" because REU is part of the wider P&P system. That wording must not be interpreted as requiring the core P&P box itself to sit in the controller circuit.

The product boundary is:

> **P&P observes and understands the race. REU electrically influences the cars.**

It distinguishes:

- **DECIDED** — accepted product/design decision.
- **WORKING ASSUMPTION** — design direction used in discussion but not yet sufficiently specified to freeze implementation.
- **OPEN** — intentionally unresolved and must not be invented by implementation.

## 2. Core P&P boundary

### DECIDED

Core P&P is fundamentally a sensor-based lap counter/race system.

Its authoritative lap/sector crossings come from timing detectors, currently including the per-lane ToF start/finish sensors. P&P does not require an electrical connection between the hand controller and powerbase in order to count laps, time races, classify competitors or maintain results.

Core P&P has **no track-power-control requirement**. Throttle manipulation, power limiting, braking, electrical start lockout and similar electrical intervention are not core P&P functions.

Therefore the core P&P box does **not** require the 3.5 mm TRS controller/powerbase interface described below.

This restores the intended Plonk & Play principle: a customer can use the core P&P race system without modifying, adapting or passing the hand-controller wiring through the P&P main unit.

## 3. Relationship to the earlier optional Power Module

### DECIDED / DESIGN HISTORY

Earlier P&P architecture allowed an optional Power Module/output capability to provide electrical track-control functions without making them prerequisites for P&P.

The later REU is the much more capable descendant of that optional electrical-control concept. The detailed controller-interface work — TRS, polarity handling, two-wire compatibility and proprietary-controller conversion — belongs to REU/electrical-control hardware, not to the core P&P box.

References during design discussion to a "P&P controller interface" mean an interface belonging to the wider P&P product ecosystem. They do not establish a requirement for that circuitry in the P&P main unit.

## 4. REU standard physical connector

### DECIDED

The standard analogue controller interface developed for REU is **3.5 mm TRS**.

For the supported native three-wire TRS families investigated:

- **Ring = TRACK**.
- **Tip and Sleeve = POWER and BRAKE/0 V**, with the orientation of those two contacts varying between manufacturers/systems.

The user must not normally be expected to know or select the manufacturer's Tip/Sleeve polarity convention.

Automatic identification of the two outer-contact functions/polarity is the accepted design direction for supported TRS families.

Examples investigated as native/compatible TRS-type systems include Scalextric Sport and Ninco. Policar has also been treated as an out-of-box TRS candidate in the hardware discussions. Final production compatibility must be verified electrically rather than inferred merely from connector shape.

## 5. Three wires do not always mean the same thing

### DECIDED

A three-contact connector must not automatically be interpreted as POWER/TRACK/BRAKE merely because it has three conductors.

For example, Carrera GO was identified as a case in which the additional controller function can be **Turbo**, rather than a conventional brake function.

Therefore connector shape and conductor count alone are not a sufficient compatibility definition.

## 6. Two-wire controllers

### DECIDED

Two-wire analogue controllers are a separate supported REU interface family in principle.

They provide the equivalent of:

- POWER
- TRACK

They do **not** acquire a genuine third-wire braking function merely by being adapted to a TRS connector.

Where a two-wire controller is presented through the standard REU TRS physical interface, the unused contact remains functionally absent. REU must respect the capabilities actually present rather than assume three-wire braking.

### OPEN

The exact production electrical detection/identification circuit for distinguishing supported two-wire arrangements from three-wire arrangements is not frozen in this document.

## 7. Non-TRS controllers — agreed cable-conversion method

### DECIDED

Where a controller/powerbase uses a proprietary or otherwise unsuitable connector and no clean plug-in interposer is available, the agreed retrofit method is a **single cut in the controller cable**, creating a standard REU insertion point.

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

The male and female are deliberately complementary so that, with REU absent, they plug directly into one another and restore the original controller-to-powerbase circuit.

With REU inserted:

```text
Controller -- converted TRS interface -> REU -> converted TRS interface -- original plug -> Powerbase
```

This means the one-time cable conversion does **not** make the controller dependent on REU. Remove REU, reconnect the complementary male/female pair, and the controller/powerbase works in its original form.

## 8. Bespoke conversion terminations

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

## 9. Manufacturer-specific adaptors/interposers

### DECIDED

Cable cutting is not mandatory where a cleaner reversible adaptor can be provided.

For systems whose existing connectors permit it, manufacturer/system-specific adaptor or interposer leads may convert the existing controller connection to the standard REU interface without modifying the user's equipment.

The preference is:

1. native standard TRS where electrically compatible;
2. reversible manufacturer-specific adaptor/interposer where practical;
3. the agreed one-cut complementary male/female conversion where necessary.

The objective is compatibility without requiring modification of the powerbase or replacement of the user's controller.

## 10. Electrical identification and polarity

### DECIDED

For supported native three-wire TRS families:

- TRACK is treated as the known Ring function;
- the remaining two contacts must be identified as POWER versus BRAKE/0 V without asking the ordinary user to understand manufacturer wiring conventions.

Direction reversal of the slot-car system was explicitly outside the immediate interface design scope.

### WORKING ASSUMPTION

Automatic electrical identification is preferable to a manufacturer selector in the user interface.

### OPEN

The final sensing/protection circuit and complete list of certified wiring families remain REU hardware-design work. Arbitrary unknown wire orders must not be assumed safely auto-identifiable merely because known supported families can be identified.

## 11. Pass-through and independence

### DECIDED

The standardised controller connection must preserve ordinary controller use when REU is removed.

For a converted non-TRS controller, this is achieved physically by reconnecting the complementary male and female conversion terminations.

REU must not turn a converted controller into proprietary equipment that can only be used with REU attached.

REU itself must also remain useful independently of P&P. Conversely, P&P remains a complete lap/race system without REU.

## 12. What is deliberately NOT decided here

This document does not decide:

- P&P-to-REU communications transport;
- REU PWM/MOSFET topology;
- BEMF sensing;
- future Ghost electrical control;
- the final REU analogue-interface circuit;
- production protection/hot-plug components;
- a universal automatic mapping algorithm for arbitrary unknown controllers.

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

### Core P&P — no analogue controller interface

```text
ToF timing detectors -> P&P -> lap timing / race state / results

Controller -------------------------------> Powerbase -> Track
```

### Native compatible TRS system with standalone REU

```text
Controller -- 3.5 mm TRS --> REU -- 3.5 mm TRS --> existing powerbase
```

### Converted non-TRS system, REU absent

```text
Controller -- cable -- [TRS male] <-> [TRS female] -- cable -- original plug --> Powerbase
```

### Converted non-TRS system, REU present

```text
Controller -- cable -- [TRS male] --> REU --> [TRS female] -- cable -- original plug --> Powerbase
```

### Two-wire controller

```text
POWER -------------------------------
TRACK -------------------------------
third TRS contact: no invented brake function
```

## 15. Design guardrail for subsequent REU work

The combined product architecture must preserve all of the following:

1. **P&P works completely without REU** and does not require controller/powerbase analogue wiring.
2. **REU works without P&P** for its standalone electrical-control functions.
3. Controller/TRS/polarity/pass-through circuitry belongs to REU, not the core P&P box.
4. Native compatible controllers remain simple to connect to REU.
5. Converted non-TRS controllers remain usable with neither P&P nor REU present by reconnecting their complementary conversion pair.
6. Two-wire controllers are never falsely treated as three-wire braking controllers.
7. Manufacturer wiring differences are handled by REU hardware/interface design, not by requiring the ordinary user to understand pin assignments.
8. Adding REU capability must not make the base P&P product less Plonk & Play.
