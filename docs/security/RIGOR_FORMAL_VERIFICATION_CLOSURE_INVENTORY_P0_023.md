# RIGOR Formal Verification Closure Inventory (P0-023)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-CLOSURE-AND-CPP-REFINEMENT-AUDIT-P0-023`  
Scope: `formal_verification/rigor_v01/` theorem classification.  
Date: 2026-08-27  
Honesty:

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

No consensus C++ was modified. Historical proofs in `formal_verification/coq/` were not deleted.

| Label | Meaning |
|-------|---------|
| **KEEP** | Directly models live v0.1 consensus (citable) |
| **ARCHIVE** | Historical Bitcoin-derived; do not cite as v0.1 spend |
| **ADD** | Required RIGOR property that was missing; added this task |
| **REMOVE** | Duplicate of a KEEP theorem; keep the file, do not cite twice |

---

## 0. Expected vs actual surface

Task-expected citable surface:

| Module | Role |
|--------|------|
| `AssetClassification.v` | Unique classifier |
| `ScriptAssetBoundary.v` | Script-as-asset / ownership predicates |
| `Ed25519Ownership.v` | Single signature boundary |
| `SignatureDomainSeparation.v` | Tagged preimage + `chain_id` |
| `ResMintCorrectness.v` | Mint create = spend |
| `NoInterpreterConsensus.v` | No VM |

Added this task (Phase 5):

| Module | Role |
|--------|------|
| `AssetStateTransition.v` | INVALID rejected; dual-pool supply identity |

Present from P0-025/026 (byte/predicate freeze; not Bitcoin VM proofs):

| Module | Classification |
|--------|----------------|
| `RigorByteNamespace.v` | **KEEP** (canonical “no opcode semantics”) |
| `RigorPredicateRegistry.v` | **KEEP** (five live predicates) |
| `RigorUpgradeNamespace.v` | **KEEP** (registry + future slots) |
| `RigorUpgradeBoundary.v` | **KEEP** (predicate-extension only) |
| `RigorWalletOwnership.v` | **KEEP** supporting (wallet, not consensus) |
| `RigorOpcodeNamespace.v` | **REMOVE** (duplicate of ByteNamespace) |

Nothing in `rigor_v01/` is EvalScript / P2WSH / CHECKMULTISIG / Taproot / ECDSA semantics. Reject fixtures (canonical P2WSH, P2PKH) are KEEP as negative cases.

---

## 1. Theorem inventory

### 1.1 `AssetClassification.v` — KEEP (+ ADD)

| Theorem / lemma | Class | Live C++ |
|-----------------|-------|----------|
| `option_b_program_is_pubkey` | KEEP | `ResWitnessProgram` = memcpy |
| `unique_classifier_is_classify` | KEEP | `AssetTypeFromScriptPubKey` |
| `classify_is_a_function_of_script_only` | KEEP | no stored asset tag |
| `no_asset_from_*` (version/address/descriptor/explorer) | KEEP | forbidden second sources |
| `p2wpkh_is_oru` | KEEP | v0/20 → ORU |
| `native_res_is_res` / `native_res_program_eq_pk` | KEEP | v1/32, program = pk |
| `mint_is_res` | KEEP | RES_MINT → RES |
| `converge_is_oru` | KEEP | CONVERGE → ORU |
| `opret_is_none` | KEEP | OP_RETURN → NONE |
| `*_is_invalid` (empty, P2WSH, RES2, CVG2, P2PKH) | KEEP | INVALID |
| `same_script_same_asset` | KEEP | purity |
| `invalid_cannot_enter_asset_domain` | **ADD** | `bad-rigor-script-type` |
| `p2wsh_cannot_enter_asset_domain` / `p2pkh_cannot_enter_asset_domain` | **ADD** | same |

### 1.2 `ScriptAssetBoundary.v` — KEEP

All ownership-predicate theorems: KEEP. Same script ⇒ same predicate. Spendable templates are disjoint constructors. CONVERGE / NONE / INVALID are not spendable.

### 1.3 `Ed25519Ownership.v` — KEEP

Three spend paths, all `AuthEd25519`. RES v1 requires `pk == program`. `spend_authorized` requires encoding + key-binding + opaque crypto bit. RFC 8032 is not implemented in Coq.

### 1.4 `SignatureDomainSeparation.v` — KEEP

Tags `RGRORU01` / `RGRRES01` / `RGRMNT01`. Preimage injectivity in tag, `chain_id`, `scriptPubKey`, amount. Field order matches `SignatureHash` in `predicate_verify.cpp`. SHA256d of the preimage is not proved here.

### 1.5 `ResMintCorrectness.v` — KEEP (+ ADD)

`mint_create_eq_spend`. Owner encoding is HASH160(pk), not Option A. Both maturity layers. **ADD:** `mint_cannot_bypass_coinbase_maturity` (age < 100 rejected).

### 1.6 `NoInterpreterConsensus.v` — KEEP (+ ADD)

No EvalScript / Miniscript / P2WSH / CHECKMULTISIG / Taproot / ECDSA constructors. **ADD:** `validation_independent_of_opcode_evaluation`.

### 1.7 `AssetStateTransition.v` — ADD (now KEEP)

`invalid_create_is_rejected`. Spend of INVALID/NONE/CONVERGE rejected. `total_supply_conservation`: `total' = total + subsidy + valid_mint - oru_burn`. Not ConnectBlock refinement. Stronger dual-pool remains `ReplayReorgCore/DualPoolConservation.v`.

### 1.8 Supporting namespace

| Module | Class |
|--------|-------|
| `RigorByteNamespace.v` | KEEP |
| `RigorPredicateRegistry.v` | KEEP |
| `RigorUpgradeNamespace.v` | KEEP |
| `RigorUpgradeBoundary.v` | KEEP |
| `RigorWalletOwnership.v` | KEEP supporting |
| `RigorOpcodeNamespace.v` | **REMOVE** from citable set (duplicate) |

### 1.9 Outside `rigor_v01/`

| Location | Spend-surface class |
|----------|---------------------|
| `coq/ReplayReorgCore` dual-pool / shape | LIVE_OTHER |
| `CvgCovenant.v` / `legacy_account_model/` | ARCHIVE |
| Phase-4/5 / `FormalVerificationCertification.v` | Paperwork; `final_verdict <> RV_Equivalent` |

---

## 2. Duplicates (REMOVE from citations)

| Item | Cite instead |
|------|----------------|
| `RigorOpcodeNamespace.no_byte_is_instruction` | `RigorByteNamespace.no_opcode_semantics` |
| Repeated SMW classifier examples | `rigor_v01` fixtures |

Files were not deleted.

---

## 3. Phase 5 missing list — closed in-model

| Proof | Before | After |
|-------|--------|-------|
| 1 Unknown ↛ asset domain | classify INVALID only | `invalid_cannot_enter_asset_domain` + `invalid_create_is_rejected` |
| 2 No opcode evaluation | boolean flags | `validation_independent_of_opcode_evaluation` |
| 3 Mint maturity ≥ 100 | conjunct | `mint_cannot_bypass_coinbase_maturity` |
| 4 Supply identity | DualPool other cone | `total_supply_conservation` (model) |

Still missing as **C++ refinement** (intentionally not claimed):

- `ClassifyRigorTxout = classify`
- `VerifyRigorPredicate = spend_authorized`
- `SignatureHash` digest = Coq SHA256d
- `VerifyDetached` = RFC 8032 in Coq
