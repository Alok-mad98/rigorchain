# RFC-010: Deterministic Execution Trace

**Version:** 1.0
**Goal:** Enable any implementation to align step-by-step for verification (debug / audit / formal)

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**

---

## 0. Principle

Every consensus decision MUST be reproducible step-by-step.

---

## 1. Execution Flow (Unique Order)

### Step 1: Classify Transactions
```
For each tx:
if input_asset == ORU and has RES output:
    type = MINT
else:
    type = SPEND
```

### Step 2: Construct Set F
```
F = all MINT transactions
```

### Step 3: Compute C
```
C = ConvergenceRoot(F)
```

### Step 4: Compute Each Candidate
```
For each tx_i in F:
H_i = Hash256(txid_i || C || B_{h-k} || LE32(h))
f_i = Mint Cost
```

### Step 5: Filter
```
Eligible if:
H_i < T · f_i
```

### Step 6: Select Winner
```
winner = argmin_i (H_i * f_j < H_j * f_i)
```

### Step 7: Verify Proof
```
Verify:
- tx references winner_txid
- converge_root == C
- signature valid
- proof structure valid
```

### Step 8: State Update
```
Mint RES
Update supply
Consume ORU
```

---

## 2. Trace Output Format (Strongly Recommended Implementation)

```json
{
  "height": 123456,
  "C": "0x...",
  "candidates": [
    {
      "txid": "...",
      "H_i": "...",
      "f_i": 100000000,
      "eligible": true
    }
  ],
  "winner": "txid..."
}
```

**Implementation**: Currently implemented via `SelectionResult` struct in `selection.h` with convergence_root, sigma_hashes, eligible, and winner_index fields. Full JSON trace output not yet implemented.

---

## 3. Verifiability

Given trace:
Any node MUST recompute and match:
- C
- H_i
- eligibility
- winner

---

## 4. Fork Detection Capability

If ANY field differs:
→ Consensus mismatch
→ Block MUST be rejected

---

## 5. Forbidden

- Skipping steps
- Caching without verification
- Reordering evaluation

---

**END OF RFC-010**
