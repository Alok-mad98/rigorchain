# RIGOR Script Opcode / Legacy VM / Future Soft-Fork Upgrade Surface Audit

**Task:** P0-025  
**Priority:** P0 — Consensus / Security / Protocol Architecture  
**Mode:** AUDIT ONLY. No consensus code was modified. No opcode was deleted. `VerifyScript`, `AssetTypeFromScriptPubKey`, `ClassifyRigorTxout`, `CheckBlock`, `ConnectBlock`, ScriptPubKey format, sighash, Ed25519, witness semantics, and maturity were not touched.  
**Date:** 2026-08-27  
**Mainnet status:** not launched. No historical UTXO / replay burden. Compatibility with Bitcoin Script is not a requirement.

Companion visualization: [`RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.canvas.tsx`](./RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.canvas.tsx) (also openable as a Cursor canvas beside chat).

Successor (namespace + predicate registry, no consensus change): [P0-026 native opcode namespace](./RIGOR_NATIVE_OPCODE_NAMESPACE_AUDIT_P0_026.md).

---

## 0. Verdict (read this first)

```
OPCODE AUDIT STATUS:
LIVE_OPCODE_COUNT:            0          (VM-executed opcodes)
LIVE_TEMPLATE_MARKER_COUNT:   5          (v0/20, v1/32, RES_MINT, CONVERGE, OP_RETURN)
LIVE_PUSH_PARSER_BYTE_COUNT:  23 named   (OP_0, OP_1..OP_16, PUSHDATA1/2/4, OP_RETURN, OP_RES_MINT, OP_CONVERGE)
DEAD_BITCOIN_OPCODE_COUNT:    91         (named enum members never matched on the production consensus path)
TOOLING_OPCODE_COUNT:         115        (full GetOpName table, including live markers; 119 with aliases)
FUTURE_RESERVED_SURFACE:      INERT      (NOP / RESERVED / OP_SUCCESS / witness v2–16 exist as names; none is an upgrade hook)

GENERAL_SCRIPT_VM:            NO
BITCOIN_OPCODE_NAMESPACE:     LEGACY ONLY   (not required; keep only while parser/RPC still decode bytes)
FUTURE_UPGRADE_MODEL:         DEDICATED_PREDICATE   (classifier dispatch; hard fork for new templates)
CONSENSUS_RISK:               LOW           (live execution) / MEDIUM residual reconnection hazard
SAFE_TO_BEGIN_OPCODE_CLEANUP: YES           (P2/P3 only; freeze live marker bytes; not this task)
FORMAL_VERIFICATION_ALIGNMENT: PARTIAL      (model has no VM and matches classifier; C++ still ships the Bitcoin enum)
```

**Architecture conclusion**

RIGOR is not a general Script VM. Production consensus is:

```
UTXO
  → ScriptPubKey bytes
  → ClassifyRigorTxout / AssetTypeFromScriptPubKey
  → fixed template
  → Ed25519 (or burn / datacarrier)
```

`EvalScript` is gone from `core/src`. There is no opcode dispatch `switch`, no `vfExec`, no altstack, no arithmetic opcode execution, no `OP_CHECKSIG` / `OP_CHECKMULTISIG` execution, no Taproot script path, no P2WSH redeem.

The Bitcoin opcode namespace is still present as an enum (115 distinct assigned values, 119 with aliases), a `GetOp` byte parser, `GetOpName` / RPC asm, tests, fuzz, and a wallet `IsMine` leftover that constructs `<32-byte pk> OP_CHECKSIG`. None of that is a live consensus execution engine.

**RIGOR should not keep a Bitcoin-style opcode VM, should not keep NOP / unknown-witness as a soft-fork surface, and should not invent `OP_RIGOR_X`.** Future upgrades should add a new ScriptPubKey template and a new dedicated predicate behind the existing classifier. With fail-closed unknown templates, that is a coordinated hard fork. That is the honest mechanism. Bitcoin NOP-soft-fork is not inherited and must not be reintroduced.

Given no mainnet, the leftover namespace is **documentation / engineering hazard**, not a live attack primitive. It can be cleaned in a later P2/P3 task. It must not be cleaned by editing `VerifyScript` or the classifier in that same patch.

---

## 1. What was read (production, not names)

| Path | Role |
|------|------|
| `core/src/script/interpreter.cpp` | `VerifyScript`, `VerifyWitnessProgram`, `VerifyP2WPKHSpend`, `VerifyResMintSpend`, `EvalPushOnlyScript` |
| `core/src/script/interpreter.h` | live `SCRIPT_VERIFY_*` flags, `SigVersion`, no EvalScript |
| `core/src/script/script.h` / `script.cpp` | full `opcodetype` enum, `GetOp` / `GetScriptOp`, `GetOpName`, `HasValidOps`, `IsWitnessProgram`, `IsPushOnly`, `IsOpSuccess`, dead `MAX_OPS_PER_SCRIPT` |
| `core/src/script/rigor_classify.cpp` | unique classifier |
| `core/src/consensus/tx_check.cpp` | `IsAllowedScript`, `CheckTransaction` fail-closed |
| `core/src/consensus/tx_verify.cpp` | `CheckTxInputs`, `GetLegacySigOpCount` (template count, not CHECKSIG walk) |
| `core/src/validation.cpp` | `CheckInputScripts` → `CScriptCheck` → `VerifyScript`; `GetBlockScriptFlags`; AddCoins skip |
| `core/src/coins.cpp` | `IsUnspendable` drops OP_RETURN from UTXO |
| `core/src/script/script_error.h` | error enum, including Bitcoin leftovers never assigned |
| `formal_verification/rigor_v01/*` | classifier + ownership + no-interpreter model |

