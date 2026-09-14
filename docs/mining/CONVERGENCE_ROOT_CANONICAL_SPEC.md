# Convergence Root Canonical Specification

**Document status:** Production documentation  
**Authority:** `P03_LIVE_EQUIVALENCE_CERTIFICATION.md`, `convergence_root.cpp`  
**Implementation:** `pool/rigor-convergence-root/` (single source of truth)

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

## 3. Pool API (canonical)

```rust
use rigor_convergence_root::{
    compute_convergence_root,              // GBT display txids → C native
    compute_convergence_root_for_rigord_f, // GCR cardinality gate
    compute_convergence_root_from_native, // draw replay internal
    native_to_hash_hex,                    // C → job snapshot string
    display_hex_to_native,                 // GBT txid → native
    hash_hex_to_native,                    // job root string → native
};
```

### Publish path (block-assembler)

```text
GBT convergence_txids[] (display hex)
  → compute_convergence_root()
  → native C
  → native_to_hash_hex(C) → job.coinbase.convergence_root_hex
  → native C embedded in coinbase scriptSig
```

### Draw replay path (winner-engine)

```text
GBT txids → display_hex_to_native → F native
  → compute_convergence_root_from_native(F_native) → C
  → native_to_hash_hex(C) in DrawReplayResult.convergence_root_display
```

AUTH-03 binds authorization to `job.coinbase.convergence_root_hex` (assembler output).

### Mint path (mint-engine)

```text
convergence_root_hex (HashHex) → hash_hex_to_native → witness proof field
convergence_txid (display hex) → display_hex_to_native → witness proof field
```

---

## 4. Audit trail (pre-remediation)

| Location | Pre-020 encoding | Post-020 |
|----------|------------------|----------|
| `block-assembler/convergence.rs` | Literal hex (wrong) | `rigor-convergence-root` |
| `winner-engine/root.rs` | Duplicate impl | Re-export only |
| `mint-engine/sigma.rs` | `display_hex_to_le32` for root (wrong) | `hash_hex_to_native` for root |
| `share-validator` | via `block_assembler::compute_convergence_root` | unchanged import, fixed upstream |
| `stratum-server` | via block-assembler job snapshot | unchanged |

---

## 5. Live replay gate (122 blocks)

When `getconvergenceresult.convergence_root_set_size < 2`, rigord **C = 0**.  
Pool must use `compute_convergence_root_for_rigord_f(size, txids)` for equivalence — not raw `|txids|`.

Fixture: `pool/rigor-convergence-root/tests/fixtures/testnet_root_replay_122.json`  
Test: `live_root_replay_122.rs` — **122 / 122 PASS**

---

## 6. Governance

| Field | Value |
|-------|-------|
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** (full P-03 stack gate unchanged) |
