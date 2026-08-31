# UTXO State Transition Refinement (P0-065)

**Task:** `TASK-RIGOR-UTXO-STATE-TRANSITION-REFINEMENT-CLOSURE-P0-065`  
**Date:** 2026-08-31  
**Nature:** Composition refinement of already-closed kernels. No new protocol rules.

```
UTXO_STATE_TRANSITION_REFINEMENT = PASS/CLOSED
COQ_ADMITTED                     = 0
COQ_AXIOMS                       = 0
COQ_PARAMETERS                   = 0
PRODUCTION_CONSENSUS_MODIFIED    = NO
PRODUCTION_KERNEL_SWITCHED       = NO
REFINEMENT_PROVED                = TRUE
EQUIVALENCE_PROVED               = FALSE
```

---

## What this is

A single explicit predicate:

```
UTXOStateTransition(S, T) = S'
```

C++ production-shaped transition equals the Coq canonical transition
on that explicit input.

Internal authorities (called, not reimplemented):

- TransactionKernel (`VerifyRigorPredicateBoundary`)
- ConsensusKernel (`ComputeRigorIssuance`, `ResolveConvergenceDraw`, `ClassifyRigorAsset`)
- LedgerKernel (`LedgerApply`)

---

## Boundary

```
explicit YES
hidden state NONE
side effect NONE
global mutable NONE
deterministic YES
single authority YES
```

Input `UTXOStateTransitionInput` carries the UTXO snapshot abstraction,
Transaction / Consensus / Ledger kernel inputs, fee data, asset metadata,
height, and maturity context.

Output `UTXOStateTransitionResult` is Accept / Reject plus `LedgerDelta`
(additions and removals).

---

## Production path (not switched)

```
ConnectBlock
    → prepare input
    → UTXOStateTransition
    → apply result
```

This TASK does not cut production. `core/src/consensus/utxo/` is a
verification oracle only and is not in `Makefile.am`.

---

## Theorems

- `transaction_gate` — TransactionKernel accept ⇒ assembled ledger TX accept
- `consensus_gate` — ConsensusKernel accept ⇒ issuance / schedule predicates
- `ledger_transition` — `CppLedgerApply = CoqLedgerApply` (P063)
- `state_transition_refinement` — `valid_input S T ⇒ Cpp = Coq`

---

## Non-goals

This TASK does not claim:

- full-node equivalence
- LevelDB proof
- network proof
- wallet proof
- CompCert proof
- VST proof

`REFINEMENT_PROVED = TRUE`  
`EQUIVALENCE_PROVED = FALSE`
