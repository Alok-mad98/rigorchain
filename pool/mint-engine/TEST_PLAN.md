# mint-engine Test Plan

**Task:** TASK-MINT-ENGINE-001, TASK-MINING-018

## Test matrix

| ID | Category | File | Cases | Status |
|----|----------|------|-------|--------|
| M-01 | Body mint tx build | `src/txbuild.rs` | witness stack, deterministic txid | PASS |
| M-02 | Ed25519 witness | `src/witness.rs` | sign/verify roundtrip | PASS |
| M-03 | Bind hash | `tests/integration.rs` | deterministic infra bind | PASS |
| M-04 | Reorg revoke | `tests/integration.rs` | `revoke_for_jobs` state | PASS |
| M-05 | Authorized mint E2E | `src/engine.rs` | WIN-002 golden → token → mint | PASS |
| M-06 | AUTH-08 bind gate | `src/engine.rs` | rejects mint without `bind_hash` on token | PASS |
| M-07 | Mint without winner | `src/engine.rs` | `mint_required == false` → `Ok(None)` | PASS |

## Golden vector (P-03)

`end_to_end_mint_from_authorized_winner` replays **WIN-002** from
`formal_verification/phase3_golden_vectors.json` with assembler-aligned convergence root
for AUTH-03.

## Commands

```bash
cd pool/mint-engine
cargo test
```

## Required output (TASK-MINING-018)

| Field | Status |
|-------|--------|
| Mint Engine Authorization Check | **COMPLETE** |
| RES Mint Authorization Integration | **COMPLETE** |
| Reorg / Invalidation Handling | **COMPLETE** |
| Unit / Integration Tests | **COMPLETE** |
| Equivalence Claimed | **false** |
