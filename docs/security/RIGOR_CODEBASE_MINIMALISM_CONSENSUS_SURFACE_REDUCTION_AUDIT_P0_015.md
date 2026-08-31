# RIGOR Codebase Minimalism and Consensus Surface Reduction

**TASK-RIGOR-CODEBASE-MINIMALISM-CONSENSUS-SURFACE-REDUCTION-AUDIT-P0-015**

**Date:** 2026-08-25
**Mode:** Audit-first. No refactor in this pass.
**Principle:** Minimum consensus boundary = minimum attack surface = maximum verifiability.

---

## Verdict

The **live spend path already matches** the required shape:

```
ScriptPubKey  →  Fixed Ownership Predicate  →  Ed25519 Verify
```

It does **not** match a General Script VM at runtime.

`EvalScript` is gone from `core/src`. `VerifyScript` is structural dispatch
(mint / v0 P2WPKH / v1 key-path). `AssetTypeFromScriptPubKey` is the unique
consensus classifier. Spend signatures are Ed25519 via libsodium.
`CheckECDSASignature` is a sealed `false` stub. `gArgs` is fenced off the
script/consensus path.

The remaining problem is **compiled residue**: Bitcoin opcode names,
`SCRIPT_VERIFY` bits with no reader, P2SH sigop accounting, miniscript,
ECDSA vtable slots, and observational files living under `consensus/`.
That residue is not exploitable today. It is the same class of risk P0-018
assigned to `EvalScript` before that file was deleted: a freeze of today's
dispatch does not freeze a later one-line re-hook.

**Do not start deleting consensus counters in this task.** Follow the
phased order below.

---

## 1. Current attack surface map

### 1.1 Live consensus kernel (keep)

| Component | Location | Role |
|---|---|---|
| `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` | `script/rigor_classify.cpp` (72 lines) | Unique script→asset map |
| `IsAllowedScript` / `CheckTransaction` | `consensus/tx_check.cpp` | Creatable output set |
| `CheckTxInputs` dual-pool | `consensus/tx_verify.cpp` | Conservation |
| `VerifyScript` | `script/interpreter.cpp` | Three predicates only |
| `VerifyP2WPKHSpend` | same | ORU: `<64-byte sig> <32-byte pk>` |
| Witness v1 key-path | same | RES: program == pk32 |
| `VerifyResMintSpend` | same | Mint: height + HASH160(pk) + Ed25519 |
| `ParseConvergeScript` | same | Unspendable burn template |
| `ed25519::VerifyDetached` | `crypto/ed25519_provider.*` | Only signature primitive |
| `rigor_consensus_lock.h` | `consensus/` | Explicit numeric locks |

Spend templates on a valid chain:

1. Witness v0 / 20 — ORU P2WPKH
2. Witness v1 / 32 native RES (not CVG2/RES2-prefixed)
3. `<maturity> <20-byte HASH160> OP_RES_MINT`
4. `<20-byte HASH160> OP_CONVERGE` (destroyed, not a UTXO)
5. `OP_RETURN` at `nValue == 0` (`ASSET_NONE`)

### 1.2 Script system — not a VM, still looks like one

