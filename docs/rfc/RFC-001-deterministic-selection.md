# RFC-001: Deterministic Selection with Converge Economics

**Version:** 1.2 (Sigma-PoW-01)
**Status:** FINAL LOCK

> **v1.2 amendment (SECURITY-001-A8, codename Sigma-PoW-01)** — overrides the σ preimage
> and draw timing previously locked by this RFC and by Sigma-CR-01. Slot `[64..95]` is now
> the parent block hash `B_{h-1}` taken verbatim; `bind_commit` and `target_prev` as a σ
> input are deleted. The block at height `h` pays the winner of the set committed by
> block `h-1`. See `docs/security/SECURITY_001_A8_REPORT.md` and RFC-000 v1.1.
>
> **v1.1 amendment (2026-05-25)** — Conv-Decouple-01 (RFC-014). Historical: `T` became the
> immutable constitutional constant; under that revision `target_prev` participated only
> inside `bind_commit`. That `bind_commit` role is superseded by v1.2.

---

## 1. Abstract

RIGOR defines a blockchain consensus protocol that reduces consensus to deterministic selection governed by a single linear inequality:

```
H_i < T · f_i      where   T := RES_PROBABILITY_TARGET = 2^240   (RFC-014)
```

The protocol introduces:
- **Converge** (economic destruction of ORU)
- **Convergence Root** (global state commitment)
- **RES** (Result) as derived asset via selection, not mining
- **State Tag** verification engine (non-executable)

All rules are deterministic, integer-only, and machine-verifiable.
Any deviation is a consensus violation.

---

## 2. Data Types (Normative)

```
bytes32  = 32-byte array
uint64   = 8-byte unsigned integer (LE)
uint32   = 4-byte unsigned integer (LE)
CAmount  = int64_t (signed, MUST be ≥ 0)
Hash256(x) = SHA256(SHA256(x))
LE32(x)    = 4-byte little-endian encoding
```

---

## 3. ORU Monetary Policy (Consensus Lock)

### 3.1 Unit
```
1 ORU = 100,000,000 base units
```

### 3.2 Block Time
```
Target spacing: 600 seconds
```

### 3.3 Subsidy
```
Initial reward: 1000 ORU
Halving:
  Interval: 210,000 blocks
  Sequence: 1000 → 500 → 250 → 125
```

### 3.4 Floor Rule (CRITICAL)
```
Minimum reward = 125 ORU
NO further halving allowed after reaching 125
```

### 3.5 Enforcement
```
subsidy = ComputeSubsidy(height)

if block_subsidy != subsidy:
    REJECT (bad-RIGOR-subsidy)
```

### 3.6 Constraints
- MUST use integer arithmetic only
- NO floating point
- NO rounding ambiguity

---

## 4. RES Monetary Policy (Consensus Lock)

### 4.1 Unit
```
1 RES = 100 base units
```

### 4.2 Mint Amount
```
RES_REWARD_AMOUNT = 10,000 (exactly 100.00 RES)
```

### 4.3 Supply Cap
```
MAX_RES_SUPPLY = 21,000,000 RES
```

### 4.4 Supply Enforcement
```
if total_RES + RES_REWARD_AMOUNT > MAX_RES_SUPPLY:
     REJECT (bad-RIGOR-RES-cap-exceeded)
```

### 4.5 Mint Conditions (STRICT)
RES mint is allowed ONLY IF:
1. |F| ≥ 1
2. winner exists
3. exactly ONE valid RES mint transaction in block

### 4.6 Forbidden
- partial mint
- multiple mint tx
- discretionary reward
- mint without Converge

---

## 5. Converge Definition (Core Upgrade)

### 5.1 Definition
```
Converge = consensus-verified destruction of ORU supply
```

### 5.2 Converge Amount
```
converge_amount(tx) =
   Σ vout.value
   for all vout where scriptPubKey[0] == OP_RETURN
```

### 5.3 Validity Conditions
A Converge output MUST satisfy:
1. scriptPubKey[0] == OP_RETURN
2. value > 0
3. provably unspendable

