# Independent Audit Replay Report

This file is a **placeholder** in the cone. The live report is written
to `--out-dir` (default `/tmp/rigor-kernel-audit-replay`) by
`release/independent_audit_replay.sh` and is **not** part of the
source lock.

Run:

```
./release/independent_audit_replay.sh --out-dir /tmp/rigor-kernel-audit-replay
```

Expected tokens (not a C++ refinement, not binary equivalence):

```
ALL_ASSUMPTIONS=CLOSED
EXTRACT_AND_TEST=PASS
DIFF_VERIFY=PASS
C_FRAGMENT_REHEARSAL=PASS
SOURCE_MANIFEST_COMPLETE=TRUE
REPRODUCIBLE_SOURCE_TREE=TRUE
COMPILER_CORRECTNESS=false
BINARY_SEMANTIC_EQUIVALENCE=false
EXTRACTION_FORMAL_PROOF=source_identity_only
INDEPENDENT_AUDIT_REPLAY=PASS
```
