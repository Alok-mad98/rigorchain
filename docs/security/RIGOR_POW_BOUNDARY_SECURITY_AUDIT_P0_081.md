# RIGOR Proof-of-Work Boundary Security Audit (P0-081)

**Task:** `TASK-RIGOR-POW-BOUNDARY-SECURITY-AUDIT-P0-081`  
**Date:** 2026-08-31  
**Nature:** PoW-boundary security audit. Isolation of header serialization, PoW
digest, compact-target validation, and ASERT difficulty. This is not a SHA-256
mathematical security proof, not a PoW economic security proof, and not a
claim that 51% / majority-hashrate attacks are impossible.

This TASK does not modify protocol rules, PoW parameters, SHA256, SHA3, ASERT,
Consensus Kernel, Transaction Kernel, or Ledger. It does not add a second hash
path, a miner/wallet PoW authority, an RPC PoW override, or a legacy validation
path.

```
Block Header
        ↓
PoW Digest          GetRigorPowHash
        ↓
Target Validation   CheckProofOfWork
        ↓
ASERT Difficulty    GetNextWorkRequired
        ↓
Accept / Reject
```

```
RIGOR_POW_BOUNDARY_SECURITY_STATUS   = CLOSED
POW_AUTHORITY_COUNT                 = 1
HASH_AUTHORITY_COUNT                = 1
ASERT_AUTHORITY_COUNT               = 1
TARGET_VALIDATION_STATUS            = PASS
HEADER_SERIALIZATION_STATUS         = PASS
FUZZ_STATUS                         = PASS
CONSENSUS_BYPASS_STATUS             = NONE

PRODUCTION_CONSENSUS_MODIFIED = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE

REFINEMENT_PROVED                      = TRUE
  (inherited: ConsensusSurfaceClosure.pow_boundary_accept_iff_le;
   unchanged; no new theorem)

POW_DIGEST_MODEL_REFINEMENT_PROVED     = FALSE
EQUIVALENCE_PROVED                     = FALSE
SHA256_MATH_PROOF_CLAIMED              = false
POW_ECONOMIC_PROOF_CLAIMED             = false
FIFTY_ONE_PERCENT_IMPOSSIBLE_CLAIMED   = false

FINAL_VERDICT:
RIGOR_POW_BOUNDARY_SECURITY_AUDIT_CLOSED
```

