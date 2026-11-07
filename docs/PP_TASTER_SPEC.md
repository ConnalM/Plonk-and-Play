# P&P Taster Specification

## Purpose

P&P Taster is the built-in, restricted client of the normal P&P Browser/Race
Control experience. It is not a separate racing product. It is deliberately
simpler than full browser-based P&P Race Control: its purpose is to let a MUG
switch on the standard base product and run the supported subset without first
connecting a phone, tablet or computer.

The Taster uses the integrated 7-inch, 800 × 480 landscape colour touchscreen.
It should be capable of attractive fonts, colours, graphics and the P&P visual
language. It must not look or behave like a crude segment display. No rotary
encoder or physical control buttons are required; local operation is through
the touchscreen.

## Cross-client contract alignment

The Taster is a controller-local presentation and control client over the
normal P&P authority. It does not create a second Race Control, Race Engine,
timing, results or persistence authority. The canonical race behaviour is
defined in `RACE_MODES_PRODUCT_SPEC.md` and the shared presentation rules are
defined in `COMMON_DISPLAY_SPEC.md` and `PRESENTATION_DESIGN_SPEC.md`; this
document defines only the Taster's restricted scope, controls and layout.

Where this document is silent, those canonical contracts apply. A different
screen size or control surface may omit features, but it may not reinterpret
race state, timing, finish policy, result meaning or event presentation.

The Taster leaves its Welcome state only after configuration has loaded and
P&P is genuinely capable of racing for the selected installation. Optional
services, including Browser connectivity and official record storage, must not
block a standalone Taster race when the required race capabilities are healthy.

## Factory Taster

The factory/default Taster assumes the standard base hardware:

- two sensors;
- two lanes;
- default session role proposal: Sensor 1 = Lane 1 Start/Finish;
- default session role proposal: Sensor 2 = Lane 2 Start/Finish;
- recommended sensors-before-Start/Finish physical arrangement;
- 10-lap Lap Race.

No sensor-role, lane-count or race-mode configuration is required before using the Factory Taster. These factory role proposals become fixed session roles when START is accepted; they do not make the Input Module aware of lane or Start/Finish meaning.

The default physical arrangement does not pretend to provide capabilities it cannot measure. In particular, reaction timing and applicable false-start detection are not available merely from the standard sensors-before-S/F arrangement.

## Standalone controls

Taster should remain comparable in complexity to a basic Scalextric-style lap counter.

The intended local controls are deliberately simple. Taster must not require the SMUG merely to choose an ordinary race distance or duration.

The local touchscreen is the only Taster control surface. It provides the
simple two-lane experience directly without a rotary encoder, physical control
buttons or a browser.

### READY

Taster provides exactly two locally selectable race types:

- **LAPS** — 1 to 999 laps;
- **ENDURANCE** — 1 to 999 minutes.

These are the only Taster race modes. Open Practice and Timed Stage are not
offered on the local touchscreen.

On first/factory use, READY defaults to **10 LAPS**.

Thereafter the last selected race type, the last selected lap distance and the
last selected Endurance duration are retained through the existing Memory/Store
boundary and restored after power-off/restart.

At READY, the touchscreen provides:

- a LAPS/ENDURANCE selector;
- an adjustment control for the selected value, clamped to 1–999 with no wrap;
- a START control for the displayed race.

Selecting LAPS adjusts laps; selecting ENDURANCE adjusts minutes.

START is not a second Taster-specific race mechanism. Once requested, the selected Taster setup is submitted to the normal P&P start procedure and normal Race Control/session machinery.

Taster setup is translated into the normal race modes and finish policies:

- LAPS uses `LapRace` with `CompleteFullRaceDistance`;
- ENDURANCE uses `Endurance` with `CompleteCurrentLap`.

The Browser may expose additional modes, but the Taster never relabels or
redefines them. Its ENDURANCE choice is the normal P&P `Endurance` mode with
the canonical Finish Current Lap policy.

### During a race

RACING includes one small, unobtrusive interruption control. Its visual
treatment belongs to the Taster presentation design; it is not a second race
operation.

