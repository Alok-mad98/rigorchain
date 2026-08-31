# Consensus Kernel Production Integration (P0-051)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PRODUCTION-INTEGRATION-P0-051`  
**Date:** 2026-08-30  
**Nature:** Production call-graph / ABI / switch-decision audit. Not a refinement.

```
PRODUCTION_CONSENSUS_KERNEL_STATUS = NOT_READY
OPTION                             = C
PRODUCTION_EXECUTION_SOURCE        = handwritten_cpp
EXTRACTED_KERNEL_ROLE              = differential_oracle
PRODUCTION_KERNEL_SWITCH           = NOT_READY
REFINEMENT_PROVED                  = false
EQUIVALENCE_PROVED                 = false
PRODUCTION_CONSENSUS_MODIFIED      = NO
```

P050 produced an executable extracted kernel. This TASK answers which
kernel `ConnectBlock` actually runs. The answer is still the handwritten
C++ Consensus Kernel.

ALIGNMENT ≠ EXTRACTION ≠ DIFFERENTIAL AGREEMENT ≠ REFINEMENT ≠
PRODUCTION SWITCH.

---

## Answers

### Q1 — Can `consensus_kernel.ml` be the production execution source?

No. Not as a library linked into `bitcoind` / `libbitcoin_consensus` /
`libbitcoinkernel`.

The extracted artifact is a valid oracle. It is not a drop-in production
object: there is no OCaml runtime in the node build, `Z` is an inductive
bigint, draw inputs are Coq `Tx` records, and `apply_delta` is not
`RigorSupplyDelta`.

### Q2 — Can a C++ → extracted-kernel adapter exist?

A *test* adapter already exists (`consensus_kernel_cli.ml`,
`cpp_consensus_kernel_cli.cpp`). It only converts I/O.

An in-process production adapter would need `caml_startup`, the OCaml
runtime lock, exception marshalling, and `Z` ↔ `int64` / `uint256`
conversion on the `ConnectBlock` path. That is not a type-conversion
shim the current node can absorb without a dedicated runtime TASK.

### Q3 — Would a production adapter become a second kernel?

A C adapter that re-coded epoch / subsidy / RES / draw / classify /
supply in C or C++ would be a second kernel. This TASK does not add
`core/src/consensus/rigor_extracted_adapter.*` for that reason.

### Q4 — What does production execute?

```
ConnectBlock
    |
    +--> state assembly / UTXO / DB / orchestration
    |
    +--> handwritten C++ Consensus Kernel
             GetBlockSubsidy / GetResMintReward / RigorIssuanceEpoch
             ClassifyRigorTxout
             ResolveConvergenceDraw
             RigorSupplyDelta
             RigorResCoinbaseExpected
```

Extracted OCaml is used by the differential oracle only.

---

## Option decision

| Option | Meaning | Result |
|---|---|---|
| A | OCaml extracted kernel inside the production node | Rejected: no OCaml in `Makefile.am`; GC / exceptions / threads |
| B | Extracted native library + C ABI + C++ adapter | Rejected: same runtime; draw ABI is Coq `Tx`+`Sigma`, not `ConvergenceDrawSet`+SHA256; supply reject layer is C++-only |
| C | C++ remains production; extracted kernel is the oracle | **Selected** |

This is the documented acceptable-but-not-closed state:

```
Coq Canonical
      ↓
Extraction
      ↓
Executable Oracle
      ↕
Production C++ Kernel
      ↓
ConnectBlock
```

Finite differential agreement can PASS. Refinement stays false.
Production authority is explicit: **handwritten C++**.

The unacceptable state would be two production authorities with no
named winner. P051 names the winner and does not pretend the oracle is
linked.

---

## Production call graph

`ConnectBlock` (`core/src/validation.cpp`):

1. Genesis (`hashGenesisBlock`): record empty `F`, return. No subsidy
   application, no draw, no `RigorSupplyDelta`.
2. `GetConvergenceDrawSetForBlock` — state assembly, not Kernel.
3. `ResolveConvergenceDraw(parent F, T)` — handwritten C++ Kernel.
4. Transaction loop — `CheckTxInputs` / `ClassifyRigorTxout`.
5. `ConnectEvaluateCoinbaseResMint` — orchestration; schedule via
   `GetResMintReward`.
6. `RigorSupplyDelta` — handwritten C++ Kernel.
7. UTXO / index mutation.

Extracted functions are not in this graph.

Full machine record:
`formal_verification/production_consensus_call_graph.json`.

---

## ABI (why A/B stop)

