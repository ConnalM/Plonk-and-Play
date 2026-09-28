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


## 11. Track configuration

The v1 product assumes one remembered physical Track Configuration rather than requiring a named multi-track database.

Normal use must not require the MUG to create or name a track before racing.

The Track Configuration contains information P&P actually uses, including:

- configured lane count;
- sensor assignments/roles;
- Start/Finish arrangement;
- optional track length;
- optional track scale.

Track length enables appropriate speed calculations. Track scale enables scale-equivalent speed calculations.

The underlying design must not unnecessarily prevent multiple saved physical Track Configurations being added later.

### 11.1 Rally stages are not tracks

A Rally Stage is an event/run definition layered on top of the current physical Track Configuration; it is not another physical track record.

Stages may differ in how the installation is used, for example:

- loop, 2 laps;
- loop, 3 laps;
- A→B;
- B→A.

Stages may optionally be given user-visible names.

## 12. Configuration presentation

P&P separates **what are we doing now?** from **what is this installation?**

### 12.1 Normal race screen

The ordinary race screen contains the small number of settings commonly needed for the imminent race.

For Lap Race this may include:

- MUG/lane assignments;
- target laps;
- finish behaviour;
- Swap Lanes;
- START.

It also provides unobtrusive access to **More Race Options** and **Setup**.

### 12.2 More Race Options

More Race Options contains settings that may genuinely vary from race to race but need not clutter the normal screen.

Examples include:

- start-light count;
- GO style;
- final-delay style;
- false-start response;
- audio choice.

An **Advanced** area may contain less-common detailed parameters such as exact random-delay limits or red-light interval.

### 12.3 Setup

Setup describes the P&P installation rather than today's race.

Logical areas include:

- **Track** — length, scale, Start/Finish arrangement;
- **Hardware** — detected sensors/modules, assignments and optional hardware;
- **MUGs** — management of saved identities;
- **Cars** — only where optional car recording is enabled;
- **Sound & Display**;
- **System** — networking, update, backup/restore, diagnostics and similar system functions.

Normal MUG creation/selection remains available directly from the race screen; a MUG-management area must not become a prerequisite to racing.

## 13. Save and confirmation behaviour

P&P should not require routine SAVE/APPLY interaction when the user's intention is already unambiguous.

Race settings become the current/remembered race configuration as they are changed.

Installation settings are stored when changed/confirmed.

Confirmation is reserved for actions that are destructive, disruptive or genuinely ambiguous, such as deleting persistent information, materially reassigning hardware or performing a factory reset.

> **Save automatically where intention is unambiguous. Confirm where an action is destructive, disruptive or ambiguous.**


## 14. Out-of-box base configuration

The base product should require **zero software configuration** when installed according to the recommended quick-start arrangement.

Factory/default base assignments are:

- Sensor 1 → Lane 1 Start/Finish;
- Sensor 2 → Lane 2 Start/Finish;
- recommended/default physical sensor arrangement → sensor before the Start/Finish line.

The Quick Setup documentation must show the matching physical arrangement clearly.

With this default physical arrangement, straightforward lap counting works immediately. Capabilities that require a different sensor relationship, including applicable false-start detection and reaction timing, are not available merely by pretending the default installation can measure them.

Those unavailable capabilities may remain visible but greyed/locked with an explanation of the physical/configuration change required to enable them.

A MUG who installs the sensors differently can change the remembered arrangement later under Track Setup. A clear diagram should be used rather than relying only on technical wording.

Optional information such as track length, track scale and MUG names must not become first-boot requirements.

## 15. MUGGLE quick-setup guide

The base product requires a short, highly visual **MUGGLE Guide** for first use.

Its purpose is not to replace the full manual. Its job is to get an ordinary MUG from unopened/unconfigured product to a working basic race with the fewest possible decisions.

The intended flow is approximately:

1. **PLONK** — position the two sensors exactly as illustrated in the recommended/default arrangement.
2. **PLUG** — connect Sensor 1/Lane 1 and Sensor 2/Lane 2 and power P&P.
3. **CONNECT** — connect a phone/tablet/computer to P&P and open its browser interface.
4. **PLAY** — choose the basic race setting such as laps and press START.

A small **Want more?** section may point towards optional MUG names, track length/scale, alternative sensor arrangements, enhanced start/false-start functions, Rally, Drag and other capabilities.

Product requirement:

> **If the MUG follows the MUGGLE Guide's recommended physical installation, the software defaults must match it and no Setup step should be required before basic racing.**