| Item | Status |
|---|---|
| `EvalScript` | **Deleted.** No `bool EvalScript` in `core/src`. |
| Opcode interpreter loop | **Deleted** with EvalScript. |
| `EvalPushOnlyScript` | Live. Push parser for mint `scriptSig` only. |
| `opcodetype` enum in `script.h` | **Full Bitcoin table** still present (CAT, MUL, CHECKMULTISIG, CHECKSIGADD, NOP1–10, CLTV, CSV, burned `0xbc`, `OP_CONVERGE`). |
| `MAX_OPS_PER_SCRIPT`, `MAX_STACK_SIZE`, `MAX_PUBKEYS_PER_MULTISIG` | Dead constants after EvalScript removal. |
| `SCRIPT_VERIFY_*` | 20 Bitcoin bits still in `interpreter.h`. |
| Flags actually read by `VerifyScript` | `WITNESS`, `SIGPUSHONLY`, `NULLFAIL`, `CLEANSTACK` (+ `FLAG18` stripped). |
| Flags ORed by `GetBlockScriptFlags` but unread | `DERSIG`, `CLTV`, `CSV`, `NULLDUMMY`, `MINIMALDATA`, `STRICTENC`, `CONST_SCRIPTCODE`, `MINIMALIF`. |
| `CheckLockTime` / `CheckSequence` on the checker | Implemented, **never called** by `VerifyScript`. BIP68 `SequenceLocks` / `IsFinalTx` still run at tx/block layer (keep). |
| `miniscript.h` | 2184 lines, compiled. Not on ConnectBlock. |
| `descriptor.cpp` | 1603 lines, still parses miniscript / `multi()`. |
| `MatchMultiA` | CHECKSIGADD matcher. Unreachable as a chain output. |
| Compatibility script (`bitcoinconsensus`) | Still accepts Bitcoin-style flag masks; sets `chain_id = ZERO`. |

P0-018 C-1 (delete EvalScript) is **done**. This audit's leftover is the
shell that EvalScript used to inhabit.

### 1.3 Asset classification boundary

**Unique consensus source:** `AssetTypeFromScriptPubKey` → `ClassifyRigorTxout`.

| Caller class | Uses unique source? |
|---|---|
| `CheckTransaction` / `CheckTxInputs` / conservation | Yes |
| Wallet send (`WalletStrictSendDestinationOk`, `spend.cpp`) | Yes |
| `key_io` asset of a destination | Yes (after building the script; never HRP) |
| `IsStandard` | Yes (`ClassifyRigorTxout`) |
| `IsWitnessStandard` | **No.** Branches on witness version/length directly. Policy only. |
| `GetDustThreshold` size model | Witness-program size heuristic, not an asset class. Policy only. |
| Descriptor infer | `Solver` == `ClassifyRigorTxout`. OK. |
| Explorer / Qt | Call `AssetTypeFromScriptPubKey`. OK. |

Forbidden classifiers (wallet / explorer / descriptor / witness-version /
fallback) are **not** used for conservation. The policy duplicate in
`IsWitnessStandard` is a second table for relay, not a second asset identity.

`IsRigorResNativeWitnessProgram` is a pure byte predicate (TASK-010 R-2).
On-curve checks stay in address encode, `IsStandard`, and
`IsValidDestination`. Do not move them into classification.

### 1.4 Cryptographic surface

| Primitive | Role |
|---|---|
| Ed25519 (libsodium) | **Only** spend / mint signature verify |
| SHA-256 / HASH160 (SHA-256+RIPEMD160) | Sighash, P2WPKH program, mint/converge owner hash |
| OpuHash | PoW, not a spend signature |
| secp256k1 library | **Absent** from `core/` |
| `CPubKey::Verify` | Ed25519 on `0x00\|\|pk32`, not ECDSA |
| `CheckECDSASignature` | Always `false` |
| `CPubKey::CheckLowS` / `NormalizeSignatureDER_LowS` | Size==64 stubs |
| `SCRIPT_ERR_SIG_DER` | **Misnamed.** Used for “sig is not 64 bytes” |
| `SCRIPT_ERR_SECP256K1_DISABLED` | Error enum still exists; EvalChecksig that would raise it is gone |
| `XOnlyPubKey::{ComputeTapTweakHash,CheckTapTweak}` | Taproot names; `CreateTapTweak` returns nullopt |
| SHA-1 | Still compiled (`crypto/sha1.cpp`); interpreter no longer executes `OP_SHA1` |

### 1.5 Global / implicit consensus state

Fences that hold:

- `lint-no-global-state.py` forbids `gArgs`, `SetRigorResEvalContext`, and
  observational ledgers as script/mint inputs.
