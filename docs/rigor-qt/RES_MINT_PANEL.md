# RES Mint Panel — TASK-P2-004

**Workspace:** `rigor-qt-prototype/src/resmintpanel.*`  
**Mode:** Read-only GUI — no consensus, protocol, wallet, or miner-agent code changes.

## Data sources

| Source | Endpoint | Role |
|--------|----------|------|
| rigord | `getresmintstatus` (JSON-RPC 1.0) | Authority cross-check when deployed on node |
| explorer-indexer-mvp | `GET /api/res/mints?limit=20` | Mint rows, `proof_items`, linkage fields |

**Note:** `getresmintstatus` is specified for this panel; current frozen `rigord` may return RPC `-32601` (method not found). The panel **soft-fails** that call and continues with explorer-indexed mint rows. No `core/` changes are made to add the RPC.

## Displayed fields

| UI field | Primary source |
|----------|----------------|
| Mint TXID | `mints[].mint_txid` |
| Vout | `mints[].mint_vout_index` |
| RES amount | `mints[].mint_amount` (atoms → RES) |
| Winner convergence TXID | `mints[].winner_convergence_txid` |
| Block | `block_height` + `block_hash` |
| Confirmations | `tip_height - block_height + 1` from `getblockchaininfo.blocks` |
| State | Confirmed / Pending |
| Proof authentication | None on the proof itself — the PCORB witness is unsigned (SECURITY-001-A / A-7 removed `sigma_signature` at witness index 6 and its key). The mint is authenticated by the ORU funding input on `vin[0]` |
| ORU card | Linked convergence TXID only — **no merged balance** |

## UX

- Copy buttons: mint TXID, vout, proof hex
- **Open /tx** opens `{explorerUrl}/tx/{txid}` in system browser
- Gray placeholder: `Data unavailable — read-only panel` when explorer fetch fails or mint list empty
- Footer: `rigord authoritative — equivalence_claimed: false`
- Refresh: 30s auto + main **Refresh** button

## Settings

**File → RPC Settings** — rigord URL/credentials + **Explorer base URL** (default `http://127.0.0.1:8080`).

## Compliance

- Read-only — no mint, sign, or broadcast controls
- ORU and RES shown in separate group boxes
- `equivalence_claimed: false` in panel footer
- No mock fallback

## Required verdict

| Field | Status |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |
