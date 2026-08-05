# RFC-011: Differential Fuzzing & Consensus Conformance System

**Version:** 1.0
**Status:** Consensus-Critical

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

---

## 0. Goal (Must Be Hardcoded)

This system guarantees:
Any two independent implementations MUST produce identical results
for all consensus-critical inputs.
Any divergence MUST be detected automatically.

---

## 1. System Structure

### 1.1 Three-Layer Architecture
```
[ Generator ] → [ Multiple Implementations ] → [ Comparator ]
```

### 1.2 Components
1. Fuzz Input Generator
2. Reference Implementation (Golden)
3. Candidate Implementations (C++, Rust, Python...)
4. Deterministic Trace Extractor
5. Byte-level Comparator

---

## 2. Fuzz Input Generation (Core)

### 2.1 Input Must Cover
- Random txid
- Random f_i (including boundary values)
- Random block height
- Random B_{h-k}
- Random transaction count (0 → 10,000)

### 2.2 Critical Boundaries (Must Focus Coverage)
- f_i = 1
- f_i = 2^63-1
- H_i = 0
- H_i = 2^256-1
- F = empty
- F = 1 element
- F = large set

### 2.3 Invalid Inputs (Must Test)
- Wrong length (≠100 bytes)
- Wrong endian
- Overflow attempt
- Duplicate txid

---

## 3. Reference Implementation (Golden Implementation)

### 3.1 Required Characteristics
- Slowest but strictest
- No optimizations
- Execute exactly per RFC-008/009/010

### 3.2 Role
Single Source of Truth

---

## 4. Execution Flow

### Step 1: Generate Test Case
```
case = generate_random_case()
```

### Step 2: Multi-Implementation Execution
```
result_A = impl_A(case)
result_B = impl_B(case)
result_ref = reference(case)
```

### Step 3: Output Trace
```json
{
  "C": "...",
  "H": [...],
  "f": [...],
  "eligible": [...],
  "winner": "..."
}
```

### Step 4: Field-by-Field Comparison
```
C_A == C_ref ?
H_A == H_ref ?
winner_A == winner_ref ?
```

---

## 5. Decision Rules (Absolutely Strict)

IF ANY field differs:
    → FAIL
    → dump full trace
    → stop execution

---

## 6. Difference Localization (Automatic)

Output:
```
Mismatch detected:
Field: H_i[3]
Impl A: 0xABC...
Impl B: 0xABD...
Expected: 0xABC...
→ Root cause likely:
   - endian bug
   - serialization mismatch
```

---

## 7. Coverage Requirements

MUST cover:
- All branches
- All edge cases
- All arithmetic boundaries

---

## 8. Specialized Attack Tests

### 8.1 Grinding Test
```
Same f_i
vary txid many times
→ ensure linear probability only
```

### 8.2 Split Test
```
f = 100
Case A: single tx (100)
Case B: 100 tx (1 each)
→ probability MUST match
```

### 8.3 Overflow Test
```
T × f_i near 2^512 boundary
→ MUST not overflow
```

---

## 9. Common Error Capture

- using uint256 for multiplication
- using float
- endian flip
- wrong concat order
- skipping empty set rule

---

## 10. Output Specification (Unified)

```json
{
  "case_id": 12345,
  "status": "PASS",
  "hash": "deterministic checksum"
}
```

---

## 11. Consensus Lock

Implementations MUST:
- pass all fuzz tests
- match reference implementation
- produce identical traces

Otherwise:
    → NOT consensus compatible

---

## 12. Final Execution Instructions

You MUST implement RFC-011 Differential Fuzzing System.

STRICT REQUIREMENTS:
1. Implement a reference model EXACTLY matching RFC-008/009/010
2. Generate random test cases covering:
   - edge cases
   - large sets
   - boundary arithmetic
3. Execute at least TWO independent implementations
4. Extract full execution trace:
   - C
   - H_i
   - f_i
   - eligibility
   - winner
5. Compare ALL fields byte-by-byte
6. On ANY mismatch:
   - STOP
   - DUMP full trace
   - REPORT error

FORBIDDEN:
- Ignoring mismatches
- Partial comparison
- Float arithmetic
- Non-deterministic behavior

GOAL:
Zero divergence across all implementations.

**Implementation Status**: FULLY IMPLEMENTED
- Location: `./formal_verification/fuzz/rfc011_differential_fuzzer.py`
- C++ harness: `./formal_verification/cpp/bin/rfc011_harness`
- Test results: 212/212 tests passing
- Usage: `python3 ./formal_verification/fuzz/rfc011_differential_fuzzer.py [--iterations N] [--seed S]`

---

**END OF RFC-011**
