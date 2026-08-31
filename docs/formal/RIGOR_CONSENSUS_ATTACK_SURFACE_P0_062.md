# Consensus Attack Surface (P0-062)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-AUDIT-PACKAGE-P0-062`  
**Date:** 2026-08-30  
**Nature:** Attack-surface map. No new protocol rules.

---

## Consensus critical

These unique production authorities must stay single-body. A second
implementation, or a ConnectBlock formula copy, is a stop.

| Surface | Authority |
|---|---|
| ORU issuance | `GetBlockSubsidy` |
| RES schedule | `GetResMintReward` |
| supply | `RigorSupplyDelta` |
| draw | `ResolveConvergenceDraw` |
| classification | `ClassifyRigorTxout` |
| spend | `VerifyRigorPredicate` |
| sighash | `SignatureHash` (canonical write in Transaction Kernel) |
| PoW | `PowBoundary` (`CheckProofOfWork` / `GetRigorPowHash` / ASERT) |

Protect these. Do not reimplement them in `validation.cpp`, wallet,
miner, GBT, or extracted OCaml production linkage.

---

## Non-kernel

Explicitly out of this Kernel audit surface:

- wallet
- miner
- GBT
- RPC
- P2P
- database
- explorer

Historical `SelectWinner` in `selection.cpp` is not ConnectBlock
authority. Extracted `consensus_kernel.ml` is
`VERIFICATION_ORACLE_ONLY`.