Repo-wide searches (not only `core/src/script/`): `OP_`, `opcodetype`, `GetOpName`, `CScript`, `GetOp`, `EvalScript`, `VerifyScript`, `SCRIPT_ERR_*`, `SCRIPT_VERIFY_*`, `SigVersion`, `MAX_OPS_PER_SCRIPT`, `MAX_SCRIPT_ELEMENT_SIZE`, `OP_NOP`, `OP_RESERVED`, `OP_CHECKSIG`, `OP_CHECKMULTISIG`, `OP_HASH160`, `OP_SHA256`, `OP_RETURN`, `OP_IF`, `OP_CHECKLOCKTIMEVERIFY`, `OP_CHECKSEQUENCEVERIFY`, `OP_CHECKSIGADD`, numeric literals `0x50`, `0x61`, `0xac`, `0xb0`, plus `vfExec`, opcode `switch`, `MAX_STACK_SIZE`.

`EvalScript` as a function does **not** exist in `core/src`. Remaining hits are comments, formal non-claims, and historical docs.

---

## 2. VerifyScript: what actually runs

Direct from `interpreter.cpp` (production):

```
VerifyScript(scriptSig, scriptPubKey, witness, flags, checker)
    |
    +-- SCRIPT_VERIFY_SIGPUSHONLY && !scriptSig.IsPushOnly() → SIG_PUSHONLY
    |
    +-- IsResMintScript(scriptPubKey)?
    |       yes → VerifyResMintSpend
    |              ParseResMintScript (canonical encoding only)
    |              spend height >= maturity
    |              EvalPushOnlyScript(scriptSig)   // push parser, not a VM
    |              pop <sig, pk>; HASH160(pk)==pkh; Ed25519 (SigVersion::RES_MINT)
    |              CLEANSTACK / unexpected witness
    |
    +-- scriptPubKey.IsWitnessProgram(version, program)?
    |       no  → SCRIPT_ERR_RIGOR_SCRIPT_TYPE
    |       version ∉ {0,1} → SCRIPT_ERR_RIGOR_SCRIPT_TYPE
    |       version==1 && len≠32 → SCRIPT_ERR_RIGOR_SCRIPT_TYPE
    |       flags lack SCRIPT_VERIFY_WITNESS → SCRIPT_ERR_RIGOR_SCRIPT_TYPE
    |              (F-03: never anyone-can-spend)
    |       scriptSig nonempty → WITNESS_MALLEATED
    |       else → VerifyWitnessProgram
    |              v0 / 20 → VerifyP2WPKHSpend
    |                         witness <64-byte sig, 32-byte pk>
    |                         HASH160(pk)==program; Ed25519 (WITNESS_V0)
    |              v0 / other length → WITNESS_PROGRAM_WRONG_LENGTH
    |              v1 / 32 / native RES → key-path
    |                         witness <sig, pk>; pk==program; Ed25519 (WITNESS_V1_RES)
    |              else (v1 CVG2/RES2, wrong shape) → DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM
    |
    +-- CONVERGE / OP_RETURN / P2PKH / P2SH / P2WSH / unknown
            never a successful branch
            CONVERGE is not a witness program → RIGOR_SCRIPT_TYPE
            (CheckTxInputs also rejects spend: bad-txns-spend-convergence)
```

### LIVE_OPCODE_EXECUTION_SET

```
LIVE_OPCODE_EXECUTION_SET = { }
```

Empty. No opcode is dispatched as a Forth operation. `stacktop` / `altstacktop` macros remain in `interpreter.cpp` as **dead residue** after EvalScript deletion; they are not called.

`EvalPushOnlyScript` is a **push-only decoder** for mint `scriptSig`. It accepts `OP_0`, direct pushes (`opcode <= OP_PUSHDATA4`), and `OP_1`..`OP_16`. Any other opcode → `SCRIPT_ERR_BAD_OPCODE`. That is parsing, not a VM.

### LIVE_SCRIPT_TEMPLATE_SET

```
LIVE_SCRIPT_TEMPLATE_SET = {
  WITNESS_V0_KEYHASH,   // OP_0 <20-byte HASH160(pk)>     ORU spend
  WITNESS_V1_RES,       // OP_1 <32-byte pk>              RES spend (Option B)
  RES_MINT,             // <CScriptNum height> <20> OP_RES_MINT
  CONVERGE,             // <20> OP_CONVERGE               ORU burn, not a UTXO
  NULL_DATA             // OP_RETURN + IsPushOnly remainder, nValue==0
}
```

These are **opcode-like byte markers inside fixed templates**, not general executable opcodes. `OP_RES_MINT` / `OP_CONVERGE` / `OP_RETURN` / witness version nibbles must not be described as “VM opcodes that consensus executes.”

`SigVersion` is a sighash domain tag selector (`RGRORU01` / `RGRRES01` / `RGRMNT01`), not a Bitcoin script version, not BIP341, not a tapscript leaf version.

---

## 3. Classifier, CheckTransaction, ConnectBlock

`ClassifyRigorTxout` (`rigor_classify.cpp`) is the unique matcher:

| Match | TxoutType | AssetType | Creatable | Spendable |
|-------|-----------|-----------|-----------|-----------|
| v0 / 20 | `WITNESS_V0_KEYHASH` | ORU | yes | yes (Ed25519 P2WPKH) |
| v1 / 32 native (not CVG2/RES2) | `WITNESS_V1_RES` | RES | yes | yes (pk == program) |
| canonical mint | `RES_MINT` | RES | coinbase only | yes after maturity |
| canonical 22-byte converge | `CONVERGE` | ORU | yes | **no** |
| `OP_RETURN` + push-only rest | `NULL_DATA` | NONE | yes, `nValue==0` | **no** |
| everything else | `NONSTANDARD` | INVALID | **no** | **no** |

