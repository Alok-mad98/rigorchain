# RIGOR L2 ↔ L3 Refinement — Feasibility Assessment

**Task:** `TASK-FV-ARCHITECTURE-REFINEMENT-FEASIBILITY-001`  
**Layer:** Architecture evaluation (no protocol change, no Coq catalog flip, no consensus edit)  
**Date:** 2026-08-20  
**Does not supersede:** [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md), [`../verification/FORMAL-VERIFICATION-STATUS.md`](../verification/FORMAL-VERIFICATION-STATUS.md)  
**Companions:** [`RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](./RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md), [`RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md`](./RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md), [`RIGOR_FORMAL_VERIFICATION_ROADMAP.md`](./RIGOR_FORMAL_VERIFICATION_ROADMAP.md)  
**Related prior notes:** [`../verification/SHARED-HASH-FEASIBILITY-2026-08.md`](../verification/SHARED-HASH-FEASIBILITY-2026-08.md) (hash binary only); Phase-5 `DrawClosureImpossibilityCatalog.v`

Honesty flags (unchanged by this assessment):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document does **not** implement refinement, does **not** flip Phase-5 `DCS_Impossible`, and does **not** claim a new correspondence theorem.

---

## Verdict

**Full L2 ↔ L3 refinement of `rigord` is not reachable under the current architecture.** The Phase-5 label `draw_closure_status = DCS_Impossible` remains the correct *conclusion* for complete Coq ⇄ C++ identity, even though two of the catalog’s freeze-era *prerequisites* (canonical encoder, executable SHA-256d in Coq) have since landed as artifacts.

The obstruction is **mostly architectural**, not a missing lemma in the existing cone:

- L2 is a Gallina model (plus an OCaml extract of the hash kernel). L3 is a Bitcoin-derived C++ node whose production hasher is `SHA256AutoDetect` SIMD.
- Several consensus-critical surfaces in L3 have **no matching type** in L2 (coinbase / Merkle / header bind, script interpreter, coin-view window walk, owner/PCORB mint authorization).
- Closing those surfaces inside today’s Coq would either invent a C++ semantics this repo does not have, or replace `rigord` with an extracted kernel — a different program, not a continuation of RC0.

**Worth funding now:** keep the finite differential gate; optionally close remaining *model* holes (script-vs-tag, `cm_draw_won` ← Selection). **Not worth funding this cycle:** mechanized Coq-function = C++-function proofs, a shared production hash binary, or a CompCert/VST campaign against `core/src/**`.

---

## 1. What “refinement” would have to mean

Three different claims are easy to collapse. Only the last is L2 ↔ L3 refinement.

| Claim | What exists today | Strength |
|-------|-------------------|----------|
| Theorems about the **model** | Kernel + monetary + replay Coq, `Qed`, no protocol-core `Axiom` | Proven *in L2* |
| Finite **correspondence** | 414 shared vectors; 207 Coq `vm_compute` lemmas; portable hash CLI; production σ/root CLI | Vector-checked L1↔L2↔L3 on a frozen set |
| Mechanized **refinement** | A theorem `⟦C++ f⟧ = Coq f` (or a shared object file) | **Absent.** `refinement_proved = false` |

Phase-5 catalogs record the third claim as unclosed. They are status bookkeeping, including `True`-bodied placeholders. They are **not** evidence that equivalence holds, and they are **not** a proof that the encoder was never written.

`DrawBridgeProgress.v` already separates those two readings: encoder and SHA-256d artifacts exist; `hash_kernel_shared_binary_with_cpp` is still false; remaining Phase-5 draw-closure gates (runtime trace / transition / accept history) are still false; even if all five catalog flags were true, the classifier would only become `DCS_PotentiallyClosable`, not closed.

---

## 2. Semantic gaps that actually exist (L1 / L2 / L3)

L1 (`formal_verification/spec/rigor_consensus_spec.py`) is the byte SSOT for draw / σ / roots / schedule and for the dual-pool / mint-shape / cap *predicates*. L2 models those plus a structural UTXO/connect machine. L3 is the shipping validator.

Gaps below are **real semantic differences**, not documentation lag.

### 2.1 Hash kernel

| Layer | What it is |
|-------|------------|
| L1 | Python `hashlib` SHA-256d |
| L2 | Executable `RigorSha256` over `list byte`; optional OCaml extract (`ExtrOcamlNatInt`) |
| L3 | `CHash256` / `CSHA256`; node startup calls `SHA256AutoDetect()` so the live `Transform` is typically SIMD (`USE_ASM`) |

Finite tripartite agreement (portable C++ Transform × hashlib × extract) is gate step 3. SIMD paths are bound by Bitcoin Core `SelfTest`, not by Coq. There is no shared `.o`. See SHARED-HASH-FEASIBILITY: production extract inside `rigord` is **rejected**.

### 2.2 Draw / σ / roots (the closest surfaces)

| Surface | L1 | L2 | L3 | Gap |
|---------|----|----|----|-----|
| σ 100-byte preimage | Encoder in spec | `CanonicalDrawEncoder.v` (offsets locked) | `ComputeSigmaHash` | Byte layout aligned; CLI-checked on golden families; **not** a C++ semantics theorem |
| Convergence_Root `C` | Sorted concat SHA-256d; `N < 2` → zeros | `ConvergeRoot.v` | `ComputeConvergenceRootFromEligibilityTxids` | Gate uses the **pure** function, not the coin-view wrapper |
| `draw_candidate_root` | `convergence_draw_eligible_root` (no `N < 2` shortcut) | `DrawCandidateRoot.v`; e2e with Selection / Window | `ComputeConvergenceDrawCandidateRoot` via `BuildConvergenceDrawSet` + coin view / undo | Coq hashes an already-filtered txid list. C++ **constructs** that list from the block body, `CCoinsViewCache` / `CBlockUndo`, and `ConvergenceWindowTail` |
| Winner | `RunSelection` | `select_winner` / `pick_better_tx` | `ResolveConvergenceDraw` | Same relation on golden vectors; window *walk* is C++ (`BuildConvergenceWindowTail`) |
| A-9-C exclusion | Spec + vectors | `WindowExclusion.v` over an abstract `already_paid` list | `FinalizeWindowedDrawSet` + undo/index walk | Model of the *rule*; not a refinement of the walk |

`draw_candidate_root_scope_is_eligible_digest` is a *non-claim*: Coq has no coinbase, tx-Merkle, or header argument. Inserting the digest into CVGE / coinbase, hashing the tx tree, and binding the header is **implementation-level** L3 (`ConnectBlock` / RPC). That chain is the whitepaper “self-proving” path; Coq stops at C / DCR / σ / draw.

### 2.3 Script classification

| Layer | Asset class | Interpreter |
|-------|-------------|-------------|
| L1 | Predicates + `ScriptMintNeg` vectors; shape in `apply_tx_model` | None |
| L2 | Pooling uses free `out_asset` (`we_do_not_claim_script_derives_asset`). **Shape** on `apply_tx`: non-empty `IsAllowedScript` / spend-CVG2 / optional PCORB *sizes* | None. `IsResScript` vs tag is vector-only |
| L3 | `AssetTypeFromScriptPubKey` / `IsResScript` / `IsAllowedScript` | Full script VM, P2WSH, `CheckTransaction` |

Empty `out_script` is a DualPool fixture placeholder in L2, not a C++ allowed template. PCORB *field semantics* (txid / root vs chain) stay in `ParseConvergeProof` / `ConnectEvaluateResMintTx`.

This is the largest **abstraction** gap between “minimal script architecture” as a verification story and the Coq connect path.

### 2.4 UTXO representation

| | L2 (`ReplayState`) | L3 (`CCoinsView` / `Coin`) |
|--|---------------------|----------------------------|
| Identity | Outpoint `(tx_id, vout)`; `tx_id` is an abstract id in the replay cone (that cone has no SHA-256) | `COutPoint` + computed txid |
| Output | `out_value`, opaque `out_script`, **explicit** `out_asset` | `scriptPubKey` + value; asset derived from script |
| Maturity | No `nHeight` / mint flag on `TxOut` | `Coin::RequiresMaturity` (100-block RES) |
| Set | Association list | Cache + undo + disk |
| RES supply | `rs_res_supply` **scalar independent of** `utxo_res` | UTXO plus ConnectBlock mint accounting |

`apply_tx` does not update `rs_res_supply`; mint-delta does not update the UTXO map (`we_do_not_claim_rs_res_supply_equals_utxo_res`). Dual-pool `apply_block_res_invariant` is a theorem about **body net plus protocol mint on the scalar**, not “the C++ UTXO sums to the cap.”

Phase-4G already classified the payload-domain mismatch: Coq Replay/Reorg/ConnectBlock observe **heights**; frozen/runtime evidence observes **block hashes**. The missing mapping is state encoding of full block bytes, not a missing `Qed` in `Selection.v`.

### 2.5 Mint authorization

| | L2 | L3 |
|--|----|----|
| Winner flag | Unconstrained `cm_draw_won` on `ConnectMintSpec` (three fields: won / ORU subsidy / mint count) | `ConnectEvaluateResMintTx`: owner encoding, PCORB parse, domain, parent draw set / outcome |
| Obligation | H-1 / winner-must-mint **under that flag**; `valid_res_mint_obligation_ignores_body` | Mint must match the scored winner of the parent draw |
| Amount / cap | `W(h)` from **header** height; `valid_res_cap` reject | Same production reject family; not a Coq theorem of `validation.cpp` |

`we_do_not_claim_cm_draw_won_is_owner_pcorb` and `we_do_not_claim_cm_draw_won_determined_by_body` make the gap unskippable: two blocks may share header and body and disagree on the winner flag. Wiring Selection `Some w` into `cm_draw_won` (Gap P1-2) would still be **model composition**, not L3 refinement.

### 2.6 L1 ↔ L2 remainder (not L3, but it bounds any later refinement)

- **207 / 414** rows are Coq-enforced. SHA-256-heavy families stay Python/C++ for `vm_compute` cost.
- NegativeCore pins dual-pool / cap / obligation **booleans**; `hash32_eq_dec` is `Qed`-sealed, so `apply_block` itself is not `vm_compute`d.
- A green L1↔L2 harness is not a C++ theorem.

---

## 3. Why Phase-5 says `DCS_Impossible`

### 3.1 What the classifier actually computes

`classify_draw_closure_status` (`DrawClosureImpossibilityCatalog.v`) returns `DCS_Impossible` if either:

1. `dcc_has_draw_canonical_encoder = false`, or
2. `dcc_has_runtime_hash_kernel = false`.

Those booleans are **not** “does `CanonicalDrawEncoder.v` / `RigorSha256.v` exist in the tree.” They are projections of the freeze-era Phase-5 reconstruction / gap catalogs (`draw_reconstruction_entry`, `GK_MissingHashKernel` in `draw_gaps`). On that frozen evidence shape they remain false, so `draw_closure_status = DCS_Impossible` still type-checks.

The file header already warns: do **not** read that equality as “the encoder was never written.” Read it as “Phase-5 equivalence is not closed.”

### 3.2 Architectural vs engineering

| Class | Obstacle | Closes if we only add lemmas / vectors? |
|-------|----------|------------------------------------------|
| **Architectural** | Different languages and TCBs (Gallina/OCaml extract vs C++ `rigord`) | No |
| **Architectural** | Production hash is SIMD AutoDetect, not the extract | No (and must not be “fixed” by linking OCaml into the node) |
| **Architectural** | L2 Replay/Connect types (height, scalar supply, asset tag, abstract winner) vs L3 (block hash, coin view, script, owner/PCORB) | No — needs new models *and* a C++ semantics, or a new node |
| **Architectural** | Coinbase → tx Merkle → header → PoW not in the Coq cone | No |
| **Architectural** | Phase-5 “runtime trace / transition / accept history” as *canonical state* were never exported by L3 in a form the catalog can ingest | Only with a new evidence architecture (still not Coq=C++) |
| **Engineering** | Encoder / SHA-256d / DCR *artifacts* now exist; catalogs were not rewritten | Hygiene only; does not close refinement |
| **Engineering** | Finite vector / CLI coverage of pure functions | Already the live L1↔L3 bridge; denser coverage is Gap P1-4 / P1-5 |
| **Engineering** | Model holes (`IsResScript` vs tag, Selection → `cm_draw_won`) | Strengthens L2; leaves `refinement_proved` false |

**Conclusion:** `DCS_Impossible` is a **frozen-architecture classifier** whose two “missing artifact” bits are stale relative to `DrawBridgeProgress.v`, but whose **impossibility of full draw/node equivalence** is still the right engineering statement. Flipping the Coq constructor to `DCS_PotentiallyClosable` without a C++ semantics would be a stronger claim than the tree supports. This task leaves the catalogs unchanged.

### 3.3 What would *not* suffice

- A green `run_differential_gate.sh` (required; still finite).
- Extracting more Coq to OCaml (second test kernel; still not `rigord`).
- Closing P0 model work (already done) or P1 script/mint composition (still model-only).
- Rewording Phase-5 placeholders (`True`-bodied lemmas are not evidence either way).

---

## 4. Optional paths

### Path A — Keep the status quo (finite differential)

**What it is.** The current three-layer system: L2 theorems about the model; L1↔L2 harness (207); L1↔L3 vectors + portable hash + production σ/root CLIs (414). Honesty flags stay false. Phase-5 keeps `DCS_Impossible`.

**What it buys.** Regression detection on the hashed kernel and on the modeled reject predicates. Public language stays accurate.

**What it does not buy.** Universal Coq = C++; SIMD; ConnectBlock; script VM; mint witness semantics.

**Effort.** Maintenance of the freeze (already Roadmap Phase 1 / 4).

**Recommendation:** **Default.** This is the honest RC0 posture.

### Path B — Local observational refinement (kernel / monetary *pure functions* only)

Two sub-options, often confused:

**B1. Denser finite correspondence (recommended if any Path B is funded).** Stratified extra Coq-enforced rows (Gap P1-4); coin-view wrapper vs pure eligibility root (P1-5); optional SIMD-vs-portable hash CLI (SHARED-HASH-FEASIBILITY option D). Still not `refinement_proved`. Still not a shared binary.

**B2. Mechanized local refinement.** Introduce a C semantics (VST / CompCert C / CN, etc.) for a handful of pure functions — e.g. portable `sha256.cpp` Transform, `HashConcatenatedSortedTxids`, `ComputeSigmaHash`, maybe `W(k)` — and prove they match Gallina.

| | B2 reality |
|--|------------|
| Best candidates | Byte-pure, already CLI-locked functions |
| Still out of scope | SIMD `Transform`, `BuildConvergenceWindowTail`, `ConnectEvaluateResMintTx`, `CheckTxInputs`, coinbase layout |
| New TCB | The C semantics, the subset of C++ accepted by that tool, and the proof of the glue |
| Effort class | Multi-month to multi-year for a few functions; comparable to a separate verified-crypto project |
| Residual vs today’s gate | The gate already fails if those functions disagree on 414 vectors + FIPS + random hash payloads. B2 removes “finite” for those functions only |

**Recommendation:** **Do not fund B2 this cycle.** ROI is poor against the existing gate. B1 remains available as ordinary Gap work, not as a refinement program.

Model-internal composition (P1-1 remainder, P1-2) is **not** Path B. It does not relate L2 to L3.

### Path C — Architecture changes required before *full* refinement could even be stated

Full `core/src/**` = Coq is a **different program**. Minimum architectural moves (any one is a fork of the verification story):

| Move | What it would do | Why it is not RC0 |
|------|------------------|-------------------|
| **C1. Extracted consensus kernel in the node** | Ship Gallina (or a C reimplementation of it) as the validator | Wrong language/TCB for hash (OCaml / `nat`-as-`int`); kills SIMD; consensus blast radius; rejected for hash already |
| **C2. Verified compilation of the C++ node** | CompCert-class pipeline for Bitcoin-derived C++ | No such pipeline exists for this codebase; C++ is outside CompCert’s C |
| **C3. Deep embedding of a C subset** | VST-style proofs of selected `.cpp` | Same as B2 scaled to ConnectBlock/script — research-program size |
| **C4. New evidence plane** | Runtime exports of traces, transition counts, accept history, block bytes so Phase-5 catalogs could ingest L3 | Could make draw closure *classifiers* less stale; still would not prove Coq = C++ |

None of C1–C4 is a “next Coq lemma.” None of them should flip `refinement_proved` until a real correspondence theorem exists.

---

## 5. Is it worth investing?

| Investment | Worth it? | Why |
|------------|-----------|-----|
| Keep Path A (gate + honesty) | **Yes** | Highest assurance per unit cost already built |
| Path B1 (denser finite / coin-view / optional SIMD CLI) | **Optional**, scoped as Gap P1-4 / P1-5 / hash note | Tightens the existing bridge; does not change the claim vocabulary |
| Path B2 (VST/CompCert on pure functions) | **No, this cycle** | Cost dominates residual risk given 414/207 + CLIs |
| Path C (shared node kernel / verified C++ / full ConnectBlock embedding) | **No, as RC0 continuation** | Architecture replacement; would need a separate charter and would still not justify flipping flags early |
| Flip `DCS_Impossible` or honesty flags because encoder/hash artifacts exist | **No** | Artifacts ≠ refinement; catalogs’ conclusion is still right |

Relative to residual *fork-class* risk, the next dollars still belong to **model** holes that hide tag-vs-script and abstract-winner-vs-Selection (P1-1 remainder, P1-2), not to a refinement campaign.

---

## 6. Conditions under which refinement could be *reopened*

Reopen a refinement *program* only if **all** of the following hold. This list is a gate, not a schedule.

1. A written C (or C++) semantics covering the functions in scope, accepted as TCB.
2. For anything called “production hash refinement”: either the proved object **is** `CHash256` including the live `Transform`, or the claim is explicitly limited to the portable Transform and `hash_kernel_shared_binary_with_cpp` stays false.
3. For anything called “draw refinement”: the Coq function’s inputs are the same objects ConnectBlock uses (block body + coin view or undo + window tail), or a proved projection from those objects to the txid list Coq already hashes.
4. For anything called “mint/connect refinement”: `cm_draw_won` is no longer an unconstrained boolean, and owner/PCORB/domain are in the model **or** explicitly carved out of the theorem statement.
5. Honesty flags flip **only** when the theorem exists (`Qed` of the correspondence), not when a catalog classifier is rewritten.

Until then, the public sentence remains: **strongly verified model + finite correspondence; shipping-binary identity is not proved.**

---

## 7. What this assessment will not be cited as

- A plan to implement refinement.
- A finding that Phase-5 was wrong to say full closure is impossible.
- Permission to set `equivalence_claimed`, `hash_kernel_shared_binary_with_cpp`, or `refinement_proved` to true.
- A claim that Path B1 “almost” is refinement.
- A C++ or Coq code change.

Gap / Roadmap entries for refinement now point here. They stay in the **do not fund full refinement** bucket, with Path A as the maintained bridge and Path B1 as optional finite work.
