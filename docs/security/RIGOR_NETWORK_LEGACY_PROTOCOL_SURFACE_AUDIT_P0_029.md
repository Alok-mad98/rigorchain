# RIGOR Network Legacy Protocol Surface Audit (P0-029)

**Task:** `TASK-RIGOR-NETWORK-CAPABILITY-NEGOTIATION-ARCHITECTURE-AND-MIGRATION-P0-029-V2`  
**Phase:** 1 — Network Legacy Surface Audit  
**Date:** 2026-08-27  
**Mode:** AUDIT of the inherited Bitcoin Core v25 P2P surface, then migration (see companion docs).  
**Hard freeze (this task did not touch):** `ConnectBlock`, `VerifyScript` / Ed25519 predicate, `ScriptPubKey` bytes, RES/ORU maturity, sighash, asset classification.

Related:

- Prior network boundary: [`RIGOR_NETWORK_SECURITY_BOUNDARY_AUDIT_P0_025.md`](./RIGOR_NETWORK_SECURITY_BOUNDARY_AUDIT_P0_025.md)
- Capability design: [`RIGOR_NETWORK_CAPABILITY_MODEL_P0_029.md`](./RIGOR_NETWORK_CAPABILITY_MODEL_P0_029.md)
- Upgrade classes: [`RIGOR_PROTOCOL_UPGRADE_MODEL_P0_029.md`](./RIGOR_PROTOCOL_UPGRADE_MODEL_P0_029.md)

---

## 0. Verdict (read this first)

```
LEGACY P2P SURFACE:           Bitcoin Core v25 handshake + message set
PROTOCOL_VERSION (serialize): 70016   (CDataStream only; not a feature gate)
MIN_PEER_PROTO_VERSION:       REMOVED (was 70016, equal to PROTOCOL_VERSION)
RIGOR_PROTOCOL_MIN_VERSION:   1
RIGOR_PROTOCOL_CURRENT_VERSION: 1
VERSION GATES (SENDHEADERS_VERSION, …): already dead after P0-025; not restored
FEATURE ENABLEMENT:           peer.HasCapability() + existing operational messages
CONSENSUS TOUCHED:            NO
```

**Architecture conclusion**

The Bitcoin **version-gate model** (`if (peerVersion >= SENDHEADERS_VERSION)`) was already hollow: P0-025 set `MIN_PEER_PROTO_VERSION == PROTOCOL_VERSION == 70016` and deleted the numeric gates. That left **no upgrade window** — a v2 node advertising a new protocol number would have been indistinguishable from “must equal 70016”.

This task replaces that floor with RIGOR protocol versions (min=1, current=1) and a `RIGORCAP` advertisement. Serialization keeps `PROTOCOL_VERSION = 70016` so `SERIALIZE_TRANSACTION_NO_WITNESS` / `ADDRV2_FORMAT` flag bits do not move.

---

## 1. What was read

| Path | Role |
|------|------|
| `core/src/version.h` | `PROTOCOL_VERSION`, former `MIN_PEER_PROTO_VERSION` |
| `core/src/protocol.h` / `protocol.cpp` | `NetMsgType`, `ServiceFlags`, inv types |
| `core/src/net.h` / `net.cpp` | transport, `CNode`, framing |
| `core/src/net_processing.cpp` | handshake, inv, headers, compact blocks, tx, addr |
| `core/src/net_permissions.*` | whitelist flags (not version gates) |
| `core/src/versionbits.*` | BIP9 **block** version bits (consensus FSM, not P2P) |
| `core/src/kernel/chainparams.*` | magic, port, `vDeployments[TESTDUMMY]` NEVER_ACTIVE |

Searches: `VERSION`, `PROTO`, `BIP`, `MIN_PEER_PROTO_VERSION`, `PROTOCOL_VERSION`, `SENDHEADERS_VERSION`, `WTXID_RELAY_VERSION`, `FEEFILTER_VERSION`, `SHORT_IDS_BLOCKS_VERSION`, `INVALID_CB_NO_BAN_VERSION`, `BIP0031_VERSION`.

---

## 2. Feature / version-gate matrix