- `GetBlockScriptFlags` ignores `block_index` / `chainman`. No delayed
  script soft-fork.
- `PrecomputedTransactionData::SetChainId` is an explicit caller input.
- `res_wallet_bootstrap_coinbase` is on `Consensus::Params`, set at chain
  construction, default 0 on public networks.

Still present (memoization, not rule lookup):

| Object | Class |
|---|---|
| `g_scriptExecutionCache` | Successful script-result memo. Insert after all checks. |
| `g_draw_set_cache` | Rebuild of F. Must not diverge from disk reconstruction. |
| `g_opus_pow_ok` | Successful PoW memo. Must not cache failures as success. |
| `ObservationalMetrics` / `runtime_supply_*` | Explorer/index. Must not reject blocks. |

Implicit unused parameters (not read by script flags, still on `Params`):

- `BIP34Height`, `BIP65Height`, `BIP66Height`, `CSVHeight`, `SegwitHeight`
- `DeploymentHeight()` / `getblockchaininfo` softfork theater
- `SegwitHeight` still used for a `NeedsRedownload` copy string in
  `node/chainstate.cpp`

`STANDARD_SCRIPT_VERIFY_FLAGS` in `policy.h` still lists the Bitcoin zoo
(`LOW_S`, `WITNESS_PUBKEYTYPE`, `DISCOURAGE_OP_SUCCESS`, …) even though
the interpreter does not read them.

### 1.6 Size snapshot (source lines)

| File / group | Lines | Class |
|---|---|---|
| `script/interpreter.cpp` | 715 | Live spend + sealed ECDSA stubs |
| `script/rigor_classify.cpp` | 72 | Live classifier |
| `script/script.h` + `script.cpp` | 640 + 369 | Opcode table + GetSigOpCount |
| `script/miniscript.h` + `.cpp` | 2184 + 412 | Residue |
| `script/descriptor.cpp` | 1603 | Residue |
| `script/sign.cpp` | 880 | Wallet signing, still miniscript-aware |
| `consensus/runtime_supply_*` | ~2.5k | Observational, wrong directory |
| `validation.cpp` | 6962 | Mixed node + consensus |
| `ed25519_provider.*` | 101 | Live crypto |

---

## 2. Components that can be deleted

### Phase 1 — no CheckBlock / ConnectBlock behavior change

| ID | Delete | Gain |
|---|---|---|
| D-1 | `miniscript.{h,cpp}`, `MatchMultiA`, `wsh()` / `multi()` / miniscript descriptor paths, related tests/fuzz | Removes a second script language from the binary |
| D-4 | ECDSA vtable (`CheckECDSASignature`, `VerifyECDSASignature`), `CheckLowS`, `NormalizeSignatureDER_LowS`, `SCRIPT_ERR_SIG_DER` rename, Dummy/PSBT checkers that exist only for ECDSA | One cryptographic entry in the type system |
| D-5 | Unreachable `TxoutType` (`PUBKEY`, `PUBKEYHASH`, `SCRIPTHASH`, `MULTISIG`, `WITNESS_V0_SCRIPTHASH`, `WITNESS_UNKNOWN`); make `GetScriptForDestination` refuse P2PKH/P2SH/P2WSH; delete `GetScriptForRawPubKey` / `GetScriptForMultisig` from send/construct paths | Wallet cannot emit consensus-invalid locks |
| D-6 | `IsRes2AssetProgram` (defined, never called), `WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE`, `XOnlyPubKey` tap-tweak methods, `ScriptExecutionData` dead fields | Retired RES2 / Taproot names leave the spend header |
| D-7 | `CheckLockTime` / `CheckSequence` on `BaseSignatureChecker` (keep `IsFinalTx` + BIP68 `SequenceLocks`) | Opcode CLTV/CSV stop existing after EvalScript |
| D-10 | SHA-1 as a consensus include; `bitcoinconsensus` Bitcoin flag mask (`SCRIPT_VERIFY_P2SH` requirement) | External verifiers cannot pretend P2SH flags matter |

