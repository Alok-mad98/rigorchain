# RIGOR Genesis ORU Zero Issuance Audit (P0-037)

**Task:** `TASK-RIGOR-GENESIS-ORU-ZERO-ISSUANCE-AUDIT-P0-037`  
**Mode:** Audit + proposed formal model + regression tests. **Production consensus C++ was not modified.**  
**Date:** 2026-08-29  
**Mainnet status:** not launched. No historical compatibility burden.

```
CURRENT_GENESIS_ORU_REWARD          = 1000
PROPOSED_GENESIS_ORU_REWARD         = 0

GENESIS_ORU_ISSUANCE                = 1000_LEDGER_AND_SERIALIZED / 0_CONNECTED
GENESIS_ORU_UTXO                    = 0
GENESIS_ORU_ALLOCATION              = 0
GENESIS_ORU_CIRCULATING_SUPPLY      = 0

GENESIS_HASH_IMPACT                 = YES
MERKLE_ROOT_IMPACT                  = YES
COINBASE_TXID_IMPACT                = YES

W32_IMPACT                          = NONE
T_IMPACT                            = NONE
RES_REWARD_IMPACT                   = NONE
MATURITY_IMPACT                     = NONE
OPUSHASH_IMPACT                     = NONE

COQ_IMPACT                          = NEW_MODULE_PROPOSED_RULE
CPP_REFINEMENT                      = NOT_PROVED
GOLDEN_VECTOR_IMPACT                = YES_MUST_REBUILD
CHAIN_REINITIALIZATION_REQUIRED     = YES

VERDICT                             = GENESIS_ZERO_SAFE_BUT_REQUIRES_GENESIS_REBUILD
```

Honesty flags:

```
MODEL_PROVED                        = proposed GenesisReward(0)=0 (Coq GenesisZeroIssuance.v)
CPP_REFINEMENT                      = NOT_PROVED
refinement_proved                   = false
GetBlockSubsidy(0) in C++           = 1000 ORU   (unchanged this TASK)
CreateGenesisBlock reward           = 1000 ORU   (unchanged this TASK)
COQ_ADMITTED                        = 0
COQ_AXIOMS                          = 0
COQ_PARAMETER                       = 0
CONSENSUS_CPP_MODIFIED              = NO
```

---

## P17 Verdict

**`GENESIS_ZERO_SAFE_BUT_REQUIRES_GENESIS_REBUILD`**

Changing height-0 ORU from 1000 to 0 is economically and structurally safe on a pre-launch chain: there is no spendable genesis premine, no treasury allocation, and no protocol need to compensate at height 1. It is **not** a hash-neutral parameter tweak. The 1000 satoshi-scaled value is inside the genesis coinbase transaction; rewriting it changes coinbase txid, merkle root, genesis hash, and every subsequent `hashPrevBlock` chain.

Do **not** land this as “reward only, genesis hash unchanged.”

Do **not** land it as `GENESIS_ZERO_SAFE_TO_LAND` (that would require the hash to stay frozen).

Do **not** treat it as blocked or as a protocol redesign: W, T, RES `W(k)`, maturity, OpuHash, ScriptPubKey layout, and the `h >= 1` ORU schedule are independent of the genesis coinbase `nValue`.

---

## P1 / P2 — Where the 1000 ORU actually is

### Classification

The current 1000 is **`GENESIS_SUBSIDY`**, appearing in two coupled production sites that happen to use the same constant:

| Site | What it is | Spendable? |
|------|------------|------------|
| `CreateGenesisBlock(..., genesisReward = ORU_INITIAL_BLOCK_SUBSIDY)` in `core/src/kernel/chainparams.cpp` | Serialized coinbase `vout[0].nValue` | **No** — `ConnectBlock` skips genesis txs |
| `GetBlockSubsidy(0, …)` in `core/src/validation.cpp` | RFC-002 ledger term `R(0)` | **No UTXO** — but **yes, it is added** to `m_chain_oru_total` / `GetChainOruTotal` |

It is **not**:

- **GENESIS_ALLOCATION** — `CreateGenesisBlock` emits exactly one output; no treasury, owner, or extra mint output.
- **GENESIS_UTXO** — ConnectBlock special-cases the genesis hash and does not `UpdateCoins` the coinbase.
- **TEST_ONLY_CONSTANT** — tests pin the production value; they did not invent it. Wallet `1000 * COIN` fixtures are almost always the **first mined block**, not genesis.

`ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN` is also the **frozen `h >= 1` base**. That constant must **not** be rewritten to 0. The proposed rule is a **height-0 special case**, not a change of the issuance base.

### A. Consensus reward

`GetBlockSubsidy` has no `nHeight == 0` branch. Halvings are `nHeight / interval`, so height 0 and height 1 both pay the full base:

```
GetBlockSubsidy(0) = 1000 ORU
GetBlockSubsidy(1) = 1000 ORU
GetBlockSubsidy(2) = 1000 ORU
```

until the first halving at `nSubsidyHalvingInterval` (210000 public / 150 regtest).

There is no `GetGenesisReward` / `GenesisCoinbaseReward` symbol. Genesis reward is the `genesisReward` argument to `CreateGenesisBlock`, independently passed as `ORU_INITIAL_BLOCK_SUBSIDY`.

### B. Genesis initial UTXO / allocation

**None.**

- One coinbase, one `vout`.
- scriptPubKey is the locked placeholder P2WPKH `00141111111111111111111111111111111111111111`.
- No genesis treasury, no genesis owner allocation, no extra asset output.

Even if ConnectBlock *did* insert the coinbase, the dummy witness program is not a protocol treasury. In production it never enters the coins view.

### C. Test fixtures

Examples that pin production, not a parallel economy:

- `alpha1_genesis_tests.cpp`: `coinbase.vout[0].nValue == 1000 * COIN`
- `validation_tests.cpp` (disabled rigor subsidy case): `GetBlockSubsidy(0) == 1000 * COIN`
- `mine_genesis.py`: `GENESIS_REWARD = 1000 * 100_000_000`
- `feature_coinstatsindex.py`: `unspendables.genesis_block == Decimal('1000')`
- `rpc_blockchain.py`: UTXO `total_amount` sums **heights 1..N only** — “genesis coinbase is not in the UTXO set”

Wallet tests with `1000 * COIN` immature credit are **mined** coinbases after genesis.

### D. Formal model (before this TASK)

| Module | Height 0 meaning |
|--------|------------------|
| `MintSupplyConservation.genesis_supply` | All zeros **before** any block. Not `GetBlockSubsidy(0)`. |
| `SupplyConservation.genesis_supply_zero` | Replay **init** state = 0. Same caveat. |
| `ConnectBlockMint.cb_expected_oru_subsidy` | `Z.shiftr 1000ORU …` at **every** `nat` height, including 0. Lemma `cb_expected_oru_subsidy_pos` proves subsidy **> 0** at all heights. |
| `ReorgIssuanceBound.oru_subsidy_at` | Same formula; applies at `h = 0`. |
| `WindowGeneric.oru_subsidy_model` | Dummy constant `1000`, independent of height — **not** `GetBlockSubsidy`. |
| DualPool `dp_genesis_mint_reward` | **RES** `W(0) = 40 RES`, not ORU. |

### E. Python / RPC / wallet

No “wallet starts with 1000 ORU genesis balance” path. RPC `getrawtransaction` refuses the genesis coinbase (“not considered an ordinary transaction”). Coinstats records the 1000 under **`unspendables.genesis_block`**.

### Answers required by the TASK

**Does the current 1000 ORU enter circulating supply?**  
**No** as spendable UTXO. **Yes** as RFC-002 ledger issuance `R(0)` (`GetBlockSubsidy(0)` is folded into `GetChainOruTotal`). Coinstats names that gap: unspendable genesis subsidy = 1000 ORU, coinbase added to the UTXO set = 0.

**Can any normal transaction spend it?**  
**No.** `ConnectBlock` returns early for `hashGenesisBlock` with the comment “skipping connection of its transactions (its coinbase is unspendable).”

**Is it only a historical/test field?**  
**No.** It is a production genesis serialization field (hash-critical) plus a production subsidy function term (ledger-critical). Tests lock those facts; they are not the source of the 1000.

---

## P3 — Target model

```
GenesisReward(0)  = 0
CumulativeIssuance(0) = 0

h >= 1:
  GenesisReward(h) = current GetBlockSubsidy(h)
                  = ORU_INITIAL_BLOCK_SUBSIDY >> min(h / interval, 3)
```

