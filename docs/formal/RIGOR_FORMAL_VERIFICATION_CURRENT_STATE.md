# RIGOR Formal Verification — Current State

**RC0 freeze:** `TASK-FV-FREEZE-RC0-READINESS-001` (2026-08-20) — citable snapshot; then-current **414 / 207**; honesty unchanged  
**Living densify:** `TASK-FV-VECTOR-SELECTIVE-DENSIFY-001` (2026-08-20) — **422 / 215**; honesty unchanged  
**Release checklist:** `TASK-FV-OPS-REGRESSION-GUARD-001` (2026-08-20) — executable pre-release path; honesty unchanged  
**Prior documentation freeze:** `TASK-FV-DOCS-FREEZE-SYNC-001` — Classification after the model-core closed loop  
**Opaque-boundary declaration:** `TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001` (2026-08-20) — residual abstract surfaces stated as non-claims; honesty unchanged  
**Prior architecture task:** `TASK-RIGOR-FORMAL-VERIFICATION-ARCHITECTURE-ASSESSMENT-P0-001`  
**Layer:** Architecture evaluation (no protocol change)  
**Date:** 2026-08-20  
**Role:** Living Classification table and guaranteed / not-guaranteed boundary for this stage. Use this file for review of *what the model currently is*. Residual opaques: §9. RC0 freeze declaration: §0.  
**Does not supersede:** [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md) on the original theorem inventory, or [`../verification/FORMAL-VERIFICATION-STATUS.md`](../verification/FORMAL-VERIFICATION-STATUS.md) as the operator gate page.

Honesty flags (must stay false until a refinement proof exists):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

## 0. RC0 freeze declaration (TASK-FV-FREEZE-RC0-READINESS-001)

This is the citable RC0 formal-verification freeze. Operator gate page: [`../verification/FORMAL-VERIFICATION-STATUS.md`](../verification/FORMAL-VERIFICATION-STATUS.md). Known L1 vs L2 splits: [`../verification/GOLDEN-VECTOR-DISCREPANCIES.md`](../verification/GOLDEN-VECTOR-DISCREPANCIES.md). Lock: `formal_verification/golden_lock.json` (tag `tightening-replay-txid-2026-08-18`).

| Layer | What RC0 guarantees | What RC0 does not guarantee |
|-------|---------------------|-----------------------------|
| **Model (L2)** | Coq theorems about the *model*: SHA-256d on `list byte`; σ 100-byte layout; Convergence_Root (`N < 2` → zeros) and `draw_candidate_root` (no that shortcut); eligible list / DCR / winner share one pipeline; A-9-C unpaid-list exclusion; RES finite-sum cap + connect-path reject **under abstract `cm_draw_won`**; ORU per-block subsidy bound; block-level dual-pool; H-1 of that flag; finite script *shape* on accepted `apply_block`. Protocol-core cone: no `Axiom` / `Admitted`. | `core/src/**` = Coq; `IsResScript` vs `out_asset`; interpreter / P2WSH; owner / PCORB / Selection as `cm_draw_won`; coinbase / Merkle / header bind of DCR; `rs_res_supply` = `utxo_res`; 100-block maturity; absolute ORU cap; real-SHA-256 fairness; P2P |
| **Differential gate** | Finite agreement on the locked shared set and Coq-enforced lemmas, plus tripartite portable hash and production σ/root CLI, **or the gate fails**. Named surface splits cannot silently disappear. RC0 freeze lock: **414 / 207**. Living lock after `TASK-FV-VECTOR-SELECTIVE-DENSIFY-001`: **422 / 215**. | Universally quantified Coq = Python = C++; SIMD hash; coin-view wrapper; `CheckTransaction` refinement |
| **Honesty** | The three flags stay **false**. A green gate is required to print them false. | Permission to set any flag true |

The 2026-08-18 operator freeze recorded 324 / 117. Pre-`EdgeCross` correspondence was 402 / 195. RC0 FV freeze recorded **414 / 207**. Living lock is **422 / 215**. `_CoqProject` **219**. Verdict theorem inventory (196 / 167) is historical and is not silently rewritten.

---

Claim vocabulary used in this document:

| Word | Meaning |
|------|---------|
| **Proven** | A Coq theorem about the *model*, closed with `Qed`, no `Axiom` / `Admitted` in the protocol-core cone |
| **Modeled** | A Coq definition or predicate exists; it may not be equivalent to C++ |
| **Vector-checked** | Finite golden-vector agreement (Python / Coq / C++ CLI) |
| **Runtime-tested** | C++ unit, functional, fuzz, or harness tests |
| **Implementation-level** | Shipping-node assurance only (no Coq theorem) |
| **Documented** | RFC / security / whitepaper prose; not mechanically checked |
| **Planned** | Roadmap only |

---

## 1. What exists

RIGOR’s formal-verification stack is three layers. They must not be collapsed.

| Layer | Artifact | Role |
|-------|----------|------|
| L1 executable spec | `formal_verification/spec/rigor_consensus_spec.py` | Normative byte semantics for draw / σ / root / schedule; dual-pool / mint-shape / cap reject predicates |
| L2 Coq model | `formal_verification/coq/` (`_CoqProject`: **219** `.v` files) | Machine-checked properties of the *model* |
| L3 production | `core/src/**` | Validated against L1 by golden vectors and CLIs, **not** by proof |

Connections that actually exist:

