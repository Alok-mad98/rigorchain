# RIGOR Mining Pool Integration Guide v1.0

**Audience:** third-party pool operators  
**Status:** implementation-locked against `rigord` consensus PoW  
**Node RPC (mainnet):** `8761`

This document specifies how to accept RIGOR work, score shares, and submit blocks. Consensus acceptance is performed only by the node (`CheckProofOfWork` + `submitblock`).

---

## 1. Algorithm Identification

| Item | Value |
|------|--------|
| Algorithm name (wire / miner / Stratum) | `rgrpow01` |
| Human-readable name | OpuHash |
| Hash | `SHA3-256("RGRPOW01" \|\| header_80)` |
| Primitive | FIPS PUB 202 SHA3-256 |
| Domain suffix | `0x06` |
| Preimage length | 88 bytes |
| Digest length | 32 bytes |

```
OpuHash(header_80) = SHA3-256("RGRPOW01" || header_80)
```

`"RGRPOW01"` is 8 ASCII bytes: `52 47 52 50 4F 57 30 31`.

`mining.subscribe` user-agent **must** contain the substring `rgrpow01` (case-insensitive).

### Forbidden constructions

Do **not** implement any of:

| Forbidden | Why it fails |
|-----------|----------------|
| `SHA3-256(header_80)` | missing domain tag; digest does not match the node |
| Keccak-256 (Ethereum, suffix `0x01`) | different padding; silent mismatch |
| `SHA256d(header_80)` as PoW | SHA256d is block **identity** only |
| Retired 64 KiB OpuHash mix (`GetOpusHash`) | not live consensus |

The Stratum identifier `opushash` is **rejected** by the reference pool. Do not advertise it.

---

## 2. Block Header Specification

RIGOR keeps a Bitcoin-compatible **80-byte** block header. Layout is the canonical `CBlockHeader` serialization (`core/src/primitives/block.h`).

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0–3 | 4 | `version` | uint32 little-endian |
| 4–35 | 32 | previous block hash | uint256 internal byte order |
| 36–67 | 32 | merkle root | uint256 internal byte order |
| 68–71 | 4 | `ntime` | uint32 little-endian |
| 72–75 | 4 | `nBits` | uint32 little-endian |
| 76–79 | 4 | `nonce` | uint32 little-endian |

Notes:

- Internal byte order for the two hashes is the Bitcoin Core `uint256` serialization (little-endian integer). GBT/RPC hex (`previousblockhash`, `merkleroot`, txid) is **display-endian** and must be **byte-reversed** before writing into the header.
- `nBits` in the header is the compact target as a little-endian uint32. GBT `bits` is 8-digit **big-endian hex** of that same compact value (example: `1d008000` → header bytes `00 80 00 1d`).
- Merkle root is SHA256d over **txids** (witness stripped), not wtxids.
- Block identity remains `SHA256d(header_80)` via `CBlockHeader::GetHash()`. OpuHash is **not** the block hash.

---

## 3. PoW Calculation

```
function OpuHash(header_80):
    # header_80: exactly 80 bytes, Bitcoin header serialization
    input = "RGRPOW01" + header_80     # 88 bytes
    return SHA3_256(input)             # FIPS 202, suffix 0x06, 32 bytes
```

SHA3-256 parameters:

| Parameter | Value |
|-----------|--------|
| Permutation | Keccak-f[1600], 24 rounds |
| Rate | 136 bytes |
| Capacity | 512 bits |
| Domain suffix | `0x06` |
| Padding for this input | `preimage \|\| 0x06 \|\| 46×0x00 \|\| 0x80` (one rate block) |

88 bytes is below the SHA3-256 rate. Implementations may special-case a single permutation. The result **must** match a general-purpose FIPS SHA3-256 of the same 88 bytes.

### Test vector (mandatory)

All-zero 80-byte header:

```
header_80  = 00 × 80
preimage   = 524752504f573031 || 00 × 80
digest     = dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc
```

