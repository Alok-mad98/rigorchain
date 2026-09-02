# Consensus Kernel Governance (P0-060)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-GOVERNANCE-CLOSURE-P0-060`  
**Date:** 2026-08-30  
**Nature:** Long-term authority lock. No new protocol rules.

```
CONSENSUS_KERNEL_GOVERNANCE    = CLOSED
AUTHORITY_LOCK                 = PASS
FORMAL_ASSUMPTION_GATE         = PASS
GOLDEN_VECTOR_FREEZE           = PASS
DUPLICATE_RULE_DETECTION       = PASS
CI_PROTECTION                  = PASS
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

## Kernel authority

| Domain | Production authority |
|---|---|
| ORU issuance | `GetBlockSubsidy` |
| RES schedule | `GetResMintReward` |
| issuance pack | `ComputeRigorIssuance` |
| supply | `RigorSupplyDelta` |
| draw | `ResolveConvergenceDraw` |
| classify | `ClassifyRigorTxout` / `ClassifyRigorAsset` |
| spend | `VerifyRigorPredicate` |

A second production body, or a ConnectBlock formula copy, is a stop:
`DUPLICATE_CONSENSUS_AUTHORITY_FOUND`. This TASK does not rewrite formulas
to clear that flag.

`SelectWinner` remains in `selection.cpp` as a historical non-ConnectBlock
selection helper. ConnectBlock draw authority is `ResolveConvergenceDraw`
only. `validation.cpp` must not call `SelectWinner`.

---

## Formal boundary

Recorded in `formal_verification/consensus_kernel_manifest.json`:

- `refinement`: true (P056–P059 Kernel / Transaction / surface boundary)
- `equivalence`: false

`tools/check-coq-consensus-assumptions.py` compiles the Kernel Coq modules
and runs `Print Assumptions`. Required counts:

- `COQ_ADMITTED = 0`
- `COQ_AXIOMS = 0`
- `COQ_PARAMETERS = 0`

Drift is `CONSENSUS_FORMAL_ASSUMPTION_DRIFT`.

Golden pins live in `formal_verification/frozen_consensus_vectors.json`
(genesis, epoch heights, spend cases, draw cases).

---

## Production boundary

Future edits must keep:

- Consensus Kernel
- Transaction Kernel
- PoW Boundary

single authority, single formula body, single formal mapping.

ConnectBlock remains orchestration: it prepares Kernel inputs and consumes
Kernel results. It must not recompute issuance, circulating delta, draw
winner, classify, or spend.

CI: `.github/workflows/consensus-kernel-integrity.yml`  
Fail: `CONSENSUS_KERNEL_INTEGRITY_FAILED`

---

## Non-goals

This TASK does not claim, and must not be cited as:

- full-node proof
- equivalence proof
- Bitcoin correctness proof
- UTXO / LevelDB / P2P / miner / wallet proof
- CompCert or VST
- PoW algorithm proof (PoW is a registered boundary)

P053 finite-vector JSON `refinement_proved` stays `false`.
