# Shared Hash Implementation — Feasibility (2026-08)

**Mode**: Assessment only. This note does **not** ship a production shared hash binary.  
**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md).  
Companion: [LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md](./LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md).

**Honesty (unchanged by this task):**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document does **not** set `hash_kernel_shared_binary_with_cpp = true`. That flag would mean the Coq kernel and production `CHash256` are the same object file. They are not.

---

## 1. Landscape in this tree

| Layer | Artifact | What it actually is |
|-------|----------|---------------------|
| Coq kernel | `formal_verification/coq/RigorSha256.v`, `CanonicalHashKernel.v` | Executable SHA-256 / SHA-256d over `list byte`. FIPS short messages and padding edges are `vm_compute` lemmas. |
| Extraction | `formal_verification/coq/ExtractHashKernel.v` (off `_CoqProject`), `scripts/extract_hash_kernel.sh` | Coq extractor → OCaml `hash256_kernel.ml` + `hash256_cli`. TCB: extractor, `ocamlopt`, `ExtrOcamlNatInt` (`nat` as machine `int`). |
| Portable C++ CLI | `formal_verification/tools/cpp_hash_cli.cpp`, `scripts/build_cpp_hash_cli.sh` | Compiles **the same source** `core/src/crypto/sha256.cpp` **without** `USE_ASM`. C++ Transform only. |
| Production hasher | `core/src/hash.h` `CHash256` (~L49–71), `core/src/crypto/sha256.cpp` | SHA-256d = `SHA256(SHA256(x))`. `CSHA256` dispatches through a function pointer `Transform`. |
| SIMD | `sha256_sse4.cpp`, `sha256_sse41.cpp`, `sha256_avx2.cpp`, `sha256_x86_shani.cpp`, `sha256_arm_shani.cpp` | `SHA256AutoDetect()` (`sha256.cpp` ~L582) swaps `Transform` / `TransformD64_*`. After selection it `assert(SelfTest())` (~L670). |
| Node startup | `core/src/kernel/context.cpp` | Calls `SHA256AutoDetect()` so `rigord` typically does **not** stay on the portable Transform. |
| Tripartite gate | `tests/test_hash_kernel_tripartite.py`, gate step 3 | Bit-exact: portable CLI × hashlib × extracted Coq on HashKernel + Sha256Fips golden rows **plus 48 random payloads × {SHA-256, SHA-256d}**. |

**What the tripartite gate already shows.** On that finite set, three independent implementations agree. A future Coq/C++ hash split fails CI immediately.

**What it does not show.** Equality on all inputs; SIMD paths; the node binary; Coq extractor soundness; a shared `.o` between Coq and C++.

Today the closest thing to “shared hash” is already **same C++ source, two binaries**: `cpp_hash_cli` and `libbitcoin_crypto` both compile `sha256.cpp`. They are still not one object with the Coq kernel, and the node’s *runtime* Transform is often SIMD.

---

## 2. What “shared hash” could mean (ranked by realism)

| ID | Meaning | Realism here | Verdict |
|----|---------|--------------|---------|
| **A** | Stronger **test-only** alignment (status quo+) | High — already 90% built | **Defer** extras; keep current gate |
| **B** | Single reference used by tests and extracted tools | Medium — two CLIs already exist | **Defer** (hygiene only) |
| **C** | Production `rigord` linked against extracted Coq/OCaml kernel | Low — wrong language, wrong TCB, kills SIMD | **Reject** |
| **D** | Hybrid: keep production C++; portable Transform remains the oracle; optional SIMD-vs-portable test | High | **Optional later**, still not the honesty flag |

No other hybrid is more realistic than D. CompCert/C extraction of `RigorSha256` does not exist in this repo. Replacing Bitcoin Core’s SHA-256 with a hand translation of the Coq model would be a consensus-critical rewrite, not a flag flip.

---

## 3. Top realistic options — costs and residue

### A / D — status quo+, optional SIMD check (recommended long-term posture)

**Code/build.** None required now. An optional spike would compile `sha256.cpp` *with* `USE_ASM`, call `SHA256AutoDetect()`, hash the same FIPS/HashKernel payloads, and compare to `cpp_hash_cli`. That lives next to the existing CLI, **not** in `bitcoind`.

