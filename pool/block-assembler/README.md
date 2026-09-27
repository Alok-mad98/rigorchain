# block-assembler

Second production component of the RIGOR Pool Stack.

Consumes frozen `TemplateRecord` rows from [`rigor-gbt`](../rigor-gbt), assembles coinbase material (BIP34 height ‖ ConvergenceRoot, witness, CVGE, RIGOR commitment), reserves an immutable RES body-mint placeholder, and publishes `JobSnapshot` rows with monotonic `job_id` / `candidate_block_id` pairs.

## Specifications

| Task | Document |
|------|----------|
| | [`docs/mining/TEMPLATE_CACHE_AND_ASSEMBLER_SPEC.md`](../../docs/mining/TEMPLATE_CACHE_AND_ASSEMBLER_SPEC.md) |
| Dependency | [`pool/rigor-gbt`](../rigor-gbt) |

## Scope

| In scope | Out of scope |
|----------|--------------|
| Coinbase assembly (height‖C, CVGE, RIGOR) | Stratum server |
| `job_id` + `candidate_block_id` assignment | Winner Discovery Engine |
| RES body-mint **placeholder** vtx slot | RES Mint Engine (real mint) |
| Immutable `JobSnapshot` cache | Database / Redis |
| Read-only job API | Payout / monitoring |

## Build & test

```bash
cd pool/block-assembler
cargo test
```

## Usage

```rust
use block_assembler::{JobPublisher, JobSnapshotApi};
use rigor_gbt::{GbtAdapter, RigordRpc, TemplateCache};
use std::sync::Arc;

let templates = Arc::new(TemplateCache::new());
let jobs = Arc::new(block_assembler::JobCache::new());
let publisher = JobPublisher::with_defaults(jobs.clone());

// After rigor-gbt ingests an ACTIVE TemplateRecord:
let template = templates.get_active_template()?.unwrap();
let job = publisher.publish_from_template(&template)?;

// Read API
let api = JobSnapshotApi::new(jobs);
let active = api.get_active_jobs()?;
```

## Assembly pipeline

1. Decode GBT `transactions[]` → `vtx[1..k]`
2. Reserve `vtx[k+1]` body-mint placeholder (no real mint yet)
3. Build coinbase `scriptSig` = BIP34 height ‖ ConvergenceRoot (32 bytes)
4. Append witness commitment (from GBT `default_witness_commitment` when present)
5. Append CVGE `OP_RETURN` (from `default_convergence_commitment`)
6. Append RIGOR draw-target `OP_RETURN` (`RIGR` + `prev_nbits` LE32)
7. Merkle branches + `assembly_hash`
8. Split `coinb1_hex` / `coinb2_hex` at extranonce2 gap

## Read API

| Method | Description |
|--------|-------------|
| `get_active_jobs()` | All `ACTIVE` snapshots |
| `get_job_by_id(job_id)` | Lookup by `job_id` |
| `get_job_history()` | Chronological publish order |

## Governance

| Field | Value |
|-------|-------|
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** |