Coq: `formal_verification/rigor_v01/GenesisZeroIssuance.v`

- `GenesisReward 0 = 0`
- `GenesisReward 1 = 1000 ORU` (sats `100000000000`)
- `GenesisReward 2 = 1000 ORU`
- `no_height_1_compensation`: height 1 is **not** 2000 ORU
- `cumulative_issuance 0 = 0`, `1 = 1000 ORU`, `2 = 2000 ORU`
- Applying the zero genesis `BlockDelta` to P0-036 `genesis_supply` leaves zeros

This module is the **proposed** rule. C++ still pays 1000 at height 0.

---

## P4 — Supply impact (P0-036 unified model)

P0-036:

```
ledger'       = ledger + subsidy − converge
circulating'  = circulating + subsidy − converge − fee_burn
RES'          = RES + valid_mint
```

**Proposed genesis block:**

| Term | Value |
|------|-------|
| subsidy | 0 |
| mint | 0 |
| converge | 0 |
| fee_burn | 0 |
| Supply(0) | 0 |

**Height 1:** existing frozen subsidy (1000 ORU public nets). Not `1000 + 1000` catch-up.

**Height 2+:** unchanged schedule. No `+1000 compensation` term exists in miner, ConnectBlock, RES mint, or treasury — and this TASK forbids adding one.

**Current vs proposed ledger at genesis:**

| | Ledger `S_ORU(0)` | UTXO circulating | Serialized coinbase |
|--|--|--|--|
| **Current** | 1000 | 0 | 1000 |
| **Proposed** | 0 | 0 | 0 |

Landing **both** `GetBlockSubsidy(0)=0` and `CreateGenesisBlock(..., 0)` aligns ledger, UTXO, and bytes. Changing only one of them recreates a mismatch.

---

## P5 — “No premine” is true for UTXO, false for the serialized/ledger 1000

There is **no** hidden:

```
Genesis reward = 0
  + Genesis treasury allocation = 1000
```

today, and none is required to land the proposal.

Today:

```
ORU reward serialized     = 1000
ORU allocation            = 0
ORU UTXO                  = 0
ORU mint                  = 0
RES mint on genesis       = 0
```

Whitepaper “no premine” is **true for spendable ORU** and **false if someone counts GetBlockSubsidy(0) or the genesis `nValue` as issued supply**. The proposed change makes those three views agree at 0.

Other assets: genesis has no RES output. Do not delete or rewrite the placeholder scriptPubKey; it is not an ORU premine.

`GetResMintReward(0) = 40 RES` remains the **epoch-0 schedule value**. Genesis ConnectBlock never mints. Height 1 still sits in epoch 0. **Do not zero RES `W(0)` as part of this change.**

---

## P6 — Coinbase / genesis special path

`ConnectBlock` (`validation.cpp`):

```
if (block_hash == params.GetConsensus().hashGenesisBlock) {
    // skip connection of its transactions (coinbase unspendable)
    view.SetBestBlock(...);
    // empty draw set for children
    return true;
}
```

Consequences:

- Genesis coinbase amount is **not** checked against `GetBlockSubsidy(0)` (`bad-cb-amount` never runs on genesis).
- Genesis is **not** a normal height-0 coinbase.
- `LoadGenesisBlock` writes the hardcoded block to disk and indexes it; `ActivateBestChain` then hits the skip.
- `ValidatePaidWinnerRecord` at height 0 forbids RES mint / paid-winner records.
- `ContextualCheckBlock`: height 0 must carry zero new convergence.

Supply / UTXO / merkle / chainstate:

- Merkle is a function of the coinbase bytes → **will change** if `nValue` changes.
- UTXO set already excludes genesis → **no UTXO delta** from rewriting `nValue`, except via a full chain reinit.
- RFC-002 ledger **will** drop 1000 at every height if `GetBlockSubsidy(0)` becomes 0 (the genesis term is in every path-fold).

---

## P7 — Hash / genesis block impact

Changing coinbase `nValue` 1000 → 0 **does** change:

1. coinbase txid  
2. merkle root (single-tx block ⇒ merkle root = txid)  
3. genesis block hash  
4. `consensus.hashGenesisBlock` asserts  
5. checkpoints `{0, hash}`  
6. `defaultAssumeValid` while it is pinned to genesis  

