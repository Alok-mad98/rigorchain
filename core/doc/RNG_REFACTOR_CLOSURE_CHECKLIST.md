# RNG Refactor Closure Checklist

## Engineering Closure Checklist for RES Refactoring

### CI Alignment

| Check | Status | Notes |
|-------|--------|-------|
| Unit tests pass | [ ] | `make check` |
| Functional tests pass | [ ] | `test_runner.py` baseline |
| Deterministic replay passes | [ ] | `feature_deterministic_replay.py` |
| Reorg determinism passes | [ ] | `feature_reorg_determinism_audit.py` |

### Unit Tests

- [ ] `uint256` arithmetic (cross multiplication)
- [ ] `arith_uint512` wide multiplication (no truncation)
- [ ] `ComputeConvergenceRoot` byte-identical across platforms
- [ ] `ComputeSigmaHash` input == 100 bytes exactly
- [ ] `IsEligible` inequality evaluation
- [ ] `IsBetter` cross-product comparison

### Functional Tests

- [ ] `feature_res_core_convergence_draw.py` — draw winner selection
- [ ] `feature_rigor_convergence_rng_consensus.py` — RNG consensus
- [ ] `feature_fault_injection.py` — fault tolerance

### Documentation

- [ ] `RFC-000` updated if any semantic change (must not happen without H-phase review)
- [ ] `RFC-ARCH-000` updated if any engineering constraint change
- [ ] This checklist signed off by 2 reviewers

## Sign-Off

| Role | Name | Date |
|------|------|------|
| Author | | |
| Reviewer 1 | | |
| Reviewer 2 | | |

*No merge without all boxes checked.*
