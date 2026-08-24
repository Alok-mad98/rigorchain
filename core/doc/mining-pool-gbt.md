# Mining pools: `getblocktemplate`, coinbase, and RES

This document describes how **Rigor Core** exposes block templates and how **coinbase outputs** relate to **ORU** (subsidy + ORU-domain fees), **RES draw mint**, and the **Segwit witness commitment**. It is aimed at integrators wiring **getblocktemplate** / **submitblock** or custom stratum backends.

**Normative draw and emission rules** remain in [convergence-consensus-v1.md](./convergence-consensus-v1.md). The **kernel is unchanged** by the path cleanup (only C and the mint commitment were uniquified):

| Kernel | Lock |
|--------|------|
| Draw | `σ_i = Hash256(txid_i ‖ C ‖ B_{h-1} ‖ LE32(h))`, eligible iff `σ_i < T · f_i`. `T` is frozen (`RES_PROBABILITY_TARGET`), not parent `nBits`. `f_i` is the burn `nValue`. |
| Window | `W = 32`. The draw paid at height `h` uses unpaid tickets in `[h-32, h-1]`. |
| Lag | **1 block.** This template **pays** the parent's committed set and **commits** C for the next block. |
| Owner | Burn lock is `<20-byte HASH160(RES program)> OP_CONVERGE`. `OP_RES_MINT` must pay that hash (`res_payout_hash` on GBT). |

Here we focus on **wire layout** and **RPC fields**.

---

## 1. RPC: `getblocktemplate`

- **Call shape:** Bitcoin-compatible. You **must** pass **`{"rules": ["segwit"]}`** (and **`"signet"`** on signet chains). See `src/rpc/mining.cpp`.
- **Connection / sync:** On mainnet-like chains the node refuses GBT if it has **zero** peer connections or is in **initial block download** (test chains may skip this).

### 1.1 Rigor-specific JSON fields

| Field | Meaning |
|--------|---------|
| `default_witness_commitment` | Hex script of the **Segwit witness commitment** output the template expects (BIP 141), if present. |
| `default_res_mint` | When the parent draw named a winner: `{value, scriptPubKey}` for the coinbase `OP_RES_MINT` output. Copy it. Omitted when there is no mint. |
| `convergence_root` | Window **C** the coinbase `scriptSig` must carry (exactly those 32 bytes after the height push). |
| `convergence_draw` | The draw this template **pays** (parent set, winner, T). Candidate field is **`res_payout_hash`** (HASH160 of the declared RES program), not a program blob. |

There is **no** `default_pcorb_carrier` and **no** `default_convergence_commitment`. Leftover `OP_RETURN` tags (CVGE / RIGR / PCORB) are not template fields and are not consensus.

Standard fields (`transactions`, `previousblockhash`, `bits`, `height`, `curtime`, `coinbasevalue`, …) follow BIP 22 / 145 behavior with Rigor’s dual-asset coinbase (see §2).

### 1.2 `coinbasevalue` (important)

`coinbasevalue` is set from **only the first coinbase output** (`vout[0]`): **ORU block subsidy + claimable ORU-domain fees** collected from the template's non-coinbase transactions (RFC-018: the RES-transfer fee burn share is **not** claimed).

It does **not** include:

- RES draw issuance (`OP_RES_MINT` / `default_res_mint` — new RES, not taken from fees),
- any historical RES-fee output (`nFeesRes` is **0** on production paths),
- witness commitment (zero-value for ORU accounting).

Pools that pay miners from **`coinbasevalue`** should treat it as **ORU** per `amount.h` / RPC conventions—not as "total block reward in all assets."

---

## 2. Coinbase construction order (`BlockAssembler::CreateNewBlock`)

Implementation: `src/node/miner.cpp`.

### 2.0 Coinbase `vin[0].scriptSig` (BIP34 + ConvergenceRoot) — **consensus-critical**

After **BIP34 / height-in-coinbase** is active on the chain (Rigor regtest/main-like deployments), the coinbase **`scriptSig` must match exactly** what the node builds:

- **Exactly two data pushes**, in order:
  1. **BIP34 block height** — same encoding rules as Bitcoin Core (`CScript() << nHeight`: `OP_1`…`OP_16` for heights `1`…`16`, otherwise a minimal push of `CScriptNum`).
  2. **32-byte ConvergenceRoot** — `uint256` equal to **`Consensus::ComputeConvergenceRootFromConvergenceSet(block.vtx, utxo_view_at_block_entry, height, consensus)`** (`src/consensus/convergence_root.cpp` → `ComputeConvergenceRootFromEligibilityTxids` in `convergence_root_eligibility.cpp`): a **Hash256** over the **concatenation** of **`GetHash()`** (witness-stripped) for every transaction in the **convergence subset** **F**, with txids sorted by **`uint256` ascending**.

