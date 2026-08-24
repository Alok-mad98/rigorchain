# RIGOR Architecture — Final Profile

**Task:** `TASK-RIGOR-ARCHITECTURE-PROFILE-REFINEMENT-AUDIT-P0-002`  
**Date:** 2026-08-22  
**Prior:** [`RIGOR_PROTOCOL_ARCHITECTURE_TECHNICAL_PROFILE.md`](./RIGOR_PROTOCOL_ARCHITECTURE_TECHNICAL_PROFILE.md)  
**Layer:** Read-only architecture calibration  
**Consensus impact:** **NONE**  
**Runtime / wallet / miner / Coq model:** **unchanged**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This file is the calibrated public identity. Counts below were re-checked against the live tree. It does not replace the P0-001 evidence log; it tightens the formula and the claim language.

Forbidden slogan (do not use in active copy):

```
script is the asset
```

Required slogan:

```
script is the compliance/classification layer
```

---

## Official one-sentence identity

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

The formal system verifies protocol models and finite vectors.
The production C++ implementation is not claimed to be formally equivalent.

```
equivalence_claimed = false
```

---

## 1. Architecture formula (calibrated)

The P0-001 draft used “N scripts + M formal properties.” That is accurate as a measurement, but too easy to inflate. The official formula does **not** embed a theorem count.

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    4 Core Monetary Script Families
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
```

| Term | Live meaning | Must not be read as |
|------|----------------|---------------------|
| 2 Native Assets | `ASSET_ORU`, `ASSET_RES` | `ASSET_NONE` is not a third asset |
| 4 Core Monetary Script Families | ORU spend, RES spend, `OP_CONVERGE`, `OP_RES_MINT` | Every witness encoding, P2WSH, or `OP_RETURN` |
| UTXO Local Validation | Spend = consume named outputs | Account balances as truth |
| Ed25519 Authentication | Spend signatures | secp256k1 / BIP341 |
| OpuHash + ASERT | Custom PoW + retarget | A proved mining implementation |
| Deterministic Reserve Issuance | RES schedule, 21M cap, Convergence-triggered mint | A complete economics proof of the C++ node |
| Deflationary Fee Burn | Eligible RES-transfer ORU fee share never becomes a UTXO | A wallet balance, burn UTXO, or global supply state |
| Formal Verification Model | Coq + Python spec + golden vectors | `equivalence_claimed = true` |
| No Global Asset State | No registry / account / contract store | “The node has no state” (UTXO and headers remain) |

**Does this match the code?** Yes, as an identity statement. Script *classifies* the asset; ConnectBlock still decides mint/draw legitimacy. That is the compliance-layer refinement, not a hidden registry.

### 1.1 RIGOR Monetary Architecture

RIGOR separates monetary functions. Do not mix these four ORU/RES quantities:

| Quantity | Role |
|----------|------|
| ORU supply ledger | Issued subsidy minus Convergence `C(h)` (RFC-002 path) |
| Circulating UTXO supply | Spendable Witness v0 ORU remaining in the UTXO set |
| Convergence destruction | ORU committed to an unspendable burn / draw entry |
| Deflationary Fee Burn | Eligible RES-transfer ORU fee share that never becomes a UTXO |

**ORU** is the PoW security asset and the transaction-fee asset. It is destroyed through Convergence and affected by Deflationary Fee Burn. The 125 ORU tail is a locked subsidy floor, not an “unlimited reserve cap.”

**RES** is the reserve asset. It is deterministically issued after protocol-defined conditions: fixed schedule, 21,000,000 cap, Convergence-triggered authorization, deterministic mint amount. RES is not mined.

Script is the compliance/classification layer. Script alone does not authorize RES creation.

---

## 2. Native asset count

```
Native Asset Count = 2
```

### 2.1 Consensus enum

`core/src/primitives/transaction.h`:

| Value | Name | Classification |
|-------|------|----------------|
| 0 | `ASSET_ORU` | **Native asset** |
| 1 | `ASSET_RES` | **Native asset** |
| 2 | `ASSET_NONE` | **Not an asset** (OP_RETURN / data carrier; `nValue` must be 0) |
| 3 | *(unused)* | **Retired** — was `ASSET_CVG` (`OP_CVG_CLAIM`). Do not reassign. |

`AssetTypeFromScriptPubKey` returns only those three live tags. `AssetTypeToString` emits `"ORU"` / `"RES"` / `"NONE"`.

### 2.2 Surface check

| Surface | Live assets |
|---------|-------------|
| Consensus `CheckTxInputs` / dual-pool | ORU and RES only. `ASSET_NONE` cannot be spent. |
| Wallet send / `getbalances` | `oru` and `res` only. |
| Functional `messages.py` | `ASSET_ORU = 0`, `ASSET_RES = 1` (comment: must match the enum). |
| Explorer importer | `asset_kind: "ORU" \| "RES" \| "NONE"`. `NONE` is display/data, not a transferable asset. |
| Python spec | `"ORU"` / `"RES"`. Aliases `ORI` / `ORB` are **historical names**, not extra assets. |

### 2.3 Not assets

| Item | Class |
|------|--------|
| `ASSET_NONE` / `OP_RETURN` / `RIGR` / `PCOR` | **not asset** |
| `ASSET_CVG` / `OP_CVG_CLAIM` | **retired** |
| Witness v1 `CVG2` prefix | **retired** as a legal chain program (`IsAllowedScript` rejects it) |
| `ORI` / `ORB` / `orum` RPC alias | **historical** names for ORU / RES |
| Pool `MintAuthorizationToken` | **not asset** (off-chain auth) |

No token, inscription, colored-coin, wrapped, or side-ledger asset exists in consensus.

---

## 3. Script surface

### 3.1 Core monetary families = 4

Confirmed against `IsAllowedScript`, `ClassifyRigorTxout`, and `CheckTxOutAssetWitnessBinding`.

```
Core = 4

