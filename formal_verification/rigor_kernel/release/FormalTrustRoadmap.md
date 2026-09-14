# Formal Trust Roadmap

Formal refinement framework established.
This is a foundation for Level 4, not a completed Level 5.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

`FORMAL_KERNEL_PHASE_REFINEMENT_FOUNDATION=PASS`

## Levels

| Level | Meaning | Status |
|---|---|---|
| 0 | Code review | Done (outside this cone) |
| 1 | Invariant proofs | Done (Category A Closed) |
| 2 | Composition proofs | Done (block / economic / pipeline) |
| 3 | Kernel completeness | Done (P0-126~130; M1–M23) |
| 4 | Refinement | **Foundation** (unified kernel transition + relation skeleton) |
| 5 | Verified compiler | **OPEN** (see `CompilerBoundary.md`) |

**Current = Level 3+**
**Next = Level 4 foundation**

**ORU Domain = Independent Verified Domain**

ORU conservation and RES issuance are separately modeled and verified.

Level 4 here means a Coq-model `refines` interface and a
preservation theorem over **model** concrete steps. It does not
mean a C++ refinement, a binary proof, or CompCert.

Level 5 remains OPEN: verified compiler, memory model, integer
semantics, calling convention, ABI, machine semantics.

## Extraction layers

Only Layer 1 → Layer 2 (source identity). See
`release/ExtractionBoundary.md`.

`EXTRACTION_FORMAL_PROOF=source_identity_only`

## Mutations

M24 breaks `observe_ok` / `refines`.
M25 makes `machine_step` nondeterministic.

Suite: M1–M41. M26 fee-creation; M27 ORU-transfer inflate;
M28 economic-refine break. M29 two next-states; M30 value without
input/subsidy. M31 broken projection; M32 concrete bypass of the
abstract step. M33 broken execution projection; M34 serialization
corruption; M35 nondeterministic executable step. M36 force-accept
invalid vector; M37 changed extraction mapping. M38 broken
runtime projection; M39 checker accepts invalid; M40 nondeterministic
runtime step; M41 failure path removed.

Coq abstract → Coq concrete refinement is Closed. Implementation
refinement remains open (OP-R2).

## Non-claims

Not claimed: implementation correspondence, compiler correctness,
binary semantic equivalence, a full node.
