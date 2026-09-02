# Consensus Kernel Mainnet Readiness Audit (P0-061)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-MAINNET-READINESS-AUDIT-P0-061`  
**Date:** 2026-08-30  
**Nature:** Pre-mainnet Kernel audit. No new protocol rules.

```
CONSENSUS_KERNEL_MAINNET_READINESS = PASS
PRODUCTION_CALLGRAPH               = FROZEN
AUTHORITY_INVENTORY                = PASS
EXTRACTED_KERNEL_ISOLATION         = PASS
RELEASE_BUILD_AUDIT                = PASS
FORMAL_REPRODUCIBILITY             = PASS
UPGRADE_POLICY                     = PASS
COQ_ADMITTED                       = 0
COQ_AXIOMS                         = 0
PRODUCTION_CONSENSUS_MODIFIED      = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED                  = TRUE
EQUIVALENCE_PROVED                 = FALSE
```

---

## Path under audit

```
Formal Kernel
      |
      v
Production C++ Kernel
      |
      v
ConnectBlock Boundary
      |
      v
Node Release
```

No unregistered drift was found between these layers. Production still
executes handwritten C++ Kernel bodies. Extracted OCaml remains an
oracle.

---

## Inventory

`formal_verification/production_consensus_inventory.json`

| Domain | Authority |
|---|---|
| issuance | `GetBlockSubsidy` |
| RES schedule | `GetResMintReward` |
| supply | `RigorSupplyDelta` |
| draw | `ResolveConvergenceDraw` |
| classification | `ClassifyRigorTxout` |
| spend | `VerifyRigorPredicate` |
| PoW | `PowBoundary` |

---

## Release isolation

`tools/audit-consensus-release-build.py` rejects:

- `formal_verification/extracted` runtime in the node
- OCaml runtime / `caml_startup` / `consensus_kernel.ml`
- verify-only ABI, shadow adapters, txkernel test input, oracle CLIs

Production release may contain only the C++ Consensus Kernel, Transaction
Kernel (`VerifyRigorPredicate`), and PoW implementation.

Leak fail: `EXTRACTED_KERNEL_LEAK_TO_PRODUCTION`.

---

## Reproducibility

`tools/reproduce-consensus-proof.sh` runs `coqc` on P056–P059, Print
Assumptions, the refinement gate, the differential gate, the authority
lint, and the Coq assumption gate. Success prints
`CONSENSUS_FORMAL_BUILD_REPRODUCIBLE`.

---

## Unchanged

```
PRODUCTION_CONSENSUS_MODIFIED = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
ISSUANCE_CHANGED              = NO
RES_CHANGED                   = NO
DRAW_CHANGED                  = NO
SUPPLY_CHANGED                = NO
POW_CHANGED                   = NO
ASERT_CHANGED                 = NO
```

---

## Non-goals

This audit does not claim, and must not be cited as:

- full-node proof
- UTXO database proof
- LevelDB proof
- network proof
- wallet proof
- miner proof
- CompCert proof
- VST proof
- equivalence proof
