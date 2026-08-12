# RIGOR Wallet Platform Matrix

**Task:** `TASK-WALLET-PROGRAM-001`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only product audit

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

This matrix evaluates **two wallet deliverables** per platform:

1. **Full-node wallet** — `rigor-qt`, `rigor-cli`, `rigord` (autotools + CI)
2. **Observer wallet** — `wallet-observer-mvp` (Node.js / Next.js)

---

## Windows

| Dimension | Full-node wallet | Observer wallet |
|-----------|------------------|-----------------|
| **Build status** | **IMPLEMENTED** — MinGW cross-compile in `desktop-release.yml` | **IMPLEMENTED** — `npm run build` (developer machine) |
| **Packaging status** | **PARTIAL** — NSIS via `core/share/setup.nsi` + `make deploy` | **MISSING** — no MSI/installer |
| **Installer status** | **PARTIAL** — NSIS exists; legacy branding (`Arda`, `bitcoincore.org`) | **MISSING** |
| **CI release artifact** | **IMPLEMENTED** — tag-triggered workflow uploads tarball/NSIS | **NOT IN CI** |
| **Missing dependencies** | Qt5, libdb++, Boost, etc. (handled by depends/CI) | Node.js 18+, npm, running explorer + rigord |
| **UX readiness** | **PARTIAL** — functional rigor-qt; installer identity wrong | **NOT READY** — developer-only bring-up |

**Windows blockers:** Rebrand NSIS (`setup.nsi` — `InstallDir $PROGRAMFILES64\Arda`, `arda.ico`, `bitcoincore.org` URL). Ship signed installer. Observer needs Electron/Tauri or documented service wrapper for non-technical users.

---

## Linux

| Dimension | Full-node wallet | Observer wallet |
|-----------|------------------|-----------------|
| **Build status** | **IMPLEMENTED** — `desktop-release.yml` ubuntu-24.04 job | **IMPLEMENTED** — `npm run build` |
| **Packaging status** | **PARTIAL** — tarball from CI; no `.deb`/AppImage | **MISSING** — systemd unit template only |
| **Installer status** | **MISSING** (end-user) — extract tarball + manual setup | **PARTIAL** — `deploy/systemd/wallet-observer-mvp.service` |
| **CI release artifact** | **IMPLEMENTED** | **NOT IN CI** |
| **Missing dependencies** | Qt5 dev packages (CI installs); user needs same runtime libs | Node.js, reverse proxy optional |
| **UX readiness** | **PARTIAL** — works for technical users | **PARTIAL** — operator systemd path documented |

**Linux blockers:** `.deb` or AppImage for rigor-qt. Observer Docker Compose bundle (node + explorer + wallet). AppStream/desktop entry with RIGOR icon.

---

## macOS

| Dimension | Full-node wallet | Observer wallet |
|-----------|------------------|-----------------|
| **Build status** | **IMPLEMENTED** — `desktop-release.yml` macOS job | **IMPLEMENTED** — same as Linux (Node) |
| **Packaging status** | **PARTIAL** — tarball; **DMG not built in CI** ("requires extra signing") | **MISSING** |
| **Installer status** | **MISSING** — `contrib/macdeploy/` exists; not in release CI | **MISSING** |
| **CI release artifact** | **PARTIAL** — tarball only | **NOT IN CI** |
| **Missing dependencies** | Xcode CLI tools, Qt5; codesign/notarize for distribution | Node.js via brew/installer |
| **UX readiness** | **PARTIAL** — build docs in `core/doc/build-osx.md` | **NOT READY** |

**macOS blockers:** DMG + Apple codesign/notarize pipeline. Universal binary decision (arm64 + x86_64). Observer native wrapper optional.

---

## Cross-platform comparison

| Capability | Windows | Linux | macOS |
|------------|---------|-------|-------|
| rigor-qt CI build | Yes | Yes | Yes |
| Signed GA release (Guix) | No | No | No |
| Observer in release pipeline | No | No | No |
| End-user installer | Partial (NSIS) | No | No |
| Non-technical install path | **No** | **No** | **No** |
| Desktop icon in repo | Partial (script only) | Partial | Partial |

---

## Build system reference

| Component | Build system | Entry point |
|-----------|--------------|-------------|
| rigord / rigor-cli / rigor-qt | GNU autotools | `core/autogen.sh && ./configure --with-gui=qt5 && make` |
| Reproducible builds | Guix | `core/contrib/guix/` (not GA-active) |
| Cross-compile | depends/ | MinGW (Windows), SDK (macOS) |
| wallet-observer-mvp | npm / Next.js 14 | `wallet-observer-mvp/package.json` |
| rigor-qt-prototype | qmake | `rigor-qt-prototype/rigor-qt-prototype.pro` |

**Note:** CMake is **not** the wallet build path for core binaries.

---

## Platform readiness verdict

| Platform | Full-node wallet | Observer wallet | Combined product |
|----------|------------------|-----------------|------------------|
| Windows | **PARTIAL** | **NOT READY** | **NOT READY** |
| Linux | **PARTIAL** | **NOT READY** | **NOT READY** |
| macOS | **PARTIAL** | **NOT READY** | **NOT READY** |

**Earliest cross-platform target:** Linux tarball + systemd stack for closed testnet operators (Phase 2 in roadmap). Public testnet requires GUI-17 packaging closure on all platforms.

---

*End of WALLET_PLATFORM_MATRIX — TASK-WALLET-PROGRAM-001*