| Bridge | Strength | Artifact |
|--------|----------|----------|
| L1 ↔ L2 | **215** vectors recomputed inside Coq (`vm_compute`) | `coq/Kernel/GoldenHarness.v` (187) + `coq/Kernel/NegativeCoreHarness.v` (28) |
| L1 ↔ L3 | **422** shared JSON vectors + generated C++ cases | `phase3_golden_vectors.json`, `selection_golden_bridge.inc` |
| Hash L2 ↔ L3 | Finite tripartite check (portable `CSHA256` × hashlib × extracted Coq) | `run_differential_gate.sh` step 3 |
| σ / ConvergenceRoot L1 ↔ L3 | Finite production-function CLI | `cpp_sigma_root_cli` (gate step 4) |
| L2 ↔ L3 refinement | **Not established** | Phase-5 catalogs record `DCS_Impossible` under current architecture. Feasibility (why, and what would have to change): [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) — Path A (finite differential) is the maintained bridge; full identity is architectural, not a missing lemma |

There is no theorem of the form “C++ = Python = Coq”.

---

## 2. Current verified components

### 2.1 Formally modeled (Coq definitions)

| Component | Primary modules | Abstraction |
|-----------|-----------------|-------------|
| SHA-256 / SHA-256d | `RigorSha256.v`, `RigorHash.v`, `CanonicalHashKernel.v` | Executable over `list byte`; not an uninterpreted oracle |
| σ preimage (100 bytes) | `SigmaPoW01.v`, `CanonicalDrawEncoder.v`, `Sigma.v` | `txid ‖ C ‖ B_{h-1} ‖ LE32(h)`; no `target_prev` slot |
| Convergence_Root `C` | `ConvergeRoot.v`, `Kernel/KernelRoot.v` | Sorted concat then SHA-256d; `N < 2` → 32 zero bytes |
| `draw_candidate_root` | `DrawCandidateRoot.v`, `Selection.v` (`eligible_candidates`), `WindowExclusion.v` (windowed / A-9-C form) | Same concat-hash as `C`, **no** `N < 2` shortcut; hashes the inequality-eligible subset of F; winner, eligible list, and root share that list. σ uses Convergence_Root of F, not DCR. **We do not claim** coinbase / tx-Merkle / header bind (`draw_candidate_root_scope_is_eligible_digest`) |
| Abstract CVG2 / invalid Convergence | `InvalidCvg.v` | Witness v1 / 32 / `CVG2` prefix; amount ≥ 1 ORU; exactly one CVG2-positive output. **Not** an interpreter and **not** wired into `apply_tx` |
| Abstract script / PCORB mint-witness shape | `ScriptMintWitness.v`, `ReplayReorgCore/ScriptShape.v` | Witness v0/v1 form, `IsAllowedScript`, `IsResScript`, PCORB 6-item sizes, spend-CVG2. **Not** an interpreter. **Shape** (non-empty created `IsAllowedScript` / spend-CVG2 / optional PCORB sizes) is a conjunct of `apply_tx` and of every accepted `apply_block` (`apply_block_succeeds_implies_script_shape`). **`IsResScript` vs `out_asset` is not.** |
| Draw selection | `Selection.v`, `OrderIndependence.v`, `Execution.v` | Eligibility `σ < T·f`; argmin by cross-product + memcmp tie-break |
| A-9-C window | `Selection.v` (W and T/W), `WindowExclusion.v` (already-paid) | Model of exclusion; not a C++ window-walk refinement |
| RES schedule W(k) | `ResSchedule.v` | Fixed-point `k^{-3/2}` through epoch 250; cutoff is a rule |
| ORU subsidy schedule | `ReplayReorgCore/ConnectBlockMint.v` | 1000 ORU, 3 halvings, then 125 ORU forever |
| RFC-018 RES-transfer ORU fee burn | `fee-burn/*.v` | 50% floor share of RES-transfer ORU fees; circulating ΔORU = subsidy − burn; RES isolated. Not a ConnectBlock refinement |
| Replay / UTXO | `ReplayReorgCore/*` | Outpoints `(tx_id, vout)`; `out_script` unread by pooling; **shape-gated** when non-empty (`ScriptShape.v`); accepted blocks carry the same conjunct (`apply_block_succeeds_implies_script_shape`); explicit `out_asset` tag, **not** derived from script (`we_do_not_claim_script_derives_asset`; `we_do_not_claim_apply_tx_consults_is_res_script`) |
| Dual-pool predicate | `DualPoolConservation.v`, `StateTransition.apply_tx` | `res_in ≥ res_out` and mint-shape exception; **block-level invariant** on `apply_block` / `valid_block_connect` (ordinary net ≤ 0; mint = `W(h)` only). Citable projection: `apply_block_res_net_bounded_by_scheduled_mints`. `rs_res_supply` remains a **scalar independent of** `utxo_res` |
| Winner-must-mint / H-1 | `ResMintObligation.v`, `StateTransition.valid_block_connect` | Abstract `cm_draw_won` — **not** Selection winner, **not** owner/PCORB (`we_do_not_claim_cm_draw_won_is_owner_pcorb`); cap overshoot is fail-closed with H-1; connect-path closed **under that flag** (success iff exactly one mint of `W(h)` under the cap; reject paths exhaustive) |
| Phase-4F/5 catalogs | `Phase4FConsensus/`, `Phase4GRefinement/`, `Phase5Equivalence/` | Status records, including honest *impossibility* of full draw closure |

### 2.2 Mechanically checked (theorems about the model)

Load-bearing results (see Verdict for the original inventory; RFC-017 added schedule theorems):

