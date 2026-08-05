# RFC-009: Canonical Serialization Specification

**Version:** 1.0
**Status:** Consensus-Critical
**Goal:** Eliminate all "serialization ambiguity", ensure bit-level consistency

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature**

---

## 0. Absolute Principle

All consensus-critical data MUST have exactly ONE valid byte representation.

Any alternative encoding is a consensus violation.

---

## 1. Basic Encoding Rules

### 1.1 Integer Encoding (Unique Rule)

```
uint32  → 4 bytes, little-endian
uint64  → 8 bytes, little-endian
uint256 → 32 bytes, raw (no endian reinterpretation)
```

### Example

```
height = 1000
LE32(height) = 0xE8 0x03 0x00 0x00
```

### Forbidden
- big-endian integers
- variable-length integers
- signed integers

---

## 2. txid / hash Representation

**txid:**
- MUST be 32 bytes
- MUST be used as raw byte array
- MUST NOT be reversed for internal computation

### Note (Bitcoin Common Pitfall)
Display (RPC) ≠ Internal representation
Consensus MUST use raw bytes

**Implementation**: Uses uint256 type with raw byte array representation, no reversal for internal computation.

---

## 3. Convergence Root (C)

### 3.1 Input
```
txids: array of bytes32
```

### 3.2 Sorting Rules (Unique)
```
Sort lexicographically by raw byte comparison:
compare(a, b):
    for i in 0..31:
        if a[i] < b[i]: return true
        if a[i] > b[i]: return false
```

**Implementation**: Uses `std::sort` on uint256, which uses memcmp byte order (platform-independent).

### 3.3 Construction
```
buf = txid_1 || txid_2 || ... || txid_n
C = Hash256(buf)
```

**Implementation**: Uses CHash256() for byte-wise concatenation and double SHA256.

### 3.4 Empty Set
```
if n == 0:
    C = 32 zero bytes
```

**Implementation**: Returns uint256::ZERO when N < 2.

### Forbidden
- Merkle tree
- pair hashing
- length prefix

---

## 4. Sigma Input Serialization (Most Critical)

### 4.1 Unique Format (100 bytes)

```
offset | size | field
----------------------
0      | 32   | txid
32     | 32   | C
64     | 32   | B_{h-k}
96     | 4    | LE32(height)
```

### Mandatory Constraints
Total size MUST be EXACTLY 100 bytes

### Forbidden
- varint
- extra padding
- field reordering

---

## 5. Proof Serialization (Converge Proof)

### 5.1 Structure

```
[0] converge_txid       (32)
[1] convergence_amount (8, uint64 LE - RFC-000 extension)
[2] bind_hash           (32)
[3] height              (8, uint64 LE)
[4] target_prev         (4, uint32 LE)
[5] converge_root       (32)
[6] proof_blob          (var, raw bytes)
[7] signature           (64)
[8] pubkey              (32, Ed25519 - RFC-000 extension)
```

### Rules
- No length prefix inside fixed fields
- proof_blob is opaque
- signature MUST be exactly 64 bytes

**Note**: Current implementation includes additional fields (convergence_amount, pubkey) as RFC-000 extensions. Per RFC-000-FINAL hierarchy, these extensions override RFC-009 base structure.

---

## 6. f_i Encoding

f_i MUST be uint64 (little-endian)

### Definition

**Mint:**
```
f_i = Mint Cost (base units)
```

**Spend:**
```
f_i = fee (base units)
```

### Forbidden
- float fee
- scaled fee
- weight normalization

**Implementation**: Uses uint64_t for weights, encoded as little-endian when serialized.

---

## 7. Comparison Input (Cross Multiplication)

```
Left  = H_i * f_j
Right = H_j * f_i
```

### Serialization Rules
- H_i MUST be uint256
- f_i MUST be uint64
- multiplication MUST promote to uint512

---

## 8. Consistency Requirements (Most Important)

For identical block input:
ALL nodes MUST produce:
- identical C
- identical H_i
- identical winner

---

## 9. Forbidden Behaviors

- Auto endian conversion
- Using std::sort default (platform dependent)
- Using big integer library without fixed width
- Hash input as struct serialization

---

**END OF RFC-009**
