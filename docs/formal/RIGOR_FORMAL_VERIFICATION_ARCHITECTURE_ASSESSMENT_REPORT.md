# RIGOR Formal Verification — Architecture Assessment Report

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-ARCHITECTURE-ASSESSMENT-P0-001`  
**Date:** 2026-08-20  
**Layer:** Evaluation and planning. **No** consensus, kernel, validation, wallet, miner, or RFC modification.  
**Verdict:** **PASS** (assessment complete; freeze honesty preserved)

Companions:

- [`RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](./RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md)
- [`RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md`](./RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md)
- [`RIGOR_FORMAL_VERIFICATION_ROADMAP.md`](./RIGOR_FORMAL_VERIFICATION_ROADMAP.md)
- [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md)

Operator freeze (unchanged): [`../verification/FORMAL-VERIFICATION-STATUS.md`](../verification/FORMAL-VERIFICATION-STATUS.md)  
Model theorems: [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md)

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

---

## Answers (required)

### 1. What has already been achieved?

A **three-layer** verification system exists and is operational:

1. A Python executable specification for draw / σ / Convergence_Root / RFC-017 schedule.
2. A Coq **model** with an executable SHA-256, byte-accurate σ, Convergence_Root, and `draw_candidate_root`, order-independent selection (under stated hypotheses), A-9-C exclusion, dual-asset mint schedules, and replay/reorg supply lemmas — closed with `Qed` and no protocol-core `Axiom` / `Admitted`.
3. A **finite** correspondence gate: **422** shared golden vectors, **215** Coq-enforced, tripartite hash CLI, production σ/root CLI.

Phase-5 catalogs **machine-check the fact that full Coq⇄C++ refinement is not closed**. That honesty is an achievement, not a defect.

### 2. What is currently formally guaranteed?

**Guaranteed about the Coq model** (not about `rigord`):

- SHA-256d is a concrete function on bytes; σ is `Hash256(txid‖C‖B_{h-1}‖LE32(h))`.
- Convergence_Root is a function of the candidate *set* (permutation invariance) and is 32 zero bytes when `N < 2`.
- `draw_candidate_root` is a function of the eligible *subset* (permutation invariance), always 32 bytes, and hashes empty concat when that subset is empty (not zeros). Winner selection uses that same subset (`winner_in_draw_eligible_set`).
- The winner is a function of the same set when weights are positive and txids distinct.
- The eligibility filter is non-vacuous at E=246; the pre-A-6 target is proved vacuous.
- A given txid wins at most once on a paid-closed window trace (model).
- RES maximum conceivable issuance under one mint per block is a finite sum `≤ 21M` (`max_issuance_within_cap`).
- ORU issuance per block is bounded by the subsidy schedule (3 halvings, then 125 ORU tail); n blocks add at most n genesis-era subsidies.
- Replay of the same block sequence yields the same model supply; rollback restores a snapshot.

**Guaranteed at the correspondence layer:** the shared 422 vectors agree across Python, the Coq-enforced subset, and the portable/production test CLIs **or the gate fails**. That is finite.

### 3. What is not yet formally guaranteed?

- That `core/src/**` equals the Coq model (refinement).
- That `draw_candidate_root` equals the C++ coinbase-bound function. The *model* digest and e2e pipeline are proved; inserting the digest into coinbase / Merkle / header is **implementation-level** C++ (`draw_candidate_root_scope_is_eligible_digest`).
- That every accepted C++ block satisfies dual-pool conservation *in Coq* (the **model** now has `apply_block_res_invariant`; that is not a C++ refinement). `rs_res_supply` is not `utxo_res`.
- That full script classification / Witness v0–v1 / CVG2 rules hold as Coq theorems of `apply_tx`. **Shape** (non-empty `IsAllowedScript` / spend-CVG2 / optional PCORB sizes) **is** an `apply_tx` conjunct (`ScriptShape.v`). `IsResScript` vs `out_asset`, P2WSH execution, and PCORB field semantics are not.
- Owner / PCORB / domain mint authorization as one composed theorem. `cm_draw_won` is not the Selection winner.
- 100-block maturity in Coq.
- Cap-overshoot: **closed in the model** (`valid_res_cap` reject); not a C++ refinement.
- Transaction-order independence of the **full ConnectBlock** (UTXO application order), as opposed to draw-set permutation invariance already proved.
- Fairness or grinding cost of **real** SHA-256.
- Network, mempool, P2P, SIMD hash paths, coin-view wrapper.
- PROOF-034 live archive (procedure only).
- Anything implied by a green gate other than agreement on that finite set.

