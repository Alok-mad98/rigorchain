# RIGOR Network Capability Model V2

Handshake is fail-closed. Capabilities are communication flags only.

```
VERSION
   |
RIGORCAP
   |
VERACK
   |
Capability-controlled transport
```

## Isolation

Capabilities:

- do not belong to consensus
- do not change state transition
- do not change transaction validity
- never enter ConnectBlock or the Script predicate
- never carry signature, predicate, or asset-rule upgrades

Feature enablement is `peer.HasCapability(CAP_xxx)`.  
Forbidden: `if (peerVersion >= X)` as a feature gate.

## Protocol vs serialization

| Constant | Role |
|----------|------|
| `RIGOR_PROTOCOL_MIN_VERSION` / `RIGOR_PROTOCOL_CURRENT_VERSION` | VERSION + RIGORCAP protocol numbers |
| `SERIALIZATION_VERSION` (`PROTOCOL_VERSION` = 70016) | `CDataStream` / `CNetMsgMaker` / compact encoding |

`GetCommonVersion()` after VERSION is `SERIALIZATION_VERSION`.

## Capability namespace

```cpp
enum class RigorCapability : uint64_t {
    CAP_NONE = 0,
    CAP_TX_RELAY       = 1 << 0,
    CAP_BLOCK_RELAY    = 1 << 1,
    CAP_ADDR_RELAY     = 1 << 2,
    CAP_COMPACT_BLOCK  = 1 << 3,
    CAP_TX_SYNC        = 1 << 4,   // named; not local-active
    CAP_RES_RELAY      = 1 << 5,   // named; not local-active
    CAP_FUTURE_RELAY_0 = 1 << 16,  // communication future only
    CAP_FUTURE_RELAY_1 = 1 << 17,
};
```

Local-active (advertised and implementable): TX, BLOCK, ADDR, COMPACT.

`CAP_TX_SYNC` has no salt-transport primitive without a per-feature command, so it is not advertised.  
`CAP_RES_RELAY` has no distinct inventory path; RES transactions use `CAP_TX_RELAY`. Not advertised.

`RIGORCAP` payload is exactly 12 bytes: `uint32 protocol_version` + `uint64 bitmap`. No `activation_epoch`.

## Enablement

```
enabled = advertised ∩ known ∩ local-active ∩ connection_mask
```

- Missing `RIGORCAP` before VERACK: disconnect.
- Empty bitmap (`CAP_NONE`): legal; enables nothing.
- Unknown bits: ignored (not enabled, not a disconnect).
- Duplicate / truncated / trailing-byte `RIGORCAP`: disconnect.

## Transport mapping

| Capability | Transport |
|------------|-----------|
| `CAP_TX_RELAY` | Witness transaction relay (`MSG_WTX` / `tx`) |
| `CAP_BLOCK_RELAY` | Block / headers / inv(block) |
| `CAP_ADDR_RELAY` | `addrv2` only |
| `CAP_COMPACT_BLOCK` | `cmpctblock` / `getblocktxn` / `blocktxn` / `MSG_CMPCT_BLOCK` |

Consensus upgrades live in the predicate registry, not in this bitmap.
