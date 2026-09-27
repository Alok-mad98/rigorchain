# Rigor Consensus Formal Verification

> **Scope statement (read this first).**
> This directory contains a Coq corpus and remaining mint / hash / classify
> differential tests. The lottery Python L1 spec was removed in Batch A. It does **not** contain a proof
> that the C++ implementation matches the specification, and it does **not**
> contain a no-fork theorem. Equivalence between the three layers is **NOT
> ESTABLISHED**. Every claim below is accompanied by the file and line where it
> can be checked; if a claim here cannot be located in the source, treat the
> claim as a documentation bug and file it.

**Authority rule:** Protocol specifications are normative SSOT.
This directory is a formal **model**, not C++ authority. If this README
disagrees with RFC-020 / RFC-018, the RFC is correct.

**Current public status:** [`docs/formal/VerificationStatus.md`](../docs/formal/VerificationStatus.md).
Claim boundary: [`docs/formal/Claims.md`](../docs/formal/Claims.md).
Machine SSOT: [`audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`](../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md).

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Product identity (P0-029):** canonical tree is
[`https://github.com/rigorchain/rigorchain`](https://github.com/rigorchain/rigorchain).
Wallet storage is a **SQLite descriptor** file named `rigor-wallet.sqlite`
(not Berkeley DB, not `wallet.dat`). There is **no** `libbitcoinconsensus`
(or `librigorconsensus`) C ABI in this tree — consensus verify is in-process
`VerifyScript` (ScriptPubKey → `AssetTypeFromScriptPubKey` → Ed25519).
HISTORICAL Bitcoin Core PR URLs in comments stay as citations of past work.

| Public document | Path |
|-----------------|------|
| Formal verification status | [`docs/formal/VerificationStatus.md`](../docs/formal/VerificationStatus.md) |
| Claims | [`docs/formal/Claims.md`](../docs/formal/Claims.md) |
| Status SSOT | [`audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`](../audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md) |
| How to verify | [`docs/security/HOW_TO_VERIFY_RIGOR.md`](../docs/security/HOW_TO_VERIFY_RIGOR.md) |
| FV release checklist | [`docs/verification/FV-RELEASE-CHECKLIST.md`](../docs/verification/FV-RELEASE-CHECKLIST.md) |

---

## 1. The three layers, and what actually connects them

| Layer | Artifact | Status |
|---|---|---|
| L1 — Executable spec | Removed (lottery Batch A). Live issuance SSOT is RFC-020. | |
| L2 — Coq model | `coq/**` (~218 files) | Machine-checked model properties. Not C++ equivalence. |
| L3 — C++ production | `core/src/**` | Realization of RFC-020 / RFC-018. Tests are evidence, not a proof. |

Connections that actually exist:

- **L1 ↔ L2**: Lottery L1 spec / `GoldenHarness.v` / `gen_coq_harness.py` were
  removed in Batch A. Remaining mint / script / hash rows live in
  `phase3_golden_vectors.json` (127) and `coq/Kernel/NegativeCoreHarness.v`.
  FIPS short SHA-256 is locked in `CanonicalHashKernel.v`. Vector-level
  agreement only; not universally quantified equivalence.
- **L1 ↔ L3**: lottery `selection_golden_bridge.inc` removed. Remaining hash
  evidence is the portable CSHA256 CLI. Runtime harness JSON still carries
  `equivalence_claimed: false`.
- **L2 ↔ L3**: **no refinement proof.** Coq SHA-256 can be extracted to OCaml
  (`extracted/hash256_kernel.ml`); portable C++ CLIs compile `sha256.cpp`
  for the gate. Those are test drivers, not a shared node binary.
  What exists toward C++ is differential testing over the shared vectors
  plus a canonical encoder spec both sides implement. PD-4F-CPP-01 remains open.

There is **no** theorem anywhere in `coq/` of the form "C++ = Python = Coq".
Any document asserting one is wrong.

---

## 2. What is actually proven in Coq

All results below are `Qed` and were located by file+line in the current tree.
Two global caveats apply and are stated once here:

- **Declared axioms.** The Verdict's syntactic and `Print Assumptions` audits
  found **zero** remaining `Axiom` / `Parameter` / `Admitted` in the protocol
  theorems (`docs/verification/FORMAL-VERIFICATION-VERDICT.md`). The byte-level
  modules (`RigorSha256.v`, `RigorHash.v`, `Kernel/*`) compute real SHA-256 on
  real bytes. Do not resurrect an uninterpreted hash oracle.
- **Model-vs-implementation caveat.** Every theorem is about the Coq model. It
  transfers to production only insofar as the model matches `core/src/**`, which
  is unproven (see §4).

### 2.1 Byte-exact SHA-256 and golden vectors (strongest artifact)

- `coq/RigorSha256.v` — executable SHA-256 over `list byte`, no parameters.
- `coq/RigorHash.v:9` — `hash256_bytes` = SHA256d at byte granularity.
- `coq/Kernel/GoldenHarness.v` — 187 kernel golden vectors (SHA-256 set plus
  WindowExclusion, SigmaEncoder, DrawCandidateRoot including 12 negative/perturbation
  rows, InvalidCvg 14 Convergence/Root rejects, ScriptMintNeg 12 script/witness
  rejects, EdgeCross root/window/obligation combination rows, extra Inequality, short HashKernel)
  and `coq/Kernel/NegativeCoreHarness.v` — 28 dual-pool / mint / cap / H-1 plus
  EdgeCross script/PCORB/schedule lemmas, recomputed by `vm_compute`
  and proven equal to the L1-produced expected bytes. Each vector is a `Lemma
  golden_<ID> : chk_<n> = true` closed with `vm_cast_no_check (eq_refl true)`,
  so **a regressed enforced vector breaks the build**. Counts are pinned in
  `golden_lock.json` (`surface_splits` names L1 vs L2 execution splits) and fail
  `scripts/check_golden_lock.py` (wired into `scripts/run_differential_gate.sh`).
  The full 422-vector set is checked by that gate.

### 2.2 Determinism

- `coq/Proofs/Determinism.v:7` — `selection_deterministic`: `Execute` yields at
  most one winner for given inputs.
- `coq/Phase4FConsensus/ConsensusDeterminism.v:12` — `execute_transition_deterministic`.
- `coq/Phase4FConsensus/ConsensusDeterminism.v:27` — `execute_trace_deterministic`
  (previously `Admitted`; discharged under SECURITY-001-F).
- `coq/Phase4FConsensus/ConsensusDeterminism.v:50` — `consensus_replay_determinism`:
  replaying the same trace from the same state yields the same state.
- `coq/ReplayReorgCore/SupplyConservation.v:222` — `supply_deterministic`.

Note the scope: these say the transition *function* is a function. Draw-set
permutation invariance is proved in `OrderIndependence.v` (see G-1). They do not
say two nodes given *different but equivalent* **block encodings** (UTXO apply
order, serialization) agree; that ConnectBlock residual remains in G-1's leftover.

### 2.3 Selection and ordering

The list-of-tx / global-supply modules `Types.v`, `State.v`, `Transition.v`, and
`Selection.v` live in [`coq/legacy_account_model/`](coq/legacy_account_model/README.md)
(W9). They do **not** match the C++ UTXO node. Structural replay/reorg is
`coq/ReplayReorgCore/`.

- `coq/RigorBytesOrder.v:26,46,69` — `bytes_ltb_trans`, `bytes_ltb_antisym`,
  `bytes_ltb_total`: the memcmp byte order is a strict total order.
  (`nat_ltb_false_gt` at line 13 was `Admitted`; discharged under SECURITY-001-F.)
- `coq/Kernel/KernelRoot.v` — `converge_root_length`,
  `converge_root_empty` / `converge_root_small` (empty F ⇒ 32 zero bytes),
  `converge_root_large`. A singleton hashes `Hash256(txid)`.
- `coq/DrawCandidateRoot.v` — `draw_candidate_root_from_flags` (L1 `convergence_draw_eligible_root`):
  length 32, empty set hashes empty concat and is not zeros, order-independent,
  eligible subset ⊆ F, winner is in that hashed set, DCR = `C` of eligible when
  |eligible| ≥ 2, A-9-C exclusion-stable, e2e pipeline (`draw_pipeline_e2e`).
  Negative/perturbation golden rows (`DCR-024`…`035`) lock tamper, duplicates,
  empty DCR ≠ zeros, singleton DCR = C, exclusion mismatch, and σ-input changes (C, B_{h-1}, h).
  Complementary `InvalidCvg` rows (`ICVG-000`…`013`) lock invalid Convergence-tx
  abstract form, committed-`C` mismatch, malformed concat/length, duplicate txids
  in F, DCR(empty) as C of N=0, and zeros as C of N=1.
  `ScriptMintNeg` rows (`SMW-000`…`011`) lock abstract `IsAllowedScript` /
  `IsResScript` / PCORB stack shape / spend-CVG2 rejects (`ScriptMintWitness.v`);
  not an interpreter. **Shape** (non-empty allowed-template / spend-CVG2 /
  optional PCORB sizes) is a conjunct of `apply_tx` (`ScriptShape.v`).
  `IsResScript` vs the `out_asset` tag is not.
  σ uses Convergence_Root of F, not DCR. Coinbase / Merkle / header bind of
  the digest is **not** claimed (`draw_candidate_root_scope_is_eligible_digest`).
  No `N < 2` shortcut.

### 2.4 UTXO and replay/reorg

- `coq/ReplayReorgCore/ReplayInvariants.v` — `utxo_spend_removes_output`,
  with `not_recreated` independent of in-block index. Distinct-txid spends
  delete unconditionally (`utxo_spend_removes_output_distinct_txid`). See §3.
- `coq/ReplayReorgCore/ReplayInvariants.v:190` —
  `utxo_spend_removes_output_unconditional_is_false`: a machine-checked
  *refutation* of the unconditional form.
- `coq/ReplayReorgCore/ReplayInvariants.v:230` — `rollback_restores_previous_state`.
- `coq/ReplayReorgCore/ReplayInvariants.v:256` — `replay_preserves_utxo_consistency`
  (same side condition).
- `coq/ReplayReorgCore/SupplyConservation.v:245,311,411` — `supply_monotonic`,
  `rollback_preserves_supply`, `genesis_supply_zero`.

### 2.5 Weak-but-honest results (self-labelled in source)

- `coq/Proofs/Safety.v:11` — `unique_mint`. The source comment at lines 7-10
  self-labels this **"WEAK (real, but content-light)"**: it proves `Execute` is a
  function, *not* supply-level mint uniqueness. Do not cite it as a supply guarantee.
- `coq/Proofs/Fairness.v:16,25` and `coq/Proofs/SplitInvariance.v:13` — real
  deterministic selection/weight-conservation lemmas that **replace** earlier
  vacuous probabilistic statements. The probabilistic versions remain OPEN.
- `coq/Phase4GRefinement/FormalVerificationCertification.v:11-21` — an explicit
  **HONESTY BOUNDARY** section. It states that "READY_FOR_RELEASE" means the
  artifact set is consolidated and machine-checked, and explicitly **does not**
  mean Coq = Runtime, Coq = C++, or Coq = Consensus. The frozen verdict is
  `RV_Partial`, not `RV_Equivalent`. Preserve this text.
- `coq/ReplayReorgCore/OpaqueBoundary.v` — residual *non-claims* (`out_script` as
  asset class, `cm_draw_won`, `rs_res_supply`). Shape checks on `apply_tx` do
  not derive `out_asset` (`we_do_not_claim_script_derives_asset`).
  `coq/DrawCandidateRoot.v` §9 — DCR is an eligible-list digest only
  (no coinbase / Merkle / header bind).

---

## 3. Known-false and explicitly-open statements

These are recorded here so nobody re-derives a false claim from an old document.

**A spent outpoint is not unconditionally removed under txid reuse.**
`apply_tx_utxo` now assigns created outputs `MkOutPoint (tx_id tx) i`
(vout starting at 0). Distinct transactions cannot collide. The
unconditional lemma remains refutable when a transaction's `tx_id`
equals the txid of an outpoint it spends (duplicate txid / BIP30-class).
The refutation is checked by the build
(`coq/ReplayReorgCore/ReplayInvariants.v`, `utxo_spend_removes_output_unconditional_is_false`).
The production case is the distinct-txid theorem
`utxo_spend_removes_output_distinct_txid`; `not_recreated` is derived
from `outpoint_txid op <> tx_id tx`. This is **not** the old
shared-placeholder modelling defect.

**Open proof obligations (unchanged, restated):** PD-4F-OB-02 (oracle agreement /
C++ bridge), PD-4F-CPP-01 (C++ equivalence), PD-4F-OB-03. Under SECURITY-001-F the
former `True`-bodied placeholder theorems in `ConsensusDeterminism.v:69`,
`ConsensusOracleBridge.v`, and `ConsensusTraceExecution.v` were replaced by
explicit `Prop` definitions naming the obligation plus a provable conditional
theorem, so the obligation is now visible in the source rather than disguised as
a discharged result.

---

## 4. Verification gap register (summary)

**Freeze note (2026-08-20).** The table below is the SECURITY-001-F snapshot
with G-4 counts and G-5 updated after the replay-txid fix, then the 2026-08-20
model-core closed loop (dual-pool connect, cap reject, `draw_candidate_root`,
negative vectors). **Equivalence is still unproved.** Current boundary:
`docs/verification/FORMAL-VERIFICATION-STATUS.md` and
`docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`.

Full, prioritized register: `docs/security/TASK_SECURITY_001_F_REPORT.md`.

| # | Gap | Why it matters |
|---|---|---|
| G-1 | **Transaction-order independence of the draw set (model).** `OrderIndependence.v` imports `Permutation` and proves `convergence_root_order_independent`, `draw_and_winner_order_independent`, and `select_winner_order_independent` (positive weights, distinct txids). Residual: ConnectBlock UTXO *application* order is not proved. | Closed for C / DCR / winner on the same candidate *set*. Not a C++ refinement. |
| G-2 | **No absolute ORU supply cap.** `cb_oru_subsidy_ok` bounds per-block ORU; RES has `max_issuance_within_cap` and `valid_res_cap` reject. The 125 ORU tail means `total_oru_supply ≤ ORU_MAX_SUPPLY` is **false by design**. | Residual is the tail, not unbounded model issuance. |
| G-3 | **No Coq↔C++ equivalence.** Hash-kernel extraction exists; there is still no refinement proof. | All Coq results are about the model only. |
| G-4 | **Vector-level, not universal, L1↔L2 agreement.** 422 shared / 215 Coq-enforced (was 25 in the original audit). | Sound but finite. Not equivalence. |
| G-5 | **Txid-reuse residual** in the replay model (§3). The shared-placeholder shortcut is closed. | Unconditional spend-delete is still false if a tx reuses a spent outpoint's txid. |
| G-6 | **Residual opaque boundaries** (`OpaqueBoundary.v`, `DrawCandidateRoot.v` §9). Script, `cm_draw_won`, DCR coinbase bind, and `rs_res_supply` vs `utxo_res` are declared non-claims. | Prevents reading Closed mint/dual-pool theorems as owner/PCORB or script-derived assets. |

---

## 5. Building and running

### Coq corpus

```bash
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j"$(nproc)"
```

Lottery `Kernel/GoldenHarness.v` is gone. Compile remaining protocol-core
files from `_CoqProject.protocol-core`.

### Assumption audit (what rests on axioms)

```bash
cd formal_verification
python3 tools/audit_assumptions.py
```

Runs `Print Assumptions` on every top-level `Theorem`/`Lemma`/`Corollary` and
emits the axiom-free vs axiom-dependent split. Run this before citing any
theorem as established.

### Python spec (L1)

Removed in lottery Batch A. Live issuance SSOT is RFC-020 / `ResSchedule.v` /
`core/src/consensus/res_issuance.h`.

### Differential gate / pre-release checklist

```bash
cd ~/rigor-official-tree
bash formal_verification/scripts/run_fv_release_checklist.sh
bash formal_verification/scripts/run_differential_gate.sh --help
```

The checklist runner is freeze artifacts plus `SKIP_TEST_RIGOR=1` `run_differential_gate.sh` (lock check is gate step 2b). Optional: `FV_COQ=1`, `FV_EXTRACT=1`, `FV_TEST_RIGOR=1`. Direct gate: `bash formal_verification/scripts/run_differential_gate.sh`. This is not a refinement proof. See [`docs/verification/FV-RELEASE-CHECKLIST.md`](../docs/verification/FV-RELEASE-CHECKLIST.md).

Lottery golden-vector oracles and the σ differential fuzzer were removed in
Batch A. Remaining hash rows: `python3 tests/test_hash_kernel_agreement.py`.

---

## 6. Byte-level requirements (normative, from L1)

These are requirements on implementations, not proven theorems.

- SHA256d exactly; no variants, no single-SHA.
- σ preimage is **100 bytes**: `txid ‖ C ‖ B_{h-1} ‖ LE32(h)`. Height is LE32 at offset 96.
  There is **no** `target_prev` slot (`canonical_encoder_has_no_target_prev_slot`).
- txids sort by **memcmp byte order** — never numeric, never string-lexicographic.
- ConvergenceRoot is a **flat concatenation** of sorted txids then SHA256d — not a
  Merkle tree, and individual txids are not pre-hashed.
- Empty F ⇒ ConvergenceRoot is 32 zero bytes (proven in the model:
  `coq/Kernel/KernelRoot.v` `converge_root_empty`). A singleton hashes
  `Hash256(txid)`, same as any larger F.
- `draw_candidate_root` is the same concat-hash; the remaining distinction is
  the empty set (DCR hashes `[]`, C is zeros). See `coq/DrawCandidateRoot.v`.
  Coinbase bind of that digest is C++ (`ComputeConvergenceDrawCandidateRoot`).
- Tie-break is **txid memcmp** (unsigned byte order) after equal cross-products.
  It is **not** `txid XOR ConvergenceRoot` (removed; RFC-000 §9).
- Inequality is strict: `σ < T · f` (`eligible` / `ConvergenceDrawInequalityHolds`).
  `f` is on-chain weight in base units; `T` is `RES_PROBABILITY_TARGET` (E=246, ÷ W=32).

Historical fork incidents motivating each rule (endianness, sort order, hash
variant, Merkle variant, tie-break ambiguity) are catalogued in the L1 spec
comments; they are context, not evidence about this codebase.

---

## 7. For auditors

1. Start with RFC-020 / `CODE_SSOT_PROTOCOL_FACTS.md` — the lottery L1 spec is gone.
2. Run `tools/audit_assumptions.py` and read the axiom-dependent list first.
3. Check `coq/Kernel/GoldenHarness.v` — this is the load-bearing cross-layer artifact.
4. Read §3 and §4 before reading any other verification document in this repo.
5. Assume Coq↔C++ equivalence is **unproven** unless you find a refinement proof.
   As of this writing there is an optional OCaml *extraction* of the Coq hash
   kernel (`extracted/`), which is not a C++ correspondence proof. Script
   classification is modeled as abstract byte-shape predicates
   (`ScriptMintWitness.v`); a **finite shape subset** is a conjunct of
   `apply_tx` (`ScriptShape.v`). `IsResScript` vs `out_asset` is not.
   Coinbase bind of `draw_candidate_root` is C++ / docs. See
   `docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` §8.
