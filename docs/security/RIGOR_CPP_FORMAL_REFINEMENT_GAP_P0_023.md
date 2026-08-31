# RIGOR C++ Formal Refinement Gap (P0-023)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-CLOSURE-AND-CPP-REFINEMENT-AUDIT-P0-023`  
Question: is

```
UTXO → scriptPubKey → AssetTypeFromScriptPubKey → fixed predicate
     → Ed25519 → state transition
```

**exactly** the Coq model of the **current C++ implementation**?

Answer: it is the unique **intended** model of both. It is **not** a proved refinement.

```
refinement_proved = false
```

No consensus C++ was modified.

Live spend file: `core/src/script/predicate_verify.cpp` (`VerifyRigorPredicate`).  
The task named `interpreter.cpp`; that file **does not exist** in this tree (renamed/split to predicate verify).

---

## 1. Asset classification

| Case | C++ `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` | Coq `classify` | Match (audit) | Refinement theorem |
|------|------------------------------------------------------|----------------|---------------|-------------------|
| ORU P2WPKH | v0 / 20 → `WITNESS_V0_KEYHASH` → `ASSET_ORU` | `AssetORU` | Yes (shape) | **No** |
| RES native | v1 / 32, not CVG2/RES2 → `WITNESS_V1_RES` → `ASSET_RES` | `AssetRES` | Yes | **No** |
| RES_MINT | `ParseResMintScript` → `RES_MINT` → `ASSET_RES` | `AssetRES` | Yes (canonical encoding) | **No** |
| CONVERGE | `ParseConvergeScript` → `CONVERGE` → `ASSET_ORU` | `AssetORU` | Yes | **No** |
| OP_RETURN | `RIGOR_MARKER_RETURN` + push-only → `NULL_DATA` → `ASSET_NONE` | `AssetNone` | Yes | **No** |
| INVALID | default `NONSTANDARD` → `ASSET_INVALID` | `AssetInvalid` | Yes (fixtures) | **No** |

C++ create reject: `CheckTransaction` `ASSET_INVALID` → `bad-rigor-script-type`.  
Coq: `invalid_cannot_enter_asset_domain` / `invalid_create_is_rejected`.

Gaps:

1. Mint parser: C++ requires `script == GetResMintScript(...)` (unique encoding). Coq `is_res_mint_script` accepts the same *shape family* (locktime push widths) but is not proved equal to `GetResMintScript` round-trip.
2. `ClassifyRigorSpendPredicate` (dispatch) returns `WITNESS_V0_ORU` for any v0 program length; `ClassifyRigorTxout` requires 20 bytes. Spend then fails in `VerifyWitnessProgram`. Not a second *asset* classifier, but it is a second *matcher*. Not modeled as a theorem of equality.
3. No Coq↔C++ byte-vector lock of `classify` vs `AssetTypeFromScriptPubKey` (Python L1 has `asset_from_script`; not extracted into the node).

**Required statement `ClassifyRigorTxout = Coq classify`:** **NOT PROVED.**

---

## 2. RES ownership (Option B)

| Check | C++ | Coq | Gap |
|-------|-----|-----|-----|
| Program encoding | `ResWitnessProgram`: `memcpy(pk → out32)` | `res_witness_program pk = pk` | Definitional match; no SHA256 |
| Spend | `memcmp(vchPubKey, program)==0` then Ed25519 | `res_v1_key_ok` is list equality | Match |
| Curve check | Not in classifier (TASK-010 R-2) | Classifier is byte-shape | Match |
| Alternate derivation | None on spend path | No SHA256 constructor | Match |

**No Option A in live C++ or rigor_v01.** Refinement of `memcmp` to Coq equality is still unproved (trivial but unstated as a CompCert/VST theorem).

---

## 3. Mint correctness