### Phase 2 — consensus-adjacent, prove unread first

| ID | Delete / shrink | Gain |
|---|---|---|
| D-2 | Unread `SCRIPT_VERIFY_*` bits; `GetBlockScriptFlags` returns only bits `VerifyScript` reads; align `STANDARD_SCRIPT_VERIFY_FLAGS` | Script flags stop looking like a configurable VM |
| D-8 | Move `runtime_supply_*`, `observational_metrics`, Coq adapter files out of `consensus/` | Reviewers stop treating explorer ledgers as ConnectBlock |
| D-9 | Buried BIP heights on `Params` after rewriting the `SegwitHeight` reindex string | Params contains only RIGOR locks |

### Phase 3 — consensus-affecting; optional and only with a lock bump

| ID | Change | Why it forks |
|---|---|---|
| D-3 | Replace `GetLegacySigOpCount` / `GetP2SHSigOpCount` / `CScript::GetSigOpCount` Bitcoin CHECKSIG/CHECKMULTISIG costing | `CheckBlock` still sums legacy sigops against `MAX_BLOCK_SIGOPS_COST`. Coinbase `scriptSig` is not a RIGOR template; `0xac` bytes still count. |

**Do not** put an on-curve check into `AssetTypeFromScriptPubKey`.
That is a Level-5 fork (TASK-010 R-2 / freeze declaration).

---

## 3. Deletion gain

| If deleted | Attack surface | Verifiability |
|---|---|---|
| Miniscript + descriptors that emit it | No compiled alternative Script language | Coq/script models stay at three templates |
| Unread SCRIPT_VERIFY bits | No “turn the VM back on” flag | Flag set is a 4-bit constant, auditable |
| ECDSA names | No second verifier symbol | One `VerifyDetached` in the vtable |
| TxoutType zombies + P2PKH constructors | No accidental invalid locks | Wallet output = classifier input |
| Observational files out of `consensus/` | Smaller review surface | `consensus/` ≈ ConnectBlock |
| Sigop counter (Phase 3) | No CHECKMULTISIG cost bomb via coinbase padding | Cost = number of live spends |

---

## 4. Consensus impact (strict)

| Change | Fork? | Reason |
|---|---|---|
| EvalScript already deleted | Shipped | Unreachable after structural `VerifyScript` |
| Stop ORing unread SCRIPT_VERIFY bits | **No, if proven unread** | Readers listed in §1.2 |
| Delete `GetP2SHSigOpCount` | Neutral **iff** no P2SH UTXO can exist | `IsAllowedScript` rejects P2SH; branch is 0 on valid chains |
| Change `GetLegacySigOpCount` | **Yes** | `CheckBlock` uses it; coinbase `scriptSig` can contain opcode bytes |
| Curve check in classifier | **Level-5 fork** | Frozen as a byte predicate |
| Remove BIP68 / `IsFinalTx` | **Yes** | Independent of EvalScript |
| Wallet miniscript deletion | **No** | Those scripts are already `ASSET_INVALID` |
| Move observational files | **No** | Path only |
| `bitcoinconsensus` flag ABI | Not a chain fork | External API |

`IsAllowedScript` and `CheckTxOutAssetWitnessBinding` overlap (both switch
on `ClassifyRigorTxout`). Merging them is a readability win, not a rule
change, if the accepted set stays identical including `NULL_DATA` vs
spendable.

---

## 5. Recommended final code structure

```
core/src/
  consensus/
    lock.h                 # rigor_consensus_lock + Params (RIGOR fields only)
    classify.cpp           # AssetTypeFromScriptPubKey (moved from script/)
    tx_check.cpp           # IsAllowedScript, CheckTransaction
    tx_verify.cpp          # dual-pool, IsFinalTx, SequenceLocks
    ownership.cpp          # three Ed25519 predicates (today: interpreter.cpp)
    convergence/           # draw, mint, root, PCORB
    merkle.cpp
  crypto/
    ed25519_provider.*     # only signature primitive
    sha256 / ripemd160     # HASH160 + sighash
  node/                    # validation.cpp orchestration, caches
  observational/           # runtime_supply_*, ObservationalMetrics
  wallet/
    descriptors            # wpkh + res v1 only; no miniscript
```

