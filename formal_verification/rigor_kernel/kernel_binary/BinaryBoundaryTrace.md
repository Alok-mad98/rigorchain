# Binary Boundary Trace

Trust-boundary v2 for the extracted kernel checker.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

## Chain

```
Abstract Kernel
        ↓
Concrete Kernel
        ↓
Executable Kernel Model
        ↓
Extracted Checker
```

`ExecutableKernelChecker` names the last Coq-model step before
the extracted test artifact (`rigor-kernel-checker`).

## PROVED

- Coq abstract → Coq concrete
- Coq concrete → executable model

See `concrete_step_refines_abstract_step` and
`executable_step_refines_concrete_step`.

## NOT CLAIMED

- Executable model → production C++
- Executable model → full node
- Compiler correctness

The extracted checker is a test artifact. It is not ConnectBlock,
not a node, and not a compiler proof.
