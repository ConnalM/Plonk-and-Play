# P&P System Lifecycle and Persistence — retired

**RETIRED as a separate governing specification on 19 November 2026.** The authoritative content has moved into the single [P&P Design Specification](PP_DESIGN_SPEC.md#system-lifecycle--pp-system-lifecycle-and-persistence). Do not edit requirements here.

[Original preserved revision](https://github.com/ConnalM/Plonk-and-Play/blob/35aedb5470f9a06193aefd2073e5a911acae53da/docs/SYSTEM_LIFECYCLE.md) (historical only).

## Legacy section links

<a id="1-purpose"></a>
- [1. Purpose](PP_DESIGN_SPEC.md#system-lifecycle--1-purpose)
<a id="2-overall-lifecycle"></a>
- [2. Overall lifecycle](PP_DESIGN_SPEC.md#system-lifecycle--2-overall-lifecycle)
<a id="3-core-initialisation"></a>
- [3. Core initialisation](PP_DESIGN_SPEC.md#system-lifecycle--3-core-initialisation)
<a id="4-persistent-state-loaded-into-working-ram-at-startup"></a>
- [4. Persistent state loaded into working RAM at startup](PP_DESIGN_SPEC.md#system-lifecycle--4-persistent-state-loaded-into-working-ram-at-startup)
<a id="5-discovery-and-reconciliation"></a>
- [5. Discovery and reconciliation](PP_DESIGN_SPEC.md#system-lifecycle--5-discovery-and-reconciliation)
<a id="6-device-initialisation-and-time-synchronisation"></a>
- [6. Device initialisation and time synchronisation](PP_DESIGN_SPEC.md#system-lifecycle--6-device-initialisation-and-time-synchronisation)
<a id="7-system-assembly-and-availability"></a>
- [7. System assembly and availability](PP_DESIGN_SPEC.md#system-lifecycle--7-system-assembly-and-availability)
<a id="8-first-time-startup-and-factory-defaults"></a>
- [8. First-time startup and factory defaults](PP_DESIGN_SPEC.md#system-lifecycle--8-first-time-startup-and-factory-defaults)
<a id="9-missing-new-and-replacement-equipment"></a>
- [9. Missing, new and replacement equipment](PP_DESIGN_SPEC.md#system-lifecycle--9-missing-new-and-replacement-equipment)
<a id="expected-equipment-missing"></a>
- [Expected equipment missing](PP_DESIGN_SPEC.md#system-lifecycle--expected-equipment-missing)
<a id="new-equipment"></a>
- [New equipment](PP_DESIGN_SPEC.md#system-lifecycle--new-equipment)
<a id="replacement-equipment"></a>
- [Replacement equipment](PP_DESIGN_SPEC.md#system-lifecycle--replacement-equipment)
<a id="10-missing-or-unusable-stored-configuration"></a>
- [10. Missing or unusable stored configuration](PP_DESIGN_SPEC.md#system-lifecycle--10-missing-or-unusable-stored-configuration)
<a id="11-persistence-policy"></a>
- [11. Persistence policy](PP_DESIGN_SPEC.md#system-lifecycle--11-persistence-policy)
<a id="configuration-and-system-state"></a>
- [Configuration and system state](PP_DESIGN_SPEC.md#system-lifecycle--configuration-and-system-state)
<a id="live-individual-race-state"></a>
- [Live individual-race state](PP_DESIGN_SPEC.md#system-lifecycle--live-individual-race-state)
<a id="completed-race-data"></a>
- [Completed race data](PP_DESIGN_SPEC.md#system-lifecycle--completed-race-data)
<a id="12-multi-race-competition-progress"></a>
- [12. Multi-race competition progress](PP_DESIGN_SPEC.md#system-lifecycle--12-multi-race-competition-progress)
<a id="13-unfinished-competition-at-startup"></a>
- [13. Unfinished competition at startup](PP_DESIGN_SPEC.md#system-lifecycle--13-unfinished-competition-at-startup)
<a id="14-graceful-shutdown"></a>
- [14. Graceful shutdown](PP_DESIGN_SPEC.md#system-lifecycle--14-graceful-shutdown)
<a id="15-uncontrolled-shutdown"></a>
- [15. Uncontrolled shutdown](PP_DESIGN_SPEC.md#system-lifecycle--15-uncontrolled-shutdown)
<a id="16-device-disappearance-and-reconnection"></a>
- [16. Device disappearance and reconnection](PP_DESIGN_SPEC.md#system-lifecycle--16-device-disappearance-and-reconnection)
<a id="17-hardware-fault-containment-and-proportionality"></a>
- [17. Hardware fault containment and proportionality](PP_DESIGN_SPEC.md#system-lifecycle--17-hardware-fault-containment-and-proportionality)
