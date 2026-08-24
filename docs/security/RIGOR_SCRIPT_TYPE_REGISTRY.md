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
| `CVG2` | `C V G 2` | Convergence commitment (ORU domain) |
| `RES2` | `R E S 2` | Optional prefix on a native RES program |
| `RIGR` | `R I G R` | Coinbase `OP_RETURN` commitment tag |

Historical name **ORB2** is obsolete public vocabulary. Current RES2 wire spelling is `RES2`.

---

## Witness v0

**Purpose:** ORU spend layer (Bitcoin-compatible witness container).

**Supported:**

| Form | Program | Spend |
|------|---------|--------|
| P2WPKH | 20 bytes (`Hash160` of the 32-byte Ed25519 pubkey) | Two-item witness `<sig><32-byte pk>`; implied `DUP HASH160 EQUALVERIFY CHECKSIG` |
| P2WSH | 32 bytes (`SHA256` of witness script) | Inner script at `SigVersion::WITNESS_V0` |

**Validation:** Ed25519 signature verification. secp256k1 / ECDSA CHECKSIG → `SCRIPT_ERR_SECP256K1_DISABLED`.

**Asset class:** `ASSET_ORU`.

P2WSH is the largest inherited execution surface. **RC0 keeps it:** AutoResMint bootstrap and `wsh()` / miniscript wallets depend on it. Further opcode narrowing would be a separate RFC, not a silent consensus change. See [RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md](./RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md).

---

## Witness v1

**Purpose:** RIGOR native protocol commitments (not a Bitcoin Taproot output).

**Supported:**

| Form | Program | Role | Asset |
|------|---------|------|-------|
| CVG2 | 32 bytes, prefix `CVG2` | Convergence commitment output | ORU domain; unspendable; not inserted into the UTXO set |
| RES key-path (native) | 32 bytes, neither `CVG2` nor `RES2` | Native RES asset output | `ASSET_RES`; program is **SHA256(32-byte Ed25519 pubkey)**; spend is `<sig><pk>` |
| RES2 prefix | 32 bytes, prefix `RES2` | Not creatable | Creation rejected; spend branch deleted |

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

**Descriptors:** ORU uses `wpkh()` / `wsh()`. RES uses `res()`. There is **no `cvg2()` descriptor** — Convergence outputs are built by `createconvergence` (`ConvergenceOutputScript()`), are unspendable, and must not look like a receive script. `tr()` / `rawtr()` remain permanently rejected.

---

## Classification summary

```
scriptPubKey
    → witness version + program
    → asset class (ORU / RES / none)
    → consensus legitimacy (conservation, mint, Convergence)
```

ORU and RES are native UTXO identities at the same protocol layer. They are not colored coins, wrapped assets, inscriptions, Runes, or external contracts.

Code SSOT: `core/src/consensus/tx_check.cpp`, `core/src/script/interpreter.cpp` (`VerifyWitnessProgram`), `core/src/primitives/transaction.cpp` (`AssetTypeFromScriptPubKey`).
