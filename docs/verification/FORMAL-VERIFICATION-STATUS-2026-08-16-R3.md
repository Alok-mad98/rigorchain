> **Historical snapshot (Round 3).** Current freeze:
> [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)

# Formal verification status — Round 3 (2026-08-16)

**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md),
the 2026-08-16 addendum, or [FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md).

## Honest status (unchanged)

The model is strongly verified; the implementation correspondence is much tighter than the original 57 vectors, but it is **not** a refinement proof.

`equivalence_claimed` remains `false`. `hash_kernel_shared_binary_with_cpp` remains `false`.

## What improved in this round

1. **Portable C++ hash CLI.** `formal_verification/tools/cpp_hash_cli.cpp` compiles production `core/src/crypto/sha256.cpp` (C++ Transform only, no SSE/SHANI) without a full `bitcoind` make. SHA-256d is the same composition as `CHash256` in `hash.h`.
2. **Tripartite divergence gate.** `tests/test_hash_kernel_tripartite.py` requires bit-exact agreement of C++ CLI, Python `hashlib`, and (when built) the extracted Coq kernel on all HashKernel / Sha256Fips golden vectors **plus 48 random payloads × {SHA-256, SHA-256d}**. A future Coq/C++ hash split fails this step immediately.
3. **Padding coverage.** FIPS set now includes 63- and 65-byte messages (two-block pad edges) in Coq `CanonicalHashKernel.v` and in the shared JSON. Reorg exclusion adds a sequential-pay / restore pair (`REORG-A9C-006/007`).
4. **Stale `test_bitcoin` detection.** The gate warns if the binary lacks `sha256_fips_golden`, while the portable CLI still covers CSHA256.

Shared vectors **309 → 315**. Coq-enforced harness lemmas **106 → 108**.

## What remains explicitly unproved

- Coq ⇄ C++ refinement.
- A shared object file with C++ (`hash_kernel_shared_binary_with_cpp = false`). The portable CLI is a **test driver** over `sha256.cpp`, not the node binary.
- SIMD SHA-256 paths (SSE4/SHANI/AVX2) vs this portable Transform — still Bitcoin Core `SelfTest` / `SHA256AutoDetect`, not this gate.
- Grinding-cost theorems about real SHA-256; uniformity; network / mempool / P2P.

## Residual hash risk (honest)

Coq (and its OCaml extract) and the portable C++ Transform are now compared on every golden hash vector and a random sample in CI. That catches kernel drift. It does not prove equality on all inputs, and it does not bind the SIMD implementations except through Core's existing self-test.

## Rebuild notes

```bash
bash formal_verification/scripts/run_differential_gate.sh

# C++ BOOST FIPS cases (after operator make of test_bitcoin):
cd ~/rigor-official-tree/core
./src/test/test_bitcoin --run_test=selection_golden_tests,convergence_consensus_tests
```
