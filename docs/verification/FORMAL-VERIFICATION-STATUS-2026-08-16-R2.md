> **Historical snapshot (Round 2).** Current freeze:
> [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)

# Formal verification status — Round 2 (2026-08-16)

**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md)
or [FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md](./FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md).

## Honest status (unchanged)

The model is strongly verified; the implementation correspondence is much tighter than the original 57 vectors, but it is **not** a refinement proof.

`equivalence_claimed` remains `false`. `refinement_proved` remains `false`.

## What improved in this round

1. **Hash kernel.** Coq `RigorSha256` is locked to FIPS 180-4 short messages and padding edges in `CanonicalHashKernel.v` (`Print Assumptions` → Closed under the global context). SHA-256d is the definitional composition `sha256 ∘ sha256`. The same FIPS / HashKernel vectors are checked in Python `hashlib` and (after rebuilding `test_bitcoin`) C++ `CSHA256` / `CHash256`.
2. **Extracted Coq kernel.** `ExtractHashKernel.v` produces OCaml `hash256_kernel.ml`. `hash256_cli` agrees with hashlib on all HashKernel and Sha256Fips golden vectors. This is the Coq kernel compiled, **not** a shared object with C++.
3. **Differential set.** Shared vectors grew **293 → 309** (encoder field extremes, duplicate-candidate exclusion, overlapping reorg `already_paid`, FIPS SHA-256). Coq-enforced harness lemmas **95 → 106**. `GoldenHarness.v` rebuilt clean.
4. **Gate.** `run_differential_gate.sh` now compares generators to the **working tree**, not `git diff` against HEAD. Uncommitted-but-consistent artifacts no longer fail as “stale”. Failures print the first unified-diff hunk. `TEST_BITCOIN` is auto-detected at `core/src/test/test_bitcoin`. Extracted hasher is built from committed `.ml` when `ocamlopt` is present.

## What remains explicitly unproved

- Coq ⇄ C++ refinement (no function-level equality).
- A single binary shared with C++ (`hash_kernel_shared_binary_with_cpp = false`).
- Grinding-cost / adversarial-advantage theorems about real SHA-256.
- SHA-256 uniformity (fairness stays idealized).
- Network / mempool / P2P.
- Phase-5 freeze catalogs still report full equivalence closure as impossible.

## Residual hash risk (honest)

Three independent implementations remain for production correspondence: Coq (and its OCaml extract, which cannot drift from Coq), C++ `CSHA256`/`CHash256`, and Python `hashlib`. Bit-exact agreement is now checked on FIPS short messages plus the golden HashKernel set. That is still a finite test, not a proof that C++ SHA-256 equals Coq SHA-256 on all inputs.

## Rebuild notes

```bash
# Coq (operator): include the new CanonicalHashKernel.v
cd ~/rigor-official-tree/formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j$(nproc)

# Extracted hasher (optional; needs coqc + ocamlopt)
bash formal_verification/scripts/extract_hash_kernel.sh

# Python + artifact gate (no full bitcoind make)
bash formal_verification/scripts/run_differential_gate.sh

# C++ FIPS / expanded HashKernel cases need a rebuilt test_bitcoin
cd ~/rigor-official-tree/core
# operator: make -j$(nproc) as usual, then:
./src/test/test_bitcoin --run_test=selection_golden_tests
```
