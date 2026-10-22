# P&P Pre-Production Register

## Purpose

This register is the parking place for work that does **not** need to interrupt current P&P development, but **must be considered and resolved before P&P is released as a production product**.

It is deliberately not the current development plan or a general wish list. Adding an item here does not authorise implementation now.

Statuses:
- **OPEN** — must be reviewed/resolved before production.
- **RESOLVED** — completed, with rationale/evidence recorded.
- **NOT REQUIRED** — reviewed and deliberately judged unnecessary, with rationale recorded.

## Security and network access

- **OPEN — P&P AP / Browser / API access review.** Test from the perspective of a curious nearby stranger: what can an unauthorised person connect to, view or control?
- **OPEN — Master/takeover permissions review.** Confirm that control ownership and takeover behaviour are appropriate for a production device.
- **OPEN — Home-network boundary.** Confirm that access to P&P cannot become an unintended route into the customer's home LAN.
- **OPEN — Wi-Fi credential exposure audit.** Wi-Fi credentials must not appear on SD, diagnostics exports, Browser/API responses or logs. P&P should not maintain an unnecessary duplicate credential store.
- **OPEN — ESP32 production security review.** Decide the proportionate production use of ESP32 facilities such as flash encryption, encrypted NVS and Secure Boot, including the realistic physical-extraction threat.
- **OPEN — Firmware/update security.** Review how production firmware is installed/updated and whether unauthorised firmware can create a meaningful risk.

## Hardware and production design

- **OPEN — Final PCB/component review.** Freeze production PCB, components and substitutions.
- **OPEN — Connector/interface review.** Confirm production connectors, pin protection and misuse/fault protection.
- **OPEN — Power design review.** Confirm supply requirements and protection.
- **OPEN — Enclosure and thermal review.** Confirm final enclosure, mounting, access and acceptable operating temperature.
- **OPEN — EMC/ESD and production tolerance review.** Determine the practical testing/protection required for a saleable product.
- **OPEN — REU future motor-feedback provision (REU-FUT-01).** Back-EMF measurement, motor-speed estimation and between-sensor position estimation are explicitly **not REU V1 features** and no V1 acceptance test shall depend on them. When the REU lane hardware is designed, preserve the option at negligible practical cost/complexity where possible: retain an ADC-capable input per lane; provide PCB pads/space for protected TRACK-to-BRAKE/return voltage sensing and conditioning; retain access to TRACK and BRAKE/return at that sensing point; provide a development test point where practical; and ensure the lane switching topology can produce a genuine COAST/high-impedance interval rather than only DRIVE and BRAKE. The purpose is only to avoid accidentally designing future motor feedback out. No V1 firmware, filtering, sampling rate, calibration, speed-estimation or position-estimation algorithm is required. Timing/ToF sensors remain authoritative; any future back-EMF-derived position estimate is experimental and must be periodically corrected by authoritative track sensors. Core REU operation remains through the existing controller connection: this provision must not create a requirement for extra track wiring or track modification.

## Reliability and recovery

- **OPEN — Power-loss behaviour.** Verify safe behaviour and recovery if power disappears during normal operation or a write.
- **OPEN — SD fault/recovery behaviour.** Verify behaviour for missing, full, corrupt or failing SD media.
- **OPEN — SD endurance/lifetime.** Review expected write load, wear and realistic service life.
- **OPEN — Factory reset and recovery.** Define and verify a safe user-accessible recovery path.
- **OPEN — Long-duration soak testing.** Run appropriate production-candidate endurance/reliability testing.

## Software/product finishing

- **OPEN — Development artefact cleanup.** Identify and remove or deliberately retain development-only behaviour and controls before release.
- **OPEN — Unavailable-mode behaviour.** Ensure disabled/unimplemented modes are presented cleanly and cannot be entered accidentally.
- **OPEN — Production defaults.** Review defaults from a new-owner perspective.
- **OPEN — Firmware versioning and upgrade/migration behaviour.** Define what happens to persistent data/configuration across production firmware upgrades.

## User experience

- **OPEN — First-time setup.** Test the complete new-owner experience without developer knowledge.
- **OPEN — Wi-Fi change / Disconnect / Forget workflow.** Verify understandable production behaviour.
- **OPEN — Lost-client / Master recovery.** Ensure an owner can recover control without specialist intervention.
- **OPEN — Error messages and recovery guidance.** Replace developer-oriented failures with useful customer-facing behaviour where appropriate.
- **OPEN — Browser compatibility/usability.** Test the intended phone/tablet/browser range and normal race use.

## Manufacturing and provisioning

- **OPEN — Production programming/provisioning procedure.** Define how a new unit becomes a known-good P&P.
- **OPEN — Product identity/serialisation.** Decide what unit identity is required and how it is assigned/stored.
- **OPEN — Production test procedure/jig.** Define a repeatable end-of-line hardware/function test.
- **OPEN — Known-good release image.** Define how production firmware releases are identified and controlled.

## Support and diagnostics

- **OPEN — Customer-safe diagnostics export.** Confirm one-file support workflow and that exports contain useful evidence without secrets/private credentials.
- **OPEN — Support/fault workflow.** Define what a customer does when P&P misbehaves and what evidence support needs.
- **OPEN — Firmware/hardware version visibility.** Make it straightforward to identify the unit's relevant versions when troubleshooting.
- **OPEN — Diagnostics privacy/redaction review.** Explicitly inspect production diagnostics for information that should not leave the device.

## Compliance and commercial release

- **OPEN — UKCA/CE applicability and testing.** Establish which requirements apply to the final product and obtain the necessary evidence/testing.
- **OPEN — RoHS/WEEE obligations.** Establish and satisfy applicable obligations.
- **OPEN — Supplied power-supply implications.** Decide whether P&P supplies a PSU and account for the compliance/support consequences.
- **OPEN — Product labels and required markings.** Determine final labelling requirements.
- **OPEN — Instructions and safety information.** Provide whatever user/safety documentation the final product requires.
- **OPEN — Warranty/returns/support policy.** Define the commercial support position before sale.

## Customer documentation

- **OPEN — Setup guide.**
- **OPEN — Normal operation guide.**
- **OPEN — Troubleshooting guide.**
- **OPEN — Firmware update instructions.**
- **OPEN — Factory reset/recovery instructions.**
- **OPEN — Support information.**

## Register rule

Whenever development produces a genuine **“not now, but before production”** item, record it here rather than diverting the current work or relying on memory. Product/design decisions remain with the product owner; an entry in this register is not authority to implement a new feature or change product behaviour.