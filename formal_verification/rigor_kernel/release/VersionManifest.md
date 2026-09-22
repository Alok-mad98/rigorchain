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
Build timestamp: 2026-09-22T13:26:33Z
Proof provenance: see release/SourceManifest.sha256
Git HEAD (host only; not a proof-file commit unless tracked): 38356df9362324fcdb4a3a253a1397cb12fe1a24
ExtractedKernel.ml sha256: 94782ad1f404d66adaaf405c39af6f52545415835f5d486f33d78d091c7aeadf
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
SourceManifest.sha256 sha256: 4348b84663da29c846da39ffb753c5ca8cbb8d148538180dac33a50c708e468a
Freeze timestamp: 2026-09-22T13:26:00Z
```

This hash is the lock of the lock file. Auditors run
`check_source_manifest.sh` (read-only). Re-freeze only when
publishing an intentional source change.

## Last freeze note

Free Path B `KernelInput` records banned in the gated cone (D-16 item 3);
examples go through `kernel_input_of`. `input_ok` and `kernel_input_of`
field equations are unchanged. The naive biconditional is not proved.
Unique-entry is not started. ND-01 / ND-02 remain closed (D-15 / D-16).
The KernelInputBridge construction is still not a production-authority
claim.
