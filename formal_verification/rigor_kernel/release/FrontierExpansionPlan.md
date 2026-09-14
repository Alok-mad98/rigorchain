# Frontier Expansion Plan — GetBlockProof, multi-tip, maturity, script spendability

Technical design for the next in-cone Category A work. Not a claim
change until each phase lands with a mutant.

This is **not** OP-R1, not C++ `ConnectBlock`, and not a reason to
set `refinement_proved` or close `OpenProperties` entries that stay
open.

The protocol has **no Script VM**. There are four locking shapes
(`T_WITNESS_V0`, `T_WITNESS_V1`, `T_MINT`, `T_CONVERGE`);
null-data and nonstandard are reject classes, not extra scripts.
Phase D is spendability of those four shapes, not an interpreter.

Companion: `FormalizationFrontier.md` (where the cone stops),
`FinalClaimsBoundary.md` (what may be said after a phase),
`OpenProperties.md` (OP-R1 / OP-S1 / OP-C1 stay as written until a
phase actually closes a line).

---

## 0. Why add anything

The cone already has:

- dual-asset rewind (RFC-020 issuance);
- two-tip most-work (Phase A: increments are `GetBlockProof`);
- `money_range` as a predicate, used on transaction *outputs*;
- a script *classifier*, not a spend check.

Those pin rules that are already modelled. They do not pin four
rules a node actually uses to accept or reject a block / spend, and
that two honest implementations can still disagree on:

| Split class | Node rule today | Model today |
|---|---|---|
| Which fork is heavier | `nChainWork += GetBlockProof` = `~T/(T+1)+1` after `SetCompact` | **Phase A done** — `block_proof_eq_div` |
| Which tip among many | `ActivateBestChain` over the header tree | **Phase B done** — `select_tip` |
| Immature coinbase / RES mint | `spend_h - coin_h < nCoinbaseMaturity` (100) | **Phase C done** — `spend_ok` |
| Unspendable / witness fail | converge reject; witness + sig check | class labels only |

Dust is **not** in that table. See §5.3.

Admission test (same five stopping criteria as the Frontier):

1. Each item kills a named consensus-split class.
2. The risk is not dominated by a cheaper unfixed risk *inside the
   same class* (we are not doing this instead of ConnectBlock; we are
   not claiming it replaces ConnectBlock).
3. Each completed phase must change `FinalClaimsBoundary.md` or
   `OpenProperties.md`.
4. Every new Category A theorem has a mutant that kills it.
5. Maturity stays 4.9. No Level 5 language.

Cost vs k-deep: all four phases are weeks-to-a-few-months of *rule*
work. k-deep needs an honest-majority / work-race model the cone
does not have. Do not start OP-R1 to “finish 2.3”.

---

## 1. Non-goals (do not sneak these in)

| Out | Why |
|---|---|
| OP-R1 / k-deep / honest majority | No hashrate model. A fake bound is worse than OPEN |
| `ActivateBestChain` / `ConnectBlock` / VST (3.2) | Implementation refinement, not a rule |
| Full Script VM / `EvalScript` | OP-S1 proper. Phase D is spendability, not an interpreter |
| Ed25519 EUF-CMA / hash collision (OP-C1) | Assumptions, not theorems |
| Dust / `IsStandardTx` / mempool | Policy. See §5.3 |
| Lottery / RFC-019 archive | Dead. Do not revive |
| `equivalence_claimed` / `refinement_proved` | Stay `false` |
| Closing 2.3 as DONE | Issuance half stays DONE; k-deep stays OPEN |

---

## 2. Phase A — `GetBlockProof` — DONE

### 2.1 Reason

`more_work_wins` is true for any additive increment. The node does
not use “any increment”. It uses, in `core/src/chain.cpp`:

```cpp
bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
if (fNegative || fOverflow || bnTarget == 0)
    return 0;
return (~bnTarget / (bnTarget + 1)) + 1;
```

The comment in that file is the identity we will prove: for a
representable target `T`,

```
(~T / (T+1)) + 1  =  2^256 / (T+1)     (integer division)
```

because `~T = 2^256 - 1 - T` on a 256-bit word, so

```
(2^256 - 1 - T) / (T+1) + 1
  = (2^256 - (T+1)) / (T+1) + 1
  = 2^256 / (T+1).
```

If two nodes used different work formulas (drop the `+1`, use `T`
instead of `T+1`, treat overflow as `powLimit`, …) they would
disagree on `nChainWork` and could split on a reorg. Abstract
`wb_work` cannot catch that. Modelling the formula can.

