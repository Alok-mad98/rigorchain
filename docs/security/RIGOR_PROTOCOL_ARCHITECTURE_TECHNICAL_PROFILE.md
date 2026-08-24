# RIGOR Protocol Architecture Technical Profile

**Task:** `TASK-RIGOR-PROTOCOL-ARCHITECTURE-TECHNICAL-PROFILE-P0-001`  
**Date:** 2026-08-22  
**Layer:** Architecture analysis (read-only)  
**Consensus impact:** **NONE**  
**Scope:** Current tree as implemented. This document does not change consensus, RFCs, wallet spend logic, miner logic, PoW, or signatures.

Honesty flags (must remain false until a refinement proof exists):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Related living documents (not superseded):

- [`../architecture/ARCHITECTURE.md`](../architecture/ARCHITECTURE.md) — storage invariant and “script is the compliance/classification layer”
- [`RIGOR_SCRIPT_TYPE_REGISTRY.md`](./RIGOR_SCRIPT_TYPE_REGISTRY.md) — older witness-v1 CVG2 wording; **live code uses `OP_CONVERGE`**
- [`../architecture/RIGOR_NO_GLOBAL_FENCE.md`](../architecture/RIGOR_NO_GLOBAL_FENCE.md) — W1 / W2 / W7 observational fence
- [`FORMAL_VERIFICATION_COVERAGE_AUDIT.md`](./FORMAL_VERIFICATION_COVERAGE_AUDIT.md) — FV classification
- [`../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) — living FV boundary

---

## Executive Summary

RIGOR is a Bitcoin-derived UTXO ledger with **exactly two native assets**. Asset class is inferred from `scriptPubKey`. `CTxOut` storage remains `(nValue, scriptPubKey)` with **no parallel asset field**. Ownership changes only by spending outputs. There is no consensus asset registry, account-balance table, contract storage, or token VM.

The accurate identity model is:

```
2 Native Assets
+
N Asset-Bearing Scripts
+
M Formal Verification Properties (model-only)
+
No Global Asset-State Dependency
+
UTXO Local Validation
+
Ed25519 Authentication
+
Custom PoW Security Model
=
RIGOR Architecture
```

Current measured values (this audit):

| Symbol | Live value | Bound |
|--------|------------|--------|
| Native assets | **2** (ORU, RES) | `ASSET_NONE` is data, not an asset. Retired `ASSET_CVG` is not live. |
| Core asset-bearing scripts | **4** | Witness v0 ORU spend, Witness v1 RES spend, `OP_CONVERGE`, `OP_RES_MINT` |
| Formal properties | **221** Coq modules; **215** Coq-enforced golden lemmas; **2,850** named results (includes Phase catalogs) | **Not** a C++ equivalence count |
| Global asset state | **None as spend authority** | Observational ledgers exist and are fenced |

Official terminology is **script is the compliance layer**, not “script alone is the complete asset.” Classification is parse-time. Legitimacy still requires UTXO conservation, mint/draw predicates, and ConnectBlock.

```
equivalence_claimed = false
```

---

## 1. Native Asset Model

### 1.1 Native asset count

```
enum AssetType : uint8_t {
    ASSET_ORU  = 0,
    ASSET_RES  = 1,
    ASSET_NONE = 2,   // OP_RETURN / data carrier; nValue must be 0
    // 3 was ASSET_CVG (retired OP_CVG_CLAIM). Do not reassign.
};
```

Source: `core/src/primitives/transaction.h`.

**Native asset count = 2.**

`ASSET_NONE` is a data-carrier class, not a transferable asset. Slot 3 (`ASSET_CVG`) is retired. Comment in the enum states that a third asset class previously polluted the dual-pool fee equation.

### 1.2 Hidden-asset scan

Searched consensus / script / primitives for a third circulating class.

| Candidate | Result |
|-----------|--------|
| Token / token id | Not a consensus asset. Pool `MintAuthorizationToken` is off-chain mint auth, not a UTXO. |
| Inscription / Ordinals / Runes | No consensus matcher. |
| Colored coin / overlay color DB | No. Class is inferred from `scriptPubKey`. |
| Wrapped asset / side ledger | No consensus wrapper. ObservationalMetrics is an observer cache, not a ledger of a third asset. |
| PCORB (`PCOR` OP_RETURN) | Proof carrier. `ASSET_NONE`. Not an asset. |
| Coinbase `RIGR` commitment | OP_RETURN target-nBits tag. `ASSET_NONE`. |
| Retired `OP_CVG_CLAIM` / `ASSET_CVG` | Opcode burned (`0xbc`). Classifier has no branch. |
| Retired Witness v1 `CVG2` prefix | `IsAllowedScript` **rejects** a 32-byte v1 program prefixed `CVG2`. |

**Conclusion:** no hidden third native asset.

### 1.3 ORU

```
ORU
 |
 |-- Coinbase subsidy
 |-- Deflationary Fee Burn (RES-transfer fees only)
 |-- Convergence destruction (OP_CONVERGE; not inserted into UTXO)
 |-- UTXO circulation (Witness v0)
```

| Surface | Location | Fact |
|---------|----------|------|
| Definition | `ASSET_ORU` in `primitives/transaction.h`; classified in `AssetTypeFromScriptPubKey` | Witness v0, or `OP_CONVERGE` |
| Creation | `GetBlockSubsidy` in `validation.cpp`; lock `ORU_INITIAL_BLOCK_SUBSIDY` | Public-network path: 1000 → 500 → 250 → **125 ORU forever** (3 halvings, then floor). No absolute ORU cap. |
| Destruction | (1) Convergence: `AddCoins` skips `IsConvergenceScript` outputs (`coins.cpp`). (2) Deflationary Fee Burn: coinbase may not claim `floor(fee/2)` of RES-transfer ORU fees. That share never becomes a UTXO. | Two distinct sinks. Do not collapse them. |
| UTXO | Witness v0 `CTxOut`. Convergence outputs are **not** UTXOs. | Spendable ORU = unspent Witness v0 coins. |
| Wallet | `getbalances.oru` / deprecated `orum`; Qt send asset `ASSET_ORU`; descriptors `wpkh()` / `wsh()` | Wallet balance is **this wallet’s** UTXO, not chain circulating supply. |
| RPC | `getruntimesupply` (`oru_supply_cumulative` = RFC-002 ledger issued − converge); `listunspent.asset = "ORU"`; `getblockchaininfo.cumulative_convergence` | `gettxoutsetinfo.total_amount` mixes ORU 1e8 and RES 1e6. **Not** circulating ORU. |

Subsidy lock: `core/src/consensus/rigor_consensus_lock.h` (`ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN`, `ORU_SUBSIDY_MAX_SHIFTS = 3`, floor `125 * COIN`).

Scale: `1 ORU = COIN = 100_000_000` base units (8 decimals).

### 1.4 RES

```
RES
 |
 |-- Mint schedule W(k)
 |-- Deterministic Reserve Issuance supply curve
 |-- 6 decimal precision
 |-- Max supply 21,000,000 RES
```

| Surface | Location | Fact |
|---------|----------|------|
| Definition | `ASSET_RES`; `IsResScript` / `IsResMintScript` | Witness v1 (RES2 prefix or native 32-byte Ed25519), or coinbase `OP_RES_MINT` |
| Creation | Coinbase `OP_RES_MINT` only, after parent-draw win. Amount `GetResMintReward(h) = W(h / 210000)`. Body txs cannot inflate RES. | `ConnectEvaluateCoinbaseResMint`. PoW does **not** create RES. |
| Destruction | No RES burn opcode. Conservation is `res_in ≥ res_out` on ordinary txs; mint is the sole create exception. | RES is not fee-burned. |
| UTXO | Witness v1 RES outputs and mature `OP_RES_MINT` outputs | Mint maturity equals coinbase maturity (`RES_MINT_MATURITY == COINBASE_MATURITY`). |
| Wallet | `getbalances.res`; descriptor `res()`; Qt RES send | Immature mint is not spendable. |
| RPC | `listunspent.asset = "RES"`; GBT `default_res_mint`; runtime supply mint counters | Some older sendmany parse paths still use **2** display decimals. Consensus scale is **6**. Treat 6 as normative. |

Deterministic Reserve Issuance locks (`rigor_consensus_lock.h` / Python spec / Coq `ResSchedule.v`; internal spec RFC-017):

| Parameter | Value |
|-----------|-------|
| `RES_COIN` | `1_000_000` (6 decimals) |
| `W(0)` | `40.000000 RES` |
| Epoch | `210_000` blocks |
| Last epoch | `k = 250`; `W(k)=0` for `k>250` |
| Decay | `W(k) = floor(W0 · (k+1)^(-3/2))` at 2⁻¹⁶ fixed point |
| `ResScheduleSum` | `99_450_437` atoms (compile-time pin) |
| `RES_MAX_SUPPLY` | `21_000_000 * RES_COIN` = `21_000_000_000_000` atoms |
| Mint cost `C` | `1024 ORU` (`E = 246`) |
| Draw window | `W = 32` |

`static_assert(ResScheduleSum() <= RES_MAX_SUPPLY / RES_REWARD_EPOCH)`: even if every block minted, the schedule cannot exhaust the cap.

### 1.5 Storage invariant

```
CTxOut = (nValue, scriptPubKey)
Asset class = AssetTypeFromScriptPubKey(scriptPubKey)
```

No persisted asset tag. Dual-pool conservation is computed by classifying each input/output script, then summing ORU and RES separately (`CheckTxInputs`, `GetValueOut(AssetType)`).

---

## 2. Script-as-Asset Analysis

### 2.1 Stated principle (this task)

> Asset ownership and behavior are determined by spending conditions, not by global registry/state.

### 2.2 Official project wording

Active architecture language is stricter than the old slogan:

> **Script is the compliance layer.** Asset *class* is inferred from `scriptPubKey`. Asset *legitimacy* requires UTXO commitments, script rules, economic proofs, and deterministic verification.

(`docs/architecture/ARCHITECTURE.md`, `PROTOCOL_TERMINOLOGY_ALIGNMENT.md`.)

The old phrase “the script is the asset” is retired because parsing a script does **not** by itself authorize a RES mint.

### 2.3 Forbidden-item check

| Surface | Present as consensus truth? |
|---------|-----------------------------|
| Global balance table | **No.** Wallet `getbalances` folds this wallet’s UTXOs. |
| Token registry | **No.** |
| Asset database | **No.** |
| Contract storage | **No.** No VM, no persistent contract map. |
| Account nonce | **No.** |

`core/src` has no `balance table`, `asset database`, or `token registry`. “Registry” hits are golden-vector / Qt settings / proof-harness catalogs, not an on-chain asset registry.

### 2.4 What script does determine

| Decision | Mechanism | Extra-script consensus? |
|----------|-----------|-------------------------|
| Asset class | `AssetTypeFromScriptPubKey` | No |
| ORU transfer | Witness v0 spend (Ed25519) | Conservation in `CheckTxInputs` |
| RES transfer | Witness v1 key-path (Ed25519) | Dual-pool + ORU fee leg |
| Convergence burn | `OP_CONVERGE` is unspendable; skipped by `AddCoins` | Draw weight = that output’s `nValue` |
| RES issuance | `OP_RES_MINT` lock (maturity + HASH160) | **Yes:** parent-draw winner, unique mint, `W(h)` |

Mint *ownership after issuance* is a spending condition. Mint *authorization* is ConnectBlock (`ConnectEvaluateCoinbaseResMint`), not a global asset table.

### 2.5 Verdict

```
Script-as-Asset compliance: PASS
```

Reason: ownership and transfer are UTXO spending conditions. There is no global asset registry or account state. Classification is script-derived.

Precision (do not drop):

- Official name is **script-as-compliance**, not “script is the complete asset.”
- RES mint *creation* is a block-level predicate (draw + schedule), still not a registry.
- Observational supply scalars exist and are fenced (see §3).

---

## 3. Global State Analysis

### 3.1 Required consensus state

These are the Bitcoin-shaped objects a node must keep to validate the next block. They are not an asset registry.

| State | Role |
|-------|------|
| UTXO set (`CCoinsView`) | Spendability and value. Convergence outputs never enter it. |
| Chain tip / block index | Header chain, nBits, ancestry. |
| Difficulty / ASERT time | Next-target computation from headers. |
| Per-block draw fields | `m_draw_candidate_root`, `m_block_converge_sum`, `m_block_res_mint_sum` — commitments and index caches derived from the block itself. |
| A-9-C paid window | Already-paid winner txids over the last 32 blocks, reconstructed from coinbase `OP_RES_MINT`. Chain history, not a mutable account map. |

### 3.2 Observational / non-authority state

| ID | Structure | Consensus spend authority? |
|----|-----------|----------------------------|
| W1 | `m_chain_res_mint_total` / `m_chain_oru_total` | **No.** Isolated on `ObservationalMetricsHandle`. Not an input to `T`, `f_i`, mint shape, or `EvalScript`. |
| W2 | Cumulative Convergence | Equality check `bad-convergence-cumulative` against the committed prefix-sum. **Not** spendable ORU and **not** a draw weight. |
| W7 | Runtime supply witness map | Keyed by **block hash**. Written after accept/reject. |
| W8 | Deflationary Fee Burn | **No chain cumulative.** Burn = unclaimed coinbase cap. |
| Wallet | Key pool, `getbalances` | Projection of wallet UTXOs. |
| RPC / explorer | Charts, `getruntimesupply` | Observer. |
| Indexes | txindex, filters | Optional; not validation authority. |

### 3.3 Forbidden global state (not found)

- balances table as consensus truth
- asset registry
- contract storage
- token state machine
- account nonce

Guardrail: `core/test/lint/lint-no-global-state.py` (this audit: **exit 0**).

### 3.4 Conclusion

```
Global state dependency:

Required:
- UTXO set
- chain tip / block index
- difficulty (ASERT / nBits)
- block-local draw commitments
- A-9-C paid-window reconstruction from recent coinbases

Forbidden (not present as consensus truth):
- global balance table
- token / asset registry
- contract storage
- account nonce

Conclusion: COMPLIANT.
No global asset-state dependency.
Observational ledgers exist and are fenced; they must not be cited as spendable balances.
```

---

## 4. Script Inventory

### 4.1 Live allowed families

Classification SSOT: `ClassifyRigorTxout` (`script/rigor_classify.cpp`) plus `IsAllowedScript` / `CheckTxOutAssetWitnessBinding` (`consensus/tx_check.cpp`). Interpreter: `script/interpreter.cpp`.

| Script | Purpose | Asset | State dependency |
|--------|---------|-------|------------------|
| Witness v0 P2WPKH (20-byte) | ORU transfer | ORU | None (UTXO + Ed25519) |
| Witness v0 P2WSH (32-byte) | ORU script-hash spend; AutoResMint bootstrap | ORU | None. Largest inherited opcode surface. |
| Witness v0 native 32-byte Ed25519 | Same 32-byte v0 classifier; interpreter fallback when program equals pubkey | ORU | None |
| Witness v1 RES2 | RES transfer (`RES2` ‖ 28-byte tail) | RES | None |
| Witness v1 native RES | RES transfer; program **is** the Ed25519 pubkey | RES | None |
| `OP_RES_MINT` | Coinbase issuance lock: `<maturity> <HASH160> OP_RES_MINT` | RES | Maturity height in the script; **authorization** is ConnectBlock (parent draw + `W(h)`) |
| `OP_CONVERGE` | Convergence burn: `<20-byte owner HASH160> OP_CONVERGE` | ORU | None. Unspendable (`SCRIPT_ERR_CONVERGE_UNSPENDABLE`). Not inserted into UTXO. |
| `OP_RETURN` (`RIGR`, `PCOR`, other) | Commitment / proof carrier | NONE | None. `nValue` must be 0. |

Witness v1 is **RIGOR-defined**. It is **not** Bitcoin Taproot and does **not** implement BIP341 (no control block, annex, or tapscript).

Authentication on spend paths: **Ed25519 only**. secp256k1 CHECKSIG → `SCRIPT_ERR_SECP256K1_DISABLED`.

### 4.2 Rejected / retired

| Surface | Status |
|---------|--------|
| P2PKH / P2SH / bare multisig as chain outputs | `bad-rigor-script-type` / `bad-txns-asset-script-nonstandard` |
| Witness v1 `CVG2` prefix | Rejected by `IsAllowedScript` |
| `OP_CVG_CLAIM` (`0xbc`) | Burned opcode |
| `tr()` / `rawtr()` / BIP341 | Permanently rejected |
| Spending `OP_CONVERGE` | `bad-txns-spend-convergence` |
| 36-byte v1 “claim program” | Not an `IsAllowedScript` chain output |

### 4.3 Doc drift (do not treat as live)

`RIGOR_SCRIPT_TYPE_REGISTRY.md` still describes Convergence as Witness v1 + `CVG2`. **Current consensus form is `OP_CONVERGE`.** Comments and RPC help text still say “CVG2” for the *economic* Convergence burn. That name is historical vocabulary for `C(h)`, not a live v1 program.

Python model (`core/test/rigor/test_script_assets.py`) already treats `SCRIPT_OP_CONVERGE` as the convergence script.

### 4.4 Script reduction

| Script | Keep? | Reason |
|--------|-------|--------|
| Witness v0 P2WPKH | **KEEP** | Core ORU transfer |
| Witness v1 RES (RES2 or native) | **KEEP** | Core RES transfer |
| `OP_CONVERGE` | **KEEP** | Monetary mechanism: destroy ORU, declare owner, enter draw set F |
| `OP_RES_MINT` | **KEEP** | Sole issuance lock; maturity + owner HASH160 |
| P2WSH | **KEEP (RC0)** | AutoResMint bootstrap; `wsh()` / miniscript. Residual attack surface. Further narrowing needs an RFC. |
| Native v0 32-byte Ed25519 | **REVIEW** | Shares the P2WSH classifier; extra interpreter branch |
| RES2 vs native dual encoding | **REVIEW** | Same asset, two program shapes. Merging would be a consensus change. |
| `OP_RETURN` | **OPTIONAL** | Commitments only |
| Witness v1 CVG2 / `OP_CVG_CLAIM` / P2PKH / P2SH / Taproot | **ALREADY REMOVED** | Not live |

**Can RIGOR reduce script complexity further?** Yes, at the margin (P2WSH opcode set, dual RES encodings). No, for the four core families — they are the current monetary surface. RIGOR already minimizes scripts rather than eliminating them.

### 4.5 Minimal RIGOR script set

```
Required:
1. Witness v0 ORU spend (P2WPKH is sufficient for ordinary transfer)
2. Witness v1 RES spend
3. OP_CONVERGE
4. OP_RES_MINT

Optional:
1. P2WSH (wallet / AutoResMint)
2. OP_RETURN commitments (RIGR, PCORB)
3. Dual RES program encodings (RES2 prefix vs native)
```

Four required scripts, not three: issuance is a distinct lock (`OP_RES_MINT`), and Convergence is no longer a Witness v1 program.

---

## 5. Formal Verification Coverage

### 5.1 Layers

| Layer | Artifact | Role |
|-------|----------|------|
| L1 | `formal_verification/spec/rigor_consensus_spec.py` | Normative executable spec for draw / σ / schedule / selected predicates |
| L2 | `formal_verification/coq/` | Machine-checked **model** |
| L3 | `core/src/**` | Production. Vector-checked against L1. **Not proved equal to L2.** |

There is no theorem of the form “C++ = Python = Coq”.

### 5.2 Live Coq census (2026-08-22)

| Metric | Count | Note |
|--------|------:|------|
| `_CoqProject` modules | **221** | Plus `ExtractHashKernel.v` on disk, not listed |
| Named `Theorem` / `Lemma` / `Corollary` / `Fact` / `Remark` | **2,850** | Live grep. Includes Phase-4F/5 catalogs and bookkeeping |
| `Qed.` closers | **1,169** | Many catalog rows use other one-line discharges |
| Inventory JSON snapshot | **2,585** | `FORMAL_THEOREM_INVENTORY.json` — older than the live grep |
| `Axiom` / `Parameter` / `Hypothesis` / `Conjecture` | **0** | Comment-stripped scan |
| `Admitted.` / `admit` | **0** | Same scan |
| Shared golden vectors | **422** | `golden_lock.json` |
| Coq-enforced golden lemmas | **215** | GoldenHarness 187 + NegativeCore 28 |

**Do not cite 2,850 as “2,850 proved protocol properties.”** Phase catalogs include status records and explicit *impossibility* of L2=L3 identity.

This audit rebuilt a stale dependent cone with `make -f Makefile.coq` (Coq 8.15.0). Load-bearing `.vo` files including `ResSchedule.vo`, `OrderIndependence.vo`, `fee-burn/Properties.vo`, and `OpaqueBoundary.vo` typechecked.

### 5.3 Verified (model)

These are Coq theorems **about the model**, optionally also vector-checked.

| Category | Status |
|----------|--------|
| Monetary schedule `W(k)` | ✓ `ResSchedule.v` (`reward_nonincreasing`, `schedule_sum_value = 99450437`, `schedule_terminates`) |
| RES cap | ✓ `max_issuance_within_cap`; connect-path overshoot reject under `cm_draw_won` |
| Reward monotonicity | ✓ |
| Fee-burn accounting | ✓ `fee-burn/*` (model circulating ΔORU; not ConnectBlock refinement) |
| Supply conservation | ✓ dual-pool + `oru_supply_bounded_by_block_count` (model subsidy) |
| Replay / rollback | ✓ `rollback_restores_previous_state` (model UTXO) |
| Convergence root / order | ✓ `convergence_root_order_independent`, `select_winner_order_independent` |
| Script *shape* | ✓ finite `IsAllowedScript` / spend-CVG2 / PCORB size conjuncts on `apply_tx` |
| SHA-256d kernel | ✓ executable over `list byte`; 215 golden lemmas |

### 5.4 Not claimed

```
NOT CLAIMED:

✗ Full C++ equivalence
✗ Mining implementation equivalence
✗ Wallet implementation equivalence
✗ Network / P2P / mempool security
✗ Interpreter / P2WSH equivalence
✗ IsResScript derives out_asset
✗ cm_draw_won = owner / PCORB / Selection
✗ Coinbase / Merkle / header bind of draw_candidate_root
✗ rs_res_supply = utxo_res
✗ Absolute ORU supply cap (false by design: 125 ORU tail)
✗ Difficulty / OpuHash implementation proof
```

```
equivalence_claimed = false
```

---

## 6. Consensus Boundary Map

```
Consensus Layer
|
|-- PoW (OpuHash + ASERT nBits)
|-- Block validation (ConnectBlock / DisconnectBlock)
|-- UTXO apply (AddCoins skips OP_CONVERGE)
|-- Script verification (Ed25519; closed type set)
|-- Monetary rules (ORU subsidy; RES W(k); 21M RES cap)
|-- Deflationary Fee Burn (claimable-ORU cap)
|-- RES mint (unique coinbase OP_RES_MINT; parent draw)
|-- Dual-pool conservation
|
-----------------

Non-Consensus Layer
|
|-- Explorer / charts / website
|-- RPC statistics (getruntimesupply, observational W1/W7)
|-- Wallet UX, descriptors, Qt labels
|-- Pool / Stratum / GBT assembly (must *mirror* rules; does not define them)
|-- Documentation / whitepaper
|-- Formal model and golden vectors (do not replace L3)
```

**Safe to change without a consensus fork** (examples): explorer copy, RPC display names, chart series, this document, wallet label strings, pool product UX, Coq comments, observational cache keying that is not read on accept/reject.

**Not safe without a consensus change:** `IsAllowedScript`, `AssetTypeFromScriptPubKey`, `GetResMintReward`, `OruResTransferFeeBurnShare`, `AddCoins` skip of Convergence, OpuHash parameters, Ed25519-only CHECKSIG, draw inequality `σ < T · f_i`.

GBT fields (`default_res_mint`, `default_pcorb_carrier`) are miner-facing projections of consensus obligations. Changing the *JSON shape* is not a fork; changing what a valid coinbase must contain is.

---

## 7. RIGOR Architecture Formula

Conservative expression of the **current** tree:

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    4 Core Monetary Script Families
        (Witness v0 ORU, Witness v1 RES, OP_CONVERGE, OP_RES_MINT)
  +
    UTXO Local Validation
  +
    Ed25519 Authentication
  +
    OpuHash + ASERT PoW
  +
    Deterministic Reserve Issuance
  +
    Deflationary Fee Burn
  +
    Formal Verification Model
  +
    No Global Asset State

equivalence_claimed = false
```

Public names above are protocol features. Internal specification IDs remain
RFC-017 (Deterministic Reserve Issuance) and RFC-018 (Deflationary Fee Burn).
Those IDs belong in engineer appendices, not in the public formula.

Not included in the formula, on purpose:

- 2,850 named Coq results as if they were all monetary proofs
- P2WSH as a *core* monetary script (inherited, kept for RC0)
- Explorer / wallet / pool as consensus
- “Script is the asset” as a complete slogan

---

## 8. Public Technical Statement

RIGOR is a two-asset UTXO protocol with a closed script-compliance surface.

It combines:

- ORU and RES native assets
- deterministic reserve issuance
- deflationary transaction economics
- Ed25519 authentication
- OpuHash adaptive Proof-of-Work
- UTXO-local validation

without:

- token registry
- account ledger
- contract storage
- global asset state

ORU is issued by proof-of-work subsidy and destroyed by Convergence and by
Deflationary Fee Burn. RES is a capped reserve, deterministically issued after
protocol-defined Convergence conditions, never mined. Asset class is inferred
from `scriptPubKey`. Script is the compliance/classification layer. Script
alone does not authorize asset creation.

The formal system verifies protocol models and finite vectors.
The production C++ implementation is not claimed to be formally equivalent.

```
equivalence_claimed = false
```

---

## 9. Validation log

Read-only scans and tests run for this profile. No consensus, RFC, wallet, miner, PoW, or signature sources were edited.

### 9.1 Code scans

| Query | Result |
|-------|--------|
| `global state` in `core/src` | Comments / test harness / kernel context. No asset-registry global. |
| `registry` in `core/src` | Golden-vector harness, Qt options. Not an asset registry. |
| `balance table` / `asset database` / `token registry` / `account nonce` in `core/src` | **No hits.** |
| `inscription` / `colored coin` / `wrapped asset` / `side ledger` in `core/src` | **No hits.** |
| `ASSET_CVG` / `OP_CVG_CLAIM` | Retired / burned only. |
| `lint-no-global-state.py` | **exit 0** |

### 9.2 Formal / parity

| Check | Result |
|-------|--------|
| `coqc` 8.15.0 / `make -f Makefile.coq` | **EXIT 0** (stale dependents rebuilt) |
| Comment-stripped Axiom / Admitted | **0 / 0** |
| `test_script_assets.py` | **OK** (2 tests) |
| `test_golden_vectors.py` | **13/13** hardcoded suite |
| `test_phase3_oracle.py` | **422/422** |
| `test_rfc017_schedule_parity.py` | **PASS** (`ResScheduleSum = 99450437`) |
| `test_rfc018_fee_burn_parity.py` | **PASS** (`equivalence_claimed=false`) |
| `test_cpp_selection_golden.py` | **PASS** |
| `scripts/check_golden_lock.py` | **PASS** (422 / 215; honesty flags false) |

A green differential gate is finite agreement. It is not a refinement proof.

---

## 10. Sources (code SSOT)

| Topic | Path |
|-------|------|
| Asset enum / `CTxOut` | `core/src/primitives/transaction.h` |
| Classification | `core/src/primitives/transaction.cpp` (`AssetTypeFromScriptPubKey`) |
| Allowed scripts | `core/src/consensus/tx_check.cpp` |
| Classifier | `core/src/script/rigor_classify.cpp` |
| Interpreter / `OP_CONVERGE` / `OP_RES_MINT` | `core/src/script/interpreter.cpp` |
| UTXO skip of Convergence | `core/src/coins.cpp` (`AddCoins`) |
| Dual-pool / spend-convergence | `core/src/consensus/tx_verify.cpp` |
| Mint gate | `core/src/consensus/rigor_res.cpp` |
| Numeric locks | `core/src/consensus/rigor_consensus_lock.h` |
| Observational fence | `core/src/chain.h` (`m_observational`) |
| Wallet balances | `core/src/wallet/rpc/coins.cpp` (`getbalances`) |
| Python spec | `formal_verification/spec/rigor_consensus_spec.py` |
| Coq project | `formal_verification/coq/_CoqProject` |