| Area | Examples | Scope |
|------|----------|--------|
| Hash | `hash256_bytes_length`; FIPS short messages in `CanonicalHashKernel.v` | Bytes in the model |
| σ layout | Offset / width lemmas in `CanonicalDrawEncoder.v` | Preimage shape |
| Root | `converge_root_length`, `converge_root_small`, `converge_root_large`; `draw_candidate_root_length`, `draw_candidate_root_empty_neq_zero`; `draw_candidate_root_is_C_of_eligible_when_large`; `draw_pipeline_e2e` | `C` and eligible-subset root; e2e alignment with winner |
| Order independence | `convergence_root_order_independent`; `draw_and_winner_order_independent`; `select_winner_order_independent` (positive weights, distinct txids) | Same candidate *set* ⇒ same `C`, same `draw_candidate_root`, and same winner |
| Eligibility | `res_probability_target_not_vacuous`; `pre_a6_target_was_vacuous`; E=246 pins `mint_cost_is_1024_oru` | Filter is selective |
| Window | `windowing_preserves_non_vacuity`; `txid_wins_at_most_once`; `draw_candidate_root_exclusion_perm_candidates`; `exclusion_e2e_consistent` | A-9-C model; DCR and winner share unpaid eligible list |
| RES cap | `max_issuance_within_cap` (finite sum); explicit `apply_block` reject on overshoot (`valid_res_cap` / `res_issuance_dual_insurance`); connect-path closed **under abstract `cm_draw_won`** (`winner_positive_W_connect_iff` / `valid_block_connect_reject_classification` / `res_issuance_dual_insurance_connect_path`); `res_mint_respects_cap` (redundant delta clamp); H-1 × cap fail-closed | Model issuance; not owner/PCORB |
| ORU bound | `apply_block_oru_bounded`; `oru_supply_bounded_by_block_count` | Linear in chain length; **no** absolute ORU cap |
| RFC-018 fee burn | `fee_burn_determinism`; `supply_conservation`; `res_isolation`; `reorg_safety`; `replay_safety`; `precision_consistency`; `fee_burn_cannot_enter_circulating_supply`; `cvg2_locked_outputs_excluded_from_circulating`; `wallet_balance_matches_spendable_utxo_model` | Model circulating ORU; ledger scalar still RFC-002. Parity: `test_rfc018_fee_burn_parity.py`; supply-layer: `test_supply_layer_semantics.py` |
| Replay | `supply_deterministic`, `rollback_restores_previous_state`, `utxo_spend_removes_output_distinct_txid`; `apply_block_res_invariant`; `fold_apply_blocks_dual_pool_supply_rollback` | Model UTXO; block-level dual-pool RES net; multi-block rollback restores pre-state |
| Golden harness | 215 `Lemma golden_* : chk_n = true` (187 kernel + 28 NegativeCoreHarness) | Finite L1↔L2 bytes / reject booleans |

Weak / idealized (self-labelled in source):

- `Proofs/Safety.unique_mint` — Execute is a function, **not** supply-level mint uniqueness.
- `Proofs/Fairness.v` — structural preference; **not** a SHA-256 probability theorem.
- `Proofs/FairnessIdealized.v` — idealized uniform digest; **not** a claim about real SHA-256.
- `ReplayReorgCore/OpaqueBoundary.v` — explicit *non-claims*: `out_script` does not classify assets; `cm_draw_won` is not owner/PCORB/Selection; `rs_res_supply` is not `utxo_res`. `DrawCandidateRoot.v` §9: DCR is an eligible-list digest only.

### 2.3 Covered by Coq (enforced vectors)

**215** build-breaking lemmas (`Kernel/GoldenHarness.v` + `Kernel/NegativeCoreHarness.v`), by family:

| Module | Coq-enforced | Shared JSON total |
|--------|--------------|-------------------|
| WindowExclusion | 59 | 59 |
| Inequality | 25 | 25 |
| DrawCandidateRoot | 36 | 36 |
| **NegativeCore** | **16** | **16** |
| **InvalidCvg** | **14** | **14** |
| **ScriptMintNeg** | **12** | **12** |
| **EdgeCross** | **20** | **20** |
| SigmaEncoder | 10 | 10 |
| ConvergenceRoot | 9 | 25 |
| RunSelection | 7 | 84 |
| SigmaPoW01 | 5 | 50 |
| HashKernel | 2 | 29 |
| **Total** | **215** | **422** (includes families with 0 Coq-enforced rows) |

`NegativeCore` (TASK-FV-VECTOR-NEGATIVE-CORE-001, 2026-08-20): dual-pool inflation, mint-shape `W(h)±1` / wrong count, hard-cap overshoot, schedule-exhausted mint, H-1 omit / winner-mismatch, without-winner mint, mixed ordinary+illegal mint bodies. Cheap `vm_compute` (no SHA-256). Python oracle uses the same predicates; mixed-body rows also run the L1 `apply_block_model`. Coq pins the dual-pool / cap / obligation booleans (`hash32_eq_dec` is a `Qed`-sealed `list_eq_dec`, so `apply_block` itself is not `vm_compute`d). Not a C++ refinement. Named L1 vs L2 surface splits (NEG-014/015, SMW-010/011, XFAM-003…005, Z-height obligation/schedule rows, omit-`coq_enforce` kernel ids) are pinned in `golden_lock.json` `surface_splits` and catalogued in [`GOLDEN-VECTOR-DISCREPANCIES.md`](../verification/GOLDEN-VECTOR-DISCREPANCIES.md).

**Count rule.** “Coq-enforced” means harness lemmas that fail the Coq build (187 + 28 = **215**). JSON `coq_enforce: true` is a later explicit flag (190 rows). Original kernel rows (`CR-000`…, `SIG-000`…, `WIN-000`…, `INQ-000`…`003`) omit the key; `gen_coq_harness.py` still includes them because their module is in `kernel_mods`. The 25 omit-key enforced ids are locked. Do not cite 190 as the L1↔L2 lock. Living correspondence is **422 / 215** (414 / 207 was the RC0 freeze; 402 / 195 was pre-`EdgeCross`).

