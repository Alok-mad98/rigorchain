# Wallet Convergence confirmation margin

| Field | Value |
|-------|-------|
| **Consensus impact** | **NONE** |
| **Audience** | Wallet implementers and operators |
| **Code** | [`core/src/wallet/mint_height_pad.h`](../../core/src/wallet/mint_height_pad.h), `CreateMintTransaction` in [`core/src/wallet/spend.cpp`](../../core/src/wallet/spend.cpp) |

This note describes how this repository's wallet constructs an ORU-destroy, RES-mint transaction. The constant 144 is this wallet's construction choice. Consensus parameters do not include it. Another wallet may choose another margin. A node accepts or rejects the transaction at the height where it is actually confirmed.

The economic claim that a flat margin is a rounding error lives in [RFC-020 §5](../protocol/RFC-020.md), [whitepaper chapter 2](../whitepaper/chapters/02-two-assets.md), and [whitepaper chapter 5](../whitepaper/chapters/05-mathematical-fairness.md). Those passages do not name 144.

## What consensus checks

A Convergence transaction destroys `B` ORU atoms and mints `R` RES atoms. The node checks one inequality at the confirming height `h`:

```text
R · (2²⁵ + h)² ≤ B << 40
```

`h` is the height of the block that includes the transaction. `destroy_cost(h)` is the display form of this relation, and it is strictly increasing. For a fixed `B`, each later height admits a smaller maximum `R`. The `R` in the transaction is fixed when the wallet signs it. A later confirmation does not rewrite `R` downward.

A mint written to the exact maximum at the next block therefore fails as soon as confirmation slips by one block. The reject code is `bad-txns-res-mint-underpaid`. A shortfall of a few RES atoms invalidates the whole transaction. After every new block the mempool rechecks Convergence mints at the next height and drops any mint that would be illegal there. The wallet then marks that mint abandoned, and the ORU inputs become spendable again. The user-visible result is a submitted transaction that the network does not accept.

## The 144 blocks are added to the height

Construction uses these heights. The numbers in the right-hand column are the worked example for a tip of 1000. The arithmetic depends only on height.

| Name | Meaning | Tip = 1000 |
|---|---|---|
| `current_height` | Tip the wallet sees | 1000 |
| `confirm_height` | Next block, the earliest height that can confirm the mint | 1001 |
| `height_pad` | `CONVERGENCE_HEIGHT_PAD` | 144 |
| `target_height` | `current_height + 144` | 1144 |
| `R` written into the transaction | Computed from `destroy_cost` at height 1144 | see the examples |

The pad is added to the current tip. It is not added to the numeric value of `destroy_cost`, and it is not added to `confirm_height`. `confirm_height + 144` would be 1145, which is not this wallet's construction height. Passing `height_pad = 0` prices the mint at `confirm_height` and is reserved for tests; one empty block then evicts that mint.

After pricing at 1144, every confirming height from 1001 through 1144 is legal. That is 144 heights. Height 1145 is the first illegal height. The transaction may be included in the next block. The pad is not a delay lock. On mainnet, with a 10-minute target spacing, the window is about 24 hours. On the public testnet, with a 1-minute target spacing, it is about 2.4 hours.

If 144 blocks are mined without the transaction, the tip reaches 1144. The mempool then checks height 1145, drops the mint, and the wallet abandons it. A new mint has to be built from the new tip. The already-signed transaction cannot be broadcast successfully.

The minimum burn and the RES preview in the Qt Convergence page and wizard both use height `tip + 144`. The page shows that minimum as “valid for 144 blocks”. `createconvergence` returns `height_pad`, `target_height`, `destroy_cost_height`, `res_out`, `exact_res_out`, `slack_res`, and `valid_until_height`. `slack_res` is the RES the same `B` would mint if priced at the next block, minus the RES actually written into the transaction. The unminted atoms stay in the destroyed ORU. They are not returned, and they are not the network fee.

