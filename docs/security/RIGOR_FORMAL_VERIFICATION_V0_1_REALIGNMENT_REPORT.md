# RIGOR Formal Verification v0.1 Realignment Report

Task: `TASK-RIGOR-FORMAL-VERIFICATION-MODEL-REALIGNMENT-AUDIT-P0-022`  
Type: Audit + model gap analysis. **No consensus code changed.**  
Date: 2026-08-26  
Honesty flags (unchanged, required):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This task does not restore Bitcoin Script, multisig, EvalScript, or any compatibility path. It asks whether the *formal* stack still describes the unique live path:

```
UTXO
  → scriptPubKey
  → AssetTypeFromScriptPubKey
  → fixed OwnershipPredicate
  → VerifyEd25519(signature, SHA256d(tagged preimage), pk)
```

**Verdict:** that path **is** the unique C++ consensus model. It was **not** the unique formal model. The old Coq cone proves draw / schedule / pooling-from-script-shape and explicitly *non-claims* a VM; it does not prove Ed25519, sighash domain separation, or mint create=spend. Those properties are now defined and proved **about a new model** in `formal_verification/rigor_v01/`. Historical proofs were archived by classification, not deleted.

---

## 0. Live C++ (audit baseline — not modified)

Authoritative spend surface after EvalScript / P2WSH / Miniscript / CHECKMULTISIG removal:

| Template | `scriptPubKey` | Asset | Ownership | `SigVersion` / tag |
|----------|----------------|-------|-----------|-------------------|
| ORU P2WPKH | `OP_0 <20>` = `HASH160(pk)` | ORU | reveal `pk`; `HASH160(pk)==program` | `WITNESS_V0` / `RGRORU01` |
| RES native v1 | `OP_1 <32>` = **`pk` (Option B)** | RES | witness `<sig><pk>` with **`pk == program`** | `WITNESS_V1_RES` / `RGRRES01` |
| RES mint | `<maturity> <20> OP_RES_MINT`, 20 = `HASH160(pk)` | RES | scriptSig `<sig><pk>`; `HASH160(pk)==20`; height | `RES_MINT` / `RGRMNT01` |
| OP_CONVERGE | `<20> OP_CONVERGE`, 20 = `HASH160(pk)` | ORU burn | **unspendable** (`AddCoins` skip) | — |
| OP_RETURN | push-only, `nValue==0` | NONE | unspendable | — |
| else | — | **INVALID** | not creatable | — |

Confirmations against this tree:

- `AssetTypeFromScriptPubKey` is the only assigning function (`script/rigor_classify.cpp`). `CTxOut` has no asset field.
- `ed25519::ResWitnessProgram` is `memcpy(pk → program)` — **Option B**, not SHA256(pk).
- `VerifyWitnessProgram` v1: `memcmp(vchPubKey, program) == 0` then `CheckEd25519Signature`.
- `GetResMintScriptForPubKey` / `GetConvergeScriptForPubKey`: `HASH160(pk32)`. Under Option B this equals `HASH160(v1 program)`.
- `ParseResMintScript` is the unique mint parser (canonical encoding only). `VerifyResMintSpend` uses that parser, then Ed25519 under `SigVersion::RES_MINT`.
- `SignatureHash`: prefix tag (8 ASCII bytes) || `chain_id` (genesis hash, explicit argument) || `nVersion` || `hashPrevouts` || `hashSequence` || `hashOutputs` || this prevout || **real** `scriptPubKey` || amount || nSequence || nLockTime. No hashtype byte. No `SigVersion::BASE`.
- `EvalScript` is **gone** from `core/src` (no `bool EvalScript`).
- RES mint maturity: script lock (`GetSpendHeight() >= maturity_height`) **and** coinbase 100 (`RES_MINT_MATURITY = COINBASE_MATURITY`; `Coin::RequiresMaturity` iff `IsCoinBase`). Mint is coinbase-only.

This is the unique model. Formal work that still talks about EvalScript, P2WSH execution, Option A, or `uint32{1}` as the only domain tag is describing a deleted chain.

---

## 1. Obsolete formal models (失效列表)

