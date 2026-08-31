# RIGOR Fixed Seed Security Model (P0-030)

**Task:** `ASK-RIGOR-MAINNET-PRE-LAUNCH-NETWORK-SECURITY-HARDENING-P0-030`  
**Date:** 2026-08-28

Bootstrap IPs cannot be encrypted. First-run connect requires public addresses. Authenticity is Ed25519 over a canonical seed manifest, plus DNS and an empty compiled snapshot.

This is a P2P bootstrap check. It is not consensus.

---

## Why not encrypt seeds

A seed IP that is secret cannot be used to start the network. Anyone who needs to join must learn at least one reachable address. Encryption of that address would still require a public key and a public locator.

Correct design: **layered authenticity**, not secrecy.

```
DNS seed                 →  seed.rigor.network.
Compiled seed snapshot   →  not used (`chainparamsseeds.h` removed)
Signed seed manifest     →  rigor_seed_v1.json  (Ed25519)
AddrMan + ASMap          →  ASN-aware buckets after connect
```

---

## 1. DNS seeds

Mainnet (`kernel/chainparams.cpp`):

- `seed.rigor.network.`
- `seed1.rigor.network.`
- `seed2.rigor.network.`
- `seed3.rigor.network.`

Testnet uses `seed*.testnet.rigor.network.`. There is no signet chain.

---

## 2. Compiled seed snapshot

`chainparamsseeds.h` / `pnSeed[]` were unused and have been removed. Operators publish a signed epoch in `rigor_seed_v1.json`. Bitcoin leftover seed IPs must not be restored.

---

## 3. Signed seed manifest

File (datadir, per network): `rigor_seed_v1.json`

```json
{
  "epoch": "2026Q3",
  "expiry": 1798761600,
  "nodes": ["203.0.113.10:38732"],
  "signature": "<64-byte Ed25519 hex>"
}
```

Canonical signed message (`SeedManifestSignedMessage`) is length-prefixed (injective):

```
tag "RIGOR_SEED_MANIFEST_V1"
chain_id          (CChainParams::NetworkIDString)
epoch
expiry uint64
nodes[]           (sorted, compact-size strings)
```

JSON parse is fail-closed: never throws; file cap 64 KiB; at most 512 nodes (reject, do not truncate); expiry must be a positive integer; epoch/node tokens are printable ASCII.

Verification (`CConnman::Start` in `net.cpp`):

1. Parse JSON
2. Epoch ∈ `{2026Q3, 2026Q4}` (`CChainParams::SeedEpochs()`)
3. `expiry` is in the future
4. Ed25519 verify against `CChainParams::SeedManifestPubkey()` over the chain-bound message

Failure logs `Ignoring signed seed manifest: …` and does not add addresses. A missing file is normal.

### Keys

| Network | Pubkey source |
|---------|----------------|
| Mainnet | compiled `d6520b13…71bb89f5`. Mechanism ready; **operator key ceremony pending**. Do not treat this as an operational signing key until that ceremony. Secret is not in the repository. |
| Test / regtest | Ed25519 from `SHA256("RIGOR_SEED_MANIFEST_TEST")`. **Publicly derivable. Test-only.** |

---

## 4. Epoch rotation

Accepted epochs are a list on `CChainParams`, currently `2026Q3` then `2026Q4`. Bumping the window is a release change: drop the expired epoch, add the next, republish `rigor_seed_v1.json`.

Old manifests with an unknown epoch are ignored. They do not fork consensus.

---

## 5. AddrMan / ASMap (after bootstrap)

- `nIdCount` is `uint64_t`. `Create()` returns null at `INT64_MAX` instead of wrapping.
- ASMap defaults **on** (`-asmap=1`). `-noasmap` keeps `/16` prefix bucketing. A missing map file is an init error when asmap is enabled. Functional tests set `asmap=0` in `rigor.conf`.

ASN-aware buckets reduce single-ASN eclipse after the node has addresses. They do not authenticate the first seed.

---

## Tests

- `core/src/test/seed_manifest_tests.cpp`
- `core/src/test/addrman_tests.cpp` (ASMap + `nIdCount` insert volume)
- `core/test/functional/p2p_eclipse_resistance.py`
- `core/test/functional/feature_asmap.py`
