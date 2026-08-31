# RIGOR Network Layer Security Boundary Audit

**Task:** P0-025 (network)  
**Filename collision:** a same-day opcode audit already exists as [`RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.md`](./RIGOR_SCRIPT_OPCODE_FUTURE_UPGRADE_SURFACE_AUDIT_P0_025.md). This document is a **different surface**. Do not merge the two.  
**Priority:** P0 — Network / Security / Protocol Architecture  
**Mode:** AUDIT + hardening applied (same day). Original classification is preserved below; live code matches §0.  
**Date:** 2026-08-27  
**Mainnet status:** not launched. Distinct P2P magic and port. Compatibility with the Bitcoin P2P network is not a requirement.

**Hard freeze (this task did not touch, and later network patches must not touch):**

- consensus bodies
- `ConnectBlock`
- `VerifyScript`
- `AssetTypeFromScriptPubKey` / `ClassifyRigorTxout`
- Ed25519 ownership predicate
- sighash rules

Companion visualization (open beside chat): [`RIGOR-NETWORK-SECURITY-BOUNDARY-AUDIT.canvas.tsx`](/home/user/.cursor/projects/home-timlee-rigor-official-tree/canvases/RIGOR-NETWORK-SECURITY-BOUNDARY-AUDIT.canvas.tsx)

---

## 0. Verdict (read this first)

```
NETWORK STATUS:               HARDENED (pre-mainnet)

INHERITED BITCOIN CORE:       v25.0   (CLIENT_VERSION_MAJOR/MINOR in config/rigor-config.h)
PROTOCOL_VERSION:             70016   (Bitcoin wtxid-relay era; MIN_PEER_PROTO_VERSION == PROTOCOL_VERSION)
TRANSPORT:                    plaintext TCP  (no BIP324)
P2P MAGIC (mainnet):          0xce 0xf1 0xdb 0xfa     port 8760
CLIENT_NAME:                  RigorCore
DNS SEEDS (mainnet):          seed{1,2,3}.rigor.network
FIXED SEEDS:                  empty

ILLEGAL SCRIPT ON RELAY:      NO      (CheckTransaction fail-closed, then IsStandard)
P2SH / P2WSH RELAY:           NO      (consensus + policy reject)
GENERAL SCRIPT VM ON WIRE:    NO      (classifier + Ed25519; EvalScript gone)
BITCOIN P2P MODEL:            YES     (version/verack, inv/getdata, BIP152, NODE_* flags)
WITNESS ON THE WIRE:          MANDATORY (MSG_BLOCK == MSG_WITNESS_BLOCK; NODE_WITNESS required at VERSION)
BIP37 / NODE_BLOOM:           OFF     (filter*/merkleblock/MSG_FILTERED_BLOCK disconnect; -peerbloomfilters InitError)
CMPCTBLOCK STALL:             v25.1   (MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK = 3; Bitcoin Core #27626)
ADDRMAN IDs:                  int64_t (CVE-2024-52919 leftover)
GETDATA INV TYPES:            WHITELIST (unknown type disconnects)
POST-v25 P2P HARDENING:       PARTIAL (no BIP324; eclipse/bootstrap still weak)
ECLIPSE / BOOTSTRAP:          WEAK    (no asmap default, no fixed seeds)

SAFE TO DELETE BLOOM/BIP37:   DONE    (P2P path; CBloomFilter remains for tests / merkleblock RPC)
SAFE TO IMPORT BITCOIN v31.1: NO      (transport/DoS cherry-picks only; not TRUC/P2A/Taproot/cluster mempool)

P0 TEST COVERAGE:             p2p_witness_mandatory.py (witness/getdata); p2p_compactblocks.py (BIP152 + stall); p2p_filter.py / p2p_nobloomfilter_messages.py (BIP37 disconnect)
                              p2p_segwit.py skipped (Bitcoin pre/post-activation leftover)
```

**Architecture conclusion**

`rigord` is **not** on the Bitcoin network. Message start, default port, user-agent, datadir, and DNS seeds are RIGOR. Transaction accept-to-mempool already matches the RIGOR asset model:

```
P2P tx message
  → CheckTransaction          (fail-closed ScriptPubKey set)
  → IsStandardTx / AreInputsStandard / IsWitnessStandard
  → CheckTxInputs             (dual-pool conservation; not this audit)
  → VerifyScript              (frozen; not this audit)
```

The **network engine** is Bitcoin Core v25.0 plus RIGOR-native P2P policy: witness-mandatory relay, BIP37 off the public protocol, compact-block stall concurrency from v25.1, 64-bit AddrMan ids, and fail-closed getdata types. It is not current with Bitcoin Core v31.1 transport (no BIP324). Eclipse/bootstrap remains weak (empty fixed seeds, no default asmap).

