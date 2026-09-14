# Formal Verification — Assumption Boundary

**Document status:** Security audit  
**Historical verification reference:** P0-001  
**Date:** 2026-08-20  
**Honesty flags (must remain false):**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document states what RIGOR’s formal stack **proves**, what it **does not claim**, and where the boundary is enforced in code and docs. It does not change consensus.

Companion: [`FORMAL_VERIFICATION_COVERAGE_AUDIT.md`](./FORMAL_VERIFICATION_COVERAGE_AUDIT.md), [`FORMAL_CLAIM_TRACEABILITY_MATRIX.md`](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md).

---

## 1. Layers (do not collapse)

| Layer | Artifact | What a green result means |
|-------|----------|---------------------------|
| L1 | `formal_verification/spec/rigor_consensus_spec.py` | Executable byte / predicate SSOT for draw, σ, roots, schedule, dual-pool / mint-shape / cap rejects |
| L2 | `formal_verification/coq/` (212 modules in `_CoqProject`) | Machine-checked properties of the **model** |
| L3 | `core/src/**` | Finite differential agreement with L1 (and harness subset with L2); **not** a refinement proof |

There is **no** theorem `C++ = Python = Coq`.

---

## 2. What RIGOR proves (in the model and/or the gate)

### 2.1 Deterministic RES price (RFC-020)

- Height → `destroy_cost(h)` is a pure function (`ResSchedule.v` / Python SSOT / C++ `ResDestroyCostAtHeight`).
- Pins: `destroy_cost(0) = 1024` ORU, height-1 pin, pole predicate, no free mint (`predicate_equals_destroy_cost`, `destroy_cost_strictly_increasing`, `no_free_mint_anywhere`).
- Differential: `tools/res_continuous_price_schedule.py` and `core/src/test/res_issuance_tests.cpp`.

### 2.2 RES supply cap

- Closed form: `(128 / 1024) · 2²⁵ · RES_COIN = 2²²` RES (`cap_is_closed_form`).
- `cumulative_res_at h ≤ RES_MAX_SUPPLY` (`cap_is_upper_bound`).
- Exact-at-cap admitted; one atom over rejected (`res_cap_atom_boundaries`).

### 2.3 Convergence / draw properties (model)

- σ 100-byte layout (no `target_prev`).
- Convergence_Root order-independence; `N < 2` → 32 zeros.
- Selection determinism / order-independence under stated hypotheses; non-vacuous target (`E = 246` ⇒ `C = 1024` ORU mint cost).
- A-9-C window exclusion model (`txid_wins_at_most_once` family).
- `draw_candidate_root` as eligible-list digest sharing the Selection unpaid-eligible pipeline (not coinbase/Merkle/header bind).

### 2.4 Script *shape* restrictions (model)

- Created-output templates, spend-CVG2, optional PCORB field sizes are conjuncts of `apply_tx` / accepted `apply_block` (`ScriptShape.v` / `ScriptShapeLift.v`).
- Abstract CVG2 / Witness form vectors (`InvalidCvg`, `ScriptMintNeg`).

### 2.5 Asset separation (dual pool, model)

- Ordinary txs: RES in ≥ RES out; mint exception is a body-tx destroy that satisfies `CheckResMintArithmetic`.
- Block-level RES net bounded by scheduled mints (`apply_block_res_net_bounded_by_scheduled_mints`).
- Explicit **non-claims**: asset class is `out_asset`, not derived from `out_script`; `rs_res_supply` ≠ `utxo_res` (`OpaqueBoundary.v`).

### 2.6 ORU monetary bound (model)

- Per-block subsidy schedule in `ConnectBlockMint.v`; `oru_supply_bounded_by_block_count` (linear in chain length).
- **No** absolute ORU hard cap (tail emission is intentional).

### 2.7 Replay / reorg structure (model)

- Deterministic supply evolution under `apply_block`; rollback restores prior snapshot; multi-block dual-pool + supply rollback lemmas.
- Not a proof of Bitcoin Core reorg policy or network propagation.

### 2.8 Finite correspondence (gate)

- Living lock: **422** shared golden vectors / **215** Coq-enforced harness lemmas (`golden_lock.json`).
- Tripartite portable hash + production σ/root CLI on gated families.

---

## 3. What RIGOR does **not** claim

| Non-claim | Why |
|-----------|-----|
| Complete implementation equivalence | `equivalence_claimed = false`; Phase-5 records `DCS_Impossible` under current architecture |
| Cryptographic primitive security beyond the executable model | Coq SHA-256 is FIPS-shaped and FIPS-vector checked; not a proof that SHA-256 is collision-resistant in the real world |
| Hardware / side-channel / TEE security | Out of model |
| Network availability, P2P honesty, mempool DoS resistance | Out of Coq; C++ / ops only |
| Real-SHA-256 fairness / grinding cost theorems | `Fairness.v` / `FairnessIdealized.v` are structural or idealized |
| `cm_draw_won` = Selection winner / owner / PCORB | Abstract flag; `we_do_not_claim_cm_draw_won_is_owner_pcorb` |
| `IsResScript` ↔ `out_asset` | Script-vs-tag remains vector / C++ |
| Full script interpreter / P2WSH execution | Shape only in model |
| Coinbase / tx-Merkle / header bind of DCR | Implementation-level |
| Shared production hash binary with Coq | `hash_kernel_shared_binary_with_cpp = false` |
| Absolute ORU cap | Tail inflation by design |
| “Fully mathematically proven blockchain” | Forbidden public wording |

---

## 4. Trust holes checked this audit

| Check | Result |
|-------|--------|
| Syntactic `Axiom` / `Parameter` / `Hypothesis` / `Admitted.` / `admit` in comment-stripped `_CoqProject` modules | **0** |
| Named lemma `res_exact_cap_admitted` | **Qed**; name means “equal-to-cap is allowed,” not Coq `Admitted` |
| Protocol-core result count (non-Phase) | 835 named results (Lemma/Theorem/Corollary) |
| Phase-4F/4G/5 catalog results | 1750 named results — mostly status / impossibility bookkeeping |
| Kernel verification (`coqchk`) | See coverage audit §PART2 (requires consistent `.vo` after `ResSchedule` rebuild) |

---

## 5. Citation rule

Allowed public sentence:

> Formally verified protocol invariants with explicit verification boundaries.

Forbidden:

> Fully mathematically proven blockchain / complete production equivalence.

Authority when prose conflicts with the live RFC: **the RFC wins**.
See [`../formal/SSOT_HIERARCHY.md`](../formal/SSOT_HIERARCHY.md).
