# RFC-013-B: Formal Specification to Verified Implementation Bridge

**Title:** Formal Specification to Verified Implementation Bridge
**Status:** CONSENSUS-CRITICAL EXTENSION

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**

---

## Goal

Ensure the written code is not "implementing the specification"
but "automatically derived from the specification"

---

## I. Core Idea (Must Understand)

The current system has three layers:
```
Layer 1: Mathematics (RFC)
Layer 2: Code (C++)
Layer 3: Runtime (Node)
```

**Problem:**
Layer 1 ≠ Layer 2 (implementation may be incorrect)

**Ultimate Solution:**
Introduce Layer 0: Formal Spec (Coq)

Result:
```
Coq (Single Source of Truth)
   ↓ Automatic Generation
C++ (Execution Body)
```

---

## II. Structure

### New Directory
```
/rigor
 ├── formal/
 │    ├── types.v
 │    ├── selection.v
 │    ├── converge.v
 │    ├── proof.v
 │
 ├── generated/
 │    ├── types.hpp
 │    ├── selection.hpp
 │
 ├── consensus/
 │    ├── (actual implementation)
```

---

## III. Core Objects (Coq Definitions)

### 1. ConvergenceRoot
```
Record ConvergenceRoot := {
  root_bytes : bytes32
}.
```

### 2. Sigma
```
Record Sigma := {
  sigma_bytes : bytes32
}.
```

### 3. Definition of H_i (Core Formula)
```
Definition compute_sigma
  (txid : bytes32)
  (C : ConvergenceRoot)
  (entropy : bytes32)
  (h : uint32)
  : Sigma := 
  hash256 (txid ++ C.root_bytes ++ entropy ++ serialize32 h).
```

### 4. Core Inequality
```
Definition eligible
  (H : Sigma)
  (T : uint256)
  (f : uint64)
  : Prop :=
  sigma_to_uint H < T * f.
```

---

## IV. Key Proofs (Must Have)

### Split Invariance
```
Theorem split_invariance :
  forall f1 f2,
    f1 + f2 = F ->
    P(f1) + P(f2) = P(F).
```

### Proportional Fairness
```
Theorem proportional :
  forall fA fB,
    P(fA) / P(fB) = fA / fB.
```

---

## V. Automatic C++ Generation (Critical)

### Coq → C++ (Extraction)
```
coq-extract --lang=cpp selection.v -o generated/selection.hpp
```

### Output (Example)
```cpp
struct Sigma {
    uint256 value;
};

Sigma compute_sigma(
    const uint256& txid,
    const uint256& C,
    const uint256& entropy,
    uint32_t height
);
```

### Critical Rules

DO NOT manually reimplement compute_sigma
ONLY use generated code

---

## VI. Bind to Real System

In validation.cpp
```cpp
#include "generated/selection.hpp"
```

Replace:
```cpp
// Delete
ComputeSelectionHash(...)

// Use
Sigma H = compute_sigma(txid, C, entropy, height);
```

---

## VII. Dual Consistency Lock (Strongest)

### Lock 1: Mathematics → Code
```
Coq → Automatic Generation → C++
```

### Lock 2: Code → Consensus
```
validation.cpp enforces execution
```

**Result:**
No one can:
- Modify formulas
- Modify implementation
- Change semantics

---

## VIII. Machine Verification (CI)

CI must do:
1. Coq proof compile
2. C++ compile
3. Compare hash output (golden vectors)

**Example:**
```bash
make formal-check
make test-vectors
```

---

## IX. Implementation Verification Status

**Verification Date:** 2026-05-08

**Compliance Status:** PARTIALLY IMPLEMENTED

### Current Implementation Status

The formal verification system is partially implemented in the codebase.

### Existing Components

- formal_verification/coq/ directory exists with Coq proofs:
  - Types.v, Sigma.v, Selection.v, ConvergeRoot.v, Execution.v
  - Hash.v, Serialization.v, Properties.v, State.v, Transition.v
  - Extraction.v, Proofs/ directory
- formal_verification/cpp/ directory exists with C++ harness:
  - rfc011_harness.cpp (RFC-011 differential fuzzer)
  - test_harness.cpp (golden vectors test harness)
  - standalone_harness.cpp
- Python executable spec (formal_verification/spec/rigor_consensus_spec.py)
- Golden test vectors (13 vectors, all passing)
- Differential fuzzing framework (RFC-011)
- JSON-based IPC integration (Python-C++ cross-validation)
- CI pipeline with GitHub Actions workflow

### Missing Components

- Automatic Coq → C++ extraction pipeline (Extraction.v exists but not used)
- generated/ directory with auto-generated C++ headers
- Direct binding of Coq-generated code to validation.cpp
- Completed Coq proofs (admit statements need filling)
- Coq extraction to OCaml/Haskell for testing
- Comprehensive boundary condition fuzzing
- Full proof coverage for all consensus rules

### Current Implementation

The codebase currently:
- Uses manually written C++ implementation in consensus/selection.cpp
- Has separate Coq formal proofs for verification (proofs defined but not completed)
- Uses Python executable spec as reference implementation
- Uses differential fuzzing to verify Python vs C++ implementations
- Does NOT use automatically extracted code from Coq
- Cross-validation between Python and C++ via JSON-based IPC

### Test Results

Golden Vectors: 13/13 passing
- ConvergenceRoot tests: 5 vectors
- PCORB hash tests: 3 vectors
- Winner selection tests: 5 vectors

### TODO

- Complete Coq proofs (fill in admit statements)
- Implement Coq → C++ extraction pipeline
- Create generated/ directory structure
- Update validation.cpp to use generated code
- Expand Coq extraction to OCaml/Haskell for testing
- Add comprehensive boundary condition fuzzing
- Expand proof coverage to all consensus rules

---

**END OF RFC-013-B**
