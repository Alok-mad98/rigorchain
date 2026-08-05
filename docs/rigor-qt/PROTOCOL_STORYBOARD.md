# Protocol Storyboard — TASK-P2-006

**Workspace:** `rigor-qt-prototype/src/protocolstoryboard.{h,cpp}`  
**Tab:** Protocol Storyboard  
**Mode:** Read-only educational visualization. Explorer-indexed data only.

## Purpose

End-to-end explanation of how a convergence event becomes a RES mint on RIGOR, using real explorer API data. No local winner computation, proof verification, or protocol reinterpretation.

## Explorer data chain

| Step | Endpoint |
|------|----------|
| 1 | `GET /api/status` |
| 2 | `GET /api/res/mints?limit=1` |
| 3 | `GET /api/block/{hash}` |
| 4 | `GET /api/lineage/{winner_txid}` |
| 5 | `GET /api/tx/{mint_or_winner_txid}` |

## UI sections

| Section | Content |
|---------|---------|
| 1 — Convergence Event | Height, winner TXID, \|F\|, convergence root |
| 2 — Draw Process | Sigma, candidate set listing, winner result (indexed) |
| 3 — PCORB | 6 canonical fields with descriptions and sizes; live hex when indexed |
| 4 — Mint Proof Authentication | States that the proof is unsigned and that authentication comes from the ORU funding input on `vin[0]` |
| 5 — RES Mint | Mint TXID, output index, RES amount, confirmation |
| 6 — Visual Timeline | Scrollable Candidate Set → … → Confirmed |
| 7 — Explainer mode | RIGOR-only prose for Root, Sigma, PCORB, RES Mint |
| 8 — Compliance | Footer: read-only, explorer authoritative, `equivalence_claimed: false` |

**SECURITY-001-A / A-7.** Section 3 lists **6** fields, not 11: the σ signature, its public key, the
leaf script, and two reserved-zero slots were removed from the witness, because σ is a pure function
of those six fields — each of which consensus checks against the block on its own — and the
verifying key came from the witness itself, so any keypair satisfied it. Section 4 was accordingly
retitled from "Ed25519" and no longer renders a signature; the storyboard timeline drops its
"Ed25519" step. σ itself is unchanged and still drives the draw (section 2).

## Explainer principles

- Describes frozen RIGOR protocol paths only  
- No Bitcoin comparisons or alternative design discussion  
- No local verification claims  

## Error state

```text
Explorer unavailable — storyboard requires indexed explorer data.
Read-only data source missing.
```

No mock fallback.

## Refresh

30 second auto-refresh and main **Refresh** button (explorer fetch runs with inspector/storyboard independently of rigord RPC success).

## Required verdict

| Field | Status |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |
