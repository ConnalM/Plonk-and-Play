# Plonk & Play™ — Presentation Design Specification

**Status:** Accepted visual-direction specification  
**Applies to:** Browser presentation and integrated P&P display  
**Authority:** Product behaviour remains governed by the relevant product specifications. This document governs visual language and presentation consistency, not race logic.

## 1. Purpose

P&P must look like one product everywhere.

The integrated display, browser Race Control, Personal MUG display, spectator display, Results and configuration screens must share a recognisable visual language. Different devices may arrange and prioritise information differently, but they must not look like unrelated applications.

The presentation should feel like a professionally designed motorsport timing product rather than a generic web dashboard, hobby project or computer game.

Core principle:

> **One P&P look. Different screens. The information that matters now dominates.**

## 2. Agreed visual direction

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

## 3. Relationship to the physical product

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

## 4. Information hierarchy

P&P must show **what matters now** rather than everything it knows.

During a race the primary hierarchy is generally:

1. race-control state and safety/control signals;
2. position and race progress;
3. immediately useful timing information;
4. secondary analysis/detail.

Secondary information must not compete visually with the current race state.

Detailed analysis belongs in Results/Details rather than being forced onto the live race display.

## 5. Shared presentation components

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

## 6. Colour rules

### 6.1 Brand colour

P&P yellow is the principal accent and selection colour. It should be distinctive but restrained; covering large portions of every screen in yellow would weaken it.

### 6.2 Lane colours

Lane colours communicate lane identity and related race information. They should not become a general page theme.

A lane colour may be used for items such as:

- lane number/identity marker;
- a narrow row accent;
- a relevant event highlight;
- Personal MUG identity.

Large saturated lane-colour backgrounds should be avoided where they reduce readability or make multi-lane displays visually noisy.

### 6.3 Status colours

Status colour must have consistent meaning across screens. Yellow/Red race-control presentation remains governed by `COMMON_DISPLAY_SPEC.md`.

Colour must not be the only means of communicating a critical state; text, iconography or light state should also make the meaning clear.

## 7. Typography and numeric presentation

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

Existing rule from `COMMON_DISPLAY_SPEC.md` remains mandatory:

> **No UI jitter caused by changing race data.**

Decorative motorsport fonts may be used sparingly for branding if appropriate, but not for dense timing information.

## 8. Live race display

The existing Common Display Specification remains authoritative for live race content and behaviour.

Visually:

- the Race Control Pod is clearly identifiable and remains in a stable location;
- lane rows remain geometrically stable during live racing;
- position changes within a lane row rather than moving the row;
- position and progress are visually dominant;
- Last, Best and Gap are clearly secondary;
- temporary events alter emphasis without rebuilding the whole screen;
- animation must not interfere with reading current race information.

Race theatre is appropriate for meaningful moments such as:

- start sequence;
- position change;
- fastest lap;
- final lap;
- finish;
- Yellow/Red race control.

Continuous animation for its own sake is not part of the P&P visual language.

## 9. Personal MUG display

The Personal MUG display is a glanceable racing instrument, not a miniature copy of the full scoreboard.

Position and race progress dominate.

Last Lap, Best Lap and Gap remain secondary. Personal alerts may temporarily dominate as already specified in `COMMON_DISPLAY_SPEC.md`.

A phone held or positioned several feet away should remain useful without requiring the MUG to read small labels while driving.

Portrait and landscape presentations may arrange the same information differently while retaining the same hierarchy and visual identity.

## 10. Results

Results switch from live fixed-lane presentation to finishing order as already specified.

The visual hierarchy should answer immediately:

> **Who won, and what happened?**

The winner should be obvious without excessive celebration or obscuring the other results.

Detailed lap, sector, speed and penalty analysis remains secondary and may live behind a Details / Analysis view.

Controls shown in visual prototypes are not automatically product requirements. In particular, History is automatically recorded according to the governing history specification; a mock-up button such as `SAVE RESULTS` must not create a new manual-save requirement.

## 11. Home and mode selection

Home should present the supported race modes clearly using the common Mode Card component.

Lap Race remains the normal/dominant starting choice in accordance with the browser-flow specification. Other supported modes remain visible without making the Home screen feel like a configuration menu.

Mode cards may use simple icons and one short explanatory phrase. They should not contain paragraphs of help.

## 12. Configuration screens

Configuration uses the same P&P visual language but may be denser than racing screens.

Configuration should favour conventional, understandable controls over decorative novelty.

Advanced settings must not visually overwhelm normal setup. Contextual help and numeric controls follow the rules in `COMMON_DISPLAY_SPEC.md`.

The fact that a configuration page can fit more information on a laptop does not justify displaying every available option simultaneously.

## 13. Responsive presentation

P&P is one presentation system, not separate unrelated applications for each screen size.

Responsive layouts should adapt the shared components and hierarchy approximately as follows:

- **Phone portrait:** Personal MUG / focused single-user presentation; very large glanceable values.
- **Phone landscape:** compact Personal MUG or compact race presentation.
- **Tablet:** full Race Control and normal setup/results.
- **Laptop/desktop:** full Race Control, configuration and detailed analysis.
- **TV/projector:** spectator-first scoreboard with controls normally absent.
- **Integrated P&P display:** local self-contained presentation optimised for its physical screen and controls.

These are presentation priorities, not hard device restrictions.

## 14. Multiple simultaneous displays

Different connected displays may legitimately show different views of the same authoritative race.

For example, at the same moment P&P may show:

- full scoreboard on a TV;
- Race Control on a tablet;
- Personal MUG views on phones;
- local race state on the integrated display.

All should unmistakably belong to the same P&P system.

The browser remains a presentation client. Visual effects, layout and animations must never become timing authority or alter authoritative race state.

## 15. Visual prototypes

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

## 16. Accessibility and practical use

P&P screens are used while people are watching and driving slot cars, often at a distance and in imperfect lighting.

Therefore:

- important text and numbers require strong contrast;
- critical information must not rely on colour alone;
- touch targets must be comfortably usable;
- important race values should not require close reading;
- decorative elements must never obscure timing information;
- motion should be brief and purposeful;
- layouts must tolerate longer MUG names and realistic numeric values without collapsing.

## 17. Implementation boundary

This specification deliberately freezes the **visual direction and consistency rules**, not a pixel-perfect final UI.

Exact spacing tokens, colour values, fonts, icon set, animation timings and component dimensions should be established through implementation/prototyping and then recorded once proven.

Do not delay race-engine or product-behaviour work merely to perfect cosmetic detail.

Equally, do not allow temporary development UI to become the product appearance by accident.

---

**Presentation principle:** Professional enough to look like a product; simple enough to remain Plonk & Play™.
