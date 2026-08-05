# RFC-000A: RIGOR Implementation Bible

**Status:** MANDATORY
**Authority:** SUBORDINATE TO RFC-000-FINAL
**Purpose:** Eliminate ALL implementation ambiguity

**Strict Execution Manual — Derived from RFC-000-FINAL**

---

## 0. GLOBAL LOCK

You are NOT implementing logic.
You are executing a specification.

You MUST:
- follow byte-level rules
- produce identical output

You MUST NOT:
- interpret
- optimize behavior
- modify structure

---

## 1. DATA TYPES

MUST USE EXACT TYPES

- uint32 → 4 bytes → little-endian
- uint64 → 8 bytes → little-endian
- bytes32 → exactly 32 bytes

**FORBIDDEN**
- big-endian
- variable-length integers
- trimming leading zeros
- implicit casting

---

## 2. BYTE CONCAT

```
concat(a, b, c) = a || b || c
```

**FORBIDDEN**
- delimiter
- length prefix
- padding

---

## 3. HASHING

ONLY VALID HASH

```
Hash256(x) = SHA256(SHA256(x))
```

**FORBIDDEN**
- single SHA256
- SHA3 / Keccak
- any alternative hash

---

## 4. HASH INPUT (100-byte lock)

EXACT STRUCTURE

```
[32] txid
[32] ConvergenceRoot
[32] B_{h-1}          (parent block hash, verbatim)
[4 ] height (LE)      (payout height h)
TOTAL = 100 bytes EXACT
```

HARD CHECK

```
len(input) != 100 → REJECT (bad-hash-length)
```

**FORBIDDEN**
- missing field
- extra field
- reordered fields
- hashing B_{h-1} again before placing it at offset 64

Superseded by SECURITY-001-A8 (Sigma-PoW-01): the field at offset 64 was previously
`B_{h-10}`, and before that revision `bind_commit = Hash256(B_{h-10} || LE32(target_prev))`.
See RFC-000 §4.

---

## 5. SEED BLOCK

```
seed = B_{h-1} = block_hash(height - 1)
```

The seed is the parent block hash, and `height` in the σ input is the height of the block
that pays the mint. The set `F` and the root `C` come from the parent block, which committed
them; the block at height `h` only pays the winner.

**FORBIDDEN**
- any bind depth other than 1
- using an ancestor deeper than the parent
- special-casing low heights

Superseded by SECURITY-001-A8 (Sigma-PoW-01): the fixed offset `k = 10` and the all-zero
bootstrap stand-in for heights below `k` are deleted. The parent block hash is defined at
every height from 1 upward.

---

## 6. ROOT (Convergence Root)

```
C = Hash256(concat(sorted(txids)))
```

SORT RULE (Critical)

Sort using unsigned byte comparison

**FORBIDDEN**
- string compare
- locale compare
- signed char compare

---

## 7. SIGMA (sigma calculation)

```
sigma_i = Hash256(100-byte input)
```

---

## 8. WEIGHT

```
f_i MUST be integer
f_i MUST >= 1
```

**FORBIDDEN**
- float
- log / sqrt / transform

---

## 9. INEQUALITY

ONLY VALID FORM

```
H_i < T * f_i
```

IMPLEMENTATION FORM

```
H_i * f_j < H_j * f_i
```

**FORBIDDEN**
- division
- float comparison

---

## 10. BIG INTEGER

All multiplication MUST be arbitrary precision

**FORBIDDEN**
- overflow
- fixed-width wraparound

---

## 11. TIE-BREAK

IF equal:
→ smaller txid wins

---

## 12. BOUNDARY

```
N < 2 → winner = NONE
f_i == 0 → REJECT
F = empty → C = Hash256(empty)
```

---

## 13. DETERMINISM

Execution MUST NOT depend on:
- time
- randomness
- environment

---

## 14. EXECUTION TRACE

MUST OUTPUT

- txid
- f_i
- H_i
- sigma_i
- eligible

GLOBAL

- C
- winner_index

**FORBIDDEN**
- missing fields
- reordered output

---

## 15. REJECT CODES

- bad-encoding
- bad-hash-length
- bad-weight
- bad-height
- bad-root
- bad-selection

---

## 16. STATE ISOLATION

Consensus uses ONLY:
- block data
- tx data

**FORBIDDEN**
- mempool
- cache
- external state

---

## 17. CHAIN IDENTITY

```
chain_name = "RIGOR"
bech32_hrp = "RG"
```

CRITICAL RULE

These are DISPLAY ONLY.
MUST NOT affect:
- hashing
- selection
- consensus

---

## 18. HIERARCHY RULE

RFC-000-FINAL overrides EVERYTHING.

If conflict:
→ ignore this document

---

## 19. ANTI-INTERPRETATION

You MUST NOT interpret.
You MUST:
→ implement literally

---

## 20. FINAL RULE

Correctness = byte-for-byte equivalence

---

## Implementation Verification Status

**Verification Date:** 2026-05-08

**Compliance Status:** COMPLIANT

### Verified Compliance

- Data Types: Using uint32 (4 bytes LE), uint64 (8 bytes LE), uint256 (32 bytes)
- Byte Concat: No delimiters, length prefixes, or padding
- Hashing: Using SHA256d (Hash256) via CHash256
- Hash Input: 100-byte structure implemented in consensus/convergence_logic.h
- Block Offset: k=10 fixed in convergence logic
- Convergence Root: Hash256(concat(sorted(txids))) implemented
- Sort Rule: Unsigned byte comparison via std::sort
- Sigma: Hash256(100-byte input) implemented
- Weight: Integer weights, f_i >= 1 enforced
- Inequality: Cross-multiplication form H_i * f_j < H_j * f_i implemented
- Big Integer: arith_uint256 for arbitrary precision
- Tie-Break: Direct txid comparison implemented
- Boundary: N < 2 → winner = NONE implemented
- Determinism: No time/randomness dependency
- State Isolation: Uses only block and tx data

### Code Locations

- consensus/selection.cpp: Selection logic implementation
- consensus/convergence_logic.h: Convergence logic interface
- consensus/amount.h: Amount types
- hash.h: SHA256d implementation

---

**END OF RFC-000A**
