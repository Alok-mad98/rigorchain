# Convergence Inspector — TASK-P2-005

**Workspace:** `rigor-qt-prototype/src/convergenceinspector.{h,cpp}`  
**Mode:** Explorer-only read-only presentation. No `core/`, consensus, wallet, or RPC changes.

## Data sources (explorer authoritative)

| Step | Endpoint | Purpose |
|------|----------|---------|
| 1 | `GET /api/status` | Tip / health |
| 2 | `GET /api/res/mints?limit=1` | Latest winner, mint txid, block |
| 3 | `GET /api/block/{hash}` | `convergence_panel` (|F|, root, winner) |
| 4 | `GET /api/lineage/{winner_txid}` | Lineage graph fields |
| 5 | `GET /api/tx/{mint_or_winner_txid}` | `converge_proof_panel` |

No local winner calculation, convergence replay, or proof verification.

## UI sections

### Summary

- Latest winner TXID  
- Latest convergence root  
- Latest candidate set size |F|  
- Latest RES mint TXID  
- Latest convergence height  

### Lineage tree (read-only)

Vertical chain from explorer `lineage_graph`:

Candidate Set → Winner → Convergence Root → Sigma → PCORB → RES Mint

### ConvergeProof

- Sigma digest (copy)  
- PCORB field table, 6 rows (copy per row)  
- Mint output, amount, confirmation status  

**SECURITY-001-A / A-7.** The chain has no Ed25519 stage: the PCORB proof's signature and public key
were removed from the witness, so there is no signature to render or copy. The inspector renders
whatever proof rows the explorer returns; against an updated indexer that is 6 items.

### Navigation

Opens system browser:

- `/tx/{winner_txid}`  
- `/block/{block_hash}`  
- `/lineage/{winner_txid}`  

## Error handling

Gray placeholder:

```text
Explorer unavailable
Read-only data source missing
```

No mock fallback. Inspector refreshes independently of rigord RPC success (30s timer + **Refresh**).

## Compliance

- Footer: `Read-only. Explorer authoritative. equivalence_claimed: false. No local verification.`  
- No wallet send, mint, broadcast, or mining controls  

## Settings

**Explorer base URL** in **File → RPC Settings** (default `http://127.0.0.1:8080`).

## Required verdict

| Field | Status |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |
