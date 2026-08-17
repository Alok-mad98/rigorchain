# Consensus Fingerprint Library

| Field | Value |
|-------|-------|
| **Task** | `TASK-ALPHAPLUS-CONSENSUS-FINGERPRINT-LIBRARY-001` |
| **Layer** | Verification / observability (not consensus) |
| **Consensus impact** | **NONE** |
| **API** | `ComputeConsensusFingerprint()` |

---

## Purpose

The Consensus Fingerprint Library publishes a **deterministic verification identity** over a small set of already-public block fields.

It exists so that:

- Explorers can display a stable digest next to a height
- Zero-trust web / WASM verifiers can recompute the same digest locally
- External tools can compare fingerprints without trusting a node, indexer, or API verdict

The library does **not** decide whether a block is valid. Block acceptance, PoW, RES allocation, fees, and wallet accounting are unchanged and do not call this function.

```
Public committed fields
        ↓
Native concat (fixed field order)
        ↓
Hash256  →  ConsensusFingerprint
        ↓
Compare / display / archive
```

---

## Deterministic input fields

`ComputeConsensusFingerprint` takes exactly five inputs, in this order:

| Order | Field | C++ type | Role in this library |
|------:|-------|----------|----------------------|
| 1 | ConvergenceRoot | `uint256` | Public commitment already bound in the block |
| 2 | winner_txid | `uint256` | Public winner transaction id |
| 3 | Sigma | `uint256` | Public selection digest for that winner |
| 4 | height | `uint32_t` | Block height |
| 5 | target_prev | `uint32_t` | Parent compact target (`nBits`) |

This library does **not** define how those fields are produced. Callers supply the public values. Field order is part of the fingerprint contract and MUST NOT change.

---

## Encoding rules

Encoding is **native serialization only**.

Forbidden in the preimage:

- String encoding
- JSON encoding
- Hex concatenation
- Delimiters, length prefixes, or padding

Canonical preimage (104 bytes):

```
offset  size  field              encoding
------  ----  -----------------  ----------------------------
0       32    ConvergenceRoot    uint256 native bytes
32      32    winner_txid        uint256 native bytes
64      32    Sigma              uint256 native bytes
96      4     height             uint32 little-endian
100     4     target_prev        uint32 little-endian
```

`uint256` native bytes are the in-memory / `Serialize()` order (`begin()` … `end()`), which is also `HexStr` / digest-order hex. This is **not** Bitcoin `GetHex()` display reversal.

`height` and `target_prev` use the same little-endian `uint32` encoding as the rest of the node (`WriteLE32`).

```
ConsensusFingerprint =
    Hash256(ConvergenceRoot || winner_txid || Sigma || height || target_prev)
```

`Hash256` is the existing RIGOR implementation: `CHash256` = SHA-256(SHA-256(·)). No other hash is permitted in this library.

---

## Security motivation

A fingerprint is a **comparison identity**, not an authority.

| Property | Why it matters |
|----------|----------------|
| Deterministic | Same public bytes always produce the same 32-byte digest |
| Binary-first | Independent implementations (C++, Python, WASM) can match without locale or JSON canonicalization bugs |
| Stateless | The function has no chain state, cache, or “already valid” shortcut |
| Untrusted publishers | An explorer or RPC may *publish* a fingerprint; a verifier *recomputes* it and compares |
| No consensus coupling | A wrong fingerprint cannot accept or reject a block |

This supports zero-trust verification: a third party recomputes the digest from public fields and checks equality. Equality means the published identity matches the local bytes. It does not mean the caller trusted the publisher’s validity claim.

The fingerprint is not a substitute for full-node validation.

---

## Compatibility guarantees

| Surface | Guarantee |
|---------|-----------|
| Consensus rules | Unchanged. This library is not on the acceptance path. |
| `CTxOut` / UTXO storage | Unchanged. |
| Genesis parameters | Unchanged. |
| PoW | Unchanged. |
| RES issuance / selection | Unchanged. |
| Fee rules | Unchanged. |
| Wallet accounting | Unchanged. |
| Kernel / `validation.cpp` / `pow.cpp` | Not modified by this library. |
| Field order | Frozen for this library. A new order would be a new function, not a silent change. |
| Hash | Frozen to existing `CHash256`. |

Existing consensus sources under `core/src/consensus/` other than the new `fingerprint.h` / `fingerprint.cpp` pair are not edited. Those two files are a verification add-on; they do not alter script, amount, or block rules.

---

## Reusable API

```cpp
#include <consensus/fingerprint.h>

uint256 ComputeConsensusFingerprint(
    const uint256& convergence_root,
    const uint256& winner_txid,
    const uint256& sigma,
    uint32_t height,
    uint32_t target_prev);
```

Intended future consumers (not implemented in this task):

- Explorer display
- Zero-trust web verifier
- WASM verifier
- External audit tools

This task adds no UI.

Published test vectors: `test_vectors/fingerprint_vectors.json`.

---

## What this document does not describe

This document does not specify how ConvergenceRoot, Sigma, or a winner are computed. Those remain the existing consensus functions. The fingerprint library only hashes the five public fields it is given.