This is still **not** C++ `arith_uint256` bit-exact refinement.
The model is the integer identity plus the compact / overflow
guards. `ocamlopt` and the C++ word type stay outside the proof.

### 2.2 Files

| Role | Path |
|---|---|
| 256-bit word (exists) | `Pow/Uint256Model.v` |
| compact nBits (exists) | `Pow/CompactBits.v` |
| **new** proof + formula | `Pow/BlockProof.v` |
| consume it | `Kernel/Chain/ForkChoice.v` (`wb_work` derived, not free) |
| mutants | `mutation/mutate_block_proof.sh` |
| C++ cite | `core/src/chain.cpp` `GetBlockProof`; `SetCompact` in arith |

### 2.3 Definitions (normative)

```text
u256_not T          :=  2^256 - 1 - T          (0 ≤ T < 2^256)

get_block_proof_from_target T :=
    0   if T ≤ 0 or T ≥ 2^256
    (u256_not T) / (T+1) + 1   otherwise

get_block_proof nBits :=
    0   if SetCompact reports negative or overflow
        or decoded target = 0
    get_block_proof_from_target (compact_decode nBits)  otherwise
```

`SetCompact` negative / overflow must be copied from the same
Bitcoin compact rules the C++ uses, not invented. If
`CompactBits.v` does not already expose those flags, add them
there; do not re-implement compact in `BlockProof.v`.

### 2.4 Category A theorems (minimum)

| Name | Statement | Mutant idea |
|---|---|---|
| `block_proof_eq_div` | valid `T` ⇒ `get_block_proof_from_target T = 2^256 / (T+1)` | drop `+1`, or use `T` not `T+1` |
| `block_proof_zero_on_bad_target` | `T ≤ 0` or `T ≥ 2^256` ⇒ proof `= 0` | return `1` on bad target |
| `block_proof_zero_on_compact_fail` | negative / overflow / decoded 0 ⇒ `0` | ignore overflow |
| `block_proof_positive_on_valid` | well-formed compact, `T > 0` ⇒ proof `> 0` | clamp to 0 always |
| `chain_work_uses_block_proof` | suffix work = sum of `get_block_proof` of each block’s nBits | revert to free `wb_work` |

Reuse `chain_work_app` / `more_work_wins` / `equal_work_keeps_active`.
They stay true; their *increments* become the formula. Do not
re-prove the two-tip lemmas unless the record change forces it.
Give them a new mutant only if the old M102–M104 no longer bite
the new definition (likely they still do).

### 2.5 Steps

1. Read `SetCompact` and the existing `compact_decode` side by
   side. Write the flag predicate. One comment block citing
   `chain.cpp:131` and the arith `SetCompact` file/line.
2. Add `Pow/BlockProof.v`. Prove `block_proof_eq_div` first — it
   is the load-bearing identity. `Qed`, `Print Assumptions` Closed.
3. Add compact-fail zeros. Do not claim the flags equal C++ until
   a later differential (out of this phase’s claim).
4. Change `work_block` to carry `nBits` (or `target`) and define
   `wb_work b := get_block_proof …`. Keep `select_suffix`.
5. Re-check M102–M104 still kill. Add M108+ in
   `mutate_block_proof.sh`. Wire `run_all_mutations.sh`.
6. Dual-table classification, `gen_final_proof_inventory.py`,
   `gen_expected_failures.py`, `freeze_source_manifest.sh`.
7. Claims: `FinalClaimsBoundary.md` most-work row becomes
   “increments are `2^256/(T+1)` after compact guards”. Still not
   `ActivateBestChain`. Frontier 2.3 paragraph drops
   “GetBlockProof is not modelled”.

Landed: `Pow/BlockProof.v`, `wb_nbits` + derived `wb_work`,
M108–M112 all `EXPECTED_FAILURE`, M102–M104 still kill.
`Print Assumptions` Closed. Not `arith_uint256` refinement.

### 2.6 Stop / do not

- Do not model `GetBlockProofEquivalentTime` (P2P / headers-sync
  heuristic, not tip selection).
- Do not prove `arith_uint256` operator equivalence.
- Do not treat this as OP-R1 progress.

---

## 3. Phase B — multi-tip total order — DONE

### 3.1 Reason

A node does not compare two suffixes in isolation. It has a set of
candidate tips (the header tree) and must pick one: strictly more
work wins; equal work keeps the already-active tip.