Historical `.v` files were **not deleted**. They remain under `formal_verification/coq/` so ReplayReorgCore / Kernel / Phase catalogs still build. Classification lives in [`formal_verification/archive/OBSOLETE_MODEL_INVENTORY.md`](../../formal_verification/archive/OBSOLETE_MODEL_INVENTORY.md). Snapshots of the retired covenant study and the legacy-account README were copied to `formal_verification/archive/coq/`.

| Model | Old formal status | Classification | Cite as v0.1 spend? |
|-------|-------------------|----------------|---------------------|
| EvalScript VM | never a Coq interpreter; named as a non-claim in `OpaqueBoundary.v` | **OBSOLETE** | No |
| Opcode execution semantics | `CvgCovenant.v` OP_CVG_CLAIM study | **OBSOLETE** | No |
| Miniscript semantics | no Coq module | **OBSOLETE** | No |
| P2WSH semantics | reject fixture only (`canonical_p2wsh`) | **OBSOLETE** (as a spend) | No |
| CHECKMULTISIG semantics | absent | **OBSOLETE** | No |
| Taproot / BIP341 semantics | comments / residual names | **OBSOLETE** | No |
| ECDSA / secp256k1 spend | absent | **OBSOLETE** | No |
| Option A `program = SHA256(pk)` | freeze text in P0-018/019 audits | **OBSOLETE** | No |
| `SigVersion::BASE` alias + trailing `uint32{1}` | P0-018 pre-remediation `SignatureHash` | **OBSOLETE** | No |
| PCORB script path on v1 RES | `parse_pcorb_shape` still in ScriptShape | **OBSOLETE** as spend; residual as size gate | No |
| OP_CVG_CLAIM / AssetCVG | `CvgCovenant.v` (archived snapshot) | **OBSOLETE** | No |
| `legacy_account_model/` as ConnectBlock / EvalScript | already deprecated in its README | **OBSOLETE** for spend | No |

**Still valid, different cone (not spend proofs):** SHA-256d kernel, σ / Convergence_Root / winner order-independence, `ResSchedule` / cap, dual-pool on `apply_block`, RFC-018 fee-burn model, 215 Coq golden harness lemmas. Those must not be thrown out with the VM.

**Partial, must not be over-cited:**

| Artifact | Aligned | Residual gap |
|----------|---------|--------------|
| `ScriptMintWitness.asset_from_script` | Option B native RES; unknown → Invalid | No Ed25519, no chain_id, still `Require CvgCovenant`, PCORB sizes |
| `AssetFromScript.v` | pooling = `f(script)` | Not ownership / signature |
| `ResMintObligation.v` | mint *count* vs `W(h)` / abstract `cm_draw_won` | Not HASH160 owner, not mint tag, not maturity |
| Python `asset_from_script` | classifier | No SignatureHash |
| Rust formal model | — | **Does not exist.** Pool `is_res_script` is an operational duplicate with a looser mint match |

P0-018 `RIGOR_ED25519_SIGNING_DOMAIN_AUDIT` and P0-019 Option A freeze text are **historical audits of a then-shipping tree**. C++ has been remediated (tags + `chain_id` + Option B). Citing those documents as the *current* signed message is itself a model-skew failure.

---

## 2. New RIGOR v0.1 formal model

Location: [`formal_verification/rigor_v01/`](../../formal_verification/rigor_v01/)  
Logical Coq library: `RigorV01` (`-R ../rigor_v01 RigorV01` in `_CoqProject`).  
Compiled with Coq 8.15.0, 0 `Axiom` / `Admitted`.

### Unique path (the only model of spend)

```
CTxOut = (nValue, scriptPubKey)          -- no stored asset tag
AssetType     = classify(scriptPubKey)   -- AssetClassification.v
Ownership     = ownership_of(scriptPubKey)
SpendPath     ∈ {ORU P2WPKH, RES v1 key-path, RES_MINT}
Authorization = encoding_ok ∧ key_binding ∧ Ed25519_Verify
signed_message = SHA256d( tag || chain_id || tx commitments || prevout
                          || scriptPubKey || amount || nSequence || nLockTime )
```

Forbidden sources of `AssetType` are modeled as functions that return `None`:

- witness version alone
- address
- descriptor
- explorer table

Option B is definitional:

```
res_witness_program(pk) = pk
```

