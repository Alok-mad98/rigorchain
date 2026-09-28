# 30-day evidence collector

Design only for a long-running private testnet. Not public mainnet.
Not a Coq proof. Does not change consensus.

## Cadence

| Item | Value |
|------|-------|
| Window | 30 UTC days |
| Write unit | one directory `evidence/YYYY-MM-DD/` |
| Format | JSONL, append-only |
| Default poll | 60 seconds (`collect.py --interval 60`) |
| Cron | `* * * * * collect.py --once` is enough |
| Prune | `collect.py --prune-days 30` deletes older day dirs |

Do not emit a random million-tx corpus. One row must map to `dataset_theorem_mapping.md`.

## Daily files

| File | Future theorem |
|------|----------------|
| `block_event.jsonl` | `network assumption` (arrival) + identity for A |
| `validation_event.jsonl` | `validation_soundness` |
| `connect_event.jsonl` | `utxo_inverse` |
| `disconnect_event.jsonl` | `utxo_inverse` |
| `reorg_event.jsonl` | `chain_selection` |
| `restart_event.jsonl` | `persistence_correctness` |
| `peer_event.jsonl` | `network assumption` |

`block_event.jsonl` row:

```
{
  "height": 12,
  "hash": "...",
  "previous": "...",
  "time": 1700000000,
  "tx_count": 1,
  "validation": "accept",
  "utxo_before": "...",
  "utxo_after": "..."
}
```

## Join

```
consensus_trace.block_hash  ==  block_event.hash
environment_observation     ==  peer_event at noted_ms
```

Missing environment → `network_context: null`. Consensus row still kept.

## What this collector does not do

- Public seed scraping
- Changing ConnectBlock / EvaluateConnect
- Writing client/wallet fields into `KernelInput`
- Treating peer count as a consensus predicate