Two-tip `select_suffix` is the binary special case. The split class
this phase kills is: “among three or more candidates, a node could
pick a non-maximal tip, or flip on a tie.” That is a rule bug, not
an honesty assumption.

This is still **not** `ActivateBestChain`. The C++ walk, skip-lists,
`m_best_invalid`, and header-sync work windows stay unmodelled.

### 3.2 Files

| Role | Path |
|---|---|
| extend | `Kernel/Chain/ForkChoice.v` |
| mutants | `mutation/mutate_fork_choice.sh` (extend; do not fork a third copy of M102–M104) |

### 3.3 Definitions (normative)

Candidates are a finite list of suffixes that already share the
same modelled prefix (same assumption as today’s two-tip file).

```text
weight(s) := chain_work s          (* now GetBlockProof-sum from Phase A *)

select_tip(active, cands) :=
    let pool = active :: cands in
    let w*   = max { weight(s) | s ∈ pool } in
    if weight(active) = w* then active
    else the first s in cands with weight(s) = w*
```

Tie-break is **keep active if active is maximal**; otherwise
**first maximal in the candidate list**. That matches “no reorg
for nothing” and makes the function total and deterministic
without a block-hash order (hash order would pull in OP-C1).

If a later audit shows C++ uses a different equal-work order
(e.g. first-seen header), record it as a documented delta; do not
silently match an unverified walk.

### 3.4 Category A theorems (minimum)

| Name | Statement | Mutant idea |
|---|---|---|
| `select_tip_maximal` | result’s work ≥ every candidate’s work | pick `hd cands` always |
| `select_tip_keeps_active` | `weight(active) = w*` ⇒ result = active | prefer first cand on ties |
| `select_tip_binary` | `select_tip a [b] = select_suffix a b` | break the specialisation |
| `select_tip_total` | result ∈ `active :: cands` | return `[]` |

Do **not** add “the selected tip is unique” — it is not, under
ties. The uniqueness that matters is the *function’s* output.

### 3.5 Steps

1. Land Phase A first so `weight` is the real increment.
2. Add `select_tip`. Prove the four theorems.
3. Keep `select_suffix` as the binary wrapper (or redefine it as
   `select_tip a [b]` and transport old proofs).
4. Mutants M11x. Inventory / freeze / claims: most-work row
   becomes “finite candidate set, max work, tie keeps active”.
5. Frontier 2.3: “two-tip” → “finite-tip rule; not ActivateBestChain”.

Landed: `select_tip` / `pool_max_work` / `first_at_work`.
`select_tip_maximal` (M113), `select_tip_keeps_active` (M114),
`select_tip_binary` (M115), `select_tip_total` (M116).
`select_suffix` kept as its own definition so M102–M104 still match.

### 3.6 Stop / do not

- No honest-majority, no k-deep, no “after N confirmations”.
- No header-tree parent pointers unless a theorem needs them
  (it should not: work is a suffix sum).
- No “all honest nodes eventually agree” (liveness / P2P).

---

## 4. Phase C — coinbase / RES-mint maturity, and spend-side `MAX_MONEY` — DONE

### 4.1 Reason

C++ consensus (`core/src/consensus/tx_verify.cpp`,
`Coin::RequiresMaturity` in `coins.h`):

```text
RequiresMaturity  ≡  fCoinBase ∨ IsResMint
reject if spend_height - coin_height < nCoinbaseMaturity
nCoinbaseMaturity = COINBASE_MATURITY = 100
RES_MINT_MATURITY is locked to the same 100
```

A node that spends a coinbase at depth 99, or treats a body RES
mint as immediately spendable, splits. The UTXO model has
`tx_coinbase` and `money_range` on *outputs*. It does not gate
*spends* by depth.

`MAX_MONEY` is already a constant and an output predicate
(`Core/Amount.v`, `UTXOSoundness.v`). What is still missing as a
*spend* rule is the input-side `MoneyRange` / `AddMoneyOverflows`
check in the same `AccumulateTxInputPools` function. That is the
same split class as “value out of range”: an input of
`MAX_MONEY+1`, or a sum that overflows `int64` before the post-hoc
range check.

`MAX_MONEY` remains an overflow sanity bound, **not** an ORU
supply cap. Phase C must not grow a “total ORU ≤ MAX_MONEY”
theorem — that would be a false rule.

### 4.2 Files

| Role | Path |
|---|---|
| constants (exists) | `Core/Amount.v` (`MAX_MONEY`); add `COINBASE_MATURITY` |
| **new** | `Kernel/Tx/Maturity.v` (or `Supply/Maturity.v`) |
| consume | UTXO spend / `valid_tx` / chain apply — the existing
  spend predicate, not a parallel one |
