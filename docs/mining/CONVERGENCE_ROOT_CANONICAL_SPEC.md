# Convergence Root Canonical Specification

**Document status:** Production documentation  
**Authority:** `P03_LIVE_EQUIVALENCE_CERTIFICATION.md`, `convergence_root.cpp`  
**Implementation:** `core/src/consensus/` (`ComputeConvergenceRootFromEligibilityTxids`)

---

## 1. rigord rule (normative)

From `Consensus::ComputeConvergenceRootFromEligibilityTxids`:

```
C = Hash256( concat( sort_memcmp( txid_native_i ) ) )
```

| Rule | Value |
|------|-------|
| Input element | `uint256` **native** 32 bytes (`tx.GetHash()` storage layout) |
| Sort comparator | Unsigned byte-wise lexicographic (`memcmp` / `std::sort` on `uint256`) |
| Concatenation | Sorted order, no delimiters, no per-leaf pre-hash |
| Hash | `Hash256` = double SHA256 |
| Boundary | If `|F| < 2` → **C = 32 zero bytes** (no hashing) |

**F** is enumerated by `ListConvergenceRootCommitmentTxids` at block entry (not re-derived here).

---

## 2. Wire encodings

| Context | Txid hex format | Root hex format | Decode function |
|---------|-----------------|-----------------|-----------------|
| GBT `convergence_txids[]` | **Display** (`uint256::GetHex`) | — | `display_hex_to_native` |
| Job snapshot `convergence_root_hex` | — | **HashHex** (forward native bytes) | `hash_hex_to_native` |
| Golden CR vectors (test corpus) | **HashHex** | **HashHex** | `hash_hex_to_native` |
| Coinbase scriptSig push | — | **native bytes** (not hex) | direct `[u8;32]` |

### Display hex (GBT txids)

1. `hex::decode(display_string)` → 32 bytes in **display order**
2. `bytes.reverse()` → **uint256 native** (little-endian storage on LE platforms)

This matches C++ `uint256::SetHex` / `ParseHex256`.

### HashHex (roots in JSON / job snapshots)

1. `hex::decode(string)` → native bytes **without** reversal
2. Matches golden `phase3_golden_vectors.json` `ConvergenceRoot.expected.convergence_root`

---

## 3. Canonical implementation

Node consensus is the source of truth:
`Consensus::ComputeConvergenceRootFromEligibilityTxids` in
`core/src/consensus/`. GBT `convergence_txids[]` uses display hex
(`uint256::GetHex` / `SetHex`). There is no in-tree Pool crate on the
public tree.

---

## 6. Governance

| Field | Value |
|-------|-------|
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** (full P-03 stack gate unchanged) |
