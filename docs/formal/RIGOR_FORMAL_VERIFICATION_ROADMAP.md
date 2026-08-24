# RIGOR Formal Verification — Recommended Roadmap

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-ARCHITECTURE-ASSESSMENT-P0-001`  
**Layer:** Planning only. No consensus, RFC, kernel, wallet, or miner changes.  
**Date:** 2026-08-20  
**Gaps:** [`RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md`](./RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md)  
**Refinement:** [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md)

Honesty flags remain **false** in every phase unless a future task produces an actual refinement proof (not planned here).

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Refinement feasibility (TASK-FV-ARCHITECTURE-REFINEMENT-FEASIBILITY-001, 2026-08-20):** full L2↔L3 identity is **not a phase of this roadmap**. Assessment: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md). Phase-5 `DCS_Impossible` is unchanged.

---

## Phase 1 — Current RC0 completion

**Goal:** Treat the 2026-08-18 operator freeze plus RFC-017 model pins as the scope baseline, the 2026-08-20 model-core closed loop as the living Classification, and `TASK-FV-FREEZE-RC0-READINESS-001` as the citable RC0 FV freeze. Do not expand scope without a new task.

| Item | Done when |
|------|-----------|
| Differential gate green | `run_fv_release_checklist.sh` / `run_differential_gate.sh` PASS; honesty flags printed false |
| Coq corpus builds | `_CoqProject` rebuild; **207** harness lemmas hold; residual opaques in `OpaqueBoundary.v` |
| RFC-017 schedule in Coq | `ResSchedule.max_issuance_within_cap` and E=246 pins remain |
| Operator docs | STATUS / FREEZE / Verdict remain canonical; pre-release path is [FV-RELEASE-CHECKLIST.md](../verification/FV-RELEASE-CHECKLIST.md) |
| Architecture freeze snapshot | `docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` §0 declaration + Classification + §8 boundary |
| No protocol work | Consensus / kernel / validation / wallet / miner untouched |

Living correspondence counts (2026-08-20): **422** shared / **215** Coq-enforced. RC0 FV freeze recorded 414 / 207. The 2026-08-18 operator freeze recorded 324 / 117. **Phase 1 complete** for the FV freeze itself. Selective densify (`TASK-FV-VECTOR-SELECTIVE-DENSIFY-001`) is a Phase-3 correspondence increment.

**Out of Phase 1:** new vector campaigns, new Coq theories, new CLIs, extracted encoder/root, SIMD extra gate, PROOF-034 live mine to 2.1M, flipping flags.

**RC0 verdict for FV:** sufficient as a **strongly verified model + finite correspondence + honest non-claims**. Not sufficient as “the shipping binary is a theorem.”

---

## Phase 2 — Increase proof coverage

**Goal:** Close the highest-value **model** holes that affect monetary and fork-class statements. Still model-only.

Priority order:

1. **P0-1** — Dual-pool predicate on `apply_tx` / `apply_block` (ordinary RES cannot inflate). **Done** 2026-08-20 through block-level invariant (`apply_block_res_invariant`; multi-block rollback/determinism joint). Still not refinement.
2. **P0-2** — Cap overshoot: model *rejects* instead of zeroing delta. **Done** 2026-08-20 (`valid_res_cap` on `apply_block`; `res_issuance_dual_insurance`; H-1 fail-closed; connect-path closed **under abstract `cm_draw_won`** via `res_issuance_dual_insurance_connect_path`). Still not refinement.
3. **P0-3** — Coq `draw_candidate_root` (eligible-subset hash; no N&lt;2 shortcut). **Done** 2026-08-20 (`DrawCandidateRoot.v`; 36 harness rows: 24 positive + 12 negative/perturbation; Selection/Window e2e). Coinbase bind remains C++. Still not refinement.
4. **P1-1** — Script classification predicates (CVG2 / RES / reject), not a VM. **Partial** 2026-08-20: vectors (`ScriptMintWitness.v` + 12 `ScriptMintNeg`) plus **shape lift** onto `apply_tx` (`ScriptShape.v`) and accepted `apply_block` (`apply_block_succeeds_implies_script_shape`). Remaining: `IsResScript` vs `out_asset`, interpreter, P2WSH.
5. **P1-2** — Wire Selection winner into `cm_draw_won` (connect-path classification with abstract winner is already closed). Owner/PCORB remain C++. The abstract-flag gap is now a non-claim lemma (`we_do_not_claim_cm_draw_won_is_owner_pcorb`).
6. **P1-3** — Optional: 100-block maturity on model coins.

Exit criteria:

- New theorems named in module headers with explicit “not C++ refinement” comments.
- `Print Assumptions` closed on the new cone.
- Freeze STATUS updated only as a dated addendum; honesty flags unchanged.

---

## Phase 3 — Expand correspondence proofs

**Goal:** Make L1↔L2↔L3 agreement denser on **reject paths** and on the two roots. Still finite. Still not refinement.

1. **P0-4** — **Done** for Convergence/Root error paths. Monetary/mint/cap/H-1 rejects (`NegativeCore`, 16). DCR/Selection negatives (`DCR-024`…`035`). `InvalidCvg` (`ICVG-000`…`013`). Script *shape* is on `apply_tx` and accepted `apply_block` (P1-1 partial). Residual crosses densified (`EdgeCross` 20, TASK-FV-VECTOR-SELECTIVE-DENSIFY-001). Remaining: `IsResScript` vs tag; coinbase bind of C/DCR is C++.
2. **P1-4** — Stratified extra Coq-enforced rows (N=0/1/2, empty eligible set, sort edges). Respect `vm_compute` cost.
3. **P1-5** — Coin-view wrapper vs pure eligibility-txid function (test-only).
4. Optional **P2-6** — Extract σ/root Coq kernel the same way hash is extracted. Test driver only. Do **not** link into `rigord`.

Exit criteria:

- Shared JSON count may grow; Coq-enforced count grows only for cheap families.
- Gate still prints honesty flags false.
- `GOLDEN-VECTOR-DISCREPANCIES.md` updated for any new intentional split.

Non-goal: mechanized `Coq function = C++ function` for ConnectBlock.

---

## Phase 4 — Continuous verification infrastructure

**Goal:** Make the existing boundary **hard to regress**, not a larger claim.

1. **P2-1** — CI: Coq rebuild + `audit_assumptions.py` + differential gate (or a documented subset) on FV-touching changes. **Partial** 2026-08-20 (`TASK-FV-OPS-REGRESSION-GUARD-001`): executable [FV-RELEASE-CHECKLIST.md](../verification/FV-RELEASE-CHECKLIST.md) + CI runs freeze artifacts and gate steps 1–4 (`SKIP_TEST_BITCOIN=1`). Coq 8.15 harness rebuild and `audit_assumptions.py` stay **local** (`FV_COQ=1`).
2. **P2-4** — Generated `ConsensusGoldenVectorsData.v` / harness / bridge header freshness (already in the gate; checklist documents when to regen vs bump `golden_lock.json`).
3. **P2-3** — If operators fund it: execute PROOF-034 archive procedure; record `proof034_complete` honestly. Empirical only.
4. **P2-2** — Stop growing Phase-5 Catalog/Audit pairs; keep Verdict + STATUS as human-readable truth.
5. **P3** — README G-1/G-2/encoder/tie-break aligned under TASK-FV-DOCS-FREEZE-SYNC-001. Remaining: Verdict theorem recount (do not silently rewrite), optional site link to this assessment.

Exit criteria:

- A broken golden vector or a new `Axiom` in the protocol cone fails CI.
- Catalogs cannot silently drift from JSON counts.
- Public language still: model verified; correspondence finite; equivalence not claimed.

---

## What this roadmap will not produce

Even after Phases 2–4:

- No proof that `core/src/**` is a refinement of `formal_verification/coq/**`.
- No shared production hash binary.
- No grinding-cost theorem about real SHA-256.
- No Coq model of P2P.

Those would require a **different architecture** (shared specification extracted into the node, or a verified compilation pipeline). That is a separate program, not a continuation of RC0 maintenance.

### Refinement paths (evaluated, not scheduled)

[`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) partitions L2↔L3 work. This roadmap **does not add a Phase 5 “do refinement.”**

| Path | Meaning | This roadmap |
|------|---------|--------------|
| **A** | Keep finite differential (422 / 215 + CLIs); honesty flags false; `DCS_Impossible` stays | **This is Phases 1–4** |
| **B1** | Denser finite correspondence (P1-4 / P1-5; optional SIMD-vs-portable hash CLI) | Optional inside Phase 3; **not** `refinement_proved` |
| **B2** | Mechanized Coq = C++ for a few pure functions (VST/CompCert-class) | **Not funded** this cycle (cost ≫ residual vs the gate) |
| **C** | Architecture replacement so full `rigord` refinement is even stateable | **Not this program** |

`DCS_Impossible` remains the Phase-5 catalog conclusion for *full* draw/node equivalence. Encoder and SHA-256d **artifacts** exist (`DrawBridgeProgress.v`); that does not reopen Path C or flip flags.

Reopen a refinement program only under the conditions in the feasibility note §6 (C semantics in TCB; production-hash claim scoped honestly; draw inputs match ConnectBlock objects or a proved projection; mint flag no longer unconstrained if the theorem says “mint”). Completing P1-1 / P1-2 does not meet those conditions.

---

## Suggested sequencing (engineering)

```
Phase 1 (now)
    freeze + assessment + green gate + Classification snapshot
        │
Phase 2 (model-core closed 2026-08-20)
    dual-pool on apply_tx ── cap reject ── draw_candidate_root   [done]
        │
        script predicates (optional parallel)                    [open]
        │
Phase 3
    negative vectors (monetary + DCR + InvalidCvg Convergence/Root done; P1-1 script remainder)
        ── extra Coq-enforced cheap rows
        │
Phase 4
    CI + (optional) PROOF-034
```

Each remaining phase is a new task. Freeze policy: do not start Phase 3 remainder / Phase 4 as unscoped maintenance.

Do **not** start Path B2 or Path C as unscoped “next Coq work.” The feasibility assessment is documentation only.
