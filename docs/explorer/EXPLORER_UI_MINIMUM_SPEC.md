# Explorer UI Minimum Specification

**Task:** `TASK-EXPLORER-BOOTSTRAP-001`  
**Audience:** Closed testnet operators — not public polish.  
**Baseline:** `mainnet-freeze-rfc014-2026-05-25`.

---

## 1. Global UI requirements

| Requirement | Implementation |
|-------------|----------------|
| Asset labeling | Every amount shows **ORU** or **RES** unit |
| Frozen disclaimer | Footer link to `WITNESS_V1_DISPLAY_RULES.md` bind disclaimer |
| Terminology | No “Taproot” / “BIP341” |
| Identity | Canonical URLs use **txid** |
| Chain badge | “Rigor testnet (closed)” + genesis short hash |
| Error state | Root mismatch banner (red, non-dismissible) |

---

## 2. Page: Blocks (`/blocks`)

| Element | Data source |
|---------|-------------|
| Table columns | height, hash (truncated), time, tx count, `rigor_res_minted`, `rigor_winner` (short), `draw_candidate_root` (short) |
| Row link | `/block/{height}` or `{hash}` |
| Filters | RES minted only (toggle) |
| Pagination | 50 per page, height DESC |

---

## 3. Page: Block detail (`/block/{hash}`)

| Section | Content |
|---------|---------|
| Header | All STABLE header fields including `res_probability_target`, `bits` (labeled PoW), `opus_hash` |
| Convergence panel | `getconvergenceresult` summary: \|F\|, committed/computed roots, winner txid |
| Transactions | txid list with badges: coinbase, convergence, res_mint, res_transfer |
| Mismatch alert | If committed ≠ computed |

---

## 4. Page: Transactions (`/tx/{txid}`)

| Section | Content |
|---------|---------|
| Header | txid (large), wtxid (small, labeled), block link, confirmations |
| Inputs/outputs | Values with ORU/RES tags |
| Witness panel | For v1 RES: 2-item key-path (the only RES spend path since A-7). For a RES mint's ORU funding input: 6-item PCORB table |
| Convergence | If `OP_CONVERGE`: burn amount, Script `OP_CONVERGE`, Asset ORU, State unspendable burn / draw participation; link to convergence lineage |
| Mint proof | If RES mint: replay binding sidebar |

**Reject:** `/tx/{wtxid}` routes without redirect to txid.

---

## 5. Page: Addresses (`/address/{bech32}`)

| Element | Notes |
|---------|-------|
| Type detection | v0 ORU vs v1 RES vs invalid |
| UTXO list | From `scantxoutset` / indexer cache |
| History | Tx list spending/creating |
| Balances | Separate ORU and RES — never summed |

---

## 6. Page: ORU balances (`/oru` or dashboard widget)

| Element | Source |
|---------|--------|
| Chain cumulative convergence | `getblockchaininfo.cumulative_convergence` |
| Recent subsidy | Last N coinbase vout[0] |
| Note | “ORU committed to convergence — see convergence lineage” |

Not a wallet — no spend actions.

---

## 7. Page: RES balances (`/res`)

| Element | Source |
|---------|--------|
| Total minted | Sum `listconvergencehistory` / chain stats |
| Max supply | Static `MAX_RES_SUPPLY` |
| Recent mints | Blocks with `rigor_res_minted` |
| Decimals | 2 dp display |

---

## 8. Page: Convergence lineage (`/convergence` or lineage graph on tx/block)

| Element | Content |
|---------|---------|
| Graph/table | convergence_txid → block → winner? → mint_txid |
| Filters | By height range |
| Candidate view | `draw_eligible` from indexed convergence |

---

## 9. Page: Convergence activity (`/convergence`)

| Element | Source |
|---------|--------|
| Per-block candidates | Table from `convergence_candidates` |
| Constants | Show `res_probability_target` hex |
| Ineligible reason | If not eligible: show \|F\|, amount, type (no invented math) |

---

## 10. Page: Chain status (`/status`)

| Element | Source |
|---------|--------|
| Tip height/hash | `getblockchaininfo` |
| Sync status | `initialblockdownload`, verificationprogress |
| Peers | `getnetworkinfo` (optional) |
| Indexer lag | `node_height - indexer_height` |
| Last P1 check | Timestamp + pass/fail |
| Cluster tip agreement | Ops log compare (manual field OK closed phase) |

---

## 11. Non-pages (explicitly out of scope)

- Exchange charts, price tickers  
- Account login / user balances  
- Token/NFT galleries  
- Mempool fee markets (optional later)

---

## 12. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **LOW** |
| Explorer classification | **UI-MIN-SPEC** |
| Consensus impact | **NONE** |
| Replay impact | **INDIRECT** — wrong labels mislead |
| Determinism impact | **NONE** |
| Indexing risk | **LOW** |
| Minimal safe recommendation | Ship 8 page types; txid URLs; separate ORU/RES |