### 4. What are the highest-value future improvements?

1. Wire dual-pool into `apply_tx` (P0-1) — **done** (block-level invariant).
2. Model cap-overshoot as reject (P0-2) — **done**.
3. Coq `draw_candidate_root` (P0-3) — **done** 2026-08-20; coinbase bind remains C++.
4. Negative golden vectors for invalid Convergence, mixing, script and mint rejects (P0-4) — **done** for Convergence/Root error paths (`InvalidCvg` + NegativeCore + DCR negatives) plus abstract script/PCORB rejects (`ScriptMintNeg`); **shape** wired into `apply_tx` (P1-1 partial).
5. Script-classification predicates without a VM (P1-1) — **partial**: shape checks on `apply_tx` (`ScriptShape.v`); `IsResScript` vs `out_asset` and P2WSH execution stay C++.
6. CI on assumptions + differential gate (P2-1).

Do **not** fund full node refinement or a shared production hash binary under the current architecture.

### 5. Is current formal verification sufficient for RC0?

**Yes, for an honest RC0 freeze:** strongly verified *model*, operational differential gate, public non-claims.

**No, if RC0 is interpreted as “the monetary protocol is a theorem of the shipping binary.”** That interpretation is not supported and is explicitly denied by the freeze flags.

### 6. What would be required to reach a stronger “mathematically verified monetary protocol” position?

A *stronger public position* needs more than more vectors. Minimum:

| Requirement | Status |
|-------------|--------|
| Monetary rules as Coq theorems | **Present** for schedules, RES finite-sum cap, ORU linear bound, H-1 obligation (model) |
| Those theorems inductive on every accepted block, including transfer conservation | **Present in the model** (`apply_block_res_invariant`); not a C++ refinement |
| Script-derived asset class, not a free `out_asset` tag | **Absent** |
| `draw_candidate_root` + coinbase commitment in the model | **Partial** (`DrawCandidateRoot.v` present; coinbase bind still C++) |
| Mechanized correspondence for the mint/connect path | **Absent** (`refinement_proved = false`) |
| Independent audit of the Coq TCB (Coq 8.15 kernel) | Operational, not an extra theorem |

Even then, “mathematically verified monetary protocol” should mean **verified model + stated correspondence bound**, not “verified `rigord`.” Reaching binary identity would require a verified compilation or a shared extracted kernel in the node — **rejected** for production hash (OCaml extract vs SIMD `rigord`). The honest stronger position is: *the monetary rules are theorems of a byte-level model that agrees with the node on a published vector set and selected CLIs.*

---

## Section 2 — Verification boundary analysis

Classification of current correspondence:

| | A. Fully mathematically specified | B. Modeled, not proven equivalent to C++ | C. Runtime verified only | D. Implementation tested only |
|---|-----------------------------------|------------------------------------------|---------------------------|-------------------------------|
| **Meaning** | RFC + Python L1 + Coq definitions with theorems about those definitions | Coq exists; no refinement | Tests / fuzz / harness; little or no Coq | C++ tests / review |

### Transaction validation

- **A/B:** Structural UTXO apply (`apply_tx_utxo`); spends must exist; created outpoints `(tx_id, vout)`.
- **C/D:** Full `CheckTxInputs`, fees, script, maturity, standardness.

BIP30-class txid reuse remains a stated residual: unconditional spend-delete is **refuted** in Coq when a tx reuses a spent outpoint’s txid; production relies on distinct txids.

### Convergence selection

- **A/B:** `select_winner`, `pick_better_tx`, order independence, T non-vacuity, E=246 cost pins.
- **C:** 84 `RunSelection` vectors (7 Coq-enforced); BOOST `selection_golden_tests`.
- **Not equivalent:** window *construction* from undo data (`BuildConvergenceWindowTail`).

### `draw_candidate_root`

- **A:** Specified in whitepaper / Python `convergence_draw_eligible_root` (sorted concat SHA-256d, **no** N&lt;2 zero).
- **B:** `DrawCandidateRoot.v` — executable definition; e2e pipeline (`draw_pipeline_e2e`): eligible list, DCR, and winner aligned; σ uses `C` of F, not DCR; A-9-C exclusion e2e. Windowed form in `WindowExclusion.v`. **Not** a C++ refinement.
- **C/D:** C++ `ComputeConvergenceDrawCandidateRoot` / coinbase bind / RPC committed vs computed remain **implementation-level**; 36 golden vectors (24 positive + 12 negative/perturbation).

