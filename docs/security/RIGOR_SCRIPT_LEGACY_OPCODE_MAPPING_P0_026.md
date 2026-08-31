# RIGOR Script Legacy Opcode Mapping (P0-026)

**Task:** P0-026  
**Rule:** do not delete byte space. Do not change live marker values. Do not attach execution to reserved names.  
**Source of truth for numbers:** `core/src/script/script.h` `enum opcodetype` (115 distinct assignments + 4 aliases).

Related: [namespace audit](./RIGOR_NATIVE_OPCODE_NAMESPACE_AUDIT_P0_026.md).

---

## 0. Byte-class legend

| Class | Meaning | May become a VM instruction later? |
|-------|---------|--------------------------------------|
| **PushMarker** | `GetOp` / small-int / witness version nibble | No. Parser only. |
| **LiveMarker** | Terminates or identifies a v0.1 template | No. Marker only. |
| **ReservedInert** | Former Bitcoin instruction. Compiles as a name. Never dispatched. | **No. Forever inert.** |
| **Burned** | Retired RIGOR number | No. Do not reassign. |
| **Vacant** | Unnamed 0xbe–0xfe. Classifier: INVALID | Only as a **future template marker**, never as an instruction |
| **InvalidSentinel** | 0xff | No |

There is no class Instruction, Stack, Computation, Branch, or Loop.

---

## 1. Required rename examples

| Old Bitcoin name | Hex | New RIGOR name | Class | Execute? |
|------------------|-----|----------------|-------|----------|
| `OP_CHECKSIG` | `0xac` | `RIGOR_RESERVED_LEGACY_CHECKSIG` | ReservedInert | **never** |
| `OP_CHECKSIGVERIFY` | `0xad` | `RIGOR_RESERVED_LEGACY_CHECKSIGVERIFY` | ReservedInert | never |
| `OP_CHECKMULTISIG` | `0xae` | `RIGOR_RESERVED_LEGACY_CHECKMULTISIG` | ReservedInert | never |
| `OP_CHECKMULTISIGVERIFY` | `0xaf` | `RIGOR_RESERVED_LEGACY_CHECKMULTISIGVERIFY` | ReservedInert | never |
| `OP_HASH160` | `0xa9` | `RIGOR_RESERVED_LEGACY_HASH160` | ReservedInert | never |
| `OP_EQUALVERIFY` | `0x88` | `RIGOR_RESERVED_LEGACY_EQUALVERIFY` | ReservedInert | never |
| `OP_IF` | `0x63` | `RIGOR_RESERVED_LEGACY_IF` | ReservedInert | never |
| `OP_ELSE` | `0x67` | `RIGOR_RESERVED_LEGACY_ELSE` | ReservedInert | never |
| `OP_ENDIF` | `0x68` | `RIGOR_RESERVED_LEGACY_ENDIF` | ReservedInert | never |
| `OP_CHECKLOCKTIMEVERIFY` / `OP_NOP2` | `0xb1` | `RIGOR_RESERVED_LEGACY_CLTV` | ReservedInert | never |
| `OP_CHECKSEQUENCEVERIFY` / `OP_NOP3` | `0xb2` | `RIGOR_RESERVED_LEGACY_CSV` | ReservedInert | never |
| `OP_RETURN` | `0x6a` | `RIGOR_MARKER_DATACARRIER` | LiveMarker | marker only |
| `OP_RES_MINT` | `0xbb` | `RIGOR_MARKER_RES_MINT` | LiveMarker | marker only |
| `OP_CONVERGE` | `0xbd` | `RIGOR_MARKER_CONVERGE` | LiveMarker | marker only |
| `OP_0` | `0x00` | `RIGOR_PUSH_0` | PushMarker | parser / v0 |
| `OP_1` | `0x51` | `RIGOR_PUSH_1` | PushMarker | parser / v1 |

Do not use `RIGOR_RESERVED_SIG_VERIFY` for `0xac`. That name implies a live check.

---

## 2. Push markers (keep bytes; rename)

| Old | Hex | New | Notes |
|-----|-----|-----|-------|
| `OP_0` / `OP_FALSE` | `0x00` | `RIGOR_PUSH_0` | Witness v0 first byte; mint height 0 |
| (direct push) | `0x01`–`0x4b` | unnamed length | `GetOp` |
| `OP_PUSHDATA1` | `0x4c` | `RIGOR_PUSHDATA1` | |
| `OP_PUSHDATA2` | `0x4d` | `RIGOR_PUSHDATA2` | |
| `OP_PUSHDATA4` | `0x4e` | `RIGOR_PUSHDATA4` | |
| `OP_1` / `OP_TRUE` | `0x51` | `RIGOR_PUSH_1` | Witness v1 first byte |
| `OP_2` … `OP_16` | `0x52`–`0x60` | `RIGOR_PUSH_2` … `RIGOR_PUSH_16` | Mint `CScriptNum`; witness v2–16 **fail closed** as programs |

`OP_1NEGATE` `0x4f` and `OP_RESERVED` `0x50` are **not** push markers in `EvalPushOnlyScript`. Map them as ReservedInert (`RIGOR_RESERVED_LEGACY_1NEGATE`, `RIGOR_RESERVED_LEGACY_50`). They remain an `IsPushOnly` quirk for OP_RETURN remainder (P0-025). Do not “fix” that quirk in a rename PR.

