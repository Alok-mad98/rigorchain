# RIGOR Wallet Error UX Standardization

**Task:** `TASK-WALLET-PROGRAM-017` — STEP-07  
**Date:** 2026-06-23

| Field | Value |
|-------|--------|
| **`equivalence_claimed`** | **false** |

---

## 1. Principles

1. **No raw error codes** in main UI — use `WarningBanner` and `StatusBadge` with plain language.
2. **Diagnostics retains detail** — technical fields, JSON exports, and `equivalence_claimed` remain on Diagnostics only.
3. **Deterministic severity** — Critical > Warning > Verification > Info.

---

## 2. Components

### WarningBanner levels

| Level | User perception | Example |
|-------|-----------------|---------|
| `Critical` | Cannot proceed safely | Node offline, insufficient ORU |
| `Warning` | Proceed with caution | Wallet locked, mint delayed |
| `Verification` | Informational compliance | Explorer not configured |
| `Info` | Neutral notice | Experimental network |

### StatusBadge colors

| Color | Default label | Use |
|-------|---------------|-----|
| Green | Ready | Connected, verified |
| Yellow | Check | Lag, attention needed |
| Red | Issue | Offline, mismatch |
| Gray | Unavailable | Not configured |

Custom text may override default labels per panel context.

---

## 3. Error message catalog (main UI)

| Condition | Message pattern | Component |
|-----------|-----------------|-----------|
| Node offline | "Node offline — connect to the network." | Convergence / Diagnostics |
| Wallet locked | "Wallet locked for spending." | Convergence / Diagnostics |
| Explorer unreachable | "Explorer not configured…" / lag warnings | Diagnostics |
| Pending convergence | "Convergence pending: {txid prefix}…" | Convergence / Diagnostics |
| Insufficient ORU | "Insufficient ORU for the entered convergence amount." | Convergence |
| Mint delayed | "Mint delayed — RES locked until maturity." | Convergence / Diagnostics |
| Archive verification | "Archive verification in progress (N%)." | Diagnostics only |

---

## 4. Prohibited in main UI

| Prohibited | Alternative |
|------------|-------------|
| `equivalence_claimed = false` as banner text | Human disclaimer in About; raw value in Diagnostics |
| `PROOF-034` in overview banners | Diagnostics verification panel |
| Config key names (`RigorExplorer/ApiBaseUrl`) | "Settings" / "explorer URL" |
| RPC method names in labels | User-facing field names |
| HTTP status codes | "Unreachable" / "Not configured" |

---

## 5. QMessageBox usage

Wizard and send flows use `QMessageBox` with:

- Title: action name (Convergence, Wallet locked)
- Body: actionable plain language
- No error codes or stack traces

---

## 6. Verdict

| Field | Status |
|-------|--------|
| Error UX | **COMPLETE** |
