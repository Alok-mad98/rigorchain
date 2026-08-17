# Formal verification status (freeze)

**Date**: 2026-08-16  
**Role**: Current hand-off status for the post-verdict bridge.  
**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md) on *what the Coq model proves*. This file states the **frozen correspondence boundary** after the hash-kernel and differential work.

Dated round notes ([R2](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md), [R3](./FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md)) are historical snapshots.

---

## Honest status (frozen)

The model is strongly verified; the implementation correspondence is much tighter than the original 57 vectors and now includes automated tripartite hash checking, but it is **not** a refinement proof.

`equivalence_claimed` remains **false**.  
`hash_kernel_shared_binary_with_cpp` remains **false**.  
`refinement_proved` remains **false**.

Coq lock: `formal_verification/coq/DrawBridgeProgress.v` (re-export of
`formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v`).

---

## What has been proved in Coq (high-level)

All of the following are theorems about the **Coq model**, with a zero-axiom discipline on the protocol development (`Print Assumptions` → Closed under the global context). They do not by themselves prove that `core/src/**` matches the model.

| Area | Where | What it is |
|------|--------|------------|
| Executable SHA-256 / SHA-256d | `RigorSha256.v`, `RigorHash.v`, `CanonicalHashKernel.v` | Byte-level hash in Coq; FIPS short messages and padding edges; SHA-256d is `sha256 ∘ sha256` |
| Draw encoder layout | `CanonicalDrawEncoder.v` | 100-byte σ preimage; offsets 0/32/64/96; LE32 height |
| Selection determinism / kernel | `Proofs/Determinism.v`, `Kernel/*` | At most one winner for given inputs; kernel wrappers |
| A-9-C no-double-win (model) | `WindowExclusion.v` | `already_paid` exclusion; `txid_wins_at_most_once` on `paid_closed_trace` |
| Replay / supply (model) | `ReplayReorgCore/*`, `Phase4FConsensus/*` | Deterministic replay and supply lemmas *in the model* |
| Golden harness | `Kernel/GoldenHarness.v` | **108** vectors recomputed by `vm_compute` (build-breaking) |

Fairness remains **idealized** (`Proofs/FairnessIdealized.v`): not a theorem about real SHA-256.

The Verdict lists the full theorem inventory. Do not read Phase-5 freeze catalogs (`DCS_Impossible`) as having been flipped; they still correctly say *full* Coq⇄C++ refinement is not closed.

---

## Golden vectors and differential status

| Layer | Count / check | Role |
|-------|----------------|------|
| Shared JSON | **315** vectors (`phase3_golden_vectors.json`, tag `hash-kernel-cpp-cli-2026-08-16`) | Python + generated C++ cases |
| Coq-enforced | **108** `vm_compute` lemmas | Cheap families: Inequality, WindowExclusion, SigmaEncoder, short HashKernel |
| FIPS SHA-256 | `CanonicalHashKernel.v` + `Sha256Fips` JSON | Padding edges including 55/56/63/64/65 |
| Python oracle | `tests/test_phase3_oracle.py` | Full 315 |
| Artifact freshness | gate step 2 | Generators vs **working tree** (not `git diff` HEAD) |

Intentional splits (Coq-enforced vs full set, hex-filter vs `FinalizeWindowedDrawSet`) are recorded in [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md). Growing a finite set is not equivalence.

---

## Tripartite hash check (scope)

**Entry**: `bash formal_verification/scripts/run_differential_gate.sh` (step 3).

Three implementations are compared bit-for-bit on HashKernel / Sha256Fips golden vectors plus a random sample:

| Side | Artifact | What it is |
|------|----------|------------|
| Coq | `extracted/hash256_cli` (from `RigorSha256`) | Extracted Coq kernel (OCaml). TCB: extractor, `ocamlopt`, `nat` as `int`. |
| C++ portable | `extracted/cpp_hash_cli` | Production `core/src/crypto/sha256.cpp`, **C++ Transform only** (no SSE/SHANI). SHA-256d = SHA256(SHA256(x)), same composition as `CHash256`. |
| Python | `hashlib` | Independent OpenSSL-backed hash. |

A split among these layers **fails the gate**. That is automated divergence detection, not a shared C++/Coq object and not a proof of equality on all inputs.

**SIMD** (SSE4 / SHANI / AVX2) is **out of this check**. Those paths must still match the C++ Transform via Bitcoin Core `SHA256AutoDetect` / `SelfTest`.

### Portable CLI vs `test_bitcoin`

| Check | Covers | Does not cover |
|-------|--------|----------------|
| Gate step 3 (`cpp_hash_cli`) | `CSHA256` portable Transform on FIPS + HashKernel + random sample | BOOST tests, encoder/`ComputeSigmaHash` in the node, SIMD |
| `test_bitcoin --run_test=selection_golden_tests` | BOOST cases compiled into that binary: encoder layout, A-9-C hex filter, σ, SHA-256d, and **`sha256_fips_golden` if the binary is rebuilt after that test was added** | Anything not linked into that binary |
| `test_bitcoin --run_test=convergence_consensus_tests` | Window builder / `already_paid` at the consensus-logic level | Hash FIPS |

If the gate prints **`test_bitcoin` STALE (no `sha256_fips_golden`)**:

- Hash FIPS bytes are **already** checked by the portable CLI. The gate may still PASS.
- BOOST `sha256_fips_golden` is **not** in that binary. Rebuild `test_bitcoin` (operator full `make` in `core/`) to cover it.

```bash
cd ~/rigor-official-tree/core
# operator: make -j$(nproc)
./src/test/test_bitcoin --run_test=selection_golden_tests,convergence_consensus_tests
```

Skip the optional BOOST suites: `SKIP_TEST_BITCOIN=1 bash formal_verification/scripts/run_differential_gate.sh`.

---

## Remaining gaps (explicit)

1. **No refinement proof** — no mechanized `Coq function = C++ function`.
2. **No shared C++ hash binary** — `cpp_hash_cli` and `hash256_cli` are test drivers.
3. **No grinding-cost / adversarial-advantage theorem** about real SHA-256.
4. **No SHA-256 uniformity** — fairness stays idealized.
5. **Network / mempool / P2P** are not in the model.
6. **Phase-5 catalogs** still report full equivalence closure as impossible.

---

## Operator reproducibility

From `~/rigor-official-tree`:

**1. Coq targets** (Coq 8.15.0):

```bash
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j$(nproc)
```

Minimum bridge lock: `CanonicalHashKernel.v`, `Kernel/GoldenHarness.v`, `Phase5Equivalence/DrawBridgeProgress.v`.

**2. Differential + tripartite gate** (needs Python 3, `g++`; OCaml optional but used if `ocamlopt` is present):

```bash
bash formal_verification/scripts/run_differential_gate.sh
# optional: COQ=1  EXTRACT=1  SKIP_TEST_BITCOIN=1
```

Help: `bash formal_verification/scripts/run_differential_gate.sh --help`

**3. Boundary** — this file. Do not infer equivalence from a green gate.

**4. Key-file check** (safe from any cwd; do not `cd` into `coq/` and then `ls formal_verification/coq/...`):

```bash
bash formal_verification/scripts/check_freeze_artifacts.sh
```

---

## Document map

| Document | Role |
|----------|------|
| **This file** | Frozen correspondence status and operator entry |
| [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md) | What the Coq model proves / does not prove |
| [FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md](./FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md) | Post-verdict gap closures (historical + freeze pointer) |
| [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md) | Intentional splits, not bugs |
| `formal_verification/README.md` | Layer map and theorem pointers in the corpus |
| R2 / R3 dated notes | Historical round logs |
