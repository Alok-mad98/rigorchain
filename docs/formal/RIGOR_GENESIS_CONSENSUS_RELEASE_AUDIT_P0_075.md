# Genesis consensus release audit (P0-075)

**Task:** `TASK-RIGOR-GENESIS-CONSENSUS-RELEASE-AUDIT-P0-075`  
**Date:** 2026-08-31  
**Nature:** Genesis launch audit. There is no historical chain and no
compatibility requirement.

This TASK does not add consensus formulas, migration logic, or a
compatibility layer. It does not keep canary infrastructure on the
production path.

```
GENESIS_CONSENSUS_READY = TRUE
REFINEMENT_PROVED       = TRUE
EQUIVALENCE_PROVED      = FALSE
```

Manifest: `formal_verification/genesis_consensus_release_audit.json`  
Vectors: `formal_verification/genesis_consensus_release_vectors.json`  
Reproduce: `tools/run_genesis_consensus_release_audit.sh`

---

## Production call graph

```
Block
    → CheckProofOfWork
    → ConnectBlock
    → RigorConsensusKernel
    → TransactionKernel (VerifyRigorPredicate)
    → LedgerApply          (formal ledger kernel; not linked into bitcoind)
    → UTXO mutation        (ConnectBlock coins view after Kernel accept)
```

| Step | Production locus |
|------|------------------|
| Header / PoW | `CheckProofOfWork` (`pow.cpp`), called from header admission |
| Block connect | `Chainstate::ConnectBlock` prepares Kernel input only |
| Consensus Kernel | `RigorConsensusKernel::ResolveDraw` / `EvaluateConnect` |
| Transaction Kernel | `VerifyRigorPredicate` (`CScriptCheck`) |
| Ledger Kernel | `LedgerApply` — unique ledger-transition authority; **not** in `Makefile.am` |
| UTXO apply | `SpendCoin` / `AddCoins` after Kernel accept |

ConnectBlock does not calculate subsidy, RES schedule, draw, asset class,
or circulating Δ. Unique formula bodies remain:

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
| PoW | `CheckProofOfWork` |

Authority lint reports no second production body.

---

## Genesis vectors (frozen)

Height **0**:

| Pin | Value |
|-----|-------|
| ORU actual issuance | 0 |
| RES actual issuance | 0 |
| RES schedule | 40000000 |
| draw (empty F) | none |

Additional heights (schedule ≠ actual; empty F remains none):

| Height | ORU subsidy | RES schedule | RES actual without mint | Maturity (spend coin from h=1) |
|--------|-------------|--------------|-------------------------|--------------------------------|
| 1 | 100000000000 | 40000000 | 0 | immature |
| 99 | 100000000000 | 40000000 | 0 | immature |
| 100 | 100000000000 | 40000000 | 0 | immature |
| 101 | 100000000000 | 40000000 | 0 | mature |
| 210000 | 50000000000 | 14142196 | 0 | — |

`nCoinbaseMaturity = 100`. RES schedule is not actual issuance.

---

## Attack boundary

Each case accepts or rejects **deterministically** (fail-closed):

| Attack | Result |
|--------|--------|
| malformed transaction / unknown script | reject (`ClassifyRigorTxout` → INVALID; spend predicate fail-closed) |
| invalid signature | reject (`VerifyRigorPredicate`) |
| invalid asset | reject |
| overflow | reject (`RigorSupplyDelta` / `LedgerApply`) |
| duplicate spend | reject (`LedgerApply`) |
| invalid draw (empty F, T = 0) | none; mint without winner rejects |
| invalid maturity (spend at 99 or 100 of a height-1 coinbase) | reject |

Evidence: `test_genesis_consensus_release_audit.cpp`,
`test_ledger_transition_refinement.cpp`,
`test_consensus_kernel_adversarial.cpp`.

---

## Non-production tooling (not linked)

Production `Makefile.am` / `libbitcoin_node` / `libbitcoinkernel` do **not**
compile:

- `consensus/shadow`
- `consensus/kernel_canary` / `node/kernel_runtime_canary.cpp`
- `consensus/kernel_migration`
- extracted Coq/OCaml oracle (`caml_`, `consensus_kernel.ml`)
- C ABI `consensus/bridge`
- `consensus/ledger` (`LedgerApply` remains the formal kernel)

ConnectBlock does not call the canary observer. `-enableconsensuskernelcanary`
is not registered in production `init.cpp`.

Those trees may remain as unlinked test/audit sources. They are not genesis
production infrastructure.

---

## Honesty

`REFINEMENT_PROVED = TRUE` is the existing Kernel / Transaction / Ledger /
UTXO-composition / ConnectBlock-**boundary** Coq refinement (P074 freeze).

This audit does **not** claim:

- C++ = Coq = Python
- a full-node proof
- that `LedgerApply` is compiled into `bitcoind`
- that PoW is a Kernel-boundary refinement theorem