**F membership** (`ListConvergenceRootCommitmentTxids`): non-coinbase txs that are `OP_CONVERGE`-shaped, meet **MIN_CONVERGENCE_AMOUNT**, declare a RES payout owner (HASH160 in that lock), and pass **`CheckTxInputs`** against **UTXO at block entry**. This C is the **window** set (unpaid tickets in the last **W=32** blocks ending at this height), not “only this block’s body.” **Fee is relay policy, not F membership:** a zero-fee burn that otherwise verifies still enters F (`DEFAULT_MIN_RELAY_TX_FEE` keeps spam off the mempool; consensus does not require `fee > 0`).

**C from F:**

- **Empty F** (`n == 0`): all-zero `uint256()` (no hash).
- **Any nonempty F** (`n >= 1`): `C = Hash256(concat(sort(txids)))`. A **singleton hashes** `Hash256(txid)`. There is **no** `|F| < 2 → zeros` shortcut.

- **No third push**, no `OP_0` placeholder, and **no “extra extranonce” data in `scriptSig`** — legacy `height + OP_0` coinbases are **rejected** (`bad-cb-convergence-root` in **`ConnectBlock`** when `ValidateCoinbaseConvergenceRootScriptSig` fails).

- **Total `scriptSig` length** must still satisfy the global coinbase `scriptSig` size bounds (`CheckTransaction` in `src/consensus/tx_check.cpp`: **2–100 bytes**).

**Extranonce / pool metadata:** use **`vin[0].scriptWitness`** (witness reserved value) and/or other **BIP22 `coinbaseaux`** fields — **not** extra pushes in `scriptSig`, or the block will fail consensus.

The node compares **`scriptSig` byte-for-byte** to `CScript() << nHeight << std::vector<unsigned char>(convergence_root)`; non-canonical encodings of the same height or root **fail**.

---

The built coinbase transaction is assembled in this **logical order**:

1. **`vin[0]`** — null prevout; **`scriptSig`** as in §2.0 (height + 32-byte ConvergenceRoot).

2. **`vout[0]` — always**
   - **Asset:** `ASSET_ORU`
   - **Value:** `(nFeesOru - nFeesOruResTransferBurn) + GetBlockSubsidy(height)` (RFC-018: do not claim the RES-transfer ORU fee burn share)
   - **`scriptPubKey`:** pool / miner payout (the `scriptPubKeyIn` passed into `CreateNewBlock`).

3. **Optional — RES draw mint** (when the parent draw named a winner and `W(h) > 0`)
   - **Asset:** `ASSET_RES`
   - **Amount:** `GetResMintReward(height)`
   - **`scriptPubKey`:** `OP_RES_MINT` paying the winner's declared HASH160.
   - Inserted **before** the witness commitment. There is no PCORB restatement and no body-tx mint.

4. **Witness commitment** — `ChainstateManager::GenerateCoinbaseCommitment` may **append** the standard **BIP 141** `OP_RETURN` witness commitment output (`src/validation.cpp`).

**External miners:** copy `vout[0]` plus `default_res_mint` when present, plus the witness commitment. The coinbase `scriptSig` must carry the window `convergence_root` from GBT.

---

## 3. Leftover OP_RETURN tags (not consensus)

**CVGE / RIGR / PCORB are not required.** Extra `OP_RETURN` outputs are allowed and ignored.

What **is** consensus:

- The unique window **C** lives in the coinbase **`scriptSig`** (§2.0).
- New RES issuance is a coinbase **`OP_RES_MINT`** output (§4).
- Parent **nBits** live in the **block header** (`prev_nbits` on GBT is informational).

Historical CVGE / RIGR / PCORB encodings may still appear as leftover `OP_RETURN` bytes in old software. `ConnectBlock` does **not** require them, match them, or pair a mint with a PCORB carrier. Do not copy those outputs from archived templates.

`convergence_draw.eligible_root` on GBT is the flat hash of the **inequality-eligible** subset (same concat-then-Hash256 shape as C). It is **not** a coinbase commitment.

**Inspection RPC:** `getblockconvergence <hash|height>` and **`getconvergenceresult`** remain useful for debugging committed vs recomputed window C / draw fields. Some `getblock` JSON keys remain **wire-stable** historical spellings (e.g. `arda_target`, `arda_winner`); integrators should treat them as opaque RPC keys, not product names.

---

## 4. RES mint (coinbase `OP_RES_MINT`)

