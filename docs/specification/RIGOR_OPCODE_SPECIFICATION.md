# RIGOR Opcode Specification

**Status:** CURRENT PRODUCTION (protocol description)  
**Sources (realization):** `core/src/script/rigor_bytes.h`, `core/src/script/rigor_classify.cpp`,
`core/src/script/predicate_verify.cpp`, `core/src/script/sign.cpp`,
`core/src/consensus/tx_check.cpp`

```
equivalence_claimed = false
documentation_consensus_authority = false
```

Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.
If this file disagrees with the live RFC or this specification layer, the protocol is correct.

Production RIGOR has **no Script VM**. There is no `EvalScript`, no
`ExecuteOpcode`, and no `interpreter.cpp`. Byte values are a frozen
namespace used by the parser and by four live templates. Consensus spends
go through `VerifyRigorPredicate`.

---

## 1. Architecture

```
scriptPubKey bytes
        ↓
ClassifyRigorTxout          (shape)
        ↓
AssetTypeFromScriptPubKey   (ORU / RES / NONE / INVALID)
        ↓
VerifyRigorPredicate        (spend, if the output is a UTXO)
```

Classification is not issuance authority. Script alone cannot mint ORU
or RES.

`CheckTransaction` admits spendable outputs only when
`IsAllowedScript` returns true (`ASSET_ORU` or `ASSET_RES`). Zero-value
`NULL_DATA` (`0x6a`) is admitted on the unspendable path
(`IsUnspendable` + `nValue == 0`). A PCORB carrier script is rejected
(`bad-pcorb-carrier`).

---

## 2. Live consensus markers

These two bytes appear in creatable production templates.

| Name | Byte | Consensus | Wallet | Explorer |
|------|------|-----------|--------|----------|
| `RIGOR_MARKER_RETURN` | `0x6a` | `NULL_DATA`. `nValue` must be 0. Asset NONE. | Optional datacarrier. Not a destination. | Data. Asset NONE. |
| `RIGOR_MARKER_CONVERGE` | `0xbd` | ORU destroy. Template: `0x14 \|\| HASH160(pk32) \|\| 0xbd` (22 bytes). Unspendable. Not inserted into the UTXO set. Same tx MAY mint Witness v1 RES when `R · (2²⁵+h)² ≤ B << 40`. | Wallet creates via Convergence send. `SignStep` refuses spends. | Label **ORU destroy / Convergence**. Never a spendable balance. |

---

## 3. Live templates (not opcode programs)

| Template | Encoding | Asset | Consensus relevance | Wallet | Explorer |
|----------|----------|-------|---------------------|--------|----------|
| ORU P2WPKH | Witness v0, 20-byte program | ORU | Spend: witness `<sig64><pk32>`, Ed25519, domain ORU | Creates `rg1q…`; signs `WITNESS_V0` | ORU address |
| RES v1 | Witness v1, 32-byte Ed25519 program (not `CVG2`/`RES2` prefix) | RES | Spend: witness `<sig64><pk32>` with `pk == program` | Creates Bech32m RES; signs `WITNESS_V1_RES` | RES address |
| RES v1 mint outputs | Witness v1, 32-byte Ed25519 program | RES | Body-tx mint when `CheckResMintArithmetic` holds | Create + spend after maturity 100 | RES |
| Converge | `<20-byte HASH160> 0xbd` | ORU | Destroy `B`; mint gate is the inequality | Create only | Convergence event |
| NULL_DATA | `0x6a` + pushes | NONE | Value 0 | Limited policy | Data carrier |

Anything else is `NONSTANDARD` → `ASSET_INVALID` → `bad-rigor-script-type`.

Not creatable: P2PKH, P2SH, P2WSH, P2PK, bare multisig, Taproot, secp256k1
CHECKSIG, Witness v1 `CVG2` / `RES2`.

---

## 4. Data-push bytes (parser-active)

Used inside templates. Not VM instructions.

