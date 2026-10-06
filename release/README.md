# RIGOR Alpha Release — Artifacts

| Field | Value |
|-------|-------|
| **Version** | `v0.1.0-alpha` |
| **Date** | 2026-08-12 |
| **Branch** | `mainnet-release-candidate-001` |

---

## Linux (`linux/`)

| File | Description |
|------|-------------|
| `rigor-wallet.deb` | Debian package (node + wallet + miner) |
| `rigor-wallet.AppImage` | Portable wallet launcher (includes AppDir extraction) |
| `rigor-alpha-linux.tar.gz` | Portable tarball (bin + config) |
| `rigor-wallet-x86_64.AppDir.tar.gz` | AppDir payload for AppImage launcher |

## Windows (`windows/`)

| File | Description |
|------|-------------|
| `rigor-wallet-0.1.0-alpha.exe` | NSIS installer (wallet + node + miner) |
| `rigor-wallet-alpha.exe` | Alias copy for release packaging |
| `rigord.exe` | Full node (PE32+ AMD64) |
| `rigor-miner.exe` | Solo GBT miner (PE32+ AMD64) |

## Checksums (`checksums/`)

| File | Description |
|------|-------------|
| `SHA256SUMS` | SHA-256 for all primary release artifacts |

---

## Verify

```bash
cd release/checksums
sha256sum -c SHA256SUMS
```

---

## Known limitations

- Public DNS seeds and seed nodes are **not** deployed.
- Public explorer and monitoring infrastructure are **pending**.
- Alpha packages are **not** Guix-signed mainnet releases.

See [RIGOR_ALPHA_RELEASE_NOTES.md](../docs/release/RIGOR_ALPHA_RELEASE_NOTES.md).
