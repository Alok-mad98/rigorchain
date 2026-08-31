# Consensus Kernel Production Call Graph (P0-061)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PRODUCTION-CALLGRAPH-P0-061`  
**Date:** 2026-08-30  
**Nature:** Frozen production path. No new protocol rules.

```
PRODUCTION_CALLGRAPH = FROZEN
PRODUCTION_KERNEL_SWITCHED = NO
```

---

## Final production path

```
Block Input
      |
      v
CheckProofOfWork          (PowBoundary: GetRigorPowHash vs nBits target;
                           header admission via ContextualCheckBlockHeader /
                           HasValidProofOfWork)
      |
      v
ConnectBlock              (orchestration only)
      |
      +------------------+
      |                  |
      v                  v
ConsensusKernel     TransactionKernel
  GetBlockSubsidy     VerifyRigorPredicate
  GetResMintReward
  RigorSupplyDelta
  ResolveConvergenceDraw
  ClassifyRigorTxout
      |
      v
Accept / Reject
```

ConnectBlock prepares Kernel inputs and consumes Kernel results. It does
not contain issuance, draw, classify, or spend formula bodies.

PoW is a registered boundary. Non-genesis work is checked at header
admission. ConnectBlock re-invokes `CheckBlock`; genesis nBits sanity
goes through `CheckProofOfWork`. This TASK does not move PoW into
ConnectBlock.

---

## Forbidden edges

```
validation.cpp
      |
      +--> subsidy formula

validation.cpp
      |
      +--> draw algorithm

validation.cpp
      |
      +--> predicate parser
```

A second production body is a stop. Historical `SelectWinner` in
`selection.cpp` is not on this graph.

---

## Extracted oracle

`formal_verification/extracted/consensus_kernel.ml` remains
`VERIFICATION_ORACLE_ONLY`. It is not an edge on the production path.
Makefile.am must not list `consensus_kernel.ml`, `caml_startup`, or an
OCaml runtime.
