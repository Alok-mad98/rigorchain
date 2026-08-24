# Formal verification architecture (assessment)

This folder holds **architecture-level evaluation** of the RIGOR formal
verification system. It does not change consensus, kernel, validation,
wallet, miner, or RFC text.

Authoritative operator freeze remains:

- [`../verification/FORMAL-VERIFICATION-STATUS.md`](../verification/FORMAL-VERIFICATION-STATUS.md)
- [`../verification/FV-RELEASE-CHECKLIST.md`](../verification/FV-RELEASE-CHECKLIST.md)
- [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md)

Honesty flags (unchanged):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

| Document | Role |
|----------|------|
| [RIGOR_FORMAL_VERIFICATION_ARCHITECTURE_ASSESSMENT_REPORT.md](./RIGOR_FORMAL_VERIFICATION_ARCHITECTURE_ASSESSMENT_REPORT.md) | Final report: what is achieved, guaranteed, not guaranteed, RC0 sufficiency |
| [RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md](./RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) | **RC0 freeze snapshot** (2026-08-20, `TASK-FV-FREEZE-RC0-READINESS-001`): §0 declaration; living Classification **422 / 215**; guaranteed / not-guaranteed; residual opaques §9 |
| [RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) | L2↔L3 refinement feasibility (2026-08-20): semantic gaps, `DCS_Impossible`, Path A/B/C; **not** an implementation plan |
| [RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md](./RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md) | P0–P3 improvement register (full refinement is an explicit non-gap) |
| [RIGOR_FORMAL_VERIFICATION_ROADMAP.md](./RIGOR_FORMAL_VERIFICATION_ROADMAP.md) | Phased future work (no protocol change; no refinement phase) |

Existing module audits in this folder (RFC-017 / RFC-018 / supply):

- [`rfc017_res_schedule/`](./rfc017_res_schedule/)
- [`rfc018_fee_burn/`](./rfc018_fee_burn/)
- [`phase4d_supply/`](./phase4d_supply/)
- [`phase4e_connectblock/`](./phase4e_connectblock/)
