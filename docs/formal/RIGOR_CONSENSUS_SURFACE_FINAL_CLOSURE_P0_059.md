# Consensus Surface Final Closure (P0-059)

**Task:** `TASK-RIGOR-CONSENSUS-SURFACE-FINAL-CLOSURE-P0-059`  
**Date:** 2026-08-30  
**Nature:** Surface-closure audit. No new protocol rules.

```
CONSENSUS_SURFACE_CLOSURE      = CLOSED
CONSENSUS_AUTHORITY_SINGLE     = PASS
CONSENSUS_DECISION_COVERAGE    = PASS
TRANSACTION_KERNEL_BOUNDARY    = PASS
CONSENSUS_KERNEL_BOUNDARY      = PASS
POW_BOUNDARY_REGISTERED        = PASS
DUPLICATE_AUTHORITY            = NONE
COQ_ADMITTED                   = 0
COQ_AXIOMS                     = 0
PRODUCTION_KERNEL_SWITCHED     = NO
REFINEMENT_PROVED              = TRUE
EQUIVALENCE_PROVED             = FALSE
```

`Print Assumptions ConsensusSurfaceClosed` is
`Closed under the global context`.

This is **surface closure**: every named consensus accept/reject decision
belongs to one already-defined formal boundary. It is not a full-node
refinement, not CompCert, not VST, not LevelDB, not network, not miner,
and not wallet.

---

## Acceptance path

```
Block received
    → PoW boundary
    → ConnectBlock orchestration
    → ConsensusKernel
    → TransactionKernel
    → Accept / Reject
```

---

## Authority table

| Domain | Authority |
|---|---|
| ORU issuance | `GetBlockSubsidy` |
| RES schedule | `GetResMintReward` |
| supply | `RigorSupplyDelta` |
| draw | `ResolveConvergenceDraw` |
| classify | `ClassifyRigorTxout` |
| spend | `VerifyRigorPredicate` |
| sighash | canonical `SignatureHash` |
| signature | Ed25519 boundary |
| PoW digest | `GetRigorPowHash` |
| ASERT | `GetNextWorkRequired` / `CalculateNextWorkRequired` |

A second production body is a stop: `DUPLICATE_CONSENSUS_AUTHORITY_FOUND`.
This TASK does not rewrite formulas to clear that flag.

---

## Formal files

| File | Role |
|---|---|
| `ConsensusSurfaceClosure.v` | `ConsensusSurfaceClosed`; path mapping |
| `PowDigest.v` | PoW digest evidence (not re-proved here) |
| P056 / P057 / P058 modules | Kernel / ConnectBlock / Transaction boundaries |

PoW is **registered**: `PowKernelInput` is header80 / nBits / target / ASERT
parameters; output is accept/reject. Keccak and ASERT math are not
reimplemented.

---

## Non-authoritative

`Phase5Equivalence`, `legacy_account_model`, and `VACUOUS-PLACEHOLDER`
are `NON_AUTHORITATIVE`. They are not refinement evidence.

---

## Unchanged

`PRODUCTION_CONSENSUS_MODIFIED = NO`  
`ISSUANCE_CHANGED = NO`  
`RES_CHANGED = NO`  
`POW_CHANGED = NO`  
`ASERT_CHANGED = NO`  
P053 / golden-lock JSON `refinement_proved` stays `false`.
