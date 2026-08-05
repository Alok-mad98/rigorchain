# RFC-ARCH-000: Engineering Constitution

**Status:** FINAL LOCK
**Layer:** Engineering Constraints
**Companion:** [RFC-000 Consensus Constitution](./RFC-000-consensus-constitution.md)
**Immutability:** Any change to this document REQUIRES a project-wide review.
Violations of this document are **engineering consensus violations**.

---

## 0. Reader Contract

You are **NOT** designing.
You are implementing an immutable spec.

```
RFC  = LAW
Code = EXECUTION
```

You are implementing **RIGOR** on top of **Bitcoin Core v25**.

**STRICT RULES**

1. DO NOT redesign Bitcoin subsystems.
2. DO NOT modify networking / storage.
3. ONLY replace:
   - script system
   - transaction validation
   - block selection logic
4. ALL consensus must follow RFC-001 to RFC-016 **EXACTLY**.
5. ANY deviation = consensus bug.
6. Implementation must be:
   - deterministic
   - integer-only
   - byte-identical across platforms
7. If uncertain: **STOP and ask. DO NOT guess.**

---

## 1. Total Strategy (Most Critical)

### Core Principle

```
DO NOT rewrite Bitcoin Core.
DO NOT redesign subsystems.
ONLY replace consensus-critical parts.
```

The real objective:

> Let Bitcoin Core "believe it has not changed",
> while its consensus has been completely replaced.

### Technical Means

- Preserve `block` / `tx` structures.
- Reuse serialization.
- Replace validation.
- Replace script.

### One-Sentence Strategy

> Use **Bitcoin** as the **Operating System**.
> Use **RIGOR** as the **Kernel Rules**.

---

## 2. Three-Layer Refactor Model

Only three layers of the project are allowed to change.

### ① Consensus Layer (MUST rewrite)

- `validation` / `consensus` / `script` → **replaced by RIGOR Engine**.

### ② Transaction Semantics Layer (refactor)

- `script` → **StateTag**
- `asset` → **ORU / RES**

### ③ Light Adaptation Layer (wallet / RPC / P2P)

- Do not change structures.
- Only add fields when strictly necessary.

---

## 3. Untouchable Zones (Survival Zones)

The following MUST be fully reused. **No redesign, no rewrite.**

- P2P networking (`net/`, `net_processing`)
- Block propagation mechanism
- Mempool framework
- UTXO database (`coinsview`)
- LevelDB
- Serialization system

**Rationale:** these are engineering stability, not project innovation points.

---

## 4. Directory Blueprint

A new isolated layer is added under `core/src/`. **It MUST NOT pollute the original structure.**

```
core/src/
 ├── consensus_rigor/              <- all core logic
 │   ├── rigor_engine.h
 │   ├── rigor_engine.cpp
 │   ├── convergence_root.h
 │   ├── convergence_root.cpp
 │   ├── sigma_hash.h
 │   ├── sigma_hash.cpp
 │   ├── selection.h
 │   ├── selection.cpp
 │   ├── asset.h
 │   ├── asset.cpp
 │   ├── statetag.h
 │   ├── statetag.cpp
 │   ├── mint_validation.h
 │   └── mint_validation.cpp
 │
 ├── validation.cpp                <- takeover entry
 └── primitives/
      └── transaction.h            <- add asset field
```

**Isolation Rule:** `core/src/consensus_rigor/` MUST NEVER import or depend on any file outside the consensus-critical surface. No logging, no config, no RPC coupling inside this directory.

---

## 5. File-Level Surgery Checklist

### 5.1 Script System — **SEVER**

**Files involved**

- `core/src/script/`
- `core/src/script/interpreter.cpp`
- `core/src/script/script.cpp`

**Forbidden**

- "Fixing" the script system.
- Preserving any execution logic.

**Required**

```cpp
// interpreter.cpp
bool EvalScript(...) {
    return false; // never allowed to execute
}
```

**Effect:** Script is dead. **StateTag is the new truth.**

### 5.2 `validation.cpp` — Core Takeover Point

**File:** `core/src/validation.cpp`

Locate `bool CheckInputs(...)` and replace with:

```cpp
bool CheckInputs(...) {
    for (const auto& tx : block.vtx) {
        if (!Rigor::VerifyTransaction(tx, context)) {
            return false;
        }
    }
    return true;
}
```

### 5.3 New Module — `rigor_engine` (Master Entry)

```cpp
// rigor_engine.h
namespace Rigor {
    bool VerifyTransaction(const CTransaction& tx, const Context& ctx);
    bool VerifyBlock(const CBlock& block, const Context& ctx);
}
```

```cpp
// rigor_engine.cpp
bool Rigor::VerifyTransaction(const CTransaction& tx, const Context& ctx) {
    AssetType asset = GetInputAsset(tx);

    if (asset == ASSET_ORU) {
        if (IsRESMintTx(tx)) {
            return VerifyConvergeProof(tx, ctx);
        }
        return true;
    }

    if (asset == ASSET_RES) {
        return VerifyRESSpend(tx);
    }

    return false;
}
```

---

## 6. Core Module Implementation Breakdown

### 6.1 `asset.cpp` — Asset Domain Dispatch

```cpp
AssetType GetInputAsset(const CTransaction& tx) {
    // MUST be deterministic.
    // MUST NOT depend on witness structure.

    if (/* ORU marker */) return ASSET_ORU;
    if (/* RES marker */) return ASSET_RES;

    return ASSET_UNKNOWN;
}
```

### 6.2 `convergence_root.cpp`

