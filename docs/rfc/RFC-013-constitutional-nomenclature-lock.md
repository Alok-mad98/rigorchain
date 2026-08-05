# RFC-013: Constitutional Nomenclature Lock

**Version:** 1.1 (Sigma-PoW-01)
**Status:** CONSENSUS-CRITICAL
**Scope:** RFC-000 ~ RFC-016

> **v1.1 amendment (SECURITY-001-A8, codename Sigma-PoW-01)**: the σ definition quoted in
> §4.2 is restated to match RFC-000 §4 as amended. The name `bind_commit`, previously locked
> as the name of the third σ field, is **retired** — the concept it named no longer exists.
> No other locked name changes role. See `docs/security/SECURITY_001_A8_REPORT.md`.

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**

Follow RFC-013 for semantic meaning (consensus-critical)
Follow RFC-013-A for implementation enforcement (non-consensus)

---

## 1. Purpose

This document defines the final, immutable naming system of the protocol.

All identifiers defined herein:
ARE PART OF CONSENSUS

Any deviation:
= CONSENSUS VIOLATION

---

## 2. Fundamental Rule

Each concept MUST have exactly ONE name
Each name MUST refer to exactly ONE concept

---

## 3. Semantic Layer Separation (Absolute Rule)

The following roles MUST remain strictly separated:

### 3.1 State Commitment
### 3.2 Randomness
### 3.3 Selection Verification
### 3.4 Mint Path Verification
### 3.5 Economic Units
### 3.6 Identity Authentication

---

## 4. Final Nomenclature Definitions

### 4.1 ConvergenceRoot

**Symbol:** C

**Definition**
```
C = Hash256(concat(sort(txid_i for tx_i ∈ F)))
```

**Role**
Global state commitment of the Converge set F

**MUST be named**
"ConvergenceRoot"

**FORBIDDEN**
- "SelectionRoot"
- "MerkleRoot"
- "StateRoot"
- "ForgeRoot"

---

### 4.2 Sigma (σ)

**Definition**
```
σ_i = Hash256(txid_i || C || B_{h-1} || LE32(h))
```

`B_{h-1}` is the parent block hash and `h` is the payout height (RFC-000 §4, §5).

**Role**
Deterministic randomness projection

**MUST be named**
"Sigma" or "σ"

**FORBIDDEN**
- "RNG"
- "Random"
- "HashOutput"
- "SIGMA_OLD"
- "bind_commit" (retired name; see below)

Superseded by SECURITY-001-A8 (Sigma-PoW-01): the third field was previously written
`B_{h-k}` with `k = 10`, and under Sigma-CR-01 it was a derived value named `bind_commit`.
Both spellings are retired. The field is now the parent block hash and has no separate name.

---

### 4.3 SelectionProof

**Role**
Structure that proves eligibility in the selection process

**MUST be named**
"SelectionProof"

**FORBIDDEN**
- "RNGProof"
- "SIGMAProof"
- "ProofV1"

---

### 4.4 ConvergeProof

**Role**
Proof structure for mint path (ORU → RES)

**MUST be named**
"ConvergeProof"

**FORBIDDEN**
- "PCSIG"
- "MintSig"
- "ProofMint"
- "PCRNG"
- "FORGE"

---

### 4.5 ORU

**Role**
Energy unit (base asset)

**MUST be named**
"ORU"

**FORBIDDEN**
- "ORI"
- "ORU"
- "MTH"
- "Fuel"
- "Energy"

---

### 4.6 RES

**Role**
Derived economic asset (selection result)

**MUST be named**
"RES"

**FORBIDDEN**
- "SIG"
- "SIGMA"
- "RNG"
- "ORB"
- "Token"
- "Coin"

---

### 4.7 Signature

**Role**
Identity authentication

**MUST be named**
"Signature"

**FORBIDDEN**
- "Sig"
- "Auth"
- "IdentitySig"

---

## 5. Critical Distinction (Mandatory)

### 5.1 Data vs Proof

```
ConvergenceRoot = DATA (state commitment)
SelectionProof = PROOF (verification structure)
```

### 5.2 Absolute Rule

DATA MUST NEVER be named as PROOF
PROOF MUST NEVER be named as DATA

### 5.3 Formal Statement

```
ConvergenceRoot ≠ SelectionProof
```

---

## 6. Execution Semantics Binding

### 6.1 Mandatory Mapping

- Sigma → randomness only
- SelectionProof → selection validation
- ConvergeProof → mint validation
- Signature → identity validation

### 6.2 Forbidden Mixing

- Sigma used as proof
- Proof used as randomness
- Signature used as eligibility

---

## 7. Implementation Constraints

All implementations MUST:

1. Use exact names defined in this RFC
2. Reject any alias or synonym
3. Maintain identical naming in:
   - code
   - RPC
   - logs
   - documentation

---

## 8. Consensus Lock

This nomenclature is FINAL and IMMUTABLE

Any change:
REQUIRES HARD FORK

---

## 9. Implementation Verification Status

**Verification Date:** 2026-05-08

**Compliance Status:** COMPLIANT

### Verified Naming Usage

- ConvergenceRoot: Used correctly in codebase (validation.cpp, rpc/blockchain.cpp, rpc/mining.cpp)
- SelectionProof: Used correctly in codebase (consensus/statetag.cpp, consensus/statetag.h)
- ConvergeProof: Used correctly in codebase (consensus/res_proof.cpp, wallet/rpc/rpcspend.cpp)
- ORU: Used correctly in codebase
- RES: Used correctly in codebase
- Signature: Used correctly in codebase

### Forbidden Names Check

No instances of forbidden names found:
- SelectionRoot: Not found
- MerkleRoot: Not found (in consensus context)
- StateRoot: Not found
- ForgeRoot: Not found
- RNGProof: Not found
- SIGMAProof: Not found
- ProofV1: Not found
- PCSIG: Not found
- MintSig: Not found
- ProofMint: Not found
- PCRNG: Not found
- FORGE: Not found (in consensus context)

### Code Locations

- ConvergenceRoot: `./core/src/consensus/selection.cpp`
- SelectionProof: `./core/src/consensus/statetag.cpp`
- ConvergeProof: `./core/src/consensus/res_proof.cpp`

---

**END OF RFC-013**
