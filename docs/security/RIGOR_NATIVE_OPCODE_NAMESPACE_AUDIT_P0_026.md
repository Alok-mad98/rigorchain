# RIGOR Native Opcode Namespace Audit (P0-026)

**Task:** `TASK-RIGOR-SCRIPT-PREDICATE-MIGRATION-AND-NATIVE-UPGRADE-NAMESPACE-DESIGN-AUDIT-P0-026`  
**Mode:** Audit and design only. No consensus rule change. ScriptPubKey bytes, live spend templates, sighash, Ed25519, and maturity are frozen.  
**Predecessor:** [P0-025 opcode / upgrade-surface audit](./RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.md)  
**Date:** 2026-08-27  
**Mainnet:** not launched.

Companions:

- [Legacy opcode mapping](./RIGOR_SCRIPT_LEGACY_OPCODE_MAPPING_P0_026.md)
- [Soft-fork upgrade model](./RIGOR_SOFT_FORK_UPGRADE_MODEL_P0_026.md)
- [Predicate registry design](./RIGOR_PREDICATE_REGISTRY_DESIGN_P0_026.md)
- Coq: `formal_verification/rigor_v01/RigorUpgradeNamespace.v`

There is no `script/opcode.h` in this tree. The Bitcoin-shaped table lives in `core/src/script/script.h` (`enum opcodetype`).

---

## 0. Verdict

| Question | Answer |
|----------|--------|
| Is there a General Script VM? | **No.** `EvalScript` is already gone. |
| Can Bitcoin opcode *semantics* be removed without a fork? | **Yes**, by renaming. Byte values must not change. |
| Should leftover bytes be deleted from the 0x00–0xff space? | **No.** Keep the space. Reclassify names. |
| May reserved bytes become instructions later? | **No.** Reserved means inert forever. |
| How do future upgrades work? | New **predicate** in the classifier, not `OP_NEWTHING`. |
| Soft-fork via NOP / unknown-witness anyone-can-spend? | **Forbidden.** |
| Wallet still constructs Bitcoin Script? | **Yes — P1.** RES `IsMine` stores `<pk> OP_CHECKSIG`. |
| Land `rigor_opcode.h` in this task? | **No.** Design only. Follow-up C++ rename. |

```
NAMESPACE STATUS:
  LIVE_MARKERS:     OP_RETURN 0x6a, OP_RES_MINT 0xbb, OP_CONVERGE 0xbd
                    + push/version bytes OP_0, OP_1..OP_16, PUSHDATA1/2/4
  RESERVED_INERT:   91 former Bitcoin instruction names (no execution)
  BURNED:           0xbc (OP_CVG_CLAIM) — do not reassign
  VACANT:           0xbe..0xfe — future *marker* slots, currently INVALID
  SENTINEL:         0xff

  BYTE_ROLE ∈ { Marker, PushMarker, ReservedInert, Burned, Vacant, Invalid }
  BYTE_ROLE ∉ { Instruction, StackOp, Computation, Branch, Loop }
```

---

## 1. Live consensus execution matrix

`ConnectBlock` → `CheckInputScripts` → `VerifyScript`. The classifier runs in `CheckTransaction` / `CheckTxInputs` before spend.

| Byte / name | Runtime reachable | Consensus impact | Role | Action |
|-------------|-------------------|------------------|------|--------|
| `OP_0` 0x00 | parser + v0 template | YES | PushMarker / witness v0 | Keep byte; rename `RIGOR_PUSH_0` |
| `OP_1` 0x51 | parser + v1 template | YES | PushMarker / witness v1 | Keep; `RIGOR_PUSH_1` |
| `OP_2`…`OP_16` | mint `CScriptNum`; v2–16 parse then **reject** | YES as push; NO as programs | PushMarker | Keep; do not treat as upgrade ops |
| `OP_PUSHDATA1/2/4` | `GetOp` | YES | PushMarker | Keep |
| implicit 0x01–0x4b | `GetOp` | YES | PushMarker | Keep (unnamed length prefix) |
| `OP_RETURN` 0x6a | classifier `NULL_DATA` | YES | LiveMarker | Keep; `RIGOR_MARKER_DATACARRIER` |
| `OP_RES_MINT` 0xbb | `VerifyResMintSpend` | YES | LiveMarker | Keep; `RIGOR_MARKER_RES_MINT` |
| `OP_CONVERGE` 0xbd | create + burn; spend fails | YES (create) | LiveMarker | Keep; `RIGOR_MARKER_CONVERGE` |
| `OP_CHECKSIG` 0xac | **NO** (not dispatched) | NONE | ReservedInert | Rename; never execute |
| `OP_CHECKMULTISIG` | NO | NONE | ReservedInert | Rename |
| `OP_HASH160` opcode | NO | NONE | ReservedInert | Rename. C++ `Hash160()` stays |
| `OP_EQUALVERIFY` opcode | NO | NONE | ReservedInert | Rename. Error code reuse is P3 |
| `OP_IF` / `ELSE` / `ENDIF` | NO | NONE | ReservedInert | Rename |
| `OP_CHECKLOCKTIMEVERIFY` | NO | NONE | ReservedInert | Rename. `nLockTime` field stays |
| `OP_CHECKSEQUENCEVERIFY` | NO | NONE | ReservedInert | Rename. BIP68 field stays |
| `OP_NOP1`…`OP_NOP10` | NO | NONE | ReservedInert | Rename. **Not** a soft-fork valve |
| `0xbc` burned | `HasValidOps` only (not consensus) | NONE on chain | Burned | Do not reassign |
| `0xbe`…`0xfe` | classifier → INVALID | reject | Vacant | Future **markers** only |
| unknown template | fail closed | YES (reject) | — | Keep fail closed |

