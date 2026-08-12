# RIGOR Alpha Release Notes

| Field | Value |
|-------|-------|
| **Version** | `v0.1.0-alpha` |
| **Date** | 2026-08-12 |
| **Codename** | Product Alpha |
| **Consensus / protocol** | **Frozen** — no W, T, or RES issuance changes in this release |

---

## Product overview

**RIGOR** is a Bitcoin-derived full node with ORU proof-of-work mining and deterministic
**RES Convergence** — a script-defined compliance layer for asset classification and
convergence minting.

This alpha release delivers installable **wallet**, **node**, and **miner** binaries for
local validation and operator testing. It is **not** a signed mainnet production launch.

---

## Components

### RIGOR Wallet

Qt-based descriptor wallet with integrated RES Convergence workflow:

- Convergence lifecycle panels (overview, history, probability, education)
- W=32 draw window displayed as **32 blocks** to the user
- Cold storage and descriptor wallet support
- Local node connectivity and RPC integration

### ORU (mining)

- `rigord` — full node with ORU PoW validation
- `rigor-miner` — solo GBT CPU miner (`miner/` source tree)
- Local mining stack validated via `contrib/alpha/` and LMS acceptance

### RES Convergence

- Deterministic convergence draw over a 32-block window (`CONVERGENCE_DRAW_WINDOW = 32`)
- RES mint path validated end-to-end in local mainnet simulation
- Wallet UI guides the full ORU → Convergence → RES workflow

---

## Features

- Bitcoin-derived full node (`rigord`)
- ORU proof-of-work mining (`rigor-miner`)
- RES deterministic convergence (RFC-014 v1.2, W=32)
- W=32 user display as 32 blocks in wallet UI
- Descriptor wallet (BIP-compatible paths)
- Cold storage workflow support
- Linux packages (deb, AppImage/portable, tarball)
- Windows NSIS installer with PE-validated binaries

---

## Download

| Platform | Artifact |
|----------|----------|
| Linux deb | `release/linux/rigor-wallet.deb` |
| Linux AppImage | `release/linux/rigor-wallet.AppImage` |
| Linux portable | `release/linux/rigor-alpha-linux.tar.gz` |
| Windows installer | `release/windows/rigor-wallet-0.1.0-alpha.exe` |
| Windows node | `release/windows/rigord.exe` |
| Windows miner | `release/windows/rigor-miner.exe` |

Checksums: `release/checksums/SHA256SUMS`

---

## Testing performed

| Area | Result | Reference |
|------|--------|-----------|
| Linux package build + install smoke | **PASS** | `docs/release/INSTALLER_VALIDATION_REPORT.md` |
| Windows PE + NSIS installer | **PASS** | `docs/release/WINDOWS_ALPHA_INSTALLER_VALIDATION_REPORT.md` |
| Miner local validation | **PASS** | `docs/release/MINER_LOCAL_VALIDATION_REPORT.md` |
| Pool local validation (devtest) | **PASS** | `docs/release/POOL_LOCAL_VALIDATION_REPORT.md` |
| Wallet UAT + convergence UI | **PASS** | `docs/release/QT_WALLET_MAINNET_PRODUCT_ACCEPTANCE_REPORT.md` |
| Unit tests (`res_proof_math`, `convergence_consensus`, `convergence_window_ui`) | **PASS** | official tree verification |
| Local mainnet simulation (full acceptance) | **PASS** | `docs/release/local_simulation/` |

---

## Build from source

```bash
cd core
make clean
./autogen.sh
./configure
make -j$(nproc) rigord rigor-wallet rigor-cli

cd ../miner && ./autogen.sh && ./configure && make

# Linux alpha packages:
bash ../scripts/alpha/build_alpha_linux.sh
```

---

## Known limitations

1. **Public seeds pending** — no production DNS seed or hard-coded seed deployment.
2. **Public infrastructure pending** — explorer, monitoring, and operator runbooks target
   closed-testnet / local alpha only.
3. **Pool production deployment pending** — Stratum pool validated locally (`rigor-pool-pilot`
   devtest); production pool ops not included in this alpha.
4. **Not Guix-signed** — binaries are build-host artifacts, not reproducibly signed mainnet
   releases.
5. **AppImage** — when `appimagetool` is unavailable, the AppImage is a portable launcher
   that extracts `rigor-wallet.AppDir` on first run.

---

## Security

See [SECURITY.md](../../SECURITY.md) and [docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md](../MAINNET_PROTOCOL_FREEZE_DECLARATION.md).

Report issues responsibly via the channels listed in SECURITY.md.

---

## License

RIGOR Core is released under the MIT License. See [LICENSE](../../LICENSE).
