# RIGOR P2P Security Hardening (P0-030)

**Task:** `ASK-RIGOR-MAINNET-PRE-LAUNCH-NETWORK-SECURITY-HARDENING-P0-030`  
**Date:** 2026-08-28

Goal: final network-layer security convergence before mainnet. Consensus is unchanged.

Related:

- [RIGOR_FIXED_SEED_SECURITY_MODEL_P0_030.md](./RIGOR_FIXED_SEED_SECURITY_MODEL_P0_030.md)
- [RIGOR_COMPACT_BLOCK_DOS_HARDENING_P0_030.md](./RIGOR_COMPACT_BLOCK_DOS_HARDENING_P0_030.md)
- [RIGOR_BLOOM_PROTOCOL_REMOVAL_REPORT_P0_030.md](./RIGOR_BLOOM_PROTOCOL_REMOVAL_REPORT_P0_030.md)
- Formal: `formal_verification/rigor_v01/NetworkSecurityBoundary.v`

---

## Compatibility

| Surface | Status |
|---------|--------|
| `ConnectBlock` / `VerifyRigorPredicate` / `AssetTypeFromScriptPubKey` | unchanged |
| RES / ORU maturity, transaction validity | unchanged |
| Handshake order (`VERSION` → `RIGORCAP` → `VERACK`) | unchanged |
| Compact-block wire layout | unchanged |
| `CNode::GetCommonVersion()` | still `PROTOCOL_VERSION` (serialization 70016) |
| RIGOR `VERSION.nVersion` | `1` … `255`; Bitcoin v25 `70016` disconnects |

---

## 1. Witness-only block relay

RIGOR has no legacy transaction model. A block without a coinbase witness **and** a witness commitment is not a relay object.

`PeerManagerImpl::ProcessBlock` (`net_processing.cpp`):

```
CheckBlock path (validation, unchanged)
        ↑
ProcessBlock
        |
 RelayBlockHasRequiredWitness
        | fail → disconnect
        v
 ProcessNewBlock
```

`RelayBlockHasRequiredWitness` requires:

1. Coinbase `HasWitness()`
2. `GetWitnessCommitmentIndex() != NO_WITNESS_COMMITMENT`

Stripped serialization still carries a commitment output; an empty coinbase witness is the strip signal. Failure logs `sent block without required witness; disconnecting`.

Serve path: `MSG_BLOCK` is treated as `MSG_WITNESS_BLOCK`. There is no `GetBlockWithoutWitness()` relay path.

Tests: `p2p_witness_required.py`, `p2p_witness_mandatory.py`.

---

## 2. Compact-block concurrency

CVE-2024-52922 class: high-bandwidth peers requesting the same block many times (duplicate fetch, memory, bandwidth).

Per-peer accounting on `CNodeState`:

| Field | Role |
|-------|------|
| `m_outstanding_compact_requests` | in-flight reconstructions |
| `m_last_compact_request_time` | last GETBLOCKTXN we sent |
| `m_compact_fetch_hashes` | hashes already fetching |

Limits:

- `MAX_PARALLEL_COMPACT_REQUESTS_PER_PEER = 3`
- same block hash cannot have two concurrent fetches from one peer
- inbound `GETBLOCKTXN` / `BLOCKTXN` require `CAP_COMPACT_BLOCK`
- inbound `GETBLOCKTXN` flood: 16 / 1s then disconnect

`SENDCMPCT` is not reintroduced. Compact HB remains local (`compact_block_hb_*`).

Tests: `p2p_compactblock_stress.py`.

---

## 3. BIP37 / Bloom removed

`NODE_BLOOM` is gone from `ServiceFlags`. Bloom command names are deleted. Any unknown command, including former BIP37 messages, logs `Unknown command "…"; disconnecting` and disconnects. Future commands must land behind a capability bit first.

Tests: `p2p_no_bloom.py`, `p2p_filter.py`, `p2p_nobloomfilter_messages.py`.

---

## 4. Eclipse resistance

| Layer | Mechanism |
|-------|-----------|
| DNS | `seed.rigor.network.` plus seed1/2/3 |
| Embedded `pnSeed[]` | removed (`chainparamsseeds.h` deleted) |
| Signed manifest | `rigor_seed_v1.json`, Ed25519, length-prefixed, chain-bound |
| AddrMan `nIdCount` | `uint64_t`; `Create()` refuses at `INT64_MAX` |
| ASMap | default on; `-noasmap` keeps `/16`; missing file is an init error |

Tests: `addrman_tests`, `seed_manifest_tests`, `p2p_eclipse_resistance.py`, `feature_asmap.py`.

---

## 5. Protocol identity

| Item | Status |
|------|--------|
| Bitcoin `CAlert` P2P | already absent |
| Signet chain | removed |
| `RIGOR_PROTOCOL_MIN_VERSION` | `1` |
| `RIGOR_PROTOCOL_CURRENT_VERSION` | `1` (accept `[MIN, CURRENT]`; raise CURRENT on upgrade) |

Bitcoin v25 peers advertise `70016` as `VERSION.nVersion` and are disconnected (`using obsolete version 70016; disconnecting`). Serialization still uses 70016 internally; that value is not a P2P feature gate.

---

## 6. Formal boundary

`NetworkSecurityBoundary.v`:

- Theorem 1: `P2P_message ≠ predicate_transition`
- Theorem 2: `invalid_network_object → reject`
- Theorem 3: `CAP_X ≠ PredicateVersion`

No `[Axiom]` / `[Admitted]`. This is an isolation model, not a C++ refinement.

---

## Acceptance

| Item | Status |
|------|--------|
| Witness-only block relay | PASS |
| Stripped block propagation removed | PASS |
| Compact block concurrency hardened | PASS |
| CVE-2024-52922 class risk reviewed | PASS |
| BIP37 removed | PASS |
| NODE_BLOOM removed | PASS |
| Bloom messages rejected | PASS |
| AddrMan uint64 | PASS |
| ASMap enabled | PASS |
| Fixed seed signed | MECHANISM READY; operator key ceremony pending |
| Eclipse resistance improved | PASS |
| Bitcoin alert removed | PASS |
| Signet chain removed | PASS |
| Consensus unchanged | PASS |
| Formal boundary updated | PASS |
