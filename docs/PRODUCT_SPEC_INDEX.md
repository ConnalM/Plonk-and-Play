# Plonk & Play™ — documentation index

**Owner-approved hierarchy (19 November 2026).** One product, one design, one delivery plan. This is navigation, not another specification.

| Level | Document | Authority |
|---|---|---|
| Principles | [Design Constitution](DESIGN_CONSTITUTION.md) | Governing principles |
| Requirements | [System Requirements](SYSTEM_REQUIREMENTS.md) | High-level capabilities |
| Design | [P&P Design Specification](PP_DESIGN_SPEC.md) | Sole architecture, behaviour and detailed design |
| Delivery | [Master Project Plan](MASTER_PROJECT_PLAN.md) | Sole current/later-v1/post-v1 backlog, priorities and evidence |
| Acceptance | ACCEPTANCE_TESTS_*.md, [WP6 matrix](WP6_FAILURE_RECOVERY_MATRIX.md), [standalone record-eligibility acceptance](STANDALONE_TASTER_RECORD_ELIGIBILITY_PLAN.md), acceptance/ | Proof requirements and results |
| Engineering reference | [Development workflow](DEVELOPMENT_WORKFLOW.md), [Wokwi](WOKWI_DEVELOPMENT_WORKFLOW.md), [XIP build](WP7_XIP_APPLICATION_BUILD_REFERENCE.md), [checkpoint procedure](HUMAN_CHECKPOINT_PROCEDURE.md) | Build/test procedures |
| History | Dated review and investigation files, Git commits and PRs | Historical evidence, not active specifications/plans |

## Design navigation

- [System architecture and lifecycle](PP_DESIGN_SPEC.md#system-architecture)
- [Race Control and Race Engine](PP_DESIGN_SPEC.md#race-control-and-race-engine)
- [Inputs and outputs](PP_DESIGN_SPEC.md#inputs-and-outputs)
- [Communications and system time](PP_DESIGN_SPEC.md#communications-and-system-time)
- [Memory and storage](PP_DESIGN_SPEC.md#memory-and-storage)
- [Presentation: Integrated Display, Browser and audio](PP_DESIGN_SPEC.md#presentation)
- [Configuration](PP_DESIGN_SPEC.md#configuration)
- [Diagnostics and recovery](PP_DESIGN_SPEC.md#diagnostics-and-recovery)
- [Physical product](PP_DESIGN_SPEC.md#physical-product)

## Migration and retirement

22 prior independent design files retired as governing specifications; each contains links to the corresponding section and exact original Git revision. The standalone eligibility plan is now an acceptance reference only. Dated reviews/investigations are evidence rather than competing task lists. Structural transfer is complete. The follow-up reconciliation removes obsolete Stage limits and resolves known contradictions in record policy, defaults, rotation and mode scope; remaining cross-document reference/evidence checks and integration are DOC-01. No firmware or release-scope change is implied.

**Working rule:** amend the relevant section of the single design for product decisions, and the master plan for outstanding work. No new parallel governing specification or to-do list. GitHub is the durable source of truth.
