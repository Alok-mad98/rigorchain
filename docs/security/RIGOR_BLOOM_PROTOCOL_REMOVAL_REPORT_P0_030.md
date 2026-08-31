# RIGOR Bloom Protocol Removal Report (P0-030)

**Task:** `ASK-RIGOR-MAINNET-PRE-LAUNCH-NETWORK-SECURITY-HARDENING-P0-030`  
**Date:** 2026-08-28

BIP37 bloom filters are not part of the RIGOR public P2P protocol. This report records the remaining surface after removal.

`CBloomFilter` has been deleted. `CRollingBloomFilter` remains as a local helper (inventory / addr known-set). It is not a network capability.

---

## Protocol

Former BIP37 commands are not in `allNetMessageTypes` and their `NetMsgType` constants are deleted. They are unknown commands: inbound disconnect.

`MSG_FILTERED_BLOCK` (wire type 3) is unused. Inbound `getdata` of that type disconnects as unsupported.

---

## Node capability

`NODE_BLOOM` is deleted from `ServiceFlags` and from `serviceFlagToStr`. `ALL_SERVICE_FLAGS` in `test/util/net.h` does not include it.

`-peerbloomfilters` is not a registered argument (`Invalid parameter -peerbloomfilters`).

---

## Inbound policy

Any command not handled in `ProcessMessage` disconnects (`Unknown command "…"; disconnecting`). That includes former bloom commands and any future name that has not been introduced behind a capability bit.

---

## Tests

| Test | Covers |
|------|--------|
| `p2p_no_bloom.py` | NODE_BLOOM absent; filter*/merkleblock/unknown command disconnect |
| `p2p_filter.py` | same plus filtered-block getdata and unregistered `-peerbloomfilters` |
| `p2p_nobloomfilter_messages.py` | filter* disconnect |
| `bloom_tests.cpp` | rolling bloom only |
