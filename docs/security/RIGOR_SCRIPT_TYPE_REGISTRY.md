# RIGOR Script Type Registry

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-FINAL-HARDENING-001`  
**Status:** Official registry of consensus-visible script families  
**Consensus impact:** **NONE** (documentation)  
**`equivalence_claimed`:** `false`

Companion: [RIGOR_SCRIPT_SECURITY_BOUNDARY.md](./RIGOR_SCRIPT_SECURITY_BOUNDARY.md)  
Architecture chapter: [`../whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`](../whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md)

This registry describes **what the protocol accepts and rejects**. It does not add script types.

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

Witness v1 is **RIGOR-defined**. It is **not** Bitcoin Taproot and does **not** implement BIP341.

Wire tags (consensus-locked ASCII, 4 bytes):

| Tag | Bytes | Role |
|-----|-------|------|
| `CVG2` | `C V G 2` | Retired witness-v1 Convergence prefix (not creatable) |
| `RES2` | `R E S 2` | Retired witness-v1 RES prefix (not creatable) |
| `RIGR` | `R I G R` | Coinbase `OP_RETURN` commitment tag |

Historical name **ORB2** is obsolete public vocabulary. Current RES2 wire spelling is `RES2`.

---

## Witness v0

**Purpose:** ORU spend layer (Bitcoin-compatible witness container).

**Supported:**

| Form | Program | Spend |
|------|---------|--------|
| P2WPKH | 20 bytes (`Hash160` of the 32-byte Ed25519 pubkey) | Two-item witness `<64-byte sig><32-byte pk>`. The 25-byte `DUP HASH160 EQUALVERIFY CHECKSIG` template is sighash `scriptCode` only; `ClassifyRigorTxout` treats chain P2PKH as `NONSTANDARD` / `ASSET_INVALID`. |

**Rejected:** P2WSH (v0, 32-byte program). `ClassifyRigorTxout` / `IsAllowedScript` / `VerifyWitnessProgram` treat v0/32 as nonstandard / wrong-length. `wsh()` and `multi()` descriptors are refused. P2SH / `sh()` / `pk()` / `pkh()` / `combo()` are likewise refused; addresses encode only v0/20 Bech32 (`wpkh`) and native v1/32 Bech32m (`res`).

**Validation:** Ed25519 signature verification. secp256k1 / ECDSA CHECKSIG → `SCRIPT_ERR_SECP256K1_DISABLED`. Consensus signatures are a raw **64-byte** Ed25519 detached signature with **no hashtype byte**.

**Asset class:** `ASSET_ORU`.

---

## Witness v1

**Purpose:** RIGOR native protocol commitments (not a Bitcoin Taproot output).

**Supported:**

| Form | Program | Role | Asset |
|------|---------|------|-------|
| Native RES key-path | 32 bytes, neither `CVG2` nor `RES2` | Native RES asset output | `ASSET_RES`; program **is the 32-byte Ed25519 pubkey**; spend is `<64-byte sig><32-byte pk>` with `pk == program` |
| CVG2 prefix | 32 bytes, prefix `CVG2` | Retired | **Not creatable.** Live Convergence is `OP_CONVERGE`. Classified `ASSET_INVALID`. |
| RES2 prefix | 32 bytes, prefix `RES2` | Retired | **Not creatable.** Spend branch deleted. Classified `ASSET_INVALID`. |

**Clarify:** Witness v1 uses a BIP141-shaped version byte. **Not BIP341 Taproot.** No control blocks, annex, tapscript, or merkle script path.

Gates: `IsAllowedScript` (v1 program size 32) + `ClassifyRigorTxout` (`WITNESS_V1_RES`).

---

## OP_RETURN

**Purpose:** Data commitment only.

**Not** asset storage. `nValue` must be 0. Classified `ASSET_NONE`. Example: coinbase `RIGR` + draw-target nBits.

---

## Rejected (consensus outputs / spends)

| Surface | Reject |
|---------|--------|
| P2PKH | `bad-rigor-script-type` / `bad-txns-asset-script-nonstandard` |
| P2SH | same |
| Bare multisig / unknown witness versions | same / `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` |
| Bitcoin Taproot control blocks | Path removed; v1 is Ed25519 key-path only |
| secp256k1 CHECKSIG | `SCRIPT_ERR_SECP256K1_DISABLED` |
| Arbitrary script execution / contract VM | Not implemented |
| Spending a CVG2 output | `bad-txns-spend-convergence` |

A 36-byte v1 “claim program” size exists in headers. It is **not** an `IsAllowedScript` chain output.

**Descriptors:** ORU uses `wpkh()`. RES uses `res()`. There is **no `cvg2()` descriptor** — Convergence outputs are built by `createconvergence` (`ConvergenceOutputScript()`), are unspendable, and must not look like a receive script. `tr()` / `rawtr()` / `wsh()` / `sh()` / `pk()` / `pkh()` / `combo()` remain rejected.

---

## Classification summary

```
scriptPubKey
    → ClassifyRigorTxout (single classifier)
    → asset class (ORU / RES / NONE / INVALID)
    → consensus legitimacy (conservation, mint, Convergence)
```

`ASSET_NONE` is only push-only `OP_RETURN`. Unrecognized scripts are `ASSET_INVALID` and are consensus-rejected. Code SSOT: `core/src/script/rigor_classify.cpp` (`ClassifyRigorTxout` / `AssetTypeFromScriptPubKey`), `core/src/consensus/tx_check.cpp` (`IsAllowedScript`), `core/src/script/interpreter.cpp` (`VerifyScript`).
