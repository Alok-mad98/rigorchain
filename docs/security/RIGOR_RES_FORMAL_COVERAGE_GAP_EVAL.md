# RES Formal Coverage Gap — Engineering Evaluation

**Follows:** [RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md](./RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md) Finding L-1  
**Date:** 2026-08-19  
**Layer:** Engineering evaluation; P1 close-out is documentation of a freeze-exception Coq increment  
**Consensus impact:** **NONE** — no C++ consensus, RFC, wallet, or miner files were modified in this pass  
**`equivalence_claimed`:** remains **`false`**

**P1 close-out (2026-08-19):** winner-must-mint + H-1 and `res_in ≥ res_out` are **YES** as *model* theorems (`ResMintObligation.v`, `DualPoolConservation.v`). They are not a C++ refinement. Accepted mismatches are §9.

This file was the go / no-go analysis for a follow-up Coq increment covering the four
properties the P0 audit listed as **C++-only**. P1 implemented two of them as
model theorems under a freeze exception. It does not reopen correspondence, and
it does not flip honesty flags.

Canonical honesty line (unchanged):

> The Coq corpus is a strongly verified **model**, not a C++ refinement proof.

---

## Verdict

| Question | Answer |
|----------|--------|
| Is a Coq extension worth a new task? | **Yes, but only a narrow increment.** |
| Should `res_in ≥ res_out` be in the next Coq phase? | **Done (P1 model).** Transfer-path anti-inflation constraint is now a Coq predicate; not C++ refinement. |
| Should the full Owner / PCORB / domain mint predicate move into Coq now? | **No.** Equivalent assurance is cheaper via structured C++ fuzzing plus the existing unit / solo-security matrix. |
| Does any option flip `equivalence_claimed`? | **No.** P1 landed as model theorems. Correspondence remains unproved. |

**Recommended scope (P1 Coq landed; remainder if funded)**

1. Coq P1 — **done (model):** winner-must-mint obligation (including H-1 / `W(h)=0`) **and** transfer conservation `res_in ≥ res_out`. Honesty flags unchanged. See §9.
2. C++ P1 parallel (≈ 1–2 engineer-weeks): structured libFuzzer targets for dual-pool `CheckTxInputs` and `ConnectEvaluateResMintTx` reject reasons.
3. Coq P2 (optional, after P1 Coin fields exist): 100-block maturity.
4. Owner / PCORB / domain: **keep C++ + fuzz**; do not start a mint-pipeline Coq theory this cycle.

Do **not** fund a “close all four Coq gaps” project. That collapses a cheap
block-validity lemma, a useful UTXO-pool lemma, a spendability lemma, and a
near-refinement of `ConnectEvaluateResMintTx` into one task. The last item
dominates cost and drift.

---

## 1. What the P0 audit actually left open

Audit §7 listed six missing formal properties. This evaluation covers the four
the task named. The other two (production coinbase RES = 0; post-schedule
accept-without-mint) are absorbed into winner-must-mint / H-1 below.

| # | Property | C++ site | Current Coq | C++ tests today |
|---|----------|----------|-------------|-----------------|
| 1 | 100-block maturity | `AccumulateTxInputPools` in `tx_verify.cpp`; `Coin::RequiresMaturity` | **None.** `TxOut` has value + opaque script only — no `nHeight`, no `COIN_FLAG_RES_MINT` | `res_orum_tests/res_mint_coin_immature_until_coinbase_maturity`; `feature_res_maturity.py` |
| 2 | Winner-must-mint | `ConnectBlock` in `validation.cpp` (`bad-rigor-res-winner-mismatch`, `bad-rigor-res-without-winner`, H-1 amount gate) | **YES (model).** `valid_res_mint_obligation` on `valid_block_connect`; H-1 in `ResMintObligation.v`. `cm_draw_won` / `cm_res_mint_count` stay abstract. Cap overshoot still zeros delta (accepted mismatch, §9) | `rfc017_exhausted_schedule_rejects_mint`; `feature_rigor_convergence_rng_consensus.py` |
| 3 | Owner / PCORB / domain | `ConnectEvaluateResMintTx` (`rigor_res.cpp`); `CheckResRewardOwner` (`res_owner.cpp`); `GetTransactionInputAsset` | Abstract `cm_draw_won : bool`. Draw kernel / σ / PCORB hashes exist as separate theories and golden vectors, **not** composed into a mint-authorization theorem | `res_proof_math_tests` (24); solo-security V-1…V-5, A-2; `convergence_consensus_tests` owner pins |
| 4 | `res_in ≥ res_out` | `CheckTxInputs_RES_DualPool` (`bad-txns-res-in-belowout`); mint exception is `CheckOruFundedResMintShape` | **YES (model).** `dual_pool_conservation` in `DualPoolConservation.v`. Predicate is **not** a conjunct of `apply_tx` / `apply_block` (accepted mismatch, §9) | `res_spend_cannot_create_additional_res`; `mixed_inputs_cannot_mint_extra_res`; `ordinary_oru_payment_cannot_create_res` |

