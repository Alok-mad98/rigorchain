# RIGOR v0.1.3 Release Notes

| Field | Value |
|-------|-------|
| **Version** | `0.1.3` |
| **Tag** | `v0.1.3` |
| **Channel** | `stable` |
| **ALPHAPLUS_RELEASE** | `false` |
| **Date** | 2026-09-03 |
| **Nature** | Release engineering / distribution. **Not** a consensus change. |

Official user downloads use the P0-092 names. The retired AlphaPlus names
(`rigor-v0.1.2-alphaplus-*`) are **not** official v0.1.3 assets.

---

## v0.1.3 changes

- Production **stable** channel: `VERSION=0.1.3`, tag `v0.1.3`, `ALPHAPLUS_RELEASE=false`.
- Official multi-platform artifact names (architecture required):
  - `rigor-v0.1.3-windows-x86_64.exe`
  - `rigor-v0.1.3-windows-x86_64.zip`
  - `rigor-v0.1.3-linux-x86_64.tar.gz`
  - `rigor-v0.1.3-macos-x86_64.dmg`
  - `rigor-v0.1.3-macos-arm64.dmg`
- GitHub Actions release matrix (`.github/workflows/release-build.yml`) builds
  Windows x86_64, Linux x86_64, macOS Intel (`macos-15-intel`), and macOS
  Apple Silicon (`macos-15`). WSL cannot generate macOS binaries.
- Release integrity file: `SHA256SUMS` plus per-artifact `.sha256` sidecars.
- No Consensus Kernel, Transaction Kernel, Ledger, ConnectBlock,
  DisconnectBlock, CoinsDB, wallet SQLite schema, classifier, PoW, ASERT, or
  protocol-parameter changes in this release.

---

## Supported platforms

| Platform | Arch | Artifact |
|----------|------|----------|
| Windows | x86_64 | `.exe` installer and `.zip` |
| Linux | x86_64 | `.tar.gz` |
| macOS Intel | x86_64 | `.dmg` (GitHub-hosted `macos-15-intel`) |
| macOS Apple Silicon | arm64 | `.dmg` (GitHub-hosted `macos-15` / `macos-latest`) |

---

## Installation

### Windows

1. Download `rigor-v0.1.3-windows-x86_64.exe` **or** the zip and extract it.
2. Run the installer (setup wizard) or use `rigord.exe` from the zip.
3. Datadir: `%APPDATA%\RIGOR`. Config: `%APPDATA%\RIGOR\rigor.conf`.

### Linux

```bash
tar -xzf rigor-v0.1.3-linux-x86_64.tar.gz
./bin/rigord --version
```

Datadir: `~/.rigor`. Config: `~/.rigor/rigor.conf`.

### macOS

1. Download `rigor-v0.1.3-macos-x86_64.dmg` (Intel) or
   `rigor-v0.1.3-macos-arm64.dmg` (Apple Silicon).
2. Open the disk image and copy the application out of the volume.
3. These images are produced on GitHub-hosted macOS runners, not on WSL.

---

## SHA256 verification

Download `SHA256SUMS` from the same GitHub Release, then:

```bash
sha256sum -c SHA256SUMS
```

On macOS:

```bash
shasum -a 256 -c SHA256SUMS
```

Expected official basenames (no `alphaplus` substring):

```
rigor-v0.1.3-windows-x86_64.exe
rigor-v0.1.3-windows-x86_64.zip
rigor-v0.1.3-linux-x86_64.tar.gz
rigor-v0.1.3-macos-x86_64.dmg
rigor-v0.1.3-macos-arm64.dmg
```

---

## Known limitations

- Packed `CLIENT_VERSION` remains `25.0.0` (wallet / fee-estimate file versions
  are not rewritten).
- Existing operator-built `rigord` may still print the previous display banner
  `v0.1.2-alphaplus` until a later identity-stamp rebuild. The **release
  channel and artifact names** are v0.1.3 stable.
- macOS `.dmg` files are not generated on WSL. Use the GitHub Actions macOS
  jobs from P0-092.
- This is not a Guix-reproducible signed/notarized build.
- Consensus rules, CoinsDB format, and wallet SQLite schema are unchanged.
- A required re-run of `bash tools/run-release-guardian-full-pipeline-audit.sh`
  on this WSL host is **BLOCKED** because in-tree `core/src/rigord` is older
  than `core/src/init.cpp` **mtime**. `git diff` on `init.cpp` is empty
  (no consensus edit). Rebuild `rigord` on the operator machine to clear
  the stale-binary gate.

---

## Security boundary

```
CONSENSUS_CODE_CHANGED=FALSE
DATABASE_FORMAT_CHANGED=FALSE
WALLET_FORMAT_CHANGED=FALSE
ALPHAPLUS_RELEASE=false
```

Reproduce the local release assembly:

```
bash tools/run-github-production-release-v0.1.3.sh
```
