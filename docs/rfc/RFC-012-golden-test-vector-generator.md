# RFC-012: Golden Test Vector Generator

**Version:** 1.1 (Conv-Decouple-01 amendment)
**Status:** Consensus-Critical

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

> **v1.1 amendment (2026-05-25, RFC-014 activation)**: All golden vectors that
> exercise the RES eligibility inequality MUST be regenerated against the
> constitutional constant `RES_PROBABILITY_TARGET = 2^240`. The legacy form
> using `SetCompact(target_prev)` as the inequality RHS is permanently retired.
> The regeneration entry point is `core/src/test/compute_rfc001_vectors.cpp`,
> which now hardwires the constant. Affected artifacts:
>
> - `core/src/test/selection_golden_tests.cpp` embedded constants
> - `formal_verification/golden_vectors.json`
> - `formal_verification/rigor_golden_vectors.json`
> - `formal_verification/rigor_golden_vectors_computed.json`
> - `formal_verification/GOLDEN_VECTORS_SUMMARY.md`
>
> `target_prev` continues to thread through σ via `bind_commit` (Sigma-CR-01)
> and therefore continues to appear in the vectors on the σ side; only the
> inequality RHS source changes.

---

## Goal

Automatically generate 1000+ test cases
Cover all boundaries
Automatically verify
Output canonical vectors

---

## I. Generator Core Principles (Must Be Hardcoded)

1. All generated cases MUST be deterministic (reproducible)
2. All cases MUST be independently verifiable
3. N == 0 MUST produce winner = null; N == 1 may produce a winner if eligible
4. All calculations MUST use reference implementation
5. Output MUST be directly usable for RFC-006

---

## II. Generator Input Parameters

```
SEED = 1337
NUM_CASES = 1000
MAX_TX = 50
MAX_FEE = 10**12
```

---

## III. Reference Engine (For Internal Generator Use)

This is the "Source of Truth", all outputs MUST come from it

```python
import hashlib
import random

def hash256(b):
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()

def le32(x):
    return x.to_bytes(4, "little")

def bytes_to_int(b):
    return int.from_bytes(b, "big")

def compute_C(txids):
    if len(txids) == 0:
        return b"\x00"*32
    txids_sorted = sorted(txids)
    return hash256(b"".join(txids_sorted))

def compute_H(txid, C, prev, height):
    return hash256(txid + C + prev + le32(height))

def select_winner(txids, fees, prev, height, T):
    N = len(txids)

    # Critical rule
    if N == 0:
        return None, [], []

    C = compute_C(txids)

    Hs = []
    eligible = []

    for i in range(N):
        H = compute_H(txids[i], C, prev, height)
        Hs.append(H)
        eligible.append(bytes_to_int(H) < T * fees[i])

    best = None

    for i in range(N):
        if not eligible[i]:
            continue

        if best is None:
            best = i
            continue

        if bytes_to_int(Hs[i]) * fees[best] < bytes_to_int(Hs[best]) * fees[i]:
            best = i
        elif bytes_to_int(Hs[i]) * fees[best] == bytes_to_int(Hs[best]) * fees[i]:
            if bytes(a ^ b for a, b in zip(txids[i], C)) < \
               bytes(a ^ b for a, b in zip(txids[best], C)):
                best = i

    return best, Hs, eligible, C
```

---

## IV. Random Case Generation

```python
def random_txid():
    return random.randbytes(32)

def generate_case(i):
    n = random.randint(0, MAX_TX)

    txids = [random_txid() for _ in range(n)]
    fees = [random.randint(1, MAX_FEE) for _ in range(n)]

    height = random.randint(0, 10_000)
    prev = random.randbytes(32)

    T = random.randint(1, 2**256 - 1)

    winner, Hs, eligible, C = select_winner(txids, fees, prev, height, T)

    return {
        "name": f"auto_{i}",
        "input": {
            "txids": [tx.hex() for tx in txids],
            "fees": fees,
            "height": height,
            "prev_hash": prev.hex(),
            "target": hex(T)
        },
        "expected": {
            "C": C.hex(),
            "sigma": [h.hex() for h in Hs],
            "eligible": eligible,
            "winner_index": winner
        }
    }
```

---

## V. Boundary Reinforcement (Very Critical)

Cannot just be random, MUST forcibly inject extreme cases

```python
def inject_edge_cases(cases):

    # N = 0
    cases.append(generate_fixed_case([], []))

    # N = 1 (MUST have no winner)
    tx = random_txid()
    cases.append(generate_fixed_case([tx], [1]))

    # All not eligible
    # T very small
    # fees very small

    # Very large numbers
    # fees = max uint64

    # Completely equal (trigger tie-break)
```

---

## VI. Main Generator

```python
def generate_all():
    random.seed(SEED)

    cases = []

    for i in range(NUM_CASES):
        cases.append(generate_case(i))

    inject_edge_cases(cases)

    return cases
```

---

## VII. Output (Write to JSON)

```python
import json

cases = generate_all()

with open("rigor_test_vectors.json", "w") as f:
    json.dump(cases, f, indent=2)
```

---

## VIII. Current Implementation Status

### Existing Golden Test Vectors

The following 7 golden test vectors are currently implemented and verified:

1. **CASE 001 — Empty Set**
   - Input: 0 transactions
   - Expected: C = 32 zero bytes, winner = null

2. **CASE 002 — Single Transaction Winner**
   - Input: 1 transaction
   - Expected: C = 32 zero bytes, winner = index 0 if eligible

3. **CASE 003 — Sorting Order**
   - Input: 2 transactions (0xFF...FF, 0x00...00)
   - Expected: C computed from sorted txids, winner = index 1

4. **CASE 004 — No Winner**
   - Input: 2 transactions, very small target
   - Expected: Both ineligible, winner = null

5. **CASE 005 — Weight Priority**
   - Input: 2 transactions (fees: 1, 1000)
   - Expected: Winner = index 1 (higher weight)

6. **CASE 006 — Tie Break**
   - Input: 2 transactions (equal fees, equal H/f)
   - Expected: Winner = index 1 (direct txid comparison per RFC-000 §9)

7. **CASE 007 — Big Int**
   - Input: 2 transactions (fees: UINT64_MAX, 1)
   - Expected: Winner = index 0 (512-bit arithmetic)

### Files

- C++ Tests: `./core/src/test/selection_golden_tests.cpp`
- Python Reference: `./formal_verification/spec/rigor_consensus_spec.py`
- Summary: `./formal_verification/GOLDEN_VECTORS_SUMMARY.md`
- Python Tests: `./formal_verification/tests/test_golden_vectors.py`

### Verification Command

```bash
cd core
./src/test/test_bitcoin --run_test=selection_golden_tests
```

All tests must pass with byte-for-byte exact matching.

### TODO

- Implement automatic test vector generator per this RFC
- Generate 1000+ random test cases
- Inject edge cases systematically
- Output JSON format compatible with RFC-006

---

**END OF RFC-012**