The per-block rise used below matches the margin table in RFC-020 §5. Near genesis the displayed cost rises by about 0.000 005 96% per block (`destroy_cost(1) − destroy_cost(0) = 6 103` atoms). One hundred blocks on that curve is about 0.000 596%.

## Example 1: an exact fit dies one block later

The tip is 1000. A construction prices `R` at the next block, 1001, and writes exactly 1.000 000 00 RES (100 000 000 atoms).

The burn is `B = 102 406 109 711` atoms, which is 1 024.061 097 11 ORU. The displayed `destroy_cost(1001)` is 1 024.061 097 10 ORU, one atom below that exact burn. The display value is a floor. The mint has to use the exact burn. Feeding the displayed cost back in as `B` does not buy one whole RES.

| Confirming height | Maximum RES atoms this `B` can mint | The written 100 000 000 atoms |
|---:|---:|---|
| 1001 | 100 000 000 | legal |
| 1002 | 99 999 994 | short by 6 atoms; the whole transaction is illegal |

Six atoms are 0.000 000 06 RES. The amount is negligible. The consensus result is that the whole transaction is rejected. The margin exists because the inequality fails, even when the missing ORU is only a few atoms.

## Example 2: this wallet's default construction

The tip is still 1000. The user wants the transaction to contain exactly 1.000 000 00 RES, and to remain legal if it confirms at height 1144. The wallet computes the burn at height 1144:

`B = 102 406 982 541` atoms = 1 024.069 825 41 ORU.

Compared with Example 1, this destroys 872 830 extra atoms, which is 0.008 728 30 ORU. Relative to the whole-RES cost at the next block, that is about 0.000 852%. The same curve produces the RFC-020 table (100 blocks is about 0.000 596%).

The resulting fields are:

| Field | Value |
|---|---|
| `burn_oru` | 102 406 982 541 atoms |
| `res_out` | 100 000 000 atoms (1.000 000 00 RES) |
| `exact_res_out` | 100 000 852 atoms (the exact fit at height 1001) |
| `slack_res` | 852 atoms (0.000 008 52 RES) |
| `target_height` | 1144 |
| `valid_until_height` | 1144 |

Three outcomes:

- The next block, 1001, confirms the transaction. The mint is legal, and the RES created is the written 1.000 000 00 RES. The same burn could have minted 1.000 008 52 RES at height 1001. The 852 unminted atoms are the price of the window. The node does not increase `R`.
- Height 1100 confirms the transaction. It is legal, and the RES created is still the written 1.000 000 00 RES.
- No block from 1001 through 1144 includes the transaction. Once the tip is 1144, the next height is 1145. At 1145 this `R` is six atoms over the maximum (99 999 994). The mempool drops the transaction, the wallet abandons it, and the ORU inputs are spendable again. The signed transaction cannot be rebroadcast successfully.

## Example 3: burning the next block's whole-RES cost

The user destroys the Example 1 amount, 1 024.061 097 11 ORU, through this wallet's default construction. The wallet still computes `R` at height 1144. It does not write 1.000 000 00 RES into the transaction.

| Field | Value |
|---|---|
| `res_out` | 99 999 147 atoms (0.999 991 47 RES) |
| `exact_res_out` | 100 000 000 atoms |
| `slack_res` | 853 atoms |

The transaction is legal at every height from 1001 through 1144. A transaction that itself contains exactly 1 RES uses the Example 2 burn: the exact whole-RES burn at height `tip + 144`. The Qt text “valid for 144 blocks” names this window.

## Boundary with the protocol

Consensus re-evaluates the inequality at the actual confirming height. An early confirmation mints the `R` that was written, and the unminted `slack_res` remains destroyed with `B`. After the window, that same written `R` fails the inequality. 144 is the number of blocks this wallet leaves so that an already-signed mint can wait out about a day in the mempool. A different wallet may leave a shorter or longer margin. Node validation is unchanged.