Reproduce: `bash tools/run-pow-boundary-security-audit.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| unique PoW authority | Production accept/reject is `CheckProofOfWork` only (`pow.cpp`). Output is ACCEPT or REJECT. Lint: `tools/lint-pow-boundary-authority.py`. |
| unique PoW digest | `CheckProofOfWork` calls `rigor_kernel::GetRigorPowHash` only. Digest is SHA3-256(`"RGRPOW01" \|\| header_80`). The removed `CheckProofOfWork(uint256)` overload cannot return. |
| unique header identity | `CBlockHeader::GetHash` is SHA256d via `SerializeHash`. It identifies the block; it is not the PoW digest. |
| unique ASERT authority | Production next-bits is `GetNextWorkRequired` → `GetNextWorkASERT`. `CalculateNextWorkRequired` is Bitcoin-style tooling and is not called from `validation.cpp`. |
| deterministic header_80 | Six fixed-width fields, little-endian integers, raw 32-byte hashes. Any field or endian change changes the digest or is REJECT. |
| illegal compact target REJECT | Negative, overflow, zero, and above-`powLimit` nBits are REJECT. Digest > target is REJECT. |
| ASERT uniqueness | Identical `(previous difficulty state + timestamps)` yields one compact target. Child `nTime` is not an exponent input. |
| fail closed under mutation | Invalid nonce, fake target, malformed nBits, header mutation, and replay of an old header with a changed prev-hash cannot keep a previously valid PoW. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| SHA256 mathematical security proof | Collision / preimage resistance of SHA-256 (header identity) is assumed from the primitive. This audit does not prove FIPS 180. |
| SHA3-256 cryptographic proof | Keccak-f[1600] security is assumed. `PowDigest.v` is an executable model, not a cryptosystem proof. |
| PoW economic security proof | Hashrate cost, miner incentives, and equilibrium difficulty are out of scope. |
| 51% / majority-hashrate impossibility | A majority of honest work is an operational / economic assumption, not a theorem of this boundary. |
| C++ ↔ Coq digest equivalence | `EQUIVALENCE_PROVED = FALSE`. `PowDigest.v` states `refinement_proved = false` for the SHA3 model versus C++. |
| miner search correctness | Historical OpuHash (`pow_opus.cpp`, `miner/sha2.c`) is not a consensus authority. Consensus accepts only `GetRigorPowHash`. |

This note is a boundary + uniqueness + rejection audit. It inherits the
existing `PowKernelBoundary` accept-boundary
(`pow_boundary_accept_iff_le`: ACCEPT iff `digest_le_target`). It does not
add a theorem.

---

## 1. PoW authority inventory

Manifest: `formal_verification/pow_boundary_audit_manifest.json`

`POW_AUTHORITY_COUNT = 1`

The production PoW *decision* is a single function. Internal roles inside
that boundary are unique; they are not competing authorities.

| Role | Production symbol | Defining file | Notes |
|------|-------------------|---------------|-------|
| block hash calculation | `CBlockHeader::GetHash` | `core/src/primitives/block.cpp` | SHA256d identity. Not compared to target. |
| PoW digest | `GetRigorPowHash` | `core/src/rigor_kernel/pow_digest.cpp` | SHA3-256(`RGRPOW01` \|\| header_80) |
| target comparison | `CheckProofOfWork` | `core/src/pow.cpp` | compact decode + `digest <= target` |
| difficulty adjustment | `GetNextWorkRequired` | `core/src/pow.cpp` | calls `GetNextWorkASERT` |

`HASH_AUTHORITY_COUNT = 1` — `GetRigorPowHash` is the only production PoW
digest. `ASERT_AUTHORITY_COUNT = 1` — `GetNextWorkRequired` is the only
production next-bits function used by `ContextualCheckBlockHeader`.

Call graph at the kernel boundary:

```
header admission
    HasValidProofOfWork / ContextualCheckBlockHeader
        GetNextWorkRequired     (expected nBits; mismatch → bad-diffbits REJECT)
        CheckProofOfWork        (compact + digest; fail → high-hash REJECT)
ConnectBlock
        RigorConsensusKernel::EvaluateConnect
        (does not re-hash PoW; does not run ASERT)