**SIMD / performance.** Production stays on AutoDetect. Tests stay on portable Transform unless the extra check is added. No mining or IBD impact.

**CI.** Current gate already builds `cpp_hash_cli` with g++. An SIMD extra step needs a machine with the ISA and a second compile flag set. Flakes if CI CPUs differ.

**Residual risk after adoption.** Still no Coq=C++ theorem. SIMD is only as good as `SelfTest()` plus whatever extra vectors you hash. Extractor/`nat`-as-`int` TCB remains.

**Effort.** Spike: **1–2 person-days**. Production-quality extra gate (CI matrix, skip-if-no-SHANI): **2–4 person-days**.

### C — extracted kernel in the node (not recommended)

**Code/build.** OCaml runtime inside `rigord`, or a C reimplementation of `RigorSha256`. Would have to replace every `CHash256` / `CSHA256` call site (headers, merkle, PoW, scripts, wallets) or intercept `Transform`. Conflicts with `SHA256AutoDetect`. Guix/depends story is new.

**SIMD / performance.** Extracted Coq SHA-256 is a `nat` list interpreter. Unusable for IBD/PoW. Even a C port would lag Core’s SHANI path.

**Residual risk.** Extraction bugs become consensus bugs. `ExtrOcamlNatInt` overflow is a real TCB hole for long messages. Setting `hash_kernel_shared_binary_with_cpp = true` would still be a **lie** if the object is OCaml, not `CHash256`.

**Effort.** Spike to prove it is a bad idea: **2–3 person-days** (already covered by this note). Production-quality: **20–40+ person-days**, high consensus risk. **Reject.**

### B — one test reference

Wire all Python/Coq/C++ hash checks through a single CLI. Low value: the tripartite split is the feature. **Effort 1–2 person-days. Defer.**

---

## 4. Minimal interface sketch (test-only, not merged into bitcoind)

Keep this out of `core/src/bitcoind`:

```
formal_verification/extracted/
  cpp_hash_cli          # already: sha256 | sha256d  <hex>
  hash256_cli           # already: extracted Coq
  # NOT proposed: linking either into rigord
```

A future SIMD oracle would be a third CLI, e.g. `cpp_hash_cli_simd`, same argv, compiled with `USE_ASM`. Still test-only. Still `hash_kernel_shared_binary_with_cpp = false`.

---

## 5. Recommendation (Part A)

| Action | Decision | Why |
|--------|----------|-----|
| Keep tripartite gate + portable CLI | **Go (already done)** | Highest ROI correspondence you already have |
| Extra SIMD-vs-portable hash check | **Defer** | `SelfTest()` already binds SIMD to Transform; extra gate is nice, not load-bearing for σ/root |
| Unify test CLIs (option B) | **Defer** | Cosmetic |
| Extracted kernel in `rigord` (option C) | **Reject** | Language, performance, TCB, consensus blast radius |
| Flip `hash_kernel_shared_binary_with_cpp` | **Reject** | No shared Coq/C++ object exists or should exist after this assessment |

**Do-now:** nothing beyond keeping the current gate green.  
**Person-days already spent on the real alignment:** the freeze-era CLI + tripartite work. Further shared-hash *implementation* should wait until someone has a reason other than the flag name.

---

## 6. Combined next steps (hash + σ/root)

See Part B for the σ / convergence-root argument. Summary:

1. **Do next (optional follow-up task):** lightweight **test-only** correspondence CLIs for `ComputeSigmaHash` and `ComputeConvergenceRootFromEligibilityTxids` — does **not** require a production shared hash.
2. **Defer:** SIMD hash extra gate; any “one binary” hygiene.
3. **Reject:** production extracted hash; refinement proofs; flipping honesty flags.

**Residual risk (honest):** C++ signedness/endianness/window bugs outside the hashed preimage and the A-9-C hex filter can still agree with every current vector. SIMD SHA-256 is trusted via Core `SelfTest`, not via Coq. `equivalence_claimed` remains false.
