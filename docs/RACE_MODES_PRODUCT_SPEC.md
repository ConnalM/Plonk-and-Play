# Plonk & Play™ — Race Modes Product Specification

**Status:** Committed product-design specification  
**Scope:** Agreed behaviour for Lap, Practice, Endurance, Rally and Drag.  
**Relationship:** Complements the architecture and first-implementation documents. Detailed RMS implementation remains deferred.

## 1. Mode hierarchy

The customer-facing mode hierarchy is:

- **LAP**
  - Race
  - Practice
- **ENDURANCE**
- **RALLY**
- **DRAG**

Normal circuit racing is called **Lap**, not Circuit.

## 2. Lap Race

### 2.1 Race definition

Lap Race is primarily defined by a target number of laps.

The three supported finish behaviours are:

1. **Winner finishes — all stop immediately.**
2. **Winner finishes — all others finish the current lap.**
3. **Winner finishes — all others complete the total race distance.**

These are the three normal Lap Race finish choices.

### 2.2 Live information

Typical live information includes:

- position;
- laps completed / target;
- last lap;
- best lap;
- gap ahead/behind;
- sectors where fitted;
- optional live current-lap time.

Sector feedback may briefly show +/- against the competitor's best.

**Fastest Lap** is a separate race statistic regardless of who wins and may generate a live callout.

### 2.3 Starts

Lap Race supports a conventional countdown using either **3 or 5 red lights**.

GO presentation can be:

- **Lights out** — reds illuminate sequentially, then all extinguish.
- **Green** — reds illuminate sequentially, then green indicates GO.

The delay after all reds may be:

- fixed and selectable;
- random within a selectable minimum/maximum;
- immediate where configured.

The interval between red lights may be configurable under Advanced.

The sequence is parameterised rather than hard-coded.

Audio may accompany the start using configured styles such as beeps, different pitches or spoken numeric countdown. A spoken countdown must not announce GO prematurely: numbers complete first, then the configured final delay, then GO.

Where a Power Module is fitted, track power may optionally be held off during the sequence and enabled at GO. Live-power starts remain available.

### 2.4 False starts

False-start detection is offered only where the configured physical Start/Finish arrangement can actually detect a pre-GO crossing.

Lap false-start responses may include:

- Off;
- Warning only;
- +1 lap;
- time penalty;
- Power penalty where a Power Module is fitted.

Hardware-dependent options may remain visible but locked/greyed with a short explanation rather than disappearing.

## 3. Practice

Practice is a proper subset of Lap, not a race with a fake winner.

Practice has no normal winner/finish condition and does not require a start sequence.

Practice may be:

- **Open**;
- **Timed**.

Recorded information may include:

- lap count;
- last lap;
- personal/session best;
- session fastest;
- sectors where fitted;
- best sectors;
- +/- versus personal best;
- theoretical best from the sum of best sectors.

The normal solo path should remain simple: **Practice → lane → Start**.

## 4. Endurance

### 4.1 Race definition

Endurance uses a configured duration rather than a target lap count.

Typical presets may include 5 min, 10 min, 30 min, 1 hour and Custom.

The winner is determined by distance/laps at expiry according to the configured finish behaviour.

### 4.2 Finish behaviours

Endurance has two normal finish choices:

1. **Stop at zero** — the race ends at 0:00. Classification uses completed laps and known progress where sector information permits it.
2. **Finish current lap** — at 0:00 each running MUG continues until its next Start/Finish crossing and then finishes.

There is no invented third timed-race finish mode.

For Finish Current Lap, competitors tied on completed laps can be separated by the order in which they complete the final lap.

### 4.3 Live and result information

Time remaining is prominent.

Typical live information includes:

- time remaining;
- position;
- current lap;
- last lap;
- best lap;
- gap;
- sectors where fitted.

Results include position, completed laps, appropriate total/finishing time, best lap, fastest lap overall, penalties and sectors where available.

### 4.4 Endurance penalties

False-start choices may include:

- Off;
- Warning;
- -1 lap in the final result;
- Power penalty where a Power Module is fitted.

A generic post-race +10 second penalty is not appropriate to fixed-duration scoring and should not be offered merely for consistency with other modes.

