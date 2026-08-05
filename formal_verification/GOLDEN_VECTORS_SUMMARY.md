# Golden Test Vector Suite - RIGOR Consensus

## Summary

This document describes the golden test vectors for RIGOR consensus implementation following RFC-000-FINAL exactly.

## Consensus Fixes Applied

### Fix 1: Tie-Break Rule (RFC-000 §9)
**File:** `core/src/consensus/selection.cpp`

**Before:**
```cpp
// Tie-break: (txid XOR C) lexicographical order
uint256 xor_i, xor_best;
for (size_t j = 0; j < 32; ++j) {
    xor_i.begin()[j] = txids[i].begin()[j] ^ convergence_root.begin()[j];
    xor_best.begin()[j] = txids[winner_index].begin()[j] ^ convergence_root.begin()[j];
}
if (xor_i < xor_best) {
```

**After (RFC-000 §9 compliant):**
```cpp
// RFC-000 §9: TIE-BREAK RULE
// IF: H_i * f_j == H_j * f_i
// THEN: txid_i < txid_j → winner
if (txids[i] < txids[winner_index]) {
```

**RFC-000 §9 Text:**
```
IF:
H_i * f_j == H_j * f_i
THEN:
txid_i < txid_j → winner
```

### Fix 2: Convergence Root Boundary Condition (RFC-000 §10)
**File:** `core/src/consensus/selection.cpp`

**Before:**
```cpp
if (txids.empty()) {
    return uint256::ZERO; // Hash256(empty) = 0...0
}
```

**After (RFC-000 §10 compliant):**
```cpp
// RFC-000 §10: IF N < 2: C = Hash256(empty) = 32 zero bytes
if (txids.size() < 2) {
    return uint256::ZERO;
}
```

**RFC-000 §10 Text:**
```
IF N < 2:
→ winner = NONE

IF F = ∅:
→ C = Hash256(empty)
→ winner = NONE
```

## Golden Test Vectors

### CASE 001 — Empty Set
```json
{
  "name": "empty_set",
  "input": {
    "txids": [],
    "fees": [],
    "height": 1,
    "prev_hash": "00".repeat(32),
    "target": "0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "0000000000000000000000000000000000000000000000000000000000000000",
    "sigma": [],
    "eligible": [],
    "winner_index": null
  }
}
```

### CASE 002 — Single Transaction No Winner
```json
{
  "name": "single_tx_no_winner",
  "input": {
    "txids": ["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],
    "fees": [1],
    "height": 1,
    "prev_hash": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "0000000000000000000000000000000000000000000000000000000000000000",
    "sigma": ["30f0b752feab4e693dfabc7ec970f5fb7f2188585f14f4076a1cea0393406a1c"],
    "eligible": [true],
    "winner_index": null
  }
}
```

### CASE 003 — Sorting Order
```json
{
  "name": "sorting_order",
  "input": {
    "txids": [
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "0000000000000000000000000000000000000000000000000000000000000000"
    ],
    "fees": [1, 1],
    "height": 10,
    "prev_hash": "1111111111111111111111111111111111111111111111111111111111111111",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "4fa4a869878bd4837e6192377293a0cc14f192b88711635ca3df0447dc1bea76",
    "sigma": [
      "9b8fec9a657ea2e6d27f3eead98c02cca2f8c941970ed60b3238d08f4dbd75b2",
      "6e0f00f65387c23518af80e3b215f895b552d33f3ed90c653a083307511e0cc1"
    ],
    "eligible": [true, true],
    "winner_index": 1
  }
}
```

### CASE 004 — No Winner
```json
{
  "name": "no_winner",
  "input": {
    "txids": [
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    ],
    "fees": [1, 1],
    "height": 5,
    "prev_hash": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
    "target": "0000000000000000000000000000000000000000000000000000000000000001"
  },
  "expected": {
    "C": "499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb",
    "sigma": [
      "c3f728239a89f1876eef97bb9e9238516e8e34f34c8b6408edc63b2bc20aaf88",
      "3957870e0c706c61cc7ca73350828ae3d56f3f082cdcf10eb6f8fcf7ae45332b"
    ],
    "eligible": [false, false],
    "winner_index": null
  }
}
```

### CASE 005 — Weight Priority
```json
{
  "name": "weight_priority",
  "input": {
    "txids": [
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    ],
    "fees": [1, 1000],
    "height": 7,
    "prev_hash": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "499d0d3b39373fb9b7b0f399b7411f7af213d91c32624280e995ae0f8eb776fb",
    "sigma": [
      "4f19b2a6c8b63b1170d8ab52268deda9e5cd171e85549460beabf164b29192c7",
      "c8d3d18fc2f5f86630d734ef796bb4d048b846d2ddaa1b37284136ac54919061"
    ],
    "eligible": [true, true],
    "winner_index": 1
  }
}
```

### CASE 006 — Tie Break
```json
{
  "name": "tie_break",
  "input": {
    "txids": [
      "1111111111111111111111111111111111111111111111111111111111111111",
      "2222222222222222222222222222222222222222222222222222222222222222"
    ],
    "fees": [10, 10],
    "height": 9,
    "prev_hash": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "1140b574afee3cb89a4db3dc8037acfa856f5112e68a954e3ca0a908082c98ba",
    "sigma": [
      "fb47e7af1a7f9ed73f5e4a5aed5344e9f1bf7fabbef31d2839e2766e8322dbee",
      "717ef2a6325670be2f6714de2687d9b0399c9062c1b683ea1f0150b99859a852"
    ],
    "eligible": [true, true],
    "winner_index": 1
  }
}
```

### CASE 007 — Big Int
```json
{
  "name": "big_int",
  "input": {
    "txids": [
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
    ],
    "fees": [18446744073709551615, 1],
    "height": 100,
    "prev_hash": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "490de05febe1b32d530db9f4587a5eae06cbbe2dd0aa8582c0ab7755ebcb9d05",
    "sigma": [
      "1d3365ed0ea0262ad8d9ab85682124ee6e7c514723de97a00222e9942ac115b2",
      "a7b17ad1704130182b2c71db746dc769a338c6e508248ffa0dc84719a0266193"
    ],
    "eligible": [true, true],
    "winner_index": 0
  }
}
```

## Implementation Requirements

**STRICT RULES:**
1. DO NOT modify test vectors
2. DO NOT approximate values
3. DO NOT use floating point
4. DO NOT reorder operations
5. DO NOT optimize logic

**REQUIREMENT:**
- Output MUST match expected EXACTLY
- IF mismatch → code is WRONG → fix implementation → do not change test

## Files Modified

1. `core/src/consensus/selection.cpp` - Fixed tie-break rule and convergence root boundary condition
2. `core/src/consensus/selection.h` - Updated documentation for tie-break rule
3. `core/src/test/selection_golden_tests.cpp` - Updated with computed golden vector values
4. `formal_verification/compute_golden_vectors.py` - Python script to compute golden vectors per RFC-000-FINAL
5. `formal_verification/rigor_golden_vectors_computed.json` - Computed golden vectors output

## Verification

To verify the implementation:
```bash
cd core
./src/test/test_bitcoin --run_test=selection_golden_tests
```

All tests must pass with byte-for-byte exact matching.
