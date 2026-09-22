# Binary Mutation Report

Harness mutations B1–B3 against the generated executable artifact
`rigor-kernel-checker`.

These tests are **not** a proof of binary correctness.

They only prove:

> the binary test harness can detect obvious implementation corruption.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`

## Summary

| Metric | Value |
|---|---|
| Cases | B1, B2, B3 |
| `EXPECTED_FAILURE` | 3 |
| `mutation_passed` | 0 |
| Result | `BINARY_MUTATION_TEST_COMPLETE=TRUE` |

## Cases

| ID | Corruption | Why the harness must fail |
|---|---|---|
| B1 | Force the checker to print `VALID` for every vector | Negative golden vectors expect `INVALID` |
| B2 | `money_range_b` always returns `true` (overflow / range bound removed) | `utxo_over_max.json` (`MAX_MONEY+1`) expects `INVALID` |
| B3 | `exp_q_raw` uses `Z.opp te` (ASERT negative-time sign erasure) | `asert_negative_time.json` expects `q = -1` |

## Recorded run

```
EXPECTED_FAILURE id=B1 artifact=rigor-kernel-checker
EXPECTED_FAILURE id=B2 artifact=rigor-kernel-checker
EXPECTED_FAILURE id=B3 artifact=rigor-kernel-checker
EXPECTED_FAILURES=3
MUTATION_PASSES=0
BINARY_HARNESS_DETECTS_CORRUPTION=TRUE
```

A stuck-true or sign-flipped artifact is now visible to CI. That is
harness strength, not extraction soundness.
