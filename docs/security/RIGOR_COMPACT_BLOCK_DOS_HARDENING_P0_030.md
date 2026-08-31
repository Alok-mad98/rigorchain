# RIGOR Compact Block DoS Hardening (P0-030)

**Task:** `ASK-RIGOR-MAINNET-PRE-LAUNCH-NETWORK-SECURITY-HARDENING-P0-030`  
**Date:** 2026-08-28

Review of compact-block handling against the CVE-2024-52922 class: a high-bandwidth peer causing duplicate block fetches, memory pressure, and bandwidth exhaustion.

Consensus is unchanged. Compact-block binary layout is unchanged. `SENDCMPCT` is not a RIGOR handshake command (compact HB is local).

---

## Surface

| Message | Direction of concern |
|---------|----------------------|
| `CMPCTBLOCK` | inbound announcement / reconstruction |
| `GETBLOCKTXN` | we send (fetch) and we receive (serve) |
| `BLOCKTXN` | inbound fill for a partial block |
| `SENDCMPCT` | leftover wire name; not used to enable HB |

Constants (`net_processing.h` / `net_processing.cpp`):

| Limit | Value |
|-------|-------|
| `MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK` | 3 (existing stall mitigation) |
| `MAX_PARALLEL_COMPACT_REQUESTS_PER_PEER` | 3 |
| `MAX_GETBLOCKTXN_PER_WINDOW` | 16 |
| `GETBLOCKTXN_RATE_WINDOW` | 1 second |

---

## Per-peer request accounting

`CNodeState`:

```
peer:
{
  outstanding_compact_requests   // m_outstanding_compact_requests
  last_request_time              // m_last_compact_request_time
  compact_fetch_hashes           // m_compact_fetch_hashes
}
```

`AccountCompactFetch(state, hash)`:

- reject if `hash` is already in `m_compact_fetch_hashes` (no duplicate concurrent fetch of the same block)
- reject if `m_outstanding_compact_requests >= MAX_PARALLEL_COMPACT_REQUESTS_PER_PEER`
- otherwise insert, increment, stamp time

`ReleaseCompactFetch` runs from `RemoveBlockRequest` when that peer had a `partialBlock` (reconstruction in flight).

Outbound `GETBLOCKTXN` is sent only after `AccountCompactFetch` succeeds. Failure drops that peer’s in-flight slot for the hash instead of stacking another reconstruction.

---

## Inbound GETBLOCKTXN / BLOCKTXN

Capability gate: missing `CAP_COMPACT_BLOCK` → disconnect.

Flood gate (inbound `GETBLOCKTXN` only): more than 16 messages in a 1s window → `getblocktxn flood … disconnecting`.

Unexpected `BLOCKTXN` (no matching partial block) is ignored, not stacked.

---

## CVE-2024-52922 class review

| Abuse | Mitigation |
|-------|------------|
| Same block, many concurrent reconstructions from one peer | `m_compact_fetch_hashes` unique-hash + max 3 outstanding |
| Many distinct reconstructions from one high-bandwidth peer | `MAX_PARALLEL_COMPACT_REQUESTS_PER_PEER` |
| GETBLOCKTXN serve flood | 16 / 1s then disconnect |
| Compact messages without compact capability | disconnect |
| Stripped reconstructed block | `ProcessBlock` witness check disconnects |

This is request accounting and disconnect policy. It does not change `CheckBlock` / `ConnectBlock`.

---

## Tests

`core/test/functional/p2p_compactblock_stress.py`:

- high-bandwidth peer, duplicate `CMPCTBLOCK`
- unexpected / slow `BLOCKTXN` (node stays connected)
- inbound `GETBLOCKTXN` flood disconnects
- honest compact reconstruction still advances the tip
