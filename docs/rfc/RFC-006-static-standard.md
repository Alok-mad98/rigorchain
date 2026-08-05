# RFC-006: Static Standard

**Golden Test Vector Suite (Machine Sole Arbiter)**

---

## A. General Principles

THIS FILE DEFINES CANONICAL CONSENSUS OUTPUTS.

ANY IMPLEMENTATION MUST MATCH ALL OUTPUTS EXACTLY.

- BYTE-FOR-BYTE MATCH IS REQUIRED.
- NO TOLERANCE.
- NO ROUNDING.
- NO INTERPRETATION.

IF ANY TEST FAILS → IMPLEMENTATION IS INVALID.

---

## B. Unified Data Format (Must be Locked)

```json
{
  "name": "string",
  "input": {
    "txids": ["hex32", "..."],
    "fees": ["uint64", "..."],
    "height": "uint32",
    "prev_hash": "hex32",
    "target": "uint256"
  },
  "expected": {
    "C": "hex32",
    "sigma": ["hex32", "..."],
    "eligible": ["bool", "..."],
    "winner_index": "int | null"
  }
}
```

---

## C. Test Coverage (Must Include)

1. Empty Set
2. Single Transaction
3. Multi-Transaction Sorting
4. Inequality Boundary
5. Large Numbers (256-bit)
6. f_i = 1
7. f_i Extremely Large
8. No Winner
9. Multi-Winner (Tie)
10. XOR Tie-Break

---

## D. Formal Test Vectors

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
    "C": "00".repeat(32),
    "sigma": [],
    "eligible": [],
    "winner_index": null
  }
}
```

### CASE-002

```json
{
  "name": "single_tx_no_winner",
  "input": {
    "txids": [
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    ],
    "fees": [1],
    "height": 1,
    "prev_hash": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "target": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  },
  "expected": {
    "C": "computed",
    "sigma": ["computed"],
    "eligible": [true],
    "winner_index": null
  }
}
```

**You must calculate the exact values here**

### CASE 003 — Sorting Affects C

```json
{
  "name": "sorting_order",
  "input": {
    "txids": [
      "ff...ff",
      "00...00"
    ],
    "fees": [1,1],
    "height": 10,
    "prev_hash": "11...11",
    "target": "ff...ff"
  },
  "expected": {
    "C": "MUST be hash(sorted(txids))",
    "winner_index": "depends"
  }
}
```

**Forced Check:**
Sorting must be raw byte lexicographical

### CASE 004 — No Winner

```json
{
  "name": "no_winner",
  "input": {
    "txids": ["aa...", "bb..."],
    "fees": [1,1],
    "height": 5,
    "prev_hash": "cc...",
    "target": "00000000000000000000000000000001"
  },
  "expected": {
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
    "txids": ["aa...", "bb..."],
    "fees": [1,1000],
    "height": 7,
    "prev_hash": "dd...",
    "target": "ff...ff"
  },
  "expected": {
    "winner_index": 1
  }
}
```

**Verification:**
winner = argmin(H / f)

### CASE 006 — Tie-break

```json
{
  "name": "tie_break",
  "input": {
    "txids": ["11...", "22..."],
    "fees": [10,10],
    "height": 9,
    "prev_hash": "ee...",
    "target": "ff...ff"
  },
  "expected": {
    "note": "force equal H/f",
    "winner_rule": "direct txid comparison (NOT XOR with C per RFC-000 §9)"
  }
}
```

### CASE 007 — Big Int Test (MUST)

```json
{
  "name": "big_int",
  "input": {
    "txids": ["ff...ff", "ee...ee"],
    "fees": [18446744073709551615, 1],
    "height": 100,
    "prev_hash": "aa...",
    "target": "ff...ff"
  },
  "expected": {
    "requirement": "no overflow",
    "winner_index": "deterministic"
  }
}
```

---

## E. Execution Instructions for You

**TASK:** Implement consensus functions that PASS ALL test vectors.

**STRICT RULES:**
1. DO NOT modify test vectors
2. DO NOT approximate values
3. DO NOT use floating point
4. DO NOT reorder operations
5. DO NOT optimize logic

**REQUIREMENT:**
Output MUST match expected EXACTLY.

IF mismatch:
→ code is WRONG
→ fix implementation
→ do not change test

---

**END OF RFC-006**
