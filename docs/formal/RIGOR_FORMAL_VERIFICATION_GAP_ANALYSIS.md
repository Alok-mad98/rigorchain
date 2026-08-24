# RIGOR Formal Verification — Gap Analysis

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-ARCHITECTURE-ASSESSMENT-P0-001`  
**Layer:** Architecture evaluation (no protocol change)  
**Date:** 2026-08-20  
**Companion:** [`RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](./RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md), [`RIGOR_FORMAL_VERIFICATION_ROADMAP.md`](./RIGOR_FORMAL_VERIFICATION_ROADMAP.md), [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md)

This register lists **verification** improvements. It does not propose consensus, RFC, kernel, wallet, or miner changes.

Priority:

| Class | Meaning |
|-------|---------|
| **P0** | Consensus-critical *verification* strengthening: a gap where a model/C++ mismatch could hide monetary or fork-class error |
| **P1** | Security-boundary strengthening: script, mint authorization, or correspondence coverage that reduces residual risk |
| **P2** | Developer / tooling: CI, catalogs, harness maintainability |
| **P3** | Documentation: stale claims, index, public exposure |

**RC0 freeze (TASK-FV-FREEZE-RC0-READINESS-001, 2026-08-20).** Living counts **414** shared / **207** Coq-enforced. Citable declaration: [`RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](./RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) §0.
P0-1 / P0-2 / P0-3 closed in the model; P0-4 done for Convergence/Root error paths (`InvalidCvg`); remaining script families are P1-1; P3-1 README alignment closed (including encoder / tie-break); P3-6 residual opaque boundaries declared (`OpaqueBoundary.v`).
**Refinement feasibility (TASK-FV-ARCHITECTURE-REFINEMENT-FEASIBILITY-001, 2026-08-20):** full L2↔L3 identity remains **not funded**; see [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md). Phase-5 `DCS_Impossible` is unchanged.
Honesty flags remain **false**.

Items already **closed in the model** (do not re-open as gaps): G-1 order independence (`OrderIndependence.v`); G-2 ORU linear bound + RES finite-sum cap; A-6 non-vacuity; A-9-C window width and already-paid model; RFC-017 schedule pins; dual-pool on `apply_tx` / `apply_block`; cap-overshoot reject; `draw_candidate_root` e2e (coinbase bind still C++).

Honesty: none of the rows below, if completed, would by themselves flip `equivalence_claimed` or `refinement_proved`.

---

## P0 — Consensus-critical verification strengthening

### P0-1. Wire dual-pool conservation into the connect path — **done** (TASK-FV-MODEL-DUALPOOL-APPLY-TX-001 / HARDENING-001 / BLOCK-LEVEL-INVARIANT-001, 2026-08-20)

**Today:** Dual-pool has reached **block-level invariant**. `valid_dual_pool_tx` is a conjunct of `apply_tx`. After successful `apply_block` / `valid_block_connect`, body RES net is ordinary slack (≤ 0) plus `n` scheduled mints of `W(h)` (`apply_block_res_invariant`). Citable projection: `apply_block_res_net_bounded_by_scheduled_mints` (net ≤ n·W(h); n=0 ⇒ net ≤ 0). Multi-block joint: `fold_apply_blocks_dual_pool_supply_rollback` (determinism + rollback of the whole suffix). Honesty flags unchanged. Residual: not a C++ refinement; `rs_res_supply` remains a scalar independent of `utxo_res` (`OpaqueBoundary.we_do_not_claim_rs_res_supply_equals_utxo_res`).

**Why P0:** “Ordinary transfers cannot create RES” is a monetary invariant. It is now an inductive property of every accepted block *in the model*.

**Work:** Completed in Coq only. Keep `equivalence_claimed = false`. Do not change C++.

### P0-2. Align cap-overshoot behaviour with production — **done** (ASK-FV-MODEL-CAP-OVERSHOOT-REJECT-001 / TASK-FV-MODEL-CAP-BOUNDARY-AND-H1-001 / TASK-FV-MODEL-CAP-H1-FULL-CONNECT-PATH-001, 2026-08-20)

**Now:** `valid_res_cap` is a conjunct of `valid_block_connect`. Overshoot is `apply_block = None` (`bad-rigor-res-max-supply`), not a zeroed delta. Exact cap is admitted; one atom over is rejected. H-1 × cap is fail-closed (`winner_cap_overshoot_fail_closed` and combinatorial include/omit cases). After `W(h)=0` a mint is the unauthorized-amount path, never winner-mismatch. Dual insurance is `res_issuance_dual_insurance` (finite sum + runtime reject). **Full connect-path closed:** `winner_positive_W_connect_iff` (winner + `W(h)>0` connects iff header/chainwork/ORU hold, exactly one mint of `W(h)`, and no overshoot); `valid_block_connect_reject_classification` (header / chainwork / ORU / winner-mismatch / unauthorized / without-winner / cap); `at_hard_cap_next_winner_mint_rejected`; epoch-boundary lemmas; body wrong-amount is mint-shape (`res_mint_shape_rejects_wrong_amount`). Top-level bundle: `res_issuance_dual_insurance_connect_path`. `res_mint_respects_cap` remains the redundant clamp. Still not refinement; honesty flags unchanged.

**Was:** If a mint would exceed 21M RES, the Coq model **zeroed the delta**. Production **rejects**.

### P0-3. Model `draw_candidate_root` as a Coq function — **done** (TASK-FV-MODEL-DRAW-CANDIDATE-ROOT-001, 2026-08-20)

**Now:** `DrawCandidateRoot.v` defines `draw_candidate_root_from_flags` matching `HashConcatenatedSortedTxids` / Python `convergence_draw_eligible_root` (no N&lt;2 collapse). E2E (TASK-FV-MODEL-DCR-SELECTION-E2E-001): `draw_pipeline_e2e` aligns eligible list, DCR, and winner; Execute and A-9-C exclusion use the same hashed list; σ’s C slot is Convergence_Root of F, not DCR. **We do not claim** coinbase / tx-Merkle / header bind (`draw_candidate_root_scope_is_eligible_digest`, TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001). 36 Coq-enforced golden rows (`DCR-000`..`023` positive; `DCR-024`..`035` negative/perturbation). Still not refinement.

**Was:** Convergence_Root `C` is proved (including `N < 2` → zeros). The **eligible-subset** root used in coinbase existed only in Python/C++.

### P0-4. Invalid / malformed consensus vectors — **done** for Convergence/Root error paths (TASK-FV-VECTOR-NEGATIVE-CORE-001 + TASK-FV-VECTOR-DCR-SELECTION-NEGATIVE-001 + TASK-FV-VECTOR-INVALID-CONVERGENCE-ROOT-001, 2026-08-20)

**Today:** Monetary reject paths are a Coq-enforced family. **16** `NegativeCore` rows (`NEG-000`…`015`) cover dual-pool inflation (multi-out / +1 / ORU→RES / RES→ORU), mint-shape `W(h)±1` and wrong output count, genesis-W at epoch 1, hard-cap one-atom overshoot and mint-at-cap, schedule-exhausted mint, H-1 omit / mint-count=2, without-winner mint, and mixed ordinary+illegal mint bodies.

DCR/Selection negatives are also Coq-enforced. **12** rows (`DCR-024`…`035`) contrast the 24 positive DCR vectors: candidate-set tamper (insert/delete/replace), eligibility-flag permutation, permutation of a tampered set, duplicate txids (concat keeps both copies), empty/singleton DCR ≠ `N<2` zeros / C, A-9-C full-F root vs excluded-set winner mismatch, and σ-input perturbation (flipped `C`, flipped `B_{h-1}`, flipped `LE32(h)`).

`InvalidCvg` (**14** rows, `ICVG-000`…`013`) is the complementary Convergence/Root reject family: invalid Convergence-tx abstract form (amount &lt; 1 ORU, nValue=0, RES-tagged CVG2, RES2 / P2WPKH script, two CVG2 outputs), committed `C` ≠ recomputed, unsorted / truncated / extra-byte concat, claimed root length ≠ 32, duplicate txids in F (`C([a,a])` is not N&lt;2 zeros), and N&lt;2 claims that bind the DCR digest as `C`. Python oracle + `Kernel/GoldenHarness.v` + `InvalidCvg.v`. Shared JSON **376 → 390**; Coq-enforced **169 → 183**.

`ScriptMintNeg` (**12** rows, `SMW-000`…`011`, TASK-FV-VECTOR-SCRIPT-AND-MINT-WITNESS-NEG-001): abstract script / mint-witness rejects. Shared JSON **390 → 402**; Coq-enforced **183 → 195**.

`EdgeCross` (**20** rows, `XFAM-000`…`019`, TASK-FV-VECTOR-EDGE-AND-CROSS-FAMILY-001 + TASK-FV-VECTOR-SELECTIVE-DENSIFY-001): combination rejects (script-shape+dual-pool, malformed-`C`+mint/cap, window+DCR+selection, schedule-boundary+mint/cap) plus eight residual densify rows (P2SH/v1-len20/OP_RETURN/PCORB × inflation; window × obligation; first-epoch boundary × cap). Shared JSON **414 → 422**; Coq-enforced **207 → 215**.

Remaining (moved to P1-1 remainder / implementation-level, not this P0 row):

- `IsResScript` vs `out_asset` (shape checks are now on `apply_tx`; asset class is still the tag)
- Inserting committed `C` / `draw_candidate_root` into coinbase / Merkle / header (C++)

**Why P0:** Differential gates that only replay happy paths do not lock reject reasons. Consensus safety is mostly “invalid blocks stay invalid.” The Convergence/Root error path is now a Coq-enforced family.

**Work:** Completed for this row. Do not expand Coq SHA-256-heavy families without a task (freeze policy).

---

## P1 — Security-boundary strengthening

### P1-1. Script classification as Coq predicates (no interpreter)

**Today:** Script rules are specified in `docs/security/RIGOR_SCRIPT_TYPE_REGISTRY.md` and enforced in C++. Replay pooling still uses an explicit `out_asset` tag (`we_do_not_claim_script_derives_asset`; `we_do_not_claim_apply_tx_consults_is_res_script`). **Abstract CVG2 container** (Witness v1 / 32 / `CVG2` prefix) is a Coq predicate (`InvalidCvg.v`) with 14 reject vectors (`we_do_not_claim_cvg2_script_is_pooling_class`). **Abstract script / PCORB shape** (`ScriptMintWitness.v`) adds 12 `ScriptMintNeg` reject vectors (`SMW-000`…`011`). TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001 (2026-08-20) lifts a **finite subset** onto `apply_tx` (`ScriptShape.v` / `ScriptShapeLift.v`): non-empty created scripts must be `IsAllowedScript` or zero-value `OP_RETURN`; spent CVG2 is rejected; `tx_pcorb = Some stack` must parse PCORB field *sizes*. TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001 (2026-08-20) states that conjunct on every accepted `apply_block` (`apply_block_succeeds_implies_script_shape`) without changing `apply_tx`. Empty `out_script` remains the DualPool fixture placeholder (not C++). **Not lifted:** `IsResScript` vs `out_asset`, P2WSH execution, PCORB field semantics, created-CVG2 min-amount / Convergence-tx class, Selection winner into `cm_draw_won`.

**Work remaining:** Do **not** model the full script interpreter or P2WSH execution in this increment (P2WSH is the largest inherited surface; RC0 keeps it). PCORB field *semantics* (txid / root vs chain) stay C++ (`ParseConvergeProof` / `ConnectEvaluateResMintTx`). Optional later: reject created outputs whose script class disagrees with `out_asset` — that would claim script derives asset and is out of this task.

### P1-2. Compose draw kernel with mint authorization

**Today:** Connect-path classification with abstract `cm_draw_won` is **closed** (`winner_positive_W_connect_iff`: winner + `W(h)>0` ⇒ exactly one mint of scheduled `W(h)` under the cap). That “winner” is the unconstrained boolean, **not** the Selection kernel and **not** owner/PCORB (`we_do_not_claim_cm_draw_won_is_owner_pcorb`; `valid_res_mint_obligation_ignores_body`). Draw/σ/PCORB hashes remain separate theories. Owner / PCORB / domain stay C++ (`ConnectEvaluateResMintTx`).

**Work:** Wire the model’s Selection winner (`Some w`) into `cm_draw_won`. Owner encoding remains C++ unless a later task funds it. Prior evaluation already recommended **not** a full mint-pipeline Coq theory this cycle.

### P1-3. 100-block RES maturity

**Today:** C++ `Coin::RequiresMaturity`; Coq `TxOut` has no height / mint flag.

**Work:** Optional Coq field + spend-rejection lemma. Lower value than P0-1 / P1-1 for fork risk; still a security-boundary invariant.

### P1-4. Expand Coq-enforced share of RunSelection / SigmaPoW01 / ConvergenceRoot

**Today:** 7 / 84 RunSelection, 5 / 50 SigmaPoW01, 9 / 25 ConvergenceRoot are Coq-enforced. SHA-256 `vm_compute` cost is the reason (documented split).

**Work:** Enforce a **stratified** extra subset (boundary heights, N=0/1/2, duplicate-adjacent sort, empty eligible set) rather than all 207 Python-only rows.

### P1-5. Coin-view wrapper residual

**Today:** Gate step 4 checks `ComputeConvergenceRootFromEligibilityTxids`, not `ComputeConvergenceRootFromConvergenceSet`.

**Work:** Test-only cases that the coin-view wrapper agrees with the pure function on the same eligibility list. Still finite; still not refinement.

---

## P2 — Developer / tooling

### P2-1. Continuous `Print Assumptions` in CI

`tools/audit_assumptions.py` exists. Make a green Coq + assumptions audit a required check on FV-touching PRs.

**Partial (2026-08-20, `TASK-FV-OPS-REGRESSION-GUARD-001`):** CI runs freeze artifacts plus the differential gate (`SKIP_TEST_BITCOIN=1`) via `run_fv_release_checklist.sh`. Coq 8.15 harness rebuild and `audit_assumptions.py` remain **local** (`FV_COQ=1`); Ubuntu `coq` is not pinned to 8.15.0. Checklist: [`../verification/FV-RELEASE-CHECKLIST.md`](../verification/FV-RELEASE-CHECKLIST.md).

### P2-2. Catalog debt

`Phase5Equivalence/` (~89 files) records that full equivalence is **not** closed, including `True`-bodied placeholders labelled as such. That honesty is valuable. The volume is expensive to maintain and easy to misread as evidence.

**Work:** Freeze the catalog; do not add sibling Catalog/Audit pairs without a task. Optionally collapse documentation of terminal status to Verdict + STATUS (already the public sources of truth).

### P2-3. PROOF-034 live archive

Procedure established; **run not executed** (`docs/archive/proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md`). Heights 630 000 / 2 100 000 / 2 100 001 remain unmaterialized on a live regtest archive.

This is empirical, not a proof. Classify P2 (tooling/archive) unless a release gate names `proof034_complete`.

### P2-4. Stale generated-data guards

RFC-017 already hit a frozen `ConsensusGoldenVectorsData.v` vs grown JSON. Keep `generate_phase4f_golden_traces.py` in the differential gate’s freshness step (or an explicit check that registry counts cannot silently diverge).

### P2-5. Differential fuzzer honesty

`differential_fuzzer.py` reports **INCONCLUSIVE** without an independent oracle. Document that in operator freeze so it is never cited as PASS.

### P2-6. Optional Coq extract of encoder/root

STATUS defers this. If done, it is a second extracted kernel (like hash), still not a shared `rigord` binary, still not refinement.

---

## P3 — Documentation

### P3-1. `formal_verification/README.md` alignment — **done** (TASK-FV-DOCS-FREEZE-SYNC-001, 2026-08-20)

README §4 records G-1 as closed in the model (`OrderIndependence.v`) and G-2 as “no *absolute* ORU cap” (125 ORU tail), with RES finite-sum + connect reject. File count **212** (`ScriptMintWitness.v`, `ScriptShape.v`, `ScriptShapeLift.v`). §6 encoder/tie-break/inequality lines now match `CanonicalDrawEncoder.v` / `Selection.v` (LE32 height, no `target_prev`; txid memcmp tie-break, not XOR; `σ < T · f`). Remaining P3: Verdict theorem inventory (do not silently rewrite), RPC “Merkle” copy, public-site wording.

### P3-2. Verdict module counts

Verdict cites 196 modules / 167 theorems. `_CoqProject` now has 212 files (RFC-017, P1 model increments, `InvalidCvg.v`, `OpaqueBoundary.v`, `ScriptMintWitness.v`, `ScriptShape.v`, `ScriptShapeLift.v`). Add a dated “corpus grew; recount on next verdict revision” note — **do not** silently update the Verdict’s theorem inventory in this task.

### P3-3. RPC “Merkle root” copy vs Convergence_Root

Whitepaper already records that Convergence_Root and `draw_candidate_root` are **not** Merkle trees. RPC help still says “Merkle” in places. Documentation-only alignment (not this task’s RFC freeze).

### P3-4. Public site

`/documentation/formal-verification` should keep honesty flags and, when operators choose, link this architecture assessment. No marketing language (“fully verified chain”).

### P3-5. Index — **done** (TASK-FV-DOCS-FREEZE-SYNC-001, 2026-08-20)

This folder’s `README.md`, `docs/README.md`, and `docs/verification/README.md` list the architecture assessment. Freeze STATUS remains the operator page; `RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` is the Classification freeze snapshot.

### P3-6. Residual opaque-boundary declaration — **done** (TASK-FV-MODEL-RESIDUAL-OPAQUE-BOUNDARY-001, 2026-08-20)

**Today:** `ReplayReorgCore/OpaqueBoundary.v` and `DrawCandidateRoot.v` §9 state the remaining abstract surfaces as machine-checked *non-claims*: `out_script` does not classify assets; `cm_draw_won` is not owner/PCORB/Selection; `rs_res_supply` is not `utxo_res`; DCR is an eligible-list digest only (no coinbase / Merkle / header bind). TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001 adds `we_do_not_claim_apply_tx_consults_is_res_script` and `we_do_not_claim_cvg2_script_is_pooling_class`. Classification table and CURRENT_STATE §8/§9 match the code. Closed theorems were not reopened. Honesty flags remain **false**.

---

## Explicit non-gaps (do not fund)

| Item | Why not |
|------|---------|
| Full Coq ⇄ C++ refinement of `rigord` | **Assessed** (`TASK-FV-ARCHITECTURE-REFINEMENT-FEASIBILITY-001`): architectural, not a missing lemma. Phase-5 `draw_closure_status = DCS_Impossible` stays; encoder/SHA-256d *artifacts* exist (`DrawBridgeProgress.v`) but catalogs were not flipped. No shared production hash; SIMD AutoDetect; coin-view / script VM / owner-PCORB / coinbase bind have no L2 type. See [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) |
| Mechanized local refinement (VST/CompCert of portable SHA-256 / σ / concat-root) | Theoretically stateable for a few pure functions; cost dominates residual risk given 414/207 + CLIs. Not a path to `refinement_proved`. Feasibility Path B2: **do not fund this cycle** |
| Shared extracted hash inside `rigord` | Rejected: extract is OCaml; node uses `SHA256AutoDetect` ([SHARED-HASH-FEASIBILITY](../verification/SHARED-HASH-FEASIBILITY-2026-08.md)) |
| Probabilistic fairness of real SHA-256 | Needs a probability model the corpus does not have |
| Network / mempool Coq | Out of monetary/consensus-kernel scope |
| Full script VM / P2WSH execution in Coq | Cost dominates; C++ + opcode policy is the live gate |
| Flipping honesty flags because the gate is green | A green finite gate is not equivalence |
| Flipping `DCS_Impossible` because encoder/hash files exist | Catalog bits are freeze-era evidence flags, not “file exists.” Conclusion (full closure not reached) remains correct |

Optional **finite** densification (Gap P1-4, P1-5, optional SIMD-vs-portable hash CLI) is Path B1 in the feasibility note. It is ordinary correspondence work. It is **not** refinement and does not leave this non-gap table.

---

## Refinement (assessed, not a P0–P3 work item)

Full L2↔L3 refinement is **not** a gap to close in this register. The feasibility note partitions:

| Path | Status in this register |
|------|-------------------------|
| A — keep finite differential | **Maintained** (Phase 1 / 4); honesty flags false |
| B1 — denser finite / coin-view / optional SIMD CLI | Already P1-4 / P1-5 / hash feasibility note |
| B2 — mechanized Coq = C++ for pure functions | Non-gap this cycle |
| C — architecture replacement (extracted node, verified C++, ConnectBlock embedding) | Non-gap; separate charter if ever reopened |

Conditions under which a refinement *program* could be reopened are listed in the feasibility note §6. Completing P1-1 / P1-2 does **not** meet those conditions.

---

## Mapping to later roadmap phases

| Gap | Roadmap |
|-----|---------|
| P3 docs freeze, gate green, honesty flags | Phase 1 (RC0 completion) — **RC0 FV freeze declared** 2026-08-20 (`TASK-FV-FREEZE-RC0-READINESS-001`; 414/207) |
| P0-1, P0-2 (**done**), P0-3 (**done**), P1-1, P1-2 | Phase 2 (proof coverage) |
| P0-4 (**done** for Convergence/Root rejects; P1-1 script remainder), P1-4, P1-5 | Phase 3 (correspondence) |
| P2-1 (**partial**: checklist + CI subset, `TASK-FV-OPS-REGRESSION-GUARD-001`), P2-3, P2-4 (gate freshness) | Phase 4 (continuous infrastructure) |
| Full L2↔L3 refinement / Path B2 / Path C | **Not a roadmap phase.** Feasibility: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) |
