# RIGOR Public Site — Site Architecture

**Task:** `TASK-PUBLICATION-002`  
**Document:** Official public website generation blueprint — architecture  
**Date:** 2026-06-13  
**Type:** Documentation + UI structure specification only — **no protocol, consensus, mining, or backend changes**  
**Content authority:** [RIGOR_UNIFIED_WHITEPAPER_v1.0.md](../../docs/WHITEPAPER.md)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`

| Field | Value |
|-------|-------|
| Architecture Impact | **NONE** (new static site shell spec) |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| `equivalence_claimed` | **false** |

---

## 1. System goal

The RIGOR public website is a **deterministic system visualization interface** and **scientific publication portal**. It exposes four non-equivalent layers:

| Layer | Site surface | Authority |
|-------|--------------|-----------|
| **Protocol** | `/protocol` | Frozen specification (engineering truth) |
| **Axiom** | `/axioms` | Mathematical invariants |
| **Lab** | `/lab`, `/verification` | Scientific interpretation |
| **Runtime explorer** | `/explorer` | Read-only observation of live state |

The site **does not** implement wallet, mining, or transaction submission. It **does not** claim equivalence between layers.

---

## 2. Route tree

```text
/                          Home — identity, layer map, live status summary
├── /protocol              Protocol Layer viewer (§1 publication)
├── /axioms                Axiom Layer viewer (§2 publication)
├── /lab                   Lab narrative viewer (§3 publication)
├── /explorer              Read-only blockchain / RPC state explorer UI
├── /verification          PROOF-034 + runtime soak status
└── /system-status         Global health: node, RPC, explorer, gates
```

### 2.1 Route-to-publication mapping

| Route | Publication source | Primary widgets |
|-------|-------------------|-----------------|
| `/` | Charter + §4.2 Home | Hero, three pillars, `SystemStatusBar` |
| `/protocol` | §1.1–§1.5 | `ProtocolViewer` tree |
| `/axioms` | §2.1–§2.6 | `AxiomViewer` + math blocks |
| `/lab` | §3.1–§3.5 | `LabViewer` + separation diagram |
| `/explorer` | §3.2 visualization + §3 data binding | `ExplorerModule` |
| `/verification` | §3.2 PROOF-034 + §5 release | Milestone table, evidence grades |
| `/system-status` | §6 + release gates | Full diagnostic panel |

### 2.2 Navigation model

**Primary nav (persistent):** Home · Protocol · Axioms · Lab · Explorer · Verification · System Status

**Secondary nav (footer):** Documentation links · `equivalence_claimed: false` · experimental disclaimer · release tier badges (CT GO / PT CONDITIONAL / M NO-GO)

---

## 3. Global layout shell

Every page renders inside `AppShell`:

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ SystemStatusBar (§6) — height, RPC, explorer, global ONLINE/DEGRADED…    │
├─────────────────────────────────────────────────────────────────────────┤
│ TopNav — logo, primary routes, equivalence_claimed badge                │
├─────────────────────────────────────────────────────────────────────────┤
│ ReadOnlyBanner — "Read-only observer · rigord is authority"               │
├─────────────────────────────────────────────────────────────────────────┤
│ <main> — page content (route-specific modules)                            │
├─────────────────────────────────────────────────────────────────────────┤
│ Footer — disclaimer, doc links, last refresh timestamp                  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Mandatory chrome (all pages)

| Element | Requirement |
|---------|-------------|
| **Top navigation bar** | All primary routes; active route highlighted |
| **System status indicator** | Aggregate: `ONLINE` \| `DEGRADED` \| `OFFLINE` \| `SYNCING` |
| **`equivalence_claimed: false` badge** | Always visible in header; red if API reports `true` (fault state) |
| **Read-only enforcement banner** | Non-dismissible; states no signing / no submission |

### 3.2 Layout grid

- 12-column responsive grid; max content width `1200px`
- Monospace column for hashes/heights; prose column for narrative
- See [STYLE_GUIDE.md](./STYLE_GUIDE.md)

---

## 4. Application layers (frontend)

```text
┌──────────────────────────────────────────────────────────────┐
│ Presentation (React/Vite static site — implementation TBD)    │
├──────────────────────────────────────────────────────────────┤
│ State orchestration (per-route UI state machine)              │
├──────────────────────────────────────────────────────────────┤
│ Data adapters (RPC proxy, explorer HTTP — read-only)          │
├──────────────────────────────────────────────────────────────┤
│ Static content (markdown from publication corpus)             │
└──────────────────────────────────────────────────────────────┘
```

**Static publication content** (protocol/axiom/lab prose) is baked at build time from `docs/publication/` and `docs/whitepaper/AXIOMS_v1.0.md`.

**Live data** flows only through adapters defined in [ROUTE_AND_DATA_BINDING_SPEC.md](./ROUTE_AND_DATA_BINDING_SPEC.md).

---

## 5. Module registry

| Module ID | Route(s) | Spec |
|-----------|----------|------|
| `SystemStatusBar` | Global | §6 below; [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) §2.5 |
| `ProtocolViewer` | `/protocol` | [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) §2.1 |
| `AxiomViewer` | `/axioms` | [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) §2.2 |
| `LabViewer` | `/lab` | [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) §2.3 |
| `ExplorerModule` | `/explorer` | [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) §2.4 |
| `VerificationPanel` | `/verification` | PROOF-034 widgets + static copy |
| `GateStatusPanel` | `/system-status`, footer | Release gate summary (static + optional ops JSON) |

---

## 6. System status module (global header)

`SystemStatusBar` is the **single aggregate health surface**. It MUST display:

| Field | Source | Display |
|-------|--------|---------|
| **Node status** | RPC `getblockchaininfo` reachability | `ONLINE` / `OFFLINE` |
| **Chain height** | `getblockchaininfo.blocks` | Monospace integer |
| **Convergence state** | `getconvergencestatus` | Summary or `DEGRADED` if unreachable |
| **RPC status** | Last RPC probe latency + error | Green / amber / red |
| **Explorer status** | `GET /api/status` | Green / amber / red |
| **Global aggregate** | Derived (see §7.1 in ROUTE spec) | Badge color |

**Refresh:** 30s interval on status routes; manual refresh button always visible.

**No mock values:** If RPC fails, show `—` with `OFFLINE` or `DEGRADED` label — never `0` or placeholder height.

---

## 7. Security and trust model

| Rule | Enforcement |
|------|-------------|
| **Read-only system** | No `send*`, `submitblock`, or wallet RPC in client |
| **No signing in UI** | No private key fields, no PSBT, no broadcast buttons |
| **Explorer is untrusted display** | Label: "Indexer mirror — defer to rigord on mismatch" |
| **rigord is authority** | RPC values supersede explorer when both present and differ |
| **All mismatch visible** | `MismatchBanner` when `committed ≠ computed` fields differ |
| **No hidden state** | Every live value shows `source`, `fetched_at`, `error` if any |

CORS: public site calls explorer API and a **read-only RPC proxy** (operator-deployed); site spec does not modify `rigord`.

---

## 8. Build and deployment (spec only)

| Artifact | Path (proposed) |
|----------|-----------------|
| Site source root | `web/public-site/` |
| Static content | `web/public-site/content/` (synced from publication docs) |
| Env config | `VITE_RPC_PROXY_URL`, `VITE_EXPLORER_API_URL` |
| Output | `web/public-site/dist/` (static hosting) |

**Non-goals:** No wallet UI, mining UI, transaction sending, RPC server changes, consensus changes.

---

## 9. Acceptance criteria

| Field | Status |
|-------|--------|
| Protocol page mapping | **COMPLETE** |
| Axiom rendering system | **COMPLETE** (see UI_LAYOUT_SPEC) |
| Lab narrative system | **COMPLETE** |
| Explorer integration spec | **COMPLETE** |
| Routing model | **COMPLETE** (see ROUTE_AND_DATA_BINDING_SPEC) |
| State machine design | **COMPLETE** |
| Visual system spec | **COMPLETE** (see STYLE_GUIDE) |
| No mock data policy | **COMPLETE** |
| Freeze compliance | **COMPLETE** |
| `equivalence_claimed` | **false** |

---

## 10. Related documents

| Document | Role |
|----------|------|
| [UI_LAYOUT_SPEC.md](./UI_LAYOUT_SPEC.md) | Component-level UI |
| [ROUTE_AND_DATA_BINDING_SPEC.md](./ROUTE_AND_DATA_BINDING_SPEC.md) | Data sources + UI state machine |
| [STYLE_GUIDE.md](./STYLE_GUIDE.md) | Visual identity |
| [RIGOR_UNIFIED_WHITEPAPER_v1.0.md](../../docs/WHITEPAPER.md) | Content authority |
| `WEBSITE_NARRATIVE_FRAMEWORK.md` | Narrative constraints |

---

## 11. Bridge role

This architecture is the bridge between:

- **Protocol system** — engineering truth in `/protocol`
- **Scientific model** — axioms in `/axioms`, lab in `/lab`
- **Public visualization** — explorer in `/explorer`

Implementation of the static site follows this blueprint in a separate engineering task; TASK-PUBLICATION-002 delivers **specification only**.
