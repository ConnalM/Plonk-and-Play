# Plonk & Play
## User Manual — V1
**Editorial draft 0.1 · 20 November 2026 · Not for customer distribution until release verification**

*Plonk. Plug. Play.*

Welcome to Plonk & Play (P&P), a slot-car racing system designed to get you racing quickly and give you more control when you want it. You can run a straightforward two-lane race on the built-in touchscreen or use Race Control on a phone, tablet or computer for more choices and race records. You do not need an Internet connection to race.

**About this draft:** The instructions describe the approved V1 product design, not a claim that the current test firmware implements every feature. Editorial verification notes are collected at the end. Customer-facing terminology follows the provisional governing specification: Integrated Display (Taster), Racer (MUG), Race Control (SMUGLAND). Final packaging terminology remains subject to owner approval.

## Contents
1. Meet P&P
2. Your first race
3. Installing and configuring your track
4. Using the Integrated Display
5. Connecting to Race Control
6. Choosing and running a race
7. Results, History and records
8. Racers, cars and settings
9. Customising the auxiliary display
10. Looking after P&P and troubleshooting
11. Going further
12. Glossary

## 1. Meet P&P

P&P measures the progress of slot cars as they pass the track sensors. The P&P unit runs the race and calculates the timing and results. The screen on the unit and any connected Browser show information from that same race.

| Part | What it does |
|---|---|
| P&P unit | Controls the race, receives detector signals and manages timing and results. |
| Integrated Display (Taster) | Built-in colour touchscreen; lets you run basic races without another device. |
| Track sensors | Detect the cars passing the relevant points on the track. |
| Race Control (SMUGLAND) | Browser interface for race setup, control, viewing and settings. |
| Removable storage | Supports persistent results and other saved information on the specified installation. |

You do not need to enter your name, register your car or create a track profile before your first race. Start with the default two-lane installation. Add names and more detail when useful.

One person can race and operate P&P; a separate race director is not compulsory. A connected Browser may observe a race without being the controlling Browser.

## 2. Your first race

Use the illustrated MUGGLE Guide supplied with P&P for the physical arrangement.

1. **PLONK:** Position the two sensors in their recommended positions before the Start/Finish line, one for each lane.
2. **PLUG:** With the equipment switched off, connect Sensor 1 for Lane 1 and Sensor 2 for Lane 2 using the labelled connections. Connect the specified P&P power supply.
3. Switch on and wait for **READY**.
4. Leave **LAPS** selected and the factory distance of 10 laps, or select your preferred distance.
5. Put both cars in their proper starting positions. Press **START**.
6. Wait until the five red start lights turn green, then race.
7. Each car finishes when it completes the selected distance. The winner is shown, and the other racer can complete the race.
8. Press **RACE AGAIN** to prepare another race. Press **START** separately when ready.

That is all you need for standalone racing. To use the full Browser interface, follow Chapter 5. Standalone races are not saved as official History or records.

[Illustration required: labelled standard two-lane installation and factory READY screen.]

## 3. Installing and configuring your track

### 3.1 The standard two-lane arrangement

P&P's factory settings match the recommended layout: Sensor 1 measures Lane 1 and Sensor 2 measures Lane 2. Both serve as Start/Finish detectors, positioned before the Start/Finish line as shown in the installation drawing.

[Hardware illustration required: confirmed sensor orientation, placement, mounting height, starting grid, direction of travel and connectors. Not to scale until checked.]

Use the supplied or manufacturer-approved mounting and connection method. Secure cables away from cars and moving parts. Do not force connectors. Use only the specified power supply; disconnect power before changing wiring. Follow the instructions for your slot-car track and controllers separately.

This recommended arrangement is intended for ordinary lap timing. It does not provide every possible start-detection measurement. Some options may be unavailable because the installed arrangement cannot measure them.

### 3.2 Changing the installation

In Browser Setup, use Track/Hardware to inspect the remembered lane and sensor arrangement. If you alter the physical layout or swap detector duties, update the corresponding configuration before racing. Do not change the arrangement during a race.

Track length and scale are optional. Enter them accurately if you want applicable speed calculations. P&P remembers one physical track configuration in V1; you do not have to give it a name.

If a sensor is missing or a required function is unavailable, correct the connection or configuration before starting. A disabled option does not necessarily mean a fault: it may need different equipment or positioning.

## 4. Using the Integrated Display (Taster)

### 4.1 Before the race

The touchscreen is designed for uncomplicated two-lane racing.

| Control | Purpose |
|---|---|
| LAPS | Race to a selected number of laps. |
| ENDURANCE | Race for a selected number of minutes, then finish the current lap. |
| Value adjustment | Select a distance or duration from 1 to 999. |
| START | Start the selected race when ready. |