### Asset separation

- **B:** `AssetKind` on `TxOut`; `dual_pool_conservation` predicate.
- **C:** C++ dual-pool + fuzz.
- **Gap:** asset tag is not derived from script in Coq.

### RES mint rules

- **A/B:** W(k), cap identity, winner-must-mint / H-1 in the model; height from **block header** (reorg-safe); connect-path closed **under abstract `cm_draw_won`** (`winner_positive_W_connect_iff`, `valid_block_connect_reject_classification`).
- **C:** `ConnectEvaluateResMintTx`, owner, PCORB, domain.
- **Closed in model:** overshoot is `apply_block = None` (`valid_res_cap`); connect success/failure classified **under abstract `cm_draw_won`**. Remaining mismatch is owner/PCORB / Selection (`we_do_not_claim_cm_draw_won_is_owner_pcorb`).

### ORU issuance

- **A/B:** 1000 ORU, interval 210000, max 3 shifts, tail 125 ORU; `cb_oru_subsidy_ok` on `valid_block_connect`.
- **C/D:** `GetBlockSubsidy` / `bad-cb-amount`.
- **Proven false as a goal:** `total_oru_supply ≤ ORU_MAX_SUPPLY` — tail emission is by design.

### Script classification

- **A (prose):** `RIGOR_SCRIPT_TYPE_REGISTRY.md` (Witness v0/v1, CVG2, RES2, rejects).
- **B:** Opaque `out_script : list nat` for pooling; asset class is the `out_asset` tag (`we_do_not_claim_script_derives_asset`). **Shape** (non-empty `IsAllowedScript` / spend-CVG2 / optional PCORB sizes) is an `apply_tx` conjunct (`ScriptShape.v`). `IsResScript` vs tag is not.
- **C/D:** `IsAllowedScript`, `IsResScript`, interpreter tests.

---

## Section 3 — Coq model assessment

### Completeness

**Strong** for: hash, σ layout, Convergence_Root, draw relation, window exclusion, RES/ORU schedules, replay supply scalars, honest non-equivalence catalogs.

**Incomplete** for: `IsResScript` vs `out_asset`, coinbase commitment chain, mint witness *semantics*, maturity, network. Dual-pool and cap-overshoot reject are **closed in the model** (not C++ refinement). Script *shape* is a conjunct of `apply_tx` (`ScriptShape.v`). Residual opaques (`out_script` as asset class, `cm_draw_won`, DCR coinbase bind, `rs_res_supply` scalar) are declared as non-claims in `OpaqueBoundary.v` / `DrawCandidateRoot.v` §9.

### Abstraction level

Appropriate for a monetary UTXO chain: byte-level where forks historically hide (endianness, concat-not-Merkle, σ offsets); abstract where C++ is a large interpreter (scripts, coin view). The residual risk is **tags instead of scripts** for asset class.

### Maintainability

- Kernel / ReplayReorgCore / `ResSchedule.v` / `Selection.v`: readable, pinned numerics fail the build if changed.
- Phase-4F/5: large Catalog/Audit duplication. Verdict: do not read `*Certification.v` as correctness proofs. `READY_FOR_RELEASE` means artifacts consolidated with `RV_Partial`, **not** Coq = C++.

### Correspondence strength

Finite and **honest**. **215** `vm_compute` lemmas (187 kernel + 28 NegativeCoreHarness) are the load-bearing L1↔L2 lock. Extra SHA-256-heavy samples are Python/C++ only (cost). No universally quantified Coq=C++.

### Missing lemmas / invariants / obligations (priority)

1. Dual-pool on every `apply_tx` — **done** (`apply_block_res_invariant`).
2. Connect reject on RES cap overshoot — **done** (`valid_res_cap`).
3. Eligible-subset root function + order independence — **done** (`DrawCandidateRoot.v`).
4. `IsResScript` / `IsConvergenceScript` byte predicates — **partial** (`ScriptMintWitness.v` + `InvalidCvg.v` + `ScriptShape.v` shape lift; `IsResScript` vs tag not on `apply_tx`).
5. Open named obligations remain: PD-4F-OB-02 (oracle/C++ bridge), PD-4F-CPP-01 (C++ equivalence; **non-closable** in frozen architecture), PD-4F-OB-03.
6. Probabilistic fairness — keep OPEN; do not fake with `Pr := 1`.
7. Residual opaque-boundary declaration — **done** (`OpaqueBoundary.v` / `DrawCandidateRoot.v` §9).