`DrawCandidateRoot` negatives (TASK-FV-VECTOR-DCR-SELECTION-NEGATIVE-001, 2026-08-20): **12** Coq-enforced rows (`DCR-024`…`035`) contrast the 24 positive DCR vectors. They lock candidate-set tamper (insert/delete/replace), eligibility-flag permutation, permutation of a tampered set, duplicate txids (not collapsed), empty/singleton ≠ `N<2` zeros / C, A-9-C pipeline mismatch (full-F root vs excluded winner), and σ-input perturbation (flipped `C`, flipped `B_{h-1}`, flipped `LE32(h)`). Still not refinement; coinbase bind remains C++.

`InvalidCvg` (TASK-FV-VECTOR-INVALID-CONVERGENCE-ROOT-001, 2026-08-20): **14** Coq-enforced rows (`ICVG-000`…`013`) complementary to the DCR family. They lock invalid Convergence-tx *abstract form* (amount &lt; 1 ORU, nValue=0, RES-tagged CVG2, RES2 / P2WPKH script, two CVG2 outputs), committed `C` ≠ recomputed root, unsorted / truncated / extra-byte concat preimages, claimed root length ≠ 32, duplicate txids in F (C of `[a,a]` is not N&lt;2 zeros), and N&lt;2 claims that try to bind the DCR digest as `C`. Abstract CVG2 is Witness v1 / 32 / `CVG2` prefix — **not** a script interpreter and **not** `IsResScript`.

`ScriptMintNeg` (TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001, 2026-08-20): **12** Coq-enforced rows (`SMW-000`…`011`). They lock abstract script / mint-witness *reject* form: Witness v0 claimed as RES (P2WPKH and P2WSH), Witness v1 RES2 claimed as ORU, v1 program length 20, P2PKH / P2SH / witness v2 (`bad-rigor-script-type`), PCORB stack of 5 items and amount-field length 7, spend-CVG2, plus two cross rows with dual-pool / mint-shape (SMW-010 script+ORU→RES; SMW-011 well-shaped `W(h)` mint with a 5-item witness). TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001 (2026-08-20) lifts **shape** rejects (disallowed created templates, spend-CVG2, PCORB sizes) onto `apply_tx`. TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001 (2026-08-20) states the same conjunct on every accepted `apply_block` (`apply_block_succeeds_implies_script_shape`) and records that `apply_tx` does not consult `IsResScript` (`we_do_not_claim_apply_tx_consults_is_res_script`). Script-vs-claimed-asset rows (SMW-000/001/005/010) stay vector-only.

`EdgeCross` (TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001 + TASK-FV-VECTOR-SELECTIVE-DENSIFY-001, 2026-08-20): **20** Coq-enforced rows (`XFAM-000`…`019`). Original 12 combination rejects (script-shape+dual-pool, malformed-`C`+mint/cap, window+DCR+selection, schedule-boundary+mint/cap) plus **8** residual densify rows: P2SH / v1-len20 / nonzero OP_RETURN + inflation; PCORB-five + inflation; window-empty + without-winner mint; window-drop-winner + H-1 omit; first-epoch-last and first-epoch-start + cap overshoot. Script/schedule/PCORB rows pin both conjuncts in `NegativeCoreHarness.v`. Window+obligation rows pin exclusion/DCR/selection **and** the Z-height obligation boolean in `GoldenHarness.v` (not a claim that `cm_draw_won` is the Selection winner).

Layer split for this family:

| Check | L1 (Python) | L2 (Coq) | L3 (C++ / this gate) |
|-------|-------------|----------|----------------------|
| v0/v1 mix, `IsResScript`, `IsAllowedScript`, P2PKH/P2SH/v2 | `eval_script_mint_neg` | `ScriptMintWitness.v` harness | Runtime tests only (not the σ/root CLI) |
| PCORB 6-item field sizes | same | `parse_pcorb_shape` | `ParseConvergeProof` unit tests; not this gate |
| spend-CVG2 | same | `classify_spend_cvg2` | `bad-txns-spend-convergence` unit tests |
| SMW-010 dual-pool ORU→RES | `dual_pool_reject=mint_wrong_amount` | script half only (`not_res_script`); monetary half already L2 via NEG-002 | `CheckTxInputs` |
| SMW-011 mint-shape + bad witness | `mint_shape_would_pass=true` and pcorb fail | pcorb half only (harness); **also** `apply_tx` rejects ill-sized `tx_pcorb` (`ScriptShapeLift`) | `ConnectEvaluateResMintTx` |
| Shape lift (P2PKH/P2SH/v2/v1-len20, spend-CVG2, PCORB sizes) | `valid_script_shape_tx` in `apply_tx_model` | `valid_script_shape_tx` conjunct of `apply_tx` | Not a `CheckTransaction` refinement |
| P2WSH vs native Ed25519; PCORB field *semantics* vs chain; `IsResScript` vs `out_asset` | not modeled as `apply_tx` success | not modeled as `apply_tx` success (vectors only for script-vs-tag) | interpreter / proof bind |

Families with **zero** Coq-enforced rows: `Sha256Fips` (8; locked separately in `CanonicalHashKernel.v`), `ReorgBind` (12), `BlockTrace` (12), `SupplyScaffold` (5), `Replay` (3), `WindowIncomplete` (2), plus the bulk of `RunSelection` / `SigmaPoW01` / `ConvergenceRoot` / `HashKernel`.

### 2.4 Covered by vectors (Python + C++ differential)

Shared set: `formal_verification/phase3_golden_vectors.json` (**422**, tag `tightening-replay-txid-2026-08-18`; +24 `DrawCandidateRoot` through TASK-FV-MODEL-DCR-SELECTION-E2E-001; +16 `NegativeCore` through TASK-FV-VECTOR-NEGATIVE-CORE-001; +12 DCR/Selection negatives through TASK-FV-VECTOR-DCR-SELECTION-NEGATIVE-001; +14 `InvalidCvg` through TASK-FV-VECTOR-INVALID-CONVERGENCE-ROOT-001; +12 `ScriptMintNeg` through TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001; +12 `EdgeCross` through TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001; +8 densify through TASK-FV-VECTOR-SELECTIVE-DENSIFY-001).

