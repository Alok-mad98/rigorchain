# rigor-gbt Architecture

**Component:** `pool/rigor-gbt`  
**Task:**
**Mode:** INFRASTRUCTURE 

## Position in pool stack

```mermaid
flowchart TB
    subgraph rigor_gbt_crate["rigor-gbt (this crate)"]
        RPC["RigordRpc"]
        VAL["validate V-01..V-10"]
        NORM["normalize → GbtRecord"]
        CACHE["TemplateCache"]
        API["TemplateCacheApi"]
    end

    RD["rigord"] --> RPC
    RPC --> VAL
    VAL --> NORM
    NORM --> CACHE
    API --> CACHE

    CACHE -.->|"future"| ASM["Block Assembler"]
    CACHE -.->|"future"| STRAT["Stratum"]
```

Solid lines are implemented in . Dashed lines are downstream components (out of scope).

## Data flow

```mermaid
sequenceDiagram
    participant N as rigord
    participant A as GbtAdapter
    participant V as validate
    participant C as TemplateCache
    participant R as TemplateCacheApi

    A->>N: getblockchaininfo
    alt IBD true
        A-->>A: SkippedIbd
    end
    A->>N: getbestblockhash
    A->>N: getblocktemplate(longpollid?)
    N-->>A: template JSON
    A->>V: V-01..V-10
    alt validation fail
        A-->>A: Rejected
    else ok
        A->>C: ingest → TemplateRecord(ACTIVE)
    end
    R->>C: get_active_template / by_id / history
```

## Immutability model

| Principle | Implementation |
|-----------|----------------|
| No in-place mutation | `TemplateRecord` fields are never updated on an existing `Arc` |
| State change = replacement | `transition_record` clones the record, sets new state/timestamp, inserts new `Arc` |
| GBT payload frozen | `GbtRecord.raw_json` and `gbt_payload_hash` set at ingest |
| Dedup | Same `longpollid` + same payload hash → no new record |

## Identifier ownership

| ID | Owner in this crate |
|----|---------------------|
| `record_id` | Assigned at `GbtRecord` normalization (UUID v4) |
| `template_version` | Cache: monotonic per `tip_hash` |
| `job_id` | Reserved on `TemplateRecord`; assigned when assembler publishes (future) |

## Reorg handling

On `tip_hash` change (`handle_tip_changed` or ingest with new tip):

1. All `ACTIVE` and `STALE` records → `INVALIDATED`
2. `active_id` cleared
3. Next successful ingest creates `template_version = 1` on new tip

## Auxiliary RPC usage

Per RIGOR_GBT_ADAPTER_SPEC audit item P-02:

| RPC | Purpose |
|-----|---------|
| `getblockchaininfo` | IBD gate — stop fetch when `initialblockdownload` is true |
| `getbestblockhash` | `tip_hash` at ingest |
| `getblocktemplate` | Primary template source |

## Impact declaration

| Category | Impact |
|----------|--------|
| Consensus | NONE |
| Protocol | NONE |
| RPC wire | NONE (client only) |
| Mining rules | NONE |
| Freeze | NONE |
| Equivalence claimed | false |
