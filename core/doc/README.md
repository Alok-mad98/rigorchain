# Rigor Core documentation

This directory is the Bitcoin-Core-derived developer documentation that
ships with the node sources.

**Start here instead:**

- Repository homepage: [`../../README.md`](../../README.md)
- Build: [`../../docs/developer/Build.md`](../../docs/developer/Build.md) and `build-unix.md` / `build-osx.md` / `build-windows.md` in this folder
- Wallet: [`../../docs/user/Wallet.md`](../../docs/user/Wallet.md)
- Protocol: [`../../docs/protocol/RFC-020.md`](../../docs/protocol/RFC-020.md)

Default data directory (from `GetDefaultDataDir()`):

| Platform | Path |
|----------|------|
| Linux | `~/.rigor` |
| macOS | `~/Library/Application Support/RIGOR` |
| Windows | `%APPDATA%\RIGOR` |

Configuration file: `rigor.conf` in the data directory.

```
equivalence_claimed = false
```
