# Golden vectors

**Canonical corpus:**

| Artifact | Path |
|----------|------|
| Shared vectors (living **422**) | `formal_verification/phase3_golden_vectors.json` |
| Count / family lock | `formal_verification/golden_lock.json` |
| Lock checker | `formal_verification/scripts/check_golden_lock.py` |
| Coq-enforced harness (**215**) | `formal_verification/coq/Kernel/*Harness.v` |

Coverage by family: [`docs/security/FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md`](../../docs/security/FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md).  
Named L1 vs L2 surface splits: [`docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md`](../../docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md).

Vectors are **finite** agreement checks — not a universally quantified equivalence proof.
