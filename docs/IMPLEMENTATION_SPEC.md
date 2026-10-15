# P&P Normal Browser --- Implementation Handover

## Status and priority

This package contains the current approved visual references and agreed
normal-Browser UX.

If anything conflicts, use this priority:

1.  P&P authority/product specification
2.  This implementation handover
3.  Approved visual HTML references
4.  Mock-up behaviour/scripts

The HTML files are visual references, not production code to paste
blindly. Existing authoritative P&P behaviour and API boundaries must be
retained. Do not resurrect older/intermediate mock-ups.

## Authority rule

P&P is authoritative. The Browser is presentation and control.

The Browser may present authoritative state, submit requests,
temporarily display a locally requested setup value while it settles,
and add presentation-only effects.

The Browser must not determine laps, timing, position, gaps, winner,
finishing state, classification, readiness or acceptance of
configuration. Reload/reconnect reconstructs the correct presentation
from P&P.

## Visual language

Use dark navy/charcoal, restrained P&P yellow, white/off-white primary
information and lane colours for lane identity. Keep the
race-equipment/instrument appearance rather than game styling. Retain
the P&P logo, pictorial top navigation, yellow selected-nav treatment,
common Race Control Pod and fixed lane-row language. Racer and Car
remain separate identities.

Normal navigation: Race \| Results \| Track Setup \| Racers \| Settings.

## Race / READY

The page answers: "What is P&P ready to do if I press START?"

Show Race Entries, racer/car/lane assignments, Race Setup, authoritative
mode/configuration, Track Status, START and Latest Result.

Mode selector: LAP RACE \| PRACTICE \| ENDURANCE \| TIMED STAGE \| DRAG

Unavailable modes are shown unavailable only when P&P reports that
state.

There is no Save/Apply. Setup controls request changes. START starts
P&P's currently authoritative proposal; it never submits Browser-local
pending values.

## Lap Race setup

Target laps 1--999, Race Entries, Rotate Lanes, Reverse direction and
Automatically rotate after each race.

Exactly three finish behaviours:

1.  All stop when winner finishes.
2.  Winner finishes --- all others finish current lap.
3.  Winner finishes --- all others complete total race distance.

Do not show an empty Advanced/More Race Options panel. Add it only when
P&P has genuine authoritative secondary options.

## Endurance setup

Duration 1--999 whole minutes, default 10 minutes, Race Entries and
applicable lane controls.

Exactly two finish behaviours: STOP AT ZERO or FINISH CURRENT LAP.

## Practice setup

Initial implementation is OPEN PRACTICE. Timed Practice remains
unavailable until P&P has the specified authoritative mechanics.

Open Practice has no race distance, winner, finish policy or competitive
position.

## Numeric controls

Support direct typing, decrement/increment and desktop hover + mouse
wheel.

Wheel over the numeric field changes by ±1 within bounds and consumes
the wheel event so the page does not also scroll. Moving the pointer
away restores normal page scrolling.

Wheel/hover is a convenience only. Touch must have equivalent usable +/-
and direct-entry operation.

Typing uses an edit buffer. Typing 120 must not generate intentions 1,
12 and 120. Commit on Enter, blur or a short settle interval.

## Pending setup

The Browser may immediately show the latest desired value, but that is
not authoritative.

While unresolved, retain the latest desired value, do not let older P&P
readback bounce it backwards, use only subtle pending indication if
delay is perceptible, and make START unavailable.

On acceptance reconcile normally. On rejection restore authoritative
value and explain briefly.

Rapid changes such as 10→4→2→1 must leave 1 displayed while machinery
catches up.

Reload/reconnect discards local pending state and reconstructs from P&P.

## STARTING

STARTING uses the stable race screen.

There are five physical lamp positions. P&P supplies authoritative start
presentation, including configured 3-red or 5-red sequence, LIGHTS OUT
and GO.

At GO all five positions are green for one second, then extinguish.
Green is not left on throughout RACING.

The Browser must not recreate the start state machine from its own
elapsed time.

Endurance must show the configured authoritative duration before GO.

## Lap Race live screen

Rows remain in physical lane order throughout the race.

Columns: POSITION \| LANE/RACER \| PROGRESS \| LAST \| BEST \| GAP

Progress example: 6 / 10.

Show mode/race distance, Fastest Lap, Track Record, Race Control Pod,
elapsed RACE TIME and PAUSE.

RACE TIME counts upward from authoritative race timing. Pause freezes
it; Resume continues it.

## Endurance live screen

Same visual family as Lap Race.

Columns: POSITION \| LANE/RACER \| LAPS \| LAST \| BEST \| GAP

Show prominent TIME REMAINING counting down from authoritative timing.

Clearly show ENDURANCE · STOP AT ZERO or ENDURANCE · FINISH CURRENT LAP.

For FINISH CURRENT LAP after expiry, TIME REMAINING remains 00:00 and
OVERTIME appears where applicable, counting upward, e.g. +00:18.

P&P determines whether overtime applies and supplies authoritative
timing. Browser does not infer expiry/overtime.

## Lane density

The same scoreboard works from 1--8 lanes: 1--2 generous, 3--4 medium,
5--6 compact, 7--8 dense.

Do not make one lane absurdly tall. Preserve column geometry. Optional
car imagery scales with row density and remains secondary.

Eight lanes should be visible without vertical scrolling at the intended
primary desktop/tablet Race Control viewport.

## Lap and finish animation

Normal lap: car accelerates left→right and exits.

Finishing lap: the same-size car accelerates left→right, pulls an
expanding chequered band, exits, and the band briefly ripples/settles
then clears.

The car does not grow during the animation.

If one racer has finished while others continue, show a small chequered
flag in that lane's fixed reserved flag slot. Do not alter row geometry
and do not add redundant FIN text.

P&P supplies the authoritative lap/finish event. Animation never gates
or determines sporting state.

## PAUSE-first session management

While actively running, the only deliberate session-management action is
PAUSE.

Competitive Lap Race / Endurance after authoritative PAUSED: RACE PAUSED
--- RESUME \| RESTART \| ABANDON

ABANDON is destructive and requires confirmation. Confirmed abandonment
returns to READY with no Result, History entry or Finish Display.

Practice after authoritative PAUSED: PRACTICE PAUSED --- RESUME \|
RESTART \| END PRACTICE

Practice does not use ABANDON.

## Resume

Lap Race / Endurance: RESUME → authoritative STARTING → start
presentation → GO → RACING.

Endurance timing remains frozen until authoritative GO.

Open Practice Resume returns directly to active Practice with no fake
STARTING/countdown/GO. Completed laps remain; partial-lap handling
follows authoritative P&P Practice semantics.

## Competitive Finish Display

P&P owns whether Finish Display is active, its timing and when it ends.
Browser owns appearance.

Default presentation duration is 5 seconds where configured, including
zero where supported.

Final live scoreboard freezes. Browser must not independently start its
own five-second timer.

Reload during Finish Display reconstructs the current authoritative
point; it does not restart the delay.

RESULTS › may request skipping the remaining Finish Display where P&P
permits it.

## Results

Results deliberately resemble the live race display.

Live race rows = physical lane order. Results rows = authoritative
classification/finishing order.

Lane number/colour remains attached to racer.

Retain familiar scoreboard geometry, Fastest Lap and Track Record. Use
restrained first-place emphasis; no separate giant winner card.

Bottom actions: HISTORY \| DETAILS \| RACE AGAIN

RACE AGAIN requests the next proposal and returns to READY; it never
immediately starts a race.

## Practice completion / summary

Flow: Active Practice → PAUSE → END PRACTICE → Practice Summary.

No competitive Finish Display or winner theatre.

Practice Summary uses the same visual family but is non-competitive.
Show relevant authoritative information such as LANE \| RACER/CAR \|
LAPS \| BEST LAP, plus Fastest Lap / Track Record where P&P supplies
them.

Do not show position, gap, winner, finishing order, leader highlighting
or competitive chequered treatment.

The lifecycle decision is explicit: an ended Practice cannot be resumed.
Practice Summary presents PRACTICE AGAIN (primary) and HOME (secondary).
PRACTICE AGAIN is an authoritative request to prepare a new Open Practice with
the retained relevant setup; START remains a separate action.

## Individual racer finish

If one racer finishes while others continue, the live race remains, the
racer stays in the physical lane row, finished state is clear, the small
chequered flag appears in the reserved slot and authoritative
progress/classification is shown.

Browser never infers finished state from lap counts.

## Devices

PC/laptop: full SMUG/Race Control experience.

Tablet: full SMUG/Race Control experience. Landscape is
preferred/optimised, not mandatory.

Phone: Personal MUG is the default intended experience rather than a
miniature SMUG. Do not create an artificial architectural prohibition
against Race Control on a phone unless separately decided.

All devices remain clients of the same authoritative P&P state.

## Browser compatibility

Target current mainstream Chrome, Edge, Firefox and Safari.

Core operation must not depend on Windows, Chrome-specific behaviour,
hover, mouse wheel or right-click.

Mouse, keyboard and touch may provide different conveniences, but
equivalent core functionality must remain available.

No browser version numbers are frozen.

## Multiple Browsers

Several Browsers may connect simultaneously, including full Race Control
and Personal MUG presentations. Different presentation/device layouts
must never create different race truth.

## Deliberately outside this handover

Do not invent detailed Personal MUG visual design, TV/projector
presentation, dedicated Race Control display, Screen Designer,
Developer/Diagnostics, full History visual design, full Details visual
design, unsupported Advanced Race Options, Timed Practice, Timed Stage,
Drag, pits/fuel, championship/competition features, Power Module
behaviour or future advanced start/false-start configuration.

## Approved visual references

-   01_RACE_READY.html --- approved Race/READY visual baseline.
-   02_LAP_RACE.html --- Lap Race live baseline with elapsed Race Time.
-   03_ENDURANCE.html --- Endurance live baseline with Time Remaining
    and conditional Overtime example. Bottom test buttons are not
    production UI.
-   04_RESULTS.html --- Results baseline: live-race family,
    authoritative result order.
-   05_PRACTICE_SUMMARY.html --- current Practice Summary baseline; note
    the RESUME authority check above.
-   06_EIGHT_LANE_DENSITY.html --- approved density/layout reference;
    placeholder car graphics are not approved assets.
-   07_LAP_FINISH_ANIMATION.html --- lap/finish animation and fixed
    finished-lane flag-slot reference.

Only these files are included deliberately. Older intermediate/broken
mock-ups are not part of the handover.
