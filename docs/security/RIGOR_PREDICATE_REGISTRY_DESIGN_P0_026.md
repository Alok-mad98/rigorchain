# RIGOR Predicate Registry Design (P0-026)

**Task:** P0-026  
**Status:** Architecture design. Classifier and `VerifyScript` already implement the v0.1 registry in C++; this document names it and freezes how it grows.  
**No consensus change in this task.**

Related: [namespace](./RIGOR_NATIVE_OPCODE_NAMESPACE_AUDIT_P0_026.md), [soft fork](./RIGOR_SOFT_FORK_UPGRADE_MODEL_P0_026.md), Coq `RigorUpgradeNamespace.v`.

---

## 0. Target control flow

```
UTXO
  → ScriptPubKey bytes
  → RigorClassifier          // ClassifyRigorTxout
  → Predicate ID             // closed registry
  → Predicate Registry       // verifier + asset + spendability
  → Verifier                 // Ed25519 today
  → State transition         // CheckTxInputs / ConnectBlock
```

Not:

```
ScriptPubKey → opcode VM → stack → OP_CHECKSIG
```

---

## 1. v0.1 registry (live)

| Predicate ID | ScriptPubKey template | Asset | Spendable | Verifier |
|--------------|----------------------|-------|-----------|----------|
| `PredOruP2WPKH` | `OP_0` `<20 HASH160(pk)>` | ORU | yes | Ed25519, `SigVersion::WITNESS_V0` |
| `PredResV1` | `OP_1` `<32 pk>` native | RES | yes | Ed25519, pk == program |
| `PredResMint` | `<CScriptNum h>` `<20>` `OP_RES_MINT` | RES | after height | Ed25519 + maturity |
| `PredConvergeBurn` | `<20>` `OP_CONVERGE` | ORU | **no** | none (not a UTXO) |
| `PredNullData` | `OP_RETURN` + push-only rest | NONE | **no** | none |

Anything else: **no ID** → `ASSET_INVALID` → not creatable.

C++ mapping:

| ID | `TxoutType` | `VerifyScript` arm |
|----|-------------|-------------------|
| `PredOruP2WPKH` | `WITNESS_V0_KEYHASH` | `VerifyP2WPKHSpend` |
| `PredResV1` | `WITNESS_V1_RES` | v1 key-path |
| `PredResMint` | `RES_MINT` | `VerifyResMintSpend` |
| `PredConvergeBurn` | `CONVERGE` | no success arm (`RIGOR_SCRIPT_TYPE` if spent) |
| `PredNullData` | `NULL_DATA` | unspendable / not a UTXO |

Coq: `predicate_of_spk` in `RigorUpgradeNamespace.v`. Canonical fixtures prove `Some Pred*`. `canonical_p2pkh` / `canonical_p2wsh` prove `None`.

---

## 2. Registry entry (future C++ shape)

Design type for `rigor_predicate_registry.cpp` (follow-up, not this patch):

```text
PredicateEntry {
  id            : PredicateId
  match         : ScriptPubKey → bool     // unique; first match wins, same order as ClassifyRigorTxout
  asset         : AssetType
  spendable     : bool
  verify        : (tx, input, utxo, witness) → bool   // omitted if not spendable
  sighash_tag   : optional domain tag
}
```

Invariants:

1. At most one `match` succeeds.  
2. `match` is a function of script bytes only (script-as-asset).  
3. `verify` must not be an opcode interpreter.  
4. Unknown → no entry, not a default ORU.  
5. Wallet construction: only `match` inverses of registry IDs (plus OP_RETURN commit tooling).

---

## 3. Extending the registry (PredFuture)

`PredFuture(slot)` exists in Coq and is **inactive** in v0.1 (`in_v01_registry = false`).

To activate slot `k` (example: RES v2 / Dilithium):

1. Write the template bytes (new version nibble and/or tagged program). **Do not** reuse `0xac` or any ReservedInert byte as an instruction. A Vacant terminator (`0xbe`+) is allowed only as a **marker**.  
2. Add a `PredicateEntry`.  
3. Add `Verify*` dedicated function.  
4. Add Coq constructor (or move `PredFuture k` into the active set).  
5. Wallet: builder keyed by predicate ID, not by assembling `OP_CHECKSIG`.  
6. Release as **version activation / hard fork** ([soft-fork doc](./RIGOR_SOFT_FORK_UPGRADE_MODEL_P0_026.md)).

Forbidden activation:

- Interpreting `RIGOR_RESERVED_LEGACY_CHECKSIG`  
- Treating `RIGOR_VACANT_FUTURE_*` as SUCCESS  
- Opening witness v2 as anyone-can-spend  

---

## 4. Wallet rule

The wallet may emit only registry templates:

| Allowed | Builder |
|---------|---------|
| ORU | `GetScriptForDestination(WitnessV0KeyHash)` |
| RES v1 | `GetScriptForDestination(WitnessV1Res)` |
| RES_MINT | `GetResMintScript` (coinbase / mint path) |
| CONVERGE | `GetConvergeScript` / `createconvergence` |
| OP_RETURN | commitment helpers (`IsRigorCommitment`) |

**Not allowed:** P2PK `<pk> OP_CHECKSIG`, P2PKH, P2SH, P2WSH, bare multisig, `tr()`.

**P1 bug:** RES `IsMine` still *looks up* `<pk> OP_CHECKSIG` in the keystore. That is not a chain output, but it is Bitcoin Script inside the wallet. Follow-up: match `HaveKey` on the 32-byte program.

Future `RES v2`: new destination type + registry ID. Descriptors: `res()` today; `resv2()` or equivalent later — still not `tr()`.

---

## 5. File map (target vs now)

| Target | Now |
|--------|-----|
| `rigor_classifier.cpp` | `rigor_classify.cpp` |
| `rigor_predicate_registry.cpp` | implicit in classify + `VerifyScript` |
| `rigor_verify.cpp` | `interpreter.cpp` |
| `rigor_opcode.h` | `script.h` `enum opcodetype` |
| `rigor_asset_rules.cpp` | `tx_check.cpp` `IsAllowedScript` |

---

## 6. Formal properties (proved in model)

From `RigorUpgradeNamespace.v` (compiled with `coqc -R . RigorV01`):

| Theorem | Claim |
|---------|--------|
| `no_byte_is_a_vm_instruction` | no `ByteClass` is an instruction |
| `future_slot_not_in_v01_registry` | `PredFuture n` inactive |
| `checksig_classifies_invalid` | `[0xac]` is not a predicate |
| `unknown_template_not_in_registry` | `predicate_of_spk = None` → `AssetInvalid` |
| `activating_future_does_not_enable_vm` | activation ≠ opcode VM |
| `live_canonicals_are_registered` | five v0.1 templates have IDs |

Not proved: refinement to `interpreter.cpp`. `equivalence_claimed = false`.
