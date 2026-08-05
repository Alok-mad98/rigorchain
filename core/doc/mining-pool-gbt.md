# Mining pools: `getblocktemplate`, coinbase, CVGE, and RES

This document describes how **Rigor Core** exposes block templates and how **coinbase outputs** relate to **ORU** (subsidy + ORU-domain fees), **RES draw mint** (body txs), **RES-domain fees**, **Segwit witness commitment**, and the **CVGE** convergence commitment. It is aimed at integrators wiring **getblocktemplate** / **submitblock** or custom stratum backends.

**Normative draw and emission rules** (when RES mints, convergence counts, supply cap, retarget) remain in [convergence-consensus-v1.md](./convergence-consensus-v1.md) (`CONSENSUS_LOCK_FORGE_V3`). Here we focus on **wire layout** and **RPC fields**.

---

## 1. RPC: `getblocktemplate`

- **Call shape:** Bitcoin-compatible. You **must** pass **`{"rules": ["segwit"]}`** (and **`"signet"`** on signet chains). See `src/rpc/mining.cpp`.
- **Connection / sync:** On mainnet-like chains the node refuses GBT if it has **zero** peer connections or is in **initial block download** (test chains may skip this).

### 1.1 Rigor-specific JSON fields

| Field | Meaning |
|--------|---------|
| `default_witness_commitment` | Hex script of the **Segwit witness commitment** output the template expects (BIP 141), if present. |
| `default_convergence_commitment` | Hex **`scriptPubKey`** of the **CVGE** `OP_RETURN` output the template’s coinbase already includes (see §3). Omitted if parsing fails (should not happen for templates from this node). |

Standard fields (`transactions`, `previousblockhash`, `bits`, `height`, `curtime`, `coinbasevalue`, …) follow BIP 22 / 145 behavior with Rigor’s dual-asset coinbase (see §2).

### 1.2 `coinbasevalue` (important)

`coinbasevalue` is set from **only the first coinbase output** (`vout[0]`): **ORU block subsidy + ORU-domain fees** collected from the template's non-coinbase transactions.

It does **not** include:

- RES draw subsidy from body mint txs,
- RES fee outputs,
- witness commitment or CVGE `OP_RETURN` outputs (those are zero-value for ORU accounting in this output).

Pools that pay miners from **`coinbasevalue`** should treat it as **ORU** per `amount.h` / RPC conventions—not as "total block reward in all assets."

---

## 2. Coinbase construction order (`BlockAssembler::CreateNewBlock`)

Implementation: `src/node/miner.cpp`.

### 2.0 Coinbase `vin[0].scriptSig` (BIP34 + ConvergenceRoot) — **consensus-critical**

After **BIP34 / height-in-coinbase** is active on the chain (Rigor regtest/main-like deployments), the coinbase **`scriptSig` must match exactly** what the node builds:

- **Exactly two data pushes**, in order:
  1. **BIP34 block height** — same encoding rules as Bitcoin Core (`CScript() << nHeight`: `OP_1`…`OP_16` for heights `1`…`16`, otherwise a minimal push of `CScriptNum`).
  2. **32-byte ConvergenceRoot** — `uint256` equal to **`Consensus::ComputeConvergenceRootFromConvergenceSet(block.vtx, utxo_view_at_block_entry, height, consensus)`** (`src/consensus/convergence_root.cpp`): a **Hash256** over the **concatenation** of **`GetHash()`** (witness-stripped) for every transaction in the **convergence subset** **F** (see `convergence-consensus-v1.md`), with txids sorted by **`uint256` ascending**. **F** includes only non-coinbase txs that are convergence-shaped, meet **MIN_FORGE_AMOUNT**, and have **fee > 0** from **`CheckTxInputs`** against **UTXO at block entry**. If **|F| < 2**, ConvergenceRoot is the **all-zero** `uint256()`.

- **No third push**, no `OP_0` placeholder, and **no “extra extranonce” data in `scriptSig`** — legacy `height + OP_0` coinbases are **rejected** (`bad-cb-convergence-root` in **`ConnectBlock`** when `ValidateCoinbaseConvergenceRootScriptSig` fails).

- **Total `scriptSig` length** must still satisfy the global coinbase `scriptSig` size bounds (`CheckTransaction` in `src/consensus/tx_check.cpp`: **2–100 bytes**).