`EvalScript`, `ScriptMachine`, opcode execution loop: **already absent.** There is nothing left to delete on the consensus path except names, dead constants, and wallet/RPC residue.

---

## 2. Proposed native namespace (`script/rigor_opcode.h` — design, not landed)

Every `uint8_t` has exactly one class. Numeric values of live markers **do not change**.

```cpp
// DESIGN ONLY — do not ship in this audit.
// Bytes are identity. Names are RIGOR. None of these are VM instructions.

enum class RigorByteClass : uint8_t {
    PushMarker,
    LiveMarker,
    ReservedInert,
    Burned,
    Vacant,
    InvalidSentinel,
};

enum RigorOpcode : uint8_t {
    RIGOR_PUSH_0              = 0x00,  // also witness v0 version nibble
    // 0x01..0x4b: direct push length (unnamed)
    RIGOR_PUSHDATA1           = 0x4c,
    RIGOR_PUSHDATA2           = 0x4d,
    RIGOR_PUSHDATA4           = 0x4e,

    RIGOR_RESERVED_LEGACY_1NEGATE = 0x4f,  // IsPushOnly quirk only
    RIGOR_RESERVED_LEGACY_50      = 0x50,

    RIGOR_PUSH_1              = 0x51,  // also witness v1 version nibble
    RIGOR_PUSH_2              = 0x52,
    // ... RIGOR_PUSH_16     = 0x60

    RIGOR_MARKER_DATACARRIER  = 0x6a,  // was OP_RETURN
    RIGOR_MARKER_RES_MINT     = 0xbb,
    RIGOR_MARKER_CONVERGE     = 0xbd,

    RIGOR_BURNED_CVG_CLAIM    = 0xbc,  // never reassign

    // 0x61..0x69, 0x6b..0xba: RIGOR_RESERVED_LEGACY_*  (inert)
    // 0xbe..0xfe: RIGOR_VACANT_FUTURE_*                 (not instructions)
    RIGOR_INVALID             = 0xff,
};
```

### Naming rule (deviation from `RIGOR_RESERVED_SIG_VERIFY`)

Do **not** name `0xac` `RIGOR_RESERVED_SIG_VERIFY`. That reads as a live verifier.

Required name:

```text
OP_CHECKSIG = 0xac  →  RIGOR_RESERVED_LEGACY_CHECKSIG = 0xac
```

Same pattern for `CHECKMULTISIG`, `HASH160`, `IF`, `CLTV`, `CSV`. The string `LEGACY` records Bitcoin origin. The class `ReservedInert` forbids execution and forbids later “turning it on.”

`RIGOR_VACANT_FUTURE_0 = 0xbe`, `FUTURE_1 = 0xbf`, … are **vacant marker slots**. Today they classify as INVALID. A later release may define a **new template that uses one of them as a terminator**, the same way `0xbb` terminates mint. That is a coordinated hard fork, not NOP-style success.

### Compatibility aliases (temporary)

During a rename PR only:

```cpp
using opcodetype = RigorOpcode;
static constexpr auto OP_RES_MINT = RIGOR_MARKER_RES_MINT;
static constexpr auto OP_CHECKSIG = RIGOR_RESERVED_LEGACY_CHECKSIG; // then delete
```

Ship aliases at most one release; then delete Bitcoin names from production headers.

---

