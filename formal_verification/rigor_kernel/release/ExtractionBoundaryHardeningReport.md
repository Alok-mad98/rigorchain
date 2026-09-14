# Extraction Boundary Hardening Report

Level 4.8 extraction-boundary hardened kernel. Coq model only.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

## 1. Current maturity

Level 4.8

`FORMAL_LEVEL_ESTIMATE=Level 4.8`

## 2. Verified chain

```
Specification
        ↓
Abstract Kernel
        ↓
Concrete Kernel
        ↓
Executable Kernel Model
        ↓
Extraction Interface
```

Verified:

- Coq executable model
- extraction interface consistency (`extraction_interface_bijective`)
- operation-trace preservation at the interface
- kernel-domain integer range vs documented runtime bound
- V1–V6 negative golden vectors rejected

## 3. Open

- Compiler
- ABI
- CPU
- Native Binary

Not claimed: compiler correctness, binary semantic equivalence,
C++ refinement, full-node verification, crypto security proofs.
