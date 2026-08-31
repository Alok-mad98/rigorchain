# Consensus Upgrade Policy (P0-061)

**Task:** `TASK-RIGOR-CONSENSUS-UPGRADE-POLICY-P0-061`  
**Date:** 2026-08-30  
**Nature:** Process lock. No new protocol rules.

```
UPGRADE_POLICY = PASS
```

---

## Rule

Any future **consensus rule change** must update all of the following in
the same change:

1. C++ Kernel (the unique production body)
2. Coq model (the unique formal body)
3. refinement proof (P056–P059 modules that cover that domain)
4. golden / frozen vectors
5. governance manifest (`formal_verification/consensus_kernel_manifest.json`)
   and production inventory (`formal_verification/production_consensus_inventory.json`)

Missing any one item is a reject. Do not land a C++-only or Coq-only
consensus edit.

---

## What counts as a consensus rule change

A change to issuance, RES schedule, ORU subsidy, draw, supply delta,
classify, spend predicate, PoW digest, PoW check, or ASERT.

ConnectBlock orchestration that only prepares Kernel inputs or consumes
Kernel results is not a rule change, but it must not grow a formula copy.

---

## What this policy does not do

It does not authorize switching production to extracted OCaml.
It does not claim full-node, UTXO, LevelDB, network, wallet, miner,
CompCert, or VST proofs.
It does not flip `equivalence` to true.
