# Differential tests

**Primary entry:**

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

| Tool | Path |
|------|------|
| Differential gate | `formal_verification/scripts/run_differential_gate.sh` |
| Golden lock | `formal_verification/scripts/check_golden_lock.py` |
| Portable hash CLI build | `formal_verification/scripts/build_cpp_hash_cli.sh` |
| σ / root CLI build | `formal_verification/scripts/build_cpp_sigma_root_cli.sh` |
| RFC-017 schedule parity | `formal_verification/tests/test_rfc017_schedule_parity.py` |
| RFC-018 fee-burn parity | `formal_verification/tests/test_rfc018_fee_burn_parity.py` |
| Release checklist wrapper | `formal_verification/scripts/run_fv_release_checklist.sh` |

Expected gate result on a clean tree with the living lock: **PASS** (finite 422/215 correspondence; honesty flags remain false).

Guide: [`docs/security/HOW_TO_VERIFY_RIGOR.md`](../../docs/security/HOW_TO_VERIFY_RIGOR.md).
