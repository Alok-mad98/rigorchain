# RIGOR mining PoW pipeline closure (P0-042)

**Task:** `TASK-RIGOR-MINING-POW-PIPELINE-CLOSURE-P0-042`  
**Frozen parameters:** not modified (W=32, T, OpuHash 64KiB/256, ASERT τ, Genesis ORU=0, maturity=100).

## Production path (from code)

```
CreateNewBlock → UpdateTime + GetNextWorkRequired(ASERT)
    → GBT bits/target/curtime/mintime/maxtime/mutable=["time"]
    → rigor-miner gbt_solo copy-bytes (P041)
    → 80-byte header (version, prev, merkle, nTime, nBits, nonce)
    → SHA256d → OpuHash mix → hash ≤ compact(target)
    → submitblock → ProcessNewBlock → ContextualCheckBlockHeader
         (expected nBits, CheckProofOfWork, MTP, max-future)
```

ASERT (`core/src/pow.cpp::GetNextWorkASERT`) scores **parent** time/height against the height-1 anchor. Child nTime does not enter the exponent. Genesis→height-1 delay is not an ASERT exponent input.

Miner grind fields: **nonce** only on the GBT SOLO path. `curtime` is copied and must lie in `[mintime, maxtime]`. nBits/prevhash/coinbase material are not miner-owned.

## Verdict

**MINING_POW_PIPELINE_CLOSED.** Machine-repeatable: GBT copy-bytes → minerd nonce grind → `submitblock` → `ProcessNewBlock`. GBT `nBits` matches accepted headers for 32 consecutive heights; reorg GBT follows the new parent; illegal `nTime` and high-hash are rejected.

- FORMAL = PARTIAL (no new Coq; no `Admitted.` proofs added)
- CPP_REFINEMENT = NOT_PROVED (Coq does not refine C++ ASERT/OpuHash)
- PRODUCTION_PARAMS = UNCHANGED
