# Race Setup — Lane Assignment and Swap Behaviour

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

## 3. Main SWAP LANES action

Race Setup provides an unobtrusive **SWAP LANES** action near the Race Setup heading/instruction area rather than giving it a large permanent block beneath the lane list.

The label remains **SWAP LANES** for both two-lane and multi-lane tracks. Pressing it opens a lane-rearrangement panel/pop-up.

## 4. Lane-rearrangement panel

The panel shows the configured physical lanes as fixed destinations and the currently assigned Race Entries within them.

The normal manual interaction is drag/rearrange:

- the user drags a Racer/Race Entry from one lane to another;
- dropping onto an occupied lane swaps/rearranges the complete entries;
- the operation must not create duplicate lane ownership;
- optional Car identity moves with its Race Entry.

The panel may provide **DONE** or equivalent to close the rearrangement interaction. This is not a second authoritative save boundary: accepted changes are changes to the pending Race Setup.

## 5. Rotate and direction

The SWAP LANES panel also provides a **ROTATE** action for quickly moving all current Race Entries by one physical lane.

For N configured lanes, normal rotation is conceptually:

`1 → 2 → 3 → ... → N → 1`

A **Reverse swap direction** checkbox reverses that mapping:

`1 → N → ... → 3 → 2 → 1`

For two lanes, either direction produces the familiar two-entry swap.

The chosen swap direction is one P&P setting used consistently by both manual ROTATE and automatic post-race lane rotation. Do not maintain contradictory browser-local direction settings.

## 6. Automatic swapping/rotation

**Auto-swap after each race** is race behaviour, not part of an individual lane row. Present it under the appropriate Race Options area rather than beneath the Race Entry list.

When enabled, P&P uses the same defined swap direction described above when preparing the appropriate next race/setup.

The browser must not independently perform an automatic rotation merely because it is open.

## 7. Ownership and authority

The browser is the interface for choosing these options; it is not their authority.

Lane assignments, swap direction and auto-swap state belong to P&P's authoritative **pending Race Setup / pending session proposal**.

Therefore:

- a browser requests a lane-assignment or swap-setting change;
- P&P validates and owns the resulting pending configuration;
- the browser reads back and presents that authoritative pending state;
- multiple connected browsers must converge on the same pending assignments/settings rather than maintaining private local versions;
- reload/reconnect must reconstruct the same pending state from P&P;
- START validates and commits that single authoritative proposal into the immutable Session Definition.

Once START is accepted, the active Session Definition's lane assignments are frozen for that session. A browser must not casually mutate the active race by editing the next/pending setup.

This requirement is intentionally consistent with the pending-session ownership cleanup: lane-swap UI must not recreate Browser-local proposal state.

## 8. Availability and disabling

SWAP LANES / rearrangement controls are available only when P&P says the pending setup can be edited.

If the relevant configuration is frozen or changing it is not currently permitted, the browser disables the action rather than making a local change that Race Control will later reject.

## 9. Presentation principle

The ordinary Race Setup remains simple:

- configured lane rows with Racer and optional Car;
- a small SWAP LANES action;
- race-defining settings such as laps and finish behaviour;
- More Race Options for secondary race behaviour, including Auto-swap;
- the permanent READY / START contract.

The lane-rearrangement panel hides the richer multi-lane manipulation until the user asks for it.

> **The lane stays put; the Race Entry moves. The browser requests; P&P owns.**
