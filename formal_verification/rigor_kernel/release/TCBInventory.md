# TCB Inventory

Trusted Computing Base of the **checking** path for this Coq kernel
cone. Trusting a component is not a proof about it.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

Companion (older host-oriented table): `../evidence/TCBInventory.md`.

`verified_compiler_in_current_tcb = false`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`EXTRACTION_FORMAL_PROOF=source_identity_only`

`TCB_LAYER_COUNT=8`

## Layers (v4)

| Layer | Name | Role |
|---|---|---|
| 0 | Specification | Declared kernel invariants / non-claims |
| 1 | Abstract Coq Kernel | Unified `KernelState` / `kernel_step` |
| 2 | Concrete Coq Kernel | `ConcreteKernelState` / `concrete_kernel_step` (Concrete Kernel) |
| 3 | Executable Kernel Model | `ExecutableKernelState` / `executable_kernel_step` |
| 4 | Extraction Interface | `kernel_call_boundary`, proved a bijection onto the declared extracted entries (injective + surjective) |
| 5 | Runtime Semantic Model | `RuntimeKernelState` / `runtime_step` (Coq-model twin of layer 3) |
| 6 | Runtime Machine Domain | `MachineKernelState` over 63-bit checked words; `machine_decode` is PARTIAL and `machine_step_of` can fail on overflow |
| 7 | Checker Contract | `abstract_checker` decides validity from a machine state |

Layer 6 is the Runtime Checker Contract.

### Layer 0 Specification

- Trusted: human-written claims / non-claims documents
- Verified: machine-checked `= false` flags in `Audit/KernelNonClaims.v`
- Open: agreement with Bitcoin / production protocol text

### Layer 1 Abstract Coq Kernel

- Trusted: Coq 8.15.0 (`coqc` / `coqtop`)
- Verified: Category-A abstract invariants (UTXO / ORU / RES / script subset / abstract PoW)
- Open: Script VM, signatures, concrete hash

### Layer 2 Concrete Kernel (Concrete Coq Kernel)

- Trusted: same Coq kernel
- Verified: Coq abstract → Coq concrete (`concrete_step_refines_abstract_step`)
- Open: C++ / node implementation (OP-R2)

### Layer 3 Executable Kernel Model

- Trusted: same Coq kernel
- Verified: executable projection / step / Coq-model serialization
- Open: ocamlopt, ABI, CPU

### Layer 4 Extraction Interface

- Trusted: Coq `Extraction` mechanism (not proved correct)
- Verified: extraction interface consistency; source identity
- Open: OCaml compiler correctness

### Layer 5 Runtime Semantic Model

- Trusted: host that runs `coqc` (OS, filesystem)
- Verified: Coq runtime abstraction (`runtime_projection_preserves_kernel_domain`)
- Open: OCaml runtime, ABI, CPU, Native binary

### Layer 6 Checker Contract

- Trusted: abstract Coq interface (not the extracted binary)
- Verified: `checker_accepts_valid_kernel_state` / `checker_rejects_invalid_kernel_state`
- Open: native checker ≡ contract; ConnectBlock

## PROVED

- Coq executable model
- extraction interface consistency
- Coq runtime abstraction

## OPEN

- OCaml compiler correctness
- ABI
- CPU
- Native binary

## Trusted

| Component | Trust reason | Verification status |
|---|---|---|
| Coq kernel (`coqc` / `coqtop` 8.15.0) | Checks `Qed` and `Print Assumptions` | **Trusted checker.** Not proved here. A/B theorems Closed under the global context |
| Coq logic (CIC as implemented) | The metalogic of the cone | Trusted. Not a kernel-consensus theorem |
| Extraction mechanism | `Extraction` of `check_*` | Source **identity** and interface totality proved. Extraction *correctness* **not** proved |
| Runtime environment | Host that runs `coqc` / the extracted checker | **Unproved** (OS, OCaml runtime, filesystem) |

Also trusted for *evidence harnesses only* (not consensus rules):
`python3`, `bash`, `make`, `gcc` (C-fragment fallback).

## Open

