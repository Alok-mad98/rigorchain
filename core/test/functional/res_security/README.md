# Pre-mainnet RES security functional suites (solo; no pool)

Adversarial tests for pre-mainnet claims that formal verification of the
*model* does not cover. This directory does **not** add consensus rules.

Lottery window / F-set / `draw_winner_txid` suites were removed in Batch B.

**Operator checklist:** `docs/security/PRE-MAINNET-CHECKLIST.md`

## Remaining runner

```bash
./test/functional/res_security/run_pre_mainnet.sh
```

1. `feature_res_solo_security.py --descriptors` — mempool / GBT / CVGE cumulative
2. `feature_res_maturity.py --descriptors` — immature ORU coinbase

Body mint: `feature_res_destroy_cost_mint.py` (default runner).

Helpers: `harness.py` (mint / supply snapshot), `cvge_helpers.py` (CVGE tamper).