| Step | C++ | Coq | Gap |
|------|-----|-----|-----|
| Create owner | `GetResMintScriptForPubKey`: `HASH160(pk32)` | `mint_create_predicate = mint_pkh` | HASH160 not computed in Coq |
| Spend owner | `VerifyResMintSpend`: `HASH160(pk)==20` | `mint_spend_predicate = mint_pkh` | Same |
| Create = spend | Same 20-byte field | `mint_create_eq_spend` | **Qed (model)** |
| Reward owner | `CheckResRewardOwner`: declared = mint field | `check_res_reward_owner` | Model |
| Coinbase-only create | `bad-txns-res-mint-noncoinbase` | not in rigor_v01 | **Gap** |
| Maturity 100 | `nSpendHeight - nHeight >= nCoinbaseMaturity` | `mint_cannot_bypass_coinbase_maturity` | Model; Params vs constant 100 (regtest uses 1) |
| Script lock | `GetSpendHeight() >= maturity_height` | `script_lock_satisfied` | Model |
| Unique parser | `ParseResMintScript` only | `MintVerifyResMintSpend` only constructor | Model |

**Formal property `mint_owner_create = mint_owner_spend`:** **Qed in the model.** Not a HASH160 implementation proof.

Regtest `nCoinbaseMaturity = 1` is a **parameter gap**: Coq freezes 100. Production mainnet params are 100.

---

## 4. Signature domain separation

C++ `SignatureHash` (`predicate_verify.cpp`):

```
tag || chain_id || nVersion || hashPrevouts || hashSequence || hashOutputs
 || this prevout || spent scriptPubKey || amount || nSequence || nLockTime
```

Tags: `RGRORU01` / `RGRRES01` / `RGRMNT01`. `chain_id` is an explicit checker argument (genesis hash).

| Field | Coq `SighashFields` | Covered |
|-------|---------------------|---------|
| Three tags | `tag_oru` / `tag_res` / `tag_mint` | Yes |
| `chain_id` (32) | `sh_chain_id` | Yes (injectivity) |
| tx commitments | nVersion, hashPrevouts, hashSequence, hashOutputs, prevout, nSequence, nLockTime | Yes (in the record) |
| `scriptPubKey` | `sh_script` + compact-size | Yes |
| amount | `sh_amount` | Yes |

Gaps:

1. Coq proves **preimage injectivity**, not `HashWriter::GetHash` = SHA256d.
2. Golden **digests** live in `core/src/test/data/rigor_sighash_p0_018.json` (C++ only).
3. Compact-size in Coq is one byte for `len < 253`; C++ uses Bitcoin compact-size (same for live templates).
4. `hashPrevouts` construction (`GetPrevoutsSHA256`) is not reimplemented in rigor_v01.

---

## 5. Spend dispatch vs state transition

| C++ | Coq | Refinement |
|-----|-----|------------|
| `VerifyRigorPredicate` | `spend_authorized` + path | **No** |
| Three paths only | `SpendPath` three constructors | Model match |
| `CheckTxInputs` INVALID/NONE spend | `spend_rejected` | Model |
| `ConnectBlock` mint/subsidy/fee-burn | `apply_block_pools` identity | **No** (DualPool is a different cone) |
| Opcode VM | `opcode_vm_in_consensus = false` | Model; C++ has no `EvalScript` |

`sign.cpp` / `pubkey.cpp` are wallet/key helpers. They call `VerifyRigorPredicate` / `VerifyDetached`. Not a second consensus interpreter. Not modeled.

`validation.cpp` ConnectBlock composition is outside rigor_v01.

---

## 6. Gap list (must stay open)

| ID | Gap | Severity for freeze |
|----|-----|---------------------|
| G-1 | No theorem `ClassifyRigorTxout = classify` | Blocks FREEZE_READY |
| G-2 | No theorem `VerifyRigorPredicate = spend_authorized` | Blocks FREEZE_READY |
| G-3 | SHA256d(preimage) not in rigor_v01 | Blocks “one signature meaning” as a Coq digest |
| G-4 | RFC 8032 / libsodium not in Coq | Blocks crypto freeze-by-Coq |
| G-5 | Mint unique-encoding vs Coq shape parser | Medium |
| G-6 | Coinbase-only mint create not in v0.1 | Medium |
| G-7 | `nCoinbaseMaturity` param vs Coq 100 | Low (mainnet 100) |
| G-8 | DualPool `apply_block` ≠ `apply_block_pools` | Expected; do not cite DualPool as ConnectBlock |
| G-9 | `ClassifyRigorSpendPredicate` v0 length | Dispatch vs classifier; fail-closed in verify |

**Refinement coverage: 0%.** Model alignment of the unique path: see freeze certificate.
