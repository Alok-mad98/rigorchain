# RGRPOW01 Compatibility Specification

**Suite:** RGRPOW01 Compatibility Test Suite v1.0  
**Audience:** third-party pool, GPU miner, and adapter authors  
**Consensus authority:** `CheckProofOfWork` → `rigor_kernel::GetRigorPowHash`

This document tells you how to prove an implementation is byte-compatible with
RIGOR. It does not change consensus.

---

## 1. Algorithm

| Item | Value |
|------|--------|
| Algorithm name | `rgrpow01` |
| Human name | OpuHash |
| Hash | `SHA3-256("RGRPOW01" \|\| header_80)` |
| Preimage | exactly **88** bytes |
| Header | Bitcoin-compatible **80-byte** block header |
| Primitive | FIPS PUB 202 SHA3-256 (Keccak-f[1600], 24 rounds, suffix `0x06`) |
| Comparison | `Uint256_LE(digest) <= target` |

Domain tag ASCII `RGRPOW01` = `52 47 52 50 4F 57 30 31`.

Do **not** hash the bare header. Do **not** use Ethereum Keccak-256. Do **not**
use SHA256d as PoW (SHA256d remains block identity only).

---

## 2. Reference vectors

Canonical JSON: `tests/rgrpow_compat/vectors/rgrpow_vectors.json`  
Narrative: `docs/RGRPOW01_REFERENCE_VECTOR_v1.0.md`

| ID | Input | Expected digest (output order) |
|----|--------|--------------------------------|
| 001 | 80 zero bytes | `dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc` |
| 002 | typed Bitcoin-style header | `8b3571829ad202105cc7fc4b1c8beef1f7190e6bfdbf2f0ac54355e7b4f041f0` |
| 003 | valid PoW, `nBits=0x1f00ffff`, nonce `87954` | `79f6bdd8417f57d1c47683bcc1ca42f4a3b0eb0a2367056f4bf3563c70680000` |

Vector 001 `digest[0] = 0xdd` is the **least significant** byte vs target.

Python check:

```python
import hashlib
assert hashlib.sha3_256(b"RGRPOW01" + bytes(80)).hexdigest() == (
    "dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc"
)
```

---

## 3. Endian rules

Header:

- `version`, `ntime`, `nBits`, `nonce`: uint32 **little-endian**
- previous hash / merkle root: uint256 **internal** order (byte-reverse of RPC hex)

Digest:

```
valid ⇔ int.from_bytes(digest, "little") <= int.from_bytes(target, "little")
```

Reversing the digest before compare is a failed implementation (see vector 003).

---

## 4. Test command

From a RIGOR checkout:

```
python3 ./test_rgrpow_compat
```

Equivalent:

```
python3 tests/rgrpow_compat/run_suite.py
pytest tests/rgrpow_compat
```

The runner compiles:

- `miner/rgrpow.c` → `rgrpow_hash80`
- `core/src/rigor_kernel/keccak.cpp` → `Sha3_256Short`

and compares them to Python `hashlib.sha3_256` and the golden JSON. If `cargo`
is installed it also builds the Rust CLI (same construction as
`share-validator::rgrpow_hash80`).

---

## 5. Expected output

```
================================
RGRPOW01 Compatibility Test

Vectors:
PASS

Serialization:
PASS

Endian:
PASS

Cross Implementation:
PASS

Coinbase Rules:
PASS
================================
```

(The runner also prints Negative, RPC Mining, Stratum, and Fuzz groups.)

Exit code `0` = compatible. Non-zero = stop; the failing digest is printed.
Any mismatch between node, miner, and validator is a suite failure.

---

## 6. Minimum self-check without this tree

1. SHA3-256 empty message = `a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a`
2. Vector 001 digest as above; `digest[0] == 0xdd`
3. `SHA3-256(bytes(80))` ≠ OpuHash
4. `SHA3-256(b"RGRPOW00" + bytes(80))` ≠ OpuHash
5. Keccak-256 ≠ OpuHash
6. Assemble vector 003 header, confirm `Uint256_LE(digest) <= SetCompact(0x1f00ffff)`

If step 2 fails, the implementation is not RIGOR.
