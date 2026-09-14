# Version Manifest

Static fields describe the Coq consensus kernel cone.
**Do not treat the host Git HEAD as proof provenance** unless the
cone `.v` files are git-tracked.

Proof-file lock: `SourceManifest.sha256` is **frozen** by
`freeze_source_manifest.sh` and **verified read-only** by
`check_source_manifest.sh` (bidirectional: listed hashes match, and
no extra lock-eligible file is absent from the list).

## Kernel

| Field | Value |
|---|---|
| Kernel version | RigorKernel critical-consensus cone P0-116–P0-120 |
| Cone path | `formal_verification/rigor_kernel/` |
| Public claim | Formally verified critical consensus invariants inside a Coq consensus kernel model, with explicit extraction and runtime trust boundaries |
| Formal boundary | Coq kernel only (`FORMAL_BOUNDARY_LAYER=COQ_KERNEL_ONLY`) |

## Toolchain (recorded host)

| Field | Value |
|---|---|
| Coq version | 8.15.0 |
| OCaml version | 4.13.1 |
| Architecture | x86_64 |

## Research sidecar (not the cone toolchain)

Decision: `L-RESEARCH-SIDECAR-2026-09-12`.
Host `PATH` stays `/usr/bin/coqc` 8.15.0; host `ccomp` ABSENT.
Do not export the sidecar onto the auditor PATH.

| Field | Value |
|---|---|
| Sidecar Coq | 8.19.2 (`OPAMROOT=certicoq/.opam-root`, switch `rigor-certicoq-8.19`) |
| Sidecar CertiCoq | 0.9+8.19 |
| Sidecar CompCert / `ccomp` | 3.13 (INRIA Non-Commercial; not vendored) |
| Cone Coq (unchanged) | 8.15.0 |

## Git source tree reproducibility

An auditor reproduces the cone from this worktree (or from a commit
that actually tracks every path in `SourceManifest.sha256`):

```
# obtain this tree
cd formal_verification/rigor_kernel
./release/check_source_manifest.sh
./run_kernel_critical.sh
```

| Check | Meaning |
|---|---|
| `SOURCE_MANIFEST_COMPLETE=TRUE` | Required `.v` files exist; frozen list matches the worktree |
| `REPRODUCIBLE_SOURCE_TREE=TRUE` | Listed hashes match **and** no unlisted lock-eligible file exists |

Git HEAD is recorded below as a **host stamp only**. It is not a
proof-file commit unless `git ls-files formal_verification/rigor_kernel`
contains the cone `.v` files. Do not cite a commit that does not.

## Dynamic (filled by check_release.sh)

```
Build timestamp: 2026-09-14T03:47:55Z
Proof provenance: see release/SourceManifest.sha256
Git HEAD (host only; not a proof-file commit unless tracked): 6886171f1537f6101decb2b1b304de71fc8cfd7a
ExtractedKernel.ml sha256: f2349792f91eff91aad51b9baf58d16e265b7c723804702792e74f8bbb42feb1
```

The native binary hash is **not** a cross-host identity. Auditors
should rebuild and compare **vector results**, not `ocamlopt` bytes.

Extraction and the binary are generated test artifacts.
`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`COMPILER_CORRECTNESS=false`

## Source lock (filled by freeze_source_manifest.sh)

```
SourceManifest.sha256 sha256: 210409dfb109362b38d1497698d0d2e9702ba6d0bfc081ac18353753c790fbbe
Freeze timestamp: 2026-09-14T03:47:09Z
```

This hash is the lock of the lock file. Auditors run
`check_source_manifest.sh` (read-only). Re-freeze only when
publishing an intentional source change.