| Item | Status |
|---|---|
| Compiler | `COMPILER_CORRECTNESS=false`. ocamlopt / gcc unproved |
| ABI | Not modelled |
| CPU semantics | Hardware unproved |
| C++ implementation | `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE` |

## Research sidecar residues (NOT checking TCB)

Phase 7 listed these. Phase 8 froze the wording. Phase 9
Aborted the Level 5 attempt (`PHASE9=ABORT`). Phase 10
exported a CPS `exp` without `pipeline.v` but still has
no mem Clight (`PHASE10=ABORT_LEVEL5`). They do
**not** enter the 8.15 auditor TCB.
`verified_compiler_in_current_tcb = false`.

| Residue | Why trusted |
|---|---|
| CertiCoq `print_Clight` | `Axiom` in `Compiler/pipeline.v` |
| CertiCoq Codegen (LambdaANF → Clight) | no `Theorem` in `Codegen/*.v` |
| LambdaANF pass theorems | exist; Phase 10 CPS `exp` of `check_oru` exists; `well_scoped` / `correct` not Closed |
| CompCert AST `semantic_preservation` | not applied to generated C |
| `gc_stack.o` / prims / FFI | clang TCB (I-8) |
| `vector_driver.c` / clang link / `libm` | trusted glue |

Detail: `certicoq/proofs/TCB_PASSES.md`.

## Explicitly not in this TCB

| Item | Status |
|---|---|
| CompCert `ccomp` / CertiCoq / CakeML | Sidecar research only; not in checking TCB |
| Production C++ `ConnectBlock` | Out of cone |
| Full node / P2P / wallet / database | Out of cone |
| SHA3 / keccak / Ed25519 implementations | Assumptions (`Kernel/CryptoAssumptions.v`) |

## C++ decode-bridge leaves (not checking TCB)

TASK 3.3A5 froze four *intended* byte-reader facts as named
assumptions of the C++ decode *story*. They are **not** Coq
`Axiom`s and they are **not** in this cone's checking TCB.

| Name | C++ site | Status |
|---|---|---|
| `Assumption_ser_readdata32_spec` | `serialize.h:100–104` | STATED; not assumed in Coq |
| `Assumption_ser_readdata32_int32_on_i32_nonneg` | `serialize.h:214` | STATED; not assumed in Coq |
| `Assumption_uint256_unserialize_take_n_32` | `uint256.h:84–87` | STATED; not assumed in Coq |
| `Assumption_ser_readdata64_spec` | `serialize.h:112–117` | STATED; not assumed in Coq |

Record: `audit/formal_bridge/DECODE_LEAF_TCB_v1.md`.
TASK 3.3A6: premises used for composition
(`DECODE_COMPOSITION_LIFT_v1.md`).
`DECODE_MAIN_THEOREM_UNDER_LEAF_TCB = STILL_PARTIAL`.
CompactSize / CScript / vector / `UnserializeTransaction`
are **not** TCB and are **not** in this cone.
TASK 3.3A7: `ser_readdata8` / `ser_readdata16` are CompactSize
sub-residuals, **not** new checking-TCB leaves.
TASK 3.3A8: `UnserializeTransaction` / CBlock `SERIALIZE_METHODS`
Coq composition is Closed; C++ inhabit stays **PARTIAL** and
is **not** TCB. Production-bool inhabit is **not** started.
TASK 3.3A9: production-bool / loose `operator>>` Coq glue is
Closed; C++ inhabit of `tos_block` / `osc_block` stays
**PARTIAL** and is **not** TCB. Main theorem **STILL_PARTIAL**.
TASK 3.3A10: decode boundary **FROZEN** at PARTIAL. Open
inhabit 1–9 stay **not** TCB. Next active is 3.3B UTXO
representation (not started; not TCB).
TASK 3.3B: UTXO snapshot / ORU-sum Coq structure is Closed;
C++ `CCoinsViewCache` inhabit stays **PARTIAL** and is **not**
TCB. `AccessCoin` / `UpdateCoins` are **not** checking-TCB leaves.

## Formally verified (not TCB — proved *using* the TCB)

Category-A kernel invariants listed in
`release/TheoremClassification.md`. Mutation evidence M1–M41.

`kernel_boundary_closed` is Category C documentation, not TCB shrink.
