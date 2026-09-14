# Formal Claim Traceability Matrix


> **Shipping issuance (RFC-020).** Continuous `destroy_cost(h)`, pole predicate
> `R · (2²⁵+h)² ≤ B << 40`, cap `2²²` RES, flat 128 ORU. Numeric SSOT:
> `tools/res_continuous_price_schedule.py`. Formal status:
> `audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`
> (`REFINEMENT_PROVED=false`).


**Document status:** Security audit  
**Historical verification reference:** P0-001  
**Date:** 2026-08-20  
**Source under review:** `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` (+ linked status pages)  
**Honesty:** `equivalence_claimed = false`

Status vocabulary: **VERIFIED** (Coq model, Closed), **VECTOR** (finite L1/L2/L3), **PARTIAL**, **DOC**, **NOT**.

| Claim (paraphrase) | Location | Proof / evidence artifact | Status |
|--------------------|----------|---------------------------|--------|
| Dual native assets ORU / RES | Ch.2; site Protocol | RFC-001 + C++ asset tags; Coq `AssetKind` / dual-pool | PARTIAL (model + impl; no refinement) |
| RES hard cap `2²²` | Ch.2 monetary table | `cap_is_closed_form`, `cap_is_upper_bound`; C++ `RES_MAX_SUPPLY` | VERIFIED (Coq model only) + VECTOR |
| RES `destroy_cost(h)` rises; no per-block grant | Ch.2 RFC-020 | `destroy_cost_strictly_increasing`, `predicate_equals_destroy_cost` | VERIFIED (Coq model only) + VECTOR |
| Formally verified monetary invariants (not complete monetary correctness) | Ch.2 | `ResSchedule.v`; assumption boundary | VERIFIED (scoped) |
| ORU no hard cap / tail inflation | Ch.2 | `ConnectBlockMint` + `oru_supply_bounded_by_block_count` | PARTIAL (bound verified; no absolute cap by design) |
| Convergence mint only after Convergence | Ch.2–3 | Selection + mint obligation model; C++ mint path | PARTIAL (`cm_draw_won` abstract) |
| Minimal script / reduced capability | Ch.4–5 | Registry docs; `ScriptShape` lift; SMW/ICVG vectors | PARTIAL (shape VERIFIED; interpreter NOT) |
| Asset separation / dual pool | Security chapters | `DualPoolConservation.v`; NEG vectors; C++ `CheckTxInputs` | VERIFIED (model) + VECTOR + runtime |
| Three-layer FV; do not claim fully proven blockchain | Ch.8 | This matrix; FormalVerification page | DOC (meta) |
| Strongly verified Coq model | Ch.8 | Verdict; `_CoqProject` rebuild; 0 Admitted/Axiom in protocol cone | VERIFIED (model) |
| Correspondence milestone counts | Ch.8 (was **324/117**) | Living lock **422/215** in `golden_lock.json` | **STALE in whitepaper** → correct to 422/215 |
| Differential gate (hash + σ/root CLI) | Ch.8 | `run_differential_gate.sh` | VECTOR |
| `equivalence_claimed = false` | Throughout | Site AppShell fault if true; freeze docs | DOC + enforced in UI |
| PROOF-034 empirical archive ≠ equivalence | Ch.8 | PROOF-034 docs; Experiment page | DOC / NOT executed as proof |
| Self-proving chain coinbase→Merkle→header→PoW | FV architecture prose | C++ ConnectBlock; Coq stops at C/DCR/σ | PARTIAL (impl + DOC; not Coq e2e) |
| No fork / liveness theorems | Implied absences | — | NOT VERIFIED |
| Network / P2P security | — | Node tests only | NOT VERIFIED |
| Wallet security assumptions | — | Wallet docs / tests | NOT VERIFIED |
| Difficulty / PoW hardness (OpuHash) | Mining sections | C++ PoW; not Coq | NOT VERIFIED (FV) |
| SHA-256 collision resistance | Implicit | Executable model + FIPS vectors only | PARTIAL (functional model, not crypto proof) |

## Whitepaper corrections required by this audit

1. Replace living correspondence figures **324 / 117** with **422 / 215** (and note RC0 freeze was 414/207; operator freeze day recorded 324/117 historically).
2. Prefer the allowed claim sentence: *Formally verified protocol invariants with explicit verification boundaries.*
3. Keep Chapter 8’s rejection of “fully verified / formally proven blockchain.”

No consensus, wallet, miner, or RFC file changes accompany this matrix.
