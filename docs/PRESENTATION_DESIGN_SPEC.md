# Plonk & Play™ — Presentation Design Specification

**Status:** Accepted visual-direction specification — Stage 15 UX reconciliation  
**Applies to:** Browser presentation and integrated P&P display  
**Authority:** Product behaviour remains governed by the relevant product specifications. This document governs visual language and presentation consistency, not race logic.

## 1. Purpose

P&P must look like one product everywhere.

The integrated display, normal browser race display, personal Racer display, spectator display, Results and configuration screens share a recognisable visual language. Different devices and purposes may arrange and prioritise information differently without becoming unrelated applications.

The presentation should feel like a professionally designed motorsport timing product rather than a generic web dashboard, hobby project or computer game.

> **One P&P look. Different screens. The information that matters now dominates.**

## 2. Agreed visual direction

The accepted visual direction is:

- dark navy/charcoal primary surfaces;
- strong but restrained P&P yellow as the principal brand/accent colour;
- clear white/off-white primary information;
- lane colours only where they communicate lane/Racer identity;
- large, highly legible timing and race-state typography;
- clean, purposeful panels with restrained rounding;
- strong hierarchy and stable geometry;
- simple, consistent iconography;
- restrained motorsport cues rather than racing clichés;
- presentation theatre for genuine race events, not continuously.

The browser should visually relate to the black/dark physical console with its restrained yellow edge/detail rather than looking like generic administration software.

The working identity uses the compact P&P mark and the strapline **SIMPLE SETUP · SERIOUS SLOT RACING** where space and context make the strapline useful.

Avoid carbon-fibre wallpaper, fake brushed metal, permanent chequered decoration, seven-segment typography everywhere, glowing controls, gratuitous cars/animation and dense telemetry-dashboard styling.

## 3. Relationship to the physical product

Browser and integrated display should share, where practical:

- P&P identity treatment;
- colour palette;
- typography hierarchy;
- panel geometry;
- timing-number treatment;
- status colours and meanings;
- Race Control Pod language;
- icons;
- event treatments such as Fastest Lap, Final Lap and Finish.

They do not require identical layouts. Integrated display, phone, tablet, laptop and TV have different jobs and available space.

## 4. Information hierarchy

P&P shows what matters now rather than everything it knows.

During a race the hierarchy is generally:

1. race-control state and safety/control signals;
2. position and race progress;
3. immediately useful timing information;
4. secondary analysis/detail.

Detailed analysis belongs in Results/Details rather than being forced onto the live display.

## 5. Shared presentation components

Implementations should use a common conceptual component vocabulary and common authoritative data bindings wherever applicable.

Useful components include:

- P&P Header / Identity Mark;
- Race Control Pod;
- Lane Row / Lane Card;
- Position Indicator;
- Race Progress;
- Timing Value;
- Gap Value;
- race clock;
- Fastest Lap treatment;
- Final Lap treatment;
- Penalty treatment;
- Finished treatment;
- Result Row;
- Primary / Secondary Action;
- Information / Help control;
- Numeric adjustment control.

A component retains the same meaning and basic visual identity wherever it appears while being allowed to scale, simplify or rearrange for its purpose/display.

This component vocabulary is the architectural foundation for possible future advanced-user configurable screens. A future editor may rearrange, show/hide or resize supported P&P components; it must not require arbitrary HTML/CSS.

Stage 15 does **not** require a generic Screen Designer, generic configurable layout engine or user-selectable **Classic/Race Control** presets. It does require an implementation that does not unnecessarily hard-wire all live presentation into one inseparable monolith.

## 6. Colour rules

P&P yellow is the principal accent and selection colour. Solid yellow should be reserved for actions/emphasis where it matters, particularly primary actions, rather than covering every selected navigation item.

Lane colours communicate lane identity and related race information. They should not become a general page theme.

Status colour must have consistent meaning. Yellow/Red race-control presentation remains governed by `COMMON_DISPLAY_SPEC.md`. Colour must not be the sole means of communicating a critical state.

## 7. Typography and numeric presentation

Race information must remain readable at a glance and at expected viewing distance.

Use a compact modern sans-serif suitable for screen display. Changing race numbers use tabular/fixed-width numeral behaviour where appropriate.

> **No UI jitter caused by changing race data.**

Decorative motorsport fonts may be used sparingly for branding, not dense timing information.

## 8. Normal live race display

The Common Display Specification governs live content and behaviour.

Visually:

