# P&P Taster Specification

## Purpose

P&P Taster is the built-in, standalone experience provided by the P&P unit itself.

It is deliberately simpler than full browser-based P&P Race Control. Its purpose is to let a MUG switch on the standard base product and run a basic race without first connecting a phone, tablet or computer.

The local display should be a decent small graphical colour display capable of attractive fonts, colours, graphics and the P&P visual language. It must not look or behave like a crude segment display.

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

The preferred hardware direction is a rotary encoder with an integral push-button, together with the local graphical display.

### READY

Taster provides exactly two locally selectable race types:

- **LAPS** — 1 to 999 laps;
- **TIMED** — 1 to 999 minutes.

On first/factory use, READY defaults to **10 LAPS**.

Thereafter the last selected race type, the last selected lap distance and the last selected timed duration are retained in persistent settings storage and restored after power-off/restart.

At READY:

- rotating the encoder changes the currently displayed lap distance or timed duration directly, with no menu entry;
- slow rotation changes the value by 1 per detent;
- medium-speed rotation accelerates adjustment to 5 per detent;
- fast rotation accelerates adjustment to 10 per detent;
- when rotation slows, adjustment promptly returns to 1 per detent;
- values are clamped to 1–999 and do not wrap;
- a short press starts the displayed race;
- a long press toggles between LAPS and TIMED, restoring the last value used for that race type.

START is not a second Taster-specific race mechanism. Once requested, the selected Taster setup is submitted to the normal P&P start procedure and normal Race Control/session machinery.

### During a race

A short encoder press has no race-control effect.

A deliberate long press requests the protected Abort interaction. The display asks the MUG to confirm **ABORT RACE?**, with **NO** as the safe/default choice. Rotating the encoder selects YES or NO and a short press confirms. If no confirmation is made within a short timeout, the confirmation is dismissed and the live race display resumes. Merely long-pressing must never itself abort the race.

### FINISHED

The final result remains displayed indefinitely until the MUG acts.

- short press = **RACE AGAIN**, using the same race type and value and entering the normal start procedure;
- rotating the encoder leaves the result view and adjusts the current race value ready for a new race;
- the same velocity-sensitive 1/5/10 adjustment applies.

Taster must not grow into a general menu-driven recreation of the browser interface. If configuration becomes menu-heavy or requires navigating levels, it belongs on the SMUG.

## What the MUG sees

Before the race, the local screen shows the Taster race type/distance and the two competitors as READY.

During a race it provides one stable display row for each of the two lanes/competitors. Each row shows the useful live information appropriate to the race type, including:

- lane number or configured MUG name;
- lap count/progress;
- latest lap time;
- race time/countdown information where appropriate for TIMED.

A genuine Start/Finish crossing for a lane may trigger a short graphical animation confined to that lane's row: for example, its little car whizzing across a Start/Finish marker while the new lap information appears. Lane 1 events animate Lane 1; Lane 2 events animate Lane 2. The other lane remains undisturbed.

The lap-crossing presentation may include an optional playful car-pass sound (the intended spirit is a brief “neeeooooowwww”). Graphics and sound are Presentation responses to authoritative race events only and have no timing or Race Engine authority.

On the winning/final lap, the appropriate lane may use a chequered-flag/finish treatment rather than the ordinary lap animation.

For Taster Lap Races, both lanes always complete the full selected race distance. The first lane to complete the target is recorded as the winner/first finisher, but the other lane continues until it has also completed the target. Taster exposes no alternative finish-policy setting.

At finish it shows the finishing/result information, including finish time as appropriate, followed by the simple result. The result remains on screen until the MUG chooses Race Again or changes the race setting.

If SMUG has configured MUG names, those names may replace generic LANE 1 / LANE 2 labels on the Taster display rather than adding unnecessary extra clutter.

The local display may use the established P&P graphical start-light language.

## SMUG-configurable Taster

The Factory Taster is only the out-of-box default.

Using the browser, a SMUG may configure a different supported Taster for subsequent standalone use. This allows the full interface to perform setup once while leaving later operation simple enough for MUGs or MINIMUGs without supervision.

Examples may include assigning names instead of lane labels and selecting other settings that remain completely operable from the local Taster interface. Lap count is a special case: it must also be adjustable directly on the Taster itself using the local rotary control.

A configured Taster must remain self-contained: the browser must not allow a Taster configuration that subsequently requires a browser merely to run it.

A RESTORE FACTORY TASTER function should return the product to the guaranteed standard two-lane, 10-lap standalone experience.

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

When appropriate it should clearly show how to connect a phone, tablet or computer to the P&P browser interface, using the P&P network details and a QR code where practical.

Thus the local screen has two complementary purposes:

1. provide the complete simple Taster experience without a browser;
2. lead the MUG into full browser-based P&P when more capability is wanted.

The printed MUGGLE Guide remains the quick physical-installation guide; P&P itself should help explain the browser connection once powered.
