# RFC-004: Canonical Serialization and Hashing Rules

**Version:** 1.0
**Status:** FINAL

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

---

## 1. Abstract

This document defines canonical byte-level serialization.

All nodes MUST produce identical bytes.

This defines:
- transaction serialization
- block serialization
- StateTag serialization
- ConvergeProof serialization
- ConvergeRoot serialization
- hash rules
- sort rules

---

## 2. Integer Rules

All integers:
- little-endian
- fixed-width

Types:
```
uint8
uint16
uint32
uint64
bytes32
varint
```

---

## 3. Hashing

Hash256:
```
SHA256(SHA256(data))
```

No alternatives.

---

## 4. Transaction Serialization

Order:
```
version        uint32
vin_count      varint
inputs[]
vout_count     varint
outputs[]
locktime       uint32
witness
```

---

## 5. Input

```
prev_txid      bytes32
vout_index     uint32
scriptSig_len  varint
scriptSig      bytes
sequence       uint32
asset_type     uint8
```

---

## 6. Output

```
amount         int64
asset_type     uint8
scriptPubKey_len
scriptPubKey
state_tag_len
state_tag
```

---

## 7. StateTag Serialization

```
type           uint8
param_len      varint
params         bytes
```

---

## 8. Converge Output

```
OP_RETURN
tag
amount
metadata
```

Bytes:
```
6a
01
uint64
varbytes
```

---

## 9. ConvergeRoot

Compute:
```
sort(txids)
concat
Hash256
```

If empty:
```
32 zero bytes
```

---

## 10. ConvergeProof Serialization

Exact order:
```
converge_txid      32
bind_hash          32
height             8
target_prev        4
converge_root      32
proof_blob         var
signature          64
```

---

## 11. Sigma Hash

```
txid
C
B_h-k
LE32(height)
```

Exactly:
```
100 bytes
```

---

## 12. Sorting

Lexicographic:
```
raw bytes
```

Never:
- big integer conversion
- hex string compare

---

## 13. Block Serialization

```
header
tx_count
transactions
```

Header:
```
version
prev_hash
merkle_root
time
bits
nonce
```

---

## 14. Signature Message

Ed25519 digest:
```
Hash256(tx_core || sighash || state)
```

---

## 15. Canonical Constraints

Must:
- same bytes
- same hash
- same root
- same winner

Reject:
- variable interpretation
- truncation
- padding
- float
- overflow

---

## 16. Reference

```
uint256 ComputeConvergeRoot();
uint256 ComputeSigmaHash();
```

**Note**: `SerializeStateTag` and `SerializeConvergeProof` functions are not currently implemented in codebase. StateTag and ConvergeProof use struct-based serialization per RFC-003 and RFC-000.

---

## 17. Consensus Lock

Immutable:
- byte order
- field order
- hash function
- length
- sort

Any change = hard fork.

---

**END OF RFC-004**