- active racing takes over the useful screen area and ordinary browser chrome is stripped back;
- the Race Control Pod is shallow and stable;
- lane rows remain fixed in physical lane order;
- position changes within a row rather than moving the row;
- Position and Progress dominate;
- Last, Best and Gap are secondary;
- temporary events change emphasis without rebuilding the screen;
- animation never interferes with reading race information.

For the standard 8-lane live browser display at its intended desktop/tablet viewport, **all eight lane rows must be simultaneously visible at a useful, easily readable size without scrolling**. This is a v1 presentation target, not an architectural declaration that P&P can never support more than eight lanes.

Race theatre is appropriate for start sequence, meaningful position changes, fastest lap, final lap, finish and race-control interventions. Continuous animation for its own sake is not.

## 9. Purpose-specific presentations

P&P needs several presentations of the same authoritative state rather than one layout squeezed everywhere.

### Integrated Taster

The local display follows `PP_TASTER_SPEC.md`: a deliberately simple, self-contained two-lane presentation operated by the local control.

### Personal Racer display

A personal phone display is a glanceable racing instrument, not a miniature full scoreboard. Position and progress dominate; Last, Best and Gap are secondary. Alerts may temporarily dominate. Portrait and landscape may arrange the same information differently.

### TV / spectator display

A TV/projector presentation prioritises clear shared race information and normally omits operational controls.

### Race Control presentation

A dedicated Race Control browser may legitimately expose denser timing and permitted operational controls. This is a purpose-specific presentation, not a user-selectable visual preset called “Race Control”.

All presentations remain clients of the same authoritative race state.

## 10. Results

Results switch from fixed lane order to finishing order. The hierarchy should answer immediately: **Who won, and what happened?**

Detailed lap, sector, speed and penalty analysis is secondary and may live behind Details / Analysis.

History is automatically recorded; visual prototypes containing `SAVE RESULTS` do not create a manual-save requirement.

## 11. Home

Home is a calm dashboard for the currently prepared race, not a wall of mode cards.

It should make the prepared race obvious, provide a large hero **START** when ready, offer **Change Race**, and show the latest result compactly where useful.

The permanent header START remains visible on Home as part of the stable header contract. The hero START and header START invoke the same action.

Race modes are selected/configured through Race Setup rather than being required as five Home cards.

## 12. Header and configuration screens

Normal browser navigation is:

**P&P logo | Race Setup | Results | Track Setup | Racers | Settings**

The logo returns Home. The right-hand header status/action area remains fixed in position and shows READY→START or NOT READY/FIX as appropriate.

Selected navigation should use restrained yellow emphasis such as text/underline; solid yellow is reserved for primary action.

Configuration may be denser than racing screens but should favour conventional understandable controls over decorative novelty.

The hierarchy is:

- **Settings** — normal owner preferences;
- **Advanced Settings** — knowledgeable-owner controls and uncommon product configuration;
- **Developer / Diagnostics** — engineering/support internals.

Advanced and Developer are not promoted to normal top-level navigation.

## 13. Responsive presentation

P&P is one presentation system, not unrelated applications by screen size.

Priorities are approximately:

- phone portrait: personal/focused Racer presentation;
- phone landscape: compact personal or race presentation;
- tablet: normal race/control/setup/results;
- laptop/desktop: full race, configuration and detail;
- TV/projector: spectator-first display;
- integrated display: local self-contained Taster presentation.

These are presentation priorities, not hard device restrictions.

## 14. Multiple simultaneous displays

Different displays may show different purpose-appropriate views of the same race at the same moment. Browser clients render state; they never become timing authorities. Loss of a browser must not affect race timing/state.

## 15. Visual prototypes

Visual mock-ups are design references, not behavioural specifications. Invented example names, cars, numbers or controls must not silently override product specifications.

The accepted direction demonstrates dark P&P styling, yellow accents, stable multi-lane timing rows, strong personal-display hierarchy and coherent race/results/spectator presentation. It does not freeze exact pixels, fonts, example data, decorative car graphics or every control shown in concept artwork.

## 16. Accessibility and practical use

Important text and numbers require strong contrast; critical information must not rely on colour alone; touch targets must be comfortable; race values must work at viewing distance; motion should be brief/purposeful; layouts must tolerate realistic names and numeric values without collapsing.

## 17. Implementation boundary

This specification freezes visual direction and consistency rules, not a pixel-perfect final UI.

Exact spacing tokens, colour values, fonts, icons and animation timings may be established through implementation/prototyping and recorded once proven.

Do not allow temporary development UI to become the product appearance by accident.

---

**Presentation principle:** Professional enough to look like a product; simple enough to remain Plonk & Play™.