| Name | Byte |
|------|------|
| `RIGOR_PUSH_0` | `0x00` |
| Direct push length | `0x01` … `0x4b` |
| `RIGOR_PUSHDATA1` | `0x4c` |
| `RIGOR_PUSHDATA2` | `0x4d` |
| `RIGOR_PUSHDATA4` | `0x4e` |
| `RIGOR_PUSH_1` … `RIGOR_PUSH_16` | `0x51` … `0x60` |

---

## 5. Rejected and unused bytes

| Byte / construction | Production status |
|---------------------|-------------------|
| Script VM / `EvalScript` | Removed |
| P2PKH, P2SH, P2WSH, P2PK, bare multisig | `ASSET_INVALID` |
| Taproot / BIP341 / BIP342 | Absent |
| secp256k1 / ECDSA | Disabled; Ed25519 only |
| `0xbb` | Not a creatable template. Body txs with this marker are rejected |
| `0xbc` (`RIGOR_BURNED_CVG_CLAIM`) | `HasValidOps` rejects |
| Bytes `0xbe`–`0xff` | Invalid (fail closed) |

All values `0x4f`–`0xba` except the live markers above are
`RIGOR_RESERVED_LEGACY_*` or `RIGOR_RESERVED_INERT_*` in
`rigor_bytes.h`. They may appear in display aliases (`GetOpNameAlias`)
for decode tools. Consensus spend paths **never switch on them**.

Do not document `OP_DUP`, `OP_HASH160`, `OP_EQUALVERIFY`, `OP_CHECKSIG`,
or similar as RIGOR monetary opcodes. Those names are Bitcoin-era labels
for unused Reserved-inert bytes and are **never executed**.

---

## 6. Convergence (`0xbd`)

1. Output script is exactly 22 bytes: push-20 + HASH160 + `0xbd`.
2. `nValue ≥ 1` atom (`nValue > 0`). There is no 1 ORU ticket floor. Minting
   `R ≥ 1` is gated only by `R · (2²⁵+h)² ≤ B << 40`.
3. Exactly one such positive output per transaction.
4. Inputs must be ORU (no RES inputs).
5. Output is classified ORU but **does not enter the UTXO set**.
6. The destroy amount `B` is the mint-inequality input.

Reject codes include `invalid-convergence-structure`,
`convergence-amount-too-small`, `convergence-must-be-orum`,
`bad-txns-spend-convergence`.

---

## 7. Mint (RFC-020 body transaction)

A mint is an ordinary body transaction:

1. ORU inputs only.
2. Exactly one `OP_CONVERGE` output with value `B`.
3. One or more Witness v1 RES outputs summing to `R` (or `R = 0` for a plain destroy).
4. `R > 0 → R · (2²⁵+h)² ≤ B << 40` (`CheckResMintArithmetic`). `R = 0` is a plain destroy.
5. Display `destroy_cost(h)` is never 0 for `h ≥ 0`.

---

## 8. Wallet and explorer rules

| Audience | Rule |
|----------|------|
| Wallet | Sign only `WITNESS_V0` and `WITNESS_V1_RES`. Never treat CONVERGE as IsMine spendable. |
| Explorer | Use the same classifier as `AssetTypeFromScriptPubKey`. Do not invent a second asset table. |
| Pool / miner | Do not parse Script to rebuild coinbase. Copy GBT `default_coinbase_tx`. |

---

## 9. Source index

| Topic | Path |
|-------|------|
| Byte enum | `core/src/script/rigor_bytes.h` |
| Classification | `core/src/script/rigor_classify.cpp` |
| Spend predicates | `core/src/script/predicate_verify.cpp` |
| Allowed scripts | `core/src/consensus/tx_check.cpp` |
| Convergence structure | `core/src/consensus/tx_verify.cpp`, `convergenceres_verify.cpp` |
| Coinbase layout | `core/src/consensus/rigor_coinbase_layout.h` |
| Wallet signing | `core/src/script/sign.cpp` |
| No-VM lint | `core/test/lint/lint-no-script-vm.py` |