```cpp
uint256 ComputeConvergenceRoot(std::vector<uint256>& txids) {
    if (txids.empty()) return uint256();

    std::sort(txids.begin(), txids.end());   // byte-wise lexicographical

    CHashWriter hasher(SER_GETHASH, 0);
    for (auto& id : txids) hasher << id;
    return hasher.GetHash();
}
```

### 6.3 `sigma_hash.cpp`

```cpp
uint256 ComputeSigmaHash(
    const uint256& txid,
    const uint256& C,
    const uint256& B_h_k,
    uint32_t       height)
{
    CHashWriter hasher(SER_GETHASH, 0);
    hasher << txid;
    hasher << C;
    hasher << B_h_k;
    hasher << height;        // LE u32
    return hasher.GetHash();
}
```

Input MUST be exactly **100 bytes** (`32 + 32 + 32 + 4`). See RFC-000 §5.

### 6.4 `selection.cpp` — The Inequality

```cpp
bool IsEligible(const uint256& H, uint256 T, uint64_t f) {
    arith_uint256 lhs = UintToArith256(H);
    arith_uint256 rhs = UintToArith256(T) * f;
    return lhs < rhs;
}
```

> **Implementation note (binding):** RFC-000 §13 requires **≥512-bit multiplication**.
> The illustrative code above MUST be upgraded in real implementation to a 512-bit
> wide multiplication (e.g. explicit 512-bit accumulator) to guarantee `T · f`
> never silently truncates. Any 256-bit wrap-around is a consensus bug.

### 6.5 Winner Selection

```cpp
bool IsBetter(const Candidate& a, const Candidate& b) {
    // Cross multiplication to avoid division.
    // Both products MUST use ≥512-bit arithmetic (RFC-000 §13, §14).
    arith_uint256 left  = a.H * b.f;
    arith_uint256 right = b.H * a.f;
    return left < right;
}
```

---

## 7. Block-Level Takeover

### 7.1 `validation.cpp → ConnectBlock`

Insert (deterministic, integer-only):

```cpp
std::vector<Candidate> candidates;

for (auto& tx : block.vtx) {
    if (IsConvergeTx(tx)) {
        Candidate c;
        c.txid = tx.GetHash();
        c.f    = GetMintCost(tx);   // integer, ≥ 1 (RFC-000 §7)
        candidates.push_back(c);
    }
}

uint256 C = ComputeConvergenceRoot(/* sorted txids of F */);

for (auto& c : candidates) {
    c.H = ComputeSigmaHash(c.txid, C, B_h_k, height);
}

Candidate winner = SelectWinner(candidates, T);

// Enforce exactly one RES mint.
CheckSingleRESMint(block, winner);
```

---

## 8. Wallet Layer (Minimal Change)

Wallet adaptation follows:

- `RFC-ADDRESS-001`
- `RFC-ADDRESS-002`

No additional wallet redesign is permitted.

---

## 9. P2P Layer (Virtually Untouched)

Only the following are allowed:

- Propagate new blocks
- Propagate transactions
- Do NOT change protocol

---

## 10. Testing Discipline (MANDATORY)

### 10.1 Test Vectors (RFC-006)

- Every vector MUST be executed.
- Output MUST be byte-identical.

### 10.2 Dual-Implementation Cross-Check

- C++ reference ↔ Python reference.
- Byte-level equivalence required.

### 10.3 Fuzzing

- Input perturbation → output MUST remain stable and deterministic.

---

## 11. 100-Year Stability Rules (Century Stability Guarantee)

1. **Consensus Isolation** — `core/src/consensus_rigor/` NEVER mixes with other logic.
2. **Constants hard-coded** — e.g.
   ```cpp
   static const uint64_t SIG_CAP = 21000000;
   ```
   Exact values are governed by consensus RFCs; engineering rule is: **no runtime source**.
3. **No configuration** — `NO config`, `NO runtime params`, `NO flags` that affect consensus.
4. **Determinism check** — every consensus function satisfies:
   `same input → same bytes output`, across platforms and compilers.

---

## 12. Forbidden List (Hard-coded for Implementers)

- Modifying the single inequality `H_i < T · f_i`.
- Using `float` / `double`.
- Introducing configurable parameters into the consensus path.
- "Optimizing" the consensus path in ways that alter bytes.
- Guessing at any RFC.
- Re-enabling script execution in any form.
- Adding logging / tracing inside `consensus_rigor/` that can influence control flow.

---

## 13. Execution Order (Final Execution Sequence)

| STEP | Task                                   |
|------|----------------------------------------|
| 1    | Disable Script (`EvalScript → false`)  |
| 2    | Introduce Asset model (ORU / RES)      |
| 3    | Implement ConvergenceRoot              |
| 4    | Implement Sigma Hash                   |
| 5    | Implement the Inequality               |
| 6    | Take over Block Validation             |
| 7    | Implement RES Mint                     |
| 8    | Wallet adaptation                      |
| 9    | Test vectors                           |
| 10   | Full-chain run                         |

---

## 14. Engineering Truth (Final Word)

> The hard part is not writing code.
> The hard part is:
> **Change 5% of the code. Change 100% of the consensus.**

Bitcoin Core gives you:

- battle-tested networking
- storage
- propagation

You replace:

- **Meaning**
- **Selection**
- **State**

---

## 15. Cross-Reference Lock

This document is the **engineering constitution**. It governs *how* code is written.
The **semantic constitution** is [RFC-000](./RFC-000-consensus-constitution.md), which
governs *what* the protocol means.

- Any engineering choice that contradicts RFC-000 is **INVALID**.
- Any RFC-000 statement that has no engineering realization MUST be scheduled here.
- Neither document may be weakened by the other.

**END OF RFC-ARCH-000.**
