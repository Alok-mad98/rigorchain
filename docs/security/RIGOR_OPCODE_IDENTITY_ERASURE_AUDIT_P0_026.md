# RIGOR Opcode Identity Erasure Audit — P0-026-V2

Task: `TASK-RIGOR-NATIVE-BYTE-NAMESPACE-AND-BITCOIN-IDENTITY-ERASURE-P0-026-V2`

This is the Phase 1 scan plus the identity-erasure map. Consensus **byte values** are frozen. Bitcoin Script **names** are removed from the live corpus.

## Unique production model (unchanged)

```
ScriptPubKey bytes
        |
        v
ClassifyRigorTxout
        |
        v
Predicate ID
        |
        v
VerifyRigorPredicate   (switch on RigorPredicateType, not on a byte)
        |
        v
Ed25519 / fail closed
```

There is no General Script VM, no EvalScript, no opcode execution loop, and no stack machine.

## Phase 1 scan (pre-erasure)

Directories: `core/src/script/`, `core/src/wallet/`, `core/src/test/`, `core/src/qt/`, `formal_verification/`, `docs/`.

| Hit | Kind | Handling |
|-----|------|----------|
| Named `enum opcodetype` in `script.h` (`OP_CHECKSIG`, `OP_HASH160`, `OP_DUP`, `OP_NOP`, `OP_RETURN`, `OP_RES_MINT`, `OP_CONVERGE`, …) | enum names | renamed into `rigor_bytes.h` (`RigorByte` / `RigorByteClass`) |
| `GetOpName` strings `"OP_CHECKSIG"` etc. | RPC/parse identity | native names / `RESERVED_0xNN` |
| `VerifyScript` | API identity | `VerifyRigorPredicate` |
| `script/interpreter.*` | directory identity | `script/predicate_verify.*` |
| `LegacyScriptPubKeyMan` RES `IsMine` via `<pk32> OP_CHECKSIG` | actual CHECKSIG ownership | deleted; `HaveCScript(v1 spk) && HaveKey(pk32)` |
| Tests `test_bitcoin` / Boost “Bitcoin Core Test Suite” | test identity | already `test_rigor` / `RIGOR Test Suite` |
| Coq `Definition OP_CHECKSIG` etc. | formal namespace | `RigorByteNamespace.v` + renamed defs in `rigor_v01/` |
| Historical comments / release-notes / `archive_disabled_bitcoin_tests/` | history | left in archives; not live consensus |

No production path executed Bitcoin instruction semantics. `EvalScript` was already gone. Live templates remain: v0/20 ORU, v1/32 RES, RES mint `0xbb`, CONVERGE `0xbd`, RETURN `0x6a`.

## Phase 2 — RIGOR byte namespace

New files:

- `core/src/script/rigor_bytes.h`
- `core/src/script/rigor_bytes.cpp`

```
enum class RigorByteClass {
    DATA_PUSH,
    RES_MINT_MARKER,
    CONVERGE_MARKER,
    RETURN_MARKER,
    RESERVED_INERT,
    INVALID
};
```

Frozen marker values (not renamed numerically):

| Byte | Class | Identifier |
|------|-------|------------|
| `0x6a` | RETURN_MARKER | `RIGOR_MARKER_RETURN` |
| `0xbb` | RES_MINT_MARKER | `RIGOR_MARKER_RES_MINT` |
| `0xbd` | CONVERGE_MARKER | `RIGOR_MARKER_CONVERGE` |
| `0xbc` | INVALID (burned) | `RIGOR_BURNED_CVG_CLAIM` |
| `0x00`–`0x4e`, `0x51`–`0x60` | DATA_PUSH | `RIGOR_PUSH_*` / `RIGOR_PUSHDATA*` |
| other `≤ 0xbd` | RESERVED_INERT | `RIGOR_RESERVED_LEGACY_*` |
| `0xbe`–`0xff` | INVALID | fail closed |

There is no `OP_CHECKSIG` / `OP_HASH160` identifier in this header.

## Phase 3 — Classifier

`ClassifyRigorTxout` in `rigor_classify.cpp` remains the unique ScriptPubKey → predicate-id entry. It matches templates (witness program, mint parse, converge parse, RETURN+push-only). It does not switch on an opcode execution table.

## Phase 4 — Predicate verify

| Old | New |
|-----|-----|
| `script/interpreter.h` | `script/predicate_verify.h` |
| `script/interpreter.cpp` | `script/predicate_verify.cpp` |
| `VerifyScript()` | `VerifyRigorPredicate()` |

`RigorPredicateType` is `{ RES_MINT, WITNESS_V0_ORU, WITNESS_V1_RES, UNKNOWN }`. Unknown fails closed (`SCRIPT_ERR_RIGOR_SCRIPT_TYPE`). Envelope checks (witness flag, empty scriptSig, v1 length) are unchanged.

## Phase 5 — Wallet

RES `IsMine` no longer stores or requires a `<32-byte program> CHECKSIG` subscript.

Ownership is:

1. live v1 scriptPubKey is in the keystore (`HaveCScript` of `0x51` + 32-byte program), and
2. `HaveKey` on that 32-byte program.

Not `CanSpend(script)` and not Bitcoin P2PK CHECKSIG.

## Phase 6 — Tests

- Binary: `test_rigor` (leftover `test_bitcoin` path only as a rebuild warning in `run_pre_mainnet.sh`).
- Boost module: `RIGOR Test Suite`.
- Utility: `rigor-util` (`bitcoin-util` is a reject target).
- Python `test_framework/script.py` uses `RIGOR_*` byte names.

## Phase 7 — Formal verification

`formal_verification/rigor_v01/RigorByteNamespace.v` (compiled):

- byte-class completeness: marker ∨ reserved ∨ invalid
- no execution loop / stack transition / opcode semantics
- `predicate_of_spk = None` ⇒ not spendable

## Frozen (must not change)

- marker bytes `0xbb` / `0xbd` / `0x6a`
- scriptPubKey serialization / witness v0 and v1 encodings
- sighash, Ed25519, maturity, ConnectBlock rules
