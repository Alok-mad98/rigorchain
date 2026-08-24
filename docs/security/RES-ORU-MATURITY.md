# RES and ORU UTXO maturity (consensus)

**Date:** 2026-08-18  
**Consensus impact: yes**  
**Inventory:** [RES-ORU-MATURITY-INVENTORY.md](./RES-ORU-MATURITY-INVENTORY.md)  
**Pre-mainnet gates:** [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md)

## What RES is / is not

- **RES** is an independent scarce asset. It is **not mined**, **not** a 1:1 ORU exchange, and **not** created in coinbase.
- The only issuance path is a **non-coinbase body mint**: when the parent block’s convergence draw has a winner, ConnectBlock requires exactly one Witness v1 UTXO of **`W(h)` RES** (`GetResMintReward`; 40.000000 RES at genesis). No winner ⇒ no mint. Hard cap **21,000,000 RES**. Recipient is the winner-declared RESO program (`bad-rigor-res-owner-mismatch` if redirected).
- Burned ORU is draw **weight** only; it does not become RES.
- `ComputeResReward` returns 0; coinbase-derived RES helper APIs abort. Production coinbase RES is `bad-cb-res-amount` (except mockable `-reswalletbootstrap`).
- Issuance is **not** calendar-guaranteed. This document is only about **spendability of UTXOs that already exist**.

## When a UTXO becomes spendable

Single runtime constant: `Consensus::Params::nCoinbaseMaturity` (compile-time SSOT `COINBASE_MATURITY = RES_MINT_MATURITY = 100` in `consensus/consensus.h`; slowtest uses 1).

**Predicate (mempool `AcceptToMemoryPool` and `ConnectBlock` — identical, via `CheckTxInputs`):**

```
nSpendHeight - coin.nHeight < nCoinbaseMaturity  ⇒  reject
```

Mempool uses `nSpendHeight = tip + 1`. A coin created at height `H` is first spendable in a tx whose spend height is `H + nCoinbaseMaturity`.

| UTXO | Marker | Reject |
|------|--------|--------|
| ORU coinbase | `Coin::fCoinBase` | `bad-txns-premature-spend-of-coinbase` (`TX_PREMATURE_SPEND`) |
| Body RES mint (Witness v1) | `Coin::nFlags & COIN_FLAG_RES_MINT` | `bad-txns-premature-spend-of-res-mint` (`TX_PREMATURE_SPEND`) |
| Ordinary RES transfer / change | neither | not delayed by this rule; still `res_in ≥ res_out` |

Wallet `getbalances().res.immature` / `res.trusted` and `IsTxImmatureResMint` are **non-consensus**. A raw tx that bypasses the wallet is still rejected by the node while immature.

Reorgs: mint flag is restored from block undo (dummy version byte, bit 0). Maturity follows the surviving chain’s mint height. A disconnected mint UTXO is gone; a spend of it is missing-inputs. Mempool entries that spend mint/coinbase are re-checked after reorg (`GetSpendsCoinbase` covers both).

Maturity does **not** create a new issuance path.

## How to run tests

From `core/` (descriptors wallet):

```bash
# Unit (CheckTxInputs + undo flag)
./src/test/test_bitcoin --run_test=res_orum_tests/res_mint_coin_immature_until_coinbase_maturity
./src/test/test_bitcoin --run_test=res_orum_tests/oru_coinbase_still_immature
./src/test/test_bitcoin --run_test=coins_tests/txundo_formatter_res_mint_flag_roundtrip

# Functional T-RES-* / T-ORU-1 / T-ISSUE-1
python3 test/functional/feature_res_maturity.py --descriptors
```

Default runner includes `feature_res_maturity.py`. Pre-mainnet bundle (window + solo + maturity): `test/functional/res_security/run_pre_mainnet.sh`. Operator gates: [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md).

## Residual risks

- Wallet confirmations use `(nCoinbaseMaturity + 1) - depth`; consensus uses the table above. A spend can enter the mempool one confirmation earlier than Overview “mature” — same as Bitcoin coinbase. Do not loosen consensus to match UI.
- Mockable `-reswalletbootstrap` coinbase RES is still a **coinbase** UTXO (`fCoinBase`), not a body mint. It is gated by coinbase maturity, not `COIN_FLAG_RES_MINT`.
- This rule does not guarantee a non-zero RES issuance rate.

## Theme Go / No-Go

**Go (2026-08-18).** `feature_res_maturity.py` (T-RES-1…6, T-ORU-1, T-ISSUE-1) and the unit cases above PASS. An immature body RES mint cannot be spent without `CheckTxInputs` accepting it, on both mempool and block paths. Wallet UI is not the enforcement path.

**Rule (one paragraph):** A Witness v1 UTXO created by a consensus-valid non-coinbase RES mint is consensus-invalid as an input until `nSpendHeight - coin.nHeight >= nCoinbaseMaturity` (same constant and depth as ORU coinbase; mempool spend height is tip+1). Reject reason is `bad-txns-premature-spend-of-res-mint` (`TX_PREMATURE_SPEND`). Ordinary RES transfers are not delayed by this flag and still cannot mint (`res_in ≥ res_out`). This is spendability of created UTXOs, not a calendar issuance guarantee.