### 5.4 Economic Conservation (MANDATORY)
```
Σ inputs = Σ outputs + fee
```

### 5.5 Minimum Threshold
```
MIN_CONVERGE = 1 ORU
```

### 5.6 Converge Transaction
A transaction is Converge TX iff:
```
input_asset(tx) == ORU
AND converge_amount(tx) ≥ MIN_CONVERGE
```

### 5.7 Critical Rule
```
OP_RETURN presence alone DOES NOT imply Converge
```

---

## 6. Convergence Set F

```
F = {
    tx |
    input_asset(tx) == ORU
    AND converge_amount(tx) ≥ MIN_CONVERGE
}
```

---

## 7. Convergence Root

```
C = Hash256( concat( sort(txid_i for tx_i ∈ F) ) )
```

### 7.1 Empty Set
```
if F == ∅:
    C = 0^256
```

### 7.2 Sorting
MUST use raw byte lexicographical order
NO endian conversion

---

## 8. SIGMA Hash

### 8.1 Input (exactly 100 bytes)
```
txid_i     : 32 bytes
C          : 32 bytes
B_{h-k}    : 32 bytes
LE32(h)    : 4 bytes
```

### 8.2 Definition
```
H_i = Hash256(input_buffer)
```

---

## 9. Weight f_i (Consensus Lock)

```
f_i = converge_amount(tx_i)
```

**Forbidden:**
- exponentiation
- normalization
- scaling
- floating-point

---

## 10. Core Inequality

VALID iff:
```
H_i < T · f_i
```

**Arithmetic:**
- MUST use ≥512-bit integer multiplication
- NO overflow allowed

---

## 11. Winner Selection

### 11.1 Order Relation
```
i ≺ j iff:
H_i · f_j < H_j · f_i
```

### 11.2 Winner
```
winner = minimal element under ≺
```

### 11.3 Tie-breaker
```
Direct txid comparison (unsigned byte-wise lexicographical order)
NOT XOR with C (per RFC-000 §9)
```

---

## 12. RES Mint Binding

### 12.1 Requirements
RES Mint TX MUST:

1. reference winner_txid
2. include Convergence Root C
3. include height (LE32) and target_prev
4. include valid signature
5. include valid Converge Proof

### 12.2 Validation
```
REQUIRE txid ∈ F
REQUIRE txid == winner
REQUIRE proof binds to C
```

---

## 13. State Tag Engine

### Definition
```
StateTag = TYPE || parameters
```

### Validation
```
if TYPE == known:
    apply fixed rule
else:
    REJECT
```

**Forbidden:**
- Script VM
- loops
- branching
- execution

---

## 14. V1 Container Model

### ScriptPubKey
```
OP_1 <32-byte program>
```

### Critical Rule
- V1 is a container ONLY
- NOT Taproot
- NO MAST
- NO control block

### Semantic Rule
```
semantic = f(input_asset_domain)
```

### Channels
| Input | Meaning |
|-------|---------|
| ORU   | RES Mint |
| RES   | RES Spend |

---

## 15. Global Consistency Requirements

All nodes MUST produce identical:
- converge_amount
- F
- C
- H_i
- winner
- RES supply

---

## 16. Rejection Conditions

Nodes MUST reject if:
- bad-RIGOR-subsidy
- bad-RIGOR-RES-cap-exceeded
- invalid converge_amount
- invalid Convergence Root
- invalid inequality
- multiple RES mint
- missing winner

---

## 17. Consensus Lock (Immutable)

1. Only rule: H_i < T · f_i
2. f_i = converge_amount (exact)
3. Converge = economic destruction, not script pattern
4. No floating point, no exponent
5. All serialization deterministic
6. Any deviation = consensus violation

---

## 18. Economic Identity (Final Statement)

```
ORU = base energy (PoW + economic input)
RES = derived scarcity via deterministic selection
No Converge → No RES
No Winner → No RES
```

---

## 19. Final Canonical Statement

```
Consensus = Deterministic Selection
           + Converge-weighted probability
           + State-tag verification
```

---

**END OF RFC-001**
