# Bitcoin Core v25 → Current Security Delta Audit

**Audit date:** 2026-09-30
**RIGOR baseline:** Bitcoin Core v25.0 engine (`CLIENT_VERSION` 25.0.0, `core/configure.ac`)
**Upstream endpoint:** Bitcoin Core 31.1, published 2026-07-08, the current stable release on [bitcoincore.org/en/download](https://bitcoincore.org/en/download/)
**Window:** fixes that landed after the v25.0 release (2023-05-25) and are public as of this date
**This document:** audit only. No patch is applied by this record.

```
equivalence_claimed = false
```

RIGOR is not a Bitcoin Core upgrade candidate. A fix is in scope only when it closes a vulnerability, a consensus-safety hole, a validation bug, a denial-of-service bug, a network hardening gap, a mempool safety bug, a wallet or RPC safety bug, or a dependency defect with security impact. New features, UI work, performance-only work, and wallet UX changes are out of scope unless they change an attack surface that RIGOR still has.

Prior network cherry-picks are recorded in [POST_V25_P2P_HARDENING.md](./POST_V25_P2P_HARDENING.md) (2026-09-18). This audit does not repeat that work as an open item.

## Executive Summary

Public Bitcoin Core advisories and release notes from v25.1 through v31.1 were compared with the RIGOR tree. Block validity, issuance, Ed25519 predicates, and the closed output templates were not found to be missing a Bitcoin consensus-rule fix that RIGOR should copy.

The highest remaining defect is local to snapshot acceptance. `hash_serialized_2` still uses the v25.0 expression whose C++ precedence drops coin height and the coinbase flag out of the assumeutxo commitment. Mainnet has no published assumeutxo hash yet. The code path is live, and regtest pins the weak hash. Background validation uses the same hash, so a snapshot that changes only height or the coinbase bit still matches. That affects maturity. It does not change `ConnectBlock`.

The one public High-severity post-v25 bug, CVE-2024-52911 (script-check use-after-free), is already closed in RIGOR by deleting the parallel script-check queue. Do not put that queue back.

Several v26–v30 denial-of-service and eclipse fixes are already present: compact-block stall concurrency, 64-bit addrman ids, per-peer orphanage limits, extra outbound diversity, unadjusted system time for local deadlines, and fail-closed explicit binds.

Still worth a separate, non-consensus review:

- rate-limit unconditional logs (CVE-2025-54604, CVE-2025-54605)
- do not treat inbound onion peers as whitelisted localhost
- make `hash_serialized_2` commit to height and coinbase, then regenerate snapshot pins
- classify witness-merkle mismatch inside compact-block reconstruction without relaxing RIGOR's 32-zero-byte witness reserved nonce
- check settings-file writes before rename
- cap `-maxmempool` and `-dbcache` on 32-bit builds

Bitcoin Core script, Taproot, TRUC, package-relay, cluster-mempool, BIP324, MuSig2, and descriptor-miniscript fixes do not apply. The wallet-directory deletion bug and the private-broadcast IP leak were introduced after v25.0 in code RIGOR does not contain.

One medium-severity Bitcoin Core fix that shipped in v30.0 is still under the project's disclosure embargo. It cannot be classified until the advisory is published.

## RIGOR Compatibility Principles

A Bitcoin patch is eligible only when all of the following hold:

1. The attacked component still exists in RIGOR and behaves the same way, or the same attack can be restated against `VerifyRigorPredicate`, asset classification, or the P2P and mempool code that RIGOR kept.
2. The patch does not change what a valid block or transaction is. In particular it must not change `ConnectBlock`, `CheckTransaction`, `CheckTxInputs`, `UpdateCoins`, `ClassifyRigorTxout`, `AssetTypeFromScriptPubKey`, `OP_CONVERGE` (`0xbd`), Witness v0 ORU, Witness v1 RES, `RGRORU01` / `RGRRES01`, or the 32-zero-byte witness reserved nonce.
3. Policy (standardness, relay feerate, ancestor limits) stays separate from consensus. A Bitcoin policy default is not ported merely because it shipped in a security release.
4. Every recommended patch needs its own review. This document is not an implementation instruction and not a merge of Bitcoin Core master.

Categories used below:

| Category | Status | Meaning |
|----------|--------|---------|
| A | APPLY | Same component, same behavior. Port the protection. |
| B | ADAPT | The vulnerable area exists, but RIGOR's rules differ. Rewrite the protection. |
| C | IGNORE | The Bitcoin component is absent, or the bug was introduced in code RIGOR never took. |
| D | DO NOT PORT | Applying the Bitcoin change would change RIGOR consensus or relax a frozen RIGOR rule. |

Severity uses exploitability, remote reach, consensus risk, crash, and funds impact: Critical, High, Medium, Low, Informational.

## Security Fix Inventory

Commits are the GitHub merge commits on `bitcoin/bitcoin` unless noted. "Already present" means the RIGOR tree was inspected and the protective behavior is already there.

| Bitcoin Version | Commit | Component | Severity | RIGOR Relevance | Classification | Action |
|-----------------|--------|-----------|----------|-----------------|----------------|--------|
| v26.0 | `da8e397e4a1bb1cd6185016d6537e82ac3f277c7` (#28685) | assumeutxo / `hash_serialized_2` | High | Same buggy serializer. Mainnet pin list is empty. Regtest pins the weak hash. Maturity depends on coin height. | B | ADAPT. Commit height and coinbase, then regenerate pins. Do not change block validity. |
| v29.0 | `3867d2421ae45c9093ceb355f5373c09e6f93c5d` (#31112); root-cause cleanup `aa1d0d7cd7355ab5154d44d831513f7fbc5028a2` (#35209, v31.1) | CVE-2024-52911 script-check use-after-free | High | Parallel `CCheckQueue` workers are gone. Checks run synchronously under `cs_main`. | B | Already closed. Do not restore the worker pool. |
| v29.2 | `2b0cd1f3fb220ff0fbaf67a7e0b410eb0c6bb25c` (#33395) | Inbound onion inherits `-whitelist` of localhost | Medium | `AddWhitelistPermissionFlags` runs on the accept address before the onion bind is treated specially. | A | APPLY |
| v29.2 | `f5f3e1f2632844a6ef1440bbb5c558449f7d8eed` (#32646) | Witness-mutated compact blocks | Medium | `FillBlock` calls `CheckBlock` only. Witness merkle is checked later. RIGOR's zero nonce is consensus, not Bitcoin's unconstrained nonce. | B | ADAPT the mutation classification only. |
| v26.0 | `b7138252ace6d21476964774e094ed1143cd7a1c` (#27213) | Eclipse / extra outbound per reachable network | Medium | Landed. See post-v25 P2P note. | A | Already present |
| v27.0 | `3c13f5d6124d887ebeb7f473d73722b570945130` (#28956) | Network-adjusted time removed from local deadlines | Medium | `GetAdjustedTime()` is system time. Future-block window is unchanged. | A | Already present. Do not retune the window. |
| v25.1 / v26.0 | `51c050787fd6bcd016969dd7e245818ebd110b67` (#27626); backport `8825983716abbc4d9125c0ee44cf4a0a470a1a27` (#27752) | CVE-2024-52922 compact-block stall | Medium | `MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK = 3` | A | Already present |
| v30.0 | `80067ac111c2f46a82efd6ed4ad54402b8d8c16c` (#31829) | Orphanage DoS | Medium | Latency score 3000 and 404,000 WU per announcing peer | A | Already present |
| v29.0 | `0d81b3ddedc73daf934242885c60f05f812ac275` (#30568) | CVE-2024-52919 part 2, 32-bit addrman id overflow | Low | `nIdCount` is `uint64_t`. Addr rate 0.1/s remains. | A | Already present |
| v29.1 / v30.0 | `2d59977601ea18f189991fa027364e69293c61a9` (#32604) | CVE-2025-54604 self-connection log fill; CVE-2025-54605 invalid-block log fill | Low | No log rate limiter. `connected to self` and invalid-chain lines are unconditional `LogPrintf`. | A | APPLY |
| v29.1 / v30.0 | `c43cc48aaaaa5c1c9fcb1175b415f7bc33e8537f` (#32530) | CVE-2025-46597 32-bit block-size check overflow | Low | The overflowing size check is still in `CheckBlock`. Production builds are 64-bit; `i686` remains a depends target. | A | APPLY the caps if 32-bit builds stay supported |
| v31.1 | `794befd4b06dcace1adbdb15e65bba1667f9bc15` (#35384) | `settings.json` replaced after a failed write | Low | `WriteSettings` returns success without checking the stream. | A | APPLY |
| v30.0 | `1861030bea7f55d08173c34d6fa11a16e6eff454` (#30469) | `coinstatsindex` `CAmount` overflow | Medium | Same `CAmount` accumulators. Default off. ORU/RES accounting is not Bitcoin's. | B | ADAPT only if the index stays supported |
| v28.0 | `45750f61d65d5ac4a28333d8f676961037b25f55` (#22729) | Startup continues after a failed explicit bind | Low | Fail-closed explicit binds already landed. | A | Already present |
| v29.2 | `0ba44d9c38af2822aa9d1b0424254de6972ca309` (#33296) | `FillBlock` on an empty header | Low | `FillBlock` already returns `READ_STATUS_INVALID` when the header is null. | A | Already guarded |
| v29.3 | `05d984b1a4fb6402d93a436203e986cde60fd99f` (#33475) | `addPackageTxs` unsigned underflow | Low | Reachable in Bitcoin when max weight is below 4000. RIGOR clamps weight to at least 4000. | C | IGNORE while the clamp remains |
| v30.0 | `a27430e259dff5efb612d570ae5107c0a949d9cc` (#32473), `dadf15f88cbad37538d85415ae5da12d4f0f1721` (#33050), `f679bad6052ad4b7ea0b57a406adc617746d54ed` (#33105) | CVE-2025-46598 script CPU exhaustion | Low | No script interpreter. Spends are fixed-width Ed25519 predicates. | C | IGNORE. Do not stop punishing consensus-invalid transactions. |
| v29.1 | `e9edd43a9592bcfb4285b2031b7cbb491a3b7c97` (#32521) | Legacy sigop standardness limit | Low | No Script VM, no legacy sigop counter as a consensus rule. | C | IGNORE |
| through v27.0 | CVE-2024-34149 (policy; not a Bitcoin Core advisory in the same series) | Tapscript size policy | Informational | Witness v1 is a 32-byte Ed25519 program, not Taproot. | C | IGNORE |
| v30.2 | `90d651a81f51f76a35658bb1e76e8806d09d18b0` (#34156) | Wallet migration deletes the wallet directory | Critical on Bitcoin 30.0/30.1 only | Introduced in 30.0. RIGOR rejects Berkeley DB wallets and does not run that migration. | C | IGNORE |
| v31.1 | `47da4f9b716d11294d4fb0f30b04a7bcf128cc14` (#35410) | `-privatebroadcast` clearnet retry leaks the sender IP | Medium on Bitcoin 31.0 only | Feature does not exist. No BIP324 v2 transport. | C | IGNORE |
| v26–v31 | BIP324, TRUC, P2A, 1p1c package relay, cluster mempool, MuSig2, miniscript | New relay, policy, and script systems | Informational | Absent, or present only as rejected templates. | C | IGNORE |
| v29.1 / v30.0 | #33106 | Lower default relay and block-min feerates | Informational | Policy economics, not a vulnerability fix. | D | DO NOT PORT as a security patch |
| v30–v31 | datacarrier / `OP_RETURN` default changes | Relay policy | Informational | RIGOR `OP_RETURN` is a closed data template. Bitcoin's default is not a RIGOR rule. | D | DO NOT PORT |
| depends, v31.1 vs this tree | SQLite 3.50.4 vs 3.38.5; Qt newer vs 5.15.5; miniupnpc removed upstream vs 2.2.2 here; libevent 2.1.12 both | Dependencies | Informational | FTS and rtree are disabled. UPnP defaults off. No new Bitcoin Core advisory names these packages after v25.0. | B | Separate dependency review. Not part of a consensus port. |

### Outside this delta

These were fixed in Bitcoin Core v25.0 itself. RIGOR's v25.0 origin already contains them. They are not missing patches.

| Item | Fixed in | Why it is not a delta |
|------|----------|------------------------|
| CVE-2024-35202 `blocktxn` assertion | v25.0 | `FillBlock` clears its header so it cannot be completed twice |
| CVE-2024-52921 mutated block clears another peer's download | v25.0 (#27608) | A peer cannot clear another peer's block download state |
| inv-to-send sort DoS | v25.0 (#27610) | Drain rate already scales with set size |

### Disclosure gap

Bitcoin Core 30.0 pre-announced five low-severity fixes. Four were disclosed on 2025-10-24 (CVE-2025-46597, CVE-2025-46598, CVE-2025-54604, CVE-2025-54605). One was later raised to medium and was not in that disclosure. Bitcoin Core 31.0 (2026-04-19) disclosed the single medium/high issue fixed in 29.0, which is CVE-2024-52911, and stated that 31.0 itself fixed no low-severity issue. The promoted v30.0 medium issue was still unpublished on this audit date, because 29.x had not reached the end-of-life point that the disclosure policy uses. No component, commit, or RIGOR classification is assigned to it.

## Consensus Impact Review

Inspected against `ConnectBlock`, `CheckTransaction`, `CheckTxInputs`, `UpdateCoins`, `ClassifyRigorTxout`, and the asset markers.

| Area | Result |
|------|--------|
| UTXO spend and create | No post-v25 Bitcoin consensus fix changes RIGOR's coin update. `UpdateCoins` still spends inputs and refuses to add an `OP_CONVERGE` output to the UTXO set. |
| Transaction validity | No post-v25 script or signature consensus change is applicable. There is no `interpreter.cpp` and no `EvalScript`. |
| Asset classification | Bitcoin Taproot, P2SH, P2WSH, and BIP341 fixes do not apply. Witness v0 remains ORU. Witness v1 remains RES. `0xbd` remains the converge destroy marker. |
| Signatures | `VerifyRigorPredicate` checks a 32-byte key and a 64-byte Ed25519 signature under `RGRORU01` or `RGRRES01`, bound to the genesis hash. Bitcoin ECDSA, Schnorr, sighash-type, and tapscript midstate fixes do not transfer. |
| Witness reserved nonce | RIGOR requires 32 zero bytes after the witness merkle match (`bad-witness-reserved-nonce`, consensus). Bitcoin leaves that value unconstrained. Copying Bitcoin's rule would be a protocol change. |
| Block weight and merkle | Merkle malleability (CVE-2012-2459) remains rejected as `BLOCK_MUTATED` inside `CheckBlock`. That check predates this window. |
| Time | `#28956` is already applied as a local clock source for deadlines and future-block comparison. It does not change the future-block limit itself. |
| Assumeutxo commitment | This is the consensus-adjacent gap. See below. It is not a block-acceptance rule. |

### Assumeutxo hash (open, Category B)

`core/src/kernel/coinstats.cpp` still has the v25.0 line:

```cpp
ss << VARINT(it->second.nHeight * 2 + it->second.fCoinBase ? 1u : 0u);
```

The conditional operator binds looser than `+`, so the hashed integer is `(height * 2 + coinbase_flag) ? 1 : 0`. For every coin above height 0 the commitment is the constant 1. Height and the coinbase bit are not in `hash_serialized_2`.

`LoadAssumeutxo` hashes the snapshot with `CoinStatsHashType::HASH_SERIALIZED` and compares it to `m_assumeutxo_data`. The later background check uses the same hash. A snapshot that changes only `nHeight` or `fCoinBase` matches both checks. RIGOR maturity for coinbase ORU and for RES mints is a function of that height.

`TxOutSer`, used by MuHash, does commit `uint32_t(height * 2 + coinbase)`. The weak path is specifically `hash_serialized_2`.

Mainnet `m_assumeutxo_data` is empty. Regtest pins at heights 110 and 200 were produced by the weak serializer (`validation_tests.cpp`). Fixing the serializer invalidates those pins and forces snapshot users onto full validation until new pins exist. It does not change which blocks `ConnectBlock` accepts. Publish no mainnet assumeutxo hash until the serializer commits height and coinbase.

### CVE-2024-52911 (closed, Category B)

Bitcoin's covert fix (#31112, v29.0) stopped returning early from block validation while worker threads still read `PrecomputedTransactionData`. The later cleanup (#35209) fixes the object lifetime directly.

RIGOR removed `CCheckQueue` scheduling from `CheckInputScripts`. Each `CScriptCheck` calls `VerifyRigorPredicate` on the connecting thread, and the cache insert happens only after every input check has returned true. A unit test still constructs a `CCheckQueue`; production validation does not. Reintroducing workers would reopen the use-after-free. Porting #31112 or #35209 verbatim is unnecessary.

### Script CPU DoS (rejected, Category C)

CVE-2025-46598 is quadratic hashing in legacy script and tapscript. RIGOR spend authorization is one Ed25519 verification over a domain-separated sighash. There is no opcode loop to amplify. `#33050` ("do not punish consensus-invalid transactions") is part of that Bitcoin mitigation. Porting it would weaken RIGOR's response to an actually invalid transaction. Leave punishment as it is.

## Network Layer Review

| Topic | RIGOR state | Disposition |
|-------|-------------|-------------|
| Compact-block stall (CVE-2024-52922) | Up to three in-flight compact-block reconstructions per block | Closed |
| Mutated-block cross-peer state (CVE-2024-52921) | In the v25.0 baseline | Closed |
| Addrman id width (CVE-2024-52919 part 2) | 64-bit id, plus the v22 address rate limit | Closed |
| Orphanage (v30 `#31829`) | Per-peer weight and latency eviction | Closed |
| Eclipse resistance (`#27213`) | Extra full-relay outbound toward a reachable network that has none | Closed |
| Bind failure (`#22729`) | Explicit bind failure aborts startup | Closed |
| Log fill (CVE-2025-54604 / 54605) | Unconditional `LogPrintf` on self-connect and on invalid-chain discovery | Open, Category A |
| Onion whitelist (`#33395`) | Localhost whitelist flags are applied in `CreateNodeFromAcceptedSocket` using the accept address. Inbound onion connections are accepted on a local bind, so they can receive `NoBan`, `ForceRelay`, `Mempool`, and `Relay`. | Open, Category A |
| Witness mutation inside `FillBlock` (`#32646`) | Reconstruction can return success before the witness merkle check. A later `BLOCK_MUTATED` result must not be turned into a ban, and must not be confused with `bad-witness-reserved-nonce`. | Open, Category B |
| Headers and peer limits | Existing header anti-DoS and peer limits are the v25-era protections plus the cherry-picks above. No additional public headers-spam CVE was fixed after v25.0. | No new port |
| BIP324 v2 transport | Not imported | Category C. New wire protocol. |
| `-privatebroadcast` | Absent | Category C |
| Mempool policy (TRUC, package RBF, full-RBF default, feerate defaults, `OP_RETURN` size) | RIGOR keeps its own standardness and fee policy | Category D if treated as a security port. Policy may be reviewed separately. It is not consensus. |

`#32646` must be rewritten, not copied. The protection RIGOR needs is: a compact block whose witness merkle does not match the coinbase commitment fails reconstruction as a mutation (`READ_STATUS_FAILED`), so one peer cannot stall download. The 32-zero-byte reserved nonce stays a consensus failure. Bitcoin's check does not know that rule.

## Wallet Layer Review

The wallet is not consensus authority. Wallet fixes may be applied when they do not change transaction validity.

| Topic | Disposition |
|-------|-------------|
| Key and signature type | Ed25519 only. Bitcoin descriptor, miniscript, MuSig2, and PSBT Taproot fixes are Category C. |
| Berkeley DB migration that deletes the wallet directory (#34156) | Category C. The bug was added in Bitcoin Core 30.0. RIGOR refuses to load a BDB `wallet.dat`. |
| `settings.json` write (#35384) | Category A. A full disk can truncate the temp file, `WriteSettings` still returns true, and `RenameOver` replaces the previous file. Check the stream before rename. |
| Static fee RPC (`-paytxfee` / `settxfee` removal in v31) | Wallet UX and fee-estimation policy. Not a vulnerability fix. Do not delete it as part of this audit. |
| `#31757` crash on double block-disconnect | RIGOR's `blockDisconnected` has local conflict-clearing behavior. Do not cherry-pick the Bitcoin patch onto that function. A separate wallet-reorg crash review is optional and is not a consensus change. |
| `coinstatsindex` overflow (#30469) | Index and RPC only, default off. A blind move from `CAmount` to `arith_uint256` would mis-state ORU and RES. Review only if the index remains a supported operator tool. |
| Backup and signing | No post-v25 Bitcoin wallet backup or seed-handling advisory maps onto the SQLite descriptor wallet as an open funds-loss bug. |

## Recommended Backports

Each item needs its own review. None of them is permission to merge Bitcoin Core master or to edit consensus rules.

1. **Assumeutxo serializer (Category B, High, latent on mainnet).** Change `hash_serialized_2` so each coin's height and coinbase flag are inside the commitment, matching the data `TxOutSer` already hashes for MuHash. Regenerate the regtest pins at heights 110 and 200 and the tests that lock those digests. Leave `ConnectBlock` and maturity constants alone. Do not publish a mainnet assumeutxo hash computed by the current serializer.
2. **Inbound onion permissions (Category A, Medium).** Do not apply `-whitelist` of the local bind address to connections accepted on the onion bind (`#33395`). Manual and clearnet whitelists stay as they are.
3. **Compact-block witness mutation (Category B, Medium).** On reconstruction, treat a witness-merkle mismatch as mutation failure. Keep `bad-witness-reserved-nonce` as a consensus failure. Do not import Bitcoin's unconstrained nonce.
4. **Log rate limit (Category A, Low).** Give each unconditional log site a quota, as in `#32604` (1 MiB per hour per source in Bitcoin Core). This closes CVE-2025-54604 and CVE-2025-54605. It does not change validation.
5. **Settings write (Category A, Low).** If the `settings.json` stream fails, do not rename it over the previous file (`#35384`).
6. **32-bit resource caps (Category A, Low).** If `i686` or other 32-bit builds remain supported, cap `-maxmempool` and `-dbcache` the way `#32530` does (500 MB and 1 GiB in Bitcoin Core 29.1). On 64-bit `size_t` the block-size multiplication in `CheckBlock` does not wrap. Dropping 32-bit support is an alternative to the cap, and is a release decision, not a consensus decision.

`coinstatsindex` (#30469) is a conditional seventh item: only if operators are expected to run `-coinstatsindex=1`. The rewrite has to follow RIGOR subsidy, burn, and mint accounting.

## Explicitly Rejected Changes

| Change | Why it does not apply |
|--------|------------------------|
| CVE-2025-46598 and `#32473` / `#33105` / `#32521` | The cost comes from the Bitcoin script interpreter and legacy sigops. RIGOR has neither. `VerifyRigorPredicate` does not grow with opcode count. |
| `#33050` stop punishing consensus-invalid transactions | That behavior exists so Bitcoin peers are not banned for script-validity edges tied to the CPU-DoS work. RIGOR should keep discouraging peers that send consensus-invalid transactions. |
| CVE-2024-52911 worker-lifetime patch as a reintroduction of parallel script checks | The vulnerable sharing is already gone. Restoring `CCheckQueue` would recreate it. |
| Taproot, Schnorr, BIP341, P2SH, P2WSH, miniscript, MuSig2 | Those spend types are not consensus-valid. Witness v1 is not Taproot. |
| CVE-2024-34149 tapscript policy limit | No tapscript. |
| BIP324 v2 transport | New wire protocol. Plaintext v1 remains the RIGOR transport. Tracking it belongs in a network specification, not in a security cherry-pick. |
| TRUC, pay-to-anchor, package relay, cluster mempool, default full-RBF, lowered relay feerates | Mempool policy and new transaction topology. They do not fix a RIGOR vulnerability. Adopting them would change which transactions are standard. |
| Bitcoin `OP_RETURN` / datacarrier default changes | RIGOR data outputs are a closed template. Bitcoin's relay default is not a monetary rule and is not a bugfix for that template. |
| Relaxing the 32-zero-byte witness reserved nonce to match BIP141 | That would change a frozen consensus check into an unconstrained extranonce. |
| Wallet migration deletion fix (`#34156`) | The deleting migration was added in Bitcoin Core 30.0. RIGOR does not have it. |
| `-privatebroadcast` IP-leak fix (`#35410`) | The feature and the v2-to-v1 retry path do not exist. |
| `-paytxfee` removal | Fee-UX policy. Not a funds-loss bug in this tree. |
| Bitcoin assumeutxo height hashes (for example height 840,000) | Wrong chain. Any future RIGOR pin must be computed on RIGOR blocks with the corrected serializer. |
| LevelDB seek-compaction disable and periodic chainstate compaction (`#35465`) | Disk write amplification under normal UTXO reads. Performance and wear, not an attacker-controlled consensus or crash bug in the public notes. Out of scope. |
| UPnP removal and NAT-PMP rewrite | No post-v25 advisory against miniupnpc 2.2.2. UPnP already defaults off (`DEFAULT_UPNP = false`). Removing it is a feature deletion, not required to close a disclosed RIGOR bug. |
| Re-porting `#27213`, `#28956`, `#22729`, `#30568`, `#27626`, `#31829` | Already present. A second port risks drifting from the reviewed tree. |

## Final Security Status

Public post-v25 Bitcoin Core security work does not justify an upgrade of the RIGOR consensus engine. The High-severity remote crash in the script checker is already closed by synchronous predicate checks. Issuance, `OP_CONVERGE`, Ed25519 domains, and output classification do not pick up a missing Bitcoin consensus patch in this window.

The open monetary-safety item is the assumeutxo content hash. It is High for any operator who loads a snapshot, and it is not reachable as an unsolicited P2P attack. Mainnet has no pin today, which is why the defect has not become a live mainnet acceptance rule. It should be fixed before any mainnet snapshot hash is published.

The other open items are availability and permission bugs: onion peers inheriting localhost whitelist (Medium, only if `-whitelist` covers the onion bind address), compact-block witness-mutation handling (Medium, propagation), and log, settings, and 32-bit caps (Low).

Applying the six recommended backports, after separate review, improves snapshot commitment, peer permissions, and disk-fill resistance. It does not increase the security of block validation beyond what synchronous `VerifyRigorPredicate` and the already-landed P2P cherry-picks already provide. It does not reduce risk from the still-embargoed v30.0 medium issue, because that issue's component is not public.

Dependency age (SQLite 3.38.5, Qt 5.15.5, miniupnpc 2.2.2) is hygiene. Those bumps are not claimed here as fixes for a demonstrated RIGOR exploit. libevent is already at the same 2.1.12-stable revision Bitcoin Core 31.1 uses. secp256k1 advisory work does not protect RIGOR spends.

**Status:** no consensus-port required. One snapshot-commitment adaptation should land before assumeutxo is used. Five smaller non-consensus backports are recommended. Bitcoin script, Taproot, policy-relay, and post-v25 feature fixes stay out of the tree.

## Sources

- [Bitcoin Core security advisories](https://bitcoincore.org/en/security-advisories/)
- CVE-2024-52911, CVE-2024-52919 (part 2), CVE-2024-52922, CVE-2025-46597, CVE-2025-46598, CVE-2025-54604, CVE-2025-54605, and the v25.0 disclosures for CVE-2024-35202, CVE-2024-52921, and the inv-to-send DoS
- Release notes v25.1, v25.2, v26.0 through v26.2, v27.0 through v27.2, v28.0 through v28.4, v29.0 through v29.4, v30.0 through v30.3, v31.0, v31.1
- Wallet migration note, 2026-01-05, and private-broadcast note, 2026-06-06
- Merge commits cited in the inventory, read from the GitHub pull-request API
- RIGOR: `core/src/kernel/coinstats.cpp`, `core/src/validation.cpp`, `core/src/net.cpp`, `core/src/net_processing.cpp`, `core/src/blockencodings.cpp`, `core/src/txorphanage.cpp`, `core/src/addrman_impl.h`, `core/src/util/settings.cpp`, `core/src/node/miner.cpp`, `core/configure.ac`
