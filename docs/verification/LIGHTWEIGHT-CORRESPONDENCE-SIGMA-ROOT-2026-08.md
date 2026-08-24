# Lightweight Correspondence — σ and Convergence Root (2026-08)

**Mode**: Assessment only. Not a refinement proof.  
**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md).  
Hash feasibility: [SHARED-HASH-FEASIBILITY-2026-08.md](./SHARED-HASH-FEASIBILITY-2026-08.md).

**Honesty (unchanged):** `equivalence_claimed = false`, `hash_kernel_shared_binary_with_cpp = false`, `refinement_proved = false`.

Scope is the two **purest** consensus functions. Out of scope: ConnectBlock, window walk, `already_paid` at block level, UTXO/replay, mempool, P2P, winner selection as a game.

---

## 1. Function anchors (this repo)

### σ — preimage then SHA-256d

| Side | Function | Location |
|------|----------|----------|
| C++ | `rigor::ComputeSigmaHash` | `core/src/consensus_rigor/sigma_hash.cpp` L36–94; layout `static_assert`s L27–32; API `sigma_hash.h` |
| C++ test | Re-encodes 100-byte buf, compares to golden preimage **and** `ComputeSigmaHash` | `core/src/test/selection_golden_tests.cpp` `canonical_draw_encoder_layout` ~L327–353; portable `cpp_sigma_root_cli` |
| Coq | `sigma_preimage_bytes`, `sigma_bytes` | `formal_verification/coq/SigmaPoW01.v` L18–22 |
| Coq layout | Offsets 0/32/64/96, LE32 height | `CanonicalDrawEncoder.v` (`SIGMA_*`, `draw_encoder_layout_locked`) |
| Python | `encode_sigma_preimage`, `compute_pcobr_hash` | `formal_verification/spec/rigor_consensus_spec.py` ~L251–292 |

Layout (all three): `txid(32) ‖ C(32) ‖ B_{h-1}(32) ‖ LE32(h)` = 100 bytes. No `target_prev`.

### Convergence root — sort, concat, SHA-256d

| Side | Function | Location |
|------|----------|----------|
| C++ | `Consensus::ComputeConvergenceRootFromEligibilityTxids` | `core/src/consensus/convergence_root_eligibility.cpp`; `n < 2` → `uint256()` **before** hashing; `std::sort` then `CHash256` write loop. Header: `convergence_root_eligibility.h` (also re-exported from `convergence_root.h`) |
| C++ wrapper | `ComputeConvergenceRootFromConvergenceSet` | same file L242–248 — **not** pure: needs coin view / `ListConvergenceRootCommitmentTxids` |
| Coq | `convergence_root_from_txid_bytes` | `ConvergeRoot.v` L11–20 |
| Coq | `converge_root_small` (`\|F\| < 2` ⇒ 32 zero bytes) | `Kernel/KernelRoot.v` L32 |
| Coq | `convergence_root_order_independent` | `OrderIndependence.v` L97 — **model** permutation theorem |
| Python | `compute_convergence_root_from_txids` | `rigor_consensus_spec.py` L196–203 |

**Purity line.** Correspondence can target `ComputeConvergenceRootFromEligibilityTxids` (txid list → 32 bytes). It cannot target `ComputeConvergenceRootFromConvergenceSet` without modeling script/fee/view.

---

## 2. What “lightweight correspondence” can mean here

Not full refinement (`∀ inputs, Coq = C++` as a mechanized theorem). Ranked by fit:

| Style | Already here? | Fit |
|-------|---------------|-----|
| Shared canonical byte encoder + differential on encoder outputs | **Yes.** 10 `SigmaEncoder` vectors, all Coq-enforced (length + txid prefix + height LE). C++ BOOST rebuilds the memcpy buf. | Keep |
| Named 1:1 mapping + checked oracle | **Yes (finite).** Comments in `sigma_hash.h` point at `CanonicalDrawEncoder.v`. Golden JSON + `cpp_sigma_root_cli` (gate step 4). | Keep |
| Extracted reference + C++ wrapper comparison | Hash extract exists; **encoder and root are not extracted** (deferred). Test-only C++ CLI wrapping the two functions is in the gate. | Hash extract optional later |
| Thin “same function on same bytes” (no state/reorg) | What the encoder/root golden families already are, finite. | Status quo |

“Exhaustive” encoder search over `uint32` height is 2^32 hashes — not a lightweight task. Stay finite.

---

## 3. Coverage counts (no new campaign)

From `phase3_golden_vectors.json` tag `tightening-replay-txid-2026-08-18` (324 total):

| Family | Total | Coq-enforced (flag or kernel default) | What is checked |
|--------|------:|--------------------------------------:|-----------------|
| `SigmaEncoder` | 10 | 10 | Preimage layout (Coq harness does **not** re-hash every encoder row) |
| `SigmaPoW01` | 50 | 5 | Full σ digest; most SHA-256-heavy rows are Python/C++ only |
| `ConvergenceRoot` | 25 | 9 | Root digest including `n<2` zeros and one permutation (`CR-010`) |
| `HashKernel` + `Sha256Fips` | 29+8 | 2 + 0 in harness (FIPS locked in `CanonicalHashKernel.v`) | Hash kernel, not encoder |

