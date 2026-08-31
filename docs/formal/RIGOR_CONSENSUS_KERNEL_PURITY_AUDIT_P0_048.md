# Consensus Kernel Purity Audit (P0-048)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-PURITY-AUDIT-P0-048`  
**Date:** 2026-08-30  
**Nature:** Static + test audit of the P047 C++ Kernel. No protocol change. No Coq. Not a refinement.

```
PRODUCTION_CONSENSUS_MODIFIED = NO
CONSENSUS_PARAMETERS_MODIFIED = NO
COQ_MODIFIED                  = NO
equivalence_claimed           = false
refinement_proved             = false
```

This document answers whether the P047 surface is a small, explicit-input,
deterministic, fail-closed Kernel — not whether Coq equals C++.

---

## Genesis / RES (do not conflate)

| Fact | Value |
|---|---|
| `GetResMintReward(0)` schedule | `RES_REWARD_GENESIS` = 40 RES |
| `GetBlockSubsidy(0)` | 0 ORU |
| Genesis ConnectBlock application | genesis coinbase is not circulating issuance |

P047 did not, and this audit must not, change `GetResMintReward(0)` to 0
because genesis does not apply a mint.

---

## Issuance boundary (P040/P041, not redesigned)

```
epoch(h)              = floor(h / 210000)
epoch(209999)         = 0
epoch(210000)         = 1
FIRST_ISSUANCE_HEIGHT = 1
FIRST_HALVING_HEIGHT  = 210000
HALVING_INTERVAL      = 210000
ORU and RES share that boundary
Σ h=1..209999 ORU     = 209999000
Σ h=1..210000 ORU     = 209999500
```

No production `(height-1)/210000` exists. Test oracles that re-derive the
same sums are oracles, not a second production schedule.

---

## 1. P047 Kernel boundary

**In the Kernel (audit target):**

| Function | Role |
|---|---|
| `RigorIssuanceEpoch` / `GetBlockSubsidy` / `ComputeRigorIssuance` / `GetResMintReward` | issuance |
| `RigorSupplyDelta` / `RigorResCoinbaseExpected` | supply + RES budget identity |
| `ResolveConvergenceDraw` | pure draw |
| `ClassifyRigorAsset` → `AssetTypeFromScriptPubKey` → `ClassifyRigorTxout` | classify |
| Named coinbase predicates in `rigor_coinbase_layout.h` | layout names only |

**Not Kernel:** `ConnectBlock`, `GetConvergenceDrawSetForBlock`, UTXO apply,
DB/index, P2P, RPC, wallet, miner, pool, `VerifyRigorPredicate`.

```
state
  ↓
GetConvergenceDrawSetForBlock     (state assembly; validation.cpp)
  ↓
F  (= ConvergenceDrawSet)
  ↓
ResolveConvergenceDraw(F, T)      (pure)
  ↓
winner | none
```

---

## 2–3. Per-function inputs and state

### Issuance

| Function | Inputs | Hidden state |
|---|---|---|
| `RigorIssuanceEpoch(h)` | `h` | none |
| `GetBlockSubsidy(h, params)` | `h`, subsidy fields of `params` | none |
| `ComputeRigorIssuance(h, params)` | those plus `GetResMintReward(h)` | none |

`params` is an explicit argument. It is not tip, UTXO, or a singleton.

Negative `h` is not a production input. `RigorIssuanceEpoch(h<0)=0`.
`GetBlockSubsidy(h<0)` does not take the genesis branch (`h==0` only) and
follows the ordinary formula (on mainnet, `GetBlockSubsidy(-1)` = base).
Representational risk only. API kept (`int height`).

### Supply

`RigorSupplyDelta(in, params, out, state)`:

| `in` field | Why required |
|---|---|
| `height` | subsidy / W(h) |
| `fees_oru`, `fee_burn` | RFC-018 claimable ORU |
| `fees_res`, `res_bootstrap`, `res_mint`, `res_coinbase` | RES layout |
| `oru_coinbase` | ORU cap |
| `converge_sum` | circulating Δ |
| `has_winner`, `draw_cardinality` | mint gates |

`state` is `BlockValidationState`: **reject-reason output**. Fields written
on failure (`Invalid`): result enum, reject string, debug string. Not read
to decide the formula. Not chainstate.

`params` is used only through `GetBlockSubsidy`. RES schedule is
`GetResMintReward(in.height)`.

Does **not** mutate UTXO, write DB, or broadcast.

### Draw

`ResolveConvergenceDraw(set, T)`:

| Input | Meaning |
|---|---|
| `set.candidates` | F |
| `set.source_block_hash`, `set.source_height`, `set.convergence_root` | σ preimage |
| `T` | `res_probability_target` |

