# RIGOR Network Capability Model (P0-029)

**Task:** `TASK-RIGOR-NETWORK-CAPABILITY-NEGOTIATION-ARCHITECTURE-AND-MIGRATION-P0-029-V2`  
**Phase:** 2–6 — design + implementation  
**Date:** 2026-08-27  

Companion audit: [`RIGOR_NETWORK_LEGACY_PROTOCOL_SURFACE_AUDIT_P0_029.md`](./RIGOR_NETWORK_LEGACY_PROTOCOL_SURFACE_AUDIT_P0_029.md).  
Upgrade classes: [`RIGOR_PROTOCOL_UPGRADE_MODEL_P0_029.md`](./RIGOR_PROTOCOL_UPGRADE_MODEL_P0_029.md).

---

## 0. Decision

Replace the Bitcoin **version gate** (`if (peerVersion >= X)`) with:

```
Protocol Version  +  Capability Advertisement  +  Feature Activation Boundary  +  Consensus Isolation
```

Capabilities:

- do **not** belong to consensus
- do **not** change state transition
- do **not** change transaction validity
- **only** control communication ability

---

## 1. Protocol version (separated from serialization)

| Constant | Value | Role |
|----------|------:|------|
| `PROTOCOL_VERSION` | 70016 | `CDataStream` / `GetSerializeSize` only. Must not collide with `SERIALIZE_TRANSACTION_NO_WITNESS` / `ADDRV2_FORMAT`. |
| `INIT_PROTO_VERSION` | 209 | Framing of the VERSION message itself (historical). |
| `RIGOR_PROTOCOL_MIN_VERSION` | 1 | Disconnect below this. |
| `RIGOR_PROTOCOL_CURRENT_VERSION` | 1 | Advertised in VERSION and RIGORCAP. |

Invariants (compile-time):

- `MIN <= CURRENT`
- `MIN != PROTOCOL_VERSION`
- `CURRENT != PROTOCOL_VERSION`

v1 node speaking to a future v2 node: `common = min(2, 1) = 1`. Connection lives. Features: `HasCapability()`.

**Forbidden:** `if (peerVersion >= XXX)` as a feature test.  
**Forbidden long-term:** `MIN_PEER_PROTO_VERSION == PROTOCOL_VERSION` (that alias is deleted).

---

## 2. `RigorCapability`

```cpp
enum class RigorCapability : uint8_t {
    BLOCK_RELAY_V1 = 0,
    TX_RELAY_V1 = 1,
    COMPACT_BLOCK_V1 = 2,
    RES_ASSET_RELAY_V1 = 3,
    FUTURE_SIGNATURE_V2 = 4,   // named; not local-active in v1
    FUTURE_PREDICATE_V2 = 5,   // named; not local-active in v1
};
```

Local-active bitmap (advertised): bits 0–3.  
Known mask: bits 0–5.  
Unknown bits (6–63): dropped on receipt.

`FUTURE_*` exist so a v2 advertisement is parseable. A v1 node **records** them and **does not enable** them. `unknown = enabled` is forbidden.

---

## 3. Peer storage

`CNodeCapabilities` on `Peer` (`net_processing.cpp`):

```cpp
bool HasCapability(RigorCapability cap) const;
void ApplyAdvertisement(uint32_t protocol_version, uint64_t bitmap, uint32_t epoch);
```

Missing `rigorcap` does not enable anything. VERACK without a prior advertisement disconnects.

Enablement:

```
enabled = advertised ∩ known ∩ local_active
```

`activation_epoch` is stored. v1 does not use it to turn on `FUTURE_*`. Epoch `0` = immediate for local-active caps.

---

## 4. Handshake message `rigorcap`

Command: `rigorcap` (≤12 bytes). Payload:

| Field | Type | v1 value |
|-------|------|----------|
| `protocol_version` | uint32 LE | `RIGOR_PROTOCOL_CURRENT_VERSION` |
| `capability_bitmap` | uint64 LE | `RIGOR_LOCAL_ACTIVE_CAPABILITIES` |
| `activation_epoch` | uint32 LE | `0` |

Window: after VERSION, before VERACK (same slot as `wtxidrelay` / `sendaddrv2`).

| Event | Action |
|-------|--------|
| Duplicate `rigorcap` | Disconnect |
| `rigorcap` after VERACK | Disconnect |
| Truncated payload | Disconnect (`size != 16`) |
| Extra trailing bytes | Disconnect (`size != 16`) |
| Missing `rigorcap` | VERACK disconnects. No baseline fill. |

Missing advertisement is **not** a consensus downgrade. An attacker who omits `RIGORCAP` is disconnected; they do not receive v1 relay by default.

---

## 5. How capabilities bind to live messages

v1 does **not** invent new block/tx encodings. Capabilities name the families already on the wire:

| Capability | Operational messages |
|------------|----------------------|
| `BLOCK_RELAY_V1` | `headers`, `getheaders`, `inv`, `getdata`, `block` |
| `TX_RELAY_V1` | `inv`/`getdata`/`tx`, `wtxidrelay`, `feefilter` |
| `COMPACT_BLOCK_V1` | `sendcmpct`, `cmpctblock`, `getblocktxn`, `blocktxn` |
| `RES_ASSET_RELAY_V1` | Same tx relay; RES outputs are ordinary txs under the classifier |

Compact-block **initiation** (`SENDCMPCT` at VERACK, HB selection) requires `HasCapability(COMPACT_BLOCK_V1)`. Incoming `block`/`tx` that already passed decode are **not** dropped solely for a missing bit — that would be an eclipse/DoS knob, not a safety feature.

`FUTURE_SIGNATURE_V2` / `FUTURE_PREDICATE_V2` have **no** operational messages in v1. They cannot change `VerifyScript`.

---

## 6. RPC

`getnetworkinfo`:

- `protocolversion` = `RIGOR_PROTOCOL_CURRENT_VERSION` (not 70016)
- `capabilities` = local-active names

`getpeerinfo`:

- `version` = peer VERSION field (RIGOR protocol version)
- `capabilities` = enabled names
- `capabilities_advertised` = whether `rigorcap` was received
- `compact_block_hb_to` / `compact_block_hb_from` = compact-block high-bandwidth state

---

## 7. Consensus isolation

```
Network Capability  ─x─►  Asset State
Network Capability  ─x─►  ConnectBlock
Network Capability  ─x─►  Script Predicate
```

Path that **is** allowed:

```
P2P Message → Decode → Validation Input → Consensus
```

Capability bits never appear in `validation.cpp` consensus bodies. Coq: `NetworkCapabilityBoundary.v` Theorem 1–3.
