# STEP-0201 Audit Record

## Run Archival Policy

Each `STEP-0201` run **must** archive the following fields to ensure reproducible audit:

| Field | Archive Target | Purpose |
|-------|----------------|---------|
| Commit | Current `git HEAD` hash | Code version |
| Build identity | OS + compiler + `depends` package set | Environmental determinism |
| Reproducible metadata | `SOURCE_DATE_EPOCH`, `HOST`, `BUILD` | Guix / deterministic build trace |
| PRNG seed policy | `feature_deterministic_replay.py --prng-seed` | Replay initialization |
| Run label | Timestamp or CI build number | Unique identification |

## Archiving Method

1. After each `STEP-0201` run, append one line to `STEP-0201-run-archive.tsv` in this directory:

```
<timestamp>\t<commit-hash>\t<os-compiler>\t<prng-seed>\t<pass/fail>\t<artifact-path>
```

2. The `artifact-path` points to the compressed archive of `test_framework.log` and `replay_consistency.log` for this run.

## Relationship to Formal Verification

- `formal_verification/coq/` consumes `STEP-0201` outputs to verify `state_hash` and `convergenceRoot` invariants.
- `reference/rigor_core.py` is the Python reference implementation; if `STEP-0201` results diverge from the Python reference, it is treated as a **consensus bug**.

## Ownership

- **Maintainer:** Release Engineering Lead
- **Reviewer:** Formal Verification Team (mandatory before each freeze phase)

*Last updated: 2026-04-24*