- Digest byte 0 is `0xdd` (least-significant limb vs target).
- Display hex (`uint256.ToString()`) is `fc04ded6f5d9f677b57536b4751c512d9e817b9dcc34e866945b8fdfc17f05dd`.

Negative checks (must **not** equal the vector):

1. `SHA3-256(00 × 80)` — no domain tag
2. Keccak-256 (`0x01` suffix) of the 88-byte preimage

Typed-header vector (field-order / endianness): see `core/src/test/rigor_pow_digest_tests.cpp` (`TypedHeader` / `pow_digest_typed_header_vector`).

---

## 4. Target Validation

```
valid(header, nBits)  ⇔  Uint256_LE(OpuHash(header)) <= target(nBits)
```

Node code (`core/src/pow.cpp::CheckProofOfWork`): reject if `UintToArith256(pow_hash) > bnTarget`. Equality is accepted.

### Endian rules

| Object | Rule |
|--------|------|
| Header uint32 fields | little-endian |
| Header prev / merkle | internal byte order (RPC hex reversed) |
| PoW digest | SHA3-256 output order; byte 0 = LSB of the compared integer |
| Target compare | little-endian uint256, same convention as Bitcoin `UintToArith256` |
| Do **not** byte-reverse the digest before compare | reversing the digest will reject valid shares |

### Compact target (`nBits`)

Decode with Bitcoin `SetCompact`:

```
target = SetCompact(nBits)
reject if negative OR zero OR overflow OR target > powLimit
```

Mainnet `powLimit` is `0000007fffffffffffffffffffffffffffffffffffffffffffffffffffffffff`.

Network `nBits` comes from GBT `bits`. The node computes it with ASERT (aserti3-2d, parent-time). Rolling `ntime` does **not** change `nBits`. Pools must not recompute difficulty from the child timestamp.

Genesis: the node skips the digest check when `SHA256d(header_80)` equals the genesis id. Pools do not need a genesis exception for live share scoring.

### Share target vs block target

| Check | Predicate |
|-------|-----------|
| Share | `Uint256_LE(digest) <= share_target` |
| Block candidate | `Uint256_LE(digest) <= ExpandCompact(job.nBits)` |

`share_target` is pool policy (`mining.set_difficulty`). A share that also meets the network target is a block candidate and must be assembled and sent to `submitblock`.

Difficulty 1 share target mapping is pool-local, but the **byte order of the compared digest is not**. Always compare the raw SHA3-256 output as a little-endian 256-bit integer.

---

## 5. Mining Template Interface

Supported node RPC:

| Method | Role |
|--------|------|
| `getblocktemplate` | job source |
| `submitblock` | block submission |

Mainnet RPC port: **8761** (not Bitcoin `8332`).

### `getblocktemplate`

Required request:

```json
{
  "rules": ["segwit"],
  "coinbase_address": "<ORU witness-v0 address, rg1q…>"
}
```

`rules: ["segwit"]` is mandatory. Without `coinbase_address`, the node **omits** `default_coinbase_tx` / `merkleroot` (placeholder payout). Pools must supply the pool payout address.

Required response fields:

| Field | Use |
|-------|-----|
| `version` | header bytes 0–3 |
| `previousblockhash` | RPC hex; reverse to header bytes 4–35 |
| `transactions` | non-coinbase txs (`data`, `txid`, `hash`) |
| `bits` | compact nBits, 8 hex digits |
| `target` | expanded 256-bit target, hex |
| `height` | next block height |
| `curtime` | starting `ntime` |
| `default_coinbase_tx` | witness-serialized node coinbase; copy scriptSig / payout / witness commitment |

Additional fields the pool must honor:

