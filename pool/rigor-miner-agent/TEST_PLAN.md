# rigor-miner-agent Test Plan

## Test matrix

| ID | Category | File | Case | Pass criteria |
|----|----------|------|------|---------------|
| T-01 | Notify parse | `tests/messages.rs` | 9-param `mining.notify` | Fields + merkle branch |
| T-02 | Rigor parse | `tests/messages.rs` | `mining.notify.rigor` object | Convergence metadata |
| T-03 | Event line | `tests/messages.rs` | Push line → `StratumEvent` | job_id extracted |
| T-04 | Handshake | `tests/integration.rs` | TCP subscribe + authorize | extranonce assigned |
| T-05 | Share submit | `tests/integration.rs` | Published job + TCP submit | `shares_submitted >= 1`, accept or reject |
| T-06 | Agent gate | `tests/integration.rs` | subscribe without opushash | Client error |
| T-07 | Job cache | `src/job/manager.rs` | lifecycle + clean_jobs | stale + stats |
| T-08 | Protocol | `src/stratum/protocol.rs` | agent gate unit | opushash required |
| T-09 | OpuHash | `tests/opuhash.rs` | golden vector + header hash | matches `share-validator` |
| T-10 | Scanner | `tests/mining_scanner.rs` | batch + partition + max target | hashes recorded, share found |
| T-11 | Mining loop | `tests/integration.rs` | notify + 2 threads | `hashes >= 200` after notify |

## Out of scope

| ID | Case | Reason |
|----|------|--------|
| O-01 | Convergence root replay | Pool/miner-agent CI in M-022-C |
| O-02 | 24h soak | Operator-run (spec S-01) |
| O-03 | Reconnect backoff | Future agent hardening |

## Commands

```bash
cd pool/rigor-miner-agent
cargo test
cargo test --test integration
cargo test --test messages
```

## Required output table 

| Field | Status |
|-------|--------|
| CPU Mining Loop | **COMPLETE** |
| Multi-thread Mining | **COMPLETE** |
| Share Submission | **COMPLETE** |
| Metrics | **COMPLETE** |
| Logging | **COMPLETE** |
| Integration Tests | **COMPLETE** |
| Documentation | **COMPLETE** |

## Governance

| Field | Value |
|-------|-------|
| Architecture impact | **NONE** |
| Consensus impact | **NONE** |
| Protocol impact | **NONE** |
| Freeze impact | **NONE** |
| `equivalence_claimed` | **false** |
