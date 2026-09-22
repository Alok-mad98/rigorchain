# Trusted Computing Base Inventory

Inventory of what the kernel cone relies on, and what is formally
verified versus merely trusted.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

## Trusted (used to check the Coq kernel)

These components check or define the Coq layer. Trusting them is not
a proof about them, but they sit on the *checking* side of the Coq
boundary.

| Component | Role |
|---|---|
| Coq kernel | Checks the kernel cone; `Print Assumptions` Closed |
| Coq logic | Calculus of inductive constructions as implemented by this Coq |
| Coq checker definitions | `coqc` / `coqtop` that admit `Qed` |

Recorded host: Coq 8.15.0.

## Untrusted (outside the proof boundary)

| Component | Role |
|---|---|
| OCaml extraction mechanism (faithful) | `extracted/Extract.v` → inductive `Z`/`nat` (no `ExtrOcamlZInt`) |
| OCaml extraction mechanism (legacy, not evidence) | `extraction/ExtractKernel.v` — P0-113 F-01; `ExtrOcamlZInt` / `ExtrOcamlNatInt`. Contrast only |
| OCaml compiler | `ocamlc` / `ocamlopt` compile extracted code + `main.ml` |
| OCaml runtime | RTS of `rigor-kernel-checker` |
| executable binary | generated test artifact; not a proved implementation |
| Host compiler toolchain | Assembler / linker used by `ocamlopt` |
| Operating system | Linux process that runs the binary |
| Filesystem / binary loader | Loads the artifact |
| Hardware / CPU | `x86_64` host that executes the process |
| `python3` / `bash` / `make` / `git` | Evidence and mutation harness |

## Formally verified

Inside the Coq kernel cone, with Closed theorems and Coq-level
mutation evidence:

| Item | Evidence |
|---|---|
| Consensus invariants | Class-A theorems in `TheoremClassification.md` |
| Kernel validators | `check_*` bool functions reflecting those theorems |
| Soundness bridge | `kernel_sound` |
| Mutation resistance | P0-109 ten expected Coq proof failures |

`kernel_boundary_closed` is a class-C documentation marker, not
invariant evidence.

## Trusted but not formally verified

| Item | Why it is trusted, not proved |
|---|---|
| Compiler correctness | Coq extraction and `ocamlopt` are not CompCert-proved here. Path documented in `release/VerifiedCompilerPath.md`; `verified_compiler_in_current_tcb = false` |
| Runtime correctness | OCaml RTS is outside the cone |
| Hardware correctness | CPU / memory model is assumed |
| OS correctness | Linux / WSL2 is assumed |
| JSON driver in `main.ml` | Parses vectors; not a consensus rule |
| Extraction integer mapping | Faithful path keeps Coq `Z`. Legacy `extraction/ExtractKernel.v` maps to native `int` (P0-113 F-01; not evidence) |

## Explicitly outside scope

The following are **not** in the TCB of this cone and are **not**
verified by it:

- bitcoin / RIGOR full node
- network stack (P2P, relay, eclipse)
- wallet
- RPC
- database / storage engine
- pool
- crypto primitive implementation (SHA3, Keccak, Ed25519)

Flags in `Audit/KernelNonClaims.v`: `tcb_defined = true`, and
`cpp_equivalence`, `full_chain_equivalence`, `node_correctness`,
`network_correctness`, `wallet_correctness`, `crypto_security`,
`extraction_equivalence`, `binary_semantic_equivalence`,
`kernel_execution_verified` remain `false`.