```

`CheckBlockHeader` applies `CheckProofOfWork` only for the genesis header
id (compact sanity; work is exempt inside `CheckProofOfWork` when
`header_id == hashGenesisBlock`). Non-genesis work is checked at header
admission. ConnectBlock does not contain a second PoW path.

Forbidden (lint-enforced):

- second hash path used as consensus authority
- miner-side consensus authority (`node/miner.cpp` must not define the
  symbols above)
- wallet-side PoW calculation authority
- RPC PoW override (`GenerateBlock` searches via `CheckProofOfWork`;
  `submitblock` enters `ProcessNewBlock`)
- legacy PoW validation path (`GetOpusHash` has no production callers)

Residue, not authority:

- `pow_opus.cpp` remains linked. It is not called from `CheckProofOfWork`,
  `validation.cpp`, miner, wallet, or RPC.
- `CalculateNextWorkRequired` remains for tests / tooling. Validation uses
  `GetNextWorkRequired` only.
- Success cache inside `CheckProofOfWork` keys on header id + nBits.
  Failures are not cached.

PoW outputs only:

- ACCEPT (`return true`)
- REJECT (`return false`)

---

## 2. Block header serialization audit

RIGOR header is Bitcoin-shaped **header_80**. `CBlockHeader` fields:

| Offset | Field | Width | Encoding |
|--------|-------|-------|----------|
| 0 | version | 4 | little-endian int32 |
| 4 | previous block hash | 32 | raw bytes |
| 36 | merkle root | 32 | raw bytes |
| 68 | timestamp | 4 | little-endian uint32 |
| 72 | nBits | 4 | little-endian uint32 |
| 76 | nonce | 4 | little-endian uint32 |

`SERIALIZE_METHODS` lists exactly those six fields. There is no auxpow,
mix hash, extra nonce, or 64-bit nonce in the header. Extra nonce lives in
the coinbase and is bound by the merkle root.

`BuildPowPreimage` writes `"RGRPOW01"` then the same 80 bytes, field by
field. `rigor_pow_digest_tests` pins that layout against `CDataStream`.

| Mutation | Result |
|----------|--------|
| endian mutation (version / time / nBits / nonce) | digest changes |
| byte-order reversal of header_80 | digest changes |
| truncated header (< 80 bytes) | not a well-formed header; digest of zero-padded remainder differs or parse REJECT |
| oversized header | extra bytes after 80 are not in the PoW preimage |
| duplicate serialization | identical 80 bytes |
| ambiguous encoding | none: all fields fixed-width; no compact-size in the header |

`HEADER_SERIALIZATION_STATUS = PASS`  
`serialization deterministic = PASS`

---

## 3. Hash boundary audit

`GetRigorPowHash`:

1. unique input byte sequence: ASCII `RGRPOW01` (8) \|\| header_80
2. unique algorithm call: `Sha3_256Short` (one Keccak-f[1600])
3. no hidden conversion (no SHA256d midstate, no OpuHash mix)
4. no fallback hash

Domain separation from header identity:

- header id = SHA256d(header_80)
- PoW digest = SHA3-256(`RGRPOW01` \|\| header_80)

Zero-header golden vector is locked in `PowDigest.v`
(`pow_digest_zero_header`) and replayed by
`test_pow_boundary_security.cpp`.

Any of the following mutations cannot keep the same valid PoW result:

| Mutation | Result |
|----------|--------|
| nonce | digest changes |
| timestamp | digest changes |
| version | digest changes |
| previous hash | digest changes |
| merkle root | digest changes |
| nBits | digest changes (and expected-bits check is independent) |

---

## 4. Target validation audit

`CheckProofOfWork` compact decode (`arith_uint256::SetCompact`) then:

```
if (negative || target == 0 || overflow || target > powLimit) REJECT
if (header id == genesis) ACCEPT          // compact already checked
if (UintToArith256(GetRigorPowHash(header)) > target) REJECT
else ACCEPT
```

Comparison is little-endian: digest byte 0 is the least significant byte
(`PowDigest.uint256_of_bytes` / `UintToArith256`).

| Boundary | Result |
|----------|--------|
| target = 0 | REJECT |
| target maximum (`powLimit`) | legal compact; digest still must be `<=` |
| target minimum (1) | legal compact; almost all digests REJECT |
| overflow compact (`nSize` too large) | REJECT |
| negative interpretation (sign bit 0x00800000) | REJECT |
| compact target encoding | fail-closed before the hash |

Forbidden:

- overflow bypass
- signed/unsigned confusion (sign bit is REJECT, not a large unsigned target)
- truncation acceptance of an illegal compact

`TARGET_VALIDATION_STATUS = PASS`

Mainnet `powLimit` =
`0000007fffffffffffffffffffffffffffffffffffffffffffffffffffffffff`
(compact `0x1d7fffff` class). Fake easier nBits above that limit REJECT.

---

## 5. ASERT difficulty audit

Unique difficulty authority: `GetNextWorkRequired` → `GetNextWorkASERT`
(aserti3-2d). Constants remain in `rigor_consensus_lock.h` (this TASK does
not change them):

- `MAINNET_POW_ASERT_HALF_LIFE = 43200`
- `ASERT_ANCHOR_HEIGHT = 1`
- per-block clamp ±12.5% (`MIN/MAX_ADJUSTMENT_FACTOR_PPM`)

| Case | Behavior |
|------|----------|
| first block / genesis parent | parent height `<` anchor → parent is the anchor; height-1 nBits matches genesis bits on honest spacing |
| genesis transition | height 1 becomes the time/target anchor thereafter |
| normal interval | parent-time evaluation; unique compact output |
| fast blocks | time_error negative → smaller next target (harder), then clamp |
| slow blocks | time_error positive → larger next target (easier), then clamp to `powLimit` |
| timestamp manipulation | child `nTime` does not enter the exponent (`pblock` unused on mainnet). GBT ntime rolling cannot produce bad-diffbits |

Integer path: `__int128` exponent, `ASERT_EXP_CLAMP = 64 * 65536`, 384-bit
wide multiply, overflow → `powLimit`, zero next-target → 1. Same
`(previous difficulty state + timestamps)` produces one target.

`CalculateNextWorkRequired` is not this authority.

---

## 6. Attack vector test

`core/src/test/test_pow_boundary_security.cpp` (standalone + Boost):

| Vector | Result |
|--------|--------|
| invalid nonce | digest changes; previous solution does not ACCEPT |
| invalid hash | digest `>` target → REJECT |
| fake target | nBits above `powLimit` or ≠ ASERT expected → REJECT |
| malformed nBits | negative / overflow / zero → REJECT |
| timestamp manipulation | does not retarget the *current* block; next-block target is unique |
| header mutation | digest changes |
| replay old valid header | prev-hash (or any field) change destroys the digest |
| chainwork comparison attack | illegal compact contributes 0 work; fake easier nBits is not the ASERT target |

```
CRASH_FOUND = NO
CONSENSUS_BYPASS = NONE
UNDEFINED_BEHAVIOR = NONE
CONSENSUS_BYPASS_STATUS = NONE
```

---

## 7. Fuzz

`fuzz/pow_boundary/` feeds random header bytes, nBits, timestamp, nonce,
and previous-hash material into compact decode + SHA3-256 short +
`digest <= target`.

Malformed input is REJECT. The harness must not panic, crash, or take an
undefined decision.

```
FUZZ_STATUS = PASS
NO CRASH
NO UNDEFINED BEHAVIOR
NO ACCEPT_INVALID
```

---

## 8. Formal / document boundary

No new formal theorem is added.

Inherited, cited only:

| Artifact | Status |
|----------|--------|
| `ConsensusSurfaceClosure.PowKernelBoundary` | ACCEPT iff `digest_le_target`. Theorem `pow_boundary_accept_iff_le`. |
| `ConsensusSurfaceClosure.pow_decisions_use_pow_boundary` | PoW digest / ASERT / check registered on `FB_PowKernel`. |
| `PowDigest.v` | SHA3-256 model. File states `equivalence_claimed = false` and `refinement_proved = false`. |

```
REFINEMENT_PROVED = TRUE
  (inherited PowBoundary accept-boundary; unchanged; no new theorem)
POW_DIGEST_MODEL_REFINEMENT_PROVED = FALSE
EQUIVALENCE_PROVED = FALSE
```

This TASK does not claim SHA256 security, PoW economic security, or 51% / majority-hashrate impossibility.

`PRODUCTION_CONSENSUS_AUTHORITY = RigorConsensusKernel(C++)` / `EXTRACTED_KERNEL_PRODUCTION = FALSE` in
`core/src/consensus/kernel/genesis_authority.h`. The Kernel remains the
ConnectBlock issuance/supply/draw authority. It is not a second PoW
authority.

---

## Gates

```
bash tools/run-pow-boundary-security-audit.sh
```

Runs: authority lint, inherited PowBoundary assumption check, mutation /
fail-closed / ASERT uniqueness test, fuzz smoke.
