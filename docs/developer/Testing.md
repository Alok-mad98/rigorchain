# Testing

**Status:** Active index.

```
equivalence_claimed = false
refinement_proved = false
```

A green test run is not a C++ refinement proof.

## Node / functional

```sh
cd core
make check
test/functional/test_runner.py
```

| Path | Role |
|------|------|
| `core/test/` | Unit tests |
| `core/test/functional/` | Functional tests |
| [`Build.md`](./Build.md) | How to compile first |

## Formal / differential

| Document | Role |
|----------|------|
| [`../security/HOW_TO_VERIFY_RIGOR.md`](../security/HOW_TO_VERIFY_RIGOR.md) | Reproduce published verification |
| [`../verification/FV-RELEASE-CHECKLIST.md`](../verification/FV-RELEASE-CHECKLIST.md) | Operator freeze checklist |
| [`../formal/VerificationStatus.md`](../formal/VerificationStatus.md) | What is and is not claimed |
| `formal_verification/scripts/run_differential_gate.sh` | Differential gate |

`LEVEL_5_CLAIMED`, `CPP_VERIFIED`, and `refinement_proved` are **false**.