**Not in the kernel:**

- opcode interpreter, EvalScript, FindAndDelete, altstack
- secp256k1, ECDSA, DER, CheckLowS
- P2PKH / P2SH / P2WSH constructors
- SCRIPT_VERIFY bits with no reader
- miniscript
- buried Bitcoin deployments

Target `VerifyScript` (already true in spirit):

```
if mint template:   VerifyResMintSpend   → Ed25519 (RES_MINT domain)
else if v0/20:      VerifyP2WPKHSpend    → Ed25519 (ORU domain)
else if v1/32 native: key-path           → Ed25519 (RES domain)
else: reject
```

No flag-selected subscript. No P2SH. No P2WSH. No Taproot control block.

---

## 6. Implementation order

0. **Record.** EvalScript / P2WSH / secp spend / unique classifier are done.
   Do not re-open them.

1. **Names and wallets (first PR).** D-1, D-4, D-5, D-6, D-7, D-10.
   Exit: `rg` over `core/src` shows no miniscript, no `CheckECDSA`, no
   `GetScriptForMultisig` on construct/send paths. All unit/functional
   tests that only existed for Bitcoin script vectors are deleted or
   rewritten against the three templates.

2. **Flags and directories.** D-2, D-8, D-9.
   Exit: `GetBlockScriptFlags` returns only bits `VerifyScript` reads.
   `lint-no-global-state.py` still green. `consensus/` no longer contains
   runtime-supply observational adapters.

3. **Sigops, optional, pre-mainnet only.** D-3.
   Replace Bitcoin CHECKSIG costing with a RIGOR cost (1 per live spend
   lock; 0 for OP_RETURN / OP_CONVERGE; reject CHECKSIG in counted
   scripts). Requires a written lock bump and golden vectors.
   **Do not do this after protocol freeze.**

---

## 7. Risk analysis for Phase 1–2

| Risk | Mitigation |
|---|---|
| Silent flag reader missed in D-2 | `rg SCRIPT_VERIFY_` over `core/src`; add a unit test that `GetBlockScriptFlags ==` the four live bits |
| Wallet still needs miniscript for imported descriptors | Refuse parse; do not keep a compiler “just in case” |
| DummySignatureChecker still implements ECDSA | Delete the virtual; PSBT extractors use push parsing + Ed25519 only |
| `SegwitHeight` removal breaks reindex copy | Rewrite the message to a RIGOR witness-mandatory sentence first |
| Moving `runtime_supply_*` breaks includes | Path-only PR, no logic |

---

## 8. What this audit does *not* authorize

- Moving Ed25519 on-curve checks into `AssetTypeFromScriptPubKey`
- Restoring EvalScript “for tests”
- Accepting secp256k1 spends
- Classifying assets from HRP, witness version, or descriptors
- Reading `gArgs` from ConnectBlock / interpreter
- Changing `GetLegacySigOpCount` without an explicit consensus lock bump

---

## 9. Relation to prior audits

| Prior | Status relative to this task |
|---|---|
| P0-018 C-1 delete EvalScript | **Done** in this tree |
| P0-018 H-1 BASE/WITNESS_V0 alias | Signing-domain issue; out of scope here (see P0-018). `SigVersion` now has `RES_MINT` as a third tag. Re-check before freeze. |
| TASK-010 R-2 byte-only classifier | **Holds.** Do not undo. |
| P0-010 §4.4 repository surface | Still true: C++ spend path is small; repository is not |
| FENCE W1 / W5 / W11 | Hold. Observational ledgers and `gArgs` stay off the spend path |

---

*End of audit. No code was changed.*
