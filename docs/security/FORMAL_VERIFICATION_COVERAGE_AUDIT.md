# Formal Verification Coverage Audit

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-COVERAGE-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Inventory only — **no consensus / wallet / miner / RFC changes.**  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Deliverables:**

| # | Document |
|---|----------|
| 1 | This file |
| 2 | [`FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md`](./FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md) |
| 3 | [`FORMAL_CLAIM_TRACEABILITY_MATRIX.md`](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md) |
| 4 | [`FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md`](./FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md) |
| 5 | [`FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md`](./FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md) |
| Appendix | [`FORMAL_THEOREM_INVENTORY.json`](./FORMAL_THEOREM_INVENTORY.json) (2873 named results; regenerate with `formal_verification/tools/gen_theorem_inventory.py`) |
| Appendix | [`FORMAL_COQ_LOAD_BEARING_THEOREMS.md`](./FORMAL_COQ_LOAD_BEARING_THEOREMS.md) |

Prior living classification: [`../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md).

---

## PART 1 — Verification Coverage Map

Classification:

| Label | Meaning |
|-------|---------|
| **VERIFIED** | Coq theorem(s) about the model, Closed; optionally also vector-checked |
| **PARTIALLY VERIFIED** | Important model lemmas *or* strong vectors, but incomplete vs production surface |
| **DOCUMENTED ONLY** | RFC / whitepaper / security prose without mechanical check of that claim |
| **NOT VERIFIED** | Outside FV boundary |

| Category | Classification | Evidence (primary) | Outside boundary |
|----------|----------------|--------------------|------------------|
| Consensus (draw / σ / C / winner) | **PARTIALLY VERIFIED** | OrderIndependence, Selection, Kernel harness; 422 vectors | Full `validation.cpp` ConnectBlock refinement |
| Asset model (ORU/RES tags) | **PARTIALLY VERIFIED** | Dual-pool + `out_asset`; OpaqueBoundary non-claims | Script-derived asset class |
| UTXO rules | **PARTIALLY VERIFIED** | ReplayInvariants / StateTransition model UTXO | CoinView, maturity flags, full CheckTxInputs |
| Script validation | **PARTIALLY VERIFIED** | ScriptShape on apply_tx/block; SMW/ICVG | Interpreter / P2WSH / IsResScript↔tag |
| Convergence consensus | **VERIFIED** *(model)* | ConvergeRoot, DCR e2e, WindowExclusion, InvalidCvg | Coinbase/Merkle/header bind; owner/PCORB |
| RES monetary policy | **VERIFIED** *(model)* | ResSchedule + obligation/cap; NEG; RFC-017 parity | cm_draw_won ≠ Selection/PCORB |
| ORU monetary policy | **PARTIALLY VERIFIED** | ConnectBlockMint subsidy; oru bound by block count; RFC-018 fee-burn model (`fee-burn/`) | Absolute cap (none by design); ConnectBlock refinement |
| Mining rules | **NOT VERIFIED** | — | Template construction, GBT, solo miner |
| Difficulty rules | **NOT VERIFIED** | — | Retarget / nBits / OpuHash (C++ only) |
| Wallet assumptions | **NOT VERIFIED** | — | Key mgmt, PSBT, address UX |
| Replay/reorg behavior | **PARTIALLY VERIFIED** | Model rollback/supply; ReorgBind scaffolds | Network reorg races; C++ tip logic |
| Network validation | **NOT VERIFIED** | — | P2P, mempool DoS, eclipse |

**Headline:** Monetary schedule/cap and convergence *model* invariants are the strongest VERIFIED cone. Shipping consensus equivalence remains **not claimed**.

---

## PART 2 — Coq Coverage Audit

### Corpus shape

| Slice | `.v` modules (approx.) | Named Theorem/Lemma/Corollary |
|-------|------------------------|-------------------------------|
| Protocol core (non-Phase) | 68 | 1123 |
| Phase-4F / 4G / 5 catalogs | ~158 | 1750 |
| **Total `_CoqProject`** | **221** | **2873** (generated; see appendix) |

Full machine inventory: `FORMAL_THEOREM_INVENTORY.json`.

### Trust-hole scan (comment-stripped sources)

| Pattern | Count |
|---------|------:|
| `Axiom` / `Parameter` / `Hypothesis` / `Variable` / `Conjecture` declarations | 0 |
| `Admitted.` / `admit` | 0 |

Note: `res_exact_cap_admitted` is a **Qed** lemma (equal-to-cap allowed).

### Load-bearing theorems

See [`FORMAL_COQ_LOAD_BEARING_THEOREMS.md`](./FORMAL_COQ_LOAD_BEARING_THEOREMS.md) for name, purpose, assumptions, proof method, and dependency notes. Phase catalogs are mostly status/`True` bookkeeping and impossibility records — not additional monetary proofs.

### `coqchk` / kernel verification

| Run | Modules | Result |
|-----|---------|--------|
| Pre-rebuild | 54 non-Phase `.vo` | **FAIL** — inconsistent assumptions over `Rigor.ResSchedule` (stale dependents after RFC-017 pin lemmas) |
| Post-rebuild | Recorded in §PART2 addendum below after `make -f Makefile.coq` | *(filled after rebuild)* |

`Print Assumptions` on load-bearing RES/selection/replay theorems is re-run after rebuild; expected: **Closed under the global context** (matches prior Verdict methodology).

### Proof-method summary (protocol core)

| Method | Typical use |
|--------|-------------|
| `vm_compute` / `vm_cast_no_check` | Golden harness equality; schedule pins; FIPS hashes |
| Integer / list induction + `lia`/`omega`-style | Cap arithmetic, dual-pool sums |
| Permutation / sorting lemmas | Order independence of C / winner |
| Structural rewriting | apply_tx / apply_block invariants |
| Explicit non-claim theorems | OpaqueBoundary |

---

## PART 3 — Assumption Boundary

See [`FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md`](./FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md).

Maintained: **`equivalence_claimed = false`**.

---

## PART 4 — Golden Vector Coverage

See [`FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md`](./FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md).

Living: **422 / 215**. No vectors added in this task.

---

## PART 5 — Website Review

See [`FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md`](./FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md).

Site updated to living counts and allowed claim wording.

---

## PART 6 — Whitepaper Review

See [`FORMAL_CLAIM_TRACEABILITY_MATRIX.md`](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md).

Ch.8 correspondence figures corrected from stale **324/117** → living **422/215**.

---

## Verdict

RIGOR’s FV stack is a **strong model + finite differential gate** with an honest open refinement gap. RES schedule/cap and convergence model properties are the clearest VERIFIED monetary/consensus invariants. Mining, difficulty, wallet, and network remain **NOT VERIFIED** by Coq. Public wording must stay inside the assumption boundary.

---

## PART2 addendum — rebuild / coqchk

| Step | Result |
|------|--------|
| `make -f Makefile.coq -j$(nproc)` after stale `ResSchedule` dependents | **EXIT 0** (~15 min) |
| Syntactic Axiom/Admitted scan (comment-stripped, 212 modules) | **0** |
| `Print Assumptions` on load-bearing schedule/selection/replay theorems | **Closed under the global context** (12/12 sampled) |
| `coqchk -R . Rigor -o` on **51** non-Phase protocol-core `.vo` (excludes `Kernel/GoldenHarness.vo`, `Kernel/NegativeCoreHarness.vo`, `Extraction.vo` for runtime) | **Modules were successfully checked**; **Axioms: \<none\>**; no type-in-type / unsafe fixpoints |
| Full `coqchk` including GoldenHarness | Not completed in-audit (harness size); harness already typechecked by `coqc` rebuild |

Sampled Closed theorems: `max_issuance_within_cap`, `reward_nonincreasing`, `cost_floor_nondecreasing_along_schedule`, `mint_cost_is_1024_oru`, `convergence_root_order_independent`, `select_winner_order_independent`, `oru_supply_bounded_by_block_count`, `apply_block_res_within_cap`, `winner_must_mint`, `apply_block_res_net_bounded_by_scheduled_mints`, `we_do_not_claim_cm_draw_won_is_owner_pcorb`, `we_do_not_claim_script_derives_asset`.
