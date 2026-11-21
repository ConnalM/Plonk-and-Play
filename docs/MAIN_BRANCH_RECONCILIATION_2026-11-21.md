# Main-branch divergence reconciliation — 21 November 2026

**Status:** Historical-reference and backlog-reconciliation record. It is not a
second Master Plan, Design Specification or REU implementation authorisation.

## Purpose and authority

The development checkpoint `c84684d4bf312bd54055361cffc1e0a079df62cc`
and `main` `dc78e1193e12eafe6af1ed1cf9d6376367b7a81d` diverged from
`71208d31f3268913e1cd3e26cde96e7dc228a0b6`. The main-only side comprises
nine documentation commits and six Markdown paths; it contains no firmware,
tests, binaries, configuration, credential or acceptance-artifact change.

The [P&P Design Specification](PP_DESIGN_SPEC.md) and the
[Master Project Plan](MASTER_PROJECT_PLAN.md) remain the sole current
authorities. This record preserves source context without importing obsolete
roadmaps or promoting future REU work into V1.

## Main-only source record and disposition

| Main source | Preserved material | Disposition |
|---|---|---|
| [Old Master Project Plan](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/MASTER_PROJECT_PLAN.md) — `04f833b` | Early batch/round roadmap and hardware-inventory method. | Superseded as a roadmap. Do not restore it as an authority. Its optional **HAVE → BASIC TESTED → INTEGRATED → ACCEPTANCE TESTED** inventory method may inform later WP7/WP11 operations. |
| [Pre-production Register](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/PRE_PRODUCTION_REGISTER.md) — `73daf37`, `2886820` | Release-readiness, manufacturing, support, security and future motor-feedback prompts. | Selected missing release checks are now canonical backlog in WP10–WP12. Existing credential, recovery, update and soak requirements remain governed where already specified. |
| [Analogue Controller Interface](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/ANALOGUE_CONTROLLER_INTERFACE_DESIGN.md) — `de2152f`, `bdf1bf0` | Controller/powerbase/TRS/two-wire/pass-through concepts. | Future REU reference only. Its core boundary is retained: core P&P does not require controller/powerbase wiring. Detailed interface decisions are not V1 P&P requirements. |
| [REU Architecture Contract](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/REU_ARCHITECTURE_AND_CAPABILITY_CONTRACT.md) and [Rex Review Reconciliation](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/REU_ARCHITECTURE_REX_REVIEW_RECONCILIATION.md) — `14d062e`, `3576d59`, `6786301` | Future P&P/REU boundary, capability/availability, electrical protection and device-discovery principles. | Future architecture reference, not a parallel authority. Preserve the separation of P&P race truth and REU electrical truth; no REU feature is thereby promoted to V1. The review is derivative context for the contract, not a second specification. |
| [REU Electrical Design](https://github.com/ConnalM/Plonk-and-Play/blob/dc78e1193e12eafe6af1ed1cf9d6376367b7a81d/docs/REU_ELECTRICAL_DESIGN.md) — `dc78e11` | TRS insertion/removal protection and passive fail-through proof prompts. | Future REU feasibility/proof input. It does not select components, PCB, firmware or a V1 feature. |

## Current backlog effect

The Master Plan now explicitly retains the formerly missing release-readiness
reviews: security/privacy and production hardening in WP10; component,
connector/power protection, thermal, EMC/ESD and compliance decisions in WP11;
and provisioning, identity, end-of-line test, release/support/customer
information in WP12. These are gates for a releasable appliance, not authority
to change product behaviour or implementation now.

Future BEMF/RPM/motor-observation, analogue controller wiring, REU electrical
execution and dual-beam/REU Drag remain post-V1. A future REU board may assess
low-cost provision for sensing/coast capability then; no current P&P board or
V1 acceptance gate is created by this record.

## Open future decisions

Before any REU work becomes authoritative or scheduled, the Project Owner and
Architecture must separately decide:

1. whether any historic REU architecture/interface material is ratified as a
   future product baseline;
2. whether low-cost BEMF/coast/ADC provision is worthwhile for a future REU
   board;
3. the product name and terminology for an REU, avoiding collision with the
   existing Racer/MUG terminology;
4. the scope and priority of interface-specific electrical proof.

No decision in this record authorises a main merge, REU implementation,
controller wiring, firmware build/flash or Taster action.