The classifier never hashes the key. RES2 / CVG2 prefixes are `ASSET_INVALID`.

### Property 1 — Asset Classification Uniqueness

**Statement.** For every script `spk`, `AssetType = classify(spk)`. There is no second classifier in the model.

**Proved in** `AssetClassification.v`: `unique_classifier_is_classify`, `classify_is_a_function_of_script_only`, `no_asset_from_witness_version` / `address` / `descriptor` / `explorer`. Fixtures: P2WPKH→ORU, native RES→RES, mint→RES, converge→ORU, OP_RETURN→NONE, empty/P2WSH/RES2/CVG2/P2PKH→INVALID.

**Not proved:** C++ `ClassifyRigorTxout` = Coq `classify` (no refinement). Wallet / explorer second tables are outside this cone (P0-018 H-2/H-3 class).

### Property 2 — Script-as-Asset Integrity

**Statement.** Same `scriptPubKey` ⇒ same asset and same ownership predicate. Distinct *spendable* templates are distinct predicates even when HASH160 material coincides (ORU pkh ≠ mint pkh ≠ converge pkh as constructors). RES v1 ownership material **is** the program.

**Proved in** `ScriptAssetBoundary.v`: `same_script_same_ownership`, constructor disjointness, canonical fixture inequalities.

**Interpreted injectivity:** the predicate includes lock *kind*, so the same 20-byte HASH160 on ORU vs mint vs converge is three predicates — you cannot spend a mint UTXO with the P2WPKH checker. That is the intended invariant. Unspendable scripts (INVALID / NONE / CONVERGE) share “no spend path”; they are not claimed injective.

### Property 3 — Single Signature Boundary

**Statement.** Every spendable path is `VerifyEd25519(signature, signed_message, pk)`. No multisig, no opcode auth, no interpreter.

**Proved in** `Ed25519Ownership.v` / `NoInterpreterConsensus.v`: `SpendPath` has exactly three constructors; `auth_of_spend p = AuthEd25519`; `spend_authorized` requires encoding + key-binding + crypto-accept; RES v1 authorized spend implies `pk = program`.

**Not proved:** RFC 8032 (libsodium). Coq uses a size encoding gate plus an opaque `sa_crypto_accept` bit. That is the honest boundary: the *type* has no other constructor; the *algorithm* is C++.

### Property 4 — Signature Domain Separation

**Statement.**

```
signature_preimage =
    tag            -- 8 bytes: RGRORU01 | RGRRES01 | RGRMNT01
 || chain_id       -- 32 bytes, genesis hash, explicit argument
 || nVersion       -- 4
 || hashPrevouts   -- 32
 || hashSequence   -- 32
 || hashOutputs    -- 32
 || this prevout   -- 36
 || compact_size || scriptPubKey
 || amount         -- 8
 || nSequence      -- 4
 || nLockTime      -- 4
```

Different chain, different path tag, different script, or different amount cannot produce the same well-formed preimage.

**Proved in** `SignatureDomainSeparation.v`: pairwise distinct tags; `different_tags_different_preimages`; `different_chain_ids_different_preimages`; `different_scripts_different_preimages` (compact-size + fixed-width suffix); `different_amounts_different_preimages`; `different_paths_different_preimages`. Golden tag/chain_id prefix locked to `RGRORU01` || `0x11`×32 matching `core/src/test/data/rigor_sighash_p0_018.json`.

**Not proved:** SHA256d of the preimage (C++ `HashWriter::GetHash`); collision resistance; Coq replay of the three digest hex strings. Those goldens remain a C++ vector lock.

### Property 5 — Ownership Conservation

**Statement.** A created spendable UTXO is spendable only by satisfying the Ed25519 check for the key committed in *that* `scriptPubKey`. No script bypass, no alternate interpreter, no hidden opcode.

**Proved in** `Ed25519Ownership.v`: `spendable_own_has_ed25519_path`; `unspendable_own_has_no_path`; no fourth `SpendPath` constructor. `NoInterpreterConsensus.v` records EvalScript / Miniscript / P2WSH / CHECKMULTISIG / Taproot / ECDSA as `false` flags and non-constructors.

