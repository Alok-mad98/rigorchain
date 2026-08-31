# RIGOR P2P Handshake Final Model

## Order

```
VERSION  →  RIGORCAP  →  VERACK  →  capability-controlled transport
```

Nothing else is sent in the handshake window.

This node sends, after processing VERSION:

1. `VERSION` (inbound only; outbound already sent it)
2. `RIGORCAP` (12-byte payload)
3. `VERACK`

After the peer's `VERACK` (and a received `RIGORCAP`):

- outbound `GETADDR` if `CAP_ADDR_RELAY`
- compact-block state if `CAP_COMPACT_BLOCK`
- witness tx inventory if `CAP_TX_RELAY`

## Fail-closed rules

| Event | Action |
|-------|--------|
| VERACK without RIGORCAP | disconnect |
| Duplicate RIGORCAP | disconnect |
| RIGORCAP size ≠ 12 | disconnect |
| RIGORCAP leftover bytes | disconnect |
| RIGORCAP after VERACK | disconnect |
| Empty bitmap | keep connection; enable nothing |
| Unknown bits | ignore |
| `wtxidrelay` / `sendaddrv2` / `sendcmpct` / `sendtxrcncl` | disconnect (obsolete Bitcoin negotiation) |
| Legacy `addr` | disconnect (`addrv2` only) |

## Version fields

- `VERSION.nVersion` is `RIGOR_PROTOCOL_CURRENT_VERSION` (currently 1).
- Stream `nVersion` after VERSION is `SERIALIZATION_VERSION` (70016).
- Capability bits, not the protocol number, select transport.
