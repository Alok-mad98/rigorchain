# Changelog

All notable **product** releases are listed here. Consensus-rule history is
not rewritten in this file; frozen parameters live in
[`docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md`](docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md).

```
equivalence_claimed = false
```

## [0.2] — 2026-09-15

Product tag `v0.2`. Not a consensus-parameter change.
Packed `CLIENT_VERSION` remains `25.0.0`.

### Added

- RFC-020 `destroy_cost(h)` as the live RES issuance rule
- Reference SOLO miner on GBT copy-bytes (ORU coinbase; RES is not mined)
- Dual-asset wallet source for ORU and RES

### Changed

- Wallet Convergence copy: destroyed ORU cannot be recovered
- Official wallet packages do not include the miner (miner is a parallel product)

### Security

- Formal verification honesty unchanged: no C++ refinement claim

Build from source: [`docs/developer/Build.md`](docs/developer/Build.md).
Notes: [`docs/release/RIGOR_V0.2_RELEASE_NOTES.md`](docs/release/RIGOR_V0.2_RELEASE_NOTES.md).

## [0.1.3] — 2026-09-03

Historical **stable** channel (`ALPHAPLUS_RELEASE=false`).

- Display identity `v0.1.3` on `rigord`, Qt wallet, Windows installer, and
  macOS `CFBundleShortVersionString`. Packed `CLIENT_VERSION` remains `25.0.0`.
- Wallet packages do not include `rigor-miner`.
- No Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock,
  DisconnectBlock, CoinsDB, wallet SQLite schema, classifier, PoW, ASERT, or
  protocol-parameter changes.

Full notes: [`docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md`](docs/release/RIGOR_V0.1.3_RELEASE_NOTES.md).

## [0.1.2-alphaplus] — 2026-08-23

Historical AlphaPlus channel. Artifact names containing `alphaplus` are
**not** official v0.2 assets.

## [0.1.1-alphaplus] — 2026-08-17

Historical AlphaPlus channel.

## [0.1.0-alpha] — 2026-08-12

First public alpha tag. Companion freeze tag: `v0.1.0-alpha-stable`.

---

Build from source: [`docs/INSTALL.md`](docs/INSTALL.md).
Security reporting: [`SECURITY.md`](SECURITY.md).