### Recommended Coq expansion order

Same as Roadmap Phase 2: dual-pool connect → cap reject → `draw_candidate_root` (landed) → script predicates → mint composition. Do not grow Phase-5 placeholders.

---

## Section 4 — Golden vector assessment

| Set | Count | Role |
|-----|-------|------|
| Shared JSON | **422** | Python oracle + generated C++ |
| Coq-enforced | **215** | Build-breaking `vm_compute` |
| Coq-enforced composition | WindowExclusion 59, Inequality 25, DCR 36, NegativeCore 16, InvalidCvg 14, ScriptMintNeg 12, EdgeCross 12, SigmaEncoder 10, CR 9, RunSelection 7, SigmaPoW01 5, HashKernel 2 | Cheap / partial SHA families |

**Coverage quality:** High for **positive** draw-kernel bytes (σ, C, inequality, already-paid filter, encoder). SupplyScaffold (5) is now checked against the RFC-017 schedule (not doc-only). FIPS padding is Coq-locked in `CanonicalHashKernel.v` rather than all 8 JSON rows in the harness.

**Important missing categories:**

| Category | Present? | Need |
|----------|----------|------|
| Invalid Convergence (committed C ≠ recomputed) | Yes (`ICVG-006`) | Coq-enforced; coinbase *insert* of C is still C++ |
| Duplicate transactions / duplicate txids in F | Yes (`DCR-029`, `ICVG-011`) | DCR concat keeps copies; C of `[a,a]` is not N&lt;2 zeros |
| Malformed roots | Yes (`ICVG-007`…`010`, `ICVG-012`/`013`) | Unsorted/truncated/extra-byte concat; length ≠ 32; N&lt;2 DCR-as-C claims |
| Asset mixing | `NegativeCore` dual-pool rows | Script-derived asset class still C++ |
| RES mint boundary | `NegativeCore` H-1 / cap / without-winner | Owner/PCORB still C++ |
| Script classification failures | Abstract CVG2 + shape lift on `apply_tx`; `IsResScript` vs tag C++ / vectors | P1-1 remainder: asset-from-script, P2WSH |

**Future expansion:** prefer **reject-path** families over more SHA-256-heavy happy-path samples. Keep the documented split: not every vector needs Coq `vm_compute`.

---

## Section 5 — Monetary rule verification

### ORU

| Rule | Formally guaranteed (model) | Correspondence |
|------|-----------------------------|----------------|
| Initial subsidy 1000 ORU | Definition + lemmas (`cb_ORU_INITIAL_SUBSIDY`) | Must stay aligned with `rigor_consensus_lock.h` |
| Halving interval 210000 | Definition | Same |
| Max 3 shifts, then tail 125 ORU | `cb_oru_shift_bounds`; tail is why **no** ORU cap theorem | C++ `GetBlockSubsidy` |
| Per-block bound | `apply_block_oru_bounded` | `bad-cb-amount` |
| No absolute supply cap | Explicitly **not** a theorem (false) | By design |

### RES

| Rule | Formally guaranteed (model) | Not guaranteed |
|------|-----------------------------|----------------|
| Cap 21M at 6 decimals | `RES_MAX_SUPPLY`; `max_issuance_within_cap` | C++ identity |
| Schedule W(k) = floor genesis·(k+1)^{-3/2} to epoch 250 | `schedule_sum_value = 99450437`; cutoff is a rule | Pointwise C++/Python compared **outside** Coq (252 epochs) |
| Mint amount from **header** height | `cb_res_mint_amount` | Reorg-safe in model |
| Convergence dependency | Abstract `cm_draw_won` (not Selection / owner / PCORB); H-1: W(h)=0 ⇒ mint count 0 | Owner/PCORB/σ inequality not composed |
| Future policy flexibility | Changing E or W(k) without updating pins **fails Coq** | That is the intended rigidity; flexibility is RFC-level, not a Coq parameter |

**Which monetary rules are formally guaranteed?** In the **model**: RES cannot exceed the finite-sum ceiling under one mint per block; overshoot is `apply_block = None`; ordinary RES net is conserved at block level (`apply_block_res_invariant`); ORU cannot mint more than the subsidy schedule per block; H-1 zero-reward obligation. **Not** guaranteed: mint authorization matching `ConnectEvaluateResMintTx`; script-derived asset class; shipping-binary equality.