| C++ cite | `consensus/consensus.h`, `coins.h:96`, `tx_verify.cpp:194` |
| mutants | `mutation/mutate_maturity.sh` |

### 4.3 Definitions (normative)

```text
COINBASE_MATURITY := 100

requires_maturity(coin) := coin.is_coinbase ∨ coin.is_res_mint

mature(spend_h, coin_h) :=
    spend_h - coin_h ≥ COINBASE_MATURITY

spend_ok(spend_h, coin) :=
    money_range(coin.value)
    ∧ (requires_maturity(coin) → mature(spend_h, coin_h))

pool_add(acc, v) :=
    None   if AddMoneyOverflows(acc, v)     (* acc + v > MAX_MONEY *)
    Some (acc + v) otherwise
```

Height is `Z` with the same non-negativity the UTXO model already
uses. Do not invent unsigned wrap; C++ uses `int` subtraction on
heights that are already consensus-bounded.

### 4.4 Category A theorems (minimum)

| Name | Statement | Mutant idea |
|---|---|---|
| `immature_coinbase_rejected` | coinbase, depth 99 ⇒ `spend_ok = false` | maturity `:= 0` or `99` |
| `mature_coinbase_at_100` | depth `≥ 100` ⇒ maturity gate passes | off-by-one (`>` not `≥`) |
| `res_mint_uses_same_maturity` | body RES mint uses the same 100 | mint maturity `:= 0` |
| `plain_transfer_no_maturity` | not coinbase and not mint ⇒ no depth gate | require maturity on all coins |
| `input_over_max_rejected` | input `> MAX_MONEY` ⇒ spend reject | drop input range check |
| `pool_add_overflow_none` | `acc + v > MAX_MONEY` ⇒ `None` | add then check (the bug `AddMoneyOverflows` exists to prevent) |

### 4.5 Steps

1. Confirm no existing theorem already states the spend-depth
   gate (search says no). Do not duplicate `money_range_zero`.
2. Add the constant next to `MAX_MONEY`. Cite both headers.
3. Attach `is_res_mint` / `is_coinbase` / `coin_height` to the
   coin record the spend path already uses. If that record cannot
   grow without rewriting half the UTXO cone, introduce a thin
   `spend_coin` view and prove the gate *there*, then hook it
   from `valid_tx` in a second commit — do not fork two UTXO
   models.
4. Prove the six theorems. Boundary tests at 99 and 100 are
   mandatory; a proof only at “some large depth” is not enough.
5. Mutants M12x. Inventory / freeze.
6. Claims: new row “coinbase and RES-mint spends require depth
   ≥ 100; inputs in `money_range`; pool add does not wrap”.
   Not ConnectBlock.

Landed: `Kernel/Tx/Maturity.v` (`spend_coin` view, not a UTXO
rewrite). M117–M122. Dust stays policy. Not hooked into
`valid_tx` / `apply_block` (that hook is a follow-up if small).

### 4.6 Dust is out — explicit

`docs/audit/ORU_DESTROY_DUST_SPENDMAP_AUDIT.md` is a **wallet /
mempool** incident. `testmempoolaccept` reason `dust` comes from
`IsStandardTx` / `IsDust`, which `validation.cpp` applies when
`m_require_standard` is set on the *pool*, not as a
`TxValidationResult::TX_CONSENSUS` block check.

A miner-constructed block may contain a dust-sized output and
honest nodes will still connect it. Putting dust in Category A
would formalise a rule the chain does not enforce, and would
license a false “destroy below dust is consensus-invalid” claim.

**Do not add dust, `GetDustThreshold`, or `destroyoru` minima to
this cone.** If a later RFC makes a destroy floor consensus,
re-open this paragraph. Until then it is Category C / policy.

---

## 5. Phase D — script spendability (OP-S1 *slice*, not the VM) — DONE

### 5.1 Reason

`script_step_deterministic` is a classifier re-application. It
cannot say whether a UTXO can be spent. Nodes split on that
question every time they disagree about:

- spending `OP_CONVERGE` (consensus reject
  `bad-txns-spend-convergence`);
- accepting a witness program with an empty / failing witness;
- treating `T_NONSTANDARD` / `T_NULL_DATA` as spendable.

The classifier already decides *what* a script is
(`ScriptClassifier.v` ↔ `ClassifyRigorTxout`). Phase D adds
*whether that class may be spent*, with signature check left as
an abstract Boolean (OP-C1 stays open).

