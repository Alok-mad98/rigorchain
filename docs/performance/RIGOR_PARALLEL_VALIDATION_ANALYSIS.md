# RIGOR Parallel Validation Analysis (P0-077)

**Task:** `TASK-RIGOR-MAINNET-PERFORMANCE-BASELINE-OPTIMIZATION-P0-077`  
**Date:** 2026-08-31  
**Nature:** Analysis only. This TASK does **not** introduce parallel consensus.

```
PARALLEL_CONSENSUS_INTRODUCED = NO
LEDGER_MUTATION_ORDERING      = sequential (unchanged)
CONNECTBLOCK_EXECUTOR         = existing C++ path (unchanged)
```

---

## Why this is not an implementation

Consensus accept/reject for a block is a function of ordered UTXO mutation.
A second concurrent mutator, a lock-free "fast path" that skips
`VerifyRigorPredicate`, or a parallel draw resolver would be a second
authority. P0-077 forbids that.

The notes below record where parallelism is **semantically safe as a
pre-check** versus where it would reorder or duplicate a kernel decision.

---

## Can be parallel (precheck / verify-only)

These steps do not mutate chainstate. Given an immutable snapshot of the
inputs they already consume, independent workers may compute the same
boolean the sequential path would compute.

| Work | Unique authority | Parallelism shape |
|---|---|---|
| Transaction precheck | encoding, size, duplicate-input syntax, scriptPubKey shape | per-tx, before kernel |
| Asset classification | `ClassifyRigorAsset` / `ClassifyRigorTxout` | per output; results must be the same bytes-in/type-out function |
| Sighash preimage | `SignatureHash` / `CppSighashPreimage` | per input; `PrecomputedTransactionData` already batches prevouts/sequences/outputs |
| Signature verification | Ed25519 via `VerifyRigorPredicate` (production) | per input **after** the preimage is bound; must not invent a second verifier |
| Independent `VerifyRigorPredicateBoundary` | verify-only explicit input | per tx when `crypto_accept` / `keyhash_match` are already supplied |

`PrecomputedTransactionData::Init` is the existing allowed batch preparation
for sighash. Reusing it is not a new hash rule.

A parallel precheck **must still fail closed**: any worker reject is a block
reject. A worker accept is not a ledger accept.

---

## Must stay sequential

| Work | Why |
|---|---|
| Consensus decision | `RigorConsensusKernel::EvaluateConnect` / `ResolveDraw` / issuance / supply are unique per height. Two workers racing a draw winner is a fork. |
| `ConnectEvaluateCoinbaseResMint` | Single mint gate on the coinbase. |
| Ledger mutation ordering | `AddCoin` / `SpendCoin` / `UpdateCoins` apply in block order. Input of tx *k* may be output of tx *k-1*. |
| `LedgerApply` / `UTXOStateTransition` | Verify-only today; if ever used as an executor, apply order is the state. |
| Tip / `DB_BEST_BLOCK` write | Two-phase head-blocks protocol is a single writer. |
| Maturity relative to the connecting height | Height is a block-global input, not a per-thread guess. |

Intra-block topologically independent transactions could in principle have
predicate checks overlapped, then a **single** thread apply UTXO mutations in
original order. That is still not implemented here. Doing it would require a
separate TASK with an explicit sequential commit phase and no second
`ConnectBlock` body.

---

## Forbidden shortcuts (even in a future parallel design)

- Accept a tx because a worker classified it, without `VerifyRigorPredicate`.
- Cache a draw winner keyed only by height (parent F and T must match).
- Approximate `Mul256x256Full`.
- Parallel `CCoinsViewCache` mutation without a total order.
- A second `GetBlockSubsidy` / `ComputeRigorIssuance` body on a worker.

---

## Recommended future split (not in this TASK)

```
Phase A  (parallel, no mutation)
    per-tx precheck + classify + sighash + Ed25519
         |
Phase B  (single-threaded)
    RigorConsensusKernel::EvaluateConnect
    ordered CCoinsViewCache apply
    supply / mint / maturity gates
         |
Phase C  (single writer)
    FlushStateToDisk
```

Phase A may return only `REJECT` or `CONTINUE`. Phase B is the unique
consensus decision. This document does not ship Phase A.

---

## Honesty

Analysis of data-race-free precheck is not a proof that a parallel ConnectBlock
would match sequential ConnectBlock. `EQUIVALENCE_PROVED` remains false.
No parallel consensus is introduced by P0-077.