Mainnet **current** freeze (RFC-016 / Alpha1):

| Field | Current |
|-------|---------|
| coinbase `nValue` | 1000 ORU |
| merkle root | `ccdba6059e3d93d9f9eddc83e17cf358d8181155d39ed4b0526d8cd4439528a8` |
| genesis hash | `1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758` |

Mainnet **proposed** vectors (same timestamp, nonce, nBits, scriptPubKey, message; only `nValue=0`). **Not production-frozen:**

| Field | Proposed |
|-------|----------|
| coinbase txid / merkle | `9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd` |
| genesis hash | `82c0cbee504576dc15a0a926382c0aa41f6a368817143cda97183bfedd9c762e` |

Locked in `core/src/test/genesis_zero_issuance_p0_037_tests.cpp` as `PROPOSED_MAINNET_*`.

Every shipped network (`main`, `testnet`, `regtest`, `devtest`, `slowtest`) passes `ORU_INITIAL_BLOCK_SUBSIDY` into `CreateGenesisBlock`. All of them need a new hash freeze.

---

## P8 — Subsequent history

```
genesis hash → block 1 prevhash → block 2 prevhash → …
```

Any local `blocks/`, `chainstate/`, wallet birth, or cached functional-test chain built on the old genesis is **foreign** after rebuild.

| Artifact | Action on landing |
|----------|-------------------|
| Protocol model `GenesisReward(0)=0` | Land with C++ |
| Existing datadir `blocks/` `chainstate/` | Delete / reindex from empty; cannot rewind genesis |
| Wallet files | New chain id; old txs are another chain |
| regtest/testnet fixtures keyed on genesis hash | Regenerate |

Mainnet has not launched, so this is an operational cost, not a live-user migration.

---

## P9 — W / T / RES / maturity

| Invariant | Relation to genesis ORU | Impact |
|-----------|-------------------------|--------|
| W = 32 | Draw window; not a function of coinbase `nValue` | NONE |
| T = frozen `RES_PROBABILITY_TARGET_CONST` | `T(W)` from unwindowed/W | NONE |
| RES `W(0) = 40 RES` | Height-epoch mint schedule | NONE (must stay) |
| Maturity = 100 | `nCoinbaseMaturity` / `RES_MINT_MATURITY` | NONE |
| OpuHash | Header PoW; genesis nBits/nonce unchanged in the proposed reconstruction | NONE (hash of header still changes because merkle changed) |
| ORU schedule `h>=1` | Same `ORU_INITIAL_BLOCK_SUBSIDY` | NONE if height 0 is special-cased |

Header merkle change **does** change the genesis header hash. OpuHash **algorithm** is untouched. Genesis nBits/nonce can stay; a new header id is still required.

---

## P10 / P11 — Coq and refinement boundary

New module `RigorV01.GenesisZeroIssuance`:

- `genesis_reward_zero`, `genesis_supply_zero`, `genesis_has_no_oru_issuance`, `genesis_has_no_oru_allocation_proposed`
- Negative predicates: `genesis_reward_nonzero`, `genesis_hidden_allocation`, `genesis_mint_nonzero`, `genesis_supply_mismatch`
- `cpp_refinement_proved = false`

**Not done (deliberate):** `ConnectBlockMint.cb_expected_oru_subsidy` still mirrors **current** C++ (1000 at height 0). Updating it now would make Coq diverge from production without a C++ landing, and would break `cb_expected_oru_subsidy_pos`.

```
MODEL_PROVED     = proposed rule in GenesisZeroIssuance.v
CPP_REFINEMENT   = NOT_PROVED
```

There is no proved equivalence `GetBlockSubsidy(0) = 0`. Claiming `refinement_proved = true` would be false.

Landing Coq follow-up (not this TASK): special-case height 0 in `cb_expected_oru_subsidy`, replace `cb_expected_oru_subsidy_pos` with `h = 0 → 0` and `h > 0 → > 0`. No new Axiom.

---

## P12 — Golden vectors