Factory settings: LAPS, 10 laps, with 5 minutes remembered for ENDURANCE. Your last selected mode, distance and duration are remembered for next time. If a larger Browser lap target is displayed, explicitly adjust it into the local range before starting a standalone race; merely viewing the larger target does not change it.

There is no local settings menu to learn. A controlling Browser can configure the lane names, built-in car pictures and remembered race values for subsequent standalone use.

### 4.2 While racing

The display keeps the same two lane rows so you can find each car's information at a glance. Watch the start lights, lap counts, latest lap times and relevant race clock. Cars must wait for GO. The standalone countdown is five red lights followed by green.

In LAPS, the winner is the first to complete the distance; the other racer finishes their full distance. In ENDURANCE, each racer completes their current eligible lap after time expires.

### 4.3 Pausing and abandoning

Touch and hold **PAUSE** for at least 0.7 seconds, then release. A quick tap does not pause the race.

When paused:
- **RESUME** continues the same race using the normal restart sequence.
- **ABANDON RACE** asks for confirmation, then ends the interrupted race and returns to READY without a completed result.

Paused time and the restart countdown do not count towards racing time. There is no standalone Restart option; abandon and start a fresh race if necessary.

### 4.4 After the race

The finish information remains visible until you choose **RACE AGAIN**. This prepares the same race settings without starting automatically and discards the previous standalone result view. There is no local History menu.

### 4.5 When a Browser takes control

When Race Control becomes the controlling Browser, the Integrated Display changes to a supporting display rather than being a second race controller. Local race-control requests are disabled while that Browser holds control. If control returns to the unit, the local controls become available again where appropriate. You do not need to restart the race just because a Browser connects or disconnects.

[Screen references required: READY, countdown, RACING, PAUSED, FINISHED and Browser-controlled display.]

## 5. Connecting to Race Control (SMUGLAND)

### 5.1 Direct connection — no Internet needed

1. Switch on P&P and make sure it is ready.
2. Open Wi-Fi settings on your phone, tablet or computer.
3. Select P&P's own local network (use the network name shown on the unit).
4. The factory network is open and needs no Wi-Fi password. Your device may tell you the network has no Internet; that is expected. Stay connected.
5. Open your ordinary web browser and enter the address shown by P&P, including the local IP address when displayed.

Race Control opens in the Browser. No separate app or cloud account is needed for local racing.

Joining the network does not by itself give you control of the race. The controlling Browser has the permitted race controls; other connected Browsers can view the available information. Check which device controls the race before trying to change settings or start.

### 5.2 Using your home or club Wi-Fi

A controlling Browser can configure P&P to join an existing network through **Settings → Advanced → Diagnostics → Network**.

Scan/select the network and supply its credentials where requested. After a successful connection, use the displayed local network address or `plonk-and-play.local` if available. If the name does not resolve, use the displayed IP address. P&P remembers one external network.

P&P's own local network remains independent of the external connection. You can still use it when your home Wi-Fi is unavailable. The controlling Browser can reconnect to the remembered network, disconnect, reconfigure credentials or forget it. Forgetting a network removes the saved credentials; disconnecting does not.

### 5.3 Finding your way around

The main interface offers race-mode choices, Race Again, Results & History and Setup. V1 includes Lap Race, Open Practice, Endurance, Timed Stage (Rally/Hill Climb/Sprint) and basic single-lane Drag. Availability on a particular unit depends on implementing the release firmware and configuring the required sensor arrangement; a greyed-out control does not prove a promised V1 mode is cancelled. A disabled control may indicate that you are observing, the race is in progress or the required equipment is not available.

Choose one of the supplied race-screen layouts — Classic, Pit Wall, Grandstand and Timing Tower — to suit your viewing preferences. Changing the presentation does not change race timing or results. Layouts adapt to phone, tablet and larger displays.

[Insert production Home/navigation, connection guide and each supplied preset screenshot after implementation.]

## 6. Choosing and running a race

### 6.1 Lap Race

Choose **Lap Race**, assign racers to lanes if wanted, set the lap target and choose how the race finishes.

| Finish option | What happens |
|---|---|
| Immediate finish | Ends the competition when the first racer reaches the target. |
| Complete current lap | The first racer reaches the target; the others finish their current lap. |
| Complete full race distance | Every required racer completes the full selected distance. |

Use **More Race Options** for available start-light, GO, delay, false-start and audio settings. The normal Browser factory Lap Race start uses five red lights and lights out for GO; the Browser can offer other supported start settings. Follow the selected signal. The standalone screen has its own fixed five-red-to-green presentation.

