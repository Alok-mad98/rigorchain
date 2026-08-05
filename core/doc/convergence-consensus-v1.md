# convergence v3.0 Consensus Lock

**See also** [`PROTOCOL-NAMING-TRUTH-SOURCE.md`](./PROTOCOL-NAMING-TRUTH-SOURCE.md) (authoritative `bad-*` rejection codes, RPC replay fields, and `state_hash` / RPL2 naming; `src/` is the source of truth).

**Marker: `CONSENSUS_LOCK_FORGE_V3`**

This document is the normative summary of **convergence RES draw** consensus rules implemented in `src/consensus/` (see `#define CONSENSUS_LOCK_FORGE_V3` in `convergencerng.h`). Mempool policy and miner ordering are **out of scope** here.

**Related docs (non-normative reminders for contributors):**

- `convergence integration directive — fee vs draw layers` (do not use fee to pick the winner; convergence transactions are ordinary transactions).
- [UTXO `Coin::nFlags` follow-up](./utxo-coin-nflags.md) (separate topic: UTXO extension flags and reorg safety).

**Historical note (v1.0 → v2.0):** Draw preimage was **`H(txid ‖ bind_hash)`** only. **v2.0** appends the **block height `H`** of the block under evaluation: **`H(txid ‖ bind_hash ‖ H)`**, with `H` serialized as **unsigned 64-bit little-endian** (same encoding as `Serialize(HashWriter, uint64_t)` in the codebase). This prevents cross-block replay of the same draw value for the same `txid` when `bind_hash` repeats in a pattern, gives a unique “draw period” per height, and makes verification inputs explicit (`txid`, bind block hash, height).

**Historical note (v2.0 → v3.0 — RES hardening layer):** **v3.0** keeps the **same** draw inequality and winner algorithm as v2.0 but tightens structure and coinbase checks:

- **One convergence output per convergence transaction:** `CountconvergenceOutputs(tx) == 1`; otherwise the transaction is invalid (`invalid-convergence-structure`). `IsconvergenceTx` is defined accordingly.
- **Coinbase CVGE v2:** `m_cumulative_convergence` and `m_draw_candidates.root` must match recomputation from the block body and prior headers (`ConnectBlock` / `ContextualCheckBlock`; mismatch **`bad-convergence-draw-candidate-root`**). The convergence winner txid is **not** stored in CVGE; it is derived via `GetBlockWinner` / `CollectConvergenceWinnerTxid` (same eligibility as `ComputeConvergenceDrawCandidateRoot`). The separate **RIGOR draw-target** `OP_RETURN` carries only the **compact parent `nBits`** (8-byte payload per `RigorTags::RIGOR`; `bad-rigor-commitment-missing`, `bad-rigor-target-mismatch`). RES mint proofs are enforced on **non-coinbase** txs (`bad-rigor-res-*` family in `rigor_res.cpp`).
- **RES mint consensus:** new RES is validated only via **witness self-proof** transactions in the block body (`rigor_res.cpp`, `res_proof.cpp`). There is **no** coinbase-derived RES program or ORU-only txid binding; deprecated coinbase-mint APIs abort if called.

---

## convergenceRoot (convergence-set PoW commitment — **code is law**)

This section documents **`Consensus::ComputeconvergenceRootFromconvergenceSet`** / **`ValidateCoinbaseconvergenceRootScriptSig`** (`src/consensus/convergence_root.{h,cpp}`) and **`ConnectBlock`** (`bad-cb-convergence-root`). It is **orthogonal** to the convergence draw preimage in §2 (RES draw uses `txid` / `bind_hash` / height — **not** convergenceRoot).

**Definition (normative):**

- Let **F** be the set of **non-coinbase** transactions `tx` in the block such that:
  - `Consensus::IsconvergenceTx(tx)` is true,
  - `Consensus::GetconvergenceAmount(tx) >= Consensus::MIN_FORGE_AMOUNT` (1 ORU),
  - `Consensus::CheckTxInputs` reports **fee > 0** against the **UTXO view at block entry** (same `CCoinsViewCache` `ConnectBlock` uses before applying the block’s transactions).
- Let **N = |F|**. Then:
  - If **N < 2**, **`convergenceRoot = uint256()`** (32 zero bytes).
  - If **N ≥ 2**, sort the convergence transaction ids by **`uint256` ascending order** (same ordering as `std::sort` on `uint256` / `memcmp` on the 32-byte blob), concatenate the **32-byte** `GetHash()` encodings in that order, and set **`convergenceRoot = Hash256(concat)`** (Bitcoin double SHA-256). Block serial order, fees as sort keys, and Merkle trees over `vtx` **must not** be used.

**Coinbase is excluded** from **F**; convergenceRoot is **not** a digest of “all non-coinbase txs”.

**Where it is committed:**

- After BIP34 / height-in-coinbase is active, **`coinbase.vin[0].scriptSig`** must equal the canonical encoding **`CScript() << nHeight << std::vector<unsigned char>(convergence_root)`** (exactly **two** pushes: height per BIP34, then **32** bytes). See [mining-pool-gbt.md](./mining-pool-gbt.md) §2.0 for integrators.

**Parsing / validation (normative):**

- The node must **not** parse `scriptSig` with fixed offsets or `memcpy`. **`Consensus::ValidateCoinbaseconvergenceRootScriptSig`** walks the script with **`CScript::GetOp`**: first push → minimal **BIP34** height (must match this block’s height); second push → opcode **must be `0x20` (OP_PUSHBYTES_32)** with **exactly 32** payload bytes equal to **`ComputeconvergenceRootFromconvergenceSet(block.vtx, view_at_block_entry, height, consensus_params)`**; **any further opcode or data** (including `OP_PUSHDATA1`+32 bytes of the same root) **invalidates** the block (`bad-cb-convergence-root`).

**Binding chain (PoW impact):**

```
convergenceTx → script_commitment (CVGE in coinbase outputs) → convergence_root (Hash256 over sorted convergence transaction txids)
    → coinbase.scriptSig → coinbase txid → merkle_root → block header → OpuHash
```

**PoW (unchanged interface):**

- convergenceRoot does **not** appear in the **80-byte block header** and is **not** passed into `GetOpusHashFromBaseHash` / OpuHash. It affects PoW **only indirectly**: it changes the coinbase txid and therefore **`hashMerkleRoot`** in the header when the convergence subset or coinbase scriptSig changes.

**RES consensus seal (relationship):**

- The **convergence subset** is bound for the chain via convergenceRoot as above. **RES verification** (`rigor_res.cpp`, `res_proof.cpp`) **must not** use convergenceRoot, coinbase `scriptSig`, `hashMerkleRoot`, or any Merkle digest over `vtx` as a proof input — only the whitelist stated in those files (e.g. `convergence_txid` identifies **one** transaction for amount checks; it is not a substitute for reading aggregate block hashes).

**Important note (maturity and RES warm-up):**

- **`COINBASE_MATURITY` is 100 blocks** (consensus constant in `src/consensus/consensus.h`). Coinbase ORU outputs are not spendable until they have **100 confirmations**, so they cannot fund valid convergence transactions before that depth is reached on-chain.
- **Logic dependency:** Because convergence entries require mature ORU inputs, the RES draw **effectively has a ~100-block warm-up** on a new chain: there may be no valid convergence candidates (and thus **no RES mints**) even though `ComputeResReward` runs every height.
- **Code implication:** Window-based retarget (§5) may see **zero** RES mints in the rolling window during the same warm-up; `GetNextResTarget` then increases the stored target (bounded by per-update clamp). This is independent of `COINBASE_MATURITY` but the **absence of valid convergence transactions** has the same practical effect: no mints to count.
- **Test implication:** Scripts that estimate RES mint frequency (e.g. `test_rng_distribution.sh`) should **exclude block heights `≤ COINBASE_MATURITY` from ratio statistics**, or **expect zero RES** in that range on a fresh chain.

---

## 1. `min_convergence` is hard consensus

- Any transaction that includes a convergence output **must** satisfy `convergence_amount >= Consensus::MIN_FORGE_AMOUNT` (currently **1.00 ORU**).
- Violations are rejected at transaction validation (`convergence-amount-too-small`, etc.).

## 2. convergence draw digest and inequality (minimal core)

