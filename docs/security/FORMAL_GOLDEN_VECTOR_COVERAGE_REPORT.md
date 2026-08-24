# Golden Vector Coverage Report

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-COVERAGE-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Authority lock:** `formal_verification/golden_lock.json`  
**Corpus:** `formal_verification/phase3_golden_vectors.json`

## Counts

| Metric | Value | Note |
|--------|-------|------|
| Shared vectors (living) | **422** | Task text cited “402” — that was pre-EdgeCross densify (`402/195`). Living lock is 422. |
| Coq-enforced harness lemmas | **215** | 187 GoldenHarness + 28 NegativeCoreHarness |
| JSON `coq_enforce: true` | 190 | Do not cite as the L1↔L2 lock (omit-key kernel rows still enforced) |
| RC0 freeze | 414 / 207 | Historical |
| Operator freeze day | 324 / 117 | Historical |

**This audit does not add vectors** (none identified as a new verification-boundary gap that lacks both Coq coverage and an existing family).

## Coverage by category

| Audit category | Families / prefixes | Shared | Coq-enforced | Verdict |
|----------------|---------------------|--------|--------------|---------|
| Consensus / selection | RunSelection (`WIN`/`A9C` mix), Inequality (`INQ`) | 84 + 25 | 7 + 25 | Strong vector; selection bulk is Python/C++ |
| Script | ScriptMintNeg (`SMW`), shape crosses in EdgeCross | 12 + part of 20 | 12 + part | Reject-form strong; interpreter not gated |
| Convergence | ConvergenceRoot (`CR`), InvalidCvg (`ICVG`), DCR (`DCR`), Sigma (`SIG`/`ENC`) | 25+14+36+50+10 | 9+14+36+5+10 | Strong on roots/σ layout; SHA-heavy SIG mostly L1/L3 |
| RES monetary | NegativeCore (`NEG`), EdgeCross schedule rows, SupplyScaffold (`SUP`) | 16+subset+5 | 16+subset+0 | Cap/dual-pool/H-1 strong; full `k=0..251` via parity test (not 252 JSON rows) |
| Supply / replay structure | SupplyScaffold, Replay (`RP`), BlockTrace (`BLK`), ReorgBind (`REORG`/`RO`) | 5+3+12+12 | 0 | Scaffold / structure — Python/C++ only by design |
| Failure / negative | NEG, ICVG, SMW, DCR-024…035, XFAM | 16+14+12+12+20 | all those | High-value reject surface well covered |
| Hash | HashKernel (`HASH`), Sha256Fips (`FIPS`) | 29+8 | 2+0 (FIPS in `CanonicalHashKernel.v`) | Finite tripartite + FIPS lock |

## Missing high-value vectors (assessed, not added)

| Candidate | Why not added |
|-----------|---------------|
| Mid-schedule wrong `W(k)` for every epoch | Already pinned by Coq `reward_pin_table` + Python↔C++ parity for all `k∈0..251` |
| Owner/PCORB mint-auth failures | Outside Coq `cm_draw_won` boundary; C++ unit/fuzz already own this surface |
| Full CheckTransaction interpreter rejects | Interpreter not in L2; would imply false equivalence progress |
| Network/reorg P2P races | Out of FV boundary |

## Recommendation

Keep lock discipline: bump `golden_lock.json` only with a named vector task. Prefer Coq lemmas or the RFC-017 parity gate over JSON growth when the property is already universally quantified in the model.
