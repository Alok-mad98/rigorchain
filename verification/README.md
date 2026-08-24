# RIGOR Public Verification Architecture

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Allowed claim:** Formally verified protocol invariants with explicit verification boundaries.

This directory is the **public entry point** for RIGOR’s verification artifacts. Canonical sources live under [`formal_verification/`](../formal_verification/) (Python SSOT, Coq corpus, golden vectors, differential gate). Subfolders here map that corpus into a layout external reviewers can navigate without hunting the tree.

Operator status (living lock **422 / 215**): [`docs/verification/FORMAL-VERIFICATION-STATUS.md`](../docs/verification/FORMAL-VERIFICATION-STATUS.md).  
Coverage audit: [`docs/security/FORMAL_VERIFICATION_COVERAGE_AUDIT.md`](../docs/security/FORMAL_VERIFICATION_COVERAGE_AUDIT.md).  
How to reproduce: [`docs/security/HOW_TO_VERIFY_RIGOR.md`](../docs/security/HOW_TO_VERIFY_RIGOR.md).

---

## What RIGOR publishes

1. **Formal specifications** — RFCs + executable Python SSOT (`formal_verification/spec/`).
2. **Coq verification models** — machine-checked properties of the *model* (`formal_verification/coq/`).
3. **Executable golden vectors** — shared JSON lock (`phase3_golden_vectors.json`, `golden_lock.json`).
4. **Cross-implementation differential checks** — Python ↔ Coq harness ↔ portable / production CLIs (`scripts/run_differential_gate.sh`).
5. **Verification boundaries** — explicit non-claims (`equivalence_claimed = false`).

RIGOR does **not** ask reviewers to trust shipping C++ alone. Artifacts are published so independent parties can re-check the verified cone.

---

## Directory map

```
verification/
├── README.md                 ← this file
├── coq/
│   ├── protocol-core/        ← UTXO / dual-pool / replay / script shape
│   ├── monetary-policy/      ← RES schedule, cap, H-1; ORU subsidy bounds
│   └── convergence/          ← σ, C, DCR, selection, A-9-C window
├── python-spec/              ← executable specification
├── golden-vectors/           ← shared vector corpus + lock
└── differential-tests/       ← gate, CLIs, parity tests
```

Each subdirectory README lists **canonical paths** into `formal_verification/` (no duplicate binaries of the Coq tree).

---

## Verified (model and/or differential gate)

| Area | What is established |
|------|---------------------|
| RES monetary schedule | Deterministic `W(k)`; pins; nonincreasing; cost-floor along schedule |
| RES supply cap | Finite-sum issuance ≤ 21M RES; overshoot reject |
| Convergence consensus model | σ layout; order-independent `C`; selection; window exclusion; DCR pipeline |
| Asset separation invariants | Dual-pool RES conservation; mint = schedule term only (model) |
| Script shape restrictions | Allowed created templates / spend-CVG2 / PCORB sizes on `apply_tx` / accepted blocks |

## Not claimed

| Non-claim |
|-----------|
| Complete C++ ↔ Coq ↔ Python equivalence (`equivalence_claimed = false`) |
| Cryptographic primitive security beyond the executable hash model (no “SHA-256 is secure” theorem) |
| Hardware / side-channel / TEE security |
| Network availability, P2P honesty, mempool DoS resistance |
| Mining template correctness, difficulty / PoW hardness, wallet security |
| “Fully proven blockchain”, “bug free”, or “zero risk” |

---

## Reproduce (summary)

```bash
# From repository root
bash formal_verification/scripts/run_differential_gate.sh   # expect PASS (finite lock)
# Optional Coq compile of protocol-core / harness:
cd formal_verification/coq && coq_makefile -f _CoqProject -o Makefile.coq && make -f Makefile.coq -j"$(nproc)"
```

Full prerequisites and expected outputs: [`docs/security/HOW_TO_VERIFY_RIGOR.md`](../docs/security/HOW_TO_VERIFY_RIGOR.md).

---

## Public surfaces

| Surface | Path |
|---------|------|
| This architecture | `verification/README.md` |
| Website | `/documentation/formal-verification` |
| Whitepaper Ch.8 | `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` |
| Claim table | `docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md` |
| Assumption boundary | `docs/security/FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md` |