---

## 3. Live markers (keep bytes)

| Old | Hex | New |
|-----|-----|-----|
| `OP_RETURN` | `0x6a` | `RIGOR_MARKER_DATACARRIER` |
| `OP_RES_MINT` | `0xbb` | `RIGOR_MARKER_RES_MINT` |
| `OP_CONVERGE` | `0xbd` | `RIGOR_MARKER_CONVERGE` |

---

## 4. ReservedInert — former Bitcoin instructions (91 named)

Never execute. Never reinterpret as `OP_RIGOR_X`. Suggested identifier: `RIGOR_RESERVED_LEGACY_<REST>` dropping the `OP_` prefix.

### Control (except RETURN)

| Old | Hex |
|-----|-----|
| `OP_NOP` | `0x61` |
| `OP_VER` | `0x62` |
| `OP_IF` | `0x63` |
| `OP_NOTIF` | `0x64` |
| `OP_VERIF` | `0x65` |
| `OP_VERNOTIF` | `0x66` |
| `OP_ELSE` | `0x67` |
| `OP_ENDIF` | `0x68` |
| `OP_VERIFY` | `0x69` |

### Stack

`OP_TOALTSTACK` `0x6b` … `OP_TUCK` `0x7d` including `OP_DUP` `0x76`.

### Splice

`OP_CAT` `0x7e` … `OP_SIZE` `0x82`.

### Bit / equal

`OP_INVERT` `0x83` … `OP_RESERVED2` `0x8a` including `OP_EQUAL` `0x87`, `OP_EQUALVERIFY` `0x88`.

### Numeric

`OP_1ADD` `0x8b` … `OP_WITHIN` `0xa5` including `OP_ADD` `OP_SUB` `OP_MUL` `OP_DIV` `OP_MOD`.

### Crypto opcodes (not C++ hash functions)

`OP_RIPEMD160` `0xa6` … `OP_CHECKMULTISIGVERIFY` `0xaf`.

### Expansion / NOP / tapscript

| Old | Hex | New note |
|-----|-----|----------|
| `OP_NOP1` | `0xb0` | Not a soft-fork hook |
| `OP_CHECKLOCKTIMEVERIFY` | `0xb1` | `nLockTime` is a tx field, not this byte |
| `OP_CHECKSEQUENCEVERIFY` | `0xb2` | `nSequence` is a tx field, not this byte |
| `OP_NOP4`…`OP_NOP10` | `0xb3`–`0xb9` | Not a soft-fork hook |
| `OP_CHECKSIGADD` | `0xba` | BIP342 leftover |

`GetOpName` may print `RESERVED` or hex after rename. Printing `OP_CHECKSIG` is protocol illusion (P3).

---

## 5. Burned / vacant / sentinel

| Byte | Class | Name |
|------|-------|------|
| `0xbc` | Burned | `RIGOR_BURNED_CVG_CLAIM` |
| `0xbe` | Vacant | `RIGOR_VACANT_FUTURE_0` |
| `0xbf` | Vacant | `RIGOR_VACANT_FUTURE_1` |
| `0xc0`…`0xfe` | Vacant | `RIGOR_VACANT_FUTURE_2` … (index = byte − 0xbe) |
| `0xff` | InvalidSentinel | `RIGOR_INVALID` |

Vacant bytes are **not** in `enum opcodetype` today (`GetOpName` → `OP_UNKNOWN`). Classifier: not a witness program, not mint/converge/OP_RETURN → `ASSET_INVALID` → `bad-rigor-script-type`.

Assigning a vacant byte as a **new template terminator** is a future hard fork (new predicate). Assigning it as an executable opcode is forbidden.

---

## 6. Who still references Bitcoin names (for the rename PR)

| Name | Consensus dispatch | Other |
|------|-------------------|--------|
| `OP_CHECKSIG` | no | wallet IsMine; `compressor.cpp`; tests; `GetOpName`; `CountScriptOpsLegacyTestOnly` |
| `OP_HASH160` | no | `IsPayToScriptHash` byte test; compressor; P2PKH fixtures |
| `OP_DUP` / `OP_EQUALVERIFY` | no | compressor P2PKH shape; tests |
| `OP_CHECKMULTISIG` | no | tests; fuzz; `CountScriptOpsLegacyTestOnly` |
| `OP_NOP`…`OP_NOP10` | no | `FormatScript` pretty-print band `OP_NOP`–`OP_NOP10` |
| `IsOpSuccess` | no | RPC `decodescript` wrap heuristic |

`Hash160()` / SHA256 in C++ are **not** the opcodes `OP_HASH160` / `OP_SHA256`.

---

## 7. Coq lock

`formal_verification/rigor_v01/RigorUpgradeNamespace.v`:

- `byte_class OP_CHECKSIG = ByteReservedInert`
- `classify [OP_CHECKSIG] = AssetInvalid`
- `predicate_of_spk canonical_p2pkh = None`
- `is_instruction _ = false` for every class
- `PredFuture n` is not in the v0.1 registry