The control recognises a deliberate 700 ms touch-and-hold on release. A short
touch, or a hold released before 700 ms, has no race-control effect. A hold at
or beyond 700 ms submits the normal authoritative `PAUSE` operation once.

Once the pause is authoritative, the Taster offers only:

- **RESUME** — use the normal P&P restart countdown and resume the preserved
  race;
- **ABANDON RACE** — requires explicit confirmation, then returns to READY and
  creates no completed result.

The shared pause/restart timing rule in `RACE_MODES_PRODUCT_SPEC.md` applies:
paused time and an authoritative restart countdown do not add racing time.
RESUME continues from the shared P&P timing boundary. The Taster may advance
or render pause/countdown values from authoritative P&P timing, but it does not
independently establish pause or countdown authority.

The Taster does not offer **RESTART**. Race Control remains responsible for
pause settlement, resume timing, abandonment and the no-result/no-History
outcome.

### FINISHED

The final result remains displayed indefinitely until the MUG acts.

- **RACE AGAIN** returns to READY with the previous mode and value retained;
- a separate **START** selection is required to begin the next countdown and
  race.

The standalone Taster result is a sealed in-memory result view, not a local
History screen. **RACE AGAIN** discards that displayed standalone result when
the authoritative operation completes and READY is restored; it does not
automatically start another race.

Taster must not grow into a general menu-driven recreation of the browser interface. If configuration becomes menu-heavy or requires navigating levels, it belongs on the SMUG.

## What the MUG sees

The Taster preserves the original Wokwi screen composition: the P&P graphical
layout, five start lights, two stable lane rows, lane-local car animations and
the bottom message strip. READY, RACING and FINISHED use the same basic
two-lane layout; there is no separate Results screen and no developer or
simulation control surface.

Before the race, the local screen shows the Taster race type/distance and the two competitors as READY.

During a race it provides one stable display row for each of the two lanes/competitors. Each row shows the useful live information appropriate to the race type, including:

- lane number or configured MUG name;
- lap count/progress;
- latest lap time;
- race time/countdown information where appropriate for ENDURANCE.

The Browser and Taster consume the same authoritative state and presentation
events/facts. The Taster may advance or render displayed clock values from
authoritative P&P timing, but never independently establishes the GO instant,
race timing, expiry, finish, overtime, classification or result authority.
Numeric fields and lane rows remain in fixed regions; changing values do not
move unrelated layout. A car or finish effect is a response to its
authoritative event and is active only for that event.

A genuine Start/Finish crossing for a lane may trigger a short graphical animation confined to that lane's row: for example, its little car whizzing across a Start/Finish marker while the new lap information appears. Lane 1 events animate Lane 1; Lane 2 events animate Lane 2. The other lane remains undisturbed.

The lap-crossing presentation may include an optional playful car-pass sound (the intended spirit is a brief “neeeooooowwww”). Graphics and sound are Presentation responses to authoritative race events only and have no timing or Race Engine authority.

On the winning/final lap, the appropriate lane may use a chequered-flag/finish treatment rather than the ordinary lap animation.

For Taster LAPS races, the canonical `CompleteFullRaceDistance` contract in
`RACE_MODES_PRODUCT_SPEC.md` applies: either lane may be first to the selected
distance, each lane's final time freezes at its own eligible crossing, and the
shared race completes only when the required entries have finished. Taster
exposes no alternative finish-policy setting.

For Taster ENDURANCE races, the canonical `FinishCurrentLap` contract in
`RACE_MODES_PRODUCT_SPEC.md` applies: the next eligible crossing after the one
shared expiry point contributes to each lane's result, with any finishing
offset/overtime derived from that same authority.

Timing fields and number formats are shared with the Browser exactly as
defined in `RACE_MODES_PRODUCT_SPEC.md` §4.6. Live elapsed/remaining race
timers use whole seconds beyond 3599 without changing units; completed
race/finish durations use unbounded-minute `mm:ss:xx` with hundredths; live
Endurance overtime uses `+0`, `+1`, `+2` and so on; and each lane's frozen
final offset uses `+mm:ss:xx`, such as `+00:18:42`. The Taster does not
redefine those formats and reserves stable width for their largest values.

