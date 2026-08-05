# Rigor Consensus Pipeline Checklist

This document is a pre-merge checklist for RIGOR consensus changes.

RIGOR identity:

- Script-Native, Self-Proving Cryptoeconomic State Machine
- Consensus state must be derived from `scriptPubKey + witness + deterministic functions`

## Canonical Block Validation Flow

Every block must follow this order:

1. Parse scripts
2. Identify Convergence set
3. Compute winner
4. Read coinbase commitment
5. Verify winner equals commitment
6. Identify RES claim
7. Verify RES equals winner
8. Compute target
9. Verify target equals commitment
10. Verify full block consistency

Do not skip or reorder steps.

## Single Source Rules

- Winner source must be unique:
  - `GetBlockWinner(...)` is the only non-test API
  - `GetBlockWinner(...)` must call `CollectConvergenceWinnerTxid(...)`
- Do not recompute winner in unrelated modules.

## Commitment Rules

- Coinbase must include RIGOR commitment payload:
  - `OP_RETURN "CVGE" (CVGE wire tag) + cumulative + ConvergenceDrawCandidates root (see convergence_commitment.h)`
- If commitment is missing: reject block
- If committed winner differs from computed winner: reject block
- If committed target differs from deterministically computed target: reject block

## RES Binding Rules

- At most one RES claim per block
- RES claim is not independent:
  - `res_winner_txid == committed_winner_txid`
- If committed winner is zero, RES claim must not exist
- If committed winner is non-zero, RES claim must exist

## convergence Rules

- convergence type is script-inferred (`CVG2`), not a stored asset field
- Convergence outputs must not enter UTXO set
- Convergence outputs must not be spendable
- Do not modify `CTxOut` serialization or DB schema for asset tagging

## Randomness Binding Rule (Critical)

convergence hash preimage must be:

- `H(txid || bind_hash || height || target_prev)`

where:

- `target_prev = pindexPrev->nBits`

Forbidden:

- using current block target (`target(H)`) in convergence hash

## Strictly Forbidden Changes

- Modifying block header format (80-byte header layout)
- Adding asset fields to transaction outputs
- Writing Convergence outputs into UTXO
- Allowing multiple RES claims per block
- Allowing RES without winner
- Allowing winner without commitment
- Introducing non-deterministic consensus dependencies

## Implementation Discipline

For consensus changes, follow:

1. unify
2. isolate
3. verify
4. commit

Guidance:

- Change one layer at a time
- If a step breaks, revert only the latest step
- Avoid broad rewrites of validation paths in one patch
