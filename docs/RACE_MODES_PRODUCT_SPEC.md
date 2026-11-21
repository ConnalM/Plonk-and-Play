# Plonk & Play™ — Race Modes Product Specification — retired

**RETIRED as a separate governing specification on 19 November 2026.** The authoritative content has moved into the single [P&P Design Specification](PP_DESIGN_SPEC.md#race-modes-product-spec--plonk--play--race-modes-product-specification). Do not edit requirements here.

[Original preserved revision](https://github.com/ConnalM/Plonk-and-Play/blob/35aedb5470f9a06193aefd2073e5a911acae53da/docs/RACE_MODES_PRODUCT_SPEC.md) (historical only).

## Legacy section links

<a id="1-mode-hierarchy"></a>
- [1. Mode hierarchy](PP_DESIGN_SPEC.md#race-modes-product-spec--1-mode-hierarchy)
<a id="11-restricted-taster-subset"></a>
- [1.1 Restricted Taster subset](PP_DESIGN_SPEC.md#race-modes-product-spec--11-restricted-taster-subset)
<a id="2-lap-race"></a>
- [2. Lap Race](PP_DESIGN_SPEC.md#race-modes-product-spec--2-lap-race)
<a id="21-race-definition"></a>
- [2.1 Race definition](PP_DESIGN_SPEC.md#race-modes-product-spec--21-race-definition)
<a id="22-live-information"></a>
- [2.2 Live information](PP_DESIGN_SPEC.md#race-modes-product-spec--22-live-information)
<a id="23-starts"></a>
- [2.3 Starts](PP_DESIGN_SPEC.md#race-modes-product-spec--23-starts)
<a id="24-false-starts"></a>
- [2.4 False starts](PP_DESIGN_SPEC.md#race-modes-product-spec--24-false-starts)
<a id="3-practice"></a>
- [3. Practice](PP_DESIGN_SPEC.md#race-modes-product-spec--3-practice)
<a id="4-endurance"></a>
- [4. Endurance](PP_DESIGN_SPEC.md#race-modes-product-spec--4-endurance)
<a id="41-race-definition"></a>
- [4.1 Race definition](PP_DESIGN_SPEC.md#race-modes-product-spec--41-race-definition)
<a id="42-finish-behaviours"></a>
- [4.2 Finish behaviours](PP_DESIGN_SPEC.md#race-modes-product-spec--42-finish-behaviours)
<a id="43-live-and-result-information"></a>
- [4.3 Live and result information](PP_DESIGN_SPEC.md#race-modes-product-spec--43-live-and-result-information)
<a id="44-endurance-penalties"></a>
- [4.4 Endurance penalties](PP_DESIGN_SPEC.md#race-modes-product-spec--44-endurance-penalties)
<a id="45-countdown-warnings"></a>
- [4.5 Countdown warnings](PP_DESIGN_SPEC.md#race-modes-product-spec--45-countdown-warnings)
<a id="46-cross-client-timing-and-presentation-contract"></a>
- [4.6 Cross-client timing and presentation contract](PP_DESIGN_SPEC.md#race-modes-product-spec--46-cross-client-timing-and-presentation-contract)
<a id="5-timed-stage"></a>
- [5. Timed Stage](PP_DESIGN_SPEC.md#race-modes-product-spec--5-timed-stage)
<a id="51-normal-loop-stage"></a>
- [5.1 Normal loop stage](PP_DESIGN_SPEC.md#race-modes-product-spec--51-normal-loop-stage)
<a id="52-single-and-multi-stage-timed-stage"></a>
- [5.2 Single and multi-stage Timed Stage](PP_DESIGN_SPEC.md#race-modes-product-spec--52-single-and-multi-stage-timed-stage)
<a id="53-point-to-point-and-hill-climb"></a>
- [5.3 Point-to-point and Hill Climb](PP_DESIGN_SPEC.md#race-modes-product-spec--53-point-to-point-and-hill-climb)
<a id="54-dnf-handling"></a>
- [5.4 DNF handling](PP_DESIGN_SPEC.md#race-modes-product-spec--54-dnf-handling)
<a id="55-event-organisation"></a>
- [5.5 Event organisation](PP_DESIGN_SPEC.md#race-modes-product-spec--55-event-organisation)
<a id="56-timed-stage-results"></a>
- [5.6 Timed Stage results](PP_DESIGN_SPEC.md#race-modes-product-spec--56-timed-stage-results)
<a id="6-drag"></a>
- [6. Drag](PP_DESIGN_SPEC.md#race-modes-product-spec--6-drag)
<a id="61-base-hardware-use"></a>
- [6.1 Base hardware use](PP_DESIGN_SPEC.md#race-modes-product-spec--61-base-hardware-use)
<a id="62-base-staging"></a>
- [6.2 Base staging](PP_DESIGN_SPEC.md#race-modes-product-spec--62-base-staging)
<a id="63-reaction-and-et"></a>
- [6.3 Reaction and ET](PP_DESIGN_SPEC.md#race-modes-product-spec--63-reaction-and-et)
<a id="64-false-start"></a>
- [6.4 False start](PP_DESIGN_SPEC.md#race-modes-product-spec--64-false-start)
<a id="65-start-tree"></a>
- [6.5 Start tree](PP_DESIGN_SPEC.md#race-modes-product-spec--65-start-tree)
<a id="66-base-results-and-speed"></a>
- [6.6 Base results and speed](PP_DESIGN_SPEC.md#race-modes-product-spec--66-base-results-and-speed)
<a id="67-future-dual-beam--reu-concept"></a>
- [6.7 Future dual-beam / REU concept](PP_DESIGN_SPEC.md#race-modes-product-spec--67-future-dual-beam--reu-concept)
<a id="68-drag-display"></a>
- [6.8 Drag display](PP_DESIGN_SPEC.md#race-modes-product-spec--68-drag-display)
<a id="69-base-drag-staging-screen-behaviour"></a>
- [6.9 Base Drag staging-screen behaviour](PP_DESIGN_SPEC.md#race-modes-product-spec--69-base-drag-staging-screen-behaviour)
<a id="610-base-drag-result-screen"></a>
- [6.10 Base Drag result screen](PP_DESIGN_SPEC.md#race-modes-product-spec--610-base-drag-result-screen)
<a id="7-general-principle"></a>
- [7. General principle](PP_DESIGN_SPEC.md#race-modes-product-spec--7-general-principle)
<a id="browser-setup-flow-refinements"></a>
- [Browser setup flow refinements](PP_DESIGN_SPEC.md#race-modes-product-spec--browser-setup-flow-refinements)
<a id="timed-stage"></a>
- [Timed Stage](PP_DESIGN_SPEC.md#race-modes-product-spec--timed-stage)
<a id="drag-setup"></a>
- [Drag setup](PP_DESIGN_SPEC.md#race-modes-product-spec--drag-setup)