`IsAllowedScript` = asset is ORU or RES (includes CONVERGE and mint; excludes OP_RETURN).

`CheckTransaction`:

- `IsUnspendable` (first byte `OP_RETURN` or script > `MAX_SCRIPT_SIZE`) → require `nValue==0`, then `CheckTxOutAssetWitnessBinding` (must still be `NULL_DATA`, not `OP_RETURN` + non-push junk).
- else require `IsAllowedScript` else `bad-rigor-script-type`.
- body cannot create `RES_MINT` (`bad-txns-res-mint-noncoinbase`).

`AddCoin`: drops `IsUnspendable` (OP_RETURN). `ConnectBlock` additionally skips `IsConvergenceScript` so CONVERGE never enters the UTXO set. `CheckTxInputs` still rejects a converge spend if a coin were present (`bad-txns-spend-convergence`).

`GetLegacySigOpCount` does **not** walk `OP_CHECKSIG`. It counts one per created v0 / v1 / mint template. Bitcoin CHECKSIG/CHECKMULTISIG costing is gone from this consensus path.

`GetBlockScriptFlags` always returns the four live flags (witness, nullfail, cleanstack, sigpushonly). There is no delayed soft-fork activation of script flags.

---

## 4. Is there a General Script VM?

| Artifact | Present? | Production consensus? |
|----------|----------|------------------------|
| `EvalScript` | **Deleted** from `core/src` | no |
| opcode dispatch `switch (opcode)` | only `GetOpName` (strings) | no |
| `vfExec` / conditional execution | **absent** | no |
| `stacktop` / `altstacktop` | macros left in `interpreter.cpp`, unused | dead code |
| `MAX_OPS_PER_SCRIPT` (201) | still in `script.h` | **not read** by VerifyScript |
| `MAX_STACK_SIZE` (1000) | still in `script.h` | **not read** |
| `MAX_PUBKEYS_PER_MULTISIG` | still in `script.h`; used by `CountScriptOpsLegacyTestOnly` | test/fuzz only |
| `CScriptNum` arithmetic opcodes | class still exists | used only to encode/decode mint height and coinbase height pushes |
| `IsOpSuccess` (BIP342) | implemented | **RPC wrap heuristic only** |
| `HasValidOps` | implemented (`opcode > MAX_OPCODE` or `0xbc` or oversize push) | **not** on CheckBlock/ConnectBlock |
| `CScript::operator[]` | yes | byte indexing for templates (`IsWitnessProgram`, converge parse), **not** VM stepping |
| disabled opcode execution | no interpreter to disable | n/a |
| CHECKSIG / CHECKMULTISIG execution | no | n/a |

**Judgement:** there is **no** general Script VM on the production consensus path. Function names and leftover constants must not be counted as a VM.

---

## 5. Fail-closed truth table (P0)

| Input | Classification | Spendable | Accepted as output | Reason |
|-------|----------------|-----------|--------------------|--------|
| known ORU v0/20 | ORU / `WITNESS_V0_KEYHASH` | yes | yes | template + Ed25519 |
| known RES v1/32 native | RES / `WITNESS_V1_RES` | yes | yes | pk == program + Ed25519 |
| RES_MINT canonical | RES / `RES_MINT` | conditional (height) | yes, coinbase only | maturity + Ed25519 |
| CONVERGE 22-byte | ORU / `CONVERGE` | **no** | yes | burn; not a UTXO |
| OP_RETURN push-only, value 0 | NONE / `NULL_DATA` | **no** | yes | datacarrier |
| OP_RETURN, value ≠ 0 | — | no | **no** | `bad-txns-opreturn-value` |
| OP_RETURN + non-push remainder | INVALID | no | **no** | `bad-txns-asset-script-nonstandard` |
| P2PKH (`DUP HASH160 EQUALVERIFY CHECKSIG`) | INVALID | no | **no** | `bad-rigor-script-type` |
| P2SH | INVALID | no | **no** | same |
| P2WSH v0/32 | INVALID | no | **no** | same; VerifyScript would be `WRONG_LENGTH` |
| v1 CVG2 / RES2 prefix | INVALID | no | **no** | `IsRigorResNativeWitnessProgram` false |
| unknown template / unknown opcode as scriptPubKey | INVALID | no | **no** | fail closed |
| unknown witness version (v2–16 well-formed program) | INVALID / NONSTANDARD | no | **no** | classifier + CheckTransaction; VerifyScript also `RIGOR_SCRIPT_TYPE` |
| missing `SCRIPT_VERIFY_WITNESS` | — | no | n/a | F-03: `RIGOR_SCRIPT_TYPE`, never anyone-can-spend |
| `0xbc` (burned OP_CVG_CLAIM) | INVALID | no | **no** | not a template; `HasValidOps` also rejects (tooling) |

### Unknown witness version — explicit

Bitcoin treated unknown witness versions as **anyone-can-spend at consensus**, with `DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` as **policy**, so a later version could be a soft fork.

RIGOR does **not** do that:

1. `ClassifyRigorTxout` returns `NONSTANDARD` for any witness program that is not v0/20 or native v1/32.
2. `CheckTransaction` therefore refuses creation (`bad-rigor-script-type`).
3. `VerifyScript` rejects `version ∉ {0,1}` with `SCRIPT_ERR_RIGOR_SCRIPT_TYPE` **before** `VerifyWitnessProgram`.
4. The else branch of `VerifyWitnessProgram` still names `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` (Bitcoin wording). That string is a **protocol illusion**. The path is a consensus failure, and those outputs cannot be created.

