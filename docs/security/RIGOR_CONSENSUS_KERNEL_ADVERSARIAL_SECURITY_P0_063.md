# Consensus Kernel adversarial security baseline (P0-063)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-ADVERSARIAL-SECURITY-P0-063`  
**Date:** 2026-08-31  
**Nature:** Attack-surface baseline against an already CLOSED Consensus Kernel / Transaction Kernel boundary. No production consensus edit. No Coq edit. No new consensus authority. No refinement proof.

```
malicious input
    ↓
Consensus Kernel Boundary
    ↓
accept / reject
```

```
CONSENSUS_KERNEL_ADVERSARIAL_SECURITY = CLOSED/PASS

ISSUANCE_ATTACKS           = PASS
MATURITY_ATTACKS           = PASS
DRAW_ATTACKS               = PASS
CLASSIFICATION_ATTACKS     = PASS
SUPPLY_ATTACKS             = PASS
TRANSACTION_ATTACKS        = PASS
CONNECTBLOCK_DRIFT_AUDIT   = PASS
FUZZ_BASELINE              = PASS

CRASH_FOUND                      = NO
DISCREPANCY_FOUND                = NO
PRODUCTION_CONSENSUS_MODIFIED    = NO
PROTOCOL_PARAMETERS_MODIFIED     = NO

REFINEMENT_PROVED   = TRUE
EQUIVALENCE_PROVED  = FALSE
```

---

## Scope

This package proves a **fail-closed adversarial baseline** on the frozen kernel surface:

| Domain | Authority | File |
|---|---|---|
| issuance | `GetBlockSubsidy` | `core/src/consensus/rigor_issuance.cpp` |
| RES | `GetResMintReward` | `core/src/consensus/rigor_consensus_lock.h` |
| issuance pack | `ComputeRigorIssuance` | `core/src/consensus/rigor_issuance.cpp` |
| draw | `ResolveConvergenceDraw` | `core/src/consensus/convergence_logic.cpp` |
| classify | `ClassifyRigorTxout` | `core/src/script/rigor_classify.cpp` |
| supply | `RigorSupplyDelta` | `core/src/consensus/rigor_supply.cpp` |
| spend | `VerifyRigorPredicate` | `core/src/script/predicate_verify.cpp` |

`ClassifyRigorAsset` is a wrapper of `ClassifyRigorTxout`. `Solver` is a wrapper of `ClassifyRigorTxout`. There is no second matcher.

ConnectBlock (`validation.cpp`) may only prepare input, call the kernel, and consume the result.

---

## Unchanged (forbidden)

Issuance schedule, ORU subsidy, RES reward, maturity rules, draw formula, supply formula, PoW, ASERT, Transaction Kernel semantics, ConnectBlock consensus behavior, Coq canonical definitions, and golden vectors were **not** modified.

---

## Artifacts

| Artifact | Role |
|---|---|
| `core/src/test/test_consensus_kernel_adversarial.cpp` | Boost adversarial suite |
| `fuzz/consensus_kernel_adversarial/` | `ConsensusKernelInput` harness + smoke |
| `tools/lint-consensus-authority-security.py` | unique-authority + ConnectBlock drift |
| `formal_verification/adversarial_security_vectors.json` | pinned attack vectors |
| this document | audit record |

Fail tokens:

| Surface | Token |
|---|---|
| issuance | `CONSENSUS_ISSUANCE_ATTACK_FOUND` |
| maturity | `MATURITY_BYPASS_FOUND` |
| draw | `DRAW_NON_DETERMINISTIC` / `DRAW_TIE_BREAK_BYPASS` |
| classify | `CLASSIFICATION_BYPASS_FOUND` |
| supply | `SUPPLY_INFLATION_ATTACK_FOUND` |
| spend | `TRANSACTION_KERNEL_BYPASS_FOUND` |
| ConnectBlock | `CONNECTBLOCK_CONSENSUS_DUPLICATION_FOUND` |
| crash | `CONSENSUS_KERNEL_CRASH_FOUND` |
| oracle | `CONSENSUS_KERNEL_ORACLE_DISCREPANCY` |

---

## Part 1 — Issuance

Heights: `0, 1, 99, 100, 101, 209999, 210000, 210001, 419999, 420000`.

Covered: epoch boundary, halving boundary, integer overflow (`INT_MAX`), maximum uint height (cast of `uint32_t` max → negative `int`), invalid height (`< 0`), reward truncation (ORU shift floor at 3; RES integer `isqrt` floor).

Genesis pins:

```
GetBlockSubsidy(0) == 0
GetResMintReward(0) == RES_REWARD_GENESIS
actual issuance via RigorSupplyDelta(height=0, no mint): ORU == 0, RES == 0
```

Schedule at height 0 is **not** actual issuance. Negative height is not a production input: RES fails closed to 0; ORU treats it as non-genesis (existing `GetBlockSubsidy` special-case is only `nHeight == 0`).

---

## Part 2 — Maturity

ORU coinbase: `spend_height - source_height >= 100`.  
RES mint lock: `spend_height >= source_height + 100`.

| source | spend | ORU | RES |
|---|---|---|---|
| 0 | 99 | immature | immature |
| 0 | 100 | mature | mature |
| 1 | 100 | immature | immature |
| 1 | 101 | mature | mature |

`VerifyRigorPredicate` on a mint script rejects spend-before-lock with `SCRIPT_ERR_UNSATISFIED_LOCKTIME`. Overflow lock (`int64` max vs `int` max spend height) stays immature (fail-closed).

---

## Part 3 — Draw

`ResolveConvergenceDraw` on `F`:

| Case | Expect |
|---|---|
| empty candidates | none |
| single eligible candidate | that candidate |
| equal weight, different txid | permutation-invariant winner |
| weight overflow (`uint64` max) | ineligible / none |
| duplicate candidate | 512-bit equality → txid tie-break, same winner |
| zero weight | none |
| `T = 0` | none |
| maximum target | eligible path remains deterministic |

Same mathematical input under candidate-order permutation produces the identical winner. Tie-break is unsigned txid `memcmp` after 512-bit `H_i * f_j` comparison. No division.

---

## Part 4 — Classification

Inputs: valid ORU, valid RES, malformed mint, empty script, unknown witness version, truncated program, oversized bytes, random bytes.

Only `ClassifyRigorTxout` is the matcher. `ClassifyRigorAsset` / `AssetTypeFromScriptPubKey` / `Solver` must agree. Unknown / truncated / oversized / random bytes fail closed to `ASSET_INVALID` (or `ASSET_NONE` only for a well-formed `OP_RETURN`).

---

## Part 5 — Supply delta

Attacks: duplicate mint amount (no hidden doubling), invalid mint, negative circulating delta (converge > subsidy is burn, not inflation), overflow coinbase, fee / burn manipulation, winner none, winner exists.

`BlockValidationState&` is a reject-reason sink. Computation does not read it. `RigorSupplyDelta` has no chainstate / UTXO / DB.

Genesis actual Δ remains `{ORU: 0, RES: 0}`.

---

## Part 6 — Transaction kernel

`VerifyRigorPredicate` on scriptPubKey / scriptSig / witness / pubkey / signature.

Rejects: empty signature, malformed signature, wrong pubkey, modified witness, modified sighash (mutated outputs), replay-style chain-id mutation, wrong asset type, empty / unknown script.

Fail-closed. Production consensus path must not define or call `EvalScript` / `VerifyScript`.

---

## Part 7 — ConnectBlock authority

`Chainstate::ConnectBlock` must not contain:

- subsidy calculation
- RES calculation
- draw resolution (`Mul256x256Full` / `SelectWinner`)
- supply formula recomposition
- predicate parsing (`ParseResMintScript` / `IsWitnessProgram` matcher triple)

It may only assemble `F` / fees / coinbase figures, call `ResolveConvergenceDraw` and `RigorSupplyDelta`, and apply the returned result. Spend evaluation remains `VerifyRigorPredicate` via `CheckInputScripts`.

---

## Part 8 — Fuzz

`fuzz/consensus_kernel_adversarial/` feeds `ConsensusKernelInput`:

- random bytes
- boundary integers
- malformed structures
- random scripts
- random draw sets

Any crash: `CONSENSUS_KERNEL_CRASH_FOUND`.  
Any C++ ≠ extracted / pinned oracle: `CONSENSUS_KERNEL_ORACLE_DISCREPANCY`.

Smoke: `bash fuzz/consensus_kernel_adversarial/run_smoke.sh`.

---

## Part 9 — Authority drift

`tools/lint-consensus-authority-security.py` fails on a second production body of any unique authority, a ConnectBlock formula copy, an `EvalScript`/`VerifyScript` consensus-path definition, or a second 512-bit winner loop.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing kernel-boundary refinement (P056–P059). This task does **not** add a new refinement. `EQUIVALENCE_PROVED = FALSE`: this is not a full-node proof.

---

## Verdict

**CONSENSUS_KERNEL_ADVERSARIAL_SECURITY_CLOSED**