**Hardening applied (pre-mainnet, no compatibility burden):**

| ID | Change | Status |
|----|--------|--------|
| P0-NET-001 | `NODE_WITNESS` required at VERSION; `MSG_BLOCK` served as witness; `GetFetchFlags` deleted | Done |
| P0-NET-002 | Compact-block stall concurrency (`mapBlocksInFlight` multimap, #27626) | Done |
| P0-NET-003 | BIP37: `filter*` / `merkleblock` / `MSG_FILTERED_BLOCK` disconnect; `-peerbloomfilters` InitError; `bloom@` permission gone | Done |
| P0-NET-003b | Getdata inv-type whitelist (gen-tx, `MSG_BLOCK`, `MSG_WITNESS_BLOCK`, `MSG_CMPCT_BLOCK`) | Done |
| P1-NET-004a | AddrMan `nIdCount` is `int64_t` | Done |
| P1-NET-005 | `MIN_PEER_PROTO_VERSION = 70016`; dead proto gates removed; Bitcoin alert + Signet seeds deleted | Done |
| Residual | No BIP324; empty fixed seeds; no default asmap | Open (P1/P2) |

---

## 1. What was read (production, not names)

| Path | Role | Lines |
|------|------|------:|
| `core/src/net.cpp` / `net.h` | TCP, accept/evict, message framing, 4 MB cap | 2955 / 1236 |
| `core/src/net_processing.cpp` / `.h` | handshake, inv, headers, compact blocks, tx, bloom, filters | 5902 / 93 |
| `core/src/protocol.h` / `protocol.cpp` | `NetMsgType`, `ServiceFlags`, `CInv`, desirable flags | 515 / 224 |
| `core/src/version.h` | `PROTOCOL_VERSION = 70016` | |
| `core/src/addrman.cpp` / `addrman_impl.h` | stochastic addr table; **`int64_t nIdCount`** | 1288 |
| `core/src/banman.h` | ban vs discourage (explicitly not a DoS shield) | |
| `core/src/headerssync.cpp` | headers pre-sync / work gate | 316 |
| `core/src/txorphanage.cpp` / `txrequest.cpp` | orphan + download tracker | 276 / 755 |
| `core/src/blockencodings.cpp` | BIP152 short-id reconstruction | 224 |
| `core/src/node/txreconciliation.cpp` | BIP330 Erlay (debug, default off) | 169 |
| `core/src/node/eviction.cpp` | inbound eviction (netgroup, onion/i2p; bloom prefer-evict removed) | |
| `core/src/policy/policy.cpp` | `IsStandard` → `ClassifyRigorTxout` | 255 |
| `core/src/consensus/tx_check.cpp` | `CheckTransaction` / `IsAllowedScript` on ATMP | |
| `core/src/validation.cpp` | `MemPoolAccept::PreChecks` | |
| `core/src/kernel/chainparams.cpp` | magic, seeds, checkpoints, assumevalid, assumeutxo | |
| `core/src/init.cpp` | `nLocalServices = NODE_NETWORK_LIMITED \| NODE_WITNESS` | |
| `core/src/config/rigor-config.h` | `CLIENT_VERSION_MAJOR 25`, `MINOR 0` | |
| `core/src/rpc/net.cpp` | `getpeerinfo`, `setban`, `addnode`, … | |

Repo searches (not only `core/src/net*`): `NODE_*`, `NetMsgType`, `CMPCTBLOCK`, `GETBLOCKTXN`, `SENDHEADERS`, `FEEFILTER`, `BIP152`, `assumeutxo`, `assumevalid`, `checkpoint`, `IsStandardTx`, `P2SH`, `P2WSH`, `BIP324` / `v2transport` (zero hits), `nIdCount`.

`BIP324` / encrypted v2 transport does **not** exist in this tree.

---

## 2. Current network architecture

```
                    DNS seeds (seed{1,2,3}.rigor.network)
                    AddrMan (new/tried buckets, /16 or -asmap)
                              |
                              v
  inbound TCP :8760  <-->  CConnman  <-->  outbound full-relay (8)
       |                       |           outbound block-relay (2)
       |                       |           feeler (1), addnode (8)
       |                       v
       |                 PeerManager (net_processing)
       |                       |
       |         +-------------+--------------+------------------+
       |         |             |              |                  |
       v         v             v              v                  v
   version/   headers-first  BIP152        tx relay          addr v1/v2
   verack     + headerssync  cmpctblock    inv/getdata/tx    rate-limited
   NODE_*     getheaders     getblocktxn   wtxidrelay        GETADDR
   ping/pong  BLOCK          blocktxn      feefilter         BanMan
                              |              |
                              v              v
                      validation.cpp    CTxMemPool
                      (FROZEN here)     CheckTransaction
                                        IsStandard*
```

**Handshake (live):** `VERSION` (proto, `nServices`, time, nonce, subver, height, `fRelay`) → optional `WTXIDRELAY` / `SENDADDRV2` / `SENDTXRCNCL` → `VERACK` → `SENDCMPCT(hb=false, ver=2)`. Messages before `VERSION` are dropped. Outbound peers lacking `GetDesirableServiceFlags` (`NODE_NETWORK` or, after IBD, `NODE_NETWORK_LIMITED`, **plus `NODE_WITNESS`**) are disconnected.

**Message framing:** 24-byte Bitcoin header (4 magic + 12 command + 4 size + 4 checksum). Reject if size > `MAX_PROTOCOL_MESSAGE_LENGTH` (4_000_000) or `MAX_SIZE`. Checksum still HASH256 of payload (plaintext, forgeable by a MITM on the path).

**Identity already RIGOR:** `pchMessageStart` mainnet `cef1dbfa`, port `8760`, `CLIENT_NAME` `RigorCore`, DNS `*.rigor.network`. Bitcoin's `f9beb4d9` / 8333 is not used on mainnet.

**Identity leftovers (comments/RPC only):** RPC/help still says “BIP152”; some comments still say “Pre-SegWit peer”. Signet Bitcoin seeds, Bitcoin final-alert, and proto floor 31800 are **gone**.

---

## 3. P2P core: what is still valid

These are **RIGOR-required network capabilities**. They are Bitcoin-shaped implementations of generic full-node functions. Keep the *capability*; do not freeze the *Bitcoin names* forever.

| Capability | Implementation | Verdict |
|------------|----------------|---------|
| TCP transport + framed messages | `net.cpp` `CConnman` | **Keep.** Replace checksum with AEAD later (BIP324-class), not this freeze. |
| Peer discovery | `AddrMan`, DNS seeds, `addr`/`addrv2`/`GETADDR`, feelers | **Keep.** Empty `vFixedSeeds` is a bootstrap gap. |
| Handshake + version gate | `ProcessMessage(VERSION/VERACK)` | **Keep**, raise `MIN_PEER_PROTO_VERSION`, drop Bitcoin alert. |
| Inventory relay | `inv` / `getdata` / `notfound`, `TxRequestTracker` | **Keep.** Caps `MAX_INV_SZ=50000`, `MAX_PEER_TX_ANNOUNCEMENTS=5000` are live DoS controls. |
| Block relay | `headers`, `block`, headers-first, download window 1024 | **Keep.** |
| Compact block | BIP152 v2 (`CMPCTBLOCKS_VERSION=2`, witness) | **Keep** as performance. Backport stall/concurrency fixes. |
| Tx relay | `tx`, wtxid, mempool accept, trickle inv | **Keep.** Policy already classifier-aligned. |
| Addr propagation | token bucket `0.1/s`, `MAX_ADDR_TO_SEND=1000` | **Keep.** |
| Peer eviction | `SelectNodeToEvict` (protect onion/i2p/localhost, last block/tx, netgroup) | **Keep.** Prefer-evict bloom peers is BIP37 residue. |
| Banning | `BanMan` ban vs discourage | **Keep as operator tool.** Code already states it is not a DoS defense. |
| Ping / stall | 2 min ping, 20 min timeout, headers download timeout | **Keep.** Timeouts that assume 10-minute blocks match **mainnet** spacing; testnet 1-minute / 30s chains inherit the same 288-block prune window. |

**Conflict with the RIGOR model (wire leftovers vs remaining policy knobs):**

| Item | Status |
|------|--------|
| `NODE_WITNESS` as optional | **Closed.** VERSION disconnects peers without the bit (inbound and outbound). |
| `MSG_BLOCK` without `MSG_WITNESS_FLAG` | **Closed.** Served as witness; requests use `MSG_WITNESS_BLOCK` / `MSG_WITNESS_TX`. |
| `NODE_BLOOM` / `filter*` / `merkleblock` | **Closed on P2P.** Messages disconnect; bit is never offered; `CBloomFilter` remains for tests / merkleblock RPC. |
| Bitcoin `ServiceFlags` comments / BIP process | **Open (P2).** Bits 24–31 comments still name the BIP process. |
| `permit_bare_multisig` default **true** | **Open (P2-NET-007).** Ignored in `IsStandardTx` but still an RPC/init knob. |
| Dust formula | **Open (P2-NET-007).** Still uses `WITNESS_SCALE_FACTOR`. Policy, not consensus. |
| Signet Bitcoin seeds | **Closed.** Removed. |

---

## 4. Bitcoin network-assumption cleanup (A / B / C)

Legend: **A** = RIGOR-required (keep semantics, maybe rename). **B** = historical Bitcoin leftover (keep until a dedicated replacement exists). **C** = safe delete/replace candidate (network-only; not this patch).

| Item | Class | Evidence | RIGOR note |
|------|:-----:|----------|------------|
| `NODE_NETWORK` | **A** | `GetDesirableServiceFlags`; IBD requires it | Full-history service bit. Keep. |
| `NODE_NETWORK_LIMITED` | **A** | prune path; 288-block serve window | Semantics useful. Constant `288` = Bitcoin 2 days at 10 min; **mainnet spacing is also 10 min**, so the window is ~2 days. Testnet 1 min → ~4.8 h. Revisit per chain. |
| `NODE_WITNESS` | **A** | required at VERSION; `GetFetchFlags` gone | Mandatory. Keep `MSG_WITNESS_FLAG` on inv until a native inv enum exists. |
| `NODE_BLOOM` | **C (P2P closed)** | `FILTERLOAD/ADD/CLEAR` disconnect | Bit unused. Message names kept only to reject. |
| `NODE_COMPACT_FILTERS` | **C** (or later light-client **A**) | BIP157/158; `-peerblockfilters` | Not required for full nodes. Do not advertise unless a RIGOR light-client spec exists. |
| `SENDHEADERS` (BIP130) | **A** | `ProcessMessage` + announce path | Right way to announce blocks. Keep. |
| `FEEFILTER` (BIP133) | **A** | per-peer min fee | ORU fees are real. Keep; ensure units are ORU, not Bitcoin vsize. |
| `SENDCMPCT` / `CMPCTBLOCK` / `GETBLOCKTXN` / `BLOCKTXN` (BIP152) | **A** | version **2 only** (witness) | Transport optimization. Independent of Script VM. Keep. |
| `WTXIDRELAY` (BIP339) | **A** | proto ≥ 70016 | Witness txids are the RIGOR tx identity. Keep. |
| `ADDR` / `ADDRV2` / `SENDADDRV2` (BIP155) | **A** | IPv6/Tor/I2P | Keep. |
| `GETHEADERS` / `HEADERS` | **A** | headers-first | Keep. |
| `GETBLOCKS` / `inv` of blocks | **B** | still handled | Headers-first superseded it. Low priority delete after all peers speak headers. |
| `MERKLEBLOCK` / `MSG_FILTERED_BLOCK` | **C (P2P closed)** | BIP37 | Disconnect / getdata whitelist. |
| `FILTERLOAD` / `FILTERADD` / `FILTERCLEAR` | **C (P2P closed)** | always disconnect | Message names kept only to reject. |
| `MEMPOOL` message | **B** | BIP37-era; gated on bloom **or** mempool permission | Restrict to permissioned peers only; do not advertise as a public dump. |
| `GETCFILTERS` / `CFILTER` / `GETCFHEADERS` / `CFHEADERS` / `GETCFCHECKPT` / `CFCHECKPT` | **C** | BIP157 | Same as `NODE_COMPACT_FILTERS`. |
| `SENDTXRCNCL` (BIP330 Erlay) | **B** | `-txreconciliation`, default **false** | Experimental. Do not enable on mainnet until specified. Candidate to strip if unused. |
| Bitcoin `alert` push (`version <= 70012`) | **C (deleted)** | was hardcoded Bitcoin alert payload | Gone. |
| `MIN_PEER_PROTO_VERSION = 70016` | **A** | `version.h` | Equal to `PROTOCOL_VERSION`. |

---

## 5. Transaction propagation vs RIGOR asset model

**Question:** can an illegal ScriptPubKey enter the gossip layer?

**Answer: no, not into the mempool, and therefore not into honest relay.**

`ProcessMessage(TX)` → `ChainstateManager::ProcessTransaction` → `AcceptToMemoryPool` → `MemPoolAccept::PreChecks`:

1. `CheckTransaction` — consensus fail-closed:
   - `IsAllowedScript` = `AssetTypeFromScriptPubKey` ∈ `{ORU, RES}` for spendable outs
   - `CheckTxOutAssetWitnessBinding` allows only v0 keyhash, v1 RES, `RES_MINT`, `CONVERGE`, `NULL_DATA`
   - P2PKH / P2SH / P2WSH / unknown witness → `bad-rigor-script-type` / `bad-txns-asset-script-nonstandard`
2. `IsStandardTx` — `ClassifyRigorTxout` only: v0 P2WPKH, v1 RES (canonical Ed25519 point), mint, converge, single `OP_RETURN`. Default branch **false** (`reason = scriptpubkey`).
3. `AreInputsStandard` — prevouts must be v0 keyhash, v1 RES, or mint. Everything else (including P2SH/P2WSH if they somehow existed) **false**.
4. `IsWitnessStandard` — explicit: `witnessversion == 0 && program.size() != 20` → **false** (P2WSH). v1 native RES must be 2-stack Ed25519.

So:

| Check | Illegal script on the wire | Enters mempool? | Relayed by honest `rigord`? |
|-------|----------------------------|-----------------|-----------------------------|
| P2PKH / P2SH / P2WSH / bare multisig / unknown witness | deserialized, then rejected | **No** | **No** |
| v1 RES with non-canonical point | consensus may classify as RES; policy `IsStandard` false | **No** (policy) | **No** |
| Oversized scriptSig / non-pushonly | policy | **No** | **No** |
| `-acceptnonstdtxn` on test chains | `m_require_standard` can be false | consensus `CheckTransaction` still runs | still fail-closed at consensus |

**Bitcoin script assumptions still in the *propagation comments and knobs*, not in the accept set:**

- `IsStandardTx` comment still describes “15-of-15 P2SH” for `MAX_STANDARD_SCRIPTSIG_SIZE=1650`.
- `policy.h` still defines `MAX_P2SH_SIGOPS`, `MAX_STANDARD_P2WSH_STACK_*`, `DEFAULT_PERMIT_BAREMULTISIG=true`.
- BIP125 RBF (`SignalsOptInRBF`, `bip125-replacement-disallowed`) is live mempool policy. Not a Script VM. Decide later; do not import Bitcoin TRUC/v3.
- Orphan pool (`DEFAULT_MAX_ORPHAN_TRANSACTIONS=100`) still exists (CVE-2012-3789 bound). `AddAnnouncer` is present (partial later backport); parent fetch is still “ask the delivering peer for txids” (v25 model).

**P2SH/P2WSH propagation logic:** **not live as a relay path.** It is leftover constants, wallet/descriptor parsers, and BIP37 merkle filters. Safe to delete from **policy/net** in a later P2/P3. Do not delete by editing `VerifyScript`.

---

## 6. Block propagation model

| Mechanism | In tree? | Fits RIGOR? |
|-----------|:--------:|-------------|
| Headers-first | **Yes** | **A. Keep.** Work is on headers; blocks follow. Independent of Script. |
| Headers pre-sync (`headerssync.cpp`) | **Yes** (v24/v25) | **A. Keep.** Combined with `nMinimumChainWork = 2^26` on mainnet (four genesis-difficulty blocks pass; a 2000-header `powLimit` flood does not). |
| Compact blocks BIP152 v2 | **Yes** | **A. Keep.** Witness is mandatory; version 1 (no witness) is rejected (`sendcmpct_version != 2` → return). |
| `getblocktxn` / `blocktxn` | **Yes** | **A. Keep.** Reconstruction from mempool. `MAX_BLOCKTXN_DEPTH=10`. |
| Parallel IBD (`MAX_BLOCKS_IN_TRANSIT_PER_PEER=16`, window 1024) | **Yes** | **A. Keep.** |
| `assumevalid` | pinned to **genesis** on mainnet | **B. Keep the knob frozen at genesis** until a buried honest tip exists. Do **not** copy Bitcoin assumevalid hashes. Skipping `VerifyScript` under assumevalid is a validation option — later ops task; not a network delete. |
| `assumeutxo` | mainnet map **empty** (`TODO`) | **B. Machinery only.** Do not ship a snapshot hash before a real chain. Do not import Bitcoin assumeutxo. |
| Checkpoints | `{0, genesis}` only | **A as an empty/genesis pin.** `bad-fork-prior-to-checkpoint` is live. Do not add Bitcoin heights. Adding later checkpoints is an ops/release task, not consensus-script. |
| Witness-stripped `MSG_BLOCK` serve | **No** | **Closed.** `MSG_BLOCK` is served with witness. |

**Not applicable / do not import from Bitcoin v31.1:** TRUC (v3), pay-to-anchor, cluster-mempool topology changes, Bitcoin Taproot announcement types.

---

## 7. DoS attack surface (v25 residual vs later fixes)

BanMan’s own comment is the right threat model: disconnect/ban is **not** a DoS shield. Caps and algorithms are.

### 7.1 Present (v25-class, still useful)

| Control | Where | Status |
|---------|-------|--------|
| 4 MB message cap | `MAX_PROTOCOL_MESSAGE_LENGTH` | Live |
| Inv size 50k | `MAX_INV_SZ` | Live |
| Tx announcement 5k / peer | `MAX_PEER_TX_ANNOUNCEMENTS` | Live |
| Tx in-flight 100 / peer | `MAX_PEER_TX_REQUEST_IN_FLIGHT` | Live |
| Addr rate 0.1/s + token bucket | `MAX_ADDR_RATE_PER_SECOND` | Live (v22+; CVE-2024-52919 *part 1* mitigated) |
| Headers pre-sync + min chainwork | `headerssync.cpp`, chainparams | Live |
| Orphan cap 100 | `LimitOrphans` / CVE-2012-3789 | Live |
| Unconnecting headers cap 10 | `MAX_NUM_UNCONNECTING_HEADERS_MSGS` | Live |
| Compact-filter range caps | `MAX_GETCFILTERS_SIZE=1000` | Live if BIP157 on |
| Historical block upload window | `HISTORICAL_BLOCK_AGE` 7 days | Live |
| Inbound eviction / netgroup | `eviction.cpp` | Live; **asmap off unless `-asmap` file** |
| `BLOCK_MUTATED` does not punish via compact block | `MaybePunishNodeForBlock` | Live (CVE-2024-52921 class, fixed in **v25.0**) |
| Malformed command | `CMessageHeader::IsCommandValid` | Live |
| Bloom oversized filter | misbehavior 100 | Live **if** bloom enabled |

### 7.2 Missing or only partial vs Bitcoin Core after v25.0

| Issue | Bitcoin fix | This tree | Risk |
|-------|-------------|-----------|------|
| Compact-block **stall** (announcer does not send `blocktxn`; ~1×`nPowTargetSpacing` wait) **CVE-2024-52922** | v**25.1** / v26: concurrent request from up to 3 HB peers, ≥1 outbound | **Backported** (`MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK = 3`, `mapBlocksInFlight` multimap) | **Closed.** |
| BIP324 encrypted transport | v26 experimental, v27 default | **Absent** | **Medium** privacy + ISP/Wi-Fi censorship of inv/tx. Not consensus. |
| `AddrMan` 32-bit `nIdCount` **CVE-2024-52919 part 2** | v29: 64-bit id | **`int64_t nIdCount`** | **Closed.** |
| Embedded asmap | v31 ships a map | `-asmap` optional, no embed | **Medium** eclipse (IPv4 /16 buckets). |
| Orphan multi-peer resolution / 1p1c | v28–v31 | `AddAnnouncer` exists; package relay **not** on P2P (`submitpackage` regtest-only) | **Low–medium** tx-relay reliability, not inflation. |
| No fixed seeds | n/a | `vFixedSeeds.clear()` on mainnet | **Medium** first-start if DNS is captured. |
| Witness-stripped block serve | Bitcoin still has it for old peers | **Refused / served as witness** | **Closed.** |
| BIP37 if operator sets `-peerbloomfilters` | Bitcoin kept optional | **InitError** | **Closed.** |

CVE-2024-35202 (`blocktxn` double-`FillBlock` crash) was fixed **in v25.0**. Treat as **not exposed** at the v25.0 baseline.

CVE-2024-52914 (quadratic orphan stall) was fixed in **0.18**. Not a v25 leftover.

---

## 8. Bitcoin Core v25.0 → v31.1 security difference matrix

RIGOR `CLIENT_VERSION` is **25.0.0**. Bitcoin Core 31.x is EOL-boundary for 28.x and includes P2P work RIGOR does **not** have. **Do not rebase `validation.cpp` / Script onto v31.1.** Cherry-pick network-only hardening.

| Module | v25.0 (this tree) | v31.1 change (Bitcoin) | RIGOR impact |
|--------|-------------------|------------------------|--------------|
| **P2P transport** | Unencrypted v1, checksum | BIP324 v2 default | **Migrate (optional but recommended):** encrypt + hide message boundaries. New proto; does not touch `VerifyScript`. |
| **P2P compact block** | HB=3; stall ≈ block interval | Concurrent HB fetch (25.1+); stall/mutated-block follow-ups | **Migrate:** backport stall concurrency only. |
| **P2P addr** | Rate limit; 32-bit ids | 64-bit ids (29); embedded asmap (31) | **Migrate** ids + consider embedded asmap. |
| **P2P tx download** | TxRequestTracker; orphan 100; announcer map partial | Multi-announcer orphan resolution; 1p1c package relay | **Evaluate.** Useful; must not import TRUC. |
| **P2P privacy RPC** | Broadcast to all tx-relay peers | `-privatebroadcast` Tor/I2P (31) | **Defer.** Nice-to-have; not a consensus gap. |
| **Validation** | RIGOR classifier + Ed25519 (already diverged) | TRUC, P2A, Taproot policy, cluster-mempool prep | **Do not import.** Freeze consensus. |
| **Mempool** | Ancestor/descendant limits, BIP125 RBF, RIGOR `IsStandard` | 0-fee parent 1p1c; TRUC; cluster | **Cherry-pick 1p1c only if specified.** Keep classifier standardness. |
| **Wallet interaction** | Descriptor/RPC still Bitcoin-shaped (other P0s) | More descriptor/Taproot defaults | **Out of scope.** Network must not re-open P2SH/P2WSH send. |
| **RPC exposure** | `getpeerinfo` BIP152 fields, `addnode`, `setban`, hidden `submitpackage` | privatebroadcast RPCs; more net introspection | **Keep current net RPCs.** Do not add Bitcoin-only methods. Review `addconnection` (test). |

---

## 9. RIGOR-native network architecture (target)

`rigord` should remain a **full node over TCP**, not a rewrite of transport in this phase.

**Keep**

- TCP (and existing Tor/I2P sockets)
- Peer discovery (AddrMan + RIGOR DNS + eventually RIGOR fixed seeds)
- Headers-first + work-gated pre-sync
- Block relay (`headers` / `block`)
- Compact block v2 (`cmpctblock` / `getblocktxn`) once stall-hardened
- Tx relay (`inv` / `getdata` / `tx` / wtxid) gated by `CheckTransaction` + classifier `IsStandard`
- Ping, eviction, operator ban
- Distinct magic / port / user-agent

**Delete or stop advertising**

- Bitcoin-optional `NODE_WITNESS` (witness is the only mode)
- `NODE_BLOOM` and `filter*` / `merkleblock`
- Public BIP157 unless a RIGOR light-client spec exists
- Witness-stripped `MSG_BLOCK` responses
- Bitcoin final-alert payload
- Bitcoin signet seeds
- Dead policy knobs: `permitbaremultisig`, P2SH/P2WSH standardness constants
- Erlay unless a RIGOR spec turns it on

**Replace (later, dedicated TASKs)**

- `ServiceFlags` → RIGOR bits, e.g. `NODE_FULL` / `NODE_PRUNED` / (optional) `NODE_FILTER`
- `MIN_PEER_PROTO_VERSION` → 70016 or `RIGOR_PROTO_VERSION`
- Checksum framing → BIP324-class AEAD (or equivalent)
- Dust / feefilter units → exact-byte ORU policy (no vsize)
- `NODE_NETWORK_LIMITED` window → per-chain `nPowTargetSpacing`, not a copied 288 if testnets matter

**Do not**

- Speak the Bitcoin magic or 8333
- Reintroduce a Script VM at the relay layer
- Import Bitcoin TRUC / P2A / Taproot relay
- Edit `ConnectBlock` / `VerifyScript` / classifier to “help” P2P cleanup

---

## 10. Freeze vs migrate vs delete

### Must freeze (consensus / identity)

| Item | Why |
|------|-----|
| `VerifyScript`, `AssetTypeFromScriptPubKey`, Ed25519, sighash | User constraint; live ownership. |
| `ConnectBlock` / `CheckBlock` | Consensus. |
| `CheckTransaction` fail-closed output set | Stops illegal scripts **before** gossip. Network cleanup must keep this order. |
| Mainnet `pchMessageStart` / port / genesis | Network partition from Bitcoin. |
| Headers-first + `nMinimumChainWork` gate | DoS. Tune the **value** in a release task, not by deleting the mechanism. |
| `assumevalid = genesis` until a buried tip | Prevents accidental skip of `VerifyScript`. |

### Must migrate (network-only, pre-mainnet)

| Item | Why |
|------|-----|
| Treat witness as mandatory; refuse stripped blocks | RIGOR model. |
| Drop BIP37 bloom/merkle from the public protocol | DoS + SPV leftover. |
| Backport compact-block stall concurrency (25.1 class) | CVE-2024-52922. |
| `nIdCount` → 64-bit | CVE-2024-52919 leftover. |
| Raise `MIN_PEER_PROTO_VERSION`; delete Bitcoin `alert` | Identity + dead code. |
| RIGOR fixed seeds + asmap story | Eclipse / first-start. |
| Remove Bitcoin signet seeds | Cross-network leak. |

### May delete later (P2/P3, still freeze consensus)

BIP157 unless specified; Erlay; `GETBLOCKS`; P2SH/P2WSH policy constants; bloom eviction preference; `permitbaremultisig` arg.

---

## 11. Risk register

| ID | Finding | Severity | Pre-mainnet exploit? | Action |
|----|---------|----------|----------------------|--------|
| N-1 | Entire PeerManager is Bitcoin v25 P2P | **Architecture** | N/A | Native flag/proto TASK |
| N-2 | No BIP324; MITM sees inv/tx | **Medium** | Yes on hostile path | Transport TASK |
| N-3 | Compact-block stall (25.0 vs 25.1) | **Closed** | — | #27626 backported |
| N-4 | No default asmap; empty fixed seeds | **Medium** | Eclipse if DNS+IPv4 captured | Bootstrap TASK |
| N-5 | 32-bit addrman ids | **Closed** | — | `int64_t nIdCount` |
| N-6 | BIP37 still compiled | **Closed on P2P** | — | filter*/merkleblock disconnect; `-peerbloomfilters` InitError |
| N-7 | Witness-stripped block serve | **Closed** | — | `MSG_BLOCK` served as witness; NODE_WITNESS required |
| N-8 | Bitcoin signet seeds | **Closed** | — | Deleted |
| N-9 | Bitcoin `alert` leftover | **Closed** | — | Deleted |
| N-10 | Illegal ScriptPubKey on relay | **None found** | No | **Freeze** CheckTransaction order |
| N-11 | Importing Bitcoin v31 validation | **High if done** | Consensus drift | **Forbidden** |

---

## 12. Follow-on TASK suggestions (do not start in this audit)

1. **P0-NET-001 — Witness-mandatory P2P** — **Done.** Disconnect peers without `NODE_WITNESS`. `MSG_BLOCK` served with witness. Covered by `p2p_witness_mandatory.py`.

2. **P0-NET-002 — Compact-block stall backport** — **Done.** Bitcoin Core #27626 / v25.1 concurrent HB fetch.

3. **P0-NET-003 — BIP37 / merkle / NODE_BLOOM removal** — **Done on P2P.** Messages disconnect; bloom permission gone; `CBloomFilter` kept for tests / merkleblock RPC.

4. **P1-NET-004 — AddrMan 64-bit + asmap/fixed-seed bootstrap** — **Ids done.** Asmap / `vFixedSeeds` still open.

5. **P1-NET-005 — Proto identity** — **Done.** `MIN_PEER_PROTO_VERSION = 70016`; alert and Bitcoin signet seeds deleted.

6. **P2-NET-006 — Transport encryption evaluation**  
   BIP324 vs a RIGOR AEAD. Spec first. No consensus.

7. **P2-NET-007 — Policy leftover cleanup**  
   `MAX_P2SH_SIGOPS`, P2WSH stack constants, `permitbaremultisig`, dust vsize. Must not touch classifier.

8. **P3-NET-008 — Explicit non-import of Bitcoin v31 mempool/validation**  
   Written gate: no TRUC, no P2A, no cluster mempool, no Taproot relay, no Bitcoin assumevalid/assumeutxo hashes.

Each TASK: **network/policy/init/RPC only.** If a patch needs `ConnectBlock` or `VerifyScript`, it is the wrong TASK.

---

## 13. Answers required by the task

### NETWORK STATUS: **HARDENED (pre-mainnet)**

Not **PASS**: still a Bitcoin v25 P2P engine without BIP324; eclipse/bootstrap still weak (empty fixed seeds, no default asmap).

Not **NEED HARDENING** for the P0 wire surface: witness is mandatory, BIP37 is off the protocol, compact-block stall concurrency is backported, getdata types are fail-closed, AddrMan ids are 64-bit, proto floor is 70016.

Not **NOT READY**: not on Bitcoin’s network; relay already fail-closed on illegal scripts; headers-first + work gate; inv/addr/orphan caps exist; magic/port/user-agent are RIGOR.

### 1. Is the network layer still a Bitcoin model?

**Yes.** Peer lifecycle, message set, `NODE_*` bits, BIP152/130/133/339, AddrMan, BanMan, and `PeerManager` are Bitcoin Core v25. RIGOR changed **what a valid tx/block is**, not **how peers gossip**.

### 2. Are there v25 leftover security risks?

**Residual — not the Script VM class.** Closed this pass: compact-block stall, AddrMan 32-bit ids, BIP37 P2P, witness-stripped serve, Bitcoin signet seeds, final-alert hex.

Still open: no BIP324; no embedded asmap / no fixed seeds.

CVE-2024-35202 and CVE-2024-52921-class mutated-block cross-peer state are **in the v25.0 baseline** (treat as addressed). Illegal scripts do **not** get honest mempool relay.

### 3. What must migrate?

Done this pass: witness-mandatory inv/block path; bloom/merkle off the protocol; compact-block stall backport; addrman 64-bit; proto floor + delete Bitcoin alert/signet seeds.

Still later: bootstrap (fixed seeds + asmap); encrypted transport. **Cherry-pick P2P from v25.1–v31.1. Do not migrate Bitcoin validation/mempool policy.**

### 4. What must freeze?

`ConnectBlock`, `VerifyScript`, `AssetTypeFromScriptPubKey`, Ed25519 predicate, sighash, `CheckTransaction` fail-closed set, mainnet magic/port/genesis, headers-first + min-chainwork **mechanism**, `assumevalid` at genesis until a real buried hash is chosen by a release TASK.

---

*End of audit. No consensus, ConnectBlock, VerifyScript, classifier, Ed25519, or sighash code was modified.*