σ is `Hash256(txid ‖ C ‖ B_{h-1} ‖ LE32(h))` with `h = source_height+1`.
No chain/DB/clock/RNG. Winner is 512-bit argmin + smaller-txid tie-break,
so a permutation of `candidates` does not change the winning txid.

### Classification

`ClassifyRigorAsset(spk)` → `AssetTypeFromScriptPubKey` → `ClassifyRigorTxout`.
Unrecognized → `ASSET_INVALID`. No `ClassifyRigorSpendPredicate`.

### Coinbase layout

| Item | Class |
|---|---|
| `Parse` / `ValidateCoinbaseConvergenceRootScriptSig` | consensus predicate |
| `CoinbaseHasResMint` | consensus predicate |
| `RigorResCoinbaseExpected` | consensus predicate |
| `ConnectEvaluateCoinbaseResMint` | evaluation (orchestration still calls it) |
| `BuildCoinbaseCommitmentScriptSig` | **construction** (miner/GBT). Named in the header; Kernel does not call it |
| BIP141 witness commitment | `CheckBlock` (not extracted) |
| extranonce | construction, not Kernel |

P041 GBT copy-bytes path is untouched.

---

## 4. Forbidden dependencies

P047 Kernel **files** do not include `validation.cpp`, node `validation.h`,
`chain.h`, `txdb`, `dbwrapper`, `rpc/`, `wallet/`, P2P, miner, or pool.

`#include <consensus/validation.h>` in `rigor_supply.cpp` is the reject-type
header, not `validation.cpp`.

`connect_block_rigor_checks.cpp` remains orchestration and may use
`CChainParams`. It is not Kernel.

---

## 5. Side effects / globals

| Location | Effect | Consensus-relevant? |
|---|---|---|
| `RigorSupplyDelta` `LogPrintf` | stderr/log | no |
| Kernel files | no `static` mutable, no TLS, no `getenv`, no RNG, no clock | — |

`HIDDEN_STATE` for consensus inputs: **none**.

---

## 6. Determinism

Same numeric/script/set inputs produce the same outputs. Draw comparison is
a total order. No unordered containers in the Kernel functions.

---

## 7. Fail-closed

| Input | Result |
|---|---|
| subsidy base/interval ≤ 0 | ORU 0 |
| missing/unauthorized/underflow mint | named reject |
| ORU/RES coinbase mismatch | named reject |
| empty / wrong-length / bad-version / malformed script | `ASSET_INVALID` |
| unset draw set / N < min / T=0 / weight 0 | no winner |

No fallback to ORU/RES on unknown scripts.

---

## 8. Single-authority matrix

| Concept | Canonical authority | Other | Status |
|---|---|---|---|
| issuance epoch | `height / 210000` (`RigorIssuanceEpoch` / `GetResMintReward`) | tests re-derive the same division | PASS (oracles) |
| ORU issuance | `GetBlockSubsidy` | callers (RPC, miner, observer) | PASS (one body) |
| RES issuance | `GetResMintReward` | same | PASS |
| asset classification | `ClassifyRigorTxout` | `ClassifyRigorAsset` façade; `Solver` / `IsResScript` call the same matcher | PASS |
| draw resolution | `ResolveConvergenceDraw` | none | PASS |
| supply delta | `RigorSupplyDelta` | ConnectBlock only calls it | PASS |
| coinbase RES budget | `RigorResCoinbaseExpected` | `VerifyCoinbaseConvergenceAndResLayout` uses it | PASS |

`VerifyRigorPredicate` re-parses spend templates. That is Transaction Kernel
dispatch, not a second **asset** classifier (P046). Out of this Kernel.

---

## 9. Formalization readiness

| Function | Ready | Isolation note |
|---|---|---|
| issuance | YES | already a small TU |
| `RigorSupplyDelta` | YES | drop `LogPrintf` later if a model forbids I/O; not a semantic change |
| `ResolveConvergenceDraw` | YES | still shares `convergence_logic.cpp` with window builders |
| `ClassifyRigorAsset` | YES | TU includes `predicate_verify.h` |
| coinbase predicates | YES for `RigorResCoinbaseExpected` / parse | `Build*` is construction |

P049 should start with issuance, classification, draw, supply_delta.
Not PoW / OpuHash / ASERT.

---

## 10. Why this task did not change the protocol

No production discrepancy was found. Changing `GetResMintReward(0)`,
isolating `ResolveConvergenceDraw` into a new TU, or deleting `LogPrintf`
would be cleanup, not an audit requirement, and the first would be a
protocol change. P048 stops at reporting.

---

