# RIGOR v0.1.1-alphaplus Release Notes

| Field | Value |
|-------|-------|
| **Version** | `v0.1.1-alphaplus` |
| **Codename** | AlphaPlus Consensus Freeze |
| **Date** | 2026-08-17 |
| **Source branch** | `mainnet-release-candidate-001` |
| **Windows installer** | NSIS shortcuts launch with `-datadir=%APPDATA%\RIGOR` |
| **Linux datadir** | `~/.rigor` (not `~/.bitcoin`) |

Installer packages are **GitHub Release assets**, not git blobs. Do not `git add` the tarballs or NSIS executables.

**2026-08-17 packaging fix.** User downloads on this tag are:

- `rigor-v0.1.1-alphaplus-windows.exe` — NSIS setup wizard
- `rigor-v0.1.1-alphaplus-windows.zip` — same wizard inside a zip
- `rigor-v0.1.1-alphaplus-linux.tar.gz` — stripped Linux `bin/` + config

Do not ship `debug-start.cmd` or `rigor-wallet-boot.log` in those packages (local diagnostics only). Windows shortcuts launch `rigor-wallet.exe` directly. Re-download if you still have an earlier file from this tag.

Default data directory is `%APPDATA%\RIGOR` / `~/.rigor`. Config file is `rigor.conf` (`bitcoin.conf` is still read if `rigor.conf` is absent).

This tag **replaces `v0.1.0-alpha` as the current user download**. Linux packages are produced by `scripts/alpha/build_alpha_linux.sh` (copy + `strip --strip-unneeded`, then tar/deb). They are not unstripped debug dumps.

---

## Product

**RIGOR** is a Bitcoin-derived full node with ORU proof-of-work and deterministic **RES
Convergence**. Asset classification is parsed from `scriptPubKey` (Witness v0 / v1 only).
There is no parallel `asset` field on `CTxOut`.

This release is the AlphaPlus identity line (`rg` HRP, RFC-016 genesis). It is **not** a
Guix-signed production launch.

Genesis:

```
1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758
```

---

## What this GitHub tag contains

**Git** (this tag / branch tip) includes operator fixes landed after the installer rebuild:

- Explorer: A-8 mint is indexed at payout height `h+1`; false CRITICAL banners from
  same-block `committed ≠ computed` and genesis zero computed are dropped.
- Pool: A-9-C window shares are accepted without requiring confirmed F txids in the
  current template body; the pool still does **not** construct or redirect RES.
- Node: automatic RES mint is counted in `nBlockTx`; mint skip paths log a reason.

**Installers** (Release assets) are stripped user packages. Rebuild from this tag if you
need later source commits inside the Qt wallet / `rigord` PE/ELF.

---

## Download

Same layout as `v0.1.0-alpha`, plus AlphaPlus versioned names.

| Platform | Artifact | Typical size |
|----------|----------|----------------|
| Windows installer | `rigor-v0.1.1-alphaplus-windows.exe` | ~19 MiB |
| Windows zip (same installer) | `rigor-v0.1.1-alphaplus-windows.zip` | ~19 MiB |
| Linux portable | `rigor-v0.1.1-alphaplus-linux.tar.gz` | ~24 MiB |

Checksums: `SHA256SUMS` on this Release.

```bash
sha256sum -c SHA256SUMS
```

Do **not** download an unstripped ~360 MiB Linux wallet tarball. That was a debug dump and is not the user installer.

---

## Identity check (installed binary)

Linux (`bin/rigord -version`):

```
RIGOR version v0.1.1-alphaplus
AlphaPlus Consensus Freeze
BUILD_GIT_COMMIT=19454804ead8-dirty
NETWORK=mainnet
```

Windows installer identity is `v0.1.1-alphaplus` / AlphaPlus Consensus Freeze.

---

## Publish path

Use `scripts/alpha/publish_github_release_alphaplus.sh` from `~/rigor-official-tree` only.
RING is not a publish source. Binaries stay out of git (100 MiB git limit).

The historical `v0.1.0-alpha` Release remains at
https://github.com/rigorchain/rigorchain/releases/tag/v0.1.0-alpha
(`scripts/alpha/publish_github_release.sh`).
