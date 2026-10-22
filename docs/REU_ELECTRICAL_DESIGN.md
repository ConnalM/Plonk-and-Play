# REU Electrical Design

**Status:** Working electrical design/specification beneath the authoritative REU Architecture and Capability Contract  
**Owner:** Rex — REU electrical/hardware design  
**Architectural authority:** `docs/REU_ARCHITECTURE_AND_CAPABILITY_CONTRACT.md`  
**Date:** 22 October 2026

This document records electrical requirements and proof work beneath the architectural Bible. It does not authorise final component selection, PCB layout, manufacturing files or firmware implementation.

---

## REU-EL-TRS-01 — TRS insertion/removal protection

Native TRS interfaces must tolerate plug insertion and removal without damage to REU circuitry.

Sensitive controller-demand sensing, MCU and active lane-control circuitry must not be exposed unprotected to the temporary Tip/Ring/Sleeve combinations that occur while a TRS plug is being inserted or removed.

The desired behaviour is conceptually:

- **NO PLUG** → sensitive circuitry isolated
- **INSERTION** → sensitive circuitry isolated
- **FULLY SEATED** → settle / validate → sensing may be enabled
- **REMOVAL BEGINS** → sensitive circuitry protected/isolated

This protection must not rely solely on the ESP32 reacting after an unsafe electrical condition has already occurred. The hardware immediately behind the connector must itself tolerate plausible insertion/removal transients and misconnections.

Investigate whether a suitable switched TRS jack, including plug-present or suitably late-mating detection, can provide reliable mechanical insertion state and potentially participate directly in hardware isolation. Do not assume this is the final solution: compare it with other practical protection/isolation arrangements.

### Architectural constraint

**Do not make Transparent Human passive fail-through dependent on firmware, MCU operation or successful plug detection.**

Treat these as two distinct paths where appropriate:

1. **Passive controller→powerbase fail-through path** — robust enough to tolerate normal TRS insertion/removal and capable of ordinary operation with REU unpowered, where the approved interface supports passive fail-through.
2. **REU sensing/active-electronics path** — appropriately limited/protected/isolated so transient TRS contact combinations cannot damage vulnerable REU electronics.

### REU-001 proof work

REU-001 shall include testing of the actual proposed TRS socket and deliberately exercise:

- slow insertion;
- rapid insertion;
- partial insertion;
- withdrawal;
- repeated insertion/removal;
- powerbase powered during insertion/removal;
- REU powered and unpowered;
- plausible temporary Tip/Ring/Sleeve misconnections.

Determine the electrical stresses that actually occur before choosing the final protection/isolation implementation.

### Design consequence to assess

REU-EL-TRS-01 may refine the Controller Interface block into two electrically distinct concerns: a robust passive fail-through path and a protected/isolatable sensing/active-electronics branch. Rex shall determine during REU-001 whether this requires any material change to the proposed Controller Interface or passive-bypass architecture and return any architectural consequence to Connal/System Architecture for review.

No production TRS jack, final isolation/protection implementation or PCB design is selected by this requirement.
