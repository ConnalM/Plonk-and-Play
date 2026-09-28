# P&P Taster Specification

## Purpose

P&P Taster is the built-in, standalone experience provided by the P&P unit itself.

It is deliberately simpler than full browser-based P&P Race Control. Its purpose is to let a MUG switch on the standard base product and run a basic race without first connecting a phone, tablet or computer.

The local display should be a decent small graphical colour display capable of attractive fonts, colours, graphics and the P&P visual language. It must not look or behave like a crude segment display.

## Factory Taster

The factory/default Taster assumes the standard base hardware:

- two sensors;
- two lanes;
- Sensor 1 = Lane 1 Start/Finish;
- Sensor 2 = Lane 2 Start/Finish;
- recommended sensors-before-Start/Finish physical arrangement;
- 10-lap Lap Race.

No sensor-role, lane-count or race-mode configuration is required before using the Factory Taster.

The default physical arrangement does not pretend to provide capabilities it cannot measure. In particular, reaction timing and applicable false-start detection are not available merely from the standard sensors-before-S/F arrangement.

## Standalone controls

Taster should remain comparable in complexity to a basic Scalextric-style lap counter.

The intended local control is one simple physical race button.

Before the race the screen clearly offers START. Pressing the button begins the configured start sequence.

After the race the screen clearly offers RACE AGAIN. Pressing the same button resets the Taster to its ready state; pressing START then begins the next race.

Taster does not need local controls for every exceptional situation such as aborting, restarting or reconfiguring a race in progress. Those are full P&P/browser functions.

## What the MUG sees

Before the race, the local screen shows the Taster race type/distance and the two competitors as READY.

During a basic Lap Race it shows, for each lane/competitor:

- lane number or configured MUG name;
- laps completed / target laps;
- latest lap time.

At finish it shows the finishing/result information, including finish time as appropriate, followed by a simple result and RACE AGAIN.

If SMUG has configured MUG names, those names may replace generic LANE 1 / LANE 2 labels on the Taster display rather than adding unnecessary extra clutter.

The local display may use the established P&P graphical start-light language.

## SMUG-configurable Taster

The Factory Taster is only the out-of-box default.

Using the browser, a SMUG may configure a different supported Taster for subsequent standalone use. This allows the full interface to perform setup once while leaving later operation simple enough for MUGs or MINIMUGs without supervision.

Examples may include changing the lap count, assigning names instead of lane labels, and selecting other settings that remain completely operable from the local Taster interface.

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
