# Formal verification

| Document | Role |
|----------|------|
| **[FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)** | **Current freeze** — single status page, operator entry, gates |
| [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md) | Pre-release commands, regen/lock triggers, CI vs local |
| [FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md) | Freeze checklist (date, commands, maintenance mode) |
| [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md) | Authoritative verdict: what the Coq *model* proves and what it does not |
| [FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md](./FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md) | Post-verdict gap closures |
| [FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md](./FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md) | Replay-txid fix + 9 high-risk vectors |
| [SHARED-HASH-FEASIBILITY-2026-08.md](./SHARED-HASH-FEASIBILITY-2026-08.md) | Shared-hash feasibility (no production binary) |
| [LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md](./LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md) | Lightweight σ / convergence-root correspondence |
| [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md) | Test-only production σ/root CLI (gate step 4; not refinement) |
| [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md) | Intentional vector-set splits |
| [Architecture snapshot](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) | 2026-08-20 Classification freeze (guaranteed / not-guaranteed) |
| [Architecture assessment](../formal/RIGOR_FORMAL_VERIFICATION_ARCHITECTURE_ASSESSMENT_REPORT.md) | 2026-08-20 inventory, gaps, roadmap (no protocol change) |
| [L2↔L3 refinement feasibility](../formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) | Why full refinement is architectural; Path A/B/C; flags stay false |

Dated round notes (**historical**, not the freeze page): [R2](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md), [R3](./FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md).

Machine-checked sources: [`../../formal_verification/`](../../formal_verification/).

Historical PROOF-xxx series: [`../archive/proof/`](../archive/proof/).

`equivalence_claimed` is **false**. This is not a refinement proof.
