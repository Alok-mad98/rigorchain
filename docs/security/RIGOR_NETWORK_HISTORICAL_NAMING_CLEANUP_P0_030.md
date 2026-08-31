# RIGOR Network Historical Naming Cleanup (P0-030)

**Task:** `ASK-RIGOR-NETWORK-HISTORICAL-NAMING-CLEANUP-P0-030`  
**Date:** 2026-08-27

Goal: remove remaining Bitcoin historical **names** from the network layer. Batch A (fail-closed handshake) additionally dropped historical RPC aliases and the missing-`rigorcap` baseline.

---

## Compatibility

Wire command strings and compact-block / addr layouts are unchanged. Handshake is **stricter**: `rigorcap` is mandatory before VERACK. RPC no longer emits `bip152_hb_*`.

| Surface | Status |
|---------|--------|
| P2P handshake order (`VERSION` → `RIGORCAP` → `wtxidrelay` / `sendaddrv2` → `VERACK`) | order unchanged; missing `rigorcap` now disconnects |
| `rigorcap` duplicate / trailing / truncated payload | disconnect |
| Message magic / compact-block binary layout / addr encoding | unchanged |
| Wire command strings `"wtxidrelay"` and `"sendaddrv2"` | unchanged |
| RPC keys `bip152_hb_to` / `bip152_hb_from` | **removed** |
| `CNode::GetCommonVersion()` | `PROTOCOL_VERSION` (serialization), not RIGOR protocol `1` |

---

## 1. Compact-block high-bandwidth (`bip152_hb_*`)

Bitcoin BIP numbers must not be RIGOR API. Chosen name: `compact_block_hb_*` (clearer than `rigor_compact_block_hb_enabled`; matches the to/from pair).

### Before (Bitcoin naming)

| Layer | Name |
|-------|------|
| `CNode` / `CNodeStats` | `m_bip152_highbandwidth_to` / `m_bip152_highbandwidth_from` |
| RPC `getpeerinfo` | `bip152_hb_to` / `bip152_hb_from` only |
| `rigor-cli` | `is_bip152_hb_*`; help “High-bandwidth BIP152 compact block relay” |
| Qt peer pane | `bip152_hb_settings`; tooltip “High bandwidth BIP152 compact block relay” |

### After (RIGOR naming)

| Layer | Name |
|-------|------|
| `CNode` / `CNodeStats` | `m_compact_block_hb_to` / `m_compact_block_hb_from` |
| RPC `getpeerinfo` (primary) | `compact_block_hb_to` / `compact_block_hb_from` |
| RPC `getpeerinfo` (alias) | **removed** (`bip152_hb_*` is not a RIGOR API) |
| `rigor-cli` | `is_compact_block_hb_*`; help “High-bandwidth compact block relay” |
| Qt peer pane | `compact_block_hb_settings`; tooltip “High bandwidth compact block relay” |

Internal C++ is RIGOR-only. Historical RPC aliases were removed in Batch A (mainnet not launched; no compatibility requirement).

---

## 2. `WTXIDRELAY`

Origin: Bitcoin BIP339. Wire command is the 12-byte P2P string `"wtxidrelay"`.

Mainnet is not launched, but changing the command string would still break:

- handshake feature negotiation
- Python `MESSAGEMAP` / `msg_wtxidrelay.msgtype = b"wtxidrelay"`
- fuzz `FUZZ_TARGET_MSG(wtxidrelay)`
- any node that already speaks this command

**Decision:** wire bytes stay. C++ identifier only.

Rejected `TXID_RELAY_V2`: this is wtxid relay, not a second txid version.  
Rejected `RIGOR_WTXID_RELAY` as a `NetMsgType` name: inconsistent with `VERSION` / `VERACK` / `SENDHEADERS` (those are not `RIGOR_*` prefixes; `RIGORCAP` is the native exception).

### Before

```cpp
namespace NetMsgType {
    extern const char* WTXIDRELAY;  // "wtxidrelay"
}
```

### After

```cpp
namespace NetMsgType {
    extern const char* WTXID_RELAY;  // still "wtxidrelay"
}
```

Feature enablement remains `RigorCapability::TX_RELAY_V1`. Handshake still sends/receives the `"wtxidrelay"` command between VERSION and VERACK.

---

## 3. `SENDADDRV2`

Origin: Bitcoin BIP155. Wire command is `"sendaddrv2"`.

