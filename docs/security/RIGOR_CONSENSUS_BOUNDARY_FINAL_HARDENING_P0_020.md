# TASK-RIGOR-CODEBASE-CONSENSUS-BOUNDARY-FINAL-HARDENING-P0-020

Post P0-015 tree review. Audit only. Frozen consensus model is not changed.

Required shape:

```
ScriptPubKey
  → AssetTypeFromScriptPubKey
  → Fixed Ownership Predicate
  → Ed25519 Verification
```

Must remain true: no EvalScript, no Script VM, no Miniscript, no ECDSA spend, no secp256k1 library, no P2SH/P2WSH spend, no multisig consensus path, no second asset classifier, no `gArgs` in ConnectBlock / interpreter.

Live templates (do not change):

1. ORU — witness v0 / 20, `HASH160(pk32)`
2. RES — witness v1 / 32 native `pk32` (not CVG2 / RES2 prefix)
3. RES_MINT — `<maturity> <20-byte HASH160> OP_RES_MINT`
4. CONVERGE — `<20-byte HASH160> OP_CONVERGE` (burn, not a spendable UTXO)
5. NONE — `OP_RETURN` and `nValue == 0`

---

## A. Must isolate (security / script-as-asset)

These cannot fork the chain. `CheckTransaction` / `IsStandard` still reject non-RIGOR locks. They violate the freeze rule that the wallet must not materialize a non-RIGOR script.

| Item | Evidence | Action |
| --- | --- | --- |
| `raw()` descriptor | `script/descriptor.cpp` `ParseScript` still accepts top-level `raw(hex)` and builds an arbitrary `CScript`. `pk` / `pkh` / `combo` / `multi` / `sortedmulti` / `sh` / `wsh` / `tr` / `rawtr` already return parse errors. | Reject unless the bytes are a live RIGOR template or `OP_RETURN`. Do not add curve checks to `ClassifyRigorTxout`. |

**A items that fork consensus: none.**

`GetScriptForRawPubKey` and `GetScriptForMultisig` are already deleted. `GetScriptForDestination(PKHash / ScriptHash / WitnessV0ScriptHash / WitnessUnknown)` returns empty. `-addresstype=legacy` / `P2SH_SEGWIT` fail wallet load. `GetNewDestination` `assert(false)` on those types. `createmultisig` / `addmultisigaddress` / `outmultisig` reject.

---

## B. Suggested (architecture hygiene)

None of these change `VerifyScript` or `ConnectBlock` predicates.

| Item | Evidence | Suggested action |
| --- | --- | --- |
| `observational_metrics` still under `consensus/` | `chain.h` includes it. `ConnectBlock` writes after accept. Getters are documented NEVER-read-during-validation. | Move beside `node/supply_monitor/`. Keep the `CBlockIndex` field. |
| `coq_supply_field_hook` still under `consensus/` | Includes `node/supply_monitor`. `CheckCoqHookMatchesAuthoritativeCaches` returns strings; it does not reject blocks. | Move with the supply monitor. Keep as an audit projection. |
| `IsMineInner` leftover types | `wallet/scriptpubkeyman.cpp` still treats Solver `PUBKEY` / `PUBKEYHASH` / `SCRIPTHASH` / `WITNESS_V0_SCRIPTHASH` / `MULTISIG` as potentially `SPENDABLE`. | Return not-mine for those types. Wallet must not believe it owns a lock the chain will never accept. |
| Dead `ScriptHash` inserts | `GetScriptPubKeys` / import still call `GetScriptForDestination(ScriptHash(...))`, which is now empty. | Delete the inserts. |
| `TxSemanticKind::ResClaim` | Enum + Qt labels. `ClassifyTxSemantic` never assigns it. | Delete. UI must not imply a fifth asset class. |
| `TxoutType` zombies | `PUBKEY`, `PUBKEYHASH`, `SCRIPTHASH`, `MULTISIG`, `WITNESS_V0_SCRIPTHASH`, `WITNESS_UNKNOWN` remain for switches. Unreachable from `ClassifyRigorTxout`. | Mark `LEGACY_ONLY`. Do not teach the classifier to emit them. |
| `rigor-tx outscript` / `createrawtransaction` hex | Utilities can still attach an arbitrary `scriptPubKey`. | Reject unless `ClassifyRigorTxout` is a live template or `OP_RETURN`. |
| Stale comments | `script.h` still mentions EvalScript on retired `OP_CVG_CLAIM`. `observational_metrics.h` still says EvalScript. | Rewrite comments. |

`runtime_supply_*` is already out of `consensus/` (`node/supply_monitor/`). That P0-015 move stands.

---

## C. Keep (do not delete)

| Item | Why |
| --- | --- |
| `GetLegacySigOpCount` | Live CheckBlock costing. Counts one per live WPKH / RES / RES_MINT output, not CHECKSIG bytes. Changing it forks. |
| `CountScriptOpsLegacyTestOnly` | Already named. Tests and fuzz only. Not used by CheckBlock. |
| `RigorTags::Res2WirePrefix` (ASCII `RES2`) | Consensus-locked **reject** spelling for fake v1 programs. Not an asset class. Native RES is unprefixed `pk32`. |
| `RigorTags::CVG2` / `PCOR` | Wire reject tags. Production rejects PCORB carriers. |
| SHA-1 in `crypto/` | Explicitly retained. Not a spend primitive. |
| `CheckECDSASignature` sealed false stub | Documents that ECDSA cannot return true. Removing the symbol invites a later re-hook. |
| `g_scriptExecutionCache` / `g_draw_set_cache` / `g_opus_pow_ok` | Memoization after validation, not a consensus lookup from `gArgs`. |
| Unread `SCRIPT_VERIFY_*` bits | Unread flags do not change the four predicates. |
| `OutputType::LEGACY` / `P2SH_SEGWIT` enum | Wallet load already errors. Keep so old configs fail closed. |

`LegacyAsset` does not exist in `src`. Allowed vocabulary: **ORU, RES, CONVERGE, RES_MINT**.

---

## Freeze constraints (do not authorize)

- Do not restore `EvalScript`.
- Do not put curve checks in the classifier (TASK-010 R-2).
- Do not fold observational supply into `ConnectBlock`.
- Do not change `GetLegacySigOpCount`.
- Do not move SHA-1.

After isolating `raw()`, the consensus model is freeze-ready. Directory moves of `observational_metrics` are optional and must not alter write-after-accept semantics.
