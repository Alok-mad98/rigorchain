# UTXO Ledger Transition Refinement (P0-063)

**Task:** `TASK-RIGOR-UTXO-LEDGER-TRANSITION-REFINEMENT-P0-063`  
**Date:** 2026-08-31  
**Nature:** Closed ledger-transition boundary. No new protocol rules.

```
UTXO_LEDGER_REFINEMENT         = PASS
VALUE_CONSERVATION             = PASS
DOUBLE_SPEND_BOUNDARY          = PASS
ASSET_CONSERVATION             = PASS
COQ_ADMITTED                   = 0
COQ_AXIOMS                     = 0
PRODUCTION_CONSENSUS_MODIFIED  = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED              = TRUE
EQUIVALENCE_PROVED             = FALSE
```

---

## Boundary

```
UTXO input state
        →
Transaction Kernel accept/reject
        →
Consensus Kernel accept/reject
        →
UTXO delta application
        →
UTXO output state
```

`CppLedgerApply = CoqLedgerApply`.

Production still mutates UTXO with the existing C++ ConnectBlock path.
`core/src/consensus/ledger/` is verify-only and is not in `Makefile.am`.

---

## Conservation

Value: `inputs + ORU issuance + RES mint >= outputs + burn + fees`  
ORU: `input ORU + issuance − burn − converge − fees = output ORU`  
RES: `input RES + mint = output RES`

Reject: negative amount, overflow past `MAX_MONEY`, duplicate outpoint,
unknown asset, immature coinbase, Transaction/Consensus Kernel reject.

`NoDupOutpoints` is a single unsorted scan (same shape as Coq
`nodup_ops`). Do not restore a `n<=24` / `std::sort` split: that is two
algorithms and blocks the B1 Clight/VST funspec for this function.

Genesis height 0: actual issuance 0. `GetResMintReward(0)` stays
40000000 and is not applied as circulating issuance.

---

## Non-goals

This TASK does not claim:

- full-node equivalence
- Bitcoin Core equivalence
- LevelDB correctness
- network correctness
- wallet correctness
