# RIGOR website / docs SOLO-first (P0-044)

**Task:** `TASK-RIGOR-WEBSITE-SOLO-DOCS-P0-044`  
**Frozen parameters:** not modified.

## Copy (from shipped path)

Test mining is `rigor-miner` → local `rigord` GBT → `submitblock`. Pool is quarantined. GBT has `default_res_mint`, not `default_pcorb_carrier`.

## Changes

- `/mining` is SOLO-first; `/mining/pool` remains a demoted observer route.
- `docs/architecture/ARCHITECTURE.md` drops the PCORB carrier GBT field.

## Verdict

**WEBSITE_SOLO_DOCS_CLOSED.** `/mining` is SOLO-first. `/mining/pool` is a demoted observer. Architecture docs no longer list `default_pcorb_carrier` as a live GBT field.

- FORMAL = N/A (copy / observer site)
- CPP_REFINEMENT = NOT_PROVED
- PRODUCTION_PARAMS = UNCHANGED
