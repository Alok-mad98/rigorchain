# RIGOR Wallet Brand Resources (Placeholders)

**Task:** `` · **BUILD-07**
**Status:** Placeholder structure only — no final artwork

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

## Directory layout

```
resources/branding/
├── logos/       # Primary mark, app icon source (1024×1024 PNG)
├── icons/       # Navigation + status SVG/PNG (66-icon pack)
├── splash/      # Splash screen artwork
└── installer/   # NSIS/DMG/AppImage pixmaps
```

## Phase A policy

- Directories exist for pipeline wiring (`build_app_icon.py`, NSIS, qrc).
- Final assets deferred to Phase G logo sprint (PROGRAM-006/007).
- Do not commit proprietary artwork until design gate passes.

## Expected consumers

| Asset | Consumer |
|-------|----------|
| `logos/rigor-primary.png` | `res/icons/build_app_icon.py` |
| `splash/rigor-splash.png` | `res/images/splash-screen.png` |
| `icons/*.svg` | `bitcoin.qrc` / navigation toolbar |
| `installer/*.bmp` | `share/setup.nsi` |