Rigor has no opcode VM to formalize. Four locking shapes, plus
two reject classes. Phase D is the spend matrix for those shapes:
no interpreter, no sighash algebra, no `EvalScript`.

OP-S1 in `OpenProperties.md` remains **Not formalized** for the
VM / signature-verification half. Phase D may add a sentence
“spendability by class + abstract verify: Closed; VM: still open”.
It must not mark OP-S1 Closed.

### 5.2 Files

| Role | Path |
|---|---|
| classifier (exists) | `Script/ScriptClassifier.v` |
| replace the toy step | `Script/Semantics/Execution.v` — or a new
  `Script/Semantics/Spend.v` so the toy step can stay Category C |
| C++ cite | `tx_verify.cpp` converge spend; witness verify path in
  `script/predicate_verify.cpp` / interpreter entry |
| mutants | `mutation/mutate_script_spend.sh` |

### 5.3 Definitions (normative)

```text
sig_ok : Witness → Script → bool     (* uninterpreted or a
                                        parameter; not Ed25519 *)

spendable_by_class cls wit script :=
    match cls with
    | T_CONVERGE | T_NULL_DATA | T_NONSTANDARD => false
    | T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => sig_ok wit script
    end

script_spend_ok script wit :=
    spendable_by_class (classify_script script) wit script
```

**`T_MINT` is an auth class.** C++ `IsResMintScript` goes to
`VerifyResMintSpend` (`predicate_verify.cpp:460`): structured
`<sig> <pk>` scriptSig, Hash160 match, Ed25519, plus a script
locktime. It is not unspendable, and it is not “spent later only
as a matured native-witness coin by re-locking”. Coin-level
maturity (Phase C) is a separate gate. Hash160 / locktime /
Ed25519 bytes stay unmodelled; `sig_ok` is abstract authorization.
See appendix §10.

### 5.4 Category A theorems (minimum)

| Name | Statement | Mutant idea |
|---|---|---|
| `converge_unspendable` | `classify = T_CONVERGE` ⇒ spend false | allow converge |
| `nulldata_unspendable` | `T_NULL_DATA` ⇒ false | allow OP_RETURN |
| `nonstandard_unspendable` | `T_NONSTANDARD` ⇒ false | allow fallback |
| `witness_requires_sig_ok` | auth class (v0 / v1 / mint) ∧ `sig_ok = false` ⇒ false | ignore `sig_ok` |
| `witness_sig_ok_spends` | auth class ∧ `sig_ok = true` ⇒ true | reject all auth |
| `classifier_then_spend` | `script_spend_ok` uses `classify_script` (no second ladder) | skip classify, force `T_NONSTANDARD` |

Do not promote `script_step_deterministic` back to A. It stays C
(classifier step, not spend).

### 5.5 Steps

1. Trace one confirmed spend of each class in C++ — **done**,
   appendix §10. §5.3 was edited: `T_MINT` is auth, not false.
2. Add `Spend.v`. Keep `sig_ok` abstract. No keccak, no Ed25519
   bytes. **done.**
3. Prove the six theorems. Mutants M123–M128. **done.**
4. Hook `script_spend_ok` into the UTXO spend predicate used by
   `valid_tx` / `apply_block` if that hook is a small change. If
   it forces a ConnectBlock-shaped rewrite, **stop after the
   standalone spend module** and record the hook as a follow-up.
   **Stopped.** `valid_tx` is structural (no script / witness
   fields). The hook is a follow-up, not this phase.
5. Inventory / freeze. `OpenProperties` OP-S1: VM still open.
   Claims: “spendability by class + abstract verify, in the
   kernel model”. Not “scripts are verified”.

Landed: `Script/Semantics/Spend.v`. `converge_unspendable`
(M123), `nulldata_unspendable` (M124), `nonstandard_unspendable`
(M125), `witness_requires_sig_ok` (M126; v0 / v1 / mint),
`witness_sig_ok_spends` (M127), `classifier_then_spend` (M128).
Standalone gate. Not hooked into `valid_tx` / `apply_block`.

### 5.6 Stop / do not

- No opcode-by-opcode interpreter.
- No sighash preimage algebra.
- No “signatures are unforgeable”.
- No classifier ≡ `rigor_classify.cpp` (C-7 stays open unless a
  later differential says otherwise).

---

## 6. Sequence

