# Plonk & Play™ — Common Display Specification

**Status:** Committed product-design specification — Stage 15 UX reconciliation  
**Applies to:** Lap, Endurance and Timed Stage  
**Exception:** Drag uses its own display design.

## 1. Design principle

P&P scales from a solo Racer using one browser to a larger event using a shared display, personal Racer screens and a separate Race Control display.

The default experience remains simple. Advanced functionality may exist without cluttering normal operation.

## 2. Full Browser Race Display

The main race presentation remains fundamentally one screen with changing states:

`READY → STARTING → RACING → FINISHING → FINISHED / RESULTS`

The start sequence does not replace the scoreboard with a separate start page.

When a session is active, ordinary browser navigation/setup chrome yields to race information. At the intended standard desktop/tablet viewport an 8-lane race must show all eight useful-sized lane rows simultaneously without scrolling.

## 3. Race Control Pod

A permanent shallow race-light housing sits across the top of the live display. It contains five light positions, visible but grey/off when inactive.

During a start they become the authoritative race start lights. The system may use three or five according to the session configuration.

During normal racing the lights are dormant but may be repurposed for race-control signals:

- Yellow active — five lights flash yellow;
- Red active — five lights flash red.

The surrounding pod/housing is an independent presentation channel. A Yellow request may pulse the surround while the lamps remain in their normal state. A false-start or Finish treatment may likewise use the housing without pretending the start lamps themselves have changed state.

Start/race-control lighting takes priority over ordinary messages.

The pod may show short messages such as `FASTEST LAP`, `FINAL LAP`, `PENALTY`, `FINISHED`.

The intended housing is a solid shallow unit with clean angled end faces and a flat lower edge, not decorative slash characters around the lamps.

## 4. Main Scoreboard

The main body consists of one row for each active physical lane. Rows remain in fixed lane order; they do not move when race positions change. The Racer's current P1/P2/etc changes prominently within the permanent row.

A typical Lap row contains:

**Position | Lane/Racer | Race Progress | Last Lap | Best Lap | Gap**

Optional Car identity may appear with the Racer without requiring a separate permanent column.

Endurance and Timed Stage use the same visual architecture with mode-appropriate information.

## 5. Temporary Row States

Row geometry remains stable while information may temporarily change emphasis for position change, fastest lap, penalty, final lap and finished state.

A finished Racer remains in the original lane row while others continue where the finish rules require it.

## 6. Track Length, Scale and Speed

Track Setup may store optional track length and scale. Where available P&P may calculate actual model average speed and scale-equivalent speed.

Speed need not clutter the default live scoreboard; it may appear in results/detail or suitable event callouts.

## 7. Ordinary PAUSE

PAUSE is a core race-control intervention and is distinct from the future formal Yellow/Red race-management procedures.

PAUSE does **not** navigate to another race screen. The accepted live race display remains visible with all authoritative race information frozen, normally slightly dimmed, and a centred overlay/modal appears:

**RACE PAUSED**

**RESUME · RESTART · ABANDON**

While paused, the five Race Control Pod lamps continuously alternate:

`Y G Y G Y` ↔ `G Y G Y G`

where **Y** is yellow and **G** is grey/off. This pattern deliberately distinguishes ordinary PAUSE from Yellow active (all five flashing yellow) and Red active (all five flashing red).

**RESUME** closes the modal and requests the normal authoritative P&P start-light sequence. The preserved race resumes only at GO. Existing laps, positions/progress and other authoritative race state are retained and paused time is excluded.

**RESTART** requires confirmation and discards current race progress before beginning again through the normal start procedure.

**ABANDON** requires confirmation and terminates the race without treating it as a completed sporting result.

Stage 15 implements ordinary PAUSE behaviour without thereby implementing the deferred full Yellow/Red RMS state machine.

The integrated Taster retains its deliberately simpler encoder-based pause/restart interaction defined in `PP_TASTER_SPEC.md`.

## 8. Finish Behaviour and Transition to Results

When the race has fully finished, the live display enters a short Finish Display.

Default duration: **5 seconds**, configurable including zero.

Finish presentation may use the Race Control Pod housing/surround and suitable winner/chequered treatment. It must not represent Finish by simply turning all five start lamps yellow.

After the configured delay P&P proceeds automatically to Results. An appropriate permitted control may skip the remaining Finish Display delay.

## 9. Results Screen

Results are displayed in finishing order, not lane order.

Typical information:

**Position | Racer | Lane | Result/time | Best Lap**

Details / Analysis may expose laps, sectors, speeds, penalties and other recorded timing data.

## 10. Post-Race Navigation

The primary action is contextual:

- **RACE AGAIN** — prepare the same standalone race again and return to READY; it does not auto-start;
- **CONTINUE** — future/competition event progression where supported;
- **NEXT STAGE** — Timed Stage multi-stage event;
- **EVENT RESULTS** — event conclusion.

Secondary actions such as Change Race, History and Home are available where appropriate.

## 11. Personal Racer Display

Any connected browser may be assigned to a particular lane and remember an assignment such as **I'M LANE 3**.

Position and race progress dominate. Last Lap, Best Lap and Gap are secondary. A reduced Race Control Pod communicates the same authoritative start and race-control state.

Personal alerts may temporarily dominate, for example Fastest Lap, Final Lap, Penalty or Finished. Browser audio may provide Racer-specific alerts.

## 12. Race Control and Solo Operation

P&P does not assume a dedicated Race Director. It supports a dedicated controller, a Racer who also operates the race, and a solo Racer.

A dedicated Race Control presentation may expose denser timing and permitted race-control functions. A TV/projector normally uses a display-only spectator presentation.

> **Every normal race/session must be capable of being run by one Racer without requiring a separate Race Director.**

## 13. Multiple Simultaneous Displays

Displays are clients, not timing authorities. The System Controller remains responsible for timing, race logic and authoritative state. Browsers render presentation, animations and browser audio. Loss of a browser connection must not affect race timing/state.

## 14. Yellow and Red — Display Principles

Detailed RMS logic remains deliberately deferred.

Yellow / Track Call may ultimately support Off, Request and Direct session policies. Yellow Off does not disable ordinary PAUSE.

- Yellow request: pod surround may pulse yellow while lamps remain normal.
- Yellow active: five lamps flash yellow.
- Red active: five lamps flash red.

A future Red procedure may ultimately offer Resume Race, Restart Race and Abandon Race. Similar button words in the ordinary PAUSE overlay do **not** make PAUSE equivalent to Red and do not authorise Stage 15 to invent the deferred RMS transitions, timing or power behaviour.

## 15. Browser UI rules

### Numeric stability

Numeric fields/live values have fixed dimensions appropriate to expected values. Use tabular numerals where appropriate.

> **No UI jitter caused by changing race data.**

### Numeric adjustment

Common numeric controls use decrement / editable value / increment. Direct editing remains available. Mouse wheel over the control increments/decrements where appropriate. Press-and-hold on +/− repeats and progressively accelerates while remaining controllable. Exact acceleration curve is implementation detail.

### Contextual help

Use concise contextual help only where terminology or consequences are not obvious. Desktop hover and touch tap may expose the same help. If a control needs a paragraph before it can be chosen, reconsider the control.

---

**Product principle:** Easy to start. Deeply configurable when you want it.
