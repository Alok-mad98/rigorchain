# Formal consensus final freeze (P0-074)

**Task:** `TASK-RIGOR-FORMAL-CONSENSUS-FINAL-FREEZE-P0-074`  
**Date:** 2026-08-31  
**Nature:** Freeze of the completed formal consensus verification system as the genesis release baseline.

This TASK does not add consensus formulas, does not change production
behavior, and does not introduce a new Kernel implementation.

```
COQ_ADMITTED           = 0
COQ_AXIOMS             = 0
COQ_PARAMETERS         = 0
REFINEMENT_PROVED      = TRUE
EQUIVALENCE_PROVED     = FALSE
AUTHORITY_LINT         = PASS
REPRODUCIBILITY        = PASS
```

Manifest: `formal_verification/rigor_formal_final_manifest.json`  
Reproduce: `tools/reproduce_rigor_formal_release.sh`

Mainnet has not launched. There is no historical chain.

---

## Proven properties

Kernel-boundary refinement in Coq (`ConsensusKernelRefinement.v`),
`Print Assumptions` closed under the global context:

- Issuance: `GetBlockSubsidy` / `GetResMintReward` / `ComputeRigorIssuance`
- Classification: `ClassifyRigorTxout` (script bytes only; fail-closed)
- Draw: `ResolveConvergenceDraw(F, T)` (not window assembly)
- Supply delta: `RigorSupplyDelta` circulating Δ (fee-burn not in ledger Δ)
- Coinbase RES identity: expected coinbase is fees + bootstrap + accepted mint

Transaction Kernel (`TransactionKernelRefinement.v` + `SignatureBoundary.v`):

- `VerifyRigorPredicate` is the unique spend predicate
- Canonical sighash domain tag; no `SIGHASH_ALL` byte; no Script VM; no CHECKMULTISIG
- Fail-closed on empty / unknown / malformed witness / wrong key or signature

Ledger Kernel (`LedgerTransitionRefinement.v`):

- `LedgerApply` equals the Coq ledger apply on the modeled domain
- Value / asset conservation cases; double-spend / overflow / unknown-asset reject
- Maturity 99/100 reject, 101 accept on the modeled coinbase coin
- DB / LevelDB is **not** in the theorem (`db_not_proved`)

UTXO composition (`UTXOStateTransitionRefinement.v`):

- Explicit order: transaction gate → consensus gate → ledger transition
- Composition refinement of those three kernels; not a coinsdb proof

ConnectBlock boundary (`ConnectBlockInputPreparation.v`,
`KernelResultApplication.v`, `ConnectBlockKernelBoundary.v`):

- Prepare Kernel input (no DB / node context in the Kernel object)
- Consume Kernel result without recomputing formulas
- Full ConnectBlock is **not** proved (`connectblock_full_not_proved`)

Production C++ (P073, unchanged here):

- Unique production authority is `RigorConsensusKernel`
- ConnectBlock prepares input, calls the Kernel, consumes the result, applies state

---

## Non-proven properties

This freeze does **not** claim:

- full-node equivalence (C++ = Coq = Python)
- CompCert proof of the compiler
- VST / separation-logic proof of the node
- network, liveness, or partition tolerance
- LevelDB / `CCoinsView` / undo-file refinement
- a complete ConnectBlock proof (PoW, BIP30, script cache, index writes)
- that extracted OCaml is production authority

`EQUIVALENCE_PROVED` remains **FALSE**.

---

## Production boundary

```
ConnectBlock
    → RigorConsensusKernel
    → Transaction Kernel (VerifyRigorPredicate)
    → consume Kernel result
    → apply UTXO / index state
```

Single authority map (frozen):

| Domain | Authority |
|--------|-----------|
| issuance | `GetBlockSubsidy` |
| RES | `GetResMintReward` |
| classification | `ClassifyRigorTxout` |
| draw | `ResolveConvergenceDraw` |
| supply | `RigorSupplyDelta` |
| spend | `VerifyRigorPredicate` |
| ledger | `LedgerApply` |
| kernel | `RigorConsensusKernel` |

`LedgerApply` is the frozen ledger kernel. It is not compiled into `bitcoind`
(`Makefile.am` does not list `consensus/ledger`). ConnectBlock applies chainstate
after the Kernel accepts.

Extracted Coq/OCaml remains a verification oracle. It is not linked into
production.

---

## Trust assumptions

- coqc 8.15 on PATH; `Print Assumptions` reports no axioms on the inventory theorems
- Unique C++ formula bodies remain at the files in the manifest
- Golden / differential agreement is finite-vector evidence, not a ∀-proof of the node
- Operators trust the C++ compiler, OS, and hardware; those are outside this freeze
