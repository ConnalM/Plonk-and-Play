# Plonk & Play™ Design Constitution

**Version 1.0**

The Plonk & Play™ principle is paramount. P&P is intended to make sophisticated race timing and control simple for the customer without achieving that simplicity by compromising the underlying architecture.

## 1. Plonk & Play™

Customer simplicity comes first.

Adding, removing or replacing a supported P&P component should require the minimum possible technical knowledge or configuration. Wherever practical, the system should determine information automatically and ask the customer only for information it genuinely cannot determine.

If a design makes P&P harder for the customer to install, expand or use, there must be a good reason for it.

## 2. Modular by Design

P&P is a system of interoperable modules, not a collection of unrelated products.

Modules communicate through clearly defined interfaces. A module should be capable of being added, replaced or upgraded without requiring unrelated parts of the system to be redesigned.

What a module does must be separated from how it is physically implemented or connected.

The Race Engine, for example, should not need to know whether a timing event originated from a wired VL sensor, an ESP-NOW wireless module, a future sensor technology or the test system.

## 3. Discover, Don't Configure

Supported P&P equipment should identify itself, its capabilities and its software/firmware version automatically wherever technically practical.

The controller should maintain a registry of known modules and remember their configuration and assigned roles.

The customer should only be asked for information that cannot reasonably be discovered automatically, such as whether a newly installed timing bridge has physically been placed at Start/Finish or at a sector point.

The communication method — wired, ESP-NOW or a future transport — should not define the identity of the module.

## 4. One Authoritative Race Engine

The P&P controller owns the race.

It is the sole authority for official timing, race state, positions and results. Sensors and modules report events. Displays and control devices observe the race and issue commands. They do not independently calculate or maintain official race state.

Closing a browser, losing Wi-Fi or disconnecting a display must not stop or corrupt an active race.

P&P will use a common system timebase. Events should be timestamped as close as practical to the point at which they are detected and expressed in, or reliably convertible to, P&P system time.

Communication latency must not determine race timing.

## 5. Testability Is Built In

Testing and diagnostics are permanent parts of the P&P architecture, not temporary development code.

Important system functions should be testable without requiring the corresponding physical hardware.

Test inputs must use the same defined interfaces as real inputs. Once a simulated sensor crossing has become a standard P&P sensor event, downstream components should process it in the same way as a genuine crossing.

The built-in test system should support manual event generation, automated race scenarios and, where useful, fault simulation.

The same diagnostic architecture may also be used for customer troubleshooting.

The test system is a protected architectural component. It must not be bypassed, duplicated, removed or replaced merely to simplify implementation.

## 6. Fail, Recover and Update Gracefully

P&P should tolerate individual component and communication failures without unnecessarily bringing down the rest of the system.

Configuration, module identities and assignments should persist across normal power cycles and restarts.

Known modules should reconnect automatically wherever practical.

Controller firmware, and intelligent-module firmware where appropriate, should be capable of being updated without requiring the customer to use development tools or reprogram devices manually.

OTA updating, version compatibility and recovery from interrupted or failed updates must therefore be considered in the architecture from the outset.

## 7. Protect the Architecture

Each important responsibility has one defined owner.

Race state belongs to the Race Engine. Hardware detection belongs to the appropriate hardware/input layer. Module identity and configuration belong to the appropriate configuration/registry system. Presentation belongs to the presentation layer.

Implementation details must not leak unnecessarily across those boundaries.

Interfaces and module responsibilities must not be changed merely because a programmer or AI coding tool believes it has found a more convenient implementation.

Architectural changes are explicit design decisions.

**Code conforms to the architecture. The code does not quietly redefine the architecture.**

---

## Governing Principle

When competing designs are technically viable, the first question is:

**Does this make the system more Plonk & Play™ for the customer, or less?**

Customer simplicity is the governing principle, supported by modularity, automatic discovery, authoritative timing, built-in testability, graceful recovery and disciplined architecture.
