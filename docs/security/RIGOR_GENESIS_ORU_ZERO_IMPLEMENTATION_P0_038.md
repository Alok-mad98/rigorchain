# RIGOR Genesis ORU Zero Implementation (P0-038)

**Task:** `TASK-RIGOR-GENESIS-ORU-ZERO-IMPLEMENTATION-P0-038`  
**Date:** 2026-08-29  
**Prior audit:** [P0-037](./RIGOR_GENESIS_ORU_ZERO_ISSUANCE_AUDIT_P0_037.md)

```
GENESIS_ORU_REWARD                  = 0
GENESIS_REBUILT                     = true
GENESIS_HASH_FROZEN                 = true
GENESIS_1000_COMPENSATION           = false

W32                                 = unchanged (32)
T                                   = unchanged
RES_REWARD                          = unchanged
MATURITY                            = unchanged (100)
OPUSHASH                            = unchanged
ORU_INITIAL_BLOCK_SUBSIDY (h>=1)    = unchanged (1000 ORU)

CONSENSUS_TESTS                     = PASS
CPP_REFINEMENT                      = NOT_PROVED
```

Mainnet has not launched. Pre-P0-038 genesis hashes are **foreign**. Wipe `blocks/` and `chainstate/`. No compatibility branch.

---

## What changed

Two production sites issued the 1000. Both now pay 0 at height 0:

| Site | Change |
|------|--------|
| `CreateGenesisBlock(..., genesisReward)` | argument is `ORU_GENESIS_BLOCK_SUBSIDY` (`0`) on **every** shipped network |
| `GetBlockSubsidy` | `nHeight == 0` returns `ORU_GENESIS_BLOCK_SUBSIDY` before halving math |

`ORU_INITIAL_BLOCK_SUBSIDY` remains `1000 * COIN` for `h >= 1`. Height 1 is **not** 2000.

New lock constant: `Consensus::ORU_GENESIS_BLOCK_SUBSIDY = 0` in `rigor_consensus_lock.h`.

---

## Frozen genesis vectors (P0-038)

| Network | genesis hash | merkle root (coinbase txid) |
|---------|--------------|-----------------------------|
| main | `82c0cbee504576dc15a0a926382c0aa41f6a368817143cda97183bfedd9c762e` | `9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd` |
| testnet | `0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce` | `54ace6b8a887ef617d5d583c7fb326a88805b7f7afba190de470898ea2885343` |
| regtest | `3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63` | `d3da525deee10cd11754b3269e919f8388184ec1f01a1222ad5419d00f0b43e3` |
| devtest | `9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0` | `ada94f709a0bb27dfb0d1862eb4c0962c8a7c000c1c6e21dc5a7841f869c0a93` |
| slowtest | `18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8` | `46ee1198cb3aeee9160bf0f88b1c99d167024fedbbf9e926d81a9f2c49d32c85` |

Signet chainparams remain removed; no independent signet genesis in this tree.

Unchanged genesis fields: timestamp, nBits, nonce, version, scriptPubKey, coinbase message.

---

## Surfaces checked

- **RPC:** `getrawtransaction` still refuses the genesis coinbase. Coinstats `unspendables.genesis_block` is now 0 because `GetBlockSubsidy(0)=0`.
- **Wallet:** no genesis UTXO; first mined coinbase is still 1000 ORU (immature 100 blocks).
- **Chain init / bootstrap:** `LoadGenesisBlock` writes the new hardcoded block; `ConnectBlock` still skips genesis txs. Checkpoints `{0, hash}` follow `hashGenesisBlock`.
- **Validation:** `bad-cb-amount` still does not run on genesis (skip). Height ≥ 1 uses the frozen subsidy. `ComputeDerivedOruSubsidyCumulative` samples era 0 at height 1 so the closed form still matches `GetBlockSubsidy` (not a +1000 compensation).
- **testnet / regtest / devtest / slowtest:** each has its own rebuilt genesis (independent timestamps/scriptPubKeys). All reward 0.

---

## Operator note

Existing local chains built on `1c2d6436…` / `cc31d596…` / etc. will not start. Delete the datadir and reinitialize. Do not add a dual-genesis decoder.

---

## Verification (2026-08-29)

| Command | Result |
|---------|--------|
| `make -j$(nproc)` | PASS |
| `make check` | PASS (univalue, `test_rigor-qt`, per-file Boost suites including genesis/subsidy/supply) |
| `./src/test/test_rigor` | PASS (1004 cases) |

Regression locks exercised:

- `GetBlockSubsidy(0) == 0`
- Genesis coinbase value `== 0`
- Height 1 reward `== ORU_INITIAL_BLOCK_SUBSIDY` (1000 ORU), not 2000
- Frozen genesis hash / merkle match P0-038 vectors
- Genesis outpoint absent from CoinsTip (no spendable ORU UTXO)

Closed-form `ComputeDerivedOruSubsidyCumulative` samples era 0 at height 1 so the sum still matches `Σ GetBlockSubsidy(i)`. That is not a height-1 compensation of the removed genesis 1000.