`equivalence_claimed = false` is already honest about this: even the **YES**
rows in the audit (cap, schedule, reorg rollback) are model theorems.

---

## 2. The model gap that matters for inflation

`ReplayState.rs_res_supply` is **not** derived from the UTXO set. Supply
conservation lemmas quantify over `protocol_res_mint_delta`. That is the right
shape for “at most one scheduled mint per connected block,” and it is already
proved (`max_issuance_within_cap`, `res_mint_respects_cap`).

It is the **wrong** shape for “an ordinary transaction cannot create RES.”
P1 added `dual_pool_conservation` as a predicate on `valid_non_mint_tx`.
If `CheckTxInputs_RES_DualPool` were deleted tomorrow, the Coq *supply-scalar*
theorems would still type-check, because that predicate is not wired into
`apply_block` (accepted mismatch, §9.1 #3). Cap lemmas bound the mint
channel; the transfer inequality now exists in the model, but C++ remains
the live gate.

That is why `res_in ≥ res_out` was not a cosmetic coverage hole: the
mint-only supply story relied on a transfer assumption the model did not
state. P1 states it as a predicate; C++ remains the live gate (§9.1 #3).

Winner-must-mint is the complementary hole on the mint channel. P1 closed
the *statement*: `valid_block_connect` now carries `valid_res_mint_obligation`
(winner ∧ `W(h)>0` ⇒ mint count 1; H-1: `W(h)=0` ⇒ count 0 even if a winner
exists). The remaining gap is correspondence: `cm_res_mint_count` is an
abstract field, not a count of scored body txs (accepted, §9).

Maturity is spendability, not issuance. A premature spend cannot inflate
RES; it can only move an immature coin. Owner / PCORB / domain is the
authorization pipeline that decides **whether** `cm_draw_won` may be treated
as a scored mint. Coq currently **assumes** that boolean.

```
Already in Coq (incl. P1)     Not in Coq
─────────────────────────     ─────────────────────────────────────────
W(h) schedule                 Coin height / RES_MINT flag / 100-block gate
Σ W(k) · 210000 ≤ cap         ORU-only domain ∧ F membership ∧ owner program
delta ∈ {0, W(h)}             dual-pool as a conjunct of apply_block
reorg rolls the scalar        trigger-underflow (N < MIN_TXS)
winner ∧ W(h)>0 ⇒ count = 1   ConnectEvaluateResMintTx / PCORB / owner
no winner ⇒ count = 0
W(h)=0 ⇒ count = 0 (H-1)
res_in ≥ res_out (predicate)
```

---

## 3. Feasibility, cost, and drift — per property

Estimates assume one engineer who already knows this Coq corpus and the
ConnectBlock mint path. They are for **model theorems** (`Qed`, zero new
axioms), not a C++ refinement. Adding correspondence would be a separate
multi-month project and is out of scope.

### 3.1 Winner-must-mint obligation — Coq P1

**Map to the model.** Extend `ConnectMintSpec` (or the block body) with a
mint-count / mint-present flag, and add a conjunct to `valid_block_connect`:

```text
res_mint_authorized := cb_res_mint_amount b > 0
winner              := cm_draw_won (blk_mint b)
mint_count          := scored mint txs in blk_txs   (or a dedicated field)

authorized ∧ winner  →  mint_count = 1
¬winner              →  mint_count = 0
¬authorized          →  mint_count = 0
```

The third line is H-1 / RFC-017 §4.4. `cb_res_mint_delta_zero_when_amount_zero`
already gives the supply half; the missing half is **block acceptance**.

**Difficulty:** Low–medium. The arithmetic is already in `ConnectBlockMint.v`.
The work is stating validity, not inventing a new kernel. A residual model
bug to document (do not silently “fix” as if it were C++): when the cap would
be exceeded the model currently **zeros the delta** instead of rejecting the
block. Production rejects (`bad-rigor-res-max-supply`). Leave that as a
named residual unless the same task is explicitly widened.

**Effort:** 3–5 engineer-days. Files: `ConnectBlockMint.v`,
`StateTransition.v`, a short `ResMintObligation.v`, `Print Assumptions`
closed.

**Drift:** Medium, and **that is the point.** H-1 was exactly a Coq-invisible
change to this coupling. Pinning the three-way case split makes the next
schedule-end mistake a proof failure. Maintenance cost is a few lemmas when
`W(h)` gating changes — rare, consensus-critical, worth the sync.

**Do not** encode owner / PCORB here. Keep `cm_draw_won` abstract.

### 3.2 `res_in ≥ res_out` — Coq P1 (same increment)

**Map to the model.** `TxOut` must carry enough to classify ORU vs RES
(script tag or an explicit `AssetType`). Dual-pool accumulation mirrors
`AccumulateTxInputPools` + `CheckTxInputs_RES_DualPool` without a script
interpreter:

```text
valid_non_mint_tx tx utxo →
  res_sum(inputs) ≥ res_sum(outputs)
  ∧  no RES→ORU conversion
  ∧  no ORU→RES except the mint-shape exception (left abstract or
     restricted to “exactly one RES output of W(h)”, still not a draw proof)
```

A stronger, better theorem once Coin fields exist:

```text
UTXO_RES(apply_tx) ≤ UTXO_RES(before)     (* ordinary *)
UTXO_RES(apply_mint) = UTXO_RES(before) + W(h)
```

That would finally connect `rs_res_supply` to the UTXO projection, which
today they are free to diverge.

**Difficulty:** Medium. The inequality is one line in C++. The Coq cost is
the dual-pool / mint-exception structure and not accidentally proving a
theorem that forbids the legitimate mint.

**Effort:** 8–12 engineer-days, including tests that a mint **may** increase
RES and that mixed ORU+RES inputs **may not**.

**Drift:** Low–medium. The inequality is stable. RFC-018’s 50% ORU fee burn
is the ORU leg and should stay out of this theorem. Dual-pool fee rules
(`bad-txns-oru-fee-required`) can be stated or left as C++-only; they are
not inflation.

**Inflation value:** Highest of the four for the transfer path. Cap lemmas
already bound the mint path.

### 3.3 100-block maturity — Coq P2

**Map to the model.** `Coin` needs `nHeight` and a maturity bit
(`IsCoinBase \/ IsResMint`). Predicate:

```text
RequiresMaturity c → spend_height - coin_height ≥ nCoinbaseMaturity
```

Flag-setting on mint connect and undo restore are the reorg half
(`ApplyBodyResMintFlag`, dummy undo byte bit 0).

**Difficulty:** Medium. The predicate is trivial; the state extension is
the work. That extension is **shared infrastructure** with §3.2’s UTXO-sum
theorem. If P1 already adds Coin height/flags, P2 is a few lemmas. If P1
classifies assets without Coin metadata, P2 pays the extension later.

**Effort:** 5–8 engineer-days standalone; **2–3 days** if P1 already extended
`Coin`.

**Drift:** Low. `nCoinbaseMaturity = 100` is locked with ORU coinbase.
Wallet UI `+1` is non-consensus (audit L-2) and must not enter the spec.

**Monetary-policy value:** Low. Not an issuance hole. Keep C++
(`res_orum_tests` + `feature_res_maturity.py`) as primary; Coq is defense-in-depth
on spendability / reorg.

### 3.4 Owner / PCORB / domain — **not** Coq this cycle

This is not one predicate. It is the composition of:

| C++ step | Reject |
|----------|--------|
| `GetTransactionInputAsset` = ORU (RES-domain returns without scoring) | `bad-rigor-res-mint-domain` / silent non-score |
| `ParseConvergeProof` | `bad-rigor-res-witness` |
| height / bind / nBits / F membership / binding | named `bad-rigor-res-*` / `bad-sigma-convergence-not-in-convergence-root-set` |
| `VerifyConvergeProof` vs frozen `T` | `bad-rigor-res-proof` |
| `CheckResRewardOwner` (exactly one v1/32 program = declared 32 bytes) | `bad-rigor-res-owner-mismatch` |
| `n_res == 1` and amount `W(h)` | `bad-rigor-res-amount` |
| `EnforceResMintUnique` | `bad-rigor-multiple-res` |

Coq already has pieces of this **as separate theories** (Selection, Sigma,
PCORB hash golden vectors). A theorem that a C++-shaped mint witness is the
**only** way to set `cm_draw_won` is a refinement of `ConnectEvaluateResMintTx`.
The freeze already records that correspondence is unproved
(`PD-4F-CPP-01` open; `equivalence_claimed = false`).

**Difficulty:** High. Witness stacks, script classification (`IsResScript` vs
CVG2), OP_RETURN owner encoding (RFC-009 exact bytes), coin-view domain, and
F as a set — not a bool.

**Effort:** 6–10 engineer-weeks for a **model-level** composition that still
does not talk to C++. Correspondence / extraction is months and is a
different project.

**Drift:** **High.** SECURITY-001-A5 (owner), A-7 (PCORB claim path removed),
and A-8 (σ chain binding) all lived on this path. A Coq mint pipeline would
have to move every time that code moves, while the C++ tests already pin the
reject reasons by name. That is the definition of model/C++ desynchronization.

**Do not** put this in the next Coq increment.

---

## 4. Priority

| Rank | Property | Why this rank | Action |
|------|----------|---------------|--------|
| **P1** | Winner-must-mint + H-1 | Cheapest Coq win; this coupling already caused a High finding; audit “before mainnet” item 1 | **Done (model).** See §9 |
| **P1** | `res_in ≥ res_out` | Only missing anti-inflation constraint on the transfer path; cap lemmas do not imply it | **Done (model).** See §9 |
| **P2** | 100-block maturity | Spendability / reorg, not issuance; cheap after Coin fields exist | Coq after P1, or remain C++ |
| **P3** | Owner / PCORB / domain | Cost and drift dominate; C++ already names every reject | **Fuzz + tests, not Coq** |

**Answer to “should `res_in ≥ res_out` go into the next Coq phase?”**

Yes. Rank it **with** winner-must-mint, not behind maturity. The two
theorems close different channels:

- winner-must-mint: mint channel **liveness / coupling** (and H-1 termination)
- `res_in ≥ res_out`: transfer channel **non-issuance**

Doing only winner-must-mint would leave the larger conceptual hole (Coq
cannot see transfer inflation) unfixed. Doing only conservation would leave
the H-1-class validity gap unfixed. Neither is a substitute for the cap
identity, which is already proved.

Maturity should not jump the queue: it does not mint.

---

## 5. Alternatives where Coq is the wrong tool

### 5.1 What exists today (and what does not)

| Tool | Covers these four properties? |
|------|-------------------------------|
| `res_orum_tests` / `res_proof_math_tests` / `convergence_consensus_tests` | Yes, as named unit pins — **primary** for 1–4 |
| `feature_res_maturity.py`, solo-security, RNG winner-mismatch functional | Yes, chain-level for maturity and winner coupling |
| `ReplayReorgCore` Coq | Cap / schedule / scalar reorg; P1 obligation + dual-pool **predicates** (not C++ refinement) |
| RFC-011 differential fuzzer | Draw / σ / hash conformance — **not** `CheckTxInputs` |
| Inherited libFuzzer `transaction` | `CheckTransaction` + standardness; **no coin view** |
| Inherited libFuzzer `coins_view` | **Does** call `CheckTxInputs` on random coins; **not** structured for Witness v1 RES, dual-pool fees, mint shape, or owner mismatch |
| KLEE / CBMC | **Not used** in this tree for consensus |

There is **no** RES-specific fuzz target. That is the cheapest coverage
upgrade for property 3, and a useful complement for 1 and 4.

### 5.2 Structured libFuzzer (recommended equivalent for property 3)

Add two targets, in the existing `core/src/test/fuzz/` harness:

1. **`res_checktxinputs`** — build a small `CCoinsViewCache` with ORU / RES /
   CVG2 / immature-mint coins; mutate vin/vout amounts and script kinds;
   assert:
   - non-mint RES domain never accepts `res_out > res_in`
   - RES→ORU conversion is rejected
   - ordinary ORU payment cannot create RES (`mixed-asset-output` unless mint shape)
   - immature `COIN_FLAG_RES_MINT` cannot spend before depth 100
2. **`res_mint_eval`** — fix a parent `ConvergenceDrawSet` + winner program;
   mutate PCORB witness, payout script, domain (inject a RES prevout), amount,
   height, bind hash; assert `ConnectEvaluateResMintTx` either scores exactly
   one honest shape or returns a **named** reject (`bad-rigor-res-owner-mismatch`,
   `bad-rigor-res-mint-domain`, `bad-sigma-convergence-not-in-convergence-root-set`,
   …). Never scores a redirected owner.

This is **not** as strong as a Coq ∀-quantified theorem, but it is stronger
than the current unit matrix on the high-dimensional witness space, and it
runs against **production C++**, which Coq cannot. Effort: 1–2 engineer-weeks
including a seed corpus of the existing BOOST cases.

Property-based BOOST (deterministic generators over amounts / programs) is
a still-cheaper subset and should land first if fuzz builds are not in CI.

### 5.3 Symbolic execution

CBMC on `CheckResRewardOwner` or `CheckTxInputs_RES_DualPool` is feasible:
bounded vin/vout counts, concrete script classifiers. That can prove the
inequality and the owner byte-compare for a bound (e.g. ≤ 4 inputs, ≤ 4
outputs). It does **not** scale to `VerifyConvergeProof` / SHA-256d / F
membership.

KLEE on the full mint evaluator fights hashing and is a research project.

**Recommendation:** do not open a symbolic-execution workstream. Use CBMC
only if a future task wants a machine-checked bound on the dual-pool
function **in C++**, as a complement to Coq, not a replacement for P1.

### 5.4 Equivalence of assurance (honest)

| Property | Coq ∀ theorem | Structured fuzz on C++ | Unit + functional |
|----------|---------------|------------------------|-------------------|
| Winner-must-mint | **Best** (3-way case split is small and H-1-class) | Weak (block-level state is heavy to fuzz) | Good, already present |
| `res_in ≥ res_out` | **Best** (closes the model assumption) | **Strong** (coins_view-style, structured) | Good on named cases, weak on combinations |
| Maturity | Nice-to-have | Strong (height × flag mutations) | Good (`feature_res_maturity.py`) |
| Owner / PCORB / domain | Expensive, high drift, still not C++ | **Best cost/assurance this cycle** | Good on named forgeries, weak on witness entropy |

“Equivalent security” is not a slogan. For property 3, fuzz-on-C++ remains the
better next dollar. Properties 2 and 4 now have model theorems; they are still
not C++ correspondence (`equivalence_claimed = false`).

---

## 6. Maintenance / desynchronization risk

The freeze (`docs/verification/FORMAL-VERIFICATION-STATUS.md`) forbids new
Coq theories without a new task. That is correct: every new definition is
another object that can silently disagree with `core/src/**`.

| Option | Drift effect |
|--------|----------------|
| Do nothing | Coq keeps proving a mint-only scalar world. C++ remains the real gate. Honest, but H-1-class coupling stays Coq-invisible. |
| Narrow P1 (obligation + conservation) | **Reduces** drift on the two monetary couplings that matter. Coin asset tags must be documented as a model of `AssetTypeFromScriptPubKey`, not a new asset theory. |
| Full mint pipeline in Coq | **Increases** drift. Duplicate `ParseConvergeProof` / owner encoding / domain. Next A-7-style removal would require a Coq rewrite the C++ tests would already catch. |
| Fuzz targets on C++ | **Zero Coq drift.** Must be updated when reject **names** change — same as BOOST. |

Keep `equivalence_claimed = false` after the Coq P1 merge. Do not describe P1
as “the mint path is formally verified.” Describe it as “the model now
states winner coupling and transfer conservation.” Accepted mismatches: §9.

---

## 7. Proposed task breakdown (if this eval is accepted)

**Task A — Coq P1 (new formal-verification task, freeze exception)** — **done (model).**  
Scope landed: `valid_res_mint_obligation` including H-1; dual-pool conservation
for non-mint txs; `AssetKind` on `TxOut`.  
Out of scope (kept): PCORB parse, σ inequality, owner encoding, C++ refinement,
flipping honesty flags.  
Gate: `coqc` + `coqchk` + `Print Assumptions` closed. Honesty flags unchanged.

**Task B — C++ structured fuzz (security test task, consensus unchanged)**  
Scope: `res_checktxinputs` + `res_mint_eval` (or property-based BOOST if
fuzz is not in the pre-mainnet gate). Seed from existing unit cases.  
Out of scope: changing reject reasons or mint rules.

**Task C — Coq P2 (optional)**  
Maturity lemmas on the Coin fields Task A introduced. Skip if Task A does
not extend Coin.

**Human approval still required** (from the P0 audit) before any change to
`W(h)`, `T`, cap, uniqueness, owner binding, or maturity constants.

---

## 8. What this evaluation does *not* claim

- That C++ is unsafe without these Coq lemmas. The P0 audit result remains
  **PASS (RC0)** with L-1 accepted as Low.
- That P1 made `equivalence_claimed` true. It did not.
- That fuzzing proves absence of mint forgeries; it raises coverage on the
  production predicate.
- That production coinbase RES = 0 needs its own Coq file; pin it as a
  conjunct of obligation / coinbase layout if a later task wants it.

---

## 9. Accepted Model Abstractions & Mismatches (P1 Cycle)

P1 proved `valid_res_mint_obligation` (including H-1) and
`dual_pool_conservation` as **model** theorems. A read-only cross-check
against `ConnectBlock` and `CheckTxInputs` classified remaining gaps as
three **MISMATCH**es and one named **ABSTRACT**. None of them is a
production inflation path.

`equivalence_claimed` remains **false**.

These mismatches make the Coq model wider than C++. Since C++ strictly
rejects these cases, there is no production inflation risk. Full refinement
is deferred to a future cycle.

### 9.1 MISMATCH (3)

| # | Gap | Coq | C++ | Why accepted |
|---|-----|-----|-----|----------------|
| 1 | Over-cap delta zeroing | `valid_block_connect` does not inspect the 21M cap. If `cm_draw_won` and `W(h)>0`, the block still connects and `cb_res_mint_delta` becomes **0** | `validation.cpp` ConnectBlock: `res_minted_before > RES_MAX_SUPPLY - res_reward_expected` → `bad-rigor-res-max-supply` (reject; coins view not flushed) | C++ never commits the block. Coq scalar still cannot exceed the cap via the mint channel. Named residual since ConnectBlockMint; P1 did not widen it into a reject |
| 2 | Missing trigger-underflow | No candidate count `N`; obligation does not mention `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` | `validation.cpp`: `N < CONVERGENCE_DRAW_MIN_TXS_FOR_RES && mint_count ≠ 0` → `bad-rigor-res-trigger-underflow` | Extra C++ reject. A mint with \|F\| < 1 cannot connect in production. Out of P1 charter (no draw-set cardinality in this layer) |
| 3 | Dual-pool predicate vs `apply_block` | `dual_pool_conservation` / `valid_non_mint_tx` are lemmas about a predicate. They are **not** conjuncts of `apply_tx` or `valid_block_connect` | Every non-coinbase tx in ConnectBlock runs `Consensus::CheckTxInputs` (`tx_verify.cpp`) | Deleting `CheckTxInputs_RES_DualPool` would still leave the Coq lemmas type-checking. Production still enforces `res_in ≥ res_out` on the live path. Wiring the predicate into `apply_block` is a later model increment, not a C++ patch |

On mismatch 1, the model can also insert mint-shaped outputs into the UTXO
list on an over-cap block that C++ would reject (`apply_txs` is not gated by
the cap). That is the same “wider model” fact: those states are not
reachable under production ConnectBlock.

### 9.2 ABSTRACT (1)

| Item | Coq | C++ | Why accepted |
|------|-----|-----|----------------|
| `cm_res_mint_count` | Dedicated field on `ConnectMintSpec`, attacker-supplied with the abstract block | `res_mint_tx_count` is derived from `res_mint_scores` after `ConnectEvaluateResMintTx` (body txs that actually scored) | P1 charter: do not formalize PCORB / owner / domain / `ConnectEvaluateResMintTx`. `cm_draw_won` stays an abstract bool. A Coq block may set `count = 1` with an empty body; C++ only increments the count when a body tx scored |

Related abstracts (out of P1, not new): `AssetKind` is a tag standing in for
`AssetTypeFromScriptPubKey`, not a script interpreter; mint-shape omits
`ParseConvergeProof`; owner encoding is C++-only.

### 9.3 What P1 did claim

| Property | Formal coverage | Artifact |
|----------|-----------------|----------|
| Winner-must-mint + H-1 | **YES** (model) | `ResMintObligation.v` on `valid_block_connect` |
| `res_in ≥ res_out` | **YES** (model) | `DualPoolConservation.v` `dual_pool_conservation` |

Do not read those YES cells as “ConnectBlock is verified.” They mean the
model now states the same three-way mint obligation and the transfer
inequality. C++ remains the production gate.

---

## Appendix A — Cost sheet (engineer-days)

| Work item | Days (likely) | Days (high) | Coq? | C++ tests? |
|-----------|---------------|-------------|------|------------|
| Winner-must-mint + H-1 validity | 3 | 5 | Yes | Already |
| `res_in ≥ res_out` dual-pool model | 8 | 12 | Yes | Already |
| Shared Coin height / flags / asset (if done in P1) | 3 | 5 | Yes | n/a |
| Maturity lemmas (after Coin fields) | 2 | 3 | Yes | Already |
| Maturity lemmas (without Coin fields) | 5 | 8 | Yes | Already |
| Owner / PCORB / domain model composition | 30 | 50 | Yes | Already |
| Structured fuzz `res_checktxinputs` + `res_mint_eval` | 5 | 10 | No | New |
| CBMC bound on dual-pool (optional) | 5 | 15 | No | New |

Recommended funded bundle: **winner-must-mint + conservation + structured
fuzz** ≈ **16–27 engineer-days**. Full four-property Coq ≈ **50–80 days**
before correspondence, with most of that in property 3.

---

## Appendix B — Code map (evaluation inputs)

| Concern | Artifact |
|---------|----------|
| Audit formal gaps | `docs/security/RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md` §7, L-1 |
| Supply delta model | `formal_verification/coq/ReplayReorgCore/ConnectBlockMint.v` |
| Connect validity (P1 obligation conjunct) | `formal_verification/coq/ReplayReorgCore/StateTransition.v` `valid_block_connect` |
| Winner-must-mint + H-1 | `formal_verification/coq/ReplayReorgCore/ResMintObligation.v` |
| Dual-pool conservation | `formal_verification/coq/ReplayReorgCore/DualPoolConservation.v` |
| UTXO asset tag (no Coin flags / height) | `formal_verification/coq/ReplayReorgCore/ReplayState.v` `TxOut.out_asset` |
| Freeze / no new theories | `docs/verification/FORMAL-VERIFICATION-STATUS.md` |
| Dual-pool + maturity | `core/src/consensus/tx_verify.cpp` |
| Mint evaluator | `core/src/consensus/rigor_res.cpp` `ConnectEvaluateResMintTx` |
| Owner | `core/src/consensus/res_owner.cpp` `CheckResRewardOwner` |
| Winner coupling | `core/src/validation.cpp` ConnectBlock mint-count block |
| Inherited CheckTxInputs fuzz | `core/src/test/fuzz/coins_view.cpp` |
