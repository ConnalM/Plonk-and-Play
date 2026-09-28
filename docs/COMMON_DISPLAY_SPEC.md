# Plonk & Play™ — Common Display Specification

**Status:** Committed product-design specification  
**Applies to:** Lap, Endurance and Timed Stage  
**Exception:** Drag uses its own display design.

## 1. Design principle

P&P should scale from a solo MUG using one browser to a full competition using a main display, individual MUG screens and a separate Race Director screen.

The simple/default experience must remain non-threatening. Advanced functionality may be available without cluttering normal operation.

## 2. Full Browser Race Display

The main display is intended for a monitor, TV, projector, tablet or full-screen browser.

The race display remains fundamentally one screen with changing states rather than changing pages during a race:

`READY → STARTING → RACING → FINISHING → FINISHED / RESULTS`

The start sequence does not replace the scoreboard with a separate start screen.

## 3. Race Control Pod

A permanent segmented area sits across the top of the full display, envisaged as a shallow/squashed half-hexagonal pod.

It contains five light positions, which remain visible but greyed when inactive.

During a start these become the race start lights. The system can use either three or five according to the selected start configuration.

During normal racing the lights are dormant but can be repurposed for race-control signals:

- **Flashing Yellow lights** — Track Call / Yellow active.
- **Flashing Red lights** — Race stopped / Red active.

The surrounding pod area is an independent display channel. For example, if a MUG requests a Yellow rather than calling one directly, the surround can pulse yellow while the five lights remain in their normal state.

Similarly, a false start can illuminate the surround without interfering with a start-light sequence already in progress.

Start/race-control lighting always takes priority over ordinary informational messages.

The pod may also display temporary messages such as:

- `FASTEST LAP`
- `FINAL LAP`
- `PENALTY`
- `FINISHED`

Exact visual treatments remain for UI prototyping.

## 4. Main Scoreboard

The main body consists of one row for each active lane/MUG.

Live race rows remain in fixed lane order:

- Lane 1
- Lane 2
- Lane 3
- Lane 4
- and so on if expansion hardware supports additional lanes.

Rows do not move when race positions change. Instead, the MUG's current `P1`, `P2`, etc. changes prominently within the permanent lane row.

This lets a MUG glance repeatedly at the same physical part of the display.

A typical Lap row contains:

**Position | Lane/MUG | Race Progress | Last Lap | Best Lap | Gap**

Race progress for a 25-lap race might display `17 / 25`.

A thin graphical progress indicator may also show race completion visually. This should be prototyped rather than considered mandatory at this stage.

Endurance and Timed Stage use the same visual architecture with mode-appropriate information.

## 5. Temporary Row States

The geometry of a lane row remains stable, but information can temporarily change emphasis.

Examples include:

- **Position change** — new position briefly emphasised.
- **Fastest lap** — relevant timing briefly highlighted.
- **Penalty** — warning treatment followed by a persistent penalty indicator where appropriate.
- **Final lap** — progress area receives a distinctive treatment.
- **Finished** — row changes into its finished state.

A finished MUG remains in its original lane row while other MUGs continue racing.

Example:

`LANE 2 | CONNAL | FINISHED | P1 | 25/25 | 3:42.18 | BEST 8.19`

This is necessary because some Lap finish rules allow other MUGs to continue after the winner has finished.

## 6. Track Length, Scale and Speed

Track Setup can optionally store:

- **Track length** — metric or imperial.
- **Track scale** — common preset scales plus Custom.

These settings are optional and stored with the track rather than entered for every race.

Where available, P&P can calculate:

- actual model average speed;
- scale-equivalent average speed.

Speed can be calculated for individual laps, stages and complete races where meaningful.

It need not clutter the default live scoreboard. It can instead appear in results, detailed timing and selected temporary announcements such as:

`FASTEST LAP — 8.19 — 137 SCALE MPH`

## 7. Finish Behaviour and Transition to Results

When the race has fully finished, the live display enters a short Finish Display.

Default duration: **5 seconds**, configurable including zero.

The Finish Display can provide suitable theatre — winner, finishing position, chequered treatment, winning time/laps etc.

A Race Director button press during this period skips the remaining delay and proceeds immediately to Results.

## 8. Results Screen

Unlike the live scoreboard, Results are displayed in finishing order, not lane order.

The default Results screen should answer **who won and what happened?** immediately rather than overwhelming the MUG.

Typical information:

**Position | MUG | Lane | Result/time | Best Lap**

