# share-validator Architecture

**Task:** TASK-MINING-008  
**Mode:** INFRASTRUCTURE (no consensus / RPC changes)

## Position

```mermaid
flowchart LR
    M["Miner"] -->|"ShareSubmission"| SV["share-validator"]
  BA["block-assembler"] -->|"JobSnapshot read"| SV
  GBT["rigor-gbt"] -->|"TemplateRecord read"| SV
    SV -->|"valid block"| RD["rigord submitblock"]
```

## Validation pipeline

```mermaid
flowchart TD
    A["validate_share / validate_block"] --> B["JobSnapshot exists"]
    B --> C["TemplateRecord FK match"]
    C --> D["Replay + stale job gate"]
    D --> E["RES mint placeholder"]
    E --> F["convergence_root recompute"]
    F --> G["CVGE + RIGOR scripts"]
    G --> H["Rebuild header"]
    H --> I["OpuHash vs target"]
```

## OpuHash

Port of `pow_opus.cpp` / `opus_pow.py`:

| Parameter | Value |
|-----------|-------|
| Buffer | 64 KiB ChaCha20 keystream |
| Rounds | 256 |
| Seed | 32-byte `SHA256d(header)` LE |
| Final | `Hash256(base ‖ mix)` |

Golden vectors: `pow_tests.cpp::OpuHash_v3plus_reference_vectors`.

## Replay guard

In-memory `seen_share_id` set with `import_seen_shares` / `export_seen_shares` hooks for future DB persistence.

## Impact

| Category | Impact |
|----------|--------|
| Consensus | NONE |
| Protocol | NONE |
| Freeze | NONE |
