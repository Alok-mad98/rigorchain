# Official packaging (frozen)

**Document status:** Production documentation  
**Naming template:** [GitHub Release v0.1.3](https://github.com/rigorchain/rigorchain/releases/tag/v0.1.3)

This is the **only** wallet packaging flow. Do not invent new artifact names
(`rigor-wallet-v0.2-linux`, `rigor-wallet-v0.2-windows.exe`, alphaplus, …).

```
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
```

Packed `CLIENT_VERSION` stays `25.0.0`. Display version is
`RIGOR_RELEASE_VERSION` in `core/src/clientversion.h`.

The assembler **does not push**. GitHub Release / tag is a separate operator
step.

---

## One command

```bash
bash tools/release/build-official.sh
```

| Env | Default | Meaning |
|-----|---------|---------|
| `RELEASE_VERSION` | from `clientversion.h` (`v0.2` → `0.2`) | Product tag without `v` |
| `CHANNEL` | `internal-mainnet-validation` | README / manifest channel |
| `SKIP_LINUX=1` | rebuild Linux | Reuse `staging/linux` |
| `SKIP_WINDOWS=1` | rebuild Windows PE+NSIS | Reuse `staging/windows` |
| `FORCE=1` | reuse if present | Rebuild even if staging exists |
| `SKIP_DEPENDS=1` | `1` on Windows PE | Skip mingw depends |

Assemble only (bins already staged):

```bash
bash tools/release/package-official.sh
```

Historical v0.1.3 assembler stays at `tools/release/package-official-v0.1.3.sh`.
`package-official-v0.2.sh` is a wrapper around `package-official.sh`.

---

## Official asset names

Same pattern as v0.1.3:

```
rigor-v${VER}-windows-x86_64.exe
rigor-v${VER}-windows-x86_64.zip
rigor-v${VER}-linux-x86_64.tar.gz
rigor-v${VER}-macos-x86_64.dmg      # not built on WSL
rigor-v${VER}-macos-arm64.dmg       # not built on WSL
SHA256SUMS
release_v${VER}_manifest.json
```

Windows always has **two** user files: NSIS `.exe` and `.zip` (installer +
`rigord.exe` + `rigor-cli.exe` + `README.txt`).

Linux tarball root is `README.txt` + `bin/` (no nested folder).

macOS DMGs: GitHub-hosted `macos-15-intel` / `macos-15`. Never ship stub DMGs.

---

## Layout

| Path | Role |
|------|------|
| `dist/release/v${VER}/staging/linux/bin/` | Stripped ELF: `rigord`, `rigor-cli`, `rigor-wallet`, `rigor-qt` |
| `dist/release/v${VER}/staging/windows/` | NSIS `rigor-v${VER}-windows-x86_64.exe` + `rigord.exe` + `rigor-cli.exe` |
| `dist/release/v${VER}/official/` | GitHub-shaped outputs (`dist/` is gitignored) |
| `audit/wallet_packaging/official/` | Visible copy of `official/` |

---

## Scripts

| Script | Does |
|--------|------|
| `tools/release/build-official.sh` | Linux stage → Windows stage → assemble |
| `tools/release/stage-official-linux.sh` | Out-of-tree `core/build-linux-wallet` (no in-tree / mingw distclean) |
| `tools/release/stage-official-windows.sh` | `SKIP_MINER=1` PE + NSIS wallet installer |
| `tools/release/package-official.sh` | Names, tar, zip, SHA256SUMS, manifest, visible copy |
| `tools/release/official_packaging.py` | Version parse + official names |

Windows PE build lives in `scripts/alpha/build_alpha_windows_pe.sh`. Wallet
packaging **must** use `SKIP_MINER=1`. Miner verify FAIL on that script is
ignored when wallet PEs exist.

Do **not** use `scripts/alpha/build_alpha_windows_nsis.sh` for official
assets (it also stages miner / alphaplus publish paths).

---

## Miner (parallel product; not in the wallet tar)

Wallet packages stay miner-free. SOLO miner packages for internal
mainnet simulation:

```
bash tools/release/build-official-miner.sh
```

| Asset | Role |
|-------|------|
| `rigor-v${VER}-miner-windows-x86_64.exe` | Miner NSIS |
| `rigor-v${VER}-miner-windows-x86_64.zip` | Miner zip + dashboard |
| `rigor-v${VER}-miner-linux-x86_64.tar.gz` | `README.txt` + `bin/rigor-miner` |
| `SHA256SUMS` | Miner checksums only |
| `release_v${VER}_miner_manifest.json` | Miner manifest |

Outputs: `dist/release/v${VER}/miner/` and `audit/miner_packaging/official/`.

---

## Publish (operator, not this script)

When you choose to push, attach everything in
`audit/wallet_packaging/official/` (or `dist/release/v${VER}/official/`) to
tag `v${VER}`. GitHub adds Source code zip/tar.gz. Drop macOS DMGs in when
the macOS jobs finish.