**Unknown witness version is fail closed, not a soft-fork hook.**

---

## 6. Opcode consensus surface map

```
Bitcoin opcode namespace (enum / GetOpName / tests / RPC asm)
      X  (not an execution engine)
      |
      X  EvalScript deleted
      ↓
GetOp byte parser ──────────────────────────────────────────────┐
      |                                                         │
      |  used by: IsWitnessProgram, ParseResMintScript,         │
      |           ParseConvergeScript, IsPushOnly,              │
      |           EvalPushOnlyScript (mint scriptSig only)      │
      ↓                                                         │
ScriptPubKey bytes                                              │
      ↓                                                         │
ClassifyRigorTxout  ← unique matcher                            │
      ↓                                                         │
CheckTransaction / IsAllowedScript                              │
      ↓                                                         │
UTXO (OP_RETURN and CONVERGE never stored)                      │
      ↓                                                         │
CheckInputScripts  →  VerifyScript  →  Ed25519                  │
      ↓                                                         │
ConnectBlock                                                    │
                                                                │
Dead Bitcoin opcodes never take this path. ─────────────────────┘
```

**Consensus-reachable nodes:** parser (push + five templates) → classifier → `VerifyScript` structural dispatch → Ed25519 → `ConnectBlock`.

**Not consensus-reachable as execution:** `OP_CHECKSIG`, conditionals, arithmetic, CLTV/CSV as opcodes, NOPs, `OP_SUCCESS`, Taproot, P2WSH inner script.

---

## 7. Table 1 — Live opcode / marker set

These bytes are **compared** on the production consensus path. They are **not** VM-executed.

| Opcode / byte | Hex | Bitcoin origin | Current definition | Production consensus | VerifyScript | Classifier | Wallet | RPC | Test | Fuzz | UI | Formal | Status |
|---------------|-----|----------------|--------------------|----------------------|--------------|------------|--------|-----|------|------|----|--------|--------|
| OP_0 | 0x00 | yes | empty push; witness v0; mint height 0 | **yes** (template / push) | mint push parser; v0 program | v0 match | constructs v0 | asm | yes | yes | — | `OP_0` | **B** marker + push parser |
| OP_1 | 0x51 | yes | small int 1; witness v1 | **yes** | mint push; v1 program | v1 match | constructs v1 | asm | yes | yes | — | `OP_1` | **B** |
| OP_2 … OP_16 | 0x52–0x60 | yes | small ints; witness v2–16 encoding | **yes** for mint `CScriptNum` 2–16; v2–16 **rejected** as programs | push parser only | v2–16 → INVALID | — | asm | yes | yes | — | `OP_16` | **B** / fail-closed versions |
| OP_PUSHDATA1/2/4 | 0x4c–0x4e | yes | push prefixes | **yes** (`GetOp`) | mint `EvalPushOnlyScript` | mint/OP_RETURN parse | — | asm | yes | yes | — | yes | **A** parser, not VM |
| implicit push 0x01–0x4b | — | yes | `GetOp` length-prefixed push | **yes** | yes | yes | yes | yes | yes | yes | — | `OP_PUSH20/32` | **A** parser |
| OP_RETURN | 0x6a | yes | NULL_DATA marker | **yes** (unspendable template) | not a spend path | `NULL_DATA` | decode | asm / commitment | yes | yes | — | `OP_RETURN` | **B** marker |
| OP_RES_MINT | 0xbb | **no** (RIGOR) | mint lock terminator | **yes** | `VerifyResMintSpend` | `RES_MINT` | mint scripts | asm | yes | yes | — | `OP_RES_MINT` | **B** marker |
| OP_CONVERGE | 0xbd | **no** (RIGOR) | burn lock terminator | **yes** (create); spend impossible | fails `RIGOR_SCRIPT_TYPE` | `CONVERGE` | `createconvergence` | asm | yes | yes | — | `OP_CONVERGE` | **B** marker |

`MAX_SCRIPT_ELEMENT_SIZE` (520) **is** live: witness items larger than 520 fail `SCRIPT_ERR_PUSH_SIZE`.

`CScriptNum` **is** live for mint maturity encoding (canonical `GetResMintScript` round-trip) and for some coinbase / commitment height pushes. It is **not** live as Bitcoin numeric opcodes (`OP_ADD` etc.).

Quirk (not a VM): `IsPushOnly` treats every opcode `<= OP_16` (0x00–0x60) as a push, including `OP_1NEGATE` (0x4f) and `OP_RESERVED` (0x50). Therefore `OP_RETURN OP_RESERVED` classifies as `NULL_DATA`. `EvalPushOnlyScript` still rejects those two on the mint spend path. Document; do not “fix” in this audit.

---

## 8. Table 2 — Dead Bitcoin opcode set

Named enum members that production consensus **never matches** except via generic `GetOp` (which does not execute them). Count: **91**.

All of the following are `DEAD_LEGACY` unless noted as reconnection magnets (P1).

### 8.1 Control / conditionals (not OP_RETURN)

`OP_NOP` 0x61, `OP_VER` 0x62, `OP_IF` 0x63, `OP_NOTIF` 0x64, `OP_VERIF` 0x65, `OP_VERNOTIF` 0x66, `OP_ELSE` 0x67, `OP_ENDIF` 0x68, `OP_VERIFY` 0x69.