At finish it shows the finishing/result information, including finish time as appropriate, followed by the simple result. The result remains on screen until the MUG chooses Race Again.

If SMUG has configured MUG names, those names may replace generic LANE 1 / LANE 2 labels on the Taster display rather than adding unnecessary extra clutter.

The local display uses the authoritative five-light countdown: one through
five red lights progress before GO, then all five lights turn green together.
The Taster never calculates GO or race timing locally.

## SMUG-configurable Taster

The Factory Taster is only the out-of-box default.

Using the browser, a SMUG may configure a different supported Taster for subsequent standalone use. This allows the full interface to perform setup once while leaving later operation simple enough for MUGs or MINIMUGs without supervision.

Examples may include assigning names instead of lane labels and selecting
other supported settings that remain completely operable from the local
touchscreen. A configured Taster must remain self-contained: the browser must
not allow a Taster configuration that subsequently requires a browser merely
to run it.

A RESTORE FACTORY TASTER function should return the product to the guaranteed standard two-lane, 10-lap standalone experience.

### Factory car graphics

The factory Taster provides the original green MGB and red E-Type side-profile
graphics, assigned independently to Lane 1 and Lane 2. SMUGLAND may select a
different graphic for either lane only from P&P's built-in graphic set. Custom
image upload is not part of this contract. Lane graphic selections are
persistent Taster configuration and are restored after restart; they are not
racer identities and are not assigned through the MUG catalog.

### Control hierarchy and records

The Taster controls P&P when no SMUGLAND controller is connected. A connected
SMUGLAND controller automatically supersedes Taster control; Taster remains a
subordinate display and its control requests are disabled. When SMUGLAND
disconnects, the controller returns control to Taster without creating a new
race authority.

The session's control source is frozen when START is accepted:

- a standalone Taster race is non-official and contributes no History, PB or
  Track Record data;
- a SMUG-controlled race follows the normal official-record rules;
- a development simulation is non-official.

Changing Browser/Taster connectivity during an active race never changes the
frozen record policy for that race.

## MINIMUG principle

A SMUG should be able to configure the Taster in the browser and then leave MINIMUGs to use the resulting standalone race without needing the browser or supervision for normal operation.

The usability test is intentionally simple:

> A MINIMUG unfamiliar with P&P should be able to switch it on, understand the local display, start the race and race again without needing to learn a menu system.

## Browser boundary

Taster is the standard two-sensor/two-lane standalone experience.

If the installation uses four lanes or otherwise uses additional sensors/hardware beyond the standard two-sensor Taster arrangement, configuration and race operation use the browser-based full P&P interface.

The local display does not need to become a general-purpose interface for arbitrary expanded hardware.

## Connection guide

The local display also helps the MUG reach full P&P.

P&P provides its own factory local Wi-Fi access point automatically. The
local display should clearly show the P&P network name and Browser address so
that a phone, tablet or computer can connect without house/club Wi-Fi or
Internet. The factory network is deliberately open; joining it provides
connectivity but does not grant SMUG/Race Director control. A QR connection aid
may be added later.

Optional external house/club Wi-Fi is not required for racing. Its sanitized
status may be presented alongside the factory AP status, including the assigned
LAN address or `plonk-and-play.local` when available. The external-network
password is controller-private and must not appear in the Taster, logs,
diagnostics, History, Results or support downloads. Loss of that external
network must not affect the P&P access point or racing. Network configuration
authority remains the Browser Advanced/Diagnostics Network section and the
existing controlling-SMUG lease; this specification does not create a second
authority mechanism.

Thus the local screen has two complementary purposes:

1. provide the complete simple Taster experience without a browser;
2. lead the MUG into full browser-based P&P when more capability is wanted.

The printed MUGGLE Guide remains the quick physical-installation guide; P&P itself should help explain the browser connection once powered.