| Module | Count | What it pins |
|--------|-------|----------------|
| RunSelection | 84 | Winner index / eligibility under T |
| WindowExclusion | 59 | A-9-C already-paid filter |
| SigmaPoW01 | 50 | σ digest |
| HashKernel | 29 | SHA-256d |
| ConvergenceRoot | 25 | `C`; `N < 2` → zeros |
| DrawCandidateRoot | 36 | 24 positive (eligible-subset root; empty → SHA-256d(`[]`); no `N < 2` shortcut; window/exclusion/e2e) + **12** negative/perturbation (`DCR-024`…`035`: tamper, dup, empty/singleton ≠ zeros / C, A-9-C mismatch, σ C/prev/height flip) |
| Inequality | 25 | `σ < T·f` |
| **NegativeCore** | **16** | Dual-pool / mint-shape / cap / H-1 / mixed-body rejects (`NEG-000`…`015`) |
| **InvalidCvg** | **14** | Invalid Convergence-tx abstract form; committed-`C` mismatch; malformed concat / length; dup txids in F; N&lt;2 claims of DCR as C (`ICVG-000`…`013`) |
| **ScriptMintNeg** | **12** | Script / mint-witness rejects: v0↔v1 mix, nonstandard templates, PCORB shape, spend-CVG2, dual-pool / mint-shape crosses (`SMW-000`…`011`) |
| **EdgeCross** | **20** | Cross-family combination rejects: script-shape+dual-pool, malformed-`C`+mint/cap, window+DCR+selection, schedule-boundary+mint/cap, plus densify (`XFAM-000`…`019`) |
| ReorgBind | 12 | Replay/reorg bind scaffolding |
| BlockTrace | 12 | Structural traces |
| SigmaEncoder | 10 | 100-byte preimage + σ |
| Sha256Fips | 8 | Padding edges (55/56/63/64/65) |
| SupplyScaffold | 5 | RFC-017 schedule vs oracle (no longer doc-only) |
| Replay | 3 | Replay scaffold |
| WindowIncomplete | 2 | Incomplete-window structure |

Gate: `formal_verification/scripts/run_differential_gate.sh` (Python oracle, artifact freshness, golden lock, tripartite hash, σ/root CLI).

### 2.5 Runtime-tested only (not Coq-equivalent)

| Surface | Where | Not |
|---------|-------|-----|
| `ConnectEvaluateResMintTx` / owner / PCORB / domain | C++ + `res_proof_math_tests`, solo-security, fuzz `res_mint_eval` | Composed mint-authorization theorem |
| Dual-pool `CheckTxInputs` | C++ + fuzz `res_checktxinputs` | Coq `apply_block` now carries a block-level RES net invariant (model only; not a C++ refinement) |
| 100-block RES maturity | C++ + `feature_res_maturity.py` | No Coq `nHeight` / mint-flag on coins |
| Script classification (`IsResScript`, Witness v0/v1, spend-CVG2) | C++ + script registry tests; **abstract** predicates in `ScriptMintWitness.v` + 12 vectors; **shape** conjunct on `apply_tx` (`ScriptShape.v`) | Opaque `out_script` for *pooling* (asset class is still `out_asset`). Shape checks (non-empty `IsAllowedScript` / spend-CVG2 / optional PCORB sizes) **are** in the model reject path. Full classification (`IsResScript` vs tag), interpreter / `CheckTransaction` remain C++ |
| Coin-view ConvergenceRoot wrapper | `ComputeConvergenceRootFromConvergenceSet` | Gate uses eligibility-txid pure function only |
| `draw_candidate_root` coinbase / RPC bind | C++ `ComputeConvergenceDrawCandidateRoot`; RPC committed vs computed | **Implementation-level.** Coq models the digest of the eligible list; it does not model inserting that digest into coinbase, the Merkle tx tree, or the header. |
| SIMD SHA-256 | Bitcoin Core `SelfTest` | Portable Transform only in the FV gate |
| Network / mempool / P2P | Node tests | Out of the Coq model |
| PROOF-034 archive | Procedure documented; **live run not executed** | Empirical chain archive, not a proof |

### 2.6 Implementation-level assurance

Shipping-node correctness for anything outside the modeled kernel (script interpreter, validation.cpp ConnectBlock, coinbase commitment layout, Merkle root of the tx tree, header PoW / OpuHash, wallet, miner) rests on C++ tests and review. The Coq corpus does not prove those binaries match the model.

---

## 3. Classification table

