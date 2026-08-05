# RFC-013-A: Symbol Table & Naming Enforcement Layer

**Title:** Symbol Table & Naming Enforcement Layer
**Status:** CONSENSUS-CRITICAL EXTENSION

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**
Follow RFC-013 for semantic meaning (consensus-critical)
Follow RFC-013-A for implementation enforcement (non-consensus)

---

## I. Core Idea (Very Critical)

Names ≠ Strings
Names = Types

In other words:
```
uint256 C;          // Weak
ConvergenceRoot C;  // Strong
```

---

## II. Strong Type Encapsulation (Must Do)

### New File: consensus/types.h

```cpp
#pragma once
#include <uint256.h>

// ==============================
// CORE TYPES (NO ALIAS ALLOWED)
// ==============================

struct ConvergenceRoot {
    uint256 value;
};

struct Sigma {
    uint256 value;
};

struct SelectionProof {
    std::vector<unsigned char> data;
};

struct ConvergeProof {
    std::vector<unsigned char> data;
};

struct ORUAmount {
    int64_t value;
};

struct RESAmount {
    int64_t value;
};

struct Signature {
    std::vector<unsigned char> data;
};
```

### Mandatory Rules

DO NOT:
- use uint256 directly for ConvergenceRoot or Sigma
- use std::vector for Proof types directly

MUST:
- use exact struct types defined here

---

## III. Forbid Implicit Conversion (Critical)

```cpp
struct ConvergenceRoot {
    uint256 value;

    explicit ConvergenceRoot(const uint256& v) : value(v) {}
};
```

This prevents:
```
uint256 x = C;   // Error
```

---

## IV. API Strong Binding (Interface-Level Lock)

### Wrong Usage (Must Forbid)
```cpp
uint256 ComputeRoot(...);
```

### Correct Usage
```cpp
ConvergenceRoot ComputeConvergenceRoot(...);
Sigma ComputeSigma(...);
```

This prevents mixing:
```cpp
Sigma s = ComputeConvergenceRoot(...); // Compilation error
```

---

## V. Function Signature Lock (Most Critical)

### selection.h
```cpp
Sigma ComputeSigma(
    const uint256& txid,
    const ConvergenceRoot& C,
    const uint256& entropy,
    uint32_t height
);
```

### verifier.h
```cpp
bool VerifySelectionProof(
    const SelectionProof& proof,
    const ConvergenceRoot& C,
    const Sigma& sigma
);
```

Must ensure:
```
Sigma ≠ Proof ≠ Root
```

---

## VI. Compile-Time Forbidden Naming (Killer Feature)

### New File: consensus/naming_guard.h

```cpp
#pragma once

// Forbidden identifiers
#define RNG static_assert(false, "Use Sigma instead of RNG")
#define RNGProof static_assert(false, "Use SelectionProof")
#define PCSIG static_assert(false, "Use ConvergeProof")
#define SIG static_assert(false, "Use RES")
#define ORU static_assert(false, "Use ORU")
```

---

## VII. Unit Test Lock (Machine Verification)

### test_naming.cpp

```cpp
static_assert(std::is_same<decltype(ComputeConvergenceRoot(...)), ConvergenceRoot>::value);
static_assert(!std::is_same<ConvergenceRoot, Sigma>::value);
```

This ensures:
Any type mixing → CI failure

---

## VIII. Runtime Defense (Double Insurance)

### debug assert

```cpp
assert(typeid(obj) == typeid(ConvergenceRoot));
```

This prevents:
Deserialization errors / RPC errors

---

## IX. Consensus Layer Final Lock (Strongest)

In ConnectBlock:

```cpp
if (!IsConvergenceRootValid(C))
    return state.Invalid(...);
```

---

## X. Implementation Verification Status

**Verification Date:** 2026-05-08

**Compliance Status:** NOT IMPLEMENTED

### Current Implementation Status

The strong type encapsulation system described in this RFC is NOT yet implemented in the codebase.

### Missing Components

- consensus/types.h: Does not exist
- consensus/naming_guard.h: Does not exist
- struct ConvergenceRoot: Not implemented (using uint256 directly)
- struct Sigma: Not implemented (using uint256 directly)
- struct SelectionProof: Not implemented (using std::vector directly)
- struct ConvergeProof: Not implemented (using std::vector directly)
- struct ORUAmount: Not implemented
- struct RESAmount: Not implemented
- struct Signature: Not implemented

### Current Implementation

The codebase currently uses:
- uint256 for ConvergenceRoot and Sigma
- std::vector<unsigned char> for proof types
- int64_t for amounts (ORU and RES)

### TODO

- Implement consensus/types.h with strong type definitions
- Implement consensus/naming_guard.h with compile-time forbidden naming
- Add explicit constructors to prevent implicit conversion
- Update function signatures to use strong types
- Add unit tests for type safety
- Add runtime assertions for type validation

---

**END OF RFC-013-A**