**Extranonce / pool metadata:** use **`vin[0].scriptWitness`** (witness reserved value) and/or other **BIP22 `coinbaseaux`** fields — **not** extra pushes in `scriptSig`, or the block will fail consensus.

The node compares **`scriptSig` byte-for-byte** to `CScript() << nHeight << std::vector<unsigned char>(convergence_root)`; non-canonical encodings of the same height or root **fail**.

---

The built coinbase transaction is assembled in this **logical order**:

1. **`vin[0]`** — null prevout; **`scriptSig`** as in §2.0 (height + 32-byte ConvergenceRoot).

2. **`vout[0]` — always**  
   - **Asset:** `ASSET_ORU`  
   - **Value:** `GetBlockSubsidy(height) + nFeesOru`  
   - **`scriptPubKey`:** pool / miner payout (the `scriptPubKeyIn` passed into `CreateNewBlock`).

3. **Optional — RES draw mint** (when the block includes a valid body RES mint coupled to the convergence winner)  
   - **Asset:** `ASSET_RES`  
   - **Amount:** per `ConnectBlock` / `rigor::ConnectEvaluateResMintTx` (`src/consensus/rigor_res.cpp`).  
   - **`scriptPubKey`:** witness v1 RES program; see §4.  
   - Inserted **before** witness commitment and CVGE.

4. **Optional — RES fee aggregation** (only if the mempool contributed **RES-domain fees** `nFeesRes > 0`)  
   - **Asset:** `ASSET_RES`  
   - **Value:** total RES fees from included txs  
   - **`scriptPubKey`:** must be a **Witness v1 RES** payout script.  
   - The node uses **`scriptPubKeyResFeeIn`** if provided to `CreateNewBlock`, otherwise **`-miningresfeeaddr=<witness-v1-RES address>`** (`src/init.cpp`). If RES fees are non-zero and **no** script is configured, **`CreateNewBlock` throws** — the pool must supply a RES fee address.

5. **Witness commitment** — `ChainstateManager::GenerateCoinbaseCommitment` may **append** the standard **BIP 141** `OP_RETURN` witness commitment output (`src/validation.cpp`).

6. **CVGE commitment** — **always appended last** in the current assembler: zero-value **`ASSET_ORU`** output with **`OP_RETURN`** payload magic **`CVGE`** (see §3).

**External miners:** If you rebuild the coinbase, you must preserve this structure (and recompute commitments) or **submit blocks built by the node** (recommended).

---

## 3. CVGE convergence commitment (coinbase `OP_RETURN`)

**Magic:** ASCII **`CVGE`** (`0x46 0x52 0x47 0x45`).  
**Serialization:** `src/consensus/convergence_commitment.cpp`

| Offset (bytes) | Size | Content |
|----------------|------|---------|
| 0 | 4 | `CVGE` |
| 4 | 1 | Version (must be **2**) |
| 5 | 8 | `m_cumulative_convergence` — **little-endian uint64** (ORU units at `COIN` scale, cumulative chain statistic) |
| 13 | 32 | `m_draw_candidates.root` — Merkle root over **ConvergenceDrawCandidates-eligible** convergence candidate txids for this block (same membership predicate as `ComputeConvergenceDrawCandidateRoot` in `convergence_logic.cpp`) |

**Script form:** `OP_RETURN <45-byte push>` (`COMMITMENT_PAYLOAD_V2_SIZE` in `convergence_commitment.cpp`). There is **no** on-chain winner field: the convergence draw winner is derived from block body txs (see `CollectConvergenceWinnerTxid` / `GetBlockWinner`).

**Consensus:** Cumulative convergence and `m_draw_candidates.root` must match values recomputed in `ConnectBlock` / `ContextualCheckBlock`; mismatch → **`bad-convergence-draw-candidate-root`**. The separate **RIGOR** draw-target `OP_RETURN` (8-byte payload: ASCII `RIGOR` tag + 4-byte LE compact parent `nBits`) is validated in **`rigor::CheckRigorDrawTargetCommitment`** (`bad-rigor-commitment-missing`, `bad-rigor-target-mismatch`). RES mint proofs are enforced only on **non-coinbase** txs via **`rigor::ConnectEvaluateResMintTx`** (`src/consensus/rigor_res.cpp`) and **`Consensus::ParseConvergeProof` / `Consensus::VerifyConvergeProof`** (`src/consensus/res_proof.cpp`). See `convergence-consensus-v1.md`.