| Component | Status | Verification level |
|-----------|--------|--------------------|
| Mathematical specification (draw, σ, C, T, W) | Present (RFC + Python L1 + Coq) | Specified + modeled; L1 is SSOT for bytes |
| Coq model (kernel + monetary + replay) | Strong; axiom-free protocol-core | **Proven** *in the model* |
| Coq Phase-4F/5 catalogs | Machine-checked status records | Modeled bookkeeping; many rows are honest non-claims |
| SHA-256 / SHA-256d | Executable Coq + FIPS + tripartite CLI | Proven (model) + vector-checked (finite) |
| σ validation | Byte layout theorems + 50+10 vectors + production CLI | Proven (layout) + vector-checked |
| Convergence_Root | Order-independence + N&lt;2 + 25 vectors + CLI; **InvalidCvg** locks committed-`C` mismatch, malformed concat/length, dup txids, N&lt;2 DCR-as-C claims | Proven (model) + vector-checked (incl. reject paths) |
| `draw_candidate_root` | `draw_pipeline_e2e` + `execute_e2e_aligned` + `exclusion_e2e_consistent`: eligible list, DCR, and winner share one pipeline; σ uses `C` of F, not DCR. 36 Coq-enforced vectors (24 positive + 12 negative/perturbation). **We do not claim** coinbase / tx-Merkle / header bind (`DrawCandidateRoot.v` §9). | **Modeled + proven properties + Selection/Window e2e**; vector-checked (incl. tamper/dup/σ-perturb); coinbase commitment **implementation-level**; not C++ refinement |
| Draw selection / winner | Determinism, total order under hypotheses, 84 vectors (7 Coq-enforced) | Proven (model) + mostly Python/C++ vectors |
| A-9-C no-double-win | `WindowExclusion.v` + 59 Coq-enforced vectors | Proven (model) + vector-checked; window *walk* is C++ |
| Transaction validation (full `CheckTxInputs`) | C++ | Runtime-tested; Coq UTXO is structural |
| Asset separation (dual pool) | Coq predicate **wired into** `apply_tx` / `apply_block` + C++ dual-pool | **Proven (model) + block-level invariant**; `rs_res_supply` is a **scalar**, not `utxo_res`; not C++ refinement |
| RES mint rules (schedule, cap, H-1) | `ResSchedule.v` + `ResMintObligation.v` + C++ | Proven (schedule/cap/H-1 in model; connect-path closed **under abstract `cm_draw_won`**, not owner/PCORB); owner/PCORB runtime-tested |
| ORU issuance (halving + tail) | `ConnectBlockMint.v` + C++ `GetBlockSubsidy` | Proven (model bound) + implementation-tested |
| RFC-018 RES-transfer ORU fee burn | `fee-burn/` + Python spec + C++ lock | Proven (model) + vector-checked (share/display parity); circulating view; not ConnectBlock refinement |
| **Script classification** | Registry + C++ `IsAllowedScript` / `IsResScript`; abstract form in `InvalidCvg.v` + `ScriptMintWitness.v`; **shape** lift in `ScriptShape.v` | Documented + runtime-tested; **CVG2 container** and **v0/v1 / IsResScript / PCORB shape / spend-CVG2** vector-checked (not an interpreter). **Shape in model:** non-empty created `IsAllowedScript`, spend-CVG2, optional PCORB sizes on `apply_tx` **and** accepted `apply_block`. **Still not in model:** `IsResScript` vs `out_asset`, P2WSH execution, `CheckTransaction` |
| Golden vectors (422 / 215) | Differential gate | Vector-checked; finite; not universal. **16** NegativeCore monetary rejects + **12** DCR/Selection negatives + **14** InvalidCvg Convergence/Root rejects + **12** ScriptMintNeg script/witness rejects + **20** EdgeCross combination/boundary rejects. Intentional L1 vs L2 surface splits: [`GOLDEN-VECTOR-DISCREPANCIES.md`](../verification/GOLDEN-VECTOR-DISCREPANCIES.md) (opaque/`Qed` and compilation-unit splits are **keep**) |
| **Refinement** (Coq ⇄ `core/src/**`) | Explicitly open | **Not proven.** `refinement_proved = false`; Phase-5 `DCS_Impossible` unchanged. Feasibility: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) (architectural; Path A maintained) |
| **Coinbase bind of `draw_candidate_root`** | C++ `ComputeConvergenceDrawCandidateRoot` + ConnectBlock / RPC | **Implementation-level.** Coq hashes the eligible list; it does not insert the digest into coinbase, the tx Merkle tree, or the header |
| Implementation correspondence | Explicitly `equivalence_claimed = false` | **Not proven** |
| Self-proving chain (coinbase → Merkle → header → PoW) | Whitepaper + C++ ConnectBlock | Documented + runtime-tested; Coq stops at C / `draw_candidate_root` / σ / draw |
| PROOF-034 runtime archive | Procedure established | Planned / not executed |
| CLI verification tools | `run_differential_gate.sh`, hash CLI, σ/root CLI | Operational (finite) |
| Fairness / grinding cost | RFC economic argument + idealized Coq | Documented; **not** a real-SHA-256 theorem |
| Network / P2P | Out of model | Implementation-tested only |

---

## 4. Coq corpus shape (maintainability note)

| Slice | `.v` files (approx.) | Role |
|-------|----------------------|------|
| Kernel / root / proofs / ReplayReorgCore / ConnectBlock / fee-burn | ~54 | Load-bearing model |
| Phase-4F / 4G / 5 catalogs | ~158 | Correspondence *status*, including impossibility |

The current `_CoqProject` lists **219** files (`fee-burn/` RFC-018 accounting added 2026-08-20; `CirculatingSemantics.v` supply-layer boundaries added 2026-08-20; `ScriptMintWitness.v` + `ScriptShape.v` / `ScriptShapeLift.v` added 2026-08-20). Protocol-core modules used in this assessment contain **no** `Axiom` / `Admitted` (comment mentions of removed placeholders remain). Phase-5 still contains lemmas labelled `VACUOUS-PLACEHOLDER` that prove `True`; they are status bookkeeping, not evidence (Verdict §3.1).

`formal_verification/README.md` §4 records G-1 as closed in the model (`OrderIndependence.v` / `Permutation`) and G-2 as the residual “no absolute ORU cap” (tail emission), not “unbounded issuance.” README §6 encoder/tie-break lines match `CanonicalDrawEncoder.v` (LE32 height, no `target_prev`; tie-break is txid memcmp). Treat the Verdict + this file as the freeze snapshot.

---

## 5. CLI and operator tools