### 4.5 Countdown warnings

Time-expiry warnings are configurable.

A useful default may include 1 minute, 30 seconds, 10 seconds, 5-4-3-2-1 and zero.

Configuration may provide Default / Off / Custom and presentation by display, audio or both.

At zero:

- **Stop at Zero:** announce/display race over.
- **Finish Current Lap:** announce/display time expired/final lap and continue until each competitor finishes.

## 5. Rally

### 5.1 Normal loop stage

For a normal loop/circuit Rally stage:

1. Car is positioned behind the Start/Finish sensor.
2. Start sequence runs.
3. GO makes the stage live but the official stage timer remains at zero.
4. The MUG launches.
5. The first Start/Finish crossing starts the official timer.
6. The car completes the configured number of stage laps.
7. The final required Start/Finish crossing stops the timer.

Reaction time is not included in the stage time.

Crossing the sensor before GO is a false start.

A stage may be one or multiple laps: 1, 2, 3 or Custom.

Individual lap times are recorded automatically as splits, while the official stage result is the total stage time.

### 5.2 Single and multi-stage Rally

**Single Stage:** one timed stage/run; fastest adjusted time wins.

**Multi-stage Rally:** several stages/runs; lowest cumulative adjusted total wins.

The same physical track may be reused as nominally different stages.

Attempts/runs per stage may be 1, 2, 3 or Custom.

Scoring choices include:

- All runs count;
- Best run counts.

A Drop Worst option is not required unless a real product need emerges.

### 5.3 Point-to-point and Hill Climb

P&P also supports an A-to-B timing engine:

**GO arms run → first sensor starts timing → second sensor stops timing.**

Sensor roles can be reversed for another stage, allowing A→B and B→A without moving sensors.

This underlying timing behaviour can support point-to-point Rally, Hill Climb, Sprint and similar A-to-B uses.

Hill Climb is not currently specified as a fake technically distinct timing mode merely for marketing. Presentation/naming may later provide discipline-specific presets where useful.

### 5.4 DNF handling

DNF scoring is configurable and may include:

- fixed DNF time;
- slowest completed time + configurable penalty;
- exclude from overall classification;
- retry stage.

A DNF does not automatically prevent the MUG from taking part in later stages.

### 5.5 Event organisation

A Rally can contain:

- Rally name;
- MUGs;
- number of stages;
- per-stage settings.

Each stage may differ, for example loop N laps or A→B/B→A.

P&P maintains the cumulative classification.

Supported running orders:

- **Stage order** — all MUGs complete Stage 1, then Stage 2 etc. This is the normal/default organisation.
- **MUG order** — MUG 1 completes all stages, then MUG 2 etc. This is an Advanced option useful where the physical track remains unchanged.

### 5.6 Rally results

The main live/default result information is:

**Position | MUG | Stage time | Penalty | Rally total | Gap**

After each stage P&P can show the stage winner/fastest and updated overall classification.

Lap splits remain expandable rather than cluttering the main table.

Final classification is by lowest cumulative adjusted time.

Secondary statistics may include fastest stage, stage wins, best lap/split, penalties, DNFs and retries.

Detailed Results provide a stage-by-stage matrix.

After a stage the primary action is **NEXT STAGE**; at the end it becomes **EVENT RESULTS**.

## 6. Drag

### 6.1 Base hardware use

The standard two-sensor P&P package supports:

- ordinary two-lane Lap timing using one Start/Finish sensor per lane;
- **single-lane Drag** by repurposing Sensor 1 as Start and Sensor 2 as Finish.

### 6.2 Base staging

Base single-lane Drag uses the Start sensor itself to establish the staging point:

1. MUG moves the car forward until the Start sensor is triggered.
2. **STAGE** illuminates: the beam/start line has been found.
3. MUG rolls back slightly until the sensor clears.
4. **STAGE** extinguishes and the run is armed/ready.
5. The race cannot start while the Start sensor remains blocked.
6. At GO the MUG launches.
7. The car triggers the Start sensor almost immediately.
8. That trigger starts ET.
9. The Finish sensor stops ET.

The rollback amount is deliberately left to the MUG's hand/skill.

### 6.3 Reaction and ET

Base reaction time is:

**Start-sensor trigger time - GO time**

This is not a physical first-motion measurement, but is a valid and useful base-system reaction figure.

Reaction time and ET are separate results.

The base display presents reaction and ET to **0.01 s**. Internal timing may be finer. Greater displayed precision may be associated with higher-performance timing hardware.

### 6.4 False start

Triggering the Start sensor before GO is a false start.

The run may continue so ET is still recorded. Reaction time may therefore be negative.

### 6.5 Start tree

Drag supports:

- **Sportsman** — ambers illuminate sequentially;
- **Pro** — ambers illuminate together, followed by green.

### 6.6 Base results and speed

Base Drag results include:

- reaction;
- ET;
- false-start status.

If track length is entered, P&P may calculate whole-run average speed.

This is **not** presented as trap speed.

### 6.7 Future dual-beam / REU concept

A future higher-performance dual-beam module is parked, not part of the base design.

Provisional concept:

- two optical beams approximately 10 mm apart;
- at Start: Beam 1 = pre-stage, Beam 2 = stage;
- at Finish: Beam 1 = trap start, Beam 2 = finish/trap end;
- fixed known spacing avoids a separate trap-distance setup.

The 10 mm spacing is provisional and requires prototype validation.

This future option must not derail or complicate the base Drag implementation.

### 6.8 Drag display

Drag deliberately does not use the common Lap/Endurance/Rally scoreboard hierarchy and gets its own recognisably drag-racing visual identity.

The display progression is:

**STAGING → TREE → RUN → RESULT**

The start display uses a proper drag-racing **Christmas Tree** rather than reusing the common five-light Race Control Pod.

The full tree layout remains visible with hardware-dependent elements shown in place. On the base single-beam system, **PRE-STAGE is not falsely simulated**: the PRE-STAGE lamp/line remains visible but greyed/dormant because that measurement requires the future dual-beam REU. The normal STAGE indication remains active for the base staging procedure.

This follows the wider P&P product rule that expansion capabilities should normally remain visible but locked/dormant rather than disappear.

During staging and the start sequence, the tree becomes visually dominant and large enough to read while the MUG is concentrating on the car.

The three amber lamps operate according to the selected **Sportsman** or **Pro** sequence. Green indicates a valid GO; red indicates a false start.

After GO, the tree remains visible with the relevant green/red state while reaction time appears as soon as the Start sensor is crossed and ET runs prominently.

At Finish, the display transitions immediately to the run result, including:

- reaction time;
- ET;
- false-start/red-light status where applicable;
- whole-run average speed where track length is known.

Example valid result:

`REACTION 0.23 | ET 6.84 | AVG 42.7 mph`

Example false start:

`RED LIGHT | REACTION -0.08 | ET 6.71`

### 6.9 Base Drag staging-screen behaviour

The base staging interaction is deliberately simple:

1. **Waiting** — full tree visible; REU-only PRE-STAGE remains greyed/dormant; STAGE is unlit. Display prompts **MOVE FORWARD TO STAGE**.
2. **Start beam blocked** — STAGE illuminates and the prompt becomes **ROLL BACK**.
3. **Beam clears** — STAGE extinguishes because the car is now physically just behind the single beam. A prominent **READY** indication confirms that staging has succeeded and the tree is armed.
4. **Start sequence** — staging instructions disappear and the Christmas Tree becomes visually dominant. The selected Sportsman or Pro sequence runs to GREEN.
5. **Start crossing** — reaction time appears as soon as the car crosses the Start beam and ET begins.
6. **Finish** — the run result takes over, showing reaction, ET, false-start status and whole-run average speed where available. **RACE AGAIN** is the primary post-run action.

If the car rolls forward and blocks the Start beam again after reaching READY but **before the start procedure begins**, P&P simply returns to the **ROLL BACK** staging state. This is not a false start.

Once the start procedure has begun, a Start-sensor crossing before GREEN is a **RED LIGHT / false start** according to the agreed Drag rules.

Further Drag display details remain open for continued product design.

## 7. General principle

Where an additional software option costs essentially nothing and does not make the normal experience confusing, P&P should avoid artificial restriction. Less-common choices can live under **Advanced**.

> **Easy to start. Deeply configurable when you want it.**