**Decision:** keep `NetMsgType::SENDADDRV2`. It matches the frozen wire command, same pattern as `SENDHEADERS` / `SENDCMPCT`.

Rejected `RIGOR_ADDR_RELAY_V2` / `ADDR_RELAY_CAPABILITY_V2` as C++ identifiers: they would imply a new capability bit or a new command string. No new capability bit was added (that would change negotiation).

Comments now describe this as a RIGOR handshake capability (prefer ADDRV2 over ADDR). `BIP155Network` / `GetBIP155Network` in `netaddress.h` are **not** renamed: those names describe the frozen addr-v2 wire encoding.

### Before

Comments treated `SENDADDRV2` as “Bitcoin BIP155 support”.

### After

Comments treat `SENDADDRV2` as RIGOR addr-v2 handshake signaling. Identifier and `"sendaddrv2"` bytes are unchanged.

---

## 4. RPC output review

### `getpeerinfo`

| Field | Classification | Action |
|-------|----------------|--------|
| `bip152_hb_to` / `bip152_hb_from` | Historical compatibility | **Removed** (Batch A) |
| `compact_block_hb_to` / `compact_block_hb_from` | RIGOR API | Primary |
| `capabilities` / `capabilities_advertised` | Already RIGOR-native (P0-029) | Unchanged |
| `version` / `protocolversion` | RIGOR protocol version | Unchanged |

### `getnetworkinfo`

No `bip` / `bitcoin` field names. `protocolversion` is already the RIGOR protocol version. Unchanged.

---

## 5. Comment cleanup (scoped files)

Operational comments in `core/src/net*`, `protocol*`, `rpc/net*`, and `core/test/functional/p2p*` no longer describe live behavior as “See BIP 152 / BIP339 / BIP155”.

**Kept on purpose**

- MIT copyright lines (“The Bitcoin Core developers”)
- BIP37 **log / error strings** that tests match (`"BIP37 merkle/filtered-block request..."`, `"BIP37 is not supported"`)
- BIP155 **addr unserialize error strings** (`"BIP155 IPv4 address with length..."`) — wire/format, not API
- `BIP155Network` enum and `sha256("bitcoin")` CJDNS prefix bytes — encoding, not naming API

---

## 6. Forbidden surfaces (not touched)

- `CDataStream`
- message magic
- serialization bytes
- addr encoding
- compact-block binary layout
- P2P handshake / VERACK / capability negotiation outputs
- consensus (`ConnectBlock`, ScriptPubKey, Ed25519, maturity, sighash, classifier)

---

## 7. Scan remainder (in-scope)

After this change, remaining `Bitcoin` / `BIP` hits in the scan roots are copyright, BIP37 disconnect logs, BIP155 addr-encoding identifiers/errors, or frozen wire-command names in tests (`wtxidrelay`, `sendaddrv2`).

---

## 8. Verification

There are no standalone `net_tests` / `netbase_tests` / `net_peer_eviction_tests` binaries. Those names are Boost suites inside `./src/test/test_rigor`.

| Command | Result |
|---------|--------|
| `make -j$(nproc)` | PASS |
| `make check` | PASS (univalue + `test_rigor-qt` + per-suite `check-local` + `rpcauth-test.py`) |
| `./src/test/test_rigor` | PASS (950 cases) |
| `./src/test/test_rigor --run_test=net_tests` | PASS (14 cases) |
| `./src/test/test_rigor --run_test=netbase_tests` | PASS (15 cases) |
| `./src/test/test_rigor --run_test=net_peer_eviction_tests` | PASS (2 cases) |
| `./test/functional/test_runner.py rpc_net.py` | PASS (getpeerinfo primary keys + historical aliases) |

Note: `QT_QPA_PLATFORM=offscreen` makes `WalletTests::walletTests` execute a pre-existing ORU overview assertion (`overview_oru_confirmed`). Native/minimal platform skips that GUI path. `make check` without forcing offscreen is PASS.

---

## 9. Batch A follow-up (fail-closed)

Mainnet is not launched. Historical aliases and missing-`rigorcap` baseline were removed:

- VERACK without `rigorcap` disconnects
- duplicate / truncated / trailing-byte `rigorcap` disconnects
- `GetCommonVersion()` is `PROTOCOL_VERSION` after VERSION
- `getpeerinfo` no longer emits `bip152_hb_*`