```
A  GetBlockProof formula + compact guards     DONE (M108–M112)
B  multi-tip select_tip (uses A’s weight)     DONE (M113–M116)
C  maturity 100 + spend-side MAX_MONEY        DONE (M117–M122)
   (dust: skip)
D  script spendability slice                  DONE (M123–M128)
   (standalone Spend.v; no valid_tx hook)

do not start: OP-R1, 3.2, full VM, dust
```

A before B (weight must be real). C is independent of A/B and
may overlap after A’s file layout is stable. D last: it is the
only phase that can discover a C++ mismatch large enough to
resize the work.

Each phase is a separate claim edit. Shipping A+B without C+D is
fine. Shipping a half-proved D is not.

---

## 7. Housekeeping (every phase)

Same bar as M99–M107:

1. `Qed`, no `Admitted`.
2. `Print Assumptions` Closed (or only the already-accepted
   abstract `sig_ok` / `H` parameters — list them).
3. New Category A ⇒ a mutant that kills that theorem.
4. Dual tables: `release/TheoremClassification.md` and
   `evidence/` copy stay in lockstep.
5. `inventory_metadata.json` → `gen_final_proof_inventory.py`.
6. `gen_expected_failures.py`; `run_all_mutations.sh` expected
   count (121 after Phase C; 127 after Phase D) rises by the
   new EXPECTED_FAILURE rows.
7. `release/freeze_source_manifest.sh`.
8. `FormalizationFrontier.md` 2.3 / recommended order: honest
   wording only. Do not mark 2.3 DONE. Do not close OP-R1.
9. `equivalence_claimed = false`, `refinement_proved = false`,
   `implemented = true` stay as they are.

Suggested mutant IDs (assign at implementation; do not gap-fill
by inventing kills):

| Phase | Provisional |
|---|---|
| A | M108–M112 |
| B | M113–M116 |
| C | M117–M122 |
| D | M123–M128 |

---

## 8. What may be said after all four land

Allowed addition to the existing public sentence:

> In the Coq consensus-kernel model, fork-choice weight is the
> compact-`nBits` `GetBlockProof` sum; among a finite set of
> suffixes the maximal-work tip is selected and a tie keeps the
> active tip; coinbase and RES-mint outputs are unspendable
> before depth 100; values outside `money_range` and overflowing
> pool adds are rejected; converge / null-data / nonstandard
> scripts are unspendable and witness / mint spends require the
> abstract signature check to succeed.

Still forbidden:

- the full node / C++ / binary is verified;
- OP-R1 / k-deep is closed;
- the Script VM is formalized;
- dust is a consensus rule;
- Level 5;
- `refinement_proved`.

---

## 9. Gate to start coding

This document is the gate. Implementation starts at Phase A
§2.5 step 1 (read `SetCompact` against `CompactBits.v`).

If a step would require a fake k-deep bound, a dust Category A
theorem, or a VM, stop and amend this file first.

---

## 10. Appendix — C++ spend path (Phase D freeze table)

Read before proving. Sources: `tx_verify.cpp:185`,
`predicate_verify.cpp` (`ClassifyRigorPredicateError` /
`VerifyResMintSpend` / `VerifyWitnessProgram`),
`rigor_classify.cpp`.

| Class | C++ gate | Model row |
|---|---|---|
| `T_WITNESS_V0` | Witness program v=0, 20-byte keyhash; `VerifyWitnessProgram` + Ed25519 (`WITNESS_V0`) | `sig_ok` |
| `T_WITNESS_V1` | v=1, 32-byte native RES program; Ed25519 (`WITNESS_V1_RES`) | `sig_ok` |
| `T_MINT` | `IsResMintScript` → `VerifyResMintSpend`: parse mint, script locktime, scriptSig `<sig> <pk>`, Hash160, Ed25519 (`RES_MINT`) | `sig_ok` (auth class). Locktime / Hash160 unmodelled |
| `T_CONVERGE` | `IsConvergenceScript` → `bad-txns-spend-convergence` before predicate | `false` |
| `T_NULL_DATA` | not mint, not witness → `SCRIPT_ERR_RIGOR_SCRIPT_TYPE`; also `ASSET_NONE` | `false` |
| `T_NONSTANDARD` | same type error; also `ASSET_INVALID` | `false` |

Mint authorization is the scriptSig stack, not the witness
stack. The Coq `Witness` type is abstract authorization
material covering both. A mint UTXO is spent by satisfying
that mint lock, not by rewriting it into a native-witness
program first. That is why §5.3’s first draft (`T_MINT =>
false`) was rejected.
