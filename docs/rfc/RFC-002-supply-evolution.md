# RFC-002: Supply Evolution Equation

**Version:** 1.0 (Canonical Lock)
**Status:** FINAL LOCK

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

---

## 1. Abstract

This document defines the canonical supply evolution model for the RIGOR protocol.

RIGOR consists of two economically linked assets:
- **ORU (Orum)** = primary work-backed asset
- **RES (Result)** = secondary scarcity asset

The system forms a closed economic transformation:
```
PoW → ORU → Converge → RES
```

This RFC formally defines:
- total supply dynamics
- destruction dynamics
- mint dynamics
- boundedness
- monotonicity
- convergence properties
- machine-verifiable state equations

All rules are consensus-critical.

---

## 2. Definitions

### 2.1 Time
Let:
```
t ∈ ℕ
```
denote block height.

### 2.2 ORU Supply
Define:
```
S_ORU(t)
```
as total ORU supply after block t.

### 2.3 RES Supply
Define:
```
S_RES(t)
```
as total RES supply after block t.

### 2.4 Subsidy
Define:
```
R(t)
```
as ORU block reward.

From RFC-001:
```
R(t) ∈ {1000, 500, 250, 125}
```

### 2.5 Converge
Define:
```
C(t)
```
as total ORU converged (destroyed) in block t.

### 2.6 RES Event
Define:
```
M(t)
```
where:
```
M(t) ∈ {0,1}
```
and:
```
M(t) = 1 if valid RES mint occurs
M(t) = 0 otherwise
```

---

## 3. ORU Supply Equation

### 3.1 State Equation
```
S_ORU(t+1) = S_ORU(t) + R(t) - C(t)
```

### 3.2 Initial Condition
```
S_ORU(0) = genesis_supply
```

### 3.3 Constraints
```
R(t) ≥ 125
C(t) ≥ 0
S_ORU(t) ≥ 0
```

---

## 4. RES Supply Equation

### 4.1 State Equation
```
S_RES(t+1) = S_RES(t) + 100·M(t)
```

Since:
```
1 successful RES mint event = 100 RES
```
internally:
```
S_RES(t+1) = S_RES(t) + 10,000·M(t)
```

### 4.2 Initial
```
S_RES(0) = 0
```

### 4.3 Bound
```
S_RES(t) ≤ MAX_RES
```
where:
```
MAX_RES = 21,000,000
```

---

## 5. Converge Definition

For each block:
```
C(t) = Σ converge_amount(tx_i)
for all valid Converge transactions in block t.
```

Constraint:
```
C(t) ≥ N(t) · MIN_CONVERGE
```
where:
```
MIN_CONVERGE = 1 ORU
```

---

## 6. RES Mint Rule

A valid RES event occurs iff:
```
M(t)=1 iff:
|F(t)| ≥ 1
AND winner exists
AND exactly one valid RES mint tx exists
AND supply cap not exceeded
```

Otherwise:
```
M(t)=0
```

---

## 7. Global System Equation

The entire protocol can be written as:
```
S_ORU(t+1) = S_ORU(t) + R(t) - C(t)
S_RES(t+1) = S_RES(t) + M(t)
```

This yields:
```
Δ_ORU = + issuance - destruction
Δ_RES = + selection event
```

---

## 8. Economic Conservation

Define total economic energy:
```
E(t) = S_ORU(t) + α·S_RES(t)
```
where:
```
α = market conversion coefficient
```

This is not consensus-critical, only analytical.

---

## 9. Monotonicity Theorems

### Theorem 1 (RES Monotonicity)
```
S_RES(t+1) ≥ S_RES(t)
```

Proof:
```
M(t) ∈ {0,1}
thus:
ΔS_RES ≥ 0
```

### Theorem 2 (RES Boundedness)
```
S_RES(t) ≤ MAX_RES
```

By consensus rejection.

### Theorem 3 (ORU Lower Bound)
Since:
```
R(t) ≥ 125
```
then:
```
S_ORU(t+1) ≥ S_ORU(t) - C(t) + 125
```

ORU can decline only if:
```
C(t) > R(t)
```

---

## 10. Equilibrium Analysis

Define equilibrium:
```
C*(t) = R(t)
```

Then:
```
S_ORU(t+1) = S_ORU(t)
```

Three states exist:

**Inflationary**
```
C(t) < R(t)
then:
S_ORU increases
```

**Neutral**
```
C(t) = R(t)
then:
S_ORU stable
```

**Deflationary**
```
C(t) > R(t)
then:
S_ORU decreases
```

---

## 11. Convergence Theorem

### Theorem 4 (Long-term ORU Stabilization)

Because reward floor exists:
```
R(t) → 125
for large t.
```

Then system converges to:
```
C(t) ≈ 125
under rational market behavior.
```

Thus:
```
lim ΔS_ORU → 0
```

Interpretation:
ORU tends toward dynamic equilibrium.

---

## 12. Scarcity Theorem

### Theorem 5

RES issuance requires ORU destruction.

Thus:
```
ΔS_RES > 0 ⇒ C(t) > 0
```

Equivalent:
```
No Converge ⇒ No RES
```

---

## 13. Upper Bound of RES

Maximum possible RES:
```
S_RES ≤ 21,000,000
```

Thus:
```
lim S_RES = 21,000,000
```

---

## 14. Implementation Rules

Nodes MUST maintain:
```
nTotalRESSupply
nBlockConverge
nCurrentSubsidy
```

Update:
```
nTotalORU += subsidy;
nTotalORU -= blockConverge;
nTotalRES += mintEvent ? RES_REWARD : 0;
```

---

## 15. Canonical Verification

Reject if:
- negative ORU
- RES > cap
- multiple mint
- invalid converge accounting

---

## 16. Final Identity

The full system:
```
PoW creates ORU
Converge destroys ORU
Selection creates RES
```

Equivalent:
```
Work → Energy → Scarcity
```

---

## 17. Consensus Lock

Immutable:
```
S_ORU(t+1)=S_ORU(t)+R(t)-C(t)
S_RES(t+1)=S_RES(t)+M(t)
```

No alternative monetary evolution allowed.

---

## 18. Core Conclusion

RIGOR is not inflation-only.

It is a dual-state convergent monetary system:
- ORU = dynamic equilibrium asset
- RES = bounded scarcity asset

---

**END OF RFC-002**
