# How to Verify RIGOR

**Task companion:** `TASK-RIGOR-FORMAL-VERIFICATION-PUBLIC-RELEASE-ARCHITECTURE-P0-001`  
**Public map:** [`verification/README.md`](../../verification/README.md)  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This guide reproduces RIGOR’s published verification artifacts. A green run means **finite agreement on the locked vector set and/or successful Coq typecheck of the model** — not “the blockchain is bug-free” or complete C++ equivalence.

---

## Prerequisites

| Tool | Typical version / notes |
|------|-------------------------|
| Git checkout of this repository | Working tree at the commit under review |
| **Python 3** | 3.9+ recommended |
| **Coq** | 8.15.x (`coqc`, `coq_makefile`, optional `coqchk`) |
| **C++ build** (for full differential gate) | Same toolchain used to build `core/` portable CLIs; gate can skip `test_rigor` with `SKIP_TEST_RIGOR=1` |
| Optional: OCaml | Only if re-extracting the hash kernel (`EXTRACT=1`) |

---

## 1. Differential gate (recommended first check)

From the **repository root**:

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

Optional:

```bash
SKIP_TEST_BITCOIN=1 bash formal_verification/scripts/run_differential_gate.sh
COQ=1 bash formal_verification/scripts/run_differential_gate.sh   # also compile golden harnesses
```

**Expected result:** script exits **0** / reports **PASS** on the living lock (**433** shared vectors / **226** Coq-enforced lemmas pinned by `formal_verification/golden_lock.json`).

What PASS means: Python oracle, golden lock, portable hash CLI, and σ/root CLI agree on the gated set.  
What PASS does **not** mean: `equivalence_claimed = true`.

---

## 2. Golden vector / lock check only

```bash
cd formal_verification
python3 scripts/check_golden_lock.py
```

**Expected:** exit 0; counts match `golden_lock.json`.

---

## 3. RFC-017 schedule parity (C++ formula mirror ≡ Python ≡ Coq pins)

```bash
cd formal_verification
python3 tests/test_rfc017_schedule_parity.py
```

**Expected:** exit 0; no mismatches for `k = 0..251`.

---

## 3b. RFC-018 fee-burn parity (C++ share/display ≡ Python ≡ Coq pins)

```bash
cd formal_verification
python3 tests/test_rfc018_fee_burn_parity.py
```

**Expected:** exit 0; burn-share pins, six-decimal RES display (`1.000000` / `0.100000`), circulating ΔORU = subsidy − burn, RES unchanged.

---

## 3c. Supply-layer semantics (circulating vs ledger vs CVG2)

```bash
cd formal_verification
python3 tests/test_supply_layer_semantics.py
```

**Expected:** exit 0; fee burn and CVG2 contribute 0 to circulating; RES display six decimals; wallet model excludes unspendable coins. `equivalence_claimed = false`.

---

## 4. Coq verification (`coqc`)

```bash
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j"$(nproc)"
```

**Expected:** exit 0; all modules in `_CoqProject` compile.

Protocol-core only (citable theorems; excludes Phase-4F/4G/5 catalogs):

```bash
bash formal_verification/scripts/run_protocol_core_coq.sh
```

This uses `_CoqProject.protocol-core` and then `Print Assumptions --strict` on
`ResSchedule.v`, `OrderIndependence.v`, `ResMintObligation.v`,
`DualPoolConservation.v`, `SupplyConservation.v`, and `OpaqueBoundary.v`.
Requires **Coq 8.15.x**. Do not `apt-install` Ubuntu `coq`.

Optional kernel check (protocol-core; harnesses are large):

```bash
# After a successful make; example pattern used in coverage audit:
coqchk -R . Rigor -o ResSchedule.vo Selection.vo OrderIndependence.vo \
  ReplayReorgCore/DualPoolConservation.vo ReplayReorgCore/ResMintObligation.vo
```

**Expected:** `Modules were successfully checked` and context summary **Axioms: \<none\>** for the checked set.

---

## 5. Release checklist wrapper

```bash
bash formal_verification/scripts/run_fv_release_checklist.sh
```

See [`docs/verification/FV-RELEASE-CHECKLIST.md`](../verification/FV-RELEASE-CHECKLIST.md).

---

## 6. Security unit gates (not the FV gate)

PoW / ASERT / `CheckProofOfWork` are **NOT VERIFIED** in Coq. There is no PoW
module. The following BOOST suites are **security / unit** gates (see
[`PRE-MAINNET-CHECKLIST.md`](./PRE-MAINNET-CHECKLIST.md) §D). They are skipped
when `SKIP_TEST_BITCOIN=1` (the default FV checklist).

From `core/` after `make`:

```bash
./src/test/test_rigor --run_test=pow_tests
./src/test/test_rigor --run_test=sighash_tests
./src/test/test_rigor --run_test=script_asset_ownership_freeze_tests
./src/test/test_rigor --run_test=res_proof_math_tests
./src/test/test_rigor --run_test=convergence_root_tests
```

---

## Interpreting results

| Outcome | Interpretation |
|---------|----------------|
| Gate PASS + Coq build OK | Published verification artifacts reproduce |
| Gate FAIL | Do not ignore — lock or oracle drift; open an issue / do not claim green FV |
| Coq FAIL | Model or environment broken; not a production-node runtime test |
| `pow_tests` / sighash / mint-auth FAIL | Security unit failure. **Not** covered by a green FV gate. |

Public wording after a green run:

> Formally verified protocol invariants within an explicitly defined verification boundary.

Forbidden after a green run: “mathematically proven complete blockchain”, “bug free”, “zero risk”.
