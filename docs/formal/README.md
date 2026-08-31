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
| [RIGOR_CONSENSUS_KERNEL_CANARY_MIGRATION_P0_068.md](./RIGOR_CONSENSUS_KERNEL_CANARY_MIGRATION_P0_068.md) | P0-068 canary: C++ vs C ABI dual-path; production default OFF; no equivalence claim |
| [RIGOR_CONSENSUS_KERNEL_MIGRATION_FREEZE_P0_068.md](./RIGOR_CONSENSUS_KERNEL_MIGRATION_FREEZE_P0_068.md) | P0-068 freeze audit: C++ = candidate = refinement pins; production switch remains OFF |
| [RIGOR_CONSENSUS_KERNEL_TESTNET_CANARY_P0_069.md](./RIGOR_CONSENSUS_KERNEL_TESTNET_CANARY_P0_069.md) | P0-069 testnet/regtest runtime canary; mainnet forbidden; production default OFF |
| [../operations/RIGOR_CONSENSUS_KERNEL_PRODUCTION_CANARY_RUNBOOK_P0_070.md](../operations/RIGOR_CONSENSUS_KERNEL_PRODUCTION_CANARY_RUNBOOK_P0_070.md) | P0-070 production canary readiness: rollout/rollback/gate |
| [../operations/RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_P0_071.md](../operations/RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_P0_071.md) | P0-071 mainnet compare-only canary; C++ remains unique decision |
| [../operations/RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN_P0_072.md](../operations/RIGOR_CONSENSUS_KERNEL_MAINNET_CANARY_LONGRUN_P0_072.md) | P0-072 long-run mainnet canary observation; compare-only; 100000-block soak |
| [RIGOR_CONSENSUS_KERNEL_GENESIS_AUTHORITY_P0_073.md](./RIGOR_CONSENSUS_KERNEL_GENESIS_AUTHORITY_P0_073.md) | P0-073 genesis Kernel is unique production authority; Option B; no OCaml |
| [RIGOR_FORMAL_CONSENSUS_FINAL_FREEZE_P0_074.md](./RIGOR_FORMAL_CONSENSUS_FINAL_FREEZE_P0_074.md) | P0-074 formal consensus genesis-release freeze; refinement true; equivalence false |
| [RIGOR_GENESIS_CONSENSUS_RELEASE_AUDIT_P0_075.md](./RIGOR_GENESIS_CONSENSUS_RELEASE_AUDIT_P0_075.md) | P0-075 genesis launch consensus audit; ready true; equivalence false |

Existing module audits in this folder (RFC-017 / RFC-018 / supply):

- [`rfc017_res_schedule/`](./rfc017_res_schedule/)
- [`rfc018_fee_burn/`](./rfc018_fee_burn/)
- [`phase4d_supply/`](./phase4d_supply/)
- [`phase4e_connectblock/`](./phase4e_connectblock/)