1. ORU spend          Witness v0
2. RES spend          Witness v1
3. OP_CONVERGE        ORU burn / draw entry
4. OP_RES_MINT        RES issuance lock
```

These four are the **required monetary scripts**. They belong under “Minimal Asset-Bearing Script Families.” They do **not** each appear as a separate line in the top-level formula.

### 3.2 Classification

**A. Required monetary scripts**

| Family | Asset | Role |
|--------|-------|------|
| Witness v0 ORU spend | ORU | Transfer |
| Witness v1 RES spend | RES | Transfer |
| `OP_CONVERGE` | ORU | Destroy ORU; declare owner HASH160; enter draw set F. Unspendable. Not a UTXO. |
| `OP_RES_MINT` | RES | Coinbase mint lock (maturity + HASH160). Authorization is ConnectBlock, not a registry. |

**B. Compatibility / encoding variants (not in the core formula)**

| Surface | Why it is not “core” |
|---------|----------------------|
| P2WSH | Inherited Witness v0 encoding. Needed today for `wsh()` / AutoResMint. Largest opcode surface. RC0 keep; not a fifth monetary asset. |
| Native v0 32-byte Ed25519 | Interpreter fallback on the same 32-byte v0 classifier as P2WSH. Variant, not a new family. |
| RES2 prefix vs native 32-byte program | Two encodings of **one** RES spend family. |
| `OP_RETURN` (`RIGR`, `PCOR`) | Data / proof carrier. `ASSET_NONE`. Not asset-bearing. |

**C. Historical / retired**

| Surface | Status |
|---------|--------|
| Witness v1 `CVG2` | Rejected by consensus. Name survives in comments and explorer tests. |
| `OP_CVG_CLAIM` (`0xbc`) | Burned opcode. |
| P2PKH / P2SH / bare multisig as chain outputs | Consensus-invalid. |
| BIP341 / `tr()` / `rawtr()` | Permanently rejected. |

### 3.3 Do these enter the official formula?

| Item | In top-level formula? |
|------|------------------------|
| P2WSH | **No** — compatibility |
| Native Ed25519 v0 | **No** — variant of ORU spend |
| RES2 prefix | **No** — encoding of RES spend |
| `OP_RETURN` | **No** — not an asset script |

Witness v1 is RIGOR-defined. It is not Bitcoin Taproot.

### 3.4 Observer drift (non-consensus)

Explorer classification of live Convergence as `OP_CONVERGE` was aligned in `TASK-RIGOR-EXPLORER-CONVERGENCE-SCRIPT-ALIGNMENT-P1-001`. Retired Witness v1 `CVG2` is display-only historical RFC-014 / retired design, not a third asset.

Root `README.md` still says “Witness v1 (RES / convergence).” That sentence is **stale** relative to `OP_CONVERGE`. This language-refinement task does not rewrite README script-path history.

---

## 4. No global asset state

### 4.1 Forbidden surfaces

| Surface | Present as consensus truth? |
|---------|-----------------------------|
| Account balance table | **No** |
| Token registry | **No** |
| Asset database | **No** |
| Contract storage | **No** |
| Account nonce | **No** |

Wallet `getbalances` and explorer `asset_kind` columns are projections of scripts and UTXOs.

### 4.2 Bitcoin-style consensus state (required, not “global asset state”)

| State | Role |
|-------|------|
| UTXO set | Spendability |
| Block index / chain tip | Header ancestry |
| Difficulty / ASERT | Next nBits |
| Draw reconstruction | F, C, paid window from recent blocks and coinbases |

W1 / W2 / W7 observational caches remain fenced. They are not spend authority.

### 4.3 Verdict

```
No Global Asset State: PASS
```

---

## 5. Formal verification metrics (do not collapse)

Do **not** write “2,850 proved properties.”

| ID | Metric | Live count | What it is | What it is not |
|----|--------|------------|------------|----------------|
| A | Coq modules | **221** (`_CoqProject`) | Machine-checked files | One theorem per file |
| B | `Qed.` closers | **1,169** | Explicit `Qed.` lines | All named results |
| C | Named theorem inventory | **2,850** live grep; **2,585** in `FORMAL_THEOREM_INVENTORY.json` | Named `Theorem`/`Lemma`/… including Phase catalogs | Proved C++ facts |
| D | Golden vectors | **422** shared | Finite L1 test cases | Universal equivalence |
| E | Coq-enforced golden lemmas | **215** (187 + 28) | Vectors recomputed in Coq | Full corpus |
| F | Cross-implementation parity | Python 422/422; Deterministic Reserve Issuance / Deflationary Fee Burn parity; selection golden | Differential agreement | Refinement proof |

Axiom / Admitted (comment-stripped): **0 / 0**.

### Model proven (examples)

- Deterministic Reserve Issuance schedule / cap / monotonicity (`ResSchedule.v`)
- Draw-root / winner order independence (model)
- Dual-pool and fee-burn *model* accounting
- Replay/rollback on the model UTXO
- Finite script-shape conjuncts on `apply_tx`

### Not proven (C++ refinement)

- `core/src/**` = Coq
- Mining / GBT / wallet / P2P
- Interpreter / P2WSH
- `IsResScript` derives `out_asset`
- Absolute ORU cap (false by design: 125 ORU tail)

```
equivalence_claimed = false
```

---

## 6. Simplification analysis

### Can complexity still fall?

| Candidate | Now | Later (would need a separate RFC / product task) |
|-----------|-----|--------------------------------------------------|
| P2WSH opcode surface | Keep for RC0 | Narrow only with an RFC |
| RES2 vs native v1 | Both live | One encoding would be a consensus change |
| Native v0 Ed25519 fallback | Live | Could be documented as P2WSH-adjacent residual |
| RPC `orum` alias / some 2-decimal RES parse paths | Wallet/RPC display debt | Display-only; consensus scale is 6 decimals |
| Explorer `CVG2` classifier | Observer lag | Indexer update; **not** consensus |

### Cannot reduce (protocol core)

1. Two native assets  
2. Four monetary script families  
3. UTXO conservation (no account table)  
4. Ed25519-only spends  
5. OpuHash + ASERT  
6. Deterministic Reserve Issuance (`W(k)` and 21M RES cap)  
7. Deflationary Fee Burn (RES-transfer ORU fee share)  
8. `equivalence_claimed = false` until a refinement proof exists  

### Minimal RIGOR architecture

```
Minimal RIGOR Architecture

Assets:     ORU, RES
Scripts:    ORU spend, RES spend, OP_CONVERGE, OP_RES_MINT
State:      UTXO + headers + difficulty + draw reconstruction
Auth:       Ed25519
Security:   OpuHash + ASERT
Policy:     Deterministic Reserve Issuance; Deflationary Fee Burn
Assurance:  Formal model + finite golden vectors
Absent:     Token registry, account ledger, contract VM, Taproot
Honesty:    equivalence_claimed = false
```

---

## 7. Public technical identity (three versions)

### Version A — Technical (whitepaper)

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    4 Core Monetary Script Families
        (ORU spend, RES spend, OP_CONVERGE, OP_RES_MINT)
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

Script is the compliance/classification layer.
equivalence_claimed = false
```

`CTxOut = (nValue, scriptPubKey)`. Asset class is inferred from the script. Legitimacy still requires UTXO rules, conservation, and ConnectBlock mint/draw predicates.

### Version B — Developer (GitHub README)

RIGOR is a Bitcoin-derived UTXO node (`rigord`) with two native assets: **ORU** (PoW security and fees) and **RES** (capped reserve, Deterministic Reserve Issuance after Convergence — not mined).

- Script is the compliance/classification layer; it is not a token registry.
- Spends are Ed25519. Witness v1 is not Taproot.
- Convergence is `OP_CONVERGE` (ORU burn). RES issuance is coinbase `OP_RES_MINT`.
- Deflationary Fee Burn removes eligible RES-transfer ORU fee components from circulation.
- Formal verification covers a Coq **model** plus 422 golden vectors.
- `equivalence_claimed = false` — C++ is not proved equal to Coq.

### Version C — Community / website

RIGOR is a two-asset, UTXO-based chain.

**ORU** pays for security and fees. **RES** is a scarce reserve created only through Deterministic Reserve Issuance after Convergence, not by mining. Eligible transaction fees are subject to Deflationary Fee Burn.

There is no token contract, no account balance table, and no admin mint key. Ownership is an unspent output plus a signature. The formal system verifies protocol models and finite vectors. The shipping node is not claimed to be formally identical to that model.

Auditable. Minimal. Deterministic. UTXO-based.

---

## 8. Consensus boundary

```
Consensus
  PoW (OpuHash + ASERT)
  Block connect / disconnect
  UTXO (Convergence not inserted)
  Script verification
  Dual-pool conservation
  Deterministic Reserve Issuance
  Deflationary Fee Burn cap

Non-consensus
  Explorer / charts / website
  RPC statistics and aliases
  Wallet labels and some display decimals
  Pool / GBT JSON shape
  This document
```

Changing explorer classification or README wording does not fork the chain. Changing `IsAllowedScript`, `GetResMintReward`, or fee-burn share does.

---

## 9. Calibration vs P0-001

| P0-001 wording | This calibration |
|----------------|------------------|
| “N scripts + M formal properties” in the headline formula | Formula stays qualitative; counts live in §3 and §5 |
| Risk of citing 2,850 as proved properties | Split A–F; 2,850 is a named-result inventory only |
| Core scripts = 4 | **Confirmed.** Compatibility encodings stay out of the formula |
| “Script is the compliance layer” | Tightened to **compliance/classification layer**; “script is the asset” remains forbidden |
| Explorer assumed aligned | **Recorded drift:** explorer still matches retired v1 `CVG2` |

P0-001 facts that stand: native count 2; no global asset state; `equivalence_claimed = false`; mint authorization is ConnectBlock.

---

## 10. Validation (this task)

| Check | Result |
|-------|--------|
| Consensus / RFC / wallet / miner / PoW / signature / Coq model edits | **None** |
| New file only | `docs/security/RIGOR_ARCHITECTURE_FINAL_PROFILE.md` |
| Native asset recount | **2** |
| Core monetary scripts | **4** |
| No Global Asset State | **PASS** |
| FV metrics | 221 / 1169 Qed / 2850 named / 422 vectors / 215 Coq lemmas |
| Honesty flags | all **false** |

```
equivalence_claimed = false
```