| Field | Rule |
|-------|------|
| `default_coinbase_scriptsig` | exact `vin[0].scriptSig`; copy; do not re-encode |
| `default_coinbase_txid` | cross-check of `default_coinbase_tx` |
| `merkleroot` | merkle of the **unmodified** template (before pool OP_RETURN) |
| `default_witness_commitment` | BIP141 commitment for the unmodified tx set |
| `mintime` / `maxtime` | allowed `ntime` window |
| `mutable` | node advertises `["time"]` only |
| `noncerange` | `00000000ffffffff` |
| `rigor_coinbase_immutable` | always `true` |
| `coinbasevalue` | ORU amount on coinbase `vout[0]` (atoms) |
| `longpollid` | template invalidation |

Do not write extranonce into `coinbaseaux`. That object is empty.

### `submitblock`

```
submitblock "<block hex>"
```

`block hex` = 80-byte header + compact-size tx count + witness-serialized transactions (coinbase first).

The node identifies the block by `SHA256d(header_80)` and validates work with OpuHash. BIP22 dummy second argument is ignored.

---

## 6. Coinbase Rules

This is the primary incompatibility with Bitcoin pools.

### Not allowed: scriptSig extranonce

RIGOR does **not** allow extranonce in the coinbase `scriptSig`.

Bitcoin pool construction:

```
scriptSig = height || extranonce1 || extranonce2
```

**Not compatible.** The node will reject the block.

Consensus `scriptSig` is:

```
scriptSig = BIP34(height) || OP_0
```

There are **no free bytes**. `default_coinbase_scriptsig` must be copied verbatim. `rigor_coinbase_immutable` is `true`.

### Required: OP_RETURN extranonce

Stratum extranonce **must** be placed in a `value = 0` `OP_RETURN` output.

```
scriptPubKey = OP_RETURN || OP_PUSH(en_gap) || extranonce1 || extranonce2
```

Reference sizes: `extranonce1 = 4` bytes, `extranonce2 = 4` bytes, `en_gap = 8`.

`coinb1` / `coinb2` split around that push payload so the classic splice still works:

```
coinbase = coinb1 || extranonce1 || extranonce2 || coinb2
```

The gap is the OP_RETURN payload, **not** the scriptSig.

### Why

`vin[0].scriptSig` is consensus-immutable (BIP34 height + `OP_0` only). Extra `OP_RETURN` outputs are ignored by consensus and are the only Stratum-compatible grind surface besides `nonce` (and `ntime` within `[mintime, maxtime]`).

### Pool coinbase assembly

1. Copy `default_coinbase_scriptsig`.
2. Keep `vout[0]` as the GBT ORU payout (`coinbasevalue`, `coinbase_address` / `default_coinbase_tx`).
3. Keep the BIP141 witness commitment if present. Witness reserved value stays **32 zero bytes**.
4. Append the OP_RETURN extranonce output.
5. Do **not** add mint / extra subsidy outputs.
6. Recompute merkle root from the **modified** coinbase txid + GBT `transactions[].txid`. Do not reuse GBT `merkleroot` after adding OP_RETURN.

Witness commitment is a function of wtxids. An OP_RETURN-only extra output does not change the witness merkle if the reserved value stays zero; it **does** change the txid merkle leaf.

---

## 7. Stratum V1 Specification

Transport: Stratum V1 JSON over TCP. Reference listen port: `3333`.

### Subscribe

```json
{"id": 1, "method": "mining.subscribe", "params": ["<miner>/1.0-rgrpow01", null]}
```

`params[0]` **must** contain `rgrpow01`. Agents advertising `opushash` are rejected.

Result:

```json
{
  "id": 1,
  "result": [
    [
      ["mining.set_difficulty", "<sid>"],
      ["mining.notify", "<sid>"]
    ],
    "<extranonce1_hex>",
    4
  ],
  "error": null
}
```

`extranonce2_size` is 4 in the reference pool (range 1–8).

### Set difficulty

```json
{"id": null, "method": "mining.set_difficulty", "params": [<difficulty>]}
```

### Notify

