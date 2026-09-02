# Consensus Kernel testnet/regtest canary (P0-069)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-TESTNET-CANARY-ACTIVATION-P0-069`  
**Date:** 2026-08-31  
**Nature:** Runtime compare on testnet/regtest. No mainnet activation. No protocol edit.

```
CONSENSUS_KERNEL_TESTNET_CANARY = CLOSED/PASS
NETWORK                         = REGTEST/TESTNET
BLOCKS_VALIDATED                = 10000
DISCREPANCY_COUNT               = 0
CRASH_COUNT                     = 0
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
REFINEMENT_PROVED               = TRUE
EQUIVALENCE_PROVED              = FALSE
```

---

## Canary purpose

On a real node, when `-enableconsensuskernelcanary` is ON:

```
Block received
    |
ConnectBlock orchestration
    |
existing C++ authority          ← production decision
    |
migration candidate compare     ← observe-only
```

Compared: accept/reject, issuance, RES mint, draw winner, classification,
transaction predicate, ledger delta, coinbase expectation.

Mismatch token: `CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY`.
No auto-repair. No fallback acceptance.

---

## Rollback

| Knob | Production | Canary |
|---|---|---|
| `-enableconsensuskernelcanary` | **OFF** (default) | ON |
| Network | mainnet refuses `MAINNET_CANARY_FORBIDDEN` | regtest / testnet / devtest / slowtest |
| ConnectBlock decision | handwritten C++ | unchanged |

Rollback is omitting the flag (or restarting without it). The C++ authority
path is never replaced.

---

## Production remains OFF

```
EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
```

Mainnet cannot start with the flag. Release default is OFF. Logs under
`logs/consensus_kernel_canary/` are written only when the canary runs.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing kernel-boundary refinement.
This TASK does not add a new proof.

`EQUIVALENCE_PROVED = FALSE`: a finite runtime compare is not a full-node
equivalence proof.

---

## Soak (regtest-shaped)

`RigorKernelRuntimeCanaryRunBlockStream(10000)` walks 10000 ConnectBlock-shaped
cases, not 10000 mined mainnet blocks. Coverage includes genesis (height 0),
issuance pins 1 / 99 / 100 / 101, halvings 209999 / 210000, RES mint maturity,
draw winner, valid/invalid spend, bad signature, malformed witness, double
spend, overflow, and invalid asset.

Parent-draw cases are never composed onto genesis: ConnectBlock's genesis
path has an empty draw set.

Logs: `logs/consensus_kernel_canary/` (written only when the canary runs).

Monitor: `tools/check_kernel_canary_runtime.py`
(discrepancy, crash, reject divergence, performance).