Let `H` be the height of the block under validation and `pindex_prev` its parent.

- **`bind_hash`** = `Consensus::convergenceDrawBindHashFromParent(pindex_prev, H)` — hash of the block at height **max(0, H − 10)** (10-block bind depth).
- **`target_prev`** = parent header **`nBits`** (compact PoW target of the block *before* the one being validated), as a **32-bit unsigned** value in the digest (see `draw_digest.cpp`).
- **Digest** `h = DrawDigest(txid, bind_hash, height_u32le, target_prev_u32le)` (256-bit, interpreted as little-endian integer where comparisons require it).
- **`T`** = `arith_uint256` from **`SetCompact(target_prev)`** (same compact bits as PoW parent target). Draw uses this **`T`**, not a separate per-index `res_draw_target` field.
- **`convergence_amount`** = ORU locked in the **single** positive **witness v1 CVG2** convergence output for that transaction.

**Consensus rule (integer, no floats on the hot path):** with `W = convergenceAmountBetaWeight(convergence_amount)` (β = 7/5 via `IntegerRoot5` / fixed scaling in `rigor_math.h` (header-inlined integer β ladder)),
## 4. RES minting (body self-proof)

- The block must contain **at least two** convergence transactions (`CountconvergenceTransactions(block) ≥ 2`) for a convergence winner to exist; coupling rules in `ConnectBlock` require **exactly one** valid RES-mint transaction in the body when there is a winner.
- New RES is issued only on **non-coinbase** txs validated by `ConnectEvaluateResMintTx` + `Consensus::VerifyResProof` (`rigor_res.cpp`, `res_proof.cpp`).
- **convergenceRoot membership (mandatory):** `proof.convergence_txid` MUST appear in the same commitment set **F** used for `convergenceRoot` (`Consensus::ListconvergenceRootCommitmentTxids` against **block-entry** UTXO). If not, the block is invalid (`bad-rigor-convergence-not-in-convergenceroot-set`).
- **convergenceRoot value binding (mandatory, single source):** The authoritative convergenceRoot bytes are **only** those in `coinbase.scriptSig` (`ParseCoinbaseconvergenceRootScriptSig`). `ConnectBlock` builds **F** once via `ListconvergenceRootCommitmentTxids` and passes that vector into RES handling, which **must not** call `ListconvergenceRootCommitmentTxids` again. On RES mint txs, **`ComputeconvergenceRootFromEligibilityTxids(F) == committed_root`** (coinbase) is enforced; mismatch → `bad-rigor-convergence-root-mismatch`. Together with `proof.convergence_txid ∈ F`, PCORB is bound to the **coinbase commitment**, not to an independent “RES-local” reconstruction of **F**.
- **Script-path mint `vin[0].scriptWitness`** must contain **nine** stack items (first push → last push): **convergence_txid (32)** ‖ **convergence_amount_sat (8 LE)** ‖ **bind_hash (32)** ‖ **height (8 LE)** ‖ **target_prev (4 LE)** ‖ **Ed25519 sig (64)** ‖ **Ed25519 pubkey (32)** ‖ **leaf script** ‖ **script-path control block**. The interpreter enforces the **PCORB** leaf hash and control structure (`script/interpreter.cpp`); `ParseResProof` consumes the first five pushes plus sig/pubkey for digest verification.
- **RES supply cap:** the hard cap constant is **`Consensus::RES_MAX_SUPPLY`** in `consensus/rigor_consensus_lock.h` (single name; also re-exported at file scope as `RES_MAX_SUPPLY` via `res_amount.h`). Whether cumulative issuance is enforced on-chain is independent of the constant definition; see validation / index code paths.

### 4.1 PCORB dual semantics (Mint mode vs Claim mode)

The same **PCORB-shaped** witness stack (`ParseRngProof` in `res_proof.cpp`) is a **unified cryptographic structure** with **two consensus meanings**. The meaning is determined **solely** by the **input asset domain** from `Consensus::GetTransactionInputAsset` (all prevouts must agree; mixed domains are invalid with `mixed-asset-output` / cross-pool rules in `CheckTxInputs`):
| Input domain | Mode | Body RES issuance (`res_mint_scores`) |
|--------------|------|----------------------------------------|
| **ORU** | **Mint** — draw-bound body mint | Yes, when winner / convergenceRoot / proof checks pass; amount is enforced in `ConnectBlock`. |
| **RES** | **Claim** — spend authorization (e.g. wallet `createresclaim`) | **No** — never scored, never counted as body mint. |

