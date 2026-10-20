# RIGOR P2P: compact-block relay

Status: in effect from the mainnet launch release. Wire format unchanged.

## Negotiation

Compact-block support is a capability bit, `CAP_COMPACT_BLOCK` (bit 3), exchanged
in `rigorcap` before `verack`. RIGOR has no `sendcmpct` message. A peer that sends
`sendcmpct`, or any unknown message, is disconnected.

## Announcement (low-bandwidth BIP152)

A node never pushes `cmpctblock` unsolicited. New blocks are announced with
`inv`, or with `headers` once the peer has sent `sendheaders` and is known to
have the parent header (BIP130). A peer that wants the compact form requests it
with `getdata(MSG_CMPCT_BLOCK)`; the node answers with `cmpctblock` and serves
`getblocktxn` with `blocktxn` as in BIP152.

A receiving node still picks up to three peers it treats as high-bandwidth for
its own download choices (`compact_block_hb_to` in `getpeerinfo`). Because there
is no `sendcmpct`, that choice is not signalled to the peer and does not cause
the peer to push. `compact_block_hb_from` is always false.

## Compatibility with earlier releases

Earlier releases set every `CAP_COMPACT_BLOCK` peer to high-bandwidth at `verack`
and pushed `cmpctblock` for every new tip. Current nodes accept those unsolicited
messages and process them as before.

An unsolicited `cmpctblock` whose parent is unknown is handled like a block `inv`:

- Out of IBD: request headers from that peer (`getheaders`), as before.
- In IBD (new in this release): request headers from that peer under the same
  headers-sync policy as `inv` — the sync peer, or at most one new peer per
  announced block — and the same `getheaders` rate limit. The reply goes through
  normal headers processing, including the anti-DoS work threshold and
  `nMinimumChainWork`. No validation or IBD protection is relaxed.

Earlier releases dropped such a message silently during IBD. With every peer
pushing `cmpctblock`, an IBD node that had already received an empty headers
reply from its sync peer could wait indefinitely for an announcement it never
acted on (audit finding R2A-L01).

## Mixed-version behaviour (tested)

| Sender | Receiver (in IBD, higher gate) | Result |
|---|---|---|
| earlier | earlier | stalls (finding reproduced) |
| earlier | current | syncs: `getheaders` on unconnecting `cmpctblock` |
| current | earlier | syncs: announced by `inv`, no `cmpctblock` push |
| current | current | syncs: announced by `inv`/`headers` |

Regression tests: `p2p_cmpctblock_ibd_recovery.py`, `p2p_compactblocks.py`,
`p2p_compactblocks_blocksonly.py`, `p2p_compactblocks_hb.py`.