Press START only when the cars are in position and everyone is ready. P&P handles the timing, finishing and classification.

False-start detection is only available where the physical detector arrangement can measure the event. Applicable options may include a warning or an extra lap; an extra-lap penalty means the affected racer must actually complete another lap.

### 6.2 Open Practice

Use Practice to practise driving or compare lap times without a competitive winner. Select **Open Practice**. Choose either immediate start (default) or the optional five-light practice start, then begin.

Practice is open-ended. Press **PAUSE**, then **END PRACTICE** to finish and view the Practice Summary. While paused you can also resume or restart. Immediate Practice resumes without a competitive restart sequence.

The summary is for the session just completed, not official race History or PBs. Choose **PRACTICE AGAIN** to prepare a new session, or **HOME** to leave. Neither action automatically starts another session.

Timed Practice is not available in V1. Do not confuse Practice with Endurance, which is a timed competitive race.

### 6.3 Endurance

Choose **Endurance**, select the duration and assign racers. The main clock counts down the remaining time.

| Finish option | What happens at zero |
|---|---|
| Stop at zero | Competition stops at the shared expiry time; the result uses the eligible completed progress. |
| Finish current lap | Eligible racers continue to their next finishing crossing, then stop. |

The winner is determined by the applicable classification rules, not by who happens to be closest to the sensor when time expires. At zero, racers still completing a lap may have an overtime figure. Do not switch the finish policy while racing.

### 6.4 Race Control during a race

The controlling Browser offers **PAUSE** during an active Lap Race, Endurance or Open Practice session. Once paused, the choices depend on the mode.

| Mode | Paused controls |
|---|---|
| Lap Race / Endurance | Resume, Restart, Abandon |
| Open Practice | Resume, Restart, End Practice |

Resume preserves the current race. Restart prepares a new session with the retained setup, not a continuation of the abandoned race. Abandon requires confirmation and does not create a completed race result. Practice ends normally into its summary rather than being abandoned.

If a Browser closes or loses its connection, P&P itself continues to control race timing and state. Reconnect to see the current authoritative state. Avoid repeatedly pressing buttons if the connection is interrupted.

### 6.5 Timed Stage (Rally / Hill Climb / Sprint) — V1 release chapter pending