---

## Section 6 — Script architecture verification

Minimal script architecture is a **documented security prerequisite** for verification tractability (`docs/security/minimal-script-architecture.md`). It is **not** itself a Coq development.

| Rule | Specified | Coq | Tests |
|------|-----------|-----|-------|
| Witness v0 P2WPKH / P2WSH / native 32-byte Ed25519 | Registry | No | C++ |
| secp256k1 CHECKSIG disabled | Registry | No | C++ |
| Witness v1 not BIP341 | Registry / whitepaper | No | C++ |
| CVG2 unspendable ORU-domain | Registry | Opaque script bytes | C++ `bad-txns-spend-convergence` |
| RES = v1 / 32 / not CVG2 | Registry | `out_asset` tag | `IsResScript` |
| Reject P2PKH/P2SH/Taproot/etc. | Registry | No | `bad-rigor-script-type` |

**Conclusion:** script *restrictions* are formally **specified in documentation** and **enforced in C++**. They are **not** mechanically verified in Coq. Dual-pool Coq work assumes asset tags. That is the largest remaining abstraction gap between “minimal script” as a verification story and the actual Coq model.

---

## Section 7 — Self-proving blockchain verification

Intended chain (whitepaper):

```
Transaction
  → script / CVG2 commitment
  → Convergence_Root / draw_candidate_root
  → coinbase commitment
  → coinbase txid
  → merkle root (tx tree)
  → block header
  → PoW / OpuHash
```

| Link | Formally specified | Mechanically checked (Coq) | Runtime verified |
|------|--------------------|----------------------------|------------------|
| Tx → CVG2 / script | Yes (registry, RFCs) | No | Yes (C++) |
| Eligible set → Convergence_Root | Yes | Yes (`ConvergeRoot`, order independence, N&lt;2) | Yes (vectors + σ/root CLI) |
| Eligible subset → `draw_candidate_root` | Yes (Python + paper) | Yes (`DrawCandidateRoot.v`; properties, not refinement) | Yes (C++ / RPC) |
| Roots → coinbase commitment | Yes (RFC commitment layout) | No | Yes (ConnectBlock) |
| Coinbase txid → tx Merkle root | Bitcoin-derived | No (not a RIGOR Coq theory) | Yes (inherited Core) |
| Header → PoW hash | Yes (OpuHash / Core pow) | No | Yes |

Coq’s self-proving fragment is **C and σ and the draw function**. The rest of the chain is **runtime validation**. Fingerprint library (`Hash256(C‖winner‖σ‖height‖…)`) is observational, not a consensus rule, and not a Coq theorem about headers.

---

## Section 8 — Gaps

See [`RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md`](./RIGOR_FORMAL_VERIFICATION_GAP_ANALYSIS.md). Summary:

| Class | Highest items |
|-------|----------------|
| P0 | Negative remainder (P1-1 script families); `draw_candidate_root` model + dual-pool/cap connect + InvalidCvg: **done** |
| P1 | Script predicates; mint composition; extra cheap Coq-enforced rows; coin-view tests |
| P2 | CI assumptions; catalog freeze; PROOF-034 execution; generated-data guards |
| P3 | README G-1/G-2/encoder/tie-break: **done** (TASK-FV-DOCS-FREEZE-SYNC-001); Verdict counts (do not silently rewrite); site index |

---

## Section 9 — Industry comparison (methodology only)

Comparison is about **how** verification is done, not product claims.

### RIGOR vs typical Bitcoin methodology

Bitcoin Core’s consensus assurance is **implementation testing**, review, and historical replay. There is no official Coq model of Bitcoin monetary issuance that the node is proved to refine. RIGOR **adds** a byte-level Coq model of *its* draw, roots, and dual-asset schedules, plus a published differential gate. RIGOR does **not** exceed Bitcoin on binary refinement: both ship C++ nodes that are not Coq extracts. RIGOR is more explicit about the bound (`equivalence_claimed = false`).

### RIGOR vs smart-contract chains

Platforms with heavy VMs (EVM, general WASM) typically verify **contract languages or VMs** (KEVM, Mi-Cho-Coq, etc.), not a small native dual-asset mint schedule. RIGOR’s bet is the opposite: **shrink the script surface** so monetary rules can be theorems. That bet is only half-landed: script restrictions are C++ + docs; Coq still uses asset tags. Smart-contract FV does not automatically yield stronger *monetary issuance* theorems than RIGOR’s `max_issuance_within_cap`.