```json
{
  "id": null,
  "method": "mining.notify",
  "params": [
    "<job_id>",
    "<prevhash>",
    "<coinb1>",
    "<coinb2>",
    ["<merkle_branch>", "..."],
    "<version>",
    "<nbits>",
    "<ntime>",
    true
  ]
}
```

| Index | Field | Encoding |
|-------|-------|----------|
| 0 | `job_id` | string |
| 1 | `prevhash` | 64 hex chars of header bytes 4–35 (internal order). Reference pool: byte-reverse of GBT `previousblockhash`. Not Bitcoin SHA256d. |
| 2 | `coinb1` | hex prefix up to the extranonce gap |
| 3 | `coinb2` | hex suffix after the extranonce gap |
| 4 | `merkle_branch` | array of 32-byte hex nodes; SHA256d pairing |
| 5 | `version` | 8 hex digits |
| 6 | `nbits` | 8 hex digits, GBT `bits` |
| 7 | `ntime` | 8 hex digits |
| 8 | `clean_jobs` | boolean |

Miner header reconstruction:

```
coinbase = coinb1 || extranonce1 || extranonce2 || coinb2
merkle   = SHA256d_merkle(txid(coinbase), merkle_branch)
header   = version_le || prevhash_bytes || merkle || ntime_le || nbits_le || nonce_le
digest   = OpuHash(header)
```

### Submit

```json
{
  "id": 4,
  "method": "mining.submit",
  "params": [
    "<worker>",
    "<job_id>",
    "<extranonce2>",
    "<ntime>",
    "<nonce>"
  ]
}
```

| Index | Field | Encoding |
|-------|-------|----------|
| 0 | `worker` | authorized worker name |
| 1 | `job_id` | from notify |
| 2 | `extranonce2` | hex, length = `extranonce2_size` |
| 3 | `ntime` | 8 hex digits |
| 4 | `nonce` | 8 hex digits |

`extranonce2` is required. It fills the OP_RETURN gap. A submit that only carries `worker`, `job_id`, `nonce`, `ntime` cannot reconstruct the coinbase.

Reference validator currently requires `ntime` to equal the job `ntime` (no share-time rolling). If a pool allows rolling, it must still keep `ntime` inside GBT `[mintime, maxtime]` and must **not** change `nBits`.

---

## 8. Reference Implementation

| Role | Path | Function / binary |
|------|------|-------------------|
| Node | `core/` | `rigord` |
| Consensus PoW | `core/src/rigor_kernel/pow_digest.cpp` | `rigor_kernel::GetRigorPowHash` |
| SHA3 primitive | `core/src/rigor_kernel/keccak.cpp` | `Sha3_256Short` / `KeccakF1600` |
| PoW check | `core/src/pow.cpp` | `CheckProofOfWork` |
| GBT / submit | `core/src/rpc/mining.cpp` | `getblocktemplate`, `submitblock` |
| SOLO miner | `miner/` | `minerd` / `rigor-miner` (`-a rgrpow01`) |
| Hash (C) | `miner/rgrpow.c` | `rgrpow_hash80` |
| Stratum miner | `pool/rigor-miner-agent/` | `rigor-miner-agent` |
| Share validator | `pool/share-validator/src/rgrpow.rs` | `rgrpow_hash80`, `verify_rgrpow_share` |
| Coinbase / OP_RETURN | `pool/block-assembler/src/{coinbase,script}.rs` | `build_extranonce_op_return` |
| Stratum server | `pool/stratum-server/` | `mining.subscribe` / `notify` / `submit` |

Do not use `pool/stratum/` (yiimp leftover).

SOLO `rigor-miner` speaks HTTP GBT only (`stratum+tcp://` is refused). Pool miners use `rigor-miner-agent`.

---

## 9. Pool Adapter Guide

Existing Bitcoin SHA256 / SHA3 / Keccak pools cannot be pointed at `rigord` unchanged. Required adapter work:

### 1. Replace the hash kernel

```
digest = SHA3-256("RGRPOW01" || header_80)   # FIPS, 0x06
compare Uint256_LE(digest) <= target
```

