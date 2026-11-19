# Plonk & Play™ — consolidated design specification

**Status:** Owner-approved single design structure; migration in progress (19 November 2026). The [Design Constitution](DESIGN_CONSTITUTION.md) and [System Requirements](SYSTEM_REQUIREMENTS.md) govern this specification. [Master Project Plan](MASTER_PROJECT_PLAN.md) is the sole delivery/backlog authority, including v1 deferrals and post-v1 candidates. Acceptance tests and dated evidence remain separate. No new design decisions or promotion of post-v1 functionality arise from the consolidation.

**Terminology:** Integrated Display (formerly Taster), Racer (formerly MUG), Race Control / controlling Browser (formerly SMUG/SMUGLAND) are the provisional customer-facing names. Existing canonical firmware identifiers and historic labels remain unchanged.

## Contents

1. [System architecture](#system-architecture)
2. [Race Control and Race Engine](#race-control-and-race-engine)
3. [Inputs and outputs](#inputs-and-outputs)
4. [Communications and system time](#communications-and-system-time)
5. [Memory and storage](#memory-and-storage)
6. [Presentation](#presentation)
7. [Configuration](#configuration)
8. [Diagnostics and recovery](#diagnostics-and-recovery)
9. [Physical product](#physical-product)

## Authority and reconciliation rules

The design describes **one product**, with shared authority and client-specific restricted views. Current owner-approved decisions take precedence over earlier drafts, historic implementation scope statements or prototype evidence. Each migrated heading retains a stable legacy anchor and original source can be recovered from Git history. Completed work and deferred scope are governed by the master plan, not old phase wording. An unresolved conflict must be recorded there and resolved explicitly, never by silently selecting convenient text. Timing and competition rules belong to Race Control/Engine; local controls and visual presentation belong to Presentation; shared audio is a system facility under Presentation; configuration owns remembered setup and physical installation defaults. The scope of old implementation slices is not a present-day product limit.

## System architecture

<!-- INSERT:architecture -->

## Race Control and Race Engine

<!-- INSERT:race -->

## Inputs and outputs

<!-- INSERT:io -->

## Communications and system time

<!-- INSERT:comms -->

## Memory and storage

<!-- INSERT:storage -->

## Presentation

<a id="common-display-spec--plonk--play--common-display-specification"></a>
### Plonk & Play™ — Common Display Specification

**Status:** Committed product-design specification  
**Applies to:** Lap, Endurance and Timed Stage on the Browser and integrated
P&P displays.
**Exception:** Drag uses its own display design. The integrated Taster is a
restricted client of this same display/race authority; it may omit modes and
controls, but it does not define separate racing or timing behaviour.

<a id="common-display-spec--1-design-principle"></a>
#### 1. Design principle

P&P should scale from a solo MUG using one browser to a full competition using a main display, individual MUG screens and a separate Race Director screen.

The simple/default experience must remain non-threatening. Advanced functionality may be available without cluttering normal operation.

<a id="common-display-spec--2-full-browser-race-display"></a>
#### 2. Full Browser Race Display

The main display is intended for a monitor, TV, projector, tablet or full-screen browser.

The race display remains fundamentally one screen with changing states rather than changing pages during a race:

`READY → STARTING → RACING → FINISHING → FINISHED / RESULTS`

The start sequence does not replace the scoreboard with a separate start screen.

<a id="common-display-spec--21-shared-client-authority-and-timing"></a>
##### 2.1 Shared client authority and timing

The Browser and Taster are presentation/control clients of the same Race
Control, Race Engine and P&P System Time authority. See [Integrated Display standalone controls](#pp-taster-spec--standalone-controls) for its restricted local mode choices; the same authoritative race-mode contract applies.

Both clients consume the same authoritative state and presentation
events/facts. Neither client invents the GO instant, elapsed/remaining clock,
Endurance expiry, finish classification, finish time or overtime. Paused time
and an approved restart countdown do not add racing time.

Mode-specific finish policy and the shared timing/format definitions are
defined only in `RACE_MODES_PRODUCT_SPEC.md` §2, §4 and §4.6. The display
consumes those values; it does not restate or reinterpret the policies.

Presentation updates are event/state driven. A client may advance or render a
displayed clock from authoritative P&P timing and may animate a genuine active
event, but a redraw timer or local calculation must not establish race timing
authority or rebuild unrelated layout regions. Numeric fields and lane rows
retain stable fixed regions while values change.

<a id="common-display-spec--3-race-control-pod"></a>
#### 3. Race Control Pod

A permanent segmented area sits across the top of the full display, envisaged as a shallow/squashed half-hexagonal pod.

It contains five light positions, which remain visible but greyed when inactive.

During a start these become the race start lights. The system can use either three or five according to the selected start configuration.

The pod has five physical light positions; there is no separate sixth green lamp. The authoritative countdown uses either three or five red lights according to the configured start. The authoritative GO style is either **Lights Out** or **Green**, as selected in the Session Definition. With Lights Out, the configured red lights extinguish at GO and all five physical positions are grey/off and dormant during ordinary racing. With Green, all five physical positions turn green simultaneously at authoritative GO for exactly one second, then all five become grey/off and dormant during ordinary racing. A configured three-light start changes only the number of red countdown positions; Green still illuminates all five physical positions. Browser and physical presentation consume this authoritative state and do not create a sixth lamp or independently establish GO authority; they may render the authoritative countdown and GO state.

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

<a id="common-display-spec--4-main-scoreboard"></a>
#### 4. Main Scoreboard

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

Endurance and Timed Stage use the same visual architecture with mode-appropriate information. Mode-specific field meaning and finish semantics are defined in `RACE_MODES_PRODUCT_SPEC.md`; this specification governs their shared visual meaning and geometry.

<a id="common-display-spec--5-temporary-row-states"></a>
#### 5. Temporary Row States

The geometry of a lane row remains stable, but information can temporarily change emphasis.

Examples include:

- **Position change** — new position briefly emphasised.
- **Fastest lap** — relevant timing briefly highlighted.
- **Penalty** — warning treatment followed by a persistent penalty indicator where appropriate.
- **Final lap** — progress area receives a distinctive treatment.
- **Finished** — row changes into its finished state.

A finished MUG remains in its original lane row while other MUGs continue racing.

Example:

`LANE 2 | CONNAL | FINISHED | P1 | 25/25 | 03:42:18 | BEST 8.19`

This is necessary because some Lap finish rules allow other MUGs to continue after the winner has finished.

<a id="common-display-spec--6-track-length-scale-and-speed"></a>
#### 6. Track Length, Scale and Speed

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

<a id="common-display-spec--7-finish-behaviour-and-transition-to-results"></a>
#### 7. Finish Behaviour and Transition to Results

When the race has fully finished, the live display enters a short Finish Display.

Default duration: **5 seconds**, configurable including zero.

The Finish Display can provide suitable theatre — winner, finishing position, chequered treatment, winning time/laps etc.

A Race Director button press during this period skips the remaining delay and proceeds immediately to Results.

<a id="common-display-spec--8-results-screen"></a>
#### 8. Results Screen

Unlike the live scoreboard, Results are displayed in finishing order, not lane order.

The default Results screen should answer **who won and what happened?** immediately rather than overwhelming the MUG.

Typical information:

**Position | Lane/MUG | Progress | Time | Best Lap | Gap**

For Lap Race Results, progress and finishing information are separate fields.
Progress shows authoritative classified race distance against the configured
target where applicable. Time shows an entry's authoritative race-relative elapsed duration at its
permitted final crossing, or at its last completed lap when it was stopped
before a final crossing. An entry with no completed lap shows an em dash. Gap shows only information
supplied by P&P, such as authoritative laps behind. The Browser does not
replace one field with another or calculate classification, finish times or
time gaps.

Secondary information can include fastest lap, penalties and speed information where track data is available.

A valid finished Results screen presents the status context `RESULTS`, even though the finished session is not startable. This is presentation context and does not change authoritative startability. After **RACE AGAIN** returns P&P to READY, normal READY/startability presentation resumes.

A **Details / Analysis** view provides individual lap times, sectors, speeds, penalties and other recorded timing data.

Graphs may eventually live here but are not part of the default Results display.

<a id="common-display-spec--9-post-race-navigation"></a>
#### 9. Post-Race Navigation

The primary action is contextual:

- **RACE AGAIN** — standalone race; same setup again.
- **CONTINUE** — competition/event; advance to next scheduled race/heat/round.
- **NEXT STAGE** — Timed Stage multi-stage event.
- **EVENT RESULTS** — event has reached its conclusion.

Secondary actions such as **NEW RACE** and **HOME** are available where appropriate.

<a id="common-display-spec--10-personal-mug-display"></a>
#### 10. Personal MUG Display

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

Browser audio can likewise provide MUG-specific alerts. Personalised spoken
announcements using MUG names are deferred until after v1.

<a id="common-display-spec--11-race-director-and-solo-operation"></a>
#### 11. Race Director and Solo Operation

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

<a id="common-display-spec--12-multiple-simultaneous-displays"></a>
#### 12. Multiple Simultaneous Displays

Displays are clients, not timing authorities.

A four-MUG event could therefore have:

- 1 × Main Scoreboard
- 4 × Personal MUG displays
- 1 × Race Director display

or any smaller combination.

The System Controller remains responsible for timing, race logic and authoritative race state. Browser and Taster perform their own rendering, animations and audio as presentation responses; neither becomes a timing authority.

The [Product Facilities audio specification](#product-facilities-spec--3-audio-and-removable-storage)
governs destination fallback, priorities and playback reliability. Audio
must never delay racing. Alternating yellow PAUSED lights are silent.

Loss of a browser connection must never affect race timing or race state.

<a id="common-display-spec--13-yellow-and-red--display-principles"></a>
#### 13. Yellow and Red — Display Principles

Detailed RMS logic is deliberately deferred until software design.

The principles already agreed are:

Yellow / Track Call is a session option with three settings:

- **Off** — Yellow / Track Call is disabled for that session.
- **Request** — a MUG may request Yellow; the race continues while the requesting MUG and RD are notified and the RD accepts or dismisses the request.
- **Direct** — a permitted MUG Yellow input immediately calls Yellow / Track Call.

**Yellow Off does not disable the Race Director's basic PAUSE control.** PAUSE remains a core race-control intervention independently of whether the session uses a Yellow / Track Call procedure.

In Request mode, the Race Control Pod surround can pulse yellow while the five lights remain in their normal state.

**Yellow active:** five lights flash yellow. This represents the normal temporary track-call/interruption state and normally leads to **RESUME**.

**Red active:** five lights flash red. This represents a more serious Race Stopped state.

From Red, the Race Director may ultimately:

- **RESUME RACE** — preserve existing progress and continue.
- **RESTART RACE** — discard progress and begin the race again from zero, with confirmation.
- **ABANDON RACE** — terminate the race.

The precise timing/power behaviour and every possible transition are RMS implementation details to be specified later rather than allowed to derail the display design now.

---

**Product principle:** Easy to start. Deeply configurable when you want it.


<a id="common-display-spec--browser-ui-rules"></a>
#### Browser UI rules

<a id="common-display-spec--numeric-stability"></a>
##### Numeric stability
Numeric fields and live numeric display regions must have fixed dimensions appropriate to their expected values. Changing digits must not resize controls or move adjacent UI.

Use tabular numerals where appropriate for clocks, lap times, gaps, counters and similar changing race data.

General rule:

> **No UI jitter caused by changing race data.**

<a id="common-display-spec--numeric-adjustment"></a>
##### Numeric adjustment
Where a numeric value is commonly adjusted, use decrement / editable value / increment controls. Direct editing must remain available so large changes do not require repeated button presses.

<a id="common-display-spec--contextual-help"></a>
##### Contextual help
Use a consistent information control for terminology or consequences that may not be obvious.

- Desktop: hover may reveal the help.
- Touch: tap reveals the same help.
- Keep help concise, normally one or two sentences.
- Do not add help icons to self-evident controls merely for consistency.

Typical candidates include Drag Course Length, Timed Stage, Loop, Point-to-Point, finish behaviour, scale speed and false-start behaviour.

If a control requires a paragraph before a MUG can choose it, reconsider the control design rather than relying on a tooltip.

---

<a id="presentation-design-spec--plonk--play--presentation-design-specification"></a>
### Plonk & Play™ — Presentation Design Specification

**Status:** Accepted visual-direction specification  
**Applies to:** Browser presentation and integrated P&P display  
**Authority:** Product behaviour remains governed by the relevant product specifications. This document governs visual language and presentation consistency, not race logic. Browser and Taster share the same presentation meanings even where their layouts and controls differ.

<a id="presentation-design-spec--1-purpose"></a>
#### 1. Purpose

P&P must look like one product everywhere.

The integrated display, browser Race Control, Personal MUG display, spectator display, Results and configuration screens must share a recognisable visual language. Different devices may arrange and prioritise information differently, but they must not look like unrelated applications.

The presentation should feel like a professionally designed motorsport timing product rather than a generic web dashboard, hobby project or computer game.

Core principle:

> **One P&P look. Different screens. The information that matters now dominates.**

<a id="presentation-design-spec--11-cross-client-product-boundary"></a>
##### 1.1 Cross-client product boundary

See [shared client authority and timing](#common-display-spec--21-shared-client-authority-and-timing) and [Integrated Display standalone controls](#pp-taster-spec--standalone-controls). Visual design may vary with the device, never the authoritative race semantics.

<a id="presentation-design-spec--2-agreed-visual-direction"></a>
#### 2. Agreed visual direction

The accepted visual direction is:

- dark navy/charcoal primary surfaces;
- restrained P&P yellow as the principal brand/accent colour;
- clear white/off-white primary information;
- lane colours used where they communicate lane/competitor identity, not as general decoration;
- large, highly legible timing and race-state typography;
- clean panels with restrained rounding and borders;
- generous spacing and strong visual hierarchy;
- simple, consistent iconography;
- restrained motorsport cues rather than decorative racing clichés;
- presentation theatre used for genuine race events, not continuously.

The visual direction is intentionally **not**:

- carbon-fibre wallpaper;
- fake brushed metal;
- permanent chequered-flag decoration;
- seven-segment typography everywhere;
- neon/glowing controls;
- gratuitous animated cars;
- a dense telemetry dashboard;
- a collection of differently styled pages.

P&P should remain recognisably P&P even when all decorative imagery is removed.

<a id="presentation-design-spec--3-relationship-to-the-physical-product"></a>
#### 3. Relationship to the physical product

The browser and integrated P&P display should visually belong to the same physical product.

Where practical they should share:

- P&P mark/wordmark treatment;
- colour palette;
- typography hierarchy;
- panel geometry;
- timing-number treatment;
- status colours and meanings;
- Race Control Pod language;
- icons;
- event treatments such as Fastest Lap, Final Lap and Finish.

They do not need identical layouts. The integrated display, phone, tablet, laptop and TV have different jobs and available space.

The integrated display may be self-contained when no controlling SMUG is
connected, but its controls still submit to the same Race Control authority as
the Browser. A connected controlling SMUG may take precedence under the
existing lease/permission contract; this changes who may request an operation,
not what the operation means.

<a id="presentation-design-spec--4-information-hierarchy"></a>
#### 4. Information hierarchy

P&P must show **what matters now** rather than everything it knows.

During a race the primary hierarchy is generally:

1. race-control state and safety/control signals;
2. position and race progress;
3. immediately useful timing information;
4. secondary analysis/detail.

Secondary information must not compete visually with the current race state.

Detailed analysis belongs in Results/Details rather than being forced onto the live race display.

<a id="presentation-design-spec--5-shared-presentation-components"></a>
#### 5. Shared presentation components

Browser and integrated-display implementations should be assembled from a common conceptual component vocabulary wherever applicable.

Initial component set:

- **P&P Header / Identity Mark**
- **Race Control Pod**
- **Status Banner**
- **Mode Card**
- **Lane Row / Lane Card**
- **Position Indicator**
- **Race Progress**
- **Timing Value**
- **Gap Value**
- **Fastest Lap treatment**
- **Final Lap treatment**
- **Penalty treatment**
- **Finished treatment**
- **Result Row**
- **Primary Action**
- **Secondary Action**
- **Information / Help control**
- **Numeric adjustment control**

A component should retain the same meaning and basic visual identity wherever it appears, while being allowed to scale or simplify for the available display.

This component vocabulary is also the intended foundation for any future configurable Screen Designer. A future SMUG should rearrange supported P&P components rather than hand-author arbitrary HTML/CSS.

<a id="presentation-design-spec--6-colour-rules"></a>
#### 6. Colour rules

<a id="presentation-design-spec--61-brand-colour"></a>
##### 6.1 Brand colour

P&P yellow is the principal accent and selection colour. It should be distinctive but restrained; covering large portions of every screen in yellow would weaken it.

<a id="presentation-design-spec--62-lane-colours"></a>
##### 6.2 Lane colours

Lane colours communicate lane identity and related race information. They should not become a general page theme.

A lane colour may be used for items such as:

- lane number/identity marker;
- a narrow row accent;
- a relevant event highlight;
- Personal MUG identity.

Large saturated lane-colour backgrounds should be avoided where they reduce readability or make multi-lane displays visually noisy.

<a id="presentation-design-spec--63-status-colours"></a>
##### 6.3 Status colours

Status colour must have consistent meaning across screens. Yellow/Red race-control presentation remains governed by `PP_DESIGN_SPEC.md#common-display-spec--plonk--play--common-display-specification`.

Colour must not be the only means of communicating a critical state; text, iconography or light state should also make the meaning clear.

<a id="presentation-design-spec--7-typography-and-numeric-presentation"></a>
#### 7. Typography and numeric presentation

Race information must remain readable at a glance and at the expected viewing distance.

Use a compact, modern sans-serif family suitable for screen display. Exact production fonts are an implementation choice until separately frozen, but browser and integrated display should use visually compatible typography.

Typography should distinguish clearly between:

- race state;
- position;
- race progress;
- live timing;
- labels/secondary information;
- configuration/help text.

Changing race numbers must use tabular/fixed-width numeral behaviour where appropriate.

Existing rule from `PP_DESIGN_SPEC.md#common-display-spec--plonk--play--common-display-specification` remains mandatory:

> **No UI jitter caused by changing race data.**

Timing-field definitions and number formats are common across Browser and
Taster. Live elapsed/remaining race timers use whole seconds and continue
beyond 3599 without changing units. Completed race/finish durations use
unbounded-minute `mm:ss:xx` with hundredths; Endurance live overtime uses
`+0`, `+1`, `+2` and so on; and a lane's frozen final offset uses
`+mm:ss:xx`, such as `+00:18:42`. The Taster may omit fields outside its
restricted mode set but may not reinterpret or reformat their authoritative
meaning. Stable regions must reserve sufficient width for these values.

Decorative motorsport fonts may be used sparingly for branding if appropriate, but not for dense timing information.

<a id="presentation-design-spec--8-live-race-display"></a>
#### 8. Live race display

The existing Common Display Specification remains authoritative for live race content and behaviour.

Visually:

- the Race Control Pod is clearly identifiable and remains in a stable location;
- lane rows remain geometrically stable during live racing;
- position changes within a lane row rather than moving the row;
- position and progress are visually dominant;
- Last, Best and Gap are clearly secondary;
- temporary events alter emphasis without rebuilding the whole screen;
- animation must not interfere with reading current race information.

Browser and Taster update from authoritative state changes and event/fact
notifications. They may advance or render displayed clock values from
authoritative P&P timing. Stable lane rows and numeric regions remain in place;
only the affected regions change. Frequent redraw is reserved for an active
event animation and must restore the background beneath moving content. A
client must not independently establish race timing, GO, pause, expiry, finish
or overtime authority, or use presentation refresh as a substitute for it.

Race theatre is appropriate for meaningful moments such as:

- start sequence;
- position change;
- fastest lap;
- final lap;
- finish;
- Yellow/Red race control.

Continuous animation for its own sake is not part of the P&P visual language.

<a id="presentation-design-spec--9-personal-mug-display"></a>
#### 9. Personal MUG display

The Personal MUG display is a glanceable racing instrument, not a miniature copy of the full scoreboard.

Position and race progress dominate.

Last Lap, Best Lap and Gap remain secondary. Personal alerts may temporarily dominate as already specified in `PP_DESIGN_SPEC.md#common-display-spec--plonk--play--common-display-specification`.

A phone held or positioned several feet away should remain useful without requiring the MUG to read small labels while driving.

Portrait and landscape presentations may arrange the same information differently while retaining the same hierarchy and visual identity.

<a id="presentation-design-spec--10-results"></a>
#### 10. Results

Results switch from live fixed-lane presentation to finishing order as already specified.

The visual hierarchy should answer immediately:

> **Who won, and what happened?**

The winner should be obvious without excessive celebration or obscuring the other results.

Detailed lap, sector, speed and penalty analysis remains secondary and may live behind a Details / Analysis view.

Controls shown in visual prototypes are not automatically product requirements. In particular, History is automatically recorded according to the governing history specification; a mock-up button such as `SAVE RESULTS` must not create a new manual-save requirement.

<a id="presentation-design-spec--11-home-and-mode-selection"></a>
#### 11. Home and mode selection

Home should present the supported race modes clearly using the common Mode Card component.

Lap Race remains the normal/dominant starting choice in accordance with the browser-flow specification. Other supported modes remain visible without making the Home screen feel like a configuration menu.

The Taster may restrict this choice surface to **LAPS** and **ENDURANCE**.
That restriction changes available controls only; it does not rename or alter
the canonical Browser race modes or finish policies.

Mode cards may use simple icons and one short explanatory phrase. They should not contain paragraphs of help.

<a id="presentation-design-spec--12-configuration-screens"></a>
#### 12. Configuration screens

Configuration uses the same P&P visual language but may be denser than racing screens.

Configuration should favour conventional, understandable controls over decorative novelty.

Advanced settings must not visually overwhelm normal setup. Contextual help and numeric controls follow the rules in `PP_DESIGN_SPEC.md#common-display-spec--plonk--play--common-display-specification`.

The fact that a configuration page can fit more information on a laptop does not justify displaying every available option simultaneously.

<a id="presentation-design-spec--13-responsive-presentation"></a>
#### 13. Responsive presentation

P&P is one presentation system, not separate unrelated applications for each screen size.

Responsive layouts should adapt the shared components and hierarchy approximately as follows:

- **Phone portrait:** Personal MUG / focused single-user presentation; very large glanceable values.
- **Phone landscape:** compact Personal MUG or compact race presentation.
- **Tablet:** full Race Control and normal setup/results.
- **Laptop/desktop:** full Race Control, configuration and detailed analysis.
- **TV/projector:** spectator-first scoreboard with controls normally absent.
- **Integrated P&P display:** local self-contained presentation optimised for its physical screen and controls.

These are presentation priorities, not hard device restrictions.

<a id="presentation-design-spec--14-multiple-simultaneous-displays"></a>
#### 14. Multiple simultaneous displays

Different connected displays may legitimately show different views of the same authoritative race.

For example, at the same moment P&P may show:

- full scoreboard on a TV;
- Race Control on a tablet;
- Personal MUG views on phones;
- local race state on the integrated display.

All should unmistakably belong to the same P&P system.

The browser remains a presentation client. Visual effects, layout and animations must never become timing authority or alter authoritative race state.

<a id="presentation-design-spec--15-visual-prototypes"></a>
#### 15. Visual prototypes

Visual mock-ups are design references, not behavioural specifications.

A mock-up may contain invented example names, cars, numbers, controls or layouts solely to communicate visual direction. Such content must not silently override the governing product specifications.

Before implementing functionality inferred from a mock-up, check the relevant product specification.

The currently accepted design-board direction demonstrates:

- dark P&P visual language;
- yellow brand accents;
- clean mode cards;
- stable multi-lane timing rows;
- strong Personal MUG hierarchy;
- coherent Race Control, Results and spectator presentations;
- shared component styling across different device formats.

It does **not** freeze exact pixels, exact fonts, example data, decorative car graphics or any invented controls shown by the concept artwork.

<a id="presentation-design-spec--16-accessibility-and-practical-use"></a>
#### 16. Accessibility and practical use

P&P screens are used while people are watching and driving slot cars, often at a distance and in imperfect lighting.

Therefore:

- important text and numbers require strong contrast;
- critical information must not rely on colour alone;
- touch targets must be comfortably usable;
- important race values should not require close reading;
- decorative elements must never obscure timing information;
- motion should be brief and purposeful;
- layouts must tolerate longer MUG names and realistic numeric values without collapsing.

<a id="presentation-design-spec--17-implementation-boundary"></a>
#### 17. Implementation boundary

This specification deliberately freezes the **visual direction and consistency rules**, not a pixel-perfect final UI.

Exact spacing tokens, colour values, fonts, icon set, animation timings and component dimensions should be established through implementation/prototyping and then recorded once proven.

Do not delay race-engine or product-behaviour work merely to perfect cosmetic detail.

Equally, do not allow temporary development UI to become the product appearance by accident.

---

**Presentation principle:** Professional enough to look like a product; simple enough to remain Plonk & Play™.

---

<a id="pp-taster-spec--pp-taster-specification"></a>
### P&P Taster Specification

<a id="pp-taster-spec--purpose"></a>
#### Purpose

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

<a id="pp-taster-spec--cross-client-contract-alignment"></a>
#### Cross-client contract alignment

The Taster is a controller-local presentation and control client over the
normal P&P authority. It does not create a second Race Control, Race Engine,
timing, results or persistence authority. The canonical race behaviour is
defined in `RACE_MODES_PRODUCT_SPEC.md` and the shared presentation rules are
defined in `PP_DESIGN_SPEC.md#common-display-spec--plonk--play--common-display-specification` and `PP_DESIGN_SPEC.md#presentation-design-spec--plonk--play--presentation-design-specification`; this
document defines only the Taster's restricted scope, controls and layout.

Where this document is silent, those canonical contracts apply. A different
screen size or control surface may omit features, but it may not reinterpret
race state, timing, finish policy, result meaning or event presentation.

The Taster leaves its Welcome state only after configuration has loaded and
P&P is genuinely capable of racing for the selected installation. Optional
services, including Browser connectivity and official record storage, must not
block a standalone Taster race when the required race capabilities are healthy.

<a id="pp-taster-spec--factory-taster"></a>
#### Factory Taster

The factory/default Taster uses the standard two-sensor, two-lane base installation in [Out-of-box base configuration](#configuration-and-identity-spec--14-out-of-box-base-configuration). This defines the sensor roles, physical arrangement and capability limits. On the compatible standard installation no sensor-role, lane-count or race-mode setup is required.

Factory remembered Race Setup (approved 18 November 2026):
- LAPS selected;
- target: 10 laps;
- Endurance duration: 5 minutes.

<a id="pp-taster-spec--standalone-controls"></a>
#### Standalone controls

Taster should remain comparable in complexity to a basic Scalextric-style lap counter.

The intended local controls are deliberately simple. Taster must not require the SMUG merely to choose an ordinary race distance or duration.

The local touchscreen is the only Taster control surface. It provides the
simple two-lane experience directly without a rotary encoder, physical control
buttons or a browser.

<a id="pp-taster-spec--ready"></a>
##### READY

Taster provides exactly two locally selectable race types:

- **LAPS** — local distance adjustments select 1 to 999 laps;
- **ENDURANCE** — 1 to 999 minutes.

These are the only Taster race modes. Open Practice and Timed Stage are not
offered on the local touchscreen.

On first/factory use, READY uses the [factory settings](#pp-taster-spec--factory-taster).

Thereafter the last selected race type, the last selected lap distance and the
last selected Endurance duration are retained through the existing Memory/Store
boundary and restored after power-off/restart.

At READY, the touchscreen provides:

- a LAPS/ENDURANCE selector;
- an adjustment control for the selected value, wrapping between 1 and 999.
  Ten-step repeats reach the boundary before wrapping on the following step;
- a START control for the displayed race.

Selecting LAPS adjusts laps; selecting ENDURANCE adjusts minutes.

START is not a second Taster-specific race mechanism. Once requested, the selected Taster setup is submitted to the normal P&P start procedure and normal Race Control/session machinery.

The Taster shows a shared Lap Race target above 999 as supplied by the
Browser or restored from storage, without altering it. Opening the screen,
selecting a mode or adjusting Endurance duration leaves that lap target intact.
However, standalone Taster START must reject a displayed Lap Race target above
999 and require an explicit local distance adjustment before it can start.
The first explicit local distance adjustment enters the local range at 999.
The existing hold-to-adjust flow submits the new value on release. Browser-
controlled races may use the shared positive 32-bit lap-target range; this
Taster-only start boundary must never rewrite the shared setting silently.

Taster setup is translated into the normal race modes and finish policies:

- LAPS uses `LapRace` with `CompleteFullRaceDistance`;
- ENDURANCE uses `Endurance` with `CompleteCurrentLap`.

The Browser may expose additional modes, but the Taster never relabels or
redefines them. Its ENDURANCE choice is the normal P&P `Endurance` mode with
the canonical Finish Current Lap policy.

<a id="pp-taster-spec--during-a-race"></a>
##### During a race

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

<a id="pp-taster-spec--finished"></a>
##### FINISHED

The final result remains displayed indefinitely until the MUG acts.

- **RACE AGAIN** returns to READY with the previous mode and value retained;
- a separate **START** selection is required to begin the next countdown and
  race.

The standalone Taster result is a sealed in-memory result view, not a local
History screen. **RACE AGAIN** discards that displayed standalone result when
the authoritative operation completes and READY is restored; it does not
automatically start another race.

Taster must not grow into a general menu-driven recreation of the browser interface. If configuration becomes menu-heavy or requires navigating levels, it belongs on the SMUG.

<a id="pp-taster-spec--what-the-mug-sees"></a>
#### What the MUG sees

The Taster preserves the original Wokwi screen composition: the P&P graphical
layout, five start lights and two stable lane rows. Moving cars and animated
flags remain deferred; the static presentation below supersedes the earlier
lane-animation restart proposal. The Taster uses
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

The accepted physical Taster timer presentation is fixed-position `MM:SS`:
two-minute digits, a stationary colon and two-second digits. Digits are drawn
into fixed cells so changing values never shift horizontally. This is a local
presentation convention; the authoritative time remains the shared P&P value.

An authoritative lap or finish event may give only the affected lane a 450 ms
brightness pulse. Where brightness cannot be applied, use its perimeter
highlight instead. The other lane remains undisturbed. A finished winner keeps
a static wreath; it is not an animation. Graphics and sound are Presentation
responses to authoritative race events only and have no timing or Race Engine
authority.

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

<a id="pp-taster-spec--smug-configurable-taster"></a>
#### SMUG-configurable Taster

The Factory Taster is only the out-of-box default.

Using the browser, a SMUG may configure a different supported Taster for subsequent standalone use. This allows the full interface to perform setup once while leaving later operation simple enough for MUGs or MINIMUGs without supervision.

The controlling Browser's Integrated Display settings allow editing the remembered mode, lap target and Endurance duration directly. They use the shared remembered Race Setup, not an independent conflicting preset store. Lane display names and built-in graphics can be set independently; blank names use generic lane labels. A configured Taster must remain self-contained: the browser must
not allow a Taster configuration that subsequently requires a browser merely
to run it.

RESTORE FACTORY TASTER restores the [factory remembered Race Setup](#pp-taster-spec--factory-taster), factory lane labels and graphics. It is distinct from system factory reset. It preserves shared
audio preferences, existing track configuration and saved racer assignments.
Standalone operation still requires a compatible two-lane installation.

<a id="pp-taster-spec--factory-car-graphics"></a>
##### Factory car graphics

The factory Taster provides the original green MGB and red E-Type side-profile
graphics, assigned independently to Lane 1 and Lane 2. SMUGLAND may select a
different graphic for either lane only from P&P's built-in graphic set. Custom
image upload is not part of this contract. Lane graphic selections are
persistent Taster configuration and are restored after restart; they are not
racer identities and are not assigned through the MUG catalog. They do not alter lane assignments or race timing.

<a id="pp-taster-spec--control-hierarchy-and-records"></a>
##### Control hierarchy and records

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

<a id="pp-taster-spec--minimug-principle"></a>
#### MINIMUG principle

A SMUG should be able to configure the Taster in the browser and then leave MINIMUGs to use the resulting standalone race without needing the browser or supervision for normal operation.

The usability test is intentionally simple:

> A MINIMUG unfamiliar with P&P should be able to switch it on, understand the local display, start the race and race again without needing to learn a menu system.

<a id="pp-taster-spec--browser-boundary"></a>
#### Browser boundary

Taster is the standard two-sensor/two-lane standalone experience.

If the installation uses four lanes or otherwise uses additional sensors/hardware beyond the standard two-sensor Taster arrangement, configuration and race operation use the browser-based full P&P interface.

The local display does not need to become a general-purpose interface for arbitrary expanded hardware.

<a id="pp-taster-spec--connection-guide"></a>
#### Connection guide

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

---

<a id="browser-flow-results-history-spec--browser-flow-results-and-history"></a>
### Browser Flow, Results and History

<a id="browser-flow-results-history-spec--home"></a>
#### Home

The browser Home screen presents the five principal modes as visible selectable cards/tiles:

- **Lap Race** — visually dominant/default;
- **Practice**;
- **Endurance**;
- **Timed Stage**;
- **Drag**.

Do not hide modes in a dropdown merely to save space.

Secondary Home actions include **Race Again**, **Results & History** and **Setup**.

<a id="browser-flow-results-history-spec--browser-presentation-architecture"></a>
#### Browser presentation architecture

P&P ships with a small number of good **out-of-the-box (OOTB) race-screen designs** rather than requiring a MUG or SMUG to design a screen before racing. The initial target is at least a clear **Classic** race display and a more information-rich **Race Control** display.

The standard Browser race displays shall be constructed from **reusable presentation components** using the same declarative screen-layout model intended for future SMUG-editable screens. OOTB screen designs are presets, not separately hard-coded interfaces.

The presentation component set may include, as appropriate to the active mode and installed capabilities, items such as MUG name, car name, lane, position, lap count, last lap, best lap, gap, race clock, sector information, fuel, reaction time, start lights and race/flag status.

The layout model shall support three display classes:

- **Mobile**;
- **Tablet**;
- **PC / large display**.

A screen design may share presentation choices and data bindings across those classes while allowing component position and size to differ by display class. A SMUG should therefore be able to customise a screen without requiring a PC layout to be mechanically squeezed onto a phone.

The future **Screen Designer** shall edit this declarative presentation model rather than HTML or executable application code. Its intended interaction is a constrained visual editor: choose P&P components/fields from a palette, position and resize them, and adjust permitted presentation properties such as typography, colours, backgrounds, lights and simple effects.

A SMUG shall be able to duplicate an OOTB preset and customise the copy. OOTB presets remain available for straightforward recovery and use.

This does **not** make the Screen Designer a v1 implementation requirement. The architectural requirement for v1 is that ordinary Browser race screens are built from reusable components and a declarative layout/preset model so that later SMUG editing extends the same system rather than requiring the Browser UI to be rewritten.

Custom screens remain presentation only. Their available live data comes from P&P's externally presentable state/Noticeboard and standard presentation interfaces. A custom layout does not acquire authority over Race Control, Race Engine or other operational state merely because it displays their information.

<a id="browser-flow-results-history-spec--lap-race-setup"></a>
#### Lap Race setup

The normal setup presents:
- lane/MUG assignment;
- optional car only when car recording is enabled;
- SWAP LANES;
- target laps using decrement / editable value / increment;
- finish behaviour;
- MORE RACE OPTIONS;
- a visually dominant START control.

More Race Options expands without requiring a separate page and may contain start lights, GO style, final delay, false-start behaviour and audio. Advanced contains less commonly adjusted timing details.

Settings auto-save where intention is unambiguous.

<a id="browser-flow-results-history-spec--practice-setup-and-control"></a>
#### Practice setup and control

Practice offers **Open** or **Timed** as visible either/or choices.

Open Practice continues until SMUG/Race Control selects **END PRACTICE** in
the normal Browser. This presentation action invokes the authoritative
Practice **END SESSION** operation.

Timed Practice ends automatically at its configured duration; SMUG may also end it early.

Session-level controls belong to SMUG/Race Control, not ordinary MUG displays.

<a id="browser-flow-results-history-spec--endurance-setup"></a>
#### Endurance setup

Endurance setup includes:
- MUG/lane assignment;
- editable duration;
- the two established finish behaviours: **Stop at zero** or **Finish current lap**;
- More Race Options.

Time remaining is dominant during the race.

<a id="browser-flow-results-history-spec--active-session-controls-frozen"></a>
#### Active-session controls (frozen)

The normal Browser exposes only the controls appropriate to the authoritative
session lifecycle and mode. P&P remains responsible for validating and
executing every operation; these rules define the normal Browser presentation
only.

| Authoritative session | Normal Browser controls |
| --- | --- |
| Active Lap Race or Endurance | **PAUSE** only |
| Paused Lap Race or Endurance | **RESUME**, **RESTART**, **ABANDON** |
| Active Open Practice | **PAUSE** only |
| Paused Open Practice | **RESUME**, **RESTART**, **END PRACTICE** |

Competitive **ABANDON** is shown only while a Lap Race or Endurance session is
authoritatively paused. It requires confirmation and uses the existing
authoritative P&P abandonment path. It creates no official Result, History
entry or Finish Display, and returns through authoritative state to READY/Home.
At the P&P operation boundary this existing competitive abandonment may retain
the operation name **END RACE**; **ABANDON** is the normal Browser control
label.

**END PRACTICE** is shown only while Open Practice is authoritatively paused.
It uses the existing authoritative Practice completion semantics and proceeds
to Practice Summary. Practice does not show **ABANDON**.

Practice Summary is the completed summary of that Practice session. It offers
**PRACTICE AGAIN** as the primary action and **HOME** as the secondary action.
PRACTICE AGAIN asks P&P to prepare a new Open Practice using the retained
relevant setup, racers and lanes; it does not resume or resurrect the ended
Practice and does not start automatically. HOME leaves the summary and returns
to the normal READY/Home experience without creating another Practice.

The Browser must not implement these transitions as local state. A reload,
reconnect or second client renders the controls from the current authoritative
P&P lifecycle and mode.

<a id="browser-flow-results-history-spec--timed-stage-setup"></a>
#### Timed Stage setup

Timed Stage first offers two clear geometry choices:
- **Loop**;
- **Point-to-Point**.

It then supports **Single Stage** or **Multi-Stage Event** without requiring a motorsport-discipline label.

Loop setup includes MUG, stage laps and Runs.

Point-to-Point includes MUG, A→B/B→A direction and Runs.

Where multiple Runs are configured, established scoring choices such as All Runs Count / Best Run Counts may be offered.

A Multi-Stage Event is a lightweight collection of Loop and/or Point-to-Point stages with event progress and cumulative results. Do not expand v1 into a general competition-management system.

<a id="browser-flow-results-history-spec--drag-setup"></a>
#### Drag setup

Drag setup includes:
- MUG;
- Sportsman or Pro start tree;
- optional contextual course length and scale information;
- STAGE as the primary action.

Selecting Drag proposes the standard two-sensor base roles contextually: Sensor 1 Start, Sensor 2 Finish. Do not ask SMUG to reassign those sensors unnecessarily. When START is accepted, those Drag roles are frozen into the Session Definition for that session; the Input Module itself does not acquire Start/Finish meaning.

Drag remains usable with no length or scale configured. Missing optional values may be added directly from Drag setup.

<a id="browser-flow-results-history-spec--immediate-lap-race-result"></a>
#### Immediate Lap Race result

The primary Results screen should answer who won without becoming an analysis dashboard.

Lap Race Results use the columns **POSITION | LANE/RACER | PROGRESS | TIME |
BEST | GAP**. PROGRESS is authoritative classified race progress against the
configured target where applicable. TIME is the authoritative per-entry
race-relative elapsed duration at that entry's permitted final crossing, or at
its last completed lap when the race stopped that entry before a final crossing.
A racer with no completed lap has no TIME and shows an em dash. The Browser
never derives this value. GAP uses only authoritative gap information supplied by P&P;
the Browser does not calculate classification or substitute a finish time for
progress or gap.

Show:
- finishing position;
- MUG or lane;
- total result/finish time or gap as appropriate;
- each MUG's Best Lap;
- overall Fastest Lap;
- RACE AGAIN;
- HOME;
- DETAILS.

DETAILS exposes the lap-by-lap data and other available information such as penalties, sectors or speed where relevant.

Results are stored automatically. There is no routine Save Results action.

<a id="browser-flow-results-history-spec--results--history"></a>
#### Results & History

History is a simple newest-first list of completed **official** sessions across
the supported modes. Standalone Taster results are deliberately excluded from
this Browser History because they are local, non-official results; a
SMUG-controlled session uses the normal History path.

A basic mode filter may be provided:
**All | Lap | Practice | Endurance | Timed Stage | Drag**.

Selecting a history item reopens its result/details.

Do not turn v1 History into a general statistics/database-management interface.

<a id="browser-flow-results-history-spec--rolling-history-buffer"></a>
##### Rolling history buffer

History is a fixed-size rolling buffer.

When the buffer is full, storing a new completed session automatically discards the oldest retained session. The MUG is not required to manage storage or respond to capacity warnings during normal use.

The exact factory buffer size is an implementation choice to be set after realistic session-size/storage testing.

Persistent record summaries are independent of the rolling session buffer. Expiry of an old session must not make a valid PB or track record revert merely because its originating session has fallen out of History.

The Browser must not infer official eligibility from the visible result or from
the client that is currently connected. Race Control freezes the trusted
session source and record policy at START, and the Race Engine applies that
policy before History, PB or Track Record persistence.

<a id="browser-flow-results-history-spec--v1-records"></a>
##### v1 records

Keep records deliberately modest:
- personal fastest lap per MUG;
- overall fastest lap / track record.

Store useful underlying race data so richer statistics can be added later without making them v1 requirements.

<a id="browser-flow-results-history-spec--storage-implementation-boundary"></a>
#### Storage implementation boundary

The product requirement is **persistent structured history**, not a particular storage engine.

SQLite, structured files or another suitable implementation may be evaluated during implementation/prototyping. Browser behaviour must not depend on that choice.

General principle:

> **Design broadly. Implement narrowly.**

<!-- INSERT:presentation -->

## Configuration

<a id="configuration-and-identity-spec--plonk--play--configuration-and-identity-specification"></a>
### Plonk & Play™ — Configuration and Identity Specification

**Status:** Committed product-design specification  
**Scope:** Basic v1 MUG, car and lane configuration.  
**Principle:** Identity enhances P&P; it must never be required to use P&P.

<a id="configuration-and-identity-spec--1-immediate-use-without-setup"></a>
#### 1. Immediate use without setup

P&P must remain genuinely Plonk & Play.

A new/default two-lane system can operate as:

- **Lane 1 — MUG 1**
- **Lane 2 — MUG 2**

The user does not have to create profiles, cars or a database before racing.

<a id="configuration-and-identity-spec--2-persistent-mug-identity"></a>
#### 2. Persistent MUG identity

A saved MUG has:

- a permanent internal ID;
- a user-visible name.

For v1, **Name is the only required profile field**.

The internal ID, not the displayed name, owns historical identity. Renaming a MUG must therefore not orphan or split their previous results.

Richer profile information may be added later without changing this identity model.

<a id="configuration-and-identity-spec--3-selecting-mugs"></a>
#### 3. Selecting MUGs

Tapping a lane's MUG name opens a simple selection list/dropdown containing saved MUG names plus appropriate temporary/default choices.

Example:

- Connal
- Hazel
- Baird
- Freya
- Guest
- No MUG / Clear
- **+ New MUG**

A MUG already assigned to another active lane remains visible but is greyed/unavailable rather than silently disappearing.

Selecting **No MUG / Clear** returns the lane to its anonymous/default identity such as **MUG 1**.

<a id="configuration-and-identity-spec--4-text-entry-clear-control"></a>
#### 4. Text-entry Clear control

Where clearing an editable text value is a sensible operation, P&P should provide a one-action **Clear** control rather than requiring repeated backspace/delete actions.

This applies to MUG-name entry and should be treated as a general UI principle.

<a id="configuration-and-identity-spec--5-guest-mugs"></a>
#### 5. Guest MUGs

A Guest can race without being added permanently to the saved MUG list.

Guest name is optional:

- blank → display **Guest**;
- entered, e.g. **Dave** → use Dave for the current race/session without automatically creating a permanent profile.

A temporary named Guest can be promoted directly to a permanent saved MUG without re-entering the name.

This may be offered while entering the Guest name (for example **Remember Dave**) and/or after the race (**Remember this MUG**).

General principle:

> Temporary identity data should be promotable to permanent identity without requiring re-entry.

<a id="configuration-and-identity-spec--6-cars"></a>
#### 6. Cars

Car recording is optional and should not clutter the normal setup screen by default.

Cars are independent entities. A car does **not** belong permanently to a particular MUG.

When car recording is enabled, a race entry may reference an optional saved car.

Therefore the conceptual relationship is:

**Race Entry = MUG + Lane + optional Car**

The same car may be driven by different MUGs in different races/heats.

This supports shared cars, including groups of younger/informal racers, without distorting the data model.

For v1, if car profiles are exposed, a simple user-visible car name is sufficient. Manufacturer/model/year/catalogue-style data is not required.

<a id="configuration-and-identity-spec--7-lane-assignment"></a>
#### 7. Lane assignment

For a normal two-lane Lap setup, the presentation should remain approximately:

**LAP RACE — 10 laps**

- **Lane 1 — Connal**
- **Lane 2 — Hazel**

**START**

Changing a MUG should normally require only selecting a different name from the lane dropdown.

<a id="configuration-and-identity-spec--8-swap-lanes"></a>
#### 8. Swap Lanes

Provide a quick **SWAP LANES** action.

This exchanges the complete lane entries, including the MUG and optional selected car where car recording is enabled.

Example:

**Connal — Lane 1 | Hazel — Lane 2**

becomes:

**Hazel — Lane 1 | Connal — Lane 2**

This provides useful manual lane rotation in v1 without requiring a full Competition Mode.

<a id="configuration-and-identity-spec--9-remembering-the-last-setup"></a>
#### 9. Remembering the last setup

P&P automatically remembers the most recently used straightforward race setup, including MUG/lane assignment and normal race settings.

Returning to the same mode should therefore normally present the previous useful setup, allowing regular users to reach START with minimal interaction.

A separate Save Setup / Load Setup system is not required for v1.

Named presets may be added later if real use demonstrates a need.

<a id="configuration-and-identity-spec--10-future-compatibility-without-v1-scope-creep"></a>
#### 10. Future compatibility without v1 scope creep

The first implementation provides only the configuration and identity features needed for straightforward racing.

Its data model and interfaces must not unnecessarily prevent later support for:

- richer MUG profiles;
- richer car profiles;
- long-term MUG/car statistics;
- multi-race competitions;
- automatic lane rotation;
- heats/rounds;
- other event structures.

These are **not v1 requirements** merely because the data model leaves room for them.

The design must not assume that:

- one MUG permanently belongs to one lane;
- one car permanently belongs to one MUG;
- a race must always be completely standalone.

> **Design broadly. Implement narrowly.**


<a id="configuration-and-identity-spec--11-track-configuration"></a>
#### 11. Track configuration

The v1 product assumes one remembered physical Track Configuration rather than requiring a named multi-track database.

Normal use must not require the MUG to create or name a track before racing.

The Track Configuration contains information P&P actually uses, including:

- configured lane count;
- remembered/default device-role proposals used when forming a Race Setup and, on accepted START, the Session Definition;
- Start/Finish arrangement;
- optional track length;
- optional track scale.

Track length enables appropriate speed calculations. Track scale enables scale-equivalent speed calculations.

The underlying design must not unnecessarily prevent multiple saved physical Track Configurations being added later.

<a id="configuration-and-identity-spec--111-rally-stages-are-not-tracks"></a>
##### 11.1 Rally stages are not tracks

A Rally Stage is an event/run definition layered on top of the current physical Track Configuration; it is not another physical track record.

Stages may differ in how the installation is used, for example:

- loop, 2 laps;
- loop, 3 laps;
- A→B;
- B→A.

Stages may optionally be given user-visible names.

<a id="configuration-and-identity-spec--12-configuration-presentation"></a>
#### 12. Configuration presentation

P&P separates **what are we doing now?** from **what is this installation?**

<a id="configuration-and-identity-spec--121-normal-race-screen"></a>
##### 12.1 Normal race screen

The ordinary race screen contains the small number of settings commonly needed for the imminent race.

For Lap Race this may include:

- MUG/lane assignments;
- target laps;
- finish behaviour;
- Swap Lanes;
- START.

It also provides unobtrusive access to **More Race Options** and **Setup**.

See [Integrated Display configuration and restoration](#pp-taster-spec--smug-configurable-taster) for the local setup, factory values and reset boundaries.

The shared configuration accepts positive 32-bit lap targets. Loading a
historical configuration preserves its stored lap target and unrelated
customer settings. The standalone adjustment/START restriction is governed by [Integrated Display READY](#pp-taster-spec--ready); it does not limit Browser race distances. Corrupt or invalid records continue through the existing storage recovery rules.

<a id="configuration-and-identity-spec--122-more-race-options"></a>
##### 12.2 More Race Options

More Race Options contains settings that may genuinely vary from race to race but need not clutter the normal screen.

Examples include:

- start-light count;
- GO style;
- final-delay style;
- false-start response;
- audio event choices and destination.

See [shared audio preferences, defaults and testing](#product-facilities-spec--35-preferences-defaults-and-testing).

An **Advanced** area may contain less-common detailed parameters such as exact random-delay limits or red-light interval.

<a id="configuration-and-identity-spec--123-setup"></a>
##### 12.3 Setup

Setup describes the P&P installation rather than today's race.

Logical areas include:

- **Track** — length, scale, Start/Finish arrangement;
- **Hardware** — detected sensors/modules, remembered/default role proposals and optional hardware;
- **MUGs** — management of saved identities;
- **Cars** — only where optional car recording is enabled;
- **Sound & Display**;
- **System** — networking, update, backup/restore, diagnostics and similar system functions. External Wi-Fi configuration belongs in the **Network** section of **Settings → Advanced → Diagnostics**; it is not a separate authority or setup area.

Normal MUG creation/selection remains available directly from the race screen; a MUG-management area must not become a prerequisite to racing.

<a id="configuration-and-identity-spec--13-save-and-confirmation-behaviour"></a>
#### 13. Save and confirmation behaviour

P&P should not require routine SAVE/APPLY interaction when the user's intention is already unambiguous.

Race settings become the current/remembered **Race Setup** as they are changed. The SMUG edits Race Setup; P&P creates the fixed internal Session Definition only when START is accepted.

Installation settings are stored when changed/confirmed.

Confirmation is reserved for actions that are destructive, disruptive or genuinely ambiguous, such as deleting persistent information, materially reassigning hardware or performing a factory reset.

> **Save automatically where intention is unambiguous. Confirm where an action is destructive, disruptive or ambiguous.**


<a id="configuration-and-identity-spec--14-out-of-box-base-configuration"></a>
#### 14. Out-of-box base configuration

The base product should require **zero software configuration** when installed according to the recommended quick-start arrangement.

Factory/default base role proposals are:

- Sensor 1 → Lane 1 Start/Finish;
- Sensor 2 → Lane 2 Start/Finish;

These are remembered/default installation proposals, not racing meaning owned by the Input Module. When START is accepted, the roles actually required for that race are frozen into the Session Definition. The same physical input may therefore serve a different role in another session without changing the Input Module.
- recommended/default physical sensor arrangement → sensor before the Start/Finish line.

The Quick Setup documentation must show the matching physical arrangement clearly.

With this default physical arrangement, straightforward lap counting works immediately. Capabilities that require a different sensor relationship, including applicable false-start detection and reaction timing, are not available merely by pretending the default installation can measure them.

Those unavailable capabilities may remain visible but greyed/locked with an explanation of the physical/configuration change required to enable them.

A MUG who installs the sensors differently can change the remembered arrangement later under Track Setup. A clear diagram should be used rather than relying only on technical wording.

Optional information such as track length, track scale and MUG names must not become first-boot requirements.

<a id="configuration-and-identity-spec--15-muggle-quick-setup-guide"></a>
#### 15. MUGGLE quick-setup guide

The base product requires a short, highly visual **MUGGLE Guide** for first use.

Its purpose is not to replace the full manual. Its job is to get an ordinary MUG from unopened/unconfigured product to a working basic race with the fewest possible decisions.

The intended flow is approximately:

1. **PLONK** — position the two sensors exactly as illustrated in the recommended/default arrangement.
2. **PLUG** — connect Sensor 1/Lane 1 and Sensor 2/Lane 2 and power P&P.
3. **CONNECT** — connect a phone/tablet/computer to P&P's factory local
   Wi-Fi access point and open the Browser address shown by P&P.
4. **PLAY** — choose the basic race setting such as laps and press START.

<a id="configuration-and-identity-spec--151-factory-local-network"></a>
##### 15.1 Factory local network

P&P provides its own local Wi-Fi access point automatically from first boot.
Normal racing is local and does not require a house/club Wi-Fi network or
Internet access. The factory access point is deliberately open, with no
Wi-Fi password; the Taster or other local P&P presentation shows the network
name and Browser address needed for connection. A QR connection aid may be
added later.

Wi-Fi association is only transport connectivity. It does not assign SMUG,
Master or Race Director authority. The existing P&P control-lease rules decide
which Browser/client may issue control requests.

Optional external house/club Wi-Fi is available through the Network section of
Settings → Advanced → Diagnostics and is never required for racing. One
external network may be remembered for reconnection. Any connected Browser may
view sanitized AP/STA status and request a scan; choosing an SSID remains local
presentation state until submission. When a remembered network is disconnected,
the controlling SMUG may issue **RECONNECT**, which uses the ESP32 driver's
already-stored STA configuration and sends no SSID or password. **RECONFIGURE**
is the separate credential-writing operation for changing the remembered
network. Connect/Reconfigure, Reconnect, Disconnect and Forget Network require
the current controlling SMUG/Race Director lease and are rejected
authoritatively for other clients. Association with either the factory AP or
the external network grants connectivity only and never grants control.

The factory AP remains automatic, open and independent. HTTP and racing remain
available as soon as the AP is ready, regardless of station failure or loss.
The external password may be accepted for provisioning and persisted only as
needed for reconnection, but must never subsequently be returned, displayed,
logged, diagnosed, exported, or included in History/Results or support data.
The ESP-IDF/Arduino Wi-Fi driver's flash-backed STA configuration is the sole
persistent credential store for that remembered network; P&P does not maintain
a duplicate SSID/password record. Disconnect retains the driver configuration
but suppresses automatic reconnection for the remainder of the boot. Explicit
Reconnect clears that suppression without writing credentials. Explicit
Reconfigure clears that suppression and may replace the driver's stored
configuration. Reboot clears suppression and permits automatic reconnection.
Forget Network erases the driver's persisted STA configuration and P&P's configured
state, with authoritative failure if the operation cannot be completed.
Network status is sanitized and may include AP/STA state, addresses, signal,
failure information and mDNS status. `plonk-and-play.local` is preferred when
available, with the assigned LAN IP as fallback. Connection, retry and scan
work must remain non-blocking; exact timing constants are not product-fixed.

A small **Want more?** section may point towards optional MUG names, track length/scale, alternative sensor arrangements, enhanced start/false-start functions, Rally, Drag and other capabilities.

Product requirement:

> **If the MUG follows the MUGGLE Guide's recommended physical installation, the software defaults must match it and no Setup step should be required before basic racing.**

---

<a id="race-setup-lane-assignment-spec--race-setup--lane-assignment-and-rotation-behaviour"></a>
### Race Setup — Lane Assignment and Rotation Behaviour

**Status:** Accepted product/UI behaviour for implementation  
**Scope:** Race Setup presentation and ownership of pending lane assignments for configured multi-lane tracks.

<a id="race-setup-lane-assignment-spec--1-physical-lanes-and-race-entries"></a>
#### 1. Physical lanes and Race Entries

The current Track Configuration determines the physical lane count shown by Race Setup, up to the supported maximum.

Physical lanes remain fixed. What moves between lanes is the complete **Race Entry**: Racer plus optional Car and any other entry-owned identity required by the pending race setup.

A lane-rearrangement operation therefore changes the pending Race Entry-to-Lane assignment; it does not renumber or redefine the physical track lanes.

<a id="race-setup-lane-assignment-spec--2-race-entry-panel-scaling"></a>
#### 2. Race Entry panel scaling

Race Setup must remain pleasant for a normal two-lane track while also accommodating larger configured tracks, including up to eight lanes where supported.

Do not permanently squash two-lane rows merely to reserve space for eight lanes.

The Race Entries panel should use available vertical space intelligently:

- few lanes: generous row height;
- increasing lane count: progressively more compact rows;
- once a sensible minimum row height is reached: keep that row height and make the Race Entries area internally scrollable if the available viewport still cannot show all configured lanes.

The intent is adaptive row height first, scrolling only when useful. Exact pixel thresholds are presentation implementation details, not product semantics.

Only configured physical lanes are shown. A two-lane Track Configuration shows two lane rows, not empty rows for lanes 3–8.

<a id="race-setup-lane-assignment-spec--3-front-screen-lane-controls"></a>
#### 3. Front-screen lane controls

Lane rearrangement is performed directly on the main Race Setup screen. It does **not** require a separate lane-swap pop-up or modal.

Above the track/lane list, Race Setup provides:

- **ROTATE LANES**;
- a nearby **Reverse direction** toggle;
- **Automatically rotate after each race**.

If an icon accompanies ROTATE LANES, a vertical double-arrow treatment is appropriate because the visible entries move up/down through the lane list. Do not use a horizontal double-arrow that suggests left/right swapping.

The track heading (for example **2-LANE TRACK** or **8-LANE TRACK**) and configured lane rows appear below these controls.

<a id="race-setup-lane-assignment-spec--4-manual-drag-and-drop"></a>
#### 4. Manual drag and drop

Race Entries remain directly draggable on the main Race Setup screen.

- the user may drag a Racer/Race Entry from one lane to another;
- dropping onto an occupied lane exchanges/rearranges the complete entries;
- the operation must not create duplicate lane ownership;
- optional Car identity moves with its Race Entry.

Drag and drop provides arbitrary manual rearrangement. It coexists with ROTATE LANES rather than being replaced by it.

A separate **CHANGE** button on every lane row is not required. The Racer/Car area itself is the natural entry point for selecting or changing that Race Entry.

<a id="race-setup-lane-assignment-spec--5-manual-rotation-and-direction"></a>
#### 5. Manual rotation and direction

**ROTATE LANES** quickly moves all current Race Entries by one physical lane.

For N configured lanes, normal rotation is conceptually:

`1 → 2 → 3 → ... → N → 1`

**Reverse direction** reverses that mapping:

`1 → N → ... → 3 → 2 → 1`

For two lanes, either direction necessarily exchanges the two entries.

The direction is one P&P setting used consistently by both manual ROTATE LANES and automatic post-race rotation. Do not maintain contradictory browser-local direction settings.

Terminology matters:

- **drag/rearrange** describes arbitrary manual reassignment;
- **rotate/rotation** describes systematic movement of every Race Entry by one lane.

Do not call the multi-lane rotation behaviour “swap lanes”.

<a id="race-setup-lane-assignment-spec--6-automatic-rotation"></a>
#### 6. Automatic rotation

**Automatically rotate after each race** is race behaviour, not part of an individual lane row.

When enabled, after a properly completed race P&P rotates every Race Entry one physical lane when preparing the next pending race setup, using the same direction setting as manual ROTATE LANES.

An abandoned/restarted race does not trigger automatic rotation.

The completed race's Results continue to show the lanes actually used in that race. Rotation affects the pending setup for the next race; it does not rewrite the completed result.

The browser must not independently perform an automatic rotation merely because it is open.

<a id="race-setup-lane-assignment-spec--7-numeric-race-controls"></a>
#### 7. Numeric race controls

Numeric Race Setup controls such as target laps should support efficient small and large changes without requiring repeated individual clicks.

For target laps:

- valid range is **1–999**;
- direct text entry is supported;
- keyboard Up/Down may increment/decrement;
- mouse wheel over the numeric control increments/decrements;
- a single press of `+` or `−` changes the value by one;
- holding `+` or `−` repeats the change;
- continued holding progressively accelerates the rate of change.

Acceleration should feel progressive rather than immediately jumping to a large step. Exact timing/rate curves are presentation implementation details and should be tuned by use.

This is a reusable P&P numeric-control behaviour. Other suitable numeric settings such as durations or run counts should use the same interaction pattern where appropriate.

<a id="race-setup-lane-assignment-spec--8-ownership-and-authority"></a>
#### 8. Ownership and authority

The browser is the interface for choosing these options; it is not their authority.

Lane assignments, rotation direction and automatic-rotation state belong to P&P's authoritative **pending Race Setup / pending session proposal**.

Therefore:

- a browser requests a lane-assignment, rotation or setting change;
- P&P validates and owns the resulting pending configuration;
- the browser reads back and presents that authoritative pending state;
- multiple connected browsers must converge on the same pending assignments/settings rather than maintaining private local versions;
- reload/reconnect must reconstruct the same pending state from P&P;
- START validates and commits that single authoritative proposal into the immutable Session Definition.

Once START is accepted, the active Session Definition's lane assignments are frozen for that session. A browser must not casually mutate the active race by editing the next/pending setup.

This requirement is intentionally consistent with the pending-session ownership cleanup: lane-management UI must not recreate Browser-local proposal state.

<a id="race-setup-lane-assignment-spec--9-availability-and-disabling"></a>
#### 9. Availability and disabling

Lane rearrangement and rotation controls are available only when P&P says the pending setup can be edited.

If the relevant configuration is frozen or changing it is not currently permitted, the browser disables the controls rather than making a local change that Race Control will later reject.

<a id="race-setup-lane-assignment-spec--10-presentation-principle"></a>
#### 10. Presentation principle

The ordinary Race Setup remains simple and direct:

- ROTATE LANES, Reverse direction and automatic rotation are visible above the configured track/lane list;
- configured lane rows show Racer and optional Car;
- entries can be dragged directly between fixed physical lanes;
- race-defining settings such as laps and finish behaviour remain alongside the entries;
- More Race Options is reserved for other secondary race behaviour;
- the permanent READY / START contract remains intact.

> **The lane stays put; the Race Entry moves. The browser requests; P&P owns.**

<!-- INSERT:configuration -->

## Diagnostics and recovery

<!-- INSERT:recovery -->

## Physical product

<a id="product-facilities-spec--plonk--play--product-facilities-specification"></a>
### Plonk & Play™ — Product Facilities Specification

**Status:** Committed product-design specification  
**Scope:** Cross-mode facilities and product decisions agreed during Lap Counter product design.

<a id="product-facilities-spec--1-base-product-hardware-direction"></a>
#### 1. Base product hardware direction

The base product has two standard optical timing sensors.

The same sensors are repurposed according to mode rather than being permanently tied to one race form.

The System Controller is authoritative for timing, race logic and state. Browser clients are presentation/control clients and must not become timing authorities.

<a id="product-facilities-spec--2-power-module"></a>
#### 2. Power Module

Track-power control is an optional hardware capability.

Without a Power Module, P&P can still detect, report and score behaviours such as false starts and penalties, but it cannot physically cut, hold or otherwise control lane power.

Where a Power Module is fitted, modes may offer appropriate power-control responses.

Hardware-dependent options should normally remain visible but locked/greyed with a concise explanation such as **Power Module required**, rather than disappearing and concealing the system's expandable capability.

Reduced-power Yellow operation is only a valid product option if the eventual Power Module hardware genuinely supports controlled reduced power.

<a id="product-facilities-spec--3-audio-and-removable-storage"></a>
#### 3. Audio and removable storage

<a id="product-facilities-spec--31-base-requirement"></a>
##### 3.1 Base requirement

Every normal P&P System Controller should include:

- persistent microSD/removable storage capability;
- onboard audio capability;
- a small internal speaker sufficient for clear race-control tones and speech.

This is a standard base capability, not an upgrade.

The exact hardware implementation is not yet fixed.

<a id="product-facilities-spec--32-storage-uses"></a>
##### 3.2 Storage uses

Persistent removable storage may retain:

- race/session results;
- lap-by-lap data;
- Timed Stage results;
- personal bests and records;
- track records;
- MUG profiles;
- event history;
- CSV exports/backups;
- configuration backup;
- audio/voice files.

The detailed record schema remains implementation design.

<a id="product-facilities-spec--33-audio-implementation"></a>
##### 3.3 Audio implementation

Essential tones reside in firmware. Prerecorded speech and effects are stored on SD.
P&P supplies the audio packs. Customer-installed packs are outside the product scope.
Launch packs provide British male and female voices. The audio pack design must
support future languages.

The implementation should favour a small, reliable, low-BOM solution with sane file/folder handling.

Options to compare before hardware freeze include:

1. off-the-shelf audio + SD module;
2. separate microSD plus audio module/amplifier;
3. audio/SD circuitry integrated into P&P's own PCB.

Previous awkward module folder/naming conventions must not dictate the new architecture.

High audio power is not a goal in itself. Clear internal race-control sound matters more than 5 W versus 10 W; a few watts may be sufficient.

<a id="product-facilities-spec--34-audio-destinations"></a>
##### 3.4 Audio destinations

Audio event/type and audio destination are separate choices.

Selectable destinations:

- P&P unit;
- Browser;
- Both;
- Mute.

In standalone operation, Browser-only falls back to P&P's internal speaker.
Mute is always respected, including during destination fallback. Browser audio
can deliver individual MUG alerts. Personalised spoken announcements using
MUG names are deferred until after v1.

<a id="product-facilities-spec--35-preferences-defaults-and-testing"></a>
##### 3.5 Preferences, defaults and testing

P&P uses one shared set of sound preferences, including standalone operation. The controlling Browser's System sound settings allow individual event configuration, including countdown beeps or spoken countdown, race announcements and spoken lap times. Preferences survive restart; system factory reset restores the defaults, but restoring the Integrated Display defaults does not reset sound.

| Factory setting | Default |
|---|---|
| Enabled events | Welcome sound, countdown beeps, lap-completion sound, final-lap announcement, time warnings, pause/resume, winner announcement, victory fanfare |
| Disabled events | Race setup announcement, spoken lap times, fastest-lap announcement, halfway announcement |
| Destination and volume | P&P internal speaker, 60% |

Endurance warnings are individually configurable.

| Endurance event | Factory default |
|---|---|
| Five minutes remaining | Off |
| One minute remaining | On |
| Thirty seconds remaining | Off |
| Ten seconds remaining | On |
| Time expired | On |
| Finish current lap | On |

Provide audio preview and independent tests for the P&P speaker and Browser
audio. Disable previews and both tests during active races.

<a id="product-facilities-spec--36-playback-priority-and-reliability"></a>
##### 3.6 Playback priority and reliability

Playback priority, highest first:

1. Countdown and GO.
2. Race interruption and completion.
3. Winner announcement.
4. Final lap and time remaining.
5. Fastest lap.
6. Ordinary lap sounds and spoken lap times.

Higher priorities may interrupt lower priorities. Discard obsolete
announcements, including stale cues on PAUSE and race restart. Play the pause
announcement once per pause. Alternating yellow warning lights remain silent.

Audio must never delay racing and must work without an internet dependency.
If a file is missing or corrupt, use an appropriate basic tone. Report audio
failures through diagnostics.

<a id="product-facilities-spec--4-browser-and-system-controller-responsibilities"></a>
#### 4. Browser and System Controller responsibilities

The System Controller owns:

- authoritative timing;
- race/session logic;
- authoritative state;
- small race facts/events and state updates.

Browsers own:

- graphics;
- animation;
- formatting;
- browser audio;
- presentation appropriate to their role.

Official timing must never depend on browser latency.

The architecture should support several simultaneous browser clients. A representative four-MUG setup may involve six browsers:

- one main scoreboard;
- four personal MUG displays;
- one Race Director display.

This scale should be prototype-tested under realistic worst-case conditions before hardware is frozen.

<a id="product-facilities-spec--41-local-taster-and-smug-control-hierarchy"></a>
##### 4.1 Local Taster and SMUG control hierarchy

See [Integrated Display control hierarchy and records](#pp-taster-spec--control-hierarchy-and-records) and [factory car graphics](#pp-taster-spec--factory-car-graphics). The Integrated Display is not an independent racing product.

<a id="product-facilities-spec--5-track-setup"></a>
#### 5. Track Setup

Track Setup may optionally store information that normally changes rarely:

<a id="product-facilities-spec--51-track-length"></a>
##### 5.1 Track length

- metres or feet;
- Unknown / Not entered allowed.

<a id="product-facilities-spec--52-track-scale"></a>
##### 5.2 Track scale

Preset examples include:

- 1:32;
- 1:24;
- 1:43;
- 1:64;
- Custom;
- Unknown / Not entered.

From track length and timing P&P can calculate actual model average speed.

Where scale is known it can also calculate scale-equivalent speed.

Speed may be calculated per lap, for fastest lap, stage or whole race where meaningful.

Speed does not need to occupy the default live scoreboard. It may appear in results, analysis or event callouts.

<a id="product-facilities-spec--6-start-presentation-and-audio"></a>
#### 6. Start presentation and audio

Start behaviour is a race-mode concern, but presentation facilities should support parameterised sequences rather than hard-coded animations.

For normal Lap racing this includes three- or five-light sequences, configurable red-light interval and configurable final delay.

The same authoritative GO time must govern race timing, display and any scheduled physical/audio outputs. Presentation may be theatrical; timing remains boring and reliable.

<a id="product-facilities-spec--7-solo-and-multi-user-product-behaviour"></a>
#### 7. Solo and multi-user product behaviour

P&P must remain usable without a dedicated Race Director.

Supported operating patterns include:

- dedicated Race Director;
- Race Director who is also racing;
- solo MUG.

Every normal race/session must be runnable by one MUG.

At the other end, P&P may scale to a main display, several personal MUG displays and a separate Race Director display without changing the fundamental race engine.

<a id="product-facilities-spec--8-product-option-philosophy"></a>
#### 8. Product-option philosophy

P&P should be **simple by default, sophisticated when wanted**.

Software capability should not be artificially withheld merely to manufacture product tiers when exposing it costs essentially nothing.

Advanced or uncommon choices may be placed under **Advanced** so the ordinary MUG is not confronted with unnecessary complexity.

Hardware-dependent premium/expansion capabilities can remain visible but locked with a clear requirement explanation.

<a id="product-facilities-spec--9-demonstration-and-attract-facilities"></a>
#### 9. Demonstration and Attract facilities

P&P should provide demonstration facilities as a product capability, distinct from normal race operation. These facilities are intended for showroom/exhibition presentation, user familiarisation, development testing and fault diagnosis.

The facilities are deliberately separated into **Attract Mode**, **Automatic Demo Race** and **Manual Demo/Test**.

<a id="product-facilities-spec--91-attract-mode"></a>
##### 9.1 Attract Mode

Attract Mode is a presentation facility analogous to the demonstration displays used on retail hi-fi and similar equipment.

It is not intended to be generally available as an ordinary race mode. A SMUG-controlled configuration option enables or disables it.

When enabled, an idle P&P unit may automatically run a short repeating presentation sequence. The sequence may demonstrate the product using lightweight graphics and animation, for example:

- the P&P logo/identity;
- a simple track drawing itself;
- small animated cars;
- start lights;
- lap-count and timing displays;
- fastest-lap callouts;
- a close finish and chequered flag;
- results;
- short visual demonstrations of supported race types and expansion capability.

Attract Mode is presentation, not authoritative race simulation. Its race sequence may therefore be deliberately scripted and choreographed for clarity and visual interest.

The display implementation should favour simple, lightweight graphical primitives, text, icons and small bitmap/sprite assets suitable for an ESP32-class controller. Presentation work must remain subordinate to authoritative race processing and must never compromise race timing or control.

The exact display hardware, resolution, artwork, animation style, sequence length and wake/exit behaviour remain deferred design decisions.

Attract Mode may ultimately be available on the local Taster display and/or connected Browser displays. That presentation scope is deliberately not fixed yet.

<a id="product-facilities-spec--v1-attract-presentation-guidance"></a>
###### V1 Attract presentation guidance

The initial implementation should use the following approximately 35-second loop as a practical starting storyboard:

- **0–3 s:** P&P / PLONK & PLAY identity;
- **3–6 s:** a simple track draws onto the screen, followed by two lane lines and small car sprites;
- **6–9 s:** start-light sequence followed by GO;
- **9–18 s:** a short choreographed race with changing lap/timing information, cars changing relative position and a FASTEST LAP moment;
- **18–22 s:** close finish, chequered flag and winner/gap presentation;
- **22–30 s:** brief feature flashes illustrating **RACE → RALLY → DRAG** and **2 → 4 → 6 → 8 LANES**;
- **30–35 s:** return to P&P / PLONK & PLAY identity, pause briefly, then repeat.

This storyboard is **presentation guidance for V1, not a frozen behavioural contract**. Timing, transitions, wording, sprites, artwork, sound effects and individual beats may be tuned once the sequence is seen on real display hardware. Such presentation tuning does not require an architectural/product-specification change provided the purpose and boundaries of Attract Mode remain unchanged.

A final P&P logo is not a dependency for V1. Until a satisfactory logo is designed, the presentation may use a clean **P&P / PLONK & PLAY** text identity.

<a id="product-facilities-spec--92-automatic-demo-race"></a>
##### 9.2 Automatic Demo Race

Automatic Demo Race is different from Attract Mode: it is a genuine P&P race driven by simulated detector activity.

Virtual detector events must enter at the defined sensor/input boundary so that the normal downstream product path is exercised. Race Control, Race Engine, Message Bus, Noticeboard, displays, audio and results processing should operate as they would for physical detector events.

The simulator may create plausible virtual competitors with slightly varying lap times. Demonstrations may be tuned to produce interesting, reasonably close racing, but the result should be determined by the real race logic rather than by a separately scripted fake result.

Automatic Demo Race may be initiated through SMUG-controlled demonstration facilities. It is not an ordinary Taster race option that a MUG can accidentally select.

<a id="product-facilities-spec--93-manual-demotest"></a>
##### 9.3 Manual Demo/Test

A manual demonstration/test facility may expose virtual versions of the installed detector inputs through SMUG.

For example, a two-lane Lap Race installation may provide virtual controls equivalent to:

- Lane 1 Start/Finish detector;
- Lane 2 Start/Finish detector.

Additional configured detectors may be represented where appropriate.

Activating a virtual detector should inject the corresponding simulated event at the same defined simulation boundary used by Automatic Demo Race, exercising the real downstream system.

This provides a useful diagnostic boundary. If a virtual detector produces the expected race behaviour while the corresponding physical detector does not, investigation can concentrate on the physical sensor/input side rather than the downstream race system.

<a id="product-facilities-spec--94-demo-session-integrity"></a>
##### 9.4 Demo-session integrity

Simulated race sessions must be authoritatively identifiable as **DEMO**, not merely labelled by presentation code.

Demo results must not contaminate genuine sporting records, including personal bests, track records, championship results or equivalent persistent competitive data.

Displays should identify simulated operation clearly enough that a demo cannot reasonably be mistaken for a live race.

The Demo exclusion above is independent of the WP8 standalone-Taster policy:
both are non-official, but a standalone Taster race is a real local race rather
than a simulated/demo session and must remain distinguishable in the frozen
session source.

The eventual persistence design may retain demo sessions for testing or support purposes, but they must remain distinguishable from genuine results.

Physical and simulated detector events must not accidentally mingle within an authoritative race. Exact behaviour when real detector activity occurs during an Automatic Demo Race remains a later design decision; possible policies include cleanly terminating the demo or ignoring physical detector activity for that demo session.

<a id="product-facilities-spec--95-architectural-principle"></a>
##### 9.5 Architectural principle

Demo capability must not create a second fake Race Engine.

Where the purpose is to demonstrate or test real P&P behaviour, simulation occurs at the sensor/input boundary and everything downstream remains normal product behaviour.

Attract Mode is the deliberate exception because it is presentation rather than race simulation.

<a id="product-facilities-spec--10-deferred-decisions"></a>
#### 10. Deferred decisions

The following remain deliberately open:

- exact ESP32/System Controller hardware;
- exact audio/SD implementation;
- exact speaker/amplifier power;
- exact Power Module electrical design;
- exact dual-beam Drag hardware;
- detailed RMS state machine;
- detailed visual styling and animations;
- detailed persistence schema;
- exact Attract Mode display hardware, graphics, sequence and wake/exit behaviour;
- whether Attract Mode presentation is local-display only or also available on Browser displays;
- exact Automatic Demo Race virtual-car model and timing variation;
- exact behaviour when physical detector activity occurs during an Automatic Demo Race;
- detailed SMUG controls for demonstration/test facilities.

These are not forgotten requirements; they are deferred implementation/product decisions.

<!-- INSERT:physical -->
