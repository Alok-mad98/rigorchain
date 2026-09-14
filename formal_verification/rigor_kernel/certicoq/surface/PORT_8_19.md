# Coq 8.19 port notes (Phase 3)

Host change only. The compile surface Gallina was **not** rewritten
for 8.19.

## 8.19-only edits

None. Sidecar `coqc` 8.19.2 accepted every surface `.v` and
`Observe.v` without deprecation failures, `Require` path changes,
or universe fixes.

Isolation (engineering, not a rule change):

- 8.19 `.vo` land in `certicoq/generated/surface-8.19/` (gitignored).
- Cone 8.15 surface `.vo` stay in `certicoq/surface/`.
- Scripts `eval $(opam env)` only inside a subshell.

## Observational witnesses

`Observe.v` evaluates INVALID-001…005 (same records as
`Kernel/VerifiedCompilerBoundary.v`), a surface-only empty-script
negative (`invalid_script`), and the six positive smokes
from `kernel_binary/test_vectors/` by `vm_compute`.

Both hosts closed the same `Example` list:

| Host | Result |
|---|---|
| `/usr/bin/coqc` 8.15.0 | `OBSERVE_8_15=PASS` |
| sidecar `coqc` 8.19.2 | `OBSERVE_8_19=PASS` |
| Agreement | `OBSERVE_AGREE=PASS` (`host_vm_compute`) |

This is **not** a cross-version metatheorem. It is not CertiCoq,
not CompCert, and not a reason to flip
`COMPILER_CORRECTNESS` / `BINARY_SEMANTIC_EQUIVALENCE`.

ASERT stays boxed `Z`. No `ExtrOcamlZInt`. The 8.19 tree is not
merged over the cone.

Replay: `certicoq/scripts/observe_surface.sh`.