C++ bridge (`selection_golden_bridge.inc`): **enc=10**, **sig=50**, plus hardcoded selection cases. Root is exercised in `convergence_root_tests.cpp` and via Python `ConvergenceRoot` rows. The portable gate driver is `extracted/cpp_sigma_root_cli` (see [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md)).

**Gap:** many `SigmaPoW01` / `ConvergenceRoot` rows are Python-only on the Coq side because `vm_compute` SHA-256 is expensive. Correspondence for those is L1↔L3 (Python/C++), not L2 `vm_compute`.

---

## 4. Present vs missing

**Present**

- Frozen 100-byte layout on all three layers; C++ `static_assert`.
- Executable SHA-256 in Coq; tripartite hash gate on the **hash** function.
- Model theorems: order independence of the root; `|F|<2` zero root.
- BOOST `canonical_draw_encoder_layout` already compares memcpy buf, `ComputeSigmaHash`, and golden σ.
- Python spec is SSOT for generating the JSON.
- Test-only `cpp_sigma_root_cli` (gate step 4) calls production `ComputeSigmaHash` and `ComputeConvergenceRootFromEligibilityTxids` without `test_bitcoin`.

**Missing (for a tighter lightweight argument)**

- Extraction of `sigma_preimage_bytes` / `convergence_root_from_txid_bytes` (only `sha256_bytes` / `hash256_bytes` are extracted) — **deferred**.
- Mechanized “this C++ function refines this Coq function”.
- SIMD: σ and root both go through `CHash256`, hence AutoDetect at runtime.

**Does not need:** a production shared hash binary. Hash agreement can stay differential.

---

## 5. What you would and would not obtain

**Would obtain (implemented — still not refinement):**

- Bit-exact, CI-cheap checks: Coq extract (or Python) vs C++ `ComputeSigmaHash` / `ComputeConvergenceRootFromEligibilityTxids` on the existing encoder/root/σ golden families, without `test_bitcoin` staleness.
- A named oracle for the two pure functions, analogous to `cpp_hash_cli`.
- Stronger regression detection for endianness on height and for `n<2` root zeros.

**Would not obtain:**

- `refinement_proved = true` or `equivalence_claimed = true`.
- Proof that `ListConvergenceRootCommitmentTxids` matches the spec (view-dependent).
- Winner selection, windowing, mint, reorg.
- SIMD hash equality beyond Core `SelfTest`.
- Coq `OrderIndependence` transferring to C++ `std::sort` (the model theorem is still about Coq lists).

---

## 6. Effort and churn risk

| Follow-up | Effort | Churn risk |
|-----------|--------|------------|
| Docs-only mapping (this note) | Done | None |
| Test-only C++ CLI wrapping the two C++ functions + Python/JSON | **Done** (gate step 4) | Low — `sigma_hash.cpp` + `convergence_root_eligibility.cpp` + portable `sha256.cpp` |
| Also extract encoder+root to OCaml | **+2–3 person-days** | Medium — extraction TCB grows; `nat` lists of txids; keep off `_CoqProject` like `ExtractHashKernel.v` |
| Coq refinement lemmas `ComputeSigmaHash = sigma_bytes` | **weeks–months** | High — needs a C++ semantics or VST/Fiat; out of freeze mindset |
| Broad new golden campaign | Rejected by this task | Avoid |

Churn: `sigma_hash.cpp` and `convergence_root.cpp` are frozen consensus. A CLI that *calls* them is stable. Extracting more Coq into OCaml is the part that bit-rots with Coq 8.15 patches.

---

## 7. Recommendation (Part B)

**CLI follow-up is in-tree** — still **not** a refinement, **not** blocked on shared production hash. Implementation: [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md).

| Decision | Item |
|----------|------|
| **Done** | Test-only `cpp_sigma_root_cli`: `sigma` and `root`, production functions, portable Transform. Wired into `run_differential_gate.sh`. Honesty flags not flipped. |
| **Optional add-on (deferred)** | Extract `sigma_preimage_bytes` + `convergence_root_from_txid_bytes` if operators want a Coq-extract third side. |
| **Not worth yet** | Mechanized refinement; exhaustive height search; modeling `ComputeConvergenceRootFromConvergenceSet`. |
| **Only after shared hash?** | **No.** Shared production hash is rejected/deferred in Part A. Lightweight correspondence uses the **existing** portable `CHash256` / `CSHA256` path. |

Not on the critical path for `equivalence_claimed`.

---

## 8. Combined recommendation (hash + correspondence)

| Now | Later | Never (this freeze) |
|-----|-------|---------------------|
| Keep tripartite hash gate + encoder golden BOOST + σ/root CLI vs JSON | Optional encoder/root Coq extract; optional SIMD-vs-portable hash extra | Production extracted SHA-256 in `rigord` |
| Keep honesty flags false | | Flip `hash_kernel_shared_binary_with_cpp` or `refinement_proved` |

**Operator residual:** model properties (including root order independence) are Coq-only; runtime agreement is finite vectors plus BOOST. Stale `test_bitcoin` no longer blocks the JSON σ/root families (portable CLI).
