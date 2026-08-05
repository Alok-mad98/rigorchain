# mint-engine

Production RES body mint engine for the RIGOR mining pool (TASK-MINT-ENGINE-001).

## Responsibilities

- Consume `MintAuthorizationToken` from Winner Engine (TASK-MINING-018)
- Build RES body mint transaction (`vout[0]` = 100 RES, ORU inputs, PCORB witness, Ed25519)
- Insert mint at `vtx[k+1]` in candidate block assembly
- Optional `submitblock` RPC to `rigord`
- Mint state machine for reorg / orphan / monitoring

## Architecture impact

| Field | Value |
|-------|-------|
| Consensus | NONE |
| Protocol | NONE |
| Freeze | NONE |
| equivalence_claimed | false |

## Authorization API

```rust
// Production path — requires Winner Engine token
engine.mint_for_authorized_winner(&token, &template, &job, &block_ctx)?;

// Internal builder (tests only) — caller supplies MintWinnerContext
engine.mint_for_winner(&preview, &template, &job, &block_ctx)?;
```

## Tests

```bash
cd pool/mint-engine && cargo test
```

See `ARCHITECTURE.md` and `TEST_PLAN.md`.