Secondary information can include fastest lap, penalties and speed information where track data is available.

A **Details / Analysis** view provides individual lap times, sectors, speeds, penalties and other recorded timing data.

Graphs may eventually live here but are not part of the default Results display.

## 9. Post-Race Navigation

The primary action is contextual:

- **RACE AGAIN** — standalone race; same setup again.
- **CONTINUE** — competition/event; advance to next scheduled race/heat/round.
- **NEXT STAGE** — Timed Stage multi-stage event.
- **EVENT RESULTS** — event has reached its conclusion.

Secondary actions such as **NEW RACE** and **HOME** are available where appropriate.

## 10. Personal MUG Display

Any connected browser can be assigned to a particular lane.

The browser should remember an assignment such as **I'M LANE 3** rather than requiring selection before every race. During organised competition, P&P may assign the lane automatically.

The personal screen prioritises information that can be read at a glance while driving.

**Position** and **race progress** dominate.

Secondary information includes:

**Last Lap | Best Lap | Gap**

It also carries a reduced version of the same Race Control Pod so the MUG sees the same start, Yellow and Red signals as the main display.

Personal alerts can temporarily dominate the screen:

- `FASTEST LAP`
- `FINAL LAP`
- `PENALTY +1 LAP`
- `YELLOW REQUESTED`
- `FINISHED — P2`

Browser audio can likewise provide MUG-specific alerts.

## 11. Race Director and Solo Operation

P&P does not assume a dedicated Race Director.

It must support:

1. Dedicated Race Director.
2. Race Director who is also a racing MUG.
3. Solo MUG.

A dedicated RD can use a separate phone/tablet/PC with denser timing information and race-control functions.

The main full-screen display can also expose controls where appropriate, although **Display Only** should be the normal TV/projector presentation.

For a solo MUG or racing RD, pre-race controls such as **START** can appear prominently and then disappear once the race begins. Normal race operation should then be automatic.

Core rule:

> **Every normal race/session must be capable of being run by one MUG without requiring a separate Race Director.**

## 12. Multiple Simultaneous Displays

Displays are clients, not timing authorities.

A four-MUG event could therefore have:

- 1 × Main Scoreboard
- 4 × Personal MUG displays
- 1 × Race Director display

or any smaller combination.

The System Controller remains responsible for timing, race logic and authoritative race state. Browsers perform their own rendering, animations and browser audio.

Loss of a browser connection must never affect race timing or race state.

## 13. Yellow and Red — Display Principles

Detailed RMS logic is deliberately deferred until software design.

The principles already agreed are:

A MUG's Yellow button can be configured as **Request** or **Direct**.

**Request:** Race continues; requesting MUG and RD are notified; the Race Control Pod surround can pulse yellow. RD accepts or dismisses.

**Direct:** immediately calls a Yellow / Track Call.

**Yellow active:** five lights flash yellow. This represents the normal temporary track-call/interruption state and normally leads to **RESUME**.

**Red active:** five lights flash red. This represents a more serious Race Stopped state.

From Red, the Race Director may ultimately:

- **RESUME RACE** — preserve existing progress and continue.
- **RESTART RACE** — discard progress and begin the race again from zero, with confirmation.
- **ABANDON RACE** — terminate the race.

The precise timing/power behaviour and every possible transition are RMS implementation details to be specified later rather than allowed to derail the display design now.

---

**Product principle:** Easy to start. Deeply configurable when you want it.


## Browser UI rules

### Numeric stability
Numeric fields and live numeric display regions must have fixed dimensions appropriate to their expected values. Changing digits must not resize controls or move adjacent UI.

Use tabular numerals where appropriate for clocks, lap times, gaps, counters and similar changing race data.

General rule:

> **No UI jitter caused by changing race data.**

### Numeric adjustment
Where a numeric value is commonly adjusted, use decrement / editable value / increment controls. Direct editing must remain available so large changes do not require repeated button presses.

### Contextual help
Use a consistent information control for terminology or consequences that may not be obvious.

- Desktop: hover may reveal the help.
- Touch: tap reveals the same help.
- Keep help concise, normally one or two sentences.
- Do not add help icons to self-evident controls merely for consistency.

Typical candidates include Drag Course Length, Timed Stage, Loop, Point-to-Point, finish behaviour, scale speed and false-start behaviour.

If a control requires a paragraph before a MUG can choose it, reconsider the control design rather than relying on a tooltip.
