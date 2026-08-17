# Addendum — Formal Verification Verdict (2026-08-16)

**Status**: Addendum to [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md)
**Does not supersede the Verdict's honesty boundary.** Full Coq ⇄ C++ refinement is still **not** proved. `equivalence_claimed` remains `false`.

This note records post-verdict remediation of the gaps named in Verdict §3.

---

## Closed in this phase

| Verdict gap | What landed | Where |
|---|---|---|
| A-9-C no-double-win unmodeled | Coq model of `already_paid` exclusion; `txid_wins_at_most_once` | `formal_verification/coq/WindowExclusion.v` |
| 57-vector finite bridge | Shared set grown to **315** vectors (window, reorg-paid, duplicate txid, boundary weights, encoder, FIPS hash kernel) | `formal_verification/phase3_golden_vectors.json` |
| Differential testing not automated | Python oracle + hash-kernel properties + C++ generated cases + CI workflow | `formal_verification/scripts/run_differential_gate.sh`, `.github/workflows/formal-verification-gate.yml` |
| No canonical draw encoder | Offset/width/endianness theorems; C++ `static_assert`s already matched; Python `encode_sigma_preimage` | `CanonicalDrawEncoder.v`, `sigma_hash.{h,cpp}`, `rigor_consensus_spec.py` |
| No shared hash kernel | Still no shared C++/Coq object. Portable `cpp_hash_cli` compiles production `sha256.cpp`; extracted OCaml is the Coq kernel. Bit-exact agreement is **required** by `test_hash_kernel_tripartite.py` | `cpp_hash_cli.cpp`, `CanonicalHashKernel.v`, `test_hash_kernel_tripartite.py` |
| Phase5Equivalence only records impossibility | New `DrawBridgeProgress.v` records encoder + kernel + A-9-C as artifacts and **still** sets `equivalence_claimed = false`. Freeze-era impossibility catalogs are marked historical, not flipped | `Phase5Equivalence/DrawBridgeProgress.v` |
| No probabilistic fairness theorem | Idealized uniform-digest calculation only: `idealized_fairness_proportional` over `Q`. Not a theorem about SHA-256 | `Proofs/FairnessIdealized.v` |

## Still open (unchanged in kind)

1. **No refinement proof.** A signedness, endianness, or window off-by-one bug that exists only in C++ is outside every Coq theorem, including the new ones.
2. **No shared C++ hash object.** Extracted OCaml is the Coq kernel, not `CHash256`. Agreement with C++ remains differential.
3. **No grinding-cost or adversarial-advantage theorem** about real SHA-256.
4. **Fairness remains idealized.** `Pr[win] ∝ f` is proved only under a uniform digest definition, which SHA-256 is not shown to satisfy.
5. **Network / mempool / P2P** still out of scope.
6. **Phase-5 freeze catalogs** still report `DCS_Impossible` for *full* equivalence closure — that is still the correct statement about refinement. Do not read `DrawBridgeProgress` as closing PD-4F-CPP-01.

## How to read the new numbers

- **315** is the Python/C++ differential set.
- **108** of those are also `vm_compute` lemmas in `Kernel/GoldenHarness.v` (cheap families: Inequality, WindowExclusion, SigmaEncoder, plus short HashKernel). FIPS single-SHA vectors live in `CanonicalHashKernel.v` so the harness does not re-hash them. The rest are Python-only / C++-only so the Coq build does not hash hundreds of preimages.
- Growing a finite vector set does not become equivalence.

## Rebuild

Coq (operator, after `cd ~/rigor-official-tree/formal_verification/coq`):

```bash
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j$(nproc)
```

Python differential gate (no bitcoind build):

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

C++ (operator, when `test_bitcoin` exists):

```bash
TEST_BITCOIN=core/src/test/test_bitcoin bash formal_verification/scripts/run_differential_gate.sh
# or:
./src/test/test_bitcoin --run_test=selection_golden_tests
./src/test/test_bitcoin --run_test=convergence_consensus_tests
```

The Verdict's one-line summary still holds, with a stronger bridge:

> The model is strongly verified; the implementation correspondence is much tighter than the original 57 vectors, but it is **not** a refinement proof.

---

## Round 2 (same day) — hash kernel + gate

See [FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md). Short record:

| Item | What landed | Still not |
|---|---|---|
| FIPS lock | `CanonicalHashKernel.v`: empty/"abc"/55/56/64/NIST-56 plus SHA-256d composition; zero axioms | C++ equality |
| Extracted kernel | `extracted/hash256_kernel.ml` + `hash256_cli`; 33 HashKernel/FIPS vectors match hashlib | Shared C++ object; extraction TCB |
| Vectors | **309** shared / **106** Coq-enforced | Equivalence |
| Gate | Compare generators to working tree; first-hunk diffs; auto `TEST_BITCOIN`; optional extract | Refinement |

`DrawBridgeProgress.v` still sets `equivalence_claimed = false` and `hash_kernel_shared_binary_with_cpp = false`.

---

## Round 3 (same day) — portable C++ hash CLI

See [FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md](./FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md).

The gate now **requires** bit-exact C++ `CSHA256` (portable Transform) vs hashlib vs extracted Coq on the hash golden set. That is still not a shared binary and not a refinement proof. Vectors **315** / Coq-enforced **108**.

---

## Freeze

Further round notes are superseded by the stable status document:

**[FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)**

Honest line, `equivalence_claimed = false`, and `hash_kernel_shared_binary_with_cpp = false` are unchanged.
