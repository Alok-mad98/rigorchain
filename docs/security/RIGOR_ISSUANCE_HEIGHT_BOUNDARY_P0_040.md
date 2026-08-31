# Issuance height boundary (P0-040)

**Task:** `TASK-RIGOR-ISSUANCE-HEIGHT-BOUNDARY-CLOSURE-P0-040`  
**Production C++:** not modified.

## Frozen semantic (from `GetBlockSubsidy` / `GetResMintReward`)

```
epoch(h) = floor(h / 210000)
ORU(0)   = 0
ORU(h≥1) = 1000 ORU >> min(epoch(h), 3)
RES(h)   = W(epoch(h))   RFC-017, not a binary shift
```

Height 210000 is the first halved height (epoch 1). First full-reward ORU heights are **1..209999** (209999 blocks). This is not `(h-1)/210000`.

ORU and RES share the same `epoch(h)`. Genesis is not an ORU issuance block. Height 1 is not compensated.

## Verdict

**ISSUANCE_HEIGHT_BOUNDARY_CLOSED** for the frozen `h/210000` semantic. Production C++ was not changed.

- Height 210000 is epoch 1 (first halved block), not the last full-reward block.
- ORU and RES share `epoch(h)`.
- `ISSUANCE_BOUNDARY_REFINEMENT` = formula correspondence (C++ + Coq + golden vectors).
- `CPP_REFINEMENT` = NOT_PROVED.