**WHY STILL PRESENT:** Bitcoin enum + `GetOpName` + `ScriptToAsmStr` + `core_read` name map + tests (`script_tests`, `transaction_tests`) + functional `test_framework/script.py`.  
**WHO REFERENCES:** parser/RPC/tests.  
**CAN IT REENTER CONSENSUS?** Only if someone restores EvalScript.  
**ACCIDENTAL RECONNECT:** medium — `SCRIPT_ERR_UNBALANCED_CONDITIONAL` / `MINIMALIF` strings still exist.

### 8.2 Stack

`OP_TOALTSTACK` … `OP_TUCK` (0x6b–0x7d), including `OP_DUP`.

`OP_DUP` appears in tests as the P2PKH fixture that classifier **rejects**.

### 8.3 Splice / disabled

`OP_CAT` `OP_SUBSTR` `OP_LEFT` `OP_RIGHT` `OP_SIZE` (0x7e–0x82). Bitcoin-disabled opcodes. No interpreter to disable.

### 8.4 Bit logic

`OP_INVERT` … `OP_RESERVED2` (0x83–0x8a), including `OP_EQUAL` / `OP_EQUALVERIFY`.

`SCRIPT_ERR_EQUALVERIFY` **is** assigned on the live P2WPKH/mint **hash mismatch** path. The **opcode** `OP_EQUALVERIFY` is not executed. Name illusion.

### 8.5 Numeric / arithmetic

`OP_1ADD` … `OP_WITHIN` (0x8b–0xa5), including `OP_ADD` `OP_SUB` `OP_MUL` `OP_DIV` `OP_MOD`.

### 8.6 Crypto opcodes (not the C++ hash functions)

`OP_RIPEMD160` `OP_SHA1` `OP_SHA256` `OP_HASH160` `OP_HASH256` `OP_CODESEPARATOR` `OP_CHECKSIG` `OP_CHECKSIGVERIFY` `OP_CHECKMULTISIG` `OP_CHECKMULTISIGVERIFY` (0xa6–0xaf).

**Critical distinction:** consensus **does** call C++ `Hash160` / SHA256 for programs and sighash. That is not the Bitcoin `OP_HASH160` / `OP_SHA256` instruction.

`OP_CHECKSIG` **WHY STILL PRESENT:** enum; `GetOpName`; tests; **wallet** `scriptpubkeyman.cpp` stores a “canonical” `<32-byte program> OP_CHECKSIG` for RES `IsMine`. Descriptors themselves reject `pk()` / `wsh()` / `tr()`.  
**CAN IT REENTER?** Wallet leftover is the most realistic reconnection magnet: a future “make IsMine use Solver” patch could treat CHECKSIG scripts as first-class.  
**Cleanup grade:** P1 (wallet), P2 (enum/tests).

### 8.7 Expansion / NOP / CLTV / CSV / Tapscript

`OP_NOP1` 0xb0, `OP_CHECKLOCKTIMEVERIFY`/`OP_NOP2` 0xb1, `OP_CHECKSEQUENCEVERIFY`/`OP_NOP3` 0xb2, `OP_NOP4`…`OP_NOP10` 0xb3–0xb9, `OP_CHECKSIGADD` 0xba.

Transaction `nLockTime` / BIP68 `nSequence` still exist as **transaction fields** (`IsFinalTx`, `SequenceLocks`). Those are not opcode execution.

Functional tests `feature_cltv.py`, `feature_csv_activation.py`, `feature_nulldummy.py` still exist as Bitcoin-era tests. They do not restore opcode CLTV in `VerifyScript`.

### 8.8 Push leftovers

`OP_1NEGATE` 0x4f, `OP_RESERVED` 0x50 — not matched as mint/witness markers. `IsPushOnly` quirk only (see §7).

### 8.9 Burned number

`OP_CVG_CLAIM_BURNED` = 0xbc. Not in `GetOpName` (prints `OP_UNKNOWN`). `HasValidOps` rejects it. Formal Coq covenant modules treat it as unreachable. **Do not reassign 0xbc.**

`OP_INVALIDOPCODE` 0xff — sentinel, not a template.

---

## 9. Table 3 — Compatibility / tooling opcode set

The **entire** `GetOpName` table (115 distinct assigned values including live markers; 119 with `OP_FALSE` / `OP_TRUE` / `OP_NOP2` / `OP_NOP3`) is used by:

| Surface | Files | Uses opcodes as |
|---------|-------|-----------------|
| RPC asm | `core_write.cpp` `ScriptToAsmStr`, `FormatScript` (`OP_NOP`..`OP_NOP10` pretty-print) | display |
| RPC parse | `core_read.cpp` `mapOpNames` (includes `OP_RESERVED`) | decode hex/asm |
| `decodescript` | `rpc/rawtransaction.cpp` — `HasValidOps`, `IsOpSuccess`, `OP_CHECKSIGADD`, offers Bitcoin `p2sh` wrap | **illusion** |
| Wallet IsMine | `wallet/scriptpubkeyman.cpp` `<pk> OP_CHECKSIG` for RES | **illusion** |
| Wallet RPC | `wallet/rpc/coins.cpp` `IsPayToScriptHash` / `IsPayToWitnessScriptHash` | dead type checks |
| `rigor-tx` | P2SH/P2WSH branches | dead |
| Unit tests | `script_tests`, `multisig_tests`, `sigopcount_tests`, `script_standard_tests`, `rigor_witness_tx_tests` | negative tests + fixtures |
| Fuzz | `fuzz/script.cpp`, `script_ops.cpp`, `eval_script.cpp` (now `EvalPushOnlyScript`) | parser |
| Bench | `bench/verify_script.cpp` | live templates + push parser |
| Functional | `test_framework/script.py` full Bitcoin opcode table (257 `OP_` hits in that tree including archives) | test builders |
| QT | no opcode enum usage of CHECKSIG/VM; asset via `AssetTypeFromScriptPubKey` | none as VM |
| Formal | `AssetClassification.v` uses `OP_DUP`/`OP_HASH160`/`OP_CHECKSIG` only as **invalid P2PKH fixture bytes** | aligned |