Design summary (mint vs user transfer): see [protocol_design.md](./protocol_design.md).

**Constants:** `src/consensus/rigor_consensus_lock.h`, `src/consensus/res_amount.h`

- **RES amounts:** Integer **atomic units** with **`RES_COIN`** scaling per **1.00 RES** (two decimal places). **`RES_REWARD_AMOUNT`** documents the nominal draw unit when applicable.

**Consensus (normative):** New RES issuance is validated only via **`rigor::ConnectEvaluateCoinbaseResMint`** (`src/consensus/rigor_res.cpp`) over the coinbase **`OP_RES_MINT`** output. When the parent draw named a winner and `W(h) > 0`, GBT **`default_res_mint`** must be copied into the reconstructed coinbase. There is no PCORB carrier and no body-tx mint.

**Removed / forbidden:** Coinbase-derived program APIs (`GetOruOnlyCoinbaseTxId`, `ResMintWitnessProgram`, …) remain as **hard-failing stubs** so accidental use aborts at runtime.

Native **user** RES outputs (transfers, not issuance) remain **witness version 1** with a **32-byte program** per `script/standard.cpp`. The mint lock itself is **`OP_RES_MINT`**.

---

## 5. Recommended integration patterns

### A. Node-assembles coinbase (simplest)

1. `getblocktemplate '{"rules":["segwit"]}'`
2. Take **`transactions`**, **`previousblockhash`**, **`bits`**, **`height`**, **`curtime`**, and the hex coinbase from the **serialized block** if your stack reconstructs the full block from the node — or run a **local** `generatetoaddress` / internal miner for testing.

3. For production, many setups **do not** hand-build coinbase: they use **`submitblock`** with a block the node already validated, or extend a template only in allowed “mutable” fields (`time`, `nonce`, extranonce in `coinbaseaux` per BIP 22).

**Rigor constraint:** Any custom coinbase must replicate **ORU `vout[0]` + optional `OP_RES_MINT` + witness commitment** ordering and formulas above, with **C** in `scriptSig`.

### B. `-miningresfeeaddr` (local automint arming)

Network fees remain **ORU-only**. Production `nFeesRes ≡ 0`; this flag is **not** a RES-fee destination.

Set in `rigor.conf` (or CLI) to arm automatic Convergence mint assembly / wallet bootstrap on mockable chains:

```text
-miningresfeeaddr=<bech32 witness-v1-RES address>
```

The address must decode to a **witness v1 RES** program (`src/node/miner.cpp` `ResolveMiningResFeeScript`). The prize still pays the draw winner via `OP_RES_MINT`, not this address.

### C. Inspection and debugging

| RPC | Purpose |
|-----|---------|
| `getblockconvergence` | Parsed convergence fields from a mined block |
| `getconvergenceresult` | Draw bind, target, candidates, committed vs computed |
| `getconvergenceinfo` / `getconvergencerank` | Additional convergence / ranking helpers |

---

## 6. Code map

| Topic | Location |
|--------|-----------|
| GBT + `default_res_mint` / `convergence_root` / `convergence_draw` | `src/rpc/mining.cpp` |
| Coinbase assembly, ORU fees, RFC-018 burn, `OP_RES_MINT` | `src/node/miner.cpp` |
| ConvergenceRoot (body txid set + coinbase `scriptSig` check) | `src/consensus/convergence_root.{h,cpp}`, `convergence_root_eligibility.cpp`, `ContextualCheckBlock` in `src/validation.cpp` |
| RES mint (coinbase only) | `src/consensus/rigor_res.cpp` (`ConnectEvaluateCoinbaseResMint`) |
| RES units | `src/consensus/res_amount.h` |
| `ConnectBlock` coinbase checks | `src/validation.cpp`, `src/connect_block_rigor_checks.cpp` |
| Draw / reward rules | [convergence-consensus-v1.md](./convergence-consensus-v1.md) |

---

## 7. Testing (RES mint randomness)

- **Functional (weak):** `test/functional/feature_rigor_convergence_rng_consensus.py` mines **≥2** convergence txs in one block on **devtest** and asserts **deterministic** inspection RPCs (`getconvergenceresult`, `getblockconvergence`), **without** asserting whether a RES draw mint appears (draw can be probabilistic).
- **Unit:** `test/res_orum_tests.cpp`, `test/rigor_witness_tx_tests.cpp` (witness path + RES classification).

## 8. See also

- [convergence-consensus-v1.md](./convergence-consensus-v1.md) — convergence draw, RES mint predicates, supply cap, retarget
- `convergence-integration-directive.md` — fee layer vs draw layer
