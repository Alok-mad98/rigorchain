# RIGOR v0.2 Release Notes

| Field | Value |
|-------|-------|
| **Version** | `0.2` |
| **Tag** | `v0.2` |
| **Title** | RIGOR v0.2 |
| **Date** | 2026-09-15 |
| **Channel** | `stable` |
| **Nature** | First stable mainnet distribution. **Not** a consensus-parameter change. |

```
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
```

Packed `CLIENT_VERSION` remains `25.0.0`. Product tag is **v0.2**.

Official names follow [v0.1.3](https://github.com/rigorchain/rigorchain/releases/tag/v0.1.3).
Retired AlphaPlus names (`rigor-v0.1.2-alphaplus-*`) are not v0.2 assets.

---

## v0.2 changes

- Wallet Convergence warning is **Destroyed ORU cannot be recovered.**
- Mint path is RFC-020 `destroy_cost(h)` (no epoch pad, no `RATE(k)` table).
- SOLO miner is RFC-020 copy-bytes GBT (128 ORU coinbase, RES is not mined).
- Official wallet packages do **not** include `rigor-miner`.
- Miner is a parallel product with its own Windows / Linux assets.

---

## Supported platforms

| Platform | Arch | Artifact |
|----------|------|----------|
| Windows | x86_64 | `rigor-v0.2-windows-x86_64.exe` and `.zip` |
| Linux | x86_64 | `rigor-v0.2-linux-x86_64.tar.gz` |
| macOS Intel | x86_64 | `rigor-v0.2-macos-x86_64.dmg` (**pending CI**) |
| macOS Apple Silicon | arm64 | `rigor-v0.2-macos-arm64.dmg` (**pending CI**) |
| Windows miner | x86_64 | `rigor-v0.2-miner-windows-x86_64.exe` and `.zip` |
| Linux miner | x86_64 | `rigor-v0.2-miner-linux-x86_64.tar.gz` |

---

## Installation

### Windows wallet

1. Download `rigor-v0.2-windows-x86_64.exe` **or** the zip and extract it.
2. Run the installer or use `rigord.exe` from the zip.
3. Datadir: `%APPDATA%\RIGOR`. Config: `%APPDATA%\RIGOR\rigor.conf`.

### Linux wallet

```bash
tar -xzf rigor-v0.2-linux-x86_64.tar.gz
./bin/rigord --version
```

Datadir: `~/.rigor`. Config: `~/.rigor/rigor.conf`.

### Windows / Linux miner

Wallet first, RPC on `127.0.0.1:8761`. Payout must be ORU (`rg1q…`). RES is not mined.

```bash
tar -xzf rigor-v0.2-miner-linux-x86_64.tar.gz
./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:8761 \
  -u RPCUSER -p RPCPASS --coinbase-addr=rg1q... -t 4
```

### macOS

`rigor-v0.2-macos-x86_64.dmg` and `rigor-v0.2-macos-arm64.dmg` will be uploaded
after `.github/workflows/release-wallet-macos.yml` completes on GitHub-hosted
macOS runners. WSL cannot emit Mach-O / UDZO.

---

## SHA256 verification

Download `SHA256SUMS` from this GitHub Release, then:

```bash
sha256sum -c SHA256SUMS
```

On macOS (after the DMGs land):

```bash
shasum -a 256 -c SHA256SUMS
```

---

## Known limitations

These are packaging and signing facts. They do **not** mark v0.2 as a test
or experimental channel.

- Not a Guix-reproducible signed/notarized build.
- Windows installers are unsigned.
- macOS DMGs attach when the GitHub-hosted macOS jobs finish.
- Formal verification of the running C++ binary is **not** claimed.

## What this is not

- Not a consensus-parameter change
- Not CPP verified
- Not Level 5