`TxoutType::{PUBKEY, PUBKEYHASH, SCRIPTHASH, MULTISIG, WITNESS_V0_SCRIPTHASH, WITNESS_UNKNOWN}` are `LEGACY_ONLY` and **unreachable** from `ClassifyRigorTxout`. Switches still exist so the compiler enumerates them.

Descriptors: `wpkh()` and `res()` only. `pk` / `pkh` / `combo` / `wsh` / `tr` / `rawtr` / `sh` are hard-refused.

---

## 10. Table 4 — Future upgrade surface (and why it is inert)

| Candidate | Bitcoin role | RIGOR today | Soft-fork hook? |
|-----------|--------------|-------------|-----------------|
| `OP_NOP1`…`OP_NOP10` | replace NOP with new opcode | dead enum; scriptPubKey containing NOP → INVALID | **No. Do not use.** |
| `OP_RESERVED` / `RESERVED1/2` | fail if executed | not executed; RESERVED may appear after OP_RETURN via IsPushOnly | **No** |
| `IsOpSuccess` / OP_SUCCESSx | BIP342 anyone-succeeds | RPC only; unknown scriptPubKey is INVALID | **No. Opposite of fail-closed.** |
| unknown witness v2–16 | anyone-can-spend + policy discourage | create rejected; spend rejected | **No** |
| `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_*` | policy wording | some codes assigned as **consensus fail** | wording hazard only |
| `STANDARD_NOT_MANDATORY_VERIFY_FLAGS` | mempool vs consensus flag split | still in `CheckInputScripts` for error class; `GetBlockScriptFlags` already includes all four flags | not an opcode upgrade |
| `TxoutType::WITNESS_UNKNOWN` | future programs | unreachable from classifier | dead enum |
| `0xbe`…`0xfe` | unused | `HasValidOps` rejects `> MAX_OPCODE` (0xbd); not a consensus gate | do not treat as reserved VM space |
| RIGOR_VERSION / program version | — | **does not exist** as a dedicated field | **should be designed**, not inherited from NOP |

**Question A — Does RIGOR need `OP_NOP1`…`OP_NOP10`?**  
No. Soft-fork-by-NOP requires a VM that treats NOP as success. RIGOR has no such VM. Keeping the names invites a future PR that “reactivates Bitcoin upgrade practice.” There is no mainnet to preserve NOP meaning for.

**Question B — New signature, asset, covenant, witness version, ownership primitive?**  
Add a **new ScriptPubKey template** and a **new dedicated predicate** in the classifier + `VerifyScript` dispatch (and Coq constructors). Do not add a general opcode. Do not reuse NOP. Do not open unknown witness versions as anyone-can-spend.

**Question C — `RIGOR_VERSION` / `RIGOR_PROGRAM_VERSION` / `RIGOR_SCRIPT_VERSION`?**  
Prefer an explicit **template id / program version inside the ScriptPubKey** that the classifier already understands as a closed set, over Bitcoin opcode reservation. A version nibble that is fail-closed today is **not** a soft-fork; it is documentation of future hard-fork slots. If a slot is not creatable, old nodes will reject new outputs.

---

## 11. Three future upgrade models

### Model A — Bitcoin-style opcode extension (`OP_RIGOR_X`)

| | |
|--|--|
| Mechanism | New opcode in the Bitcoin namespace; restore or grow an interpreter |
| Soft vs hard | Soft only if old nodes **succeed** on the byte (NOP/SUCCESS/anyone-can-spend). That is exactly the Bitcoin hazard RIGOR removed. |
| Pros | Familiar to Bitcoin reviewers |
| Cons | Reintroduces a VM; formal cost explodes; protocol illusion becomes real; no mainnet benefit |
| Consensus risk | **High** if a VM returns |
| Formal cost | Opcode semantics, disabled lists, resource limits, conditionals |
| **Verdict** | **Reject.** |

### Model B — Versioned ScriptPubKey (`version ‖ program`)

| | |
|--|--|
| Mechanism | Classifier: version 0 = current ownership; version 1 = future |
| Soft vs hard | **Hard fork** under today’s fail-closed rule (old nodes reject unknown version at **create**). Soft fork only if old nodes **accept creation** and either (unsafe) anyone-can-spend or (still a hard fork on **spend**) treat as unspendable while new nodes allow spends. |
| Pros | Closed dispatch; matches current `IsWitnessProgram` shape (v0/v1 already) |
| Cons | Easy to cargo-cult Bitcoin “unknown version = upgrade surface.” F-03 exists because missing witness flags used to be anyone-can-spend. |
| Consensus risk | Low if unknown stays INVALID; High if someone “opens” v2 for upgrades |
| Formal cost | One more constructor per version |
| **Verdict** | Acceptable **as a template family**, not as a silent soft-fork valve. Current v0/v1 is already this shape for ORU/RES. |

### Model C — Fixed ScriptPubKey + dedicated consensus predicate (recommended)