| Surface | Extracted OCaml | Production C++ | Production FFI |
|---|---|---|---|
| height / amount | inductive `z` | `int` / `int64_t` | not linked |
| T / σ | inductive `z` | `uint256` | not linked |
| script | `int` list (`ExtrOcamlNatInt`) | `CScript` | not linked |
| classify | `AssetType` | `AssetType` | oracle CLI only |
| draw | `tx list → tx option` + Coq `Sigma` | `ConvergenceDrawSet` + `CalculateSigmaHash` | **blocked** |
| supply | `apply_delta` option | `RigorSupplyDelta` + reject reasons | **not a substitute** |

`Extract Constant` = none. No `Obj.magic`.

`RigorSupplyDelta` still owns winner-must-mint, trigger underflow, and
`bad-cb-amount`. `apply_delta` is the Coq UTXO accounting model. P049
already forbids unifying `oru_circulating_delta` with Coq circulating.

Full machine record:
`formal_verification/extracted_consensus_kernel_abi.json`.

---

## Duplicate Kernel audit

One production body per formula:

| Formula | Sole production body |
|---|---|
| `RigorIssuanceEpoch` | `rigor_issuance.h` |
| `GetBlockSubsidy` | `rigor_issuance.cpp` |
| `GetResMintReward` / `ResRewardForEpoch` | `rigor_consensus_lock.h` |
| `ClassifyRigorTxout` | `rigor_classify.cpp` |
| `ResolveConvergenceDraw` | `convergence_logic.cpp` |
| `RigorSupplyDelta` | `rigor_supply.cpp` |
| `RigorResCoinbaseExpected` | `rigor_supply.h` |

`ClassifyRigorAsset` / `AssetTypeFromScriptPubKey` / `Solver` are
aliases of `ClassifyRigorTxout`, not second matchers.

Callers (`miner.cpp`, RPC, `auto_res_mint`, observational
`GetChainOruTotal` / `ComputeDerivedOruSubsidyCumulative`) invoke those
bodies. They do not restate `floor(h/210000)` or `W(k)`.

`(height-1)/210000` does not appear in Kernel files.

Lint: `tools/lint-consensus-kernel-production-source.py`.

`WindowGeneric.oru_subsidy_model = 1000` remains Coq bookkeeping. It is
not a production or extraction authority.

---

## Genesis (unchanged)

| Fact | Value |
|---|---|
| `GetBlockSubsidy(0)` | 0 |
| `oru_issued_at_height(0)` | 0 |
| `GetResMintReward(0)` | `RES_REWARD_GENESIS` = 40000000 |
| `res_issued_at_height(0)` | 0 |
| Genesis draw | none |
| Genesis circulating Δ | 0 |

Schedule at height 0 was not set to 0.

---

## Issuance / maturity (unchanged)

```
epoch(h)              = floor(h / 210000)
epoch(209999)         = 0
epoch(210000)         = 1
FIRST_ISSUANCE_HEIGHT = 1
FIRST_HALVING_HEIGHT  = 210000
ORU maturity          = 100
RES maturity          = 100
```

ORU and RES share the epoch.

---

## What this TASK did not do

- Did not modify issuance, epoch, RES schedule, maturity, draw,
  classification, supply, coinbase, PoW, ASERT, genesis, sighash, or
  Ed25519.
- Did not switch `ConnectBlock` to extracted OCaml.
- Did not add a production adapter that reimplements the Kernel.
- Did not add Phase5 bookkeeping theorems.
- Did not flip `REFINEMENT_PROVED` or `EQUIVALENCE_PROVED`.
- Did not modify P041 GBT.

---

## Tests

| Check | Role |
|---|---|
| `extract_consensus_kernel.sh` | Coq extract + oracle CLIs |
| `test_consensus_kernel_extraction.py` | P050 tripartite |
| `test_consensus_kernel_production_integration.py` | P051 heights + Option C invariants |
| `lint-consensus-kernel-production-source.py` | no extracted link; one body per formula |
| `run_differential_gate.sh` | existing golden / hash / classify gate |
| `mining_solo_gbt_copy_bytes.py` | P041; not re-run here (`bitcoind` / `test_rigor` absent). Miner/GBT sources unmodified. |

---

## Next TASK

A later TASK may attempt Option B only after a dedicated, audited C ABI
that does **not** reimplement Kernel formulas and does **not** put the
OCaml GC on `ConnectBlock` without a runtime design. Until then:

```
HANDWRITTEN_CONSENSUS_KERNEL = PRODUCTION_AUTHORITY
EXTRACTED_KERNEL             = DIFFERENTIAL_ORACLE
```