**Parsing:** Walk **all** `vout`; the **last** valid CVGE push wins (`FindLastConvergenceCommitmentInCoinbase`).

**Inspection RPC:** `getblockconvergence <hash|height>` returns parsed fields from a confirmed block (`src/rpc/blockchain.cpp`). **`getconvergenceresult`** exposes bind hash, draw target, candidates, and committed vs recomputed roots for debugging.

**Note:** Some `getblock` JSON fields remain **wire-stable** historical spellings (e.g. `arda_target`, `arda_winner`); integrators should treat them as opaque RPC keys, not product names.

---

## 4. RES mint (block body — witness self-proof)

Design summary (mint vs user transfer): see [protocol_design.md](./protocol_design.md).

**Constants:** `src/consensus/rigor_consensus_lock.h`, `src/consensus/res_amount.h`

- **RES amounts:** Integer **atomic units** with **`RES_COIN`** scaling per **1.00 RES** (two decimal places). **`RES_REWARD_AMOUNT`** documents the nominal draw unit when applicable.

**Consensus (normative):** New RES issuance is validated only via **`rigor::ConnectEvaluateResMintTx`** (`src/consensus/rigor_res.cpp`) and proof helpers in **`src/consensus/res_proof.cpp`**. Inputs are **convergence transaction txid**, **bind hash**, **block height**, **parent compact target (`nBits`)**, and the **explicit witness proof stack** — never coinbase txid, coinbase `scriptSig`, or `hashMerkleRoot`.

**Removed / forbidden:** Coinbase-derived program APIs (`GetOruOnlyCoinbaseTxId`, `ResMintWitnessProgram`, …) remain as **hard-failing stubs** so accidental use aborts at runtime.

**`scriptPubKey` shape** for native RES outputs is **witness version 1** with a **32-byte program** (RES2-tagged or legacy layout per `script/standard.cpp`); the **program bytes are not** derived from the coinbase.

---

## 5. Recommended integration patterns

### A. Node-assembles coinbase (simplest)

1. `getblocktemplate '{"rules":["segwit"]}'`  
2. Take **`transactions`**, **`previousblockhash`**, **`bits`**, **`height`**, **`curtime`**, and the hex coinbase from the **serialized block** if your stack reconstructs the full block from the node — or run a **local** `generatetoaddress` / internal miner for testing.

3. For production, many setups **do not** hand-build coinbase: they use **`submitblock`** with a block the node already validated, or extend a template only in allowed “mutable” fields (`time`, `nonce`, extranonce in `coinbaseaux` per BIP 22).

**Rigor constraint:** Any custom coinbase must replicate **ORU + RES + witness + CVGE** ordering and formulas above.

### B. `-miningresfeeaddr` (required when mempool has RES-fee txs)

If included transactions pay **RES** fees, **`CreateNewBlock`** requires a RES fee destination. Set in `rigor.conf` (or CLI):

```text
-miningresfeeaddr=<bech32 witness-v1-RES address>
```

The address must decode to a **witness v1 RES** program (`src/node/miner.cpp` `ResolveMiningResFeeScript`).

### C. Inspection and debugging

| RPC | Purpose |
|-----|---------|
| `getblockconvergence` | CVGE fields from a mined block |
| `getconvergenceresult` | Draw bind, target, candidates, committed vs computed |
| `getconvergenceinfo` / `getconvergencerank` | Additional convergence / ranking helpers |

---

## 6. Code map

| Topic | Location |
|--------|-----------|
| GBT + `default_convergence_commitment` | `src/rpc/mining.cpp` |
| Coinbase assembly, fees, RES, CVGE | `src/node/miner.cpp` |
| ConvergenceRoot (body txid set + coinbase `scriptSig` check) | `src/consensus/convergence_root.{h,cpp}`, `ContextualCheckBlock` in `src/validation.cpp` |
| CVGE encode/decode | `src/consensus/convergence_commitment.{h,cpp}` |
| RES mint + draw coupling | `src/consensus/rigor_res.cpp`, `src/consensus/res_proof.cpp`, `src/consensus/convergence_logic.{h,cpp}` |
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
