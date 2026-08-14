# LMS RES Maturity Model

| Field | Value |
|-------|-------|
| **Task** | `TASK-LMS-HARNESS-MATURITY-ALIGNMENT-001` |
| **Date** | 2026-08-12 |
| **Scope** | LMS harness / documentation only |
| **Consensus** | Unchanged (W / T / RES issuance frozen) |

---

## Protocol / wallet behavior

RES minted by Convergence draw (body mint) is recognized by the wallet immediately, but is **not** immediately spendable as trusted credit.

```text
RES mint included in block
        ↓
wallet detects amount_res = 100.00
        ↓
category = immature
res.immature = 100.00
res.trusted  = 0.00
        ↓
wait nCoinbaseMaturity + 1 confirmations  (= 101)
        ↓
category ≠ immature  (typically "receive")
res.trusted  = 100.00
res.immature = 0.00
        ↓
UTXO spendable (listunspent)
```

Implementation reference:

- `CWallet::IsTxImmatureResMint`
- `CWallet::GetTxBlocksToResMintMaturity` → `nCoinbaseMaturity + 1`

This mirrors coinbase maturity. It is **not** a consensus mint failure when `trusted` is still zero right after payout.

---

## LMS assertion model (aligned)

| Stage | Check ID | Expected |
|-------|----------|----------|
| On-chain mint | `real_res_mint_observed` | Body mint present, `res_atoms == 10000` |
| Case A (immediate) | `case_a_immediate_immature` | `amount_res=100`, `category=immature`, `ismine`, `res.immature≥100` |
| Case B (after maturity) | `case_b_after_maturity_trusted` | `amount_res=100`, not immature, `res.trusted≥100`, spendable UTXO |
| Mesh | `p2p_sync_after_mint_hunt` | Nodes synced |

Harness helpers:

- `res_visible()` = trusted + immature + untrusted
- `mine_res_to_maturity()` mines until 101 confirmations

---

## What LMS must not do

- Require `getbalances()["res"]["trusted"] >= 100` immediately after mint
- Bypass maturity (`-reswalletbootstrap` for this path, fake balances, manual RES creation)
- Change W, T, or RES issuance parameters

---

## Operator run

```bash
cd ~/rigor-official-tree
LMS_MAX_MINT_ROUNDS=20 bash contrib/testing/local_mainnet_simulation/run_full_acceptance.sh
```

Artifacts: `contrib/testing/local_mainnet_simulation/out/phase_res_mint_hunt.json`