## 11. Open items (not production bugs)

1. Mixed draw TU (`convergence_logic.cpp` includes `chain.h` / `coins.h`).
2. Classify TU includes Transaction Kernel headers.
3. `rigor_coinbase_layout.h` *names* a constructor (`BuildCoinbaseCommitmentScriptSig`).
4. `int` height cannot express fuzz `uint64`; production heights are `int`.
5. `mining_basic.py` P2P tail timeout (getdata unanswered) is outside this Kernel.

Machine inventory: `formal_verification/consensus_kernel_purity.json`.

---

## 12. Discrepancies

No production-semantic discrepancy was found.

`GetResMintReward(0) == RES_REWARD_GENESIS` is the schedule value.
Genesis ConnectBlock application of circulating issuance is a different
fact (`GetBlockSubsidy(0) == 0`; genesis coinbase is not circulating).
Those two facts remain distinct. This audit did not collapse them.

---

## 13. Not covered (out of this TASK)

- Coq refinement / CompCert / VST / Extraction
- PoW Kernel, OpuHash, ASERT, nBits
- Transaction Kernel / sighash / Ed25519
- miner / pool / GBT / wallet / P2P implementation
- website / explorer
- `mining_basic.py` P2P timeout (infrastructure; node sent getdata and
  never received the block). Not a Kernel reject.

This TASK did not edit Coq sources. The working tree already contained
unrelated prior-task files (`PowDigest.v`, `_CoqProject` additions,
SPEC-008 nBits). Those are not P048 changes and were not used as
grounds to alter this Kernel.

---

## 14. Tests run (2026-08-30)

| Check | Result |
|---|---|
| `tools/lint-consensus-kernel-purity.py --self-test` | PASS (synthetic `validation.h` + `ConnectBlock` + `GetTime` detected) |
| `tools/lint-consensus-kernel-purity.py` | PASS (7 Kernel files) |
| `test_rigor` kernel / issuance / classify / draw / validation suites | PASS (78 cases) |
| `run_differential_gate.sh` | PASS (golden lock 433/226; honesty flags remain false) |
| `mining_solo_gbt_copy_bytes.py` | PASS (heights 1–3; GBT copy-bytes boundary intact) |
| `feature_res_maturity.py` | PASS |
| Golden vector files vs HEAD | unchanged |

---

## 15. Verdict

```
============================================================
CONSENSUS KERNEL PURITY AUDIT
============================================================

KERNEL_PURITY_STATUS              = PASS

ISSUANCE_PURITY                    = PASS
SUPPLY_PURITY                      = PASS
DRAW_PURITY                        = PASS
CLASSIFICATION_PURITY              = PASS
COINBASE_LAYOUT_PURITY             = PASS

EXPLICIT_INPUTS                    = PASS
HIDDEN_STATE                       = NONE
SIDE_EFFECTS                       = FOUND
GLOBAL_MUTABLE_STATE               = NONE

DETERMINISM                        = PASS
FAIL_CLOSED                        = PASS
SINGLE_AUTHORITY                   = PASS

DEPENDENCY_DIRECTION               = PASS
FORBIDDEN_DEPENDENCIES             = NONE

ISSUANCE_CANONICAL                 = PASS
ORU_RES_BOUNDARY                   = PASS
GENESIS_SEMANTICS                  = PASS

SUPPLY_FORMULA                     = PASS
DRAW_PURE_BOUNDARY                 = PASS
CLASSIFICATION_SINGLE_PATH         = PASS
COINBASE_ASSEMBLER_ABSENT          = PASS

FORMALIZATION_READY                = YES

COQ_MODIFIED                       = NO
PRODUCTION_CONSENSUS_MODIFIED      = NO
CONSENSUS_PARAMETERS_MODIFIED      = NO
GOLDEN_VECTORS_MODIFIED            = NO

STATIC_LINT                        = PASS
DIFFERENTIAL_GATE                  = PASS
CORE_TEST                          = PASS
FUNCTIONAL_TEST                    = PASS

============================================================

FINAL_VERDICT = CONSENSUS_KERNEL_PURITY_CLOSED
```

`SIDE_EFFECTS = FOUND` is `LogPrintf` on ORU/RES coinbase reject.
It does not change accept/reject or numeric outputs. Criterion 2
requires the absence of *consensus-relevant* side effects; that holds.

`FORMALIZATION_READY = YES` means the functions are explicit-input,
deterministic, fail-closed, and single-authority enough to enter P049
alignment work. It is **not** a Coq ↔ C++ equivalence claim.

P049, if opened, should start with issuance, classification, draw,
and `RigorSupplyDelta`. Not PoW / OpuHash / ASERT.
