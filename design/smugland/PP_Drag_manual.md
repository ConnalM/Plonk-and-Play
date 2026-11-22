# P&P — Drag Racing (base single-lane, two-sensor)

**V1 design-review chapter.** This chapter describes the approved intended interface, not a released feature. The linked HTML uses supplied example states and produces neither real timing nor official records.

## 1. Purpose and required hardware

Base Drag uses **one lane** and the two sensors already supplied with P&P: **Sensor 1 = Start** and **Sensor 2 = Finish**. No second competing lane, dual-beam pre-stage, intermediate speed trap, or REU hardware is required. Both sensors and the car must be positioned correctly. Do not confuse Drag course length (physical distance from Start sensor to Finish sensor) with a circuit lap length.

## 2. Set up a run

Select **Drag** in SMUGLAND and choose the MUG. Select a **Sportsman** or **Pro** Christmas Tree; Sportsman is the default. You may leave **Course Length** and **Track Scale** blank and still obtain reaction and elapsed time (ET). If you know the length, enter the physical distance in metres. Track scale may be entered as 1:n for scale-equivalent estimates. These are optional inputs to the shared underlying track configuration, not a substitute timing source.

Only the Race Director/SMUG can change setup and control racing. A MUG or display-only view cannot start a race. During an active run setup is locked. After a completed run use **NEW SETUP** to change settings.

## 3. Stage the car

1. The full Drag Christmas Tree appears. **PRE-STAGE** is visible but dormant and labelled **REU required**; the base single-beam system must never claim to sense pre-stage.
2. Follow **MOVE FORWARD TO STAGE**. Move forward until Sensor 1 is triggered. **STAGE** illuminates and the display says **ROLL BACK**.
3. Roll back slightly until Sensor 1 is clear. STAGE goes out; **READY** confirms the car is just behind the starting line.
4. If the car reblocks Sensor 1 **before the start procedure begins**, the display returns to ROLL BACK. This is **not** a false start. Clear it again before starting the tree. A start must not proceed with the Start sensor blocked.

## 4. Christmas Tree and the run

The Drag tree is separate from the five-light general Race Control Pod. With **Sportsman**, the three amber lamps illuminate sequentially; with **Pro**, they illuminate together. The GREEN/GO indication follows the completed selected tree sequence.

At GO, the car launches. **Reaction** is measured from GO to the subsequent Start-sensor crossing. That Start crossing starts **ET**, which Sensor 2 stops at Finish. Reaction and ET are different quantities and appear separately, nominally to **0.01 second** in base Drag.

If the Start sensor is crossed after the start procedure begins but **before GO**, the tree shows **RED LIGHT**. The run can continue to the Finish sensor so ET is still available. Reaction may be negative (for example, **−0.08 s**); do not erase either result simply because the run was a false start.

## 5. Finish and results

For a valid run, **ET is the dominant figure**, followed by Reaction and optional average speed. For a false start, **RED LIGHT is dominant**, while reaction and ET remain readable. A valid sample is **ET 6.84 s, reaction +0.23 s**. A red-light sample is **ET 6.71 s, reaction −0.08 s**. These are deliberately static review fixtures, not live measurements.

When course length is known, **whole-run average speed** is the course length divided by ET. It must be labelled **average speed**, not **trap speed**. If distance or ET is unavailable, display a dash rather than invent a value. Any scale-equivalent distance/speed is separate and must be labelled. **TRAP SPEED — REU required** remains dormant; explaining it should not imply a speed-trap measurement exists.

The standard product design stores eligible completed runs automatically; there is **no routine SAVE RESULTS button**. The review-only HTML instead maintains a small session-local list of sample outcomes and writes no official History, Personal Best, Track Record or other stored result.

After a finish, **RACE AGAIN** starts another staging sequence with retained setup. **NEW SETUP** returns to configuration. No new race should be started merely by navigating back to the results page.

## 6. Auxiliary Taster display

The approved **800 × 480** auxiliary view retains the recognisable Christmas Tree and shows the appropriate staging, start, GO, running, valid finish or red-light state. PRE-STAGE stays greyed when REU is absent. The result view prioritises ET on a valid run or RED LIGHT on a false start. This view reflects parent race facts and is read-only: it has no independent detector clock or racing authority.

## 7. Review limitations and handover

The prototype lets a reviewer inject **simulated** blocked/clear/amber/GO/crossing/finish events through a separate engineering review strip. These are illustrative controller inputs only, not SMUGLAND production controls. There is no real sensor event ingress, timer, output hardware, endurance integration, race-scoring engine or persistent record write in this prototype.

Production acceptance must separately verify actual timing and sensor mapping, race authority, automatic eligible record handling, physical display readability, timing accuracy and error recovery. Specialist dual-beam/REU competition Drag and trap-speed hardware remain deferred. This document and the Drag prototype do **not** modify the frozen V21 or Timed Stage designs.

**References:** [Original Drag design brief](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7043167797), governing Design Specification §6, and [approved V1 scope](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7061304177).
