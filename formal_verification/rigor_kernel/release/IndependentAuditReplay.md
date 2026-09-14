# Independent Audit Replay

One-shot reproduction of **claimed** kernel results, plus the
historical P0-113 finding replay.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

No stronger claim is made.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

## One-click command

From this cone, on a host that already has the toolchain below:

```
cd formal_verification/rigor_kernel
./release/independent_audit_replay.sh
# optional: --out-dir /tmp/my-audit
```

The script **does not write the audited tree**. It copies the cone to
`--out-dir` (default `/tmp/rigor-kernel-audit-replay`) and writes
reports there. `check_source_manifest.sh` runs read-only on the
original tree.

Success ends with `INDEPENDENT_AUDIT_REPLAY=PASS`.

The script does **not** run the forty-one Coq mutation cases. Full CI
including mutations is `./run_kernel_critical.sh`. Claim / TCB / count
attack surface: `./release/final_adversarial_audit.sh`.

## Toolchain (minimal)

| Dependency | Recorded / required | Role |
|---|---|---|
| Coq (`coqc`, `coqtop`, `coq_makefile`) | **8.15.0** | Compile cone; `Print Assumptions` |
| OCaml (`ocamlopt`) | **4.13.x** (recorded 4.13.1) | Faithful extract binary |
| Python 3 | 3.x | Diff oracle; SourceManifest; classification grep |
| GNU make | any recent | `coq_makefile` build |
| gcc | C99 host compiler | C-fragment **fallback** |
| bash | this script | Driver |

Optional: CompCert `ccomp`. **Not required.**
Optional: `rsync` (falls back to `cp -a`).

Not required: C++ node build, database, wallet, network, keccak
library, CompCert license.

## What the script reproduces

1. Coq compile of `_CoqProject.kernel-critical` (in `--out-dir/work`)
2. `scripts/check_theorem_classification.py` then `Print Assumptions` Closed on every Category A/B theorem
3. `scripts/extract_and_test.sh` — extract, 6 VALID + INVALID-001…005, then differential verify
4. Differential verify (count regenerated; `MATCH` = implementation agreement only)
5. `scripts/verified_compiler_path.sh` — hand-written C99 fragment rehearsal
6. `release/check_source_manifest.sh` (read-only, original tree)

Publisher freeze (not part of replay): `release/freeze_source_manifest.sh`.

## Expected result without CompCert

This is the **documented** result on the reference host:

```
ccomp: ABSENT
VERIFIED_COMPILER=ABSENT
C_FRAGMENT_REHEARSAL=PASS
COMPILER=gcc
COMPILER_CORRECTNESS=false
BINARY_SEMANTIC_EQUIVALENCE=false
EXTRACT_AND_TEST=PASS
DIFF_VERIFY=PASS
SOURCE_MANIFEST_COMPLETE=TRUE
REPRODUCIBLE_SOURCE_TREE=TRUE
ALL_ASSUMPTIONS=CLOSED
INDEPENDENT_AUDIT_REPLAY=PASS
```

If `ccomp` is present, the rehearsal uses it and prints
`VERIFIED_COMPILER=PRESENT`. That still does **not** claim a Coq-extract
refinement or binary semantic equivalence.

`MATCH` on differential vectors means the extracted checker and the
shadow **agreed**. It is **not** an independent correctness assertion
and **not** a C++ / ConnectBlock refinement.

## Consensus / runtime

No consensus code changed.

- `src/` — not modified
- `node/` — not modified
- `wallet/` — not modified
- `pool/` — not modified
- `protocol/` — not modified
- `database/` — not modified

`CONSENSUS_CODE_CHANGED=FALSE`

## Historical P0-113 findings

### F-01

Fixed by extraction boundary correction.

Proof coverage is Spec → Coq definitions → Coq proofs → Coq
validators. Extraction, OCaml runtime, compiler, and binary are
outside the boundary. Legacy `extraction/ExtractKernel.v` still
imports `ExtrOcamlZInt` and is **not evidence**.

### F-02

Fixed by a frozen `release/SourceManifest.sha256` and a **read-only**
bidirectional `check_source_manifest.sh`. Re-freeze is publisher-only.

### F-03

Fixed by negative vectors INVALID-001 … INVALID-005.

### F-04

Fixed by theorem classification (Category A / B / C).

### F-05

Fixed by PoW / ASERT scope documents.

### F-06

Fixed by classifying `kernel_boundary_closed` as Category C.

## Later closure notes

P0-117: differential verification + `Kernel/CryptoAssumptions.v`.
P0-118: verified-compiler trust root + `OpenProperties.md`.
P0-119: this one-click replay + expanded differential coverage.
Post-review: read-only source lock, A/B-driven Print Assumptions,
honest shadow columns, C-fragment token rename.

`KERNEL_COMPLETENESS_MATRIX=100%`
