# Refinement Mapping

Interface map from the six extracted Coq checkers to production
critical paths.

**Status (binding):**

> Model-side source identity is proved.
> Production side is differential verification only.
> There is **no** production-code refinement proof.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`cpp_equivalence = false`

This document is not a CompCert / C++ refinement.

## Compared fragment

Differential tests compare the **extracted OCaml checker** (executable
specification of the Coq validators) against a **shadow**. Each domain
table below splits the shadow into:

| Kind | Meaning |
|---|---|
| **production-derived** | Constants / formulas copied from cited C++ (e.g. `MAX_MONEY`, `W(h)`) |
| **model-mirrored** | Conditions that exist in the Coq checker and are re-encoded in Python, not read from production control flow |

The shadow is **not** `ConnectBlock`. It does not link the C++ node.
Production sources are cited; they are not modified.

`MATCH` means the extracted checker and the shadow **agreed**. It is
**not** an independent correctness assertion and **not** a C++
refinement. When a vector carries `expect` (`VALID` / `INVALID`), the
oracle additionally checks that absolute verdict; `AUTO` / omitted
`expect` is agreement-only.

## UTXO — `check_utxo`

| | |
|---|---|
| Model I/O | `tx` → `bool`. Fields: `tx_id`, `tx_in`, `tx_out.value`, `coinbase` |
| Production path | `MoneyRange` in `core/src/consensus/amount.h`; structural empty-in/out checks in transaction validation (`CheckTransaction` family) |
| production-derived | `0 <= value <= MAX_MONEY` with `MAX_MONEY = 10^10 * COIN` (`amount.h`) |
| model-mirrored | non-coinbase must have non-empty in and out; input outpoints distinct; outputs do not collide with inputs; output-sum in range |
| Known differences | Production also checks scripts, signatures, prevout existence, locktime, and ConnectBlock contextual rules. The model does **not**. A model `VALID` is not a ConnectBlock accept |

## ORU — `check_oru`

| | |
|---|---|
| Model I/O | `kernel_tx` → `bool`. Fields: domain, `oru_in/out`, `res_in/out`, supply/subsidy |
| Production path | Fee / supply accounting in `core/src/consensus/rigor_supply.cpp` and ConnectBlock value checks |
| production-derived | fee does not create ORU (`oru_out <= oru_in`) as the comparable accounting inequality |
| model-mirrored | non-negative legs; `DomORU` forbids RES legs; `DomRES` requires `oru_in > 0` and `res_out <= res_in` |
| Known differences | Production reconstructs amounts from the UTXO set and scripts. The model is given already-split integers. Hidden-inflation via a non-modelled path is out of scope |

## RES — `check_res`

| | |
|---|---|
| Model I/O | `res_tx` → `bool`. Fields: burn `B`, mint `R`, height |
| Production path | `CheckResMintArithmetic` / pole form in `core/src/consensus/res_issuance.h`; body-tx check in `tx_verify.cpp` |
| production-derived | continuous `destroy_cost(h)`; cap `419430400000000` atoms (`2²²` RES) |
| model-mirrored | Positive mint requires `R · (2²⁵+h)² ≤ B << 40` |
| Known differences | Lottery `W(h)` / coinbase mint / draw flag are **archived Category C**. C++ ConnectBlock does **not** reject on a running `S_RES` scalar (C-3). |

## Script — `check_script`

| | |
|---|---|
| Model I/O | script bytes + expected `ScriptType` → `bool` |
| Production path | `ClassifyRigorTxout` in `core/src/script/rigor_classify.cpp` |
| production-derived | (none claimed as bit-identical to `rigor_classify.cpp`; C-7 is open) |
| model-mirrored | Ladder: witness v0/20 → WITNESS_V0; witness v1/32 native → WITNESS_V1; else terminate; then mint / converge / OP_RETURN / NONSTANDARD |
| Known differences | Classifier refinement vs `rigor_classify.cpp` is **open** (C-7). Differential tests check the comparable fragment only. No script VM, no Ed25519 |

## PoW — `check_pow`

| | |
|---|---|
| Model I/O | supplied `digest` integer, `target`, `compact_t` → `bool` |
| Production path | `CheckProofOfWork` in `core/src/pow.cpp`; compact `SetCompact` / `GetCompact` |
| production-derived | (none of the hash / `powLimit` branches; digest is supplied) |
| model-mirrored | `0 < target` and `digest <= target`; `compact_t >= 0`; compact encode/decode **lower bound** (`decode(encode t) <= t`). This lower bound is a Coq-model condition, **not** a `CheckProofOfWork` check |
| Known differences | Production computes `GetRigorPowHash` (keccak / `"RGRPOW01"`). The model takes the digest as an uninterpreted integer. Hash implementation is **not** compared. See `Kernel/CryptoAssumptions.v`. Production additionally rejects `bnTarget > UintToArith256(params.powLimit)` and `SetCompact` `fNegative` / `fOverflow`. The model has no `powLimit` upper bound and no compact sign/overflow flags |

## ASERT — `check_asert`

| | |
|---|---|
| Model I/O | `te`, `tau`, `expected_q` → `bool` |
| Production path | `GetNextWorkASERT` in `core/src/pow.cpp` (`exp_q = trunc(te * 65536 / half_life)`) |
| production-derived | Toward-zero division of `te * 65536 / tau` matches C++ `__int128` `/` on that fragment |
| model-mirrored | `te < 0`, `tau > 0`, and `expected_q = exp_q_raw` (**unclamped**). The model compares `exp_q_raw`. Production clamps to `±ASERT_EXP_CLAMP` **before** using the exponent |
| Known differences | Production then builds `factor`, shifts a 256-bit target, clamps to `powLimit`, and returns compact `nBits`. Full ASERT target transition is **not** a refinement. When `\|te * 65536 / tau\| > ASERT_EXP_CLAMP`, the unclamped model quotient and the production clamped exponent diverge |

## Status table

| Checker | Source identity (Coq) | Diff-verified fragment | Production refinement |
|---|---|---|---|
| `check_utxo` | proved | MoneyRange + structural tx | **not proved** |
| `check_oru` | proved | conservation inequalities | **not proved** |
| `check_res` | proved | `W(h)` mint bound | **not proved** |
| `check_script` | proved | classification ladder fragment | **not proved** |
| `check_pow` | proved | integer target compare | **not proved** |
| `check_asert` | proved | `exp_q` quot fragment | **not proved** |

Run: `scripts/diff_verify.sh`
Report: `evidence/DiffVerifyReport.md` (count regenerated each run)

`MATCH` is comparable-fragment **agreement** only. It is not a C++
refinement and not an independent correctness proof.