| Item | Pre-P0-029 state | RIGOR handling | Class |
|------|------------------|----------------|-------|
| `SENDHEADERS_VERSION` | **Absent.** SENDHEADERS sent after headers-sync work gate (P0-025 removed `if (version >= 70012)`). | Keep operational `sendheaders`. Covered by `BLOCK_RELAY_V1`. | KEEP + CAPABILITY |
| `WTXID_RELAY_VERSION` | **Absent.** `wtxidrelay` always offered after VERSION. | Keep operational `wtxidrelay`. Covered by `TX_RELAY_V1`. | KEEP + CAPABILITY |
| `FEEFILTER_VERSION` | **Absent.** `feefilter` always sent to tx-relay peers. | Keep operational `feefilter`. Covered by `TX_RELAY_V1`. | KEEP + CAPABILITY |
| `SHORT_IDS_BLOCKS_VERSION` | **Absent.** Compact blocks always offered (`SENDCMPCT` ver=2). | Keep operational compact-block messages. Covered by `COMPACT_BLOCK_V1`. | KEEP + CAPABILITY |
| `INVALID_CB_NO_BAN_VERSION` | **Absent.** Compact-block misbehavior uses current PeerManager rules. | No Bitcoin-era no-ban branch to restore. | REMOVE (already gone) |
| `BIP0031_VERSION` | **Absent.** Ping/pong always nonce-bearing. | Keep ping/pong. | KEEP |
| Compact block | Live (`cmpctblock` / `getblocktxn` / `blocktxn`, stall concurrency from v25.1). | Capability `COMPACT_BLOCK_V1`. HB slot still 3 peers. | KEEP + CAPABILITY |
| Fee filter | Live. | Capability `TX_RELAY_V1`. | KEEP + CAPABILITY |
| Addr relay | Live (`addr` / `addrv2` / `sendaddrv2`). | Keep. Not a new capability in v1 (addr is connection hygiene). | KEEP |
| Tx relay | Live (`inv`/`getdata`/`tx`, wtxid). `fRelay` still selects TxRelay. | Capability `TX_RELAY_V1`. | KEEP + CAPABILITY |
| `MIN_PEER_PROTO_VERSION == 70016` | Floor = current; **no v1/v2 window**. | Replaced by `RIGOR_PROTOCOL_MIN_VERSION = 1`. | REPLACE |
| `PROTOCOL_VERSION` in VERSION message | Advertised 70016 (Bitcoin wtxid-era number). | VERSION advertises `RIGOR_PROTOCOL_CURRENT_VERSION`. `PROTOCOL_VERSION` remains serialization-only. | REPLACE |
| `if (peerVersion >= X)` as feature test | Not present for the named gates; handshake still used `min(nVersion, PROTOCOL_VERSION)` as common version. | Common version = `min(nVersion, RIGOR_PROTOCOL_CURRENT_VERSION)`. Features = `HasCapability()`. | REPLACE |
| Bloom / BIP37 | Disconnect on `filter*` / `merkleblock` / `MSG_FILTERED_BLOCK` (P0-025). | Unchanged. | KEEP (reject path) |
| Compact filters (BIP157/158) | Optional `NODE_COMPACT_FILTERS`. | Unchanged. Service bit, not a version gate. | KEEP |
| Erlay / BIP330 | Debug, default off. | Unchanged. Not a version gate. | KEEP |
| BIP324 v2 transport | Absent. | Out of scope. | KEEP (absent) |
| `versionbits` / BIP9 | `DEPLOYMENT_TESTDUMMY` NEVER_ACTIVE (except regtest start=0). Taproot deployment removed. | **Not P2P.** Consensus block-version FSM. Untouched. | KEEP (consensus; isolated) |

Class legend: **KEEP** = operational protocol stays; **REMOVE** = Bitcoin-era branch gone; **REPLACE** = RIGOR equivalent; **CAPABILITY** = advertised in `RIGORCAP`.

---

## 3. Handshake before this task

```
VERSION (proto=70016, services, nonce, subver, height, fRelay)
  → WTXIDRELAY, SENDADDRV2, optional SENDTXRCNCL
  → VERACK
  → SENDCMPCT(hb=false, ver=2)
```

Disconnect if `nVersion < 70016` or missing `NODE_WITNESS`.

There was **no** `SENDHEADERS_VERSION` test. SENDHEADERS is delayed until the peer’s best-known work exceeds `MinimumChainWork()`.

---

## 4. Handshake after this task

```
VERSION (proto=RIGOR_PROTOCOL_CURRENT_VERSION=1, …)
  → RIGORCAP (protocol_version, capability_bitmap, activation_epoch)
  → WTXIDRELAY, SENDADDRV2, optional SENDTXRCNCL
  → VERACK
       disconnect if !Received()   # no baseline fill
  → SENDCMPCT iff HasCapability(COMPACT_BLOCK_V1)
```

Disconnect if `nVersion < RIGOR_PROTOCOL_MIN_VERSION` (1) or missing `NODE_WITNESS`.

After VERSION, `CNode::SetCommonVersion(PROTOCOL_VERSION)` — serialization stream only. The VERSION integer stays the RIGOR protocol number.

A later v2 node advertising `nVersion=2` still connects: `nVersion >= MIN`. Differences are capabilities, not the serialization integer.

---

## 5. `versionbits` is not this surface

`core/src/versionbits.*` implements the Bitcoin BIP9 **block header** threshold FSM. Chainparams leave `DEPLOYMENT_TESTDUMMY` inactive on mainnet. That is a **consensus activation** leftover, not a P2P version gate. P0-029 does not retarget it. Future hard consensus upgrades use the predicate-registry model ([P0-026](./RIGOR_SOFT_FORK_UPGRADE_MODEL_P0_026.md)), not BIP9.

---

## 6. Identity leftovers (accepted historical)

| Location | Status |
|----------|--------|
| MIT copyright / “Bitcoin Core developers” | KEEP |
| `PROTOCOL_VERSION = 70016` as **serialization** constant | KEEP (flag-bit collision avoidance) |
| RPC `bip152_hb_to` / `bip152_hb_from` | REMOVED (P0-030 Batch A; `compact_block_hb_*` only) |
| `m_compact_block_hb_*` on `CNode` | RIGOR identifier |
| Comments “Historical origin: Bitcoin BIPxxx” | KEEP (origin citation) |
| `pchMessageStart` / port 8760 / `RigorCore` | Already RIGOR (P0-025) |

---

## 7. Consensus isolation (audit claim)

```
P2P message → Decode → mempool/block validation input → consensus
```

`CNodeCapabilities` lives on `Peer` in `net_processing.cpp`. It is not passed into `ConnectBlock`, `VerifyScript`, `AssetTypeFromScriptPubKey`, or maturity checks. Formal statement: `formal_verification/rigor_v01/NetworkCapabilityBoundary.v`.