**Not proved:** “only the holder of the corresponding private key” as a cryptographic theorem (would require an unforgeability axiom, which this cone refuses). Production unforgeability is libsodium’s RFC 8032, assumed, not Coq.

---

## 3. Properties that must be re-proved (and current status)

| Property | Old cone | v0.1 model (`rigor_v01`) | C++ refinement | Freeze-grade? |
|----------|----------|--------------------------|----------------|---------------|
| P1 Classification uniqueness | partial (`asset_kind_of_script`) | **Qed** (model) | No | Model yes / node no |
| P2 Script-as-asset | pooling only | **Qed** (model) | No | Model yes / node no |
| P3 Ed25519 unique auth | **MISSING** | **Qed** boundary; crypto opaque | No | Not freeze-grade |
| P4 tag \|\| chain_id domain | **MISSING** | **Qed** preimage injectivity; SHA256d not in cone | No | Partial |
| P5 Ownership conservation | non-claim (no VM) | **Qed** (no bypass constructor) | No | Model yes / crypto no |
| RES mint create = spend = HASH160(pk) | mint *count* vs `W(h)` | **Qed** (predicate identity; HASH160 not computed) | No | Partial |
| Mint maturity (100 + script lock) | absent | **Qed** (two-layer predicate) | No | Model only |
| Mint signature domain `RGRMNT01` | absent | **Qed** (tag inequality) | No | Model only |

“Must re-prove” relative to production still includes a **refinement theorem** `VerifyScript = rigor_v01 spend_authorized`. That theorem does not exist. This task does not claim it.

---

## 4. Deleted surfaces — no proof required

Do **not** rebuild Coq semantics for:

| Surface | Action |
|---------|--------|
| EvalScript / opcode VM / altstack / CODESEPARATOR | **无需证明** — deleted from consensus |
| Miniscript | **无需证明** |
| P2WSH redeem / witness v0/32 spend | **无需证明** — `ASSET_INVALID` |
| CHECKMULTISIG / CHECKSIGADD | **无需证明** |
| Taproot script path / control block / annex | **无需证明** |
| ECDSA / DER / LowS as a spend | **无需证明** |
| Option A `SHA256(pk)` as the v1 program | **无需证明** — forbidden constructor |
| PCORB as a second way to spend RES | **无需证明** — deleted (A-7) |
| OP_CVG_CLAIM covenant | **无需证明** — never shipped |

Keeping `CvgCovenant.v` in the *build* is an over-approximation (the model admits a class the node cannot create). That is the safe direction and is already documented in `OpaqueBoundary.v`. It is **not** a v0.1 specification.

---

## 5. Proof coverage matrix

| 组件 | 旧证明 | 状态 | 新证明 |
|------|--------|------|--------|
| EvalScript | 不存在（仅 non-claim） | **删除** | **无需** |
| Opcode VM | 不存在 | **删除** | **无需** |
| Miniscript | 不存在 | **删除** | **无需** |
| P2WSH | reject fixture / Python canonical | **删除（spend）** | **无需** |
| Multisig / CHECKMULTISIG | 不存在 | **删除** | **无需** |
| Taproot | 不存在 | **删除** | **无需** |
| ECDSA spend | 不存在 | **删除** | **无需** |
| Option A SHA256(pk) | 审计文档曾冻结 | **删除** | **禁止**（Option B Qed） |
| Asset classification | 部分（`AssetFromScript.v` / SMW） | **需要更新** | **必须** — `AssetClassification.v` **Qed** |
| Script-as-Asset | 部分（same script ⇒ same pool） | **缺失所有权谓词** | **必须** — `ScriptAssetBoundary.v` **Qed** |
| Ed25519 verify | **新增 / 缺失** | **缺失算法** | **必须** — 边界 Qed；RFC 8032 仍为 C++ |
| chain_id + 三标签 | **新增 / 缺失** | **缺失** | **必须** — 原像单射 Qed；SHA256d 仍为 C++ 向量 |
| RES mint owner HASH160(pk) | 旧 mint-count 模型 | **更新** | **必须** — `ResMintCorrectness.v` **Qed**（谓词层） |
| RES mint maturity | C++ only | **缺失** | **必须** — 双层谓词 Qed |
| No interpreter | 注释 / OpaqueBoundary | **更新** | **必须** — `NoInterpreterConsensus.v` **Qed** |
| Draw / σ / C / winner | Kernel + OrderIndependence | **LIVE_OTHER** | 保持；非 spend |
| ResSchedule / cap | `ResSchedule.v` / obligation | **LIVE_OTHER** | 保持；非 spend |
| Dual-pool | `DualPoolConservation.v` | **LIVE_OTHER** | 保持（仍为 `apply_block` 模型） |