Remove:

- SHA256d as PoW
- SHA3-256 of the bare 80-byte header
- Keccak-256 (`0x01`)

Keep SHA256d for merkle, txid, and block id.

### 2. Support the `rgrpow01` identifier

- Advertise algorithm `rgrpow01`.
- Require it in `mining.subscribe`.
- Reject `opushash`, `sha256d`, `keccak`, `sha3-256` user-agents unless they actually implement this preimage.

### 3. Support immutable coinbase scriptSig

- Copy `default_coinbase_scriptsig`.
- Never append extranonce, pool signature, or arbitrary bytes to `scriptSig`.
- Treat GBT `mutable` as `time` only.

### 4. Support OP_RETURN extranonce

- Split `coinb1`/`coinb2` on an `OP_RETURN` push, not on `scriptSig`.
- Recompute merkle after splice.
- Keep BIP141 reserved witness at 32 zero bytes.

### 5. GBT / submit wiring

- Call `getblocktemplate` with `rules: ["segwit"]` and `coinbase_address`.
- Submit full blocks with `submitblock` to `rigord:8761`.
- Use GBT `bits` as network target; do not derive nBits from child `ntime`.

---

## 10. Compatibility Matrix

| Feature | Bitcoin | RIGOR |
|---------|---------|-------|
| Header size | 80 bytes | 80 bytes |
| Header fields | version, prev, merkle, ntime, nBits, nonce | same |
| PoW hash | SHA256d(header_80) | SHA3-256(`"RGRPOW01" \|\| header_80`) |
| Hash primitive | SHA-256 | FIPS SHA3-256 (suffix `0x06`) |
| Block ID | SHA256d(header_80) | SHA256d(header_80) |
| Merkle / txid | SHA256d | SHA256d |
| PoW vs identity | same function | **different functions** |
| Valid work | SHA256d ≤ target | OpuHash ≤ target (LE uint256) |
| GBT | yes | yes (`rules: ["segwit"]` required) |
| `submitblock` | yes | yes |
| Coinbase `scriptSig` extranonce | standard | **forbidden** |
| Extranonce location | `scriptSig` | `OP_RETURN` output |
| Coinbase mutability | BIP22 `coinbase` mutable | `rigor_coinbase_immutable: true` |
| GBT `mutable` | time, coinbase, … | `["time"]` only |
| Algorithm name | `sha256d` | `rgrpow01` |
| Mainnet RPC | 8332 | 8761 |
| Difficulty retarget | nBits from child time (Bitcoin) | ASERT on parent time; nTime roll does not change bits |

---

## 11. Final Integration Checklist

Pool operator:

- [ ] Implement `rgrpow01` = `SHA3-256("RGRPOW01" || header_80)` with FIPS suffix `0x06`
- [ ] Verify the all-zero header vector (`dd057fc1…de04fc`) and the no-tag / Keccak-256 negative cases
- [ ] Compare `Uint256_LE(digest) <= target` (do not reverse the digest)
- [ ] Support RIGOR GBT: `rules: ["segwit"]`, `coinbase_address`, copy `default_coinbase_tx` / `default_coinbase_scriptsig`
- [ ] Keep coinbase `scriptSig` immutable (BIP34 height + `OP_0` only)
- [ ] Place extranonce in an `OP_RETURN` output; splice `coinb1`/`coinb2` there
- [ ] Recompute merkle after OP_RETURN; keep witness reserved = 32 zero bytes
- [ ] Advertise Stratum algorithm / user-agent `rgrpow01`
- [ ] Accept `mining.submit` as `[worker, job_id, extranonce2, ntime, nonce]`
- [ ] Score shares against share target; promote network-target shares to blocks
- [ ] `submitblock` the full serialized block to `rigord` (mainnet `:8761`)
- [ ] Confirm the node accepts the block (`CheckProofOfWork` + connect)

Do not ship until the zero-header vector matches and a mined block is accepted by `submitblock`.