| | |
|--|--|
| Mechanism | `ScriptPubKey → Classifier → Predicate(k) → Ed25519 or future verifier or burn` |
| Soft vs hard | New `k` is a **coordinated hard fork** (old nodes: INVALID). Honest, auditable, matches “no mainnet baggage.” |
| Pros | Smallest consensus surface; Coq already models this (`SpendPath` has three constructors); no NOP; no SUCCESS; no hidden anyone-can-spend |
| Cons | Cannot stealth-activate rules on old nodes |
| Consensus risk | **Low** |
| Formal cost | Add one inductive constructor + fail-closed default |
| Backward compatibility | None required pre-mainnet |
| Upgrade safety | Reviewer must change classifier + VerifyScript + Coq together; cannot “just define an opcode” |

**Recommendation:** Model C. Use Model B only as the **encoding** of a new template (a new first byte or a new tagged program), still fail-closed until a release that knows it. Never Model A.

For covenants: a dedicated template (like CONVERGE already is) plus a dedicated checker, not `OP_CHECKSIGADD` / tapscript.

For a new signature scheme: new `SigVersion` domain tag + new predicate branch, or a hard-fork replacement of Ed25519. Do not put algorithm choice in an opcode.

---

## 12. Protocol illusion (Bitcoin Script still looks alive)

Even with no VM, third parties can believe RIGOR still runs Bitcoin Script because:

1. `enum opcodetype` is a full Bitcoin table including CHECKMULTISIG, CLTV, CHECKSIGADD.  
2. RPC `asm` prints `OP_CHECKSIG`, `OP_CHECKLOCKTIMEVERIFY`, `OP_NOP4`, …  
3. `decodescript` still talks about wrapping in P2SH / P2WSH.  
4. `SCRIPT_ERR_*` strings mention OP_VERIFY, CHECKMULTISIG dummy, OP_SUCCESSx reserved for soft-forks, secp256k1 disabled, CONVERGE unspendable — several of these **are never assigned** in `VerifyScript` (`SCRIPT_ERR_SECP256K1_DISABLED`, `SCRIPT_ERR_CONVERGE_UNSPENDABLE`, `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS`, `SCRIPT_ERR_OP_RETURN`, …). CONVERGE spends fail as `RIGOR_SCRIPT_TYPE`.  
5. Wallet IsMine constructs `OP_CHECKSIG`.  
6. `docs/security/minimal-script-architecture.md` is **stale** (still describes P2WSH as the largest inherited execution surface). Live code rejects P2WSH.  
7. Functional tests `feature_cltv.py` / `feature_csv_activation.py` / `p2p_segwit.py` / archived `feature_taproot.py`.  
8. `libbitcoinconsensus` naming (separate P0-028).  

**This is not currently a consensus attack surface.** It is a **misreading surface** and a **reconnect surface**.

Illusion opcodes of special interest:

| Opcode | Live consensus execution? | Reenter risk |
|--------|---------------------------|--------------|
| OP_CHECKSIG / VERIFY | no | P1 wallet IsMine |
| OP_CHECKMULTISIG / VERIFY | no | tests + sigop leftover names |
| OP_SHA1 / SHA256 / HASH160 / HASH256 | no (C++ hashes exist) | name collision with Hash160() |
| OP_IF / NOTIF / ELSE / ENDIF | no | error strings |
| OP_DUP / EQUAL / EQUALVERIFY | no | P2PKH tests; EQUALVERIFY error code reused |
| OP_ADD / SUB / MUL / DIV / MOD | no | low |
| CLTV / CSV opcodes | no | `nLockTime`/`nSequence` still exist as tx fields |
| OP_CHECKSIGADD | no | RPC `IsOpSuccess` neighbor |
| OP_NOP1–10 | no | **do not** reuse as upgrade |

---

## 13. Formal verification alignment

`formal_verification/rigor_v01/` :

| Question | Answer |
|----------|--------|
| Models opcode VM? | **No.** `NoInterpreterConsensus.v`: `opcode_vm_in_consensus = false`. |
| Models ScriptPubKey? | **Yes**, as `list byte`. |
| Models classifier? | **Yes**, `classify` matches C++ order (witness, mint, converge, OP_RETURN, else INVALID). |
| Models unknown template? | **Yes**, `AssetInvalid`. |
| Models future version? | Unknown witness versions → `AssetInvalid`. No reserved-success version. |
| Models reserved / NOP? | **No** (correct: they are not consensus). |
| Models fixed predicate dispatch? | **Yes**, `SpendPath` = P2WPKH \| RES v1 \| mint; all `AuthEd25519`. CONVERGE is ownership-not-spendable. |

**FORMAL / IMPLEMENTATION SURFACE MISMATCH:** Coq has no opcode table. C++ still ships 115 named opcodes, dead `SCRIPT_ERR_*`, dead `MAX_OPS_PER_SCRIPT`, unused `stacktop` macros, `IsOpSuccess`. The **live** spend/classify path is aligned; the **leftover C++ namespace** is not in the model.

`equivalence_claimed = false` remains correct. This audit does not claim Coq refines `interpreter.cpp`.

Stale formal/docs: `OpaqueBoundary.v` still names EvalScript as a ghost non-claim; `core/doc/convergence-consensus-v1.md` still says `EvalScript` returns `SCRIPT_ERR_CONVERGE_UNSPENDABLE` (false: EvalScript is gone; converge fails as `RIGOR_SCRIPT_TYPE`).

---

## 14. Cleanup grades (do not delete in this task)

### P0 — do not delete / do not change numeric values

- `OP_0`, `OP_1`…`OP_16`, `OP_PUSHDATA1/2/4`, implicit pushes  
- `OP_RETURN`, `OP_RES_MINT` (0xbb), `OP_CONVERGE` (0xbd)  
- Do not reassign `0xbc`  
- `GetOp` / `GetScriptOp`, `CScriptNum` mint encoding, `IsWitnessProgram`, `IsPushOnly`  
- `VerifyScript` dispatch, classifier, `CheckTransaction` fail-closed  
- `MAX_SCRIPT_ELEMENT_SIZE` (live witness cap)