| Tool | Path | What a green run means |
|------|------|------------------------|
| Differential gate | `formal_verification/scripts/run_differential_gate.sh` | Finite L1/L2/L3 agreement on the frozen vector set; **422/215** pinned by `golden_lock.json` |
| Golden lock | `formal_verification/scripts/check_golden_lock.py` | JSON / harness / lemma counts; NegativeCore 16, InvalidCvg 14, ScriptMintNeg 12, DCR-024…035, EdgeCross 20; named `surface_splits` (NEG-014/015, SMW-010/011, XFAM-003…005, Z-height rows, omit-`coq_enforce` kernel ids) |
| Freeze artifact check | `formal_verification/scripts/check_freeze_artifacts.sh` | Expected files present; honesty false; living 422/215 |
| FV release checklist | `formal_verification/scripts/run_fv_release_checklist.sh` | Freeze artifacts + `SKIP_TEST_BITCOIN=1` gate; see [`../verification/FV-RELEASE-CHECKLIST.md`](../verification/FV-RELEASE-CHECKLIST.md) |
| Hash CLI | `scripts/build_cpp_hash_cli.sh` | Portable `CSHA256` vs hashlib vs extract |
| σ/root CLI | `scripts/build_cpp_sigma_root_cli.sh` | Production `ComputeSigmaHash` + eligibility root vs JSON families |
| Assumption audit | `tools/audit_assumptions.py` | `Print Assumptions` split |
| Vacuity check | `tools/check_vacuous.py` | Detects `True`-bodied placeholders |
| Coq rebuild | `coq_makefile` + `make` | Model + 215 harness lemmas (187 kernel + 28 NegativeCoreHarness) |

A green gate prints the three honesty flags as **false**. That is required, not a failure.

---

## 6. Documentation exposure

| Surface | Content |
|---------|---------|
| Operator freeze | `docs/verification/FORMAL-VERIFICATION-STATUS.md` |
| FV release checklist | `docs/verification/FV-RELEASE-CHECKLIST.md` |
| Model verdict | `docs/verification/FORMAL-VERIFICATION-VERDICT.md` |
| Public site | `/documentation/formal-verification` |
| Whitepaper Ch. 8 | Three-layer architecture; honesty flags |
| This folder | Architecture assessment (this file and siblings); L2↔L3 refinement feasibility |
| Module audits | `docs/formal/rfc017_res_schedule/`, `phase4d_supply/`, `phase4e_connectblock/` |
| Historical PROOF series | `docs/archive/proof/` (PROOF-000 … PROOF-034) |

---

## 7. Maturity (one line)

The **consensus and monetary model** is strongly verified. **Correspondence** is a finite differential gate (422 / 215). **Shipping-binary identity** is not proved. That is the current, honest state.

---

## 8. Freeze snapshot — guaranteed vs not guaranteed (2026-08-20)

Citable one-page declaration: **§0** (`TASK-FV-FREEZE-RC0-READINESS-001`). This section is the expanded boundary.

Honesty flags for this snapshot (required **false**):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Guaranteed (Coq model, `Qed`):** SHA-256d on `list byte`; σ 100-byte layout; Convergence_Root (`N < 2` → zeros) and `draw_candidate_root` (no that shortcut); eligible list / DCR / winner share one pipeline; A-9-C exclusion uses the same unpaid list; RES finite-sum cap + connect-path reject **under abstract `cm_draw_won`**; ORU per-block subsidy bound; block-level dual-pool on `apply_block` (citable: `apply_block_res_net_bounded_by_scheduled_mints`); H-1 winner-must-mint classification **of that flag**; finite script *shape* on every accepted block (`apply_block_succeeds_implies_script_shape`). Finite L1↔L2 lock: **215** harness lemmas. Finite L1↔L3 lock: **422** shared vectors + portable hash / σ / root CLIs.

**Not guaranteed:**

| Boundary | Why it stays open |
|----------|-------------------|
| **Refinement** (`core/src/**` = Coq) | Phase-5 `DCS_Impossible` (unchanged). Architectural: no C++ semantics, no shared production hash, SIMD AutoDetect, coin-view / script VM / owner-PCORB / coinbase bind have no L2 type. Assessment: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md). Finite gate is Path A, not a flag flip |
| **Script classification** | Replay `out_script` is opaque **for pooling**; asset class is an `out_asset` tag (`we_do_not_claim_script_derives_asset`; `we_do_not_claim_apply_tx_consults_is_res_script`). **Shape checks are in `apply_tx` and every accepted `apply_block`:** non-empty created `IsAllowedScript` / zero-value `OP_RETURN`, spend-CVG2, optional PCORB sizes (`ScriptShape.v` / `apply_block_succeeds_implies_script_shape`). Empty script remains a DualPool fixture placeholder (not C++). **Still not in the model:** `IsResScript` vs tag, interpreter, P2WSH execution, `CheckTransaction`, PCORB field semantics |
| **Coinbase / Merkle / header bind of `draw_candidate_root`** | Coq hashes the eligible list (`draw_candidate_root_scope_is_eligible_digest`). Inserting that digest into coinbase, the tx Merkle tree, and the header is **implementation-level** C++ (`ComputeConvergenceDrawCandidateRoot`) |
| Owner / PCORB / domain mint authorization | Abstract `cm_draw_won` is **not** the Selection winner and **not** `ConnectEvaluateResMintTx` (`we_do_not_claim_cm_draw_won_is_owner_pcorb`) |
| Scalar vs UTXO RES | `rs_res_supply` is independent of `utxo_res` (`we_do_not_claim_rs_res_supply_equals_utxo_res`); body apply does not update the scalar; mint-delta does not update UTXO |
| 100-block RES maturity | C++ `Coin::RequiresMaturity` only |
| Coin-view ConvergenceRoot wrapper | Gate checks the eligibility-txid pure function |
| Full script reject families (`IsResScript`, spend-CVG2, P2PKH/P2SH) | P1-1 **partial**: shape rejects (P2PKH/P2SH/v2/v1-len20, spend-CVG2, PCORB sizes) **are** `apply_tx` conjuncts; `IsResScript` vs `out_asset` and P2WSH execution stay C++ / vector-only |
| Absolute ORU supply cap | False by design (125 ORU tail) |
| Real-SHA-256 fairness / grinding cost | Idealized Coq only |
| Network / mempool / P2P | Out of model |