---

## 6. Mainnet freeze — formal-verification status

| Question | Answer |
|----------|--------|
| Is the unique v0.1 *consensus* path ScriptPubKey → unique classifier → fixed predicate → Ed25519? | **Yes, in C++.** |
| Did the old formal cone already prove that path? | **No.** It proved script *shape* and pooling-from-script, and honestly non-claimed a VM. |
| Does a new formal cone now *state and prove* that path (as a model)? | **Yes.** `formal_verification/rigor_v01/`, six modules, Coq 8.15.0, 0 Admitted. |
| Is Coq = `rigord`? | **No.** `refinement_proved = false`. |
| Is Ed25519 itself proved? | **No.** Encoding + unique constructor. Algorithm is libsodium. |
| Is HASH160 computed in Coq? | **No.** 20-byte field identity only. Under Option B, `HASH160(program)=HASH160(pk)` is definitional on the program, not a RIPEMD160 proof. |
| Are sighash goldens in Coq? | **No.** C++ JSON `rigor_sighash_p0_018.json` (three paths). Coq proves preimage *structure*. |
| Can formal verification be cited as freeze-authority for spend? | **Not yet.** Cite: “v0.1 spend model is aligned and machine-checked; production equivalence is not.” |
| Can it be cited as freeze-authority for “no EvalScript / no multisig”? | **As a model of dispatch, yes.** As a theorem about the linked `bitcoind`, no. |

Allowed public sentence (unchanged):

> Formally verified protocol invariants with explicit verification boundaries.

Forbidden (still):

> Formally verified blockchain / Coq = runtime / mainnet freeze certified by Coq.

**Spend-surface freeze recommendation:** C++ is freeze-shaped (Option B, tagged sighash, unique classifier, no EvalScript). Formal spend proofs are now *about the right model*. They are not a substitute for the C++ tests and P0-018/020 code audits until a refinement or a byte-level vector lock of `SignatureHash` + `VerifyDetached` exists in this cone.

---

## 7. What this task changed (and did not)

**Changed (formal only):**

- `formal_verification/archive/` — inventory + snapshots; historical proofs kept in `coq/`
- `formal_verification/rigor_v01/` — six Coq modules + README
- `formal_verification/coq/_CoqProject` and `_CoqProject.protocol-core` — `-R ../rigor_v01 RigorV01` and the six sources (protocol-core 69 modules)
- `.gitignore` — v0.1 `.vo` artifacts
- this report

**Not changed:** `core/src/**`, wallet, explorer, Python L1 (except it was audited), rust pool, draw/schedule Coq.

**Not restored:** EvalScript, Script VM, Miniscript, P2WSH spend, CHECKMULTISIG, Taproot, ECDSA spend, Option A.

---

## 8. Residual gaps (do not paper over)

1. **Refinement.** `RigorV01.classify` vs `AssetTypeFromScriptPubKey`; `spend_authorized` vs `VerifyScript`. Unproved.
2. **SHA256d(preimage) in Coq.** `RigorSha256.v` can consume the JSON preimages; this task did not add those lemmas.
3. **HASH160 / RIPEMD160.** Mint owner is a 20-byte field. Collision resistance assumed.
4. **`ScriptMintWitness` / `CvgCovenant` still in the protocol-core build.** Classifier is largely aligned; covenant and PCORB shape are retired semantics still compiled. Safe over-approx, not the unique model.
5. **Python L1** has no SignatureHash. **Rust formal model** does not exist. Pool `is_res_script` can drift.
6. **Second classifiers outside consensus** (wallet version→asset, explorer table) are out of this Coq cone; P0-018/019 still apply to those clients.

Closing (1) and (2) is the next honest step if formal verification is to speak at freeze time about *signatures*. Closing (4) is a later deletion of retired constructors, not a consensus change.