**Normative:** witness layout, script-path vs key-path, or output templates **must not** override this table. `ParseResProof` reads witness bytes only to validate **Mint mode** after the domain is already **ORU**; it does **not** select Mint vs Claim.

## 5. convergence window / cadence metadata on the block index

- **`has_res`** (0/1): this block included a successful body RES mint (convergence-cycle marker).
- **`last_res_mint_block_hash`**: hash of the latest ancestor block (including this block) with `has_res == 1`, used with **`m_cumulative_convergence`** to define the **ORU-domain convergence window** since the last RES mint (`ConnectBlock` / RPC `getconvergencestatus` “current convergence sum”). This is **not** the draw `T` and does not use a remaining-supply scalar in the inequality.

---

**Block index on-disk:** `CBlockIndex` stores **`m_block_res_mint_sum`** (RES issued by body mint txs in **this** block only; index/RPC aid). Changing on-disk index serialization requires **`-reindex`** or a fresh datadir when upgrading across incompatible layouts.

## References (code)

| Topic | Location |
|--------|----------|
| `min_convergence` | `Consensus::GetMinconvergenceAmount`, `tx_verify.cpp` |
| Inequality + winner | `convergence_logic.cpp`, `convergence_logic.h` |
| Draw digest + T from parent `nBits` + bind | `draw_digest.{h,cpp}`, `convergence_logic.{h,cpp}` |
| RES max supply constant (single source) | `Consensus::RES_MAX_SUPPLY` in `consensus/rigor_consensus_lock.h` (`RES_MAX_SUPPLY` via `res_amount.h`) |
| Per-block body RES issued (index/RPC) | `CBlockIndex::m_block_res_mint_sum` in `chain.h` / `ConnectBlock` |
| Convergence window / last RES mint block | `CBlockIndex::has_res`, `last_res_mint_block_hash`, `m_cumulative_convergence` |
| Block connection / RES mint (body txs) | `validation.cpp` `ConnectBlock` + `rigor::ConnectEvaluateResMintTx` |
| Convergence structure / CVGE vs recomputed ConvergenceDrawCandidates root | `tx_verify.cpp` (`invalid-convergence-structure`); `ContextualCheckBlock` / `ConnectBlock` (`bad-convergence-draw-candidate-root`, `bad-convergence-cumulative`, `bad-convergence-missing`) |
| RIGOR draw-target coinbase commitment (compact parent `nBits`) + RES body mints | `validation.cpp` `rigor::CheckRigorDrawTargetCommitment` (`bad-rigor-commitment-missing`, `bad-rigor-target-mismatch`); `rigor_res.cpp` / `res_proof.cpp` (`bad-rigor-res-*`) |
| Fee asset bucketing (non-coinbase) | `ConnectBlock` / miner template (`GetTransactionInputAsset`; `bad-txns-fee-domain`) |
| Coinbase maturity (convergence input availability) | `COINBASE_MATURITY` in `consensus.h` (100 blocks) |
| Devtest DB / crash / reorg / UTXO hash harness (`FORGE_DB_TEST_SUITE_V1`) | `test_convergence_db_suite_v1.sh` (repo root); UTXO: `FORGE_DB_UTXO_ONLY=1` or `FORGE_DB_INCLUDE_UTXO=1` |
| Weak draw / CVGE RPC test (≥2 convergence txs per block, no RES mint assertion) | `test/functional/feature_rigor_convergence_rng_consensus.py` |
| RES mint witness proof / conservation | `test/res_orum_tests.cpp`, `test/rigor_witness_tx_tests.cpp` |
| ConvergenceRoot (sorted convergence-set flat `Hash256` + coinbase `scriptSig`) | `convergence_root.{h,cpp}`, `validation.cpp` `ConnectBlock` (`bad-cb-convergence-root`); pool wiring: [mining-pool-gbt.md](./mining-pool-gbt.md) §2.0 |
