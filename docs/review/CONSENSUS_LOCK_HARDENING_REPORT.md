# Consensus Lock Hardening Report

**Task:** TASK-CONSENSUS-LOCK-HARDENING-001  
**Date:** 2026-09-15

```
protocol_changes = NONE
consensus_changes = NONE
mathematical_identity_changes = NONE
rfc_unchanged = true
consensus_bytes_unchanged = true
```

The lock protects the existing consensus identity.  
It does not introduce a new consensus rule.  
It does not replace RFC governance.  
It only prevents accidental deviation.

---

## Files changed

### Added

| File | Role |
|------|------|
| `docs/review/CONSENSUS_GATE_AUDIT.md` | Pre-change audit of existing CI / tests |
| `docs/consensus/CONSENSUS_LOCK_POLICY.md` | What is locked, what may change, what needs RFC |
| `docs/review/CONSENSUS_LOCK_HARDENING_REPORT.md` | This report |
| `tools/check-convergence-identity.py` | Gate 1 |
| `tools/check-convergence-predicate-shape.py` | Gate 2 |
| `tools/check-publication-boundary.py` | Gate 3 |
| `tools/run-consensus-lock.sh` | Local runner for the three locks |
| `.github/workflows/consensus-lock.yml` | Three jobs, text-only, path-filtered |

### Documentation flags only (not consensus)

| File | Change |
|------|--------|
| `README.md` | Declare `cpp_formal_equivalence = false` and `documentation_consensus_authority = false` next to the existing flag |
| `docs/WHITEPAPER.md` | Declare `cpp_formal_equivalence = false` in the existing flag block |

### Not changed

- `docs/protocol/RFC-020.md`
- `docs/protocol/RFC-018.md`
- `core/src/consensus/res_issuance.h`
- `core/src/consensus/rigor_consensus_lock.h`
- `core/src/consensus/tx_verify.cpp`
- Transaction serialization
- Issuance parameters (`2^25`, `40`, `1024`, `RES_COIN`, ORU subsidy)

---

## Tests added

| Gate | Command | Result (local) |
|------|---------|----------------|
| `convergence_identity_test` | `python3 tools/check-convergence-identity.py` | `CONVERGENCE_IDENTITY_TEST=PASS` |
| `convergence_predicate_shape_check` | `python3 tools/check-convergence-predicate-shape.py` | `CONVERGENCE_PREDICATE_SHAPE_CHECK=PASS` |
| `publication_boundary_check` | `python3 tools/check-publication-boundary.py` | `PUBLICATION_BOUNDARY_CHECK=PASS` |

Identity vectors include:

- `h = 0`, `R = 1 RES`, `B = 1024 ORU` → pass
- `h = 0`, `B = required − 1 atom` → fail
- observational checkpoints `210000`, `420000`, pole `2^25`
- `h = INT32_MAX`
- a grid where conceptual uncleared cost and `B << 40` must agree

No floating point. No price table. No epoch table. No display `>> 50`.

Predicate shape scans only `CheckResMintArithmetic` and the `tx_verify.cpp` mint call site.

---

## CI structure after this task

```
Consensus Protection
│
├── convergence_identity_test
│
├── convergence_predicate_shape_check
│
└── publication_boundary_check
```

Existing workflows (`mainnet-freeze-gate`, `consensus-kernel-integrity`,
`formal-integrity-gate`, release jobs) are **unchanged**. They were not
merged into this lock and were not expanded.

Path filters were later split so docs/web edits do not run identity
vectors: see
[`CONSENSUS_LOCK_REVIEW_HARDENING_REPORT.md`](./CONSENSUS_LOCK_REVIEW_HARDENING_REPORT.md).
Gate 2 wording is the arithmetic shape of `CheckResMintArithmetic`; it
does not define the entire consensus execution path.

Not added: RFC checksum CI, markdown-wide build CI, formula-duplication CI,
Coq-on-every-push, compile-on-every-docs-edit.

---

## Protocol / consensus / identity

| Claim | Status |
|-------|--------|
| Protocol changes | NONE |
| Consensus changes | NONE |
| Mathematical identity changes | NONE |
| RFC unchanged | YES |
| Consensus bytes unchanged | YES |
| Mathematical identity protected | YES |

Reproduce:

```
bash tools/run-consensus-lock.sh
```
