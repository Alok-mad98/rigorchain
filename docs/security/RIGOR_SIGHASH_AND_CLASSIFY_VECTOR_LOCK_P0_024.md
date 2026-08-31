# RIGOR Sighash and Classify Vector Lock (P0-024)

Task: `TASK-RIGOR-SIGHASH-AND-CLASSIFY-VECTOR-LOCK-P0-024`  
Date: 2026-08-28  
Honesty:

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

No consensus C++ was modified. `ConnectBlock`, `VerifyRigorPredicate`, asset encodings, witness formats, `RES_MINT` / `CONVERGE` / `OP_RETURN` layouts, Ed25519, maturity, and `chain_id` are unchanged.

This task is a **finite golden-vector lock**, not a ∀ refinement theorem. `refinement_proved` remains **UNCHANGED** (`false`).

Live C++ spend path remains:

```
UTXO → scriptPubKey → AssetTypeFromScriptPubKey → OwnershipPredicate
     → SignatureHash → SHA256d → VerifyEd25519 / VerifyRigorPredicate
```

`core/src/script/interpreter.cpp` does not exist. `SignatureHash` lives in `core/src/script/predicate_verify.cpp`. `sign.cpp` only *calls* that function.

---

## Classification Status

| Surface | Status |
|---------|--------|
| `ClassifyRigorTxout` C++ | **PASS** |
| `AssetTypeFromScriptPubKey` C++ | **PASS** |
| Coq `AssetClassification.classify` | **PASS** |
| Python `asset_from_script` | **PASS** |

### C++ ↔ Coq AssetType map

| Script | C++ `TxoutType` | C++ `AssetType` | Coq `AssetType` |
|--------|-----------------|-----------------|-----------------|
| ORU witness v0 / 20 | `WITNESS_V0_KEYHASH` | `ASSET_ORU` | `AssetORU` |
| RES witness v1 / 32 (pk is the program) | `WITNESS_V1_RES` | `ASSET_RES` | `AssetRES` |
| `RES_MINT` | `RES_MINT` | `ASSET_RES` | `AssetRES` |
| `CONVERGE` | `CONVERGE` | `ASSET_ORU` | `AssetORU` |
| `OP_RETURN` | `NULL_DATA` | `ASSET_NONE` | `AssetNone` |
| unknown (empty, P2WSH, P2PKH, CVG2, RES2, v1/20) | `NONSTANDARD` | `ASSET_INVALID` | `AssetInvalid` |

`RES_MINT` is not a fifth asset. Both layers map it to RES. `CONVERGE` is ORU-classified and unspendable.

SSOT: `formal_verification/vectors/rigor_classification_vectors.json`  
C++ replay: `core/src/test/rigor_classification_vector_tests.cpp`  
Coq: `formal_verification/rigor_v01/VectorAgreement.v` (`classification_vectors_agree`)  
Python: `formal_verification/spec/rigor_consensus_spec.py` `asset_from_script` via `formal_verification/tests/test_p0_024_vector_lock.py`

---

## SignatureHash Status

C++ preimage (frozen, `predicate_verify.cpp`):

```
tag || chain_id || nVersion || hashPrevouts || hashSequence
|| hashOutputs || this prevout || spent scriptPubKey || amount
|| nSequence || nLockTime
```

Coq `SignatureDomainSeparation.signature_preimage` uses the same field order. Compact-size script prefix is `len :: script` for all live templates (`len < 253`). Tags are 8 ASCII bytes at offset 0.

| Path | Tag | Status |
|------|-----|--------|
| ORU P2WPKH | `RGRORU01` | **PASS** |
| RES v1 | `RGRRES01` | **PASS** |
| RES_MINT | `RGRMNT01` | **PASS** |

SSOT: `formal_verification/vectors/rigor_sighash_vectors.json`  
Digests are SHA256d of the preimage, uint256 **internal / serialized** byte order (same as P0-018).  
C++ replay: `core/src/test/rigor_sighash_vector_tests.cpp` calls live `SignatureHash()`.  
Python harness: `python/rigor_sighash.py` reconstructs the preimage independently and SHA256ds it. **Not consensus.**  
Coq: `hash256_bytes preimage = golden_digest` in `VectorAgreement.v` (`sighash_hash_vectors_agree`), plus tag-prefix lemmas (`sighash_tag_vectors_agree`).

---

## Refinement Matrix

Finite vector agreement only. Not `⟦C++ f⟧ = Coq f` for all inputs.

| Item | Status |
|------|--------|
| AssetClassification | **PASS** (vectors) |
| ScriptBoundary | **PASS** (unchanged v0.1 model; classifier vectors cover the admit-set) |
| SignatureDomain | **PASS** (tag + field order + golden preimages) |
| SHA256d Kernel | **PASS** (Coq `hash256_bytes` on the three preimages; C++ `CHash256`; Python `hashlib`) |
| C++ Vector Agreement | **PASS** |
| Python Vector Agreement | **PASS** |
| `refinement_proved` | **UNCHANGED** (`false`) |

---

## What this is not

- Not VST. Not a libsodium / Ed25519 proof.
- Not a change to SHA256, Script format, or a restored Script VM.
- Not a Rust model.
- Python `SignatureHash` is a vector/tool harness only.

---

## Compile verification

| Layer | Command | Result |
|-------|---------|--------|
| C++ `make -j$(nproc)` | `core/` | **PASS** |
| C++ `test_rigor` (full) | `core/src/test/test_rigor` | **PASS** (974 cases, including the two new suites) |
| C++ `make check` | `qt/test/test_rigor-qt` | **FAIL (pre-existing)** — `AppTests::appTests` timed out at 300s (`ip_asn.map` missing; WSL Qt). Not caused by this task. Automake stopped before `check-local`. |
| Python | `formal_verification/tests/test_p0_024_vector_lock.py` | **PASS** |
| Coq `VectorAgreement.vo` | `coqc -R . Rigor -R ../rigor_v01 RigorV01` | **PASS** |
| Coq `Print Assumptions` | `classification_vectors_agree`, `sighash_hash_vectors_agree` | Closed under the global context |

`make -f Makefile.coq clean` of the full historical tree was **not** run: it would rebuild 200+ unrelated modules. `VectorAgreement.v` was compiled against the existing `rigor_v01` + `RigorHash` `.vo` cone.

`_CoqProject.protocol-core` also lists `NetworkCapabilityBoundary.v` and `NetworkSecurityBoundary.v` so the protocol-core partition matches `_CoqProject` (those modules already existed; they were not added as new models in this task).

---

## Files added

| Path | Role |
|------|------|
| `formal_verification/vectors/rigor_classification_vectors.json` | Classifier SSOT |
| `formal_verification/vectors/rigor_sighash_vectors.json` | Sighash SSOT |
| `core/src/test/rigor_classification_vector_tests.cpp` | C++ classifier replay |
| `core/src/test/rigor_sighash_vector_tests.cpp` | C++ `SignatureHash` replay |
| `formal_verification/rigor_v01/VectorAgreement.v` | Coq finite `classify` / `hash256_bytes` lock |
| `python/rigor_sighash.py` | L1 harness (not consensus) |
| `formal_verification/tests/test_p0_024_vector_lock.py` | Python replay |
