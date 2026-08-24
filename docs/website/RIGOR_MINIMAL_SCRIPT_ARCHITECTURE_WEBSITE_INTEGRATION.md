# RIGOR Minimal Script Architecture — Website Integration

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-WHITEPAPER-001`  
**Date:** 2026-08-19  
**Layer:** Website / documentation only  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

Binds the minimal-script architecture chapter into `web/rigorchain-site/` using the existing black / white / gray technical theme. Does not change consensus, RFCs, wallet, miner, or interpreter code.

---

## Routes

| Route | Page | Status |
|-------|------|--------|
| `/documentation/security` | `SecurityArchitecturePage` | Live hub (was placeholder) |
| `/documentation/security/minimal-script-architecture` | `MinimalScriptArchitecturePage` | Live long-form chapter |

Navigation:

```
Documentation
|
Security Architecture
|
Minimal Script Architecture
```

Navbar shows that cluster when the path is under `/documentation/security`. Documentation SubNav lists both pages. Footer includes Security Architecture.

Theme tokens reused: `--bg-primary` `#000000`, `--text-primary` `#ffffff`, `--bg-panel` `#1a1a1a`. Components: `SubNav`, `DarkCard`, `SectionTitle`, existing `.card` / `.prose-section` / `.pipeline-diagram` / `.data-table`.

---

## Files not changed

- `core/src/kernel/`
- `core/src/consensus/`
- `core/src/pow.cpp`
- `core/src/validation.cpp`
- script interpreter
- wallet
- miner
- `docs/rfc/`
