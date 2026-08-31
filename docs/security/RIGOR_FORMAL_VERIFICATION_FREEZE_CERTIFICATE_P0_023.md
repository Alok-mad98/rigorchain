# RIGOR Formal Verification Freeze Certificate (P0-023)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-CLOSURE-AND-CPP-REFINEMENT-AUDIT-P0-023`  
Date: 2026-08-27  
Type: Model alignment + missing-obligation close. **Not** a C++ refinement proof.

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Freeze readiness: `PARTIAL_READY`**

This document is a **certificate of the model**, not of `rigord`.  
Forbidden reading: “formally verified implementation”, “C++ refinement proved”, “Coq = runtime”.

Companions:

- [Closure inventory](./RIGOR_FORMAL_VERIFICATION_CLOSURE_INVENTORY_P0_023.md)
- [C++ refinement gap](./RIGOR_CPP_FORMAL_REFINEMENT_GAP_P0_023.md)
- [Execution-model gap](./RIGOR_FORMAL_EXECUTION_MODEL_GAP_P0_023.md)

---

## 1. Unique path (live C++ and Coq model)

```
UTXO
  → scriptPubKey
  → AssetTypeFromScriptPubKey / classify
  → fixed OwnershipPredicate
  → VerifyEd25519 / spend_authorized
  → state transition (create/spend pools)
```

C++ spend entry: `VerifyRigorPredicate` in `predicate_verify.cpp`.  
C++ classifier: `rigor_classify.cpp`.  
Coq: `formal_verification/rigor_v01/`.

No EvalScript. No Bitcoin Script VM. No ECDSA spend. No Taproot. No Miniscript.

---

## 2. Verified (about the model — Qed)

| Item | Status | Primary theorem / module |
|------|--------|--------------------------|
| Asset classification boundary | ✔ model | `classify`; unique function of script; unknown → INVALID |
| Unknown scripts cannot enter asset domain | ✔ model | `invalid_cannot_enter_asset_domain`, `invalid_create_is_rejected` |
| No interpreter consensus | ✔ model | `no_eval_script`, `validation_independent_of_opcode_evaluation` |
| Fixed ownership predicates | ✔ model | `ownership_of`; three spendable constructors |
| Ed25519 ownership model | ✔ model boundary | `every_spendable_path_is_ed25519`; `pk == program` on RES v1 |
| Mint create/spend equivalence | ✔ model | `mint_create_eq_spend`; not Option A |
| Mint cannot bypass maturity 100 | ✔ model | `mint_cannot_bypass_coinbase_maturity` |
| Signature domain separation | ✔ preimage | three tags + `chain_id` + script + amount injectivity |
| Supply invariants | ✔ model identity | `total_supply_conservation`; DualPool remains the stronger apply_block cone |

Allowed public phrasing:

> Model proved. Property proved. Invariant proved (of the Coq model).

---

## 3. Not yet formally closed

| Item | Status |
|------|--------|
| C++ `ClassifyRigorTxout` = Coq `classify` | Open (G-1) |
| C++ `VerifyRigorPredicate` = `spend_authorized` | Open (G-2) |
| C++ Ed25519 (`libsodium` `VerifyDetached`) refinement | Open (G-4) |
| SHA256d implementation refinement (`HashWriter::GetHash`) | Open (G-3) |
| `SignatureHash` byte-level digest equivalence | Open (JSON goldens are C++ only) |
| Runtime extraction of the unique path | Open (extract is SHA-256 kernel only) |
| ConnectBlock = `apply_block_pools` | Open |
| Coinbase-only mint create in rigor_v01 | Open (G-6) |

Honesty flags in `DrawBridgeProgress.v` / STATUS / rigor_v01 README remain **false**.  
`FormalVerificationCertification.v` is artifact paperwork: `final_verdict <> RV_Equivalent`. It is **not** a C++ certificate. Do not cite its filename as freeze authority.

---

## 4. False-claim audit (Phase 3)

Searched: `refinement_proved`, `equivalent`, `certified` under `formal_verification/` and freeze docs.

| Finding | Action |
|---------|--------|
| `refinement_proved = false` everywhere checked | **Keep false.** No flip. |
| Phase-5 / Certification modules reject `RV_Equivalent` | Honest; misleading *names* only |
| No `refinement_proved = true` | None to remove |
| No “formally verified implementation” in `formal_verification/` | None to remove |

This certificate does **not** introduce those phrases as claims.

---

## 5. Coverage

| Slice | Coverage |
|-------|----------|
| Unique-path **model** obligations (P1–P5 + P0-023 P1–P4) | **100%** stated and Qed in `rigor_v01` |
| C++ **refinement** of that path | **0%** |
| Executable extract of spend (SignatureHash + Ed25519 + classify) | **0%** |
| Combined freeze-grade (model 7 + refinement 4, all required for FREEZE_READY) | **7 / 11 ≈ 64%** |

**Current formal coverage percentage (freeze-grade): 64%.**  
**Model-only coverage of the unique path: 100%.**

---

## 6. Missing theorem list (production)

1. `ClassifyRigorTxout_eq_classify` (byte function equality or finite vector lock)
2. `VerifyRigorPredicate_eq_spend_authorized`
3. `SignatureHash_digest_eq_sha256d_preimage` (replay `rigor_sighash_p0_018.json` in `RigorSha256.v`)
4. Unforgeability / RFC 8032 (leave as TCB; do not fake in Coq)
5. `CheckTransaction` coinbase-only mint in rigor_v01
6. Extraction of `classify` (optional; Python already has it)

---

## 7. Freeze readiness

**`PARTIAL_READY`**

| Grade | Meaning | This tree |
|-------|---------|-----------|
| `NOT_READY` | Unique path not even the Coq model | No — model matches |
| `PARTIAL_READY` | Model Qed; C++ unrefined | **Yes** |
| `FREEZE_READY` | Model + refinement or equivalent vector lock of classify + SignatureHash + verify | **No** |

Protocol rules may be frozen by RFC/C++ lock. Formal verification **cannot** be cited as freeze-authority for “the node implements the spec” until G-1–G-3 close or are explicitly excluded from the public claim.

---

## 8. Next TASK recommendation

**`TASK-RIGOR-SIGHASH-AND-CLASSIFY-VECTOR-LOCK-P0-024`**

Do not start VST/CompCert of libsodium.

Do:

1. Replay the three `rigor_sighash_p0_018.json` preimages through existing `RigorSha256.hash256_bytes` (closes G-3 for the goldens).
2. Add a finite C++/Python/Coq table for `AssetTypeFromScriptPubKey` vs `classify` / `asset_from_script` (closes G-1 as a lock, not a ∀ theorem).
3. Leave Ed25519 in libsodium; do not extract it.
4. Keep `refinement_proved = false` unless a real `⟦C++ f⟧ = Coq f` theorem exists.

That is the honest next step from PARTIAL_READY toward a spend-surface freeze citation, without restoring a Script VM.
