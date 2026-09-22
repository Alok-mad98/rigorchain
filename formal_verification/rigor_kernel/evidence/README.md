# RigorKernel Formal Evidence

This package contains formal evidence for consensus kernel invariants.

It does not claim full implementation verification.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

This directory records:

- which Coq theorems were proved and under what assumptions
- how those theorems cover the P0-106 kernel IDs
- mutation tests that show an incorrect rule makes a proof fail
- how an auditor can reproduce the build and `Print Assumptions` check
- the claims boundary (what is proved, and what is not)
- theorem → Coq validator (proved); extraction/binary are test traces
  (`BinaryTrace.md`, `ExtractionBoundary.md`, `BoundaryCorrection.md`)
- refinement / TCB freeze at Coq validators (`RefinementBoundary.md`,
  `TCBInventory.md`, `KernelCone.md`, `ReproducibleBuild.md`,
  `BoundaryMutationReport.md`, `TheoremClassification.md`)
- external-audit release package (`../release/`)

It does **not** claim C++ implementation equivalence, full-chain /
full-node equivalence, cryptographic security, wallet security, or
network security.
