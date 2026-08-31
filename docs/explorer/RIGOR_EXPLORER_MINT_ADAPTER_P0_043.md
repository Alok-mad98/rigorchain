# RIGOR explorer mint adapter (P0-043)

**Task:** `TASK-RIGOR-EXPLORER-MINT-ADAPTER-P0-043`  
**Frozen parameters:** not modified (W=32, T, OpuHash, ASERT, Genesis ORU=0, maturity=100).

## Live path (from node)

Issuance is the coinbase `RIGOR_MARKER_RES_MINT` / `OP_RES_MINT` output (`CreateNewBlock` / `ConnectEvaluateCoinbaseResMint`). `rigor_res_minted` is `CBlockIndex::has_res`. The paid ticket is the **parent** draw winner. Body PCORB witness is not a mint proof.

## Adapter

Indexer detects mint from coinbase script (`findCoinbaseOpResMint`). Display and lineage go winner → sigma → coinbase mint. PCORB stacks are not classified as issuance.

## Verdict

**EXPLORER_MINT_ADAPTER_CLOSED.** Indexer mint rows come from coinbase `OP_RES_MINT`. Body PCORB is not issuance. Lineage is winner → sigma → coinbase mint.

- FORMAL = N/A (read-only observer)
- CPP_REFINEMENT = NOT_PROVED
- PRODUCTION_PARAMS = UNCHANGED
