# RIGOR Formal Execution Model Gap (P0-023)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-CLOSURE-AND-CPP-REFINEMENT-AUDIT-P0-023`  
Date: 2026-08-27  

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

---

## 1. Trees that were named vs trees that exist

| Named path | Actual path | Role |
|------------|-------------|------|
| `formal_verification/python/` | **does not exist** | — |
| `formal_verification/spec/` | `spec/rigor_consensus_spec.py` | L1 draw / schedule / **classifier** |
| `formal_verification/python-spec/` | RFC-018 fee-burn + supply layer | Monetary, not spend |
| `formal_verification/rust/` | **does not exist** | — |
| `pool/**/*.rs` | Operational pool code | Not a formal model |
| `extraction/` | **does not exist** | — |
| `formal_verification/extracted/` | OCaml SHA-256 kernel + C++ hash CLIs | Hash only |

---

## 2. Is there an executable extracted model of the unique path?

**No.**

`extracted/hash256_kernel.ml` is Coq `RigorSha256` extracted to OCaml. It implements SHA-256 / SHA256d on bytes. It does **not** implement:

| Surface | In extraction? |
|---------|----------------|
| `SignatureHash` preimage layout | No |
| Ed25519 verify boundary | No |
| `AssetTypeFromScriptPubKey` / `classify` | No |
| `VerifyRigorPredicate` | No |
| Mint maturity / HASH160 | No |

README: extracted OCaml is “the Coq kernel compiled, not a shared object with C++.” Differential tests (`test_hash_kernel_tripartite.py`) compare Python / Coq-extract / portable C++ Transform. SIMD `SHA256AutoDetect` is still a separate path.

---

## 3. Python

### 3.1 `spec/rigor_consensus_spec.py` (L1)

| Function | Present? |
|----------|----------|
| `asset_from_script` | **Yes** — ORU / RES / NONE / INVALID, Option B native RES |
| `is_allowed_script` / `is_res_script` | Yes |
| `SignatureHash` / tags / `chain_id` | **No** |
| Ed25519 verify | **No** |
| Mint create = spend / maturity 100 | **No** (mint *shape* only) |

This is the draw-kernel SSOT plus a classifier table used by golden vectors. It is **not** an executable unique-path spend model.

### 3.2 `python-spec/`

RFC-018 fee burn and supply-layer semantics. Honesty flags false. Not SignatureHash / Ed25519 / classification of spends.

---

## 4. Rust

No `formal_verification/rust` model.

`pool/share-validator/src/res_mint.rs` `is_res_script` is an operational duplicate of the classifier. Mint match is looser than C++ unique encoding. **Not a proof. Can drift.**

---

## 5. What would close this gap (not this task)

1. Coq `classify` vs Python `asset_from_script` vs C++ JSON vectors (finite lock).
2. Coq SHA256d of the three `rigor_sighash_p0_018.json` preimages (uses existing `RigorSha256.v`).
3. Do **not** extract Ed25519 from Coq (no RFC 8032 development). Keep libsodium as TCB; lock `VerifyDetached` with C++ tests only.

Until (1)+(2), there is no executable formal spend model. Extraction remains a hash kernel.
