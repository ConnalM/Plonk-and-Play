# Race Setup — Lane Assignment and Rotation Behaviour

**Status:** Accepted product/UI behaviour for implementation  
**Scope:** Race Setup presentation and ownership of pending lane assignments for configured multi-lane tracks.

## 1. Physical lanes and Race Entries

The current Track Configuration determines the physical lane count shown by Race Setup, up to the supported maximum.

Physical lanes remain fixed. What moves between lanes is the complete **Race Entry**: Racer plus optional Car and any other entry-owned identity required by the pending race setup.

A lane-rearrangement operation therefore changes the pending Race Entry-to-Lane assignment; it does not renumber or redefine the physical track lanes.

## 2. Race Entry panel scaling

Race Setup must remain pleasant for a normal two-lane track while also accommodating larger configured tracks, including up to eight lanes where supported.

Do not permanently squash two-lane rows merely to reserve space for eight lanes.

The Race Entries panel should use available vertical space intelligently:

- few lanes: generous row height;
- increasing lane count: progressively more compact rows;
- once a sensible minimum row height is reached: keep that row height and make the Race Entries area internally scrollable if the available viewport still cannot show all configured lanes.

The intent is adaptive row height first, scrolling only when useful. Exact pixel thresholds are presentation implementation details, not product semantics.

Only configured physical lanes are shown. A two-lane Track Configuration shows two lane rows, not empty rows for lanes 3–8.

## 3. Front-screen lane controls

Lane rearrangement is performed directly on the main Race Setup screen. It does **not** require a separate lane-swap pop-up or modal.

Above the track/lane list, Race Setup provides:

- **ROTATE LANES**;
- a nearby **Reverse direction** toggle;
- **Automatically rotate after each race**.

If an icon accompanies ROTATE LANES, a vertical double-arrow treatment is appropriate because the visible entries move up/down through the lane list. Do not use a horizontal double-arrow that suggests left/right swapping.

The track heading (for example **2-LANE TRACK** or **8-LANE TRACK**) and configured lane rows appear below these controls.

## 4. Manual drag and drop

Race Entries remain directly draggable on the main Race Setup screen.

- the user may drag a Racer/Race Entry from one lane to another;
- dropping onto an occupied lane exchanges/rearranges the complete entries;
- the operation must not create duplicate lane ownership;
- optional Car identity moves with its Race Entry.

Drag and drop provides arbitrary manual rearrangement. It coexists with ROTATE LANES rather than being replaced by it.

A separate **CHANGE** button on every lane row is not required. The Racer/Car area itself is the natural entry point for selecting or changing that Race Entry.

## 5. Manual rotation and direction

**ROTATE LANES** quickly moves all current Race Entries by one physical lane.

For N configured lanes, normal rotation is conceptually:

`1 → 2 → 3 → ... → N → 1`

**Reverse direction** reverses that mapping:

`1 → N → ... → 3 → 2 → 1`

For two lanes, either direction necessarily exchanges the two entries.

The direction is one P&P setting used consistently by both manual ROTATE LANES and automatic post-race rotation. Do not maintain contradictory browser-local direction settings.

Terminology matters:

- **drag/rearrange** describes arbitrary manual reassignment;
- **rotate/rotation** describes systematic movement of every Race Entry by one lane.

Do not call the multi-lane rotation behaviour “swap lanes”.

## 6. Automatic rotation

**Automatically rotate after each race** is race behaviour, not part of an individual lane row.

When enabled, after a properly completed race P&P rotates every Race Entry one physical lane when preparing the next pending race setup, using the same direction setting as manual ROTATE LANES.

An abandoned/restarted race does not trigger automatic rotation.

The completed race's Results continue to show the lanes actually used in that race. Rotation affects the pending setup for the next race; it does not rewrite the completed result.

The browser must not independently perform an automatic rotation merely because it is open.

## 7. Numeric race controls

Numeric Race Setup controls such as target laps should support efficient small and large changes without requiring repeated individual clicks.

For target laps:

- valid range is **1–999**;
- direct text entry is supported;
- keyboard Up/Down may increment/decrement;
- mouse wheel over the numeric control increments/decrements;
- a single press of `+` or `−` changes the value by one;
- holding `+` or `−` repeats the change;
- continued holding progressively accelerates the rate of change.

Acceleration should feel progressive rather than immediately jumping to a large step. Exact timing/rate curves are presentation implementation details and should be tuned by use.

This is a reusable P&P numeric-control behaviour. Other suitable numeric settings such as durations or run counts should use the same interaction pattern where appropriate.

## 8. Ownership and authority

The browser is the interface for choosing these options; it is not their authority.

Lane assignments, rotation direction and automatic-rotation state belong to P&P's authoritative **pending Race Setup / pending session proposal**.

Therefore:

- a browser requests a lane-assignment, rotation or setting change;
- P&P validates and owns the resulting pending configuration;
- the browser reads back and presents that authoritative pending state;
- multiple connected browsers must converge on the same pending assignments/settings rather than maintaining private local versions;
- reload/reconnect must reconstruct the same pending state from P&P;
- START validates and commits that single authoritative proposal into the immutable Session Definition.

Once START is accepted, the active Session Definition's lane assignments are frozen for that session. A browser must not casually mutate the active race by editing the next/pending setup.

This requirement is intentionally consistent with the pending-session ownership cleanup: lane-management UI must not recreate Browser-local proposal state.

## 9. Availability and disabling

Lane rearrangement and rotation controls are available only when P&P says the pending setup can be edited.

If the relevant configuration is frozen or changing it is not currently permitted, the browser disables the controls rather than making a local change that Race Control will later reject.

## 10. Presentation principle

The ordinary Race Setup remains simple and direct:

- ROTATE LANES, Reverse direction and automatic rotation are visible above the configured track/lane list;
- configured lane rows show Racer and optional Car;
- entries can be dragged directly between fixed physical lanes;
- race-defining settings such as laps and finish behaviour remain alongside the entries;
- More Race Options is reserved for other secondary race behaviour;
- the permanent READY / START contract remains intact.

> **The lane stays put; the Race Entry moves. The browser requests; P&P owns.**