### P1 — reconnection magnets; dedicated follow-up

- Wallet `<pk> OP_CHECKSIG` IsMine canonical script  
- `SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` / NOP / OP_SUCCESS wording vs actual fail-closed  
- `stacktop` / `altstacktop` / `popstack` dead macros in `interpreter.cpp`  
- `IsOpSuccess`  
- `STANDARD_NOT_MANDATORY_VERIFY_FLAGS` Bitcoin-shaped split  
- `TxoutType` LEGACY_ONLY members if switches can be narrowed without consensus change  
- Stale docs claiming P2WSH / EvalScript / secp CHECKSIG  

### P2 — dead code / tests / constants (safe cleanup task)

- `MAX_OPS_PER_SCRIPT`, `MAX_STACK_SIZE`, `MAX_PUBKEYS_PER_MULTISIG`, `MAX_PUBKEYS_PER_MULTI_A`, BIP342 `VALIDATION_WEIGHT_*`  
- `CountScriptOpsLegacyTestOnly`  
- `IsPayToScriptHash` / `IsPayToWitnessScriptHash` if all remaining callers are tests/RPC wrap  
- Unit tests that only exist to exercise deleted EvalScript (`multisig_tests` spend paths already expect failure)  
- Unassigned `SCRIPT_ERR_*` (CHECKMULTISIGVERIFY, DISABLED_OPCODE, SECP256K1_DISABLED, CONVERGE_UNSPENDABLE, DISCOURAGE_UPGRADABLE_NOPS, OP_SUCCESS, taproot script-path errors, …)  
- Named dead opcodes in `GetOpName` (can collapse to `OP_UNKNOWN`)  

### P3 — naming / compatibility

- RPC asm displaying Bitcoin names for unknown bytes  
- `decodescript` P2SH wrap  
- Functional `test_framework/script.py` full opcode dictionary  
- `OP_NOP2` alias = CLTV in `GetOpName`  

**SAFE_TO_BEGIN_OPCODE_CLEANUP: YES** for a **new** task that is P2/P3-only, does not edit `VerifyScript` / classifier / marker values, and treats P1 wallet CHECKSIG as a separate patch. Not this audit.

---

## 15. Live SCRIPT_VERIFY / SCRIPT_ERR (so leftovers are visible)

**Flags actually read by `VerifyScript`:** `SIGPUSHONLY`, `CLEANSTACK`, `WITNESS`, `NULLFAIL`. Bit positions frozen for ABI. All other Bitcoin verify bits are gone from the enum.

**Errors actually assigned in `interpreter.cpp`:** `UNKNOWN_ERROR`, `SIG_PUSHONLY`, `RIGOR_SCRIPT_TYPE`, `NEGATIVE_LOCKTIME`, `UNSATISFIED_LOCKTIME`, `BAD_OPCODE`, `INVALID_STACK_OPERATION`, `PUBKEYTYPE`, `EQUALVERIFY` (hash mismatch), `SIG_DER` (64-byte gate), `SIG_NULLFAIL`, `EVAL_FALSE` (Ed25519 fail), `CLEANSTACK`, `WITNESS_UNEXPECTED`, `WITNESS_MALLEATED`, `WITNESS_PROGRAM_*`, `PUSH_SIZE`, `DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` (misnamed fail-closed).

Everything else in `script_error.h` is **string/table residue**.

---

## 16. Architecture answers (required)

**Should RIGOR keep the Bitcoin opcode namespace?**  
**LEGACY ONLY.** Consensus needs a small closed byte alphabet (pushes + five templates). It does not need CHECKSIG through NOP10. Pre-mainnet is the time to shrink names. Do not keep the namespace “for soft forks.”

**Does RIGOR need an opcode VM?**  
**No.**

**What should future (so-called) soft-forks depend on?**  
They should **not** depend on NOP or unknown witness anyone-can-spend. New rules = new dedicated predicate + classifier entry. That is a **hard fork** under fail-closed. If a later network ever wants true soft-fork restrictiveness, it must be designed explicitly (old nodes already accept the bytes and new nodes add constraints). RIGOR currently has no such latent success path, and should not add one just in case.

**Do leftover opcodes form a real consensus attack surface today?**  
**No execution surface.** An attacker cannot run `OP_CHECKMULTISIG` or `OP_IF` to spend or inflate. Unknown templates cannot be created. Residual risk is **developer reconnection** and **third-party mis-implementation** that copies Bitcoin Script semantics from the enum/RPC and ships a forked node.

---

## 17. What this task did not do

No opcode deleted. No numeric value changed. No `VerifyScript` / classifier / `CheckBlock` / `ConnectBlock` / ScriptPubKey / sighash / Ed25519 / witness / maturity / consensus rule change. Documentation and classification only.

---

## OPCODE AUDIT STATUS

```
LIVE_OPCODE_COUNT: 0
DEAD_BITCOIN_OPCODE_COUNT: 91
TOOLING_OPCODE_COUNT: 115
FUTURE_RESERVED_SURFACE: INERT (NOP/RESERVED/OP_SUCCESS/witness v2–16 are not upgrade hooks)

GENERAL_SCRIPT_VM: NO

BITCOIN_OPCODE_NAMESPACE: LEGACY ONLY

FUTURE_UPGRADE_MODEL: DEDICATED_PREDICATE

CONSENSUS_RISK: LOW

SAFE_TO_BEGIN_OPCODE_CLEANUP: YES

FORMAL_VERIFICATION_ALIGNMENT: PARTIAL
```
