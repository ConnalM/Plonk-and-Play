# Plonk & Play™ — Configuration and Identity Specification

**Status:** Committed product-design specification  
**Scope:** Basic v1 MUG, car and lane configuration.  
**Principle:** Identity enhances P&P; it must never be required to use P&P.

## 1. Immediate use without setup

P&P must remain genuinely Plonk & Play.

A new/default two-lane system can operate as:

- **Lane 1 — MUG 1**
- **Lane 2 — MUG 2**

The user does not have to create profiles, cars or a database before racing.

## 2. Persistent MUG identity

A saved MUG has:

- a permanent internal ID;
- a user-visible name.

For v1, **Name is the only required profile field**.

The internal ID, not the displayed name, owns historical identity. Renaming a MUG must therefore not orphan or split their previous results.

Richer profile information may be added later without changing this identity model.

## 3. Selecting MUGs

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

## 4. Text-entry Clear control

Where clearing an editable text value is a sensible operation, P&P should provide a one-action **Clear** control rather than requiring repeated backspace/delete actions.

This applies to MUG-name entry and should be treated as a general UI principle.

## 5. Guest MUGs

A Guest can race without being added permanently to the saved MUG list.

Guest name is optional:

- blank → display **Guest**;
- entered, e.g. **Dave** → use Dave for the current race/session without automatically creating a permanent profile.

A temporary named Guest can be promoted directly to a permanent saved MUG without re-entering the name.

This may be offered while entering the Guest name (for example **Remember Dave**) and/or after the race (**Remember this MUG**).

General principle:

> Temporary identity data should be promotable to permanent identity without requiring re-entry.

## 6. Cars

Car recording is optional and should not clutter the normal setup screen by default.

Cars are independent entities. A car does **not** belong permanently to a particular MUG.

When car recording is enabled, a race entry may reference an optional saved car.

Therefore the conceptual relationship is:

**Race Entry = MUG + Lane + optional Car**

The same car may be driven by different MUGs in different races/heats.

This supports shared cars, including groups of younger/informal racers, without distorting the data model.

For v1, if car profiles are exposed, a simple user-visible car name is sufficient. Manufacturer/model/year/catalogue-style data is not required.

## 7. Lane assignment

For a normal two-lane Lap setup, the presentation should remain approximately:

**LAP RACE — 10 laps**

- **Lane 1 — Connal**
- **Lane 2 — Hazel**

**START**

Changing a MUG should normally require only selecting a different name from the lane dropdown.

## 8. Swap Lanes

Provide a quick **SWAP LANES** action.

This exchanges the complete lane entries, including the MUG and optional selected car where car recording is enabled.

Example:

**Connal — Lane 1 | Hazel — Lane 2**

becomes:

**Hazel — Lane 1 | Connal — Lane 2**

This provides useful manual lane rotation in v1 without requiring a full Competition Mode.

## 9. Remembering the last setup

P&P automatically remembers the most recently used straightforward race setup, including MUG/lane assignment and normal race settings.

Returning to the same mode should therefore normally present the previous useful setup, allowing regular users to reach START with minimal interaction.

A separate Save Setup / Load Setup system is not required for v1.

Named presets may be added later if real use demonstrates a need.

## 10. Future compatibility without v1 scope creep

The first implementation provides only the configuration and identity features needed for straightforward racing.

Its data model and interfaces must not unnecessarily prevent later support for:

- richer MUG profiles;
- richer car profiles;
- long-term MUG/car statistics;
- multi-race competitions;
- automatic lane rotation;
- heats/rounds;
- other event structures.

These are **not v1 requirements** merely because the data model leaves room for them.

The design must not assume that:

- one MUG permanently belongs to one lane;
- one car permanently belongs to one MUG;
- a race must always be completely standalone.

> **Design broadly. Implement narrowly.**