## 3. Interpreter boundary — split plan (follow-up)

Current `core/src/script/`:

| File | Today | Target |
|------|-------|--------|
| `interpreter.cpp` | structural `VerifyScript` + sighash + mint/v0/v1 predicates | rename `rigor_verify.cpp` |
| `interpreter.h` | flags, `SigVersion`, parsers | `rigor_verify.h` |
| `rigor_classify.cpp` | unique classifier | keep; optionally `rigor_classifier.cpp` |
| `script.h` enum | Bitcoin table | move names to `rigor_opcode.h` |
| `script.cpp` `GetOp` | push parser | `rigor_marker.cpp` / keep `GetOp` |
| EvalScript / stack VM / opcode loop | **already deleted** | n/a |

**Why EvalScript cannot be deleted in this task:** it is already gone. Remaining `stacktop` / `altstacktop` macros in `interpreter.cpp` are dead; a follow-up may delete the macros without a fork.

Target layout (no VM):

```
script/
  rigor_opcode.h              // byte classes + names (same numbers)
  rigor_marker.cpp            // GetOp, IsPushOnly, IsWitnessProgram
  rigor_classifier.cpp        // ClassifyRigorTxout (already exists)
  rigor_predicate_registry.cpp
  rigor_verify.cpp            // VerifyScript dispatch
  rigor_asset_rules.cpp       // IsAllowedScript wrappers if desired
```

Forbidden in that tree: `EvalScript`, opcode `switch` that computes, `vfExec`, altstack, `OP_SUCCESS`.

---

## 4. Wallet contamination (must fix in a follow-up; not this patch)

Production `LegacyScriptPubKeyMan::IsMine` for `WITNESS_V1_RES`:

```128:132:core/src/wallet/scriptpubkeyman.cpp
        const CScript canonical = CScript() << vSolutions[0] << OP_CHECKSIG;
        CScript subscript;
        const bool have_canonical =
            (keystore.GetCScript(CScriptID(canonical), subscript) && subscript == canonical) ||
```

This does **not** put `<pk> OP_CHECKSIG` on chain (`GetScriptForDestination(WitnessV1Res)` emits `OP_1 <32>`). It **does** require the wallet keystore to store a Bitcoin P2PK-shaped script to recognize RES. Tests (`ismine_tests.cpp`, `wallet_ed25519_key_consistency_tests.cpp`) construct the same object.

Other wallet/script residue:

| Location | What | On-chain? | Grade |
|----------|------|-----------|-------|
| `scriptpubkeyman.cpp` IsMine RES | `<pk> OP_CHECKSIG` lookup | no | **P1** |
| `GetScriptForDestination` | v0/v1 only; P2PKH/P2SH/P2WSH return empty | no | OK |
| `descriptor.cpp` | `tr`/`wsh`/`pk`/`pkh` refused | no | OK |
| `sign.cpp` | no `OP_CHECKSIG` | — | OK |
| `compressor.cpp` | compresses P2PKH/P2PK/P2SH shapes | only if such script exists | P1 (dead special-case) |
| `wallet/rpc/*` help text | P2SH / P2WSH wording | docs | P3 |
| `wallet/test/*` | P2PK fixtures | test | P2 |

**Rule:** the wallet may only *construct* chain scripts in `LIVE_SCRIPT_TEMPLATE_SET` (ORU v0/20, RES v1/32, RES_MINT, CONVERGE, OP_RETURN). Future RES v2 must go through the predicate registry, not a new opcode.

Recommended IsMine for RES: `HaveKey` derived from the 32-byte program. Delete the CHECKSIG subscript.

---

## 5. What this audit does not change

- `OP_RES_MINT` = `0xbb`, `OP_CONVERGE` = `0xbd`
- Witness v0/v1 encodings
- Sighash tags, Ed25519, maturity
- Fail-closed unknown templates

---

## 6. Follow-up sequence (no consensus fork if bytes and VerifyScript stay)

1. Wallet IsMine: drop CHECKSIG subscript (P1).  
2. `GetOpName` / RPC asm: print `RIGOR_MARKER_*` / `RESERVED` / hex (P3).  
3. Introduce `rigor_opcode.h` with aliases; retarget call sites (P2).  
4. Delete Bitcoin names; delete dead `MAX_OPS_PER_SCRIPT`, `IsOpSuccess`, unused `SCRIPT_ERR_*` (P2).  
5. Rename `interpreter.cpp` → `rigor_verify.cpp` (P2).  
6. Do **not** implement NOP or vacant-byte execution in any of those PRs.