### RIGOR vs projects that advertise formal verification

| Pattern | Example class | RIGOR |
|---------|---------------|-------|
| Verified kernel / compiler, then run that artifact | seL4, CompCert | **Not** this: `rigord` is not extracted Coq |
| Spec + model + tests, honest gap | Careful protocol Coq developments | **This** (kernel + catalogs of non-closure) |
| Model-only “verified blockchain” marketing | Collapsing layers | **Forbidden** by freeze flags and Phase-5 impossibility catalogs |

RIGOR’s distinctive methodological choice is to **formalize non-equivalence** (Phase-5 `DCS_Impossible`, vacuous placeholders labelled as such) rather than leave the gap implicit. That is closer to good verification engineering than to a completed verified implementation.

---

## Section 10 — Roadmap

See [`RIGOR_FORMAL_VERIFICATION_ROADMAP.md`](./RIGOR_FORMAL_VERIFICATION_ROADMAP.md).

| Phase | Content |
|-------|---------|
| 1 RC0 | Freeze, green gate, this assessment; no scope expansion |
| 2 Proof coverage | Dual-pool connect, cap reject, `draw_candidate_root`: **landed**; residual opaques **declared**; script **shape** on `apply_tx`; `IsResScript` vs tag / `cm_draw_won` wiring: open |
| 3 Correspondence | NegativeCore + DCR negatives + InvalidCvg + ScriptMintNeg: **landed**; P1-1 asset-from-script / coin-view: open |
| 4 Continuous | CI, generated-artifact guards, optional PROOF-034 archive |
| — | **No refinement phase.** L2↔L3 identity: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) (Path A = this roadmap; B2/C not funded) |

---

## Proven / modeled / tested / planned

| | |
|--|--|
| **Proven** | Model theorems listed in §2 of the Verdict plus RFC-017 `ResSchedule` / Selection pins / mint obligation / dual-pool on `apply_tx` / `apply_block` / cap-overshoot reject / `draw_candidate_root` e2e |
| **Modeled** | Replay UTXO, ConnectBlock scaffold, Phase-4F traces, Phase-5 status |
| **Tested** | 422 vectors, CLIs, C++ consensus tests, fuzzers |
| **Planned** | Gap P0–P3 and roadmap phases; PROOF-034 live run |

---

## Formal verification maturity

| Axis | Assessment |
|------|------------|
| Model (draw, hash, schedules) | **High** |
| Model (script, full connect, mint witness) | **Low–medium** |
| Correspondence | **Finite / partial** |
| Binary identity | **Not established** |
| Honesty / boundary documentation | **High** |
| RC0 adequacy under honest reading | **Sufficient** |
| “Mathematically verified monetary protocol” as binary theorem | **Not reached** |

---

## Recommended next actions

1. Keep freeze maintenance: green `run_differential_gate.sh`; do not flip honesty flags.
2. Remaining after P0-4 Convergence/Root: P1-1 **shape** is on `apply_tx`; `IsResScript` vs tag and coinbase bind of C/DCR remain C++.
3. README G-1/G-2/encoder/tie-break repaired under TASK-FV-DOCS-FREEZE-SYNC-001. RC0 FV freeze: `RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` §0 (`TASK-FV-FREEZE-RC0-READINESS-001`; 414/207).
4. Do not schedule Coq⇄C++ refinement or production extracted hash. Feasibility of L2↔L3 identity: [`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](./RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) (Path A maintained; B2/C not funded; `DCS_Impossible` and honesty flags unchanged).

---

## Sources (inspected, not modified)

`formal_verification/README.md`, `coq/_CoqProject`, `Selection.v`, `ResSchedule.v`, `ConvergeRoot.v`, `DrawCandidateRoot.v`, `InvalidCvg.v`, `ScriptMintWitness.v`, `SigmaPoW01.v`, `OrderIndependence.v`, `WindowExclusion.v`, `ReplayReorgCore/*`, `Kernel/GoldenHarness.v`, `phase3_golden_vectors.json` (422), `scripts/run_differential_gate.sh`, `docs/verification/FORMAL-VERIFICATION-*.md`, `docs/archive/proof/PROOF-034-*.md`, `docs/security/RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md`, script registry / minimal-script docs, whitepaper Ch. 7–8.
