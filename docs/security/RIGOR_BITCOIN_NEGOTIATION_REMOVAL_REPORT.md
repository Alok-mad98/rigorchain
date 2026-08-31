# RIGOR Bitcoin Negotiation Removal Report

Mainnet is not launched. Bitcoin command-driven feature negotiation is removed, not aliased.

## Removed from handshake (no longer sent)

| Command | Bitcoin role | RIGOR replacement |
|---------|--------------|-------------------|
| `wtxidrelay` | negotiate wtxid vs txid inventory | `CAP_TX_RELAY` ⇒ witness relay always |
| `sendaddrv2` | negotiate addr v2 vs v1 | `CAP_ADDR_RELAY` ⇒ `addrv2` only |
| `sendcmpct` | negotiate compact-block support / HB | `CAP_COMPACT_BLOCK`; HB is local selection |
| `sendtxrcncl` | Erlay salt / version | not implemented (`CAP_TX_SYNC` is not local-active) |

Inbound copies of those four commands disconnect: `obsolete bitcoin negotiation command`.

Wire strings remain in `NetMsgType` so the node can recognize and reject them. No renamed Bitcoin commands (`rigortxid`, `rigoraddrv2`, `rigorcmpct`, `rigortxrcn`) were added.

## Compact block

- Permission: `HasCapability(CAP_COMPACT_BLOCK)`.
- Transport primitives unchanged: `cmpctblock`, `getblocktxn`, `blocktxn`.
- High-bandwidth peer selection is local (up to 3 peers). It no longer sends `sendcmpct`.
- RPC keeps `compact_block_hb_to` / `compact_block_hb_from`. `hb_from` is no longer set by a negotiation command. Bitcoin aliases `bip152_hb_*` stay removed.

## Address relay

- Send and accept `addrv2` only.
- Legacy `addr` disconnects.
- Requires `CAP_ADDR_RELAY`.

## Transaction relay

- Witness inventory (`MSG_WTX`) when `CAP_TX_RELAY`.
- No txid-only negotiation path.

## RES relay

No distinct RES inventory path exists. `CAP_RES_RELAY` is named and is **not** advertised. RES transactions travel as ordinary txs under `CAP_TX_RELAY`.

## Consensus leakage removed

Deleted from the capability enum:

- `FUTURE_SIGNATURE_V2`
- `FUTURE_PREDICATE_V2`
- `activation_epoch`

Future communication slots are `CAP_FUTURE_RELAY_0` / `CAP_FUTURE_RELAY_1` only. Predicate upgrades stay in the predicate registry.