Timed Stage is a required V1 racing mode. Its approved design supports ordered stages/attempts, split timing where configured, false-start rules, Director-controlled retry/DNF and stage/event classifications. **Editorial checkpoint:** verify all operation against the implemented firmware and the approved [Timed Stage design/manual](https://github.com/ConnalM/Plonk-and-Play/blob/d3f46e8a56e134e11388e6127d957283259167af/design/smugland/PP_Timed_Stage_manual.md) before writing customer button-by-button instructions. Do not present illustrative prototype timing as official results.

### 6.6 Drag — V1 release chapter pending

Basic V1 Drag uses a single lane with two sensors, one for Start and one for Finish; the approved design includes staging, the Christmas Tree, reaction and elapsed time, false starts and results. **Editorial checkpoint:** insert exact customer instructions only once Gina's consolidated Drag design passes final review and real firmware/sensors have passed physical tests. Dual-beam and REU-dependent specialist functions are not part of basic V1.

## 7. Results, History and records

### 7.1 Reading a result

After a competitive race, the results identify the finishing order and winner where there is one. Use the available Details view to inspect individual lap timing. Shared first places and incomplete information should be read as shown; an unavailable timing gap is not a zero-second gap.

**Race Again** prepares another race using the retained setup. Check assignments and any automatic lane rotation before starting again.

### 7.2 Which races are saved?

| Session | Official History / records |
|---|---|
| Eligible normal Browser-controlled physical-detector Lap Race or Endurance | Yes, subject to timing validity and successful storage. |
| Standalone Integrated Display race | No. |
| Open Practice | No. |
| Simulated/test race | No. |
| Abandoned competitive race | No completed History entry. An already valid eligible fastest lap may still count. |

A race does not become official simply because a Browser connects partway through it. The source and eligibility are established for the session. A simulated lap cannot become a real track record.

### 7.3 History and personal bests

Open **Results & History** to see retained official sessions, newest first. Select a result to reopen it. Older sessions are removed automatically when the history buffer fills.

A personal best (PB) is a racer's fastest eligible lap. A track record is the overall fastest eligible lap for the track. Valid PBs and track records are retained independently of ordinary History expiry.

Clearing History and clearing records are separate actions. Read the confirmation before proceeding; deleting old History does not automatically delete best laps. A storage warning means saved information may be unavailable or incomplete, not that live timing has necessarily failed.

## 8. Racers, cars and settings

### 8.1 Racer and lane assignment

You can race immediately using generic lane names. Select a racer in the lane row to choose a saved name, Guest, clear the assignment or create a new racer. A guest can have a temporary name without becoming a permanent profile.

Renaming a saved racer preserves their historical identity. P&P does not assign one racer permanently to one lane.

Use Rotate Lanes, reverse direction, automatic rotation or direct assignment as available on the race setup screen. Check the actual lane assignment before each start. Car recording is optional; it is distinct from the decorative built-in car graphics on the Integrated Display.

### 8.2 Remembering your settings

Normal race settings are remembered automatically where your intention is clear. A separate Save Setup action is not needed for everyday racing. Use More Race Options for race-specific choices and Setup for your installation.

### 8.3 Integrated Display settings

In the controlling Browser, use Integrated Display settings to set the remembered mode, lap value, duration, lane display names and built-in car graphic for each lane. Factory graphics are green MGB and red E-Type. You cannot upload custom car images.

Restoring the factory standalone display returns its own remembered race choices, lane labels and graphics to their defaults. It does not erase saved racer assignments, track settings or shared sound preferences.

### 8.4 Sound

Sound settings are shared by standalone and Browser operation. Choose individual event sounds, volume and destination (P&P unit, Browser, Both or Mute). Browser-only sound falls back to the unit when racing standalone; Mute is always respected.

The intended factory destination is the unit speaker at 60%. Countdown, lap completion, final lap, time warnings, pause/resume and winner sounds are enabled by default. Spoken lap times and additional announcements can be enabled. Endurance default warnings are one minute and ten seconds remaining, time expired and finish current lap; five-minute and thirty-second warnings are off.

Use the sound preview and separate speaker/Browser tests before a race, not during one.

## 9. Customising the auxiliary display

When Race Control is in charge, the Integrated Display can show selected race information without duplicating the controls of the Browser.

### 9.1 Choosing information

Open the auxiliary display configuration in Race Control. Select a preset or the fields that matter to you, such as race clock, racer names, lap counts, positions, last/best times and gap to the racer ahead where available.

The layout adjusts to the information and actual configured racing lanes. Fixed display is the default. For more information than fits comfortably, enable cycling and choose the information sets, maximum lanes shown per page and how long to display each.

Preview your changes. **Apply** activates your draft configuration; **Save** stores a named configuration. Saving and applying are separate choices. Use Save As, Rename and Delete to manage your named configurations. Manual selection is the default; you can also associate saved configurations with particular race types.

Changes can be applied without interrupting an active race. Restoring auxiliary display defaults applies the protected factory configuration and retains named custom configurations and unrelated settings.

### 9.2 What happens during race events?

Starting lights take priority over ordinary information. During a pause or competitive restart, the same race information is retained beneath the appropriate signal or pause overlay. The ordinary cycling stops during the interruption and resumes at the appropriate point afterwards.

At the finish, results replace normal information cycling. When several pages are needed, the winner remains visible on each page where there is a winner. Practice instead shows a non-competitive summary.

A clock-only configuration is useful when you want large, easily read timing figures. If a value is not available, P&P does not make up a measurement.

[Insert production setup, preview, cycling, pause overlay and finish screenshots. Confirm SD persistence/fallback and actual supported lane counts before publication.]

## 10. Looking after P&P and troubleshooting

| Symptom | What to check |
|---|---|
| Screen not ready | Check specified power and required detector connections. Read any message before proceeding. |
| A car is not counted | Check the correct lane connection, detector placement and unobstructed detection path. Test at low speed before racing. |
| Phone says no Internet | Expected on the factory P&P network. Stay connected and enter the displayed local address. |
| Browser cannot open P&P | Confirm you are on the correct network; use the exact displayed address, not a web search. Try direct P&P Wi-Fi if external Wi-Fi fails. |
| Browser shows race but controls are disabled | Check whether another Browser holds control, or whether the current race state prohibits the action. |
| Local START unavailable | Check Browser control, supported installation, valid local race value and READY status. |
| Lap or timing option unavailable | Check whether the installed sensors and their configured arrangement support it. |
| No sound | Check Mute, volume, event setting and sound destination; use the idle sound tests. |
| No stored result | Check session eligibility and any storage warning. Standalone and Practice results are not official records. |
| Auxiliary layout not retained | Check whether you saved as well as applied it, and whether storage is available. Use the built-in factory configuration if necessary. |

Do not erase everything to fix an ordinary connection or detection problem.

### 10.1 Restart, resets and updates

Use the normal System instructions when P&P is idle. Restart, reset and erase are different:

- **Restart P&P** reloads the system and retains committed customer data and configuration. An interrupted individual race is not automatically restored.
- **Reset settings** returns configuration and networking to defaults while retaining committed History and records.
- **Erase everything** is destructive: it removes customer settings and records. Use only deliberately, after reading the confirmation.
- **Firmware update** uses the supported update procedure. Do not disconnect power during an update. The approved design retains customer data and attempts recovery or rollback if an update fails.

Only the controlling Browser can carry out the permitted sensitive operations, subject to confirmations and the settled non-racing state. If recovery mode appears, follow the supplied support instructions rather than repeatedly resetting the unit.

[Final support contact, update procedure, data backup/export availability, exact storage handling and power specifications require confirmation.]

## 11. Going further

**V1 includes five mode families:** Lap Race, Open Practice, Endurance, Timed Stage (rally/hill climb/sprint timing), and basic single-lane two-sensor Drag. The current engineering firmware does not yet implement all these functions: the release manual must be validated against the integrated product before distribution.

Timed Practice, specialist dual-beam or REU-dependent Drag functions, and additional hardware-dependent features remain future candidates. A displayed but unavailable capability may reflect unavailable equipment or an engineering build; it does not alter the agreed V1 release requirements. Later releases will have separate instructions when available.

## 12. Glossary

| Term | Meaning |
|---|---|
| P&P | Plonk & Play, the racing system. |
| Integrated Display / Taster | Built-in touchscreen for simple standalone races or auxiliary viewing. |
| Race Control / SMUGLAND | Browser interface used to control and view P&P. |
| Racer / MUG | Person racing; MUG also appears in historical project terminology for user interfaces. |
| SMUG | Historical P&P term for the controlling user/interface. |
| MINIMUG | Project term for simple standalone use without needing the full Browser interface. |
| MUGGLE Guide | Illustrated first-use installation and quick-start guide. |
| REU | Race Enhancement Unit; optional future hardware capability. |
| PB | Personal best eligible lap. |
| Start/Finish sensor | Detector used to register a car crossing the relevant track point. |
| GO | Authoritative instant racing begins. |
| Auxiliary display | Integrated Display used to present information while Race Control is in charge. |

---

## Editorial verification and release checklist — remove from printed edition

Draft source: [PP Design Specification](PP_DESIGN_SPEC.md), [Master Project Plan](MASTER_PROJECT_PLAN.md), frozen [V21 prototype](https://github.com/ConnalM/Plonk-and-Play/blob/b794e9e78642d586b107e6982d0372d7dc356b53/design/smugland/PP_Browser_v21_auxiliary_review.html), [owner design approval](https://github.com/ConnalM/Plonk-and-Play/issues/2#issuecomment-7039376953). The source specification takes precedence over prototype fixtures.

- Hardware: confirm box contents, supplied power rating, connectors, safety/maintenance, sensor geometry/orientation/mounting, cable routing and recommended starting grid. Replace illustration placeholders with actual product photographs/diagrams. Confirm actual expansion capability, not the 1–8 lane design target.
- Software: verify all named screens, paths, labels, mode availability and reset/confirmation behaviour against release firmware; obtain clean production screenshots, not fixture screenshots. Verify Browser preset names and actual controls, optional Practice sequence and false-start/repeat semantics, shared sound, auxiliary editor and storage fallback.
- Networking: verify shipped AP name/address, actual connection instructions on Taster, direct AP and external Wi-Fi, multiple clients, loss/reconnect and control acquisition. Do not print development IP addresses or passwords.
- Persistence/recovery: confirm history capacity if advertised, SD requirements, safe insertion/removal, storage warnings, update/rollback, backup/export and support instructions. Do not promise features not yet accepted.
- Terminology and packaging: provisional customer-facing names used in this draft; confirm branding, support/contact, accessibility and product/legal safety copy. No dimensions or electrical specifications invented.
- Scope: Timed Stage and basic single-lane Drag were expressly promoted to required V1 on 21 November 2026. Incorporate verified instructions for both in the final release manual after implementation, including sensor position, start/finish rules, classifications, false starts and results; until then refer to their approved design/manual drafts without claiming physical implementation. Timed Practice, dual-beam/REU enhancements and unrelated optional modes remain deferred. Gina's design approval is not implementation acceptance.
- Acceptance: WP8-02 physical testing, WP8-03/04/07/08 implementation and relevant WP6/storage/hardware acceptance remain gates. Cross-check against final release plan. Perform an unfamiliar-user walkthrough of the MUGGLE Guide with actual packaged hardware.
