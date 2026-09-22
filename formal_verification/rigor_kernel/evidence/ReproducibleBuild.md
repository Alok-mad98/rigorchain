# Reproducible Build Evidence

How an auditor rebuilds the same kernel cone, the same extracted
artifact, and the same golden-vector results.

This is **source-to-artifact reproducibility of the kernel cone**.
It is not a bit-identical binary guarantee across hosts, and it is
not a C++ node rebuild.

## Recorded environment (P0-111 host)

| Item | Value |
|---|---|
| Coq | 8.15.0 (compiled with OCaml 4.13.1) |
| OCaml (`ocamlc` / `ocamlopt`) | 4.13.1 |
| OS | Linux (WSL2) `6.6.87.2-microsoft-standard-WSL2` |
| Architecture | `x86_64` |

Re-record:

```
coqc --version
ocamlc -vnum
ocamlopt -vnum
uname -a
```

Required: Coq **8.15.0**. No MathComp, CompCert, or extra Coq plugins.
No sha3 / keccak / ed25519 import.

## Flow

```
git clone <this-repository>
cd formal_verification/rigor_kernel
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make
./extraction/build.sh
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh
```

Or the single gate:

```
./run_kernel_critical.sh
```

## Expected identity

| Same | Meaning |
|---|---|
| Same source | Kernel cone under `formal_verification/rigor_kernel/` |
| Same artifact | `ExtractedKernel.ml` extracted from the same validators; `rigor-kernel-checker` built from that extraction |
| Same vector result | Golden vectors (VALID and INVALID) match their declared `expect` |

This is not a binary semantic proof. `BINARY_SEMANTIC_EQUIVALENCE=false`.

Different host compilers may produce a different binary encoding.
The **observable kernel decision** on the golden vectors must match.
