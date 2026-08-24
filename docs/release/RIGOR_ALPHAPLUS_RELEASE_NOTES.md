# RIGOR v0.1.2-alphaplus Release Notes

| Field | Value |
|-------|-------|
| **Version** | `v0.1.2-alphaplus` |
| **Codename** | AlphaPlus Consensus Freeze |
| **Date** | 2026-08-23 |
| **Source branch** | `main` |
| **Windows installer** | NSIS shortcuts launch with `-datadir=%APPDATA%\RIGOR` |
| **Linux datadir** | `~/.rigor` (not `~/.bitcoin`) |

Installer packages are **GitHub Release assets**, not git blobs. Do not `git add` the tarballs or NSIS executables.

**User downloads on this tag:**

- `rigor-v0.1.2-alphaplus-windows.exe` — NSIS setup wizard
- `rigor-v0.1.2-alphaplus-windows.zip` — same wizard inside a zip
- `rigor-v0.1.2-alphaplus-linux.tar.gz` — stripped Linux `bin/` + config

Default data directory is `%APPDATA%\RIGOR` / `~/.rigor`. Config file is `rigor.conf`.

This tag **replaces `v0.1.1-alphaplus` as the current user download** for the wallet GUI line. Consensus (genesis, PoW, W/T, RES, fees) is unchanged from AlphaPlus.

---

## Wallet / product changes in this release

- Product display name is **Rigor Wallet** (title bar, splash, About, installer shortcuts).
- Windows fee radios use a solid selected indicator (Fusion style + filled check/radio).
- Removed obsolete **Auto-settle Convergence when mining locally** option (coinbase `OP_RES_MINT` is automatic).
- Removed **Settings → Advanced → System Diagnostics** page.
- About dialog includes a clickable GitHub releases link: https://github.com/rigorchain/rigorchain/releases
- Earlier GUI fixes included in this package: Send destroy-only Convergence, history ORU/RES filters, self-send amount display, NSIS header wordmark.

---

## Product

**RIGOR** is a Bitcoin-derived full node with ORU proof-of-work and deterministic **RES
Convergence**. Asset classification is parsed from `scriptPubKey` (Witness v0 / v1 only).

Genesis:

```
1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758
```

---

## Installer sizes (approximate)

| Asset | Name | Size |
|-------|------|------|
| Windows installer | `rigor-v0.1.2-alphaplus-windows.exe` | ~19 MiB |
| Windows zip (same installer) | `rigor-v0.1.2-alphaplus-windows.zip` | ~19 MiB |
| Linux portable | `rigor-v0.1.2-alphaplus-linux.tar.gz` | ~24 MiB |

---

## Version banner

```
RIGOR version v0.1.2-alphaplus
```

Use `scripts/alpha/publish_github_release_alphaplus.sh` from `~/rigor-official-tree` only.
