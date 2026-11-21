# Race Control and Race Engine Detailed Design — retired

**RETIRED as a separate governing specification on 19 November 2026.** The authoritative content has moved into the single [P&P Design Specification](PP_DESIGN_SPEC.md#race-control-engine-design--race-control-and-race-engine-detailed-design). Do not edit requirements here.

[Original preserved revision](https://github.com/ConnalM/Plonk-and-Play/blob/35aedb5470f9a06193aefd2073e5a911acae53da/docs/RACE_CONTROL_ENGINE_DESIGN.md) (historical only).

## Legacy section links

<a id="1-purpose"></a>
- [1. Purpose](PP_DESIGN_SPEC.md#race-control-engine-design--1-purpose)
<a id="2-responsibility-split"></a>
- [2. Responsibility split](PP_DESIGN_SPEC.md#race-control-engine-design--2-responsibility-split)
<a id="3-conceptual-boundary"></a>
- [3. Conceptual boundary](PP_DESIGN_SPEC.md#race-control-engine-design--3-conceptual-boundary)
<a id="race-control-to-race-engine"></a>
- [Race Control to Race Engine](PP_DESIGN_SPEC.md#race-control-engine-design--race-control-to-race-engine)
<a id="race-engine-to-race-control"></a>
- [Race Engine to Race Control](PP_DESIGN_SPEC.md#race-control-engine-design--race-engine-to-race-control)
<a id="4-session-definition"></a>
- [4. Session definition](PP_DESIGN_SPEC.md#race-control-engine-design--4-session-definition)
<a id="41-session-source-and-record-policy"></a>
- [4.1 Session source and record policy](PP_DESIGN_SPEC.md#race-control-engine-design--41-session-source-and-record-policy)
<a id="5-time-and-event-ordering"></a>
- [5. Time and event ordering](PP_DESIGN_SPEC.md#race-control-engine-design--5-time-and-event-ordering)
<a id="6-completion-stop-and-reset"></a>
- [6. Completion, stop and reset](PP_DESIGN_SPEC.md#race-control-engine-design--6-completion-stop-and-reset)
<a id="7-race-modes"></a>
- [7. Race modes](PP_DESIGN_SPEC.md#race-control-engine-design--7-race-modes)
<a id="8-optional-race-features"></a>
- [8. Optional race features](PP_DESIGN_SPEC.md#race-control-engine-design--8-optional-race-features)
<a id="81-startfinish-first-crossing-behaviour"></a>
- [8.1 Start/Finish first-crossing behaviour](PP_DESIGN_SPEC.md#race-control-engine-design--81-startfinish-first-crossing-behaviour)
<a id="9-hardware-availability-and-outputs"></a>
- [9. Hardware, availability and outputs](PP_DESIGN_SPEC.md#race-control-engine-design--9-hardware-availability-and-outputs)
<a id="10-information-boundaries"></a>
- [10. Information boundaries](PP_DESIGN_SPEC.md#race-control-engine-design--10-information-boundaries)
<a id="101-authoritative-facts-current-state-noticeboard-and-persistence"></a>
- [10.1 Authoritative facts, current state, Noticeboard and persistence](PP_DESIGN_SPEC.md#race-control-engine-design--101-authoritative-facts-current-state-noticeboard-and-persistence)
<a id="11-testability"></a>
- [11. Testability](PP_DESIGN_SPEC.md#race-control-engine-design--11-testability)
<a id="12-proportionality"></a>
- [12. Proportionality](PP_DESIGN_SPEC.md#race-control-engine-design--12-proportionality)
