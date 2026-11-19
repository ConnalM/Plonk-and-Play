# Plonk & Play™ System Architecture — retired

**RETIRED as a separate governing specification on 19 November 2026.** The authoritative content has moved into the single [P&P Design Specification](PP_DESIGN_SPEC.md#system-architecture--plonk--play-system-architecture). Do not edit requirements here.

[Original preserved revision](https://github.com/ConnalM/Plonk-and-Play/blob/35aedb5470f9a06193aefd2073e5a911acae53da/docs/SYSTEM_ARCHITECTURE.md) (historical only).

## Legacy section links

<a id="1-architectural-approach"></a>
- [1. Architectural approach](PP_DESIGN_SPEC.md#system-architecture--1-architectural-approach)
<a id="2-high-level-responsibilities"></a>
- [2. High-level responsibilities](PP_DESIGN_SPEC.md#system-architecture--2-high-level-responsibilities)
<a id="3-devices-capabilities-and-physical-mounting"></a>
- [3. Devices, capabilities and physical mounting](PP_DESIGN_SPEC.md#system-architecture--3-devices-capabilities-and-physical-mounting)
<a id="4-input-architecture"></a>
- [4. Input architecture](PP_DESIGN_SPEC.md#system-architecture--4-input-architecture)
<a id="41-input-module-and-input-devices"></a>
- [4.1 Input Module and Input Devices](PP_DESIGN_SPEC.md#system-architecture--41-input-module-and-input-devices)
<a id="42-detector-events"></a>
- [4.2 Detector events](PP_DESIGN_SPEC.md#system-architecture--42-detector-events)
<a id="43-extensible-events"></a>
- [4.3 Extensible events](PP_DESIGN_SPEC.md#system-architecture--43-extensible-events)
<a id="44-device-roles-and-session-interpretation"></a>
- [4.4 Device roles and session interpretation](PP_DESIGN_SPEC.md#system-architecture--44-device-roles-and-session-interpretation)
<a id="45-transport-independence-and-transport-adapters"></a>
- [4.5 Transport independence and Transport Adapters](PP_DESIGN_SPEC.md#system-architecture--45-transport-independence-and-transport-adapters)
<a id="46-separation-of-knowledge"></a>
- [4.6 Separation of knowledge](PP_DESIGN_SPEC.md#system-architecture--46-separation-of-knowledge)
<a id="5-device-failure-and-replacement"></a>
- [5. Device failure and replacement](PP_DESIGN_SPEC.md#system-architecture--5-device-failure-and-replacement)
<a id="6-registry-and-discovery"></a>
- [6. Registry and Discovery](PP_DESIGN_SPEC.md#system-architecture--6-registry-and-discovery)
<a id="61-stable-identity"></a>
- [6.1 Stable identity](PP_DESIGN_SPEC.md#system-architecture--61-stable-identity)
<a id="62-replacement-devices"></a>
- [6.2 Replacement devices](PP_DESIGN_SPEC.md#system-architecture--62-replacement-devices)
<a id="63-simple-directly-connected-hardware"></a>
- [6.3 Simple directly connected hardware](PP_DESIGN_SPEC.md#system-architecture--63-simple-directly-connected-hardware)
<a id="64-capability-level-availability"></a>
- [6.4 Capability-level availability](PP_DESIGN_SPEC.md#system-architecture--64-capability-level-availability)
<a id="65-transport-independent-discovery"></a>
- [6.5 Transport-independent discovery](PP_DESIGN_SPEC.md#system-architecture--65-transport-independent-discovery)
<a id="66-power-on-reconciliation-and-readiness"></a>
- [6.6 Power-on reconciliation and readiness](PP_DESIGN_SPEC.md#system-architecture--66-power-on-reconciliation-and-readiness)
<a id="7-configuration-race-setup-and-working-ram"></a>
- [7. Configuration, Race Setup and working RAM](PP_DESIGN_SPEC.md#system-architecture--7-configuration-race-setup-and-working-ram)
<a id="71-installation-configuration"></a>
- [7.1 Installation configuration](PP_DESIGN_SPEC.md#system-architecture--71-installation-configuration)
<a id="72-race-setup"></a>
- [7.2 Race Setup](PP_DESIGN_SPEC.md#system-architecture--72-race-setup)
<a id="73-user--presentation-preferences"></a>
- [7.3 User / presentation preferences](PP_DESIGN_SPEC.md#system-architecture--73-user--presentation-preferences)
<a id="74-configuration-versus-live-state"></a>
- [7.4 Configuration versus live state](PP_DESIGN_SPEC.md#system-architecture--74-configuration-versus-live-state)
<a id="75-startup-reconciliation"></a>
- [7.5 Startup reconciliation](PP_DESIGN_SPEC.md#system-architecture--75-startup-reconciliation)
<a id="76-configuration-changes-during-operation"></a>
- [7.6 Configuration changes during operation](PP_DESIGN_SPEC.md#system-architecture--76-configuration-changes-during-operation)
<a id="77-optional-drivercar-data"></a>
- [7.7 Optional driver/car data](PP_DESIGN_SPEC.md#system-architecture--77-optional-drivercar-data)
<a id="77-authoritative-noticeboard"></a>
- [7.7 Authoritative Noticeboard](PP_DESIGN_SPEC.md#system-architecture--77-authoritative-noticeboard)
<a id="8-pp-system-time"></a>
- [8. P&P System Time](PP_DESIGN_SPEC.md#system-architecture--8-pp-system-time)
<a id="9-race-control"></a>
- [9. Race Control](PP_DESIGN_SPEC.md#system-architecture--9-race-control)
<a id="90-normal-live-communication"></a>
- [9.0 Normal live communication](PP_DESIGN_SPEC.md#system-architecture--90-normal-live-communication)
<a id="91-race-modes"></a>
- [9.1 Race modes](PP_DESIGN_SPEC.md#system-architecture--91-race-modes)
<a id="92-optional-race-features"></a>
- [9.2 Optional race features](PP_DESIGN_SPEC.md#system-architecture--92-optional-race-features)
<a id="93-racesession-state-and-calculations"></a>
- [9.3 Race/session state and calculations](PP_DESIGN_SPEC.md#system-architecture--93-racesession-state-and-calculations)
<a id="94-race-facts-versus-actions"></a>
- [9.4 Race facts versus actions](PP_DESIGN_SPEC.md#system-architecture--94-race-facts-versus-actions)
<a id="10-output-architecture"></a>
- [10. Output architecture](PP_DESIGN_SPEC.md#system-architecture--10-output-architecture)
<a id="101-time-critical-outputs"></a>
- [10.1 Time-critical outputs](PP_DESIGN_SPEC.md#system-architecture--101-time-critical-outputs)
<a id="102-failure-behaviour"></a>
- [10.2 Failure behaviour](PP_DESIGN_SPEC.md#system-architecture--102-failure-behaviour)
<a id="103-track-power"></a>
- [10.3 Track power](PP_DESIGN_SPEC.md#system-architecture--103-track-power)
<a id="11-sound"></a>
- [11. Sound](PP_DESIGN_SPEC.md#system-architecture--11-sound)
<a id="12-presentation-and-user-interaction"></a>
- [12. Presentation and user interaction](PP_DESIGN_SPEC.md#system-architecture--12-presentation-and-user-interaction)
<a id="120-local-browser-access"></a>
- [12.0 Local Browser access](PP_DESIGN_SPEC.md#system-architecture--120-local-browser-access)
<a id="121-context-sensitive-controls"></a>
- [12.1 Context-sensitive controls](PP_DESIGN_SPEC.md#system-architecture--121-context-sensitive-controls)
<a id="122-one-master--race-director"></a>
- [12.2 One Master / Race Director](PP_DESIGN_SPEC.md#system-architecture--122-one-master--race-director)
<a id="123-non-master-clients"></a>
- [12.3 Non-Master clients](PP_DESIGN_SPEC.md#system-architecture--123-non-master-clients)
<a id="124-requests-and-browser-synchronisation"></a>
- [12.4 Requests and browser synchronisation](PP_DESIGN_SPEC.md#system-architecture--124-requests-and-browser-synchronisation)
<a id="125-browser-clockdisplay-updates"></a>
- [12.5 Browser clock/display updates](PP_DESIGN_SPEC.md#system-architecture--125-browser-clockdisplay-updates)
<a id="13-memory-module"></a>
- [13. Memory Module](PP_DESIGN_SPEC.md#system-architecture--13-memory-module)
<a id="130-physical-storage-resource-ownership"></a>
- [13.0 Physical storage resource ownership](PP_DESIGN_SPEC.md#system-architecture--130-physical-storage-resource-ownership)
<a id="131-information-ownership-and-delivery"></a>
- [13.1 Information ownership and delivery](PP_DESIGN_SPEC.md#system-architecture--131-information-ownership-and-delivery)
<a id="132-event-ordering-duplication-and-availability"></a>
- [13.2 Event ordering, duplication and availability](PP_DESIGN_SPEC.md#system-architecture--132-event-ordering-duplication-and-availability)
<a id="133-identity-and-low-level-addressing"></a>
- [13.3 Identity and low-level addressing](PP_DESIGN_SPEC.md#system-architecture--133-identity-and-low-level-addressing)
<a id="134-system-controller-and-communication-topology"></a>
- [13.4 System Controller and communication topology](PP_DESIGN_SPEC.md#system-architecture--134-system-controller-and-communication-topology)
<a id="135-pp-communications-bus"></a>
- [13.5 P&P communications bus](PP_DESIGN_SPEC.md#system-architecture--135-pp-communications-bus)
<a id="1351-authority-and-message-policing"></a>
- [13.5.1 Authority and message policing](PP_DESIGN_SPEC.md#system-architecture--1351-authority-and-message-policing)
<a id="1352-one-message-one-or-many-consumers"></a>
- [13.5.2 One message, one or many consumers](PP_DESIGN_SPEC.md#system-architecture--1352-one-message-one-or-many-consumers)
<a id="1353-state-notification-and-resynchronisation"></a>
- [13.5.3 State notification and resynchronisation](PP_DESIGN_SPEC.md#system-architecture--1353-state-notification-and-resynchronisation)
<a id="1354-transport-adapters"></a>
- [13.5.4 Transport Adapters](PP_DESIGN_SPEC.md#system-architecture--1354-transport-adapters)
<a id="136-manual-user-gateway-mug"></a>
- [13.6 Manual User Gateway (MUG)](PP_DESIGN_SPEC.md#system-architecture--136-manual-user-gateway-mug)
<a id="14-test-and-diagnostics"></a>
- [14. Test and diagnostics](PP_DESIGN_SPEC.md#system-architecture--14-test-and-diagnostics)
<a id="15-failure-and-recovery"></a>
- [15. Failure and recovery](PP_DESIGN_SPEC.md#system-architecture--15-failure-and-recovery)
<a id="16-updates-and-compatibility"></a>
- [16. Updates and compatibility](PP_DESIGN_SPEC.md#system-architecture--16-updates-and-compatibility)
<a id="17-detailed-design-status"></a>
- [17. Detailed design status](PP_DESIGN_SPEC.md#system-architecture--17-detailed-design-status)