| Vector | Current (frozen) | Proposed (computed, not frozen) |
|--------|------------------|----------------------------------|
| Mainnet coinbase `nValue` | 1000 ORU | 0 |
| Mainnet merkle / txid | `ccdba605…9528a8` | `9d3fbd20…3515dd` |
| Mainnet genesis hash | `1c2d6436…2ad758` | `82c0cbee…9c762e` |
| Height 0 issuance | 1000 ledger / 0 UTXO | 0 / 0 |
| Height 1 reward | 1000 ORU | 1000 ORU |
| Height 2 reward | 1000 ORU | 1000 ORU |

No 1000-ORU jump at height 1 in the proposed cumulative series.

---

## P13 / P14 — Tests added

File: `core/src/test/genesis_zero_issuance_p0_037_tests.cpp`

Current-state locks (must pass **before** landing):

- `current_getblocksubsidy_height0_is_1000`
- `genesis_serialized_reward_is_1000`
- `genesis_hash_matches_frozen_vector` / `genesis_merkle_matches_frozen_vector`
- `genesis_no_oru_utxo` / `genesis_no_hidden_allocation` / `genesis_no_res_mint`
- `height_1_reward_unchanged` / `height_2_reward_unchanged` (function + mined `TestChain100Setup`)
- `frozen_p0_parameters_unchanged` (W, T, RES W(0), maturity, `ORU_INITIAL_BLOCK_SUBSIDY`)

Proposed-impact locks:

- `proposed_reward_zero_changes_txid_merkle_hash` (mainnet proposed hashes)
- `proposed_zero_changes_every_shipped_network_genesis_hash`

Negative / mismatch:

- `genesis_reward_nonzero_is_current_serialized_state` — current fails the proposed “reward = 0” predicate
- `genesis_hidden_allocation_rejected_by_construction` — extra vout changes the hash
- `genesis_mint_nonzero_is_not_the_frozen_genesis`
- `genesis_supply_mismatch_ledger_vs_utxo` — ledger 1000 vs UTXO 0

After a C++ landing, the “current 1000” cases become the new zeros; proposed hashes become the production freeze. Do not flip those tests in this TASK.

---

## P15 — No compensatory logic

Forbidden on landing:

- `GetBlockSubsidy(1) = 2000`
- extra height-1 treasury
- RES mint bump
- fee-burn exemption that credits 1000
- “to keep old supply, add 1000 somewhere”

Required economics:

```
Genesis = 0
h >= 1  = existing frozen schedule
```

not

```
Genesis = 0 + hidden compensation = 1000
```

---

## Landing checklist (future TASK — not executed here)

1. `CreateGenesisBlock(..., /*genesisReward=*/0)` on **every** network.  
2. `GetBlockSubsidy`: `if (nHeight == 0) return 0;` **before** the halving arithmetic. Do **not** change `ORU_INITIAL_BLOCK_SUBSIDY`.  
3. Freeze new hashes in `chainparams.cpp` asserts, checkpoints, `alpha1_genesis_tests.cpp`, `genesis-lockdown.md`.  
4. `mine_genesis.py` `GENESIS_REWARD = 0`.  
5. Coinstats / functional tests: `unspendables.genesis_block` becomes 0.  
6. Coq `ConnectBlockMint.cb_expected_oru_subsidy` height-0 special case.  
7. Wipe local `blocks/` + `chainstate/`.  
8. Re-run `make check` and genesis/supply/mint/reorg tests.  
9. Confirm `GetBlockSubsidy(1)` still 1000 ORU and `GetResMintReward(0)` still 40 RES.

If (1) and (2) are not landed together, ledger and bytes diverge.

---

## P0 freeze (this TASK)

Unchanged: W=32, T, RES reward curve, OpuHash, Ed25519, ScriptPubKey layout, sighash, maturity=100, ORU schedule for `h>=1`, converge, fee-burn, P2P, wallet format, `ORU_INITIAL_BLOCK_SUBSIDY`.

---

## Verification (this TASK)

```
cd formal_verification/coq
make -f Makefile.coq clean
make -f Makefile.coq -j$(nproc)

cd ../../core
make -j$(nproc)
make check
./src/test/test_rigor --run_test=genesis_zero_issuance_p0_037_tests,genesis_zero_issuance_p0_037_mined_tests
```

Unit coverage also includes existing `alpha1_genesis_tests`, subsidy tests, `mint_supply_reorg_p0_036_tests`, and `w32_reorg_stability_tests` via `make check`.