A green `run_differential_gate.sh` does **not** flip any honesty flag.

**Supersedes dated P1 mismatch tables** in `docs/security/RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md` §9 / monetary-policy audit §7.1 for the *model connect path*: over-cap no longer zeros the delta (`valid_res_cap` reject); `valid_dual_pool_tx` is a conjunct of `apply_tx`. Those audits remain valid as dated P0/P1 evaluations. They do not describe the 2026-08-20 Coq connect path. Owner / PCORB / maturity / refinement remain open as stated above.

**Freeze completeness (this snapshot):**

| Item | State |
|------|--------|
| P0-1 dual-pool on `apply_tx` / `apply_block` | **Done** (model) |
| P0-2 cap-overshoot reject + connect-path (abstract `cm_draw_won`) | **Done** (model) |
| P0-3 `draw_candidate_root` + Selection/Window e2e | **Done** (model); coinbase bind C++ |
| P0-4 negative vectors | **Done** for Convergence/Root error paths (`InvalidCvg` + NegativeCore + DCR negatives); script/mint-witness reject form (`ScriptMintNeg`); **shape** wired into `apply_tx` (TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001) and stated on accepted `apply_block` (TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001); cross-family combination/boundary rows (`EdgeCross`); remaining: `IsResScript` vs tag, coinbase bind |
| Classification table (§3) | Aligned with `_CoqProject` **219**, JSON **422**, harness **215** |
| RC0 freeze declaration (§0) | **Declared** (`TASK-FV-FREEZE-RC0-READINESS-001`); then-current 414/207 |
| Selective densify | **Done** (`TASK-FV-VECTOR-SELECTIVE-DENSIFY-001`); living 422/215; honesty false |
| FV release checklist | **Declared** (`TASK-FV-OPS-REGRESSION-GUARD-001`); runner + CI; no new model claim |
| Residual opaque boundaries (§9) | **Declared** (`OpaqueBoundary.v` + `DrawCandidateRoot.v` §9); follow-up non-claims `we_do_not_claim_apply_tx_consults_is_res_script` / `we_do_not_claim_cvg2_script_is_pooling_class`; honesty flags unchanged |
| Honesty flags | **false** / **false** / **false** |

---

## 9. Residual opaque boundaries (TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001)

These surfaces remain abstract **on purpose**. The lemmas below are *non-claims*: they make the gap unskippable. They do not weaken any Closed theorem.

| Surface | What the model is | We do not claim | Machine-checked non-claim |
|---------|-------------------|-----------------|---------------------------|
| `TxOut.out_script` | Opaque `list nat` for *pooling* (`out_asset`); **shape-gated** when non-empty | Script classification as asset; `IsResScript` derives `out_asset` | `we_do_not_claim_script_derives_asset` |
| Abstract CVG2 (`InvalidCvg.v`) | Witness v1 / 32 / `CVG2` prefix predicate + 14 vectors; spend-CVG2 **is** an `apply_tx` conjunct | Interpreter; `IsResScript`; CVG2 as pooling class | `we_do_not_claim_cvg2_script_is_pooling_class`; comment-level in `InvalidCvg.v` header; pooling ignores `out_script` |
| Abstract script / PCORB shape (`ScriptMintWitness.v` + `ScriptShape.v`) | v0/v1 form, `IsAllowedScript`, `IsResScript`, PCORB 6-item sizes, spend-CVG2 + 12 vectors. **In `apply_tx` and accepted `apply_block`:** allowed-template / spend-CVG2 / optional PCORB sizes | Interpreter; P2WSH execution; `ParseConvergeProof` field semantics; `IsResScript` vs `out_asset` | Header non-claim in `ScriptMintWitness.v`; `ScriptShapeLift.allowed_v0_tagged_res_shape_ok`; `we_do_not_claim_apply_tx_consults_is_res_script`; `OpaqueBoundary` pooling still uses the tag |
| `cm_draw_won` | Unconstrained boolean on `ConnectMintSpec` | Selection winner; owner; PCORB; `ConnectEvaluateResMintTx` | `we_do_not_claim_cm_draw_won_determined_by_body`; `we_do_not_claim_cm_draw_won_is_owner_pcorb`; `valid_res_mint_obligation_ignores_body` |
| `draw_candidate_root` | SHA-256d of the eligible-txid concat | Coinbase insert; tx Merkle tree; header bind; Coq = C++ | `draw_candidate_root_scope_is_eligible_digest` (`DrawCandidateRoot.v` §9) |
| `rs_res_supply` | Independent `Z` scalar on `ReplayState` | Equality with `utxo_res`; body apply updates the scalar; mint-delta updates UTXO | `we_do_not_claim_rs_res_supply_equals_utxo_res`; `apply_tx_does_not_update_rs_res_supply`; `apply_connectblock_supply_delta_does_not_update_utxo` |

Honesty flags remain **false**. A green gate does not close any row in this table.

### 9.1 Small-lift follow-ups (TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001)

**Did:** state the existing shape conjunct on accepted `apply_block`; cite dual-pool net as `apply_block_res_net_bounded_by_scheduled_mints`; add the two non-claims above. Closed dual-pool / cap / H-1 theorems were not reopened.

**Did not:** `IsResScript` vs tag on `apply_tx`; interpreter / P2WSH; created-CVG2 min-amount; Selection→`cm_draw_won`; refinement.
