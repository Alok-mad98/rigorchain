# Post-v25 P2P hardening (network-only cherry-picks)

**Date:** 2026-09-18  
**Baseline:** Bitcoin Core v25 engine (`CLIENT_VERSION` 25.0.0)  
**Scope:** `net.cpp`, `net_processing.cpp`, `addrman`, `txorphanage`, `timedata`  
**Not in scope:** `ConnectBlock`, `VerifyScript`, classifier, Ed25519, sighash, TRUC, P2A, Taproot relay, cluster mempool, BIP324

```
equivalence_claimed = false
```

RIGOR is not on the Bitcoin network. These changes copy **DoS, eclipse, and clock-skew** behaviour from Bitcoin Core v26–v30 into the existing v25 `PeerManager` / `CConnman`. They do not change what a valid transaction or block is.

## Absorbed

| Source | What landed | Why |
|--------|-------------|-----|
| v27 `#28956` | `GetAdjustedTime()` is unadjusted system time. Peer VERSION samples still fill `GetTimeOffset()` for RPC and the clock-skew warning. | Removes the “honest majority of outbound peers” assumption from header timestamps, IBD stall, and compact-block deadlines. |
| v26 `#27213` | Extra full-relay outbound toward a reachable network that currently has none. Extra-peer eviction will not drop the last full-relay/manual peer on a network. `AddrMan::Select` can filter by network. | Eclipse / partition resistance when Tor, I2P, CJDNS, or IPv6 is actually reachable. |
| v28 `#22729` | Startup fails if **any** explicit `-bind` / `-whitebind` / onion bind fails (default dual-stack still succeeds if one family works). | Operator cannot believe they are listening on an interface they are not. |
| v29 orphan parent fetch | A new INV announcer of an existing orphan is asked for missing parents. | Completes the earlier `AddAnnouncer` backport; one stalling peer no longer owns parent fetch. |
| v30 `#31829` | Orphanage trims by per-peer DoS score (announcements and weight). Global latency score 3000 and 404,000 WU per announcing peer. Unique-tx `-maxorphantx` default is **0** (optional extra ceiling). Eviction no longer picks a random global orphan. | One peer cannot flush another peer’s orphans. |

`AddTx` no longer re-enters `AddAnnouncer` while holding `m_mutex` (that path deadlocked on a duplicate orphan).

## Intentionally not imported

| Item | Reason |
|------|--------|
| BIP324 v2 transport | New wire protocol. Spec first (`P2-NET-006`). |
| Embedded asmap / RIGOR fixed seeds | Need a RIGOR map and live seed inventory. `-asmap` and DNS seeds remain. |
| TRUC / P2A / 1p1c package relay / cluster mempool | Mempool policy, not a transport DoS patch. |
| UPnP removal / NAT-PMP rewrite | Dependency change, not required for the P2P DoS surface. |
| Bitcoin assumevalid / assumeutxo hashes | Wrong chain. |

## Tests

From `core/`:

```
src/test/test_rigor --run_test=timedata_tests
src/test/test_rigor --run_test=addrman_tests/addrman_select_by_network
src/test/test_rigor --run_test=orphanage_tests/orphanage_peer_dos_eviction
```

## Residual (unchanged)

- Plaintext v1 framing (checksum is still HASH256).
- Empty mainnet `vFixedSeeds`; no default asmap.
- Witness-mandatory P2P, BIP37 disconnect, compact-block stall concurrency, 64-bit AddrMan ids remain as previously hardened.
