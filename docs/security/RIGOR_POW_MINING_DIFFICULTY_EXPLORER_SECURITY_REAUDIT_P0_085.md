# RIGOR PoW / Mining / Difficulty / GBT / ConnectBlock / Explorer Security Re-Audit (P0-085)

**Task:** `P0-085 UPDATE` — incremental re-audit of an in-progress P0-085.  
**Date:** 2026-09-02  
**Nature:** Audit-first. This document does **not** modify the Consensus Kernel, Transaction Kernel, Ledger, PoW algorithm, ASERT parameters, Genesis, issuance, supply, or draw. Release Guardian was inspected only; it was not edited.

This is an increment on the existing node PoW-boundary work (`docs/security/RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md`) and the already-landed GBT / ConnectBlock golden vectors (`rigor_gbt_layout_vectors.json`, `rigor_connectblock_vectors.json`, `rigor_connectblock_gbt_vector_tests`, `test_p0_016_vector_lock.py`). Those artifacts are now treated as **CONSENSUS_BOUNDARY_GOLDEN_VECTORS**, not ordinary unit tests.

```
POW_SECURITY_REAUDIT=BLOCKED
RIGOR_POW_MINING_DIFFICULTY_EXPLORER_SECURITY_REAUDIT_CLOSED   = NOT ISSUED

GBT_LAYOUT_GOLDEN_VECTOR=PASS
CONNECTBLOCK_GOLDEN_VECTOR=PASS
CONNECTBLOCK_INVALID_MERKLE_REJECTION=PASS

POW_HASH_AUTHORITY_COUNT=1
POW_TARGET_AUTHORITY_COUNT=1
POW_ACCEPTANCE_AUTHORITY_COUNT=1
MINING_HASH_AUTHORITY_COUNT=2
GBT_AUTHORITY_COUNT=1
CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT=1
POW_AUTHORITY_DUPLICATION=FAIL

MINER_NODE_POW_DIFFERENTIAL=FAIL
DIFFICULTY_CONSISTENCY=PASS
GBT_POW_CONSISTENCY=PASS
STRATUM_POW_CONSISTENCY=FAIL
SOLO_MINER_POW_CONSISTENCY=FAIL
CHAINWORK_CONSISTENCY=PASS
EXPLORER_POW_SEMANTICS=FAIL

LEGACY_POW_PRODUCTION_REACHABLE=TRUE

FUZZ_STATUS=PASS
CRASH_FOUND=NO
UNDEFINED_BEHAVIOR=NO

FORMAL_POW_CLAIMED=FALSE
POW_MATHEMATICAL_SECURITY_PROVED=FALSE
LIVE_MINT_GOLDEN_VECTOR=NOT_COVERED
REORGBIND_GOLDEN_VECTOR=NOT_COVERED

GOLDEN_VECTOR_SCOPE = GBT_LAYOUT + CONNECTBLOCK + SERIALIZATION + REJECTION_BOUNDARY
RELEASE_GUARDIAN_POW_COVERAGE=PARTIAL
```

The node still accepts or rejects work through a single production path:

```
GetRigorPowHash  →  CheckProofOfWork  →  ContextualCheckBlockHeader / ProcessNewBlock
```

The shipped solo miner and pool grind **OpuHash**. That is not a consensus bypass (the node will not accept an OpuHash-only mainnet header), but it is a second production hash. That is why this re-audit is **BLOCKED** and is **not** closed.

---

## 1. Golden vectors are consensus-boundary locks

The existing JSON files are the freeze of:

```
GBT layout
  → coinbase
  → witness commitment
  → merkle root
  → block header
  → ProcessNewBlock
  → ConnectBlock
  → accept / reject
```

Same fixture, same input, same byte result. The C++ suite replays production functions (`BlockAssembler::CreateNewBlock`, `GetWitnessCommitmentIndex`, `GenerateCoinbaseCommitment` / `vchCoinbaseCommitment`, `GetResMintScript`, `BlockMerkleRoot`, `DecodeHexBlk`, `ChainstateManager::ProcessNewBlock`, `CheckProofOfWork`, `GetRigorPowHash`).

Verified 2026-09-02:

```
./test/test_rigor --run_test=rigor_connectblock_gbt_vector_tests
→ 5 cases, no errors

python3 formal_verification/tests/test_p0_016_vector_lock.py
→ P0-016 Python vector lock: PASS
```

Cases:

| Case | Role |
|------|------|
| `gbt_layout_vectors_p0_016` | Live `CreateNewBlock` vs frozen GBT fields |
| `connectblock_valid_height1_hex` | Frozen hex accepted; tip height 1 |
| `connectblock_invalid_stale_merkle_hex` | Extra tx, stale merkle → reject; height stays 0 |
| `connectblock_consensus_mutations_p0_085` | Seven consensus-field mutations |
| `pow_hash_not_legacy_opuhash_p0_085` | `GetRigorPowHash(valid) != GetOpusHash(valid)` |

What these vectors **do not** prove:

- live mint (height-1 empty mempool; genesis has no parent draw winner)
- full mint lifecycle
- ReorgBind
- PoW mathematical security
- miner ↔ node hash equivalence

Mint coverage remains the byte encoding of `GetResMintScript(2, pkh)` only:

```
0102148c874780fd3a764b3c3f31b8936f0eb2aaa8a0a6bb
```

---

## 2. Field provenance (production function, not a second encoder)

### `rigor_gbt_layout_vectors.json` — `gbt_height1_empty_mempool`

| Field | Frozen value | Production source |
|-------|----------------|-------------------|
| `height` | `1` | `pindexPrev->nHeight + 1` after genesis |
| `coinbasevalue` | `100000000000` | `CreateNewBlock` coinbase `vout[0].nValue` (subsidy; empty mempool, no ORU fees) |
| `witness_commitment_index` | `1` | `GetWitnessCommitmentIndex` |
| `default_witness_commitment` | `6a24aa21a9ed…8cf9` | `BlockAssembler` / `GenerateCoinbaseCommitment` (`tmpl->vchCoinbaseCommitment`) |
| `mint_present` | `false` | `IsResMintScript` scan of coinbase vouts — **no live mint** |
| `mint_scriptPubKey` | `""` | same scan |
| `canonical_mint_script` | `010214…a6bb` | `GetResMintScript(2, pkh)` encoding only |
| `canonical_mint_maturity_height` | `2` | regtest maturity = height 1 + `nCoinbaseMaturity=1` |
| `payout_scriptPubKey` | `0014384e00…a196` | `GetScriptForDestination(WitnessV0KeyHash)` of frozen key `00…01` |
| `coinbase_scriptSig` | `5120` + 32 zero + `20` + 32 zero | live `CreateNewBlock` coinbase `vin[0].scriptSig` (height `OP_1` + paid-winner / C slots) |
| `merkle_root` | `5611125c…1446` | `BlockMerkleRoot` of the template (coinbase + witness commitment, no mempool txs) |
| `prevhash` | `3070f47e…af63` | `Params().GenesisBlock()` identity hash |
| `bits` | `207fffff` | `pblock->nBits` from `GetNextWorkRequired` (regtest `fPowNoRetargeting=true` → genesis compact) |
| `nTime` | `1778224815` | mock clock = genesis time + 100 |

GBT RPC (`rpc/mining.cpp`) exposes the same compact as `bits` and the expanded `SetCompact(pblock->nBits)` as `target`. That compact is the compact `CheckProofOfWork` later consumes after `ContextualCheckBlockHeader` requires `block.nBits == GetNextWorkRequired(...)`.

### `rigor_connectblock_vectors.json`

| Row | Production path |
|-----|-----------------|
| `valid_height1_coinbase_only` | Same `CreateNewBlock` fixture, merkle set, nonce searched with **`CheckProofOfWork` / `GetRigorPowHash`**, hex via `CDataStream << block`. `DecodeHexBlk` + `ProcessNewBlock` accepts. Tip height 1. |
| `invalid_stale_merkle_extra_tx` | Extra dummy tx appended **without** updating `hashMerkleRoot`. `ProcessNewBlock` rejects (`bad-txnmrklroot`). Height stays 0. |

Serializer order, witness-commitment index, coinbase value, mint-script encoding, merkle root, and 80-byte header layout are therefore frozen against drift.

---

## 3. ConnectBlock rejection boundary

Existing invalid vector is now classified:

```
CONNECTBLOCK_INVALID_MERKLE_REJECTION=PASS
```

`ProcessNewBlock` rejects; chain height remains 0.

Additional mutations (`connectblock_consensus_mutations_p0_085`) start from the **valid golden hex** (not a second harness):

| Mutation | What changes | Observed |
|----------|--------------|----------|
| merkle root | XOR first merkle byte | reject, height 0 |
| coinbase | `vout[0].nValue += 1` | reject, height 0 |
| tx ordering | vtx `[cb, tx1, tx2]` with merkle of `[cb, tx2, tx1]` | reject, height 0 |
| witness commitment | last commitment script byte XOR 1 | reject, height 0 |
| nBits | `nBits = 0` (fail-closed compact) | reject, height 0 |
| header timestamp | `nTime += 1` | hex, identity hash, and `GetRigorPowHash` all change |
| nonce | `nNonce += 1` | hex, identity hash, and `GetRigorPowHash` all change |

Timestamp and nonce produce a **different block**. On easy regtest they may still satisfy `CheckProofOfWork`. The suite therefore requires reject only when the mutated header fails PoW. That is not acceptance of the **original** golden block. A consensus-affecting change cannot keep the frozen hex.

---

## 4. PoW unique authority (node)

| Question | Answer |
|----------|--------|
| Digest | `rigor_kernel::GetRigorPowHash` = SHA3-256(`"RGRPOW01" \|\| header_80`) via `BuildPowPreimage` (`core/src/rigor_kernel/pow_digest.cpp`) |
| Accept | `CheckProofOfWork` (`core/src/pow.cpp`) calls `GetRigorPowHash` only. Genesis is exempt by **identity** `block.GetHash() == hashGenesisBlock`. |
| Target next-bits | `GetNextWorkRequired` → `GetNextWorkASERT`. `CalculateNextWorkRequired` is legacy Bitcoin-style tooling and is **not** called from `validation.cpp` or GBT. |
| Header check | `ContextualCheckBlockHeader`: `nBits == GetNextWorkRequired`, then `CheckProofOfWork` (`validation.cpp` ~4724–4735). |
| Compact fail-closed | negative / overflow / zero / above `powLimit` → false (`pow.cpp` ~410–414). Hash > target → false. |

`tools/lint-pow-boundary-authority.py` (2026-09-02):

```
POW_AUTHORITY_COUNT = 1
HASH_AUTHORITY_COUNT = 1
ASERT_AUTHORITY_COUNT = 1
LEGACY_POW_PATH = NONE
OPUSHASH_CONSENSUS_CALLERS = NONE
PASS: unique PoW boundary authorities
```

That lint is **node-scoped** (`core/src`). It correctly reports that `CheckProofOfWork` does not call `GetOpusHash`, and that `node/miner.cpp` / wallet / RPC do not define a second accept function. It does **not** scan `miner/` or `pool/`.

`core/src/pow_opus.cpp` still exists. Production validation does not call it. Stale comments in `validation.cpp` still say “OpuHash uses the header id only” (documentation P2).

```
POW_HASH_AUTHORITY_COUNT=1          (node digest)
POW_TARGET_AUTHORITY_COUNT=1       (GetNextWorkRequired)
POW_ACCEPTANCE_AUTHORITY_COUNT=1   (CheckProofOfWork)
GBT_AUTHORITY_COUNT=1              (CreateNewBlock / getblocktemplate)
CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT=1  (ProcessNewBlock)
MINING_HASH_AUTHORITY_COUNT=2      (node SHA3 generate vs miner/pool OpuHash)
POW_AUTHORITY_DUPLICATION=FAIL
```

Legacy OpuHash **cannot** reach production `CheckProofOfWork`, `ProcessNewBlock`, or GBT target computation. It **can** reach production nonce search in `miner/` and pool share/block checks.

```
LEGACY_POW_PRODUCTION_REACHABLE=TRUE
```

---

## 5. GBT → miner → node consistency

**GBT → node (PASS)**

- `BlockAssembler::CreateNewBlock` sets `pblock->nBits = GetNextWorkRequired(...)`.
- GBT RPC publishes `bits` and `target = SetCompact(pblock->nBits)`.
- `ContextualCheckBlockHeader` requires the submitted header’s `nBits` equal `GetNextWorkRequired`.
- `CheckProofOfWork` uses that same compact.

So:

```
GBT target  =  Node expected nBits  =  CheckProofOfWork compact
```

GBT header construction uses the same `CBlockHeader` field order as `BuildPowPreimage` / `CDataStream` (version, prev, merkle, time, bits, nonce — 80 bytes). Node PoW input bytes are those 80 bytes. **Byte identity GBT ↔ node holds.**

**GBT → external miner → node (FAIL)**

The solo miner copies header bytes, then grinds `opus_hash` (`miner/sha2.c` `scanhash_sha256d`). The node hashes `"RGRPOW01" \|\| header_80` with SHA3-256. Same bytes, different digest.

`pow_hash_not_legacy_opuhash_p0_085` asserts `GetRigorPowHash(valid) != GetOpusHash(valid)` on the golden height-1 header.

Internal RPC `generate` / `GenerateBlock` loops `CheckProofOfWork` (SHA3). That path is consistent. The shipped `minerd` is not.

Regtest `powLimit` is easy (`207fffff`). Both digests often meet target, so functional GBT copy-bytes tests can hide the mismatch. Mainnet OpuHash-mined headers fail `high-hash`.

```
GBT_POW_CONSISTENCY=PASS
MINER_NODE_POW_DIFFERENTIAL=FAIL
```

---

## 6. Solo mining

| Component | Hash used for nonce search |
|-----------|----------------------------|
| `rpc/mining.cpp` `GenerateBlock` | `CheckProofOfWork` → `GetRigorPowHash` |
| `miner/cpu-miner.c` default `ALGO_SHA256D` | `scanhash_sha256d` → `opus_hash` |
| `miner/sha2.c` | `opus_hash(wire, digest)` then compare to share/network target |

Historical OpuHash is **not** confined to legacy / test / documentation. It is the production solo-miner grind.

The miner does not decide consensus validity. `submitblock` → `DecodeHexBlk` → `ProcessNewBlock` → `CheckProofOfWork`. Node remains the accept authority.

```
SOLO_MINER_POW_CONSISTENCY=FAIL
```

---

## 7. Stratum / pool mining

Pool share validation (`pool/share-validator/src/opuhash.rs`, `validator.rs`) and the pool miner agent (`pool/rigor-miner-agent/src/pow/worker.rs`) grind and check **OpuHash**.

Share target is correctly **not** the consensus target:

- `pool/stratum-server/src/difficulty.rs` — `share_target = diff1 / difficulty`
- session tests assert share target ≠ network target on mainnet-hard bits
- vardiff comments document the earlier bug of `share_target == network target`

The pool may compute shares, count miners, and build jobs. It is **not** a PoW consensus authority: network acceptance remains `ProcessNewBlock`.

`verify_opuhash_block` can still mark a share as “block found” against the **network compact** using OpuHash. The node will reject that header on mainnet. That is share/network confusion of **hash function**, not of target numeric width.

```
STRATUM_POW_CONSISTENCY=FAIL
```

---

## 8. Difficulty / target

`CheckProofOfWork` fail-closed:

- negative compact → reject
- overflow compact → reject
- zero target → reject
- target above `powLimit` → reject
- `GetRigorPowHash` > target → reject

Covered by `pow_tests.cpp` (`CheckProofOfWork_test_*`) and `test_pow_boundary_security.cpp`.

GBT / node / `CheckProofOfWork` share one compact authority (`GetNextWorkRequired`).

```
DIFFICULTY_CONSISTENCY=PASS
```

---

## 9. ASERT

Production next-bits (when retargeting is enabled) is `GetNextWorkASERT`:

| Parameter | Mainnet lock |
|-----------|----------------|
| Anchor | `ASERT_ANCHOR_HEIGHT = 1` (`rigor_consensus_lock.h`) |
| Half-life | `MAINNET_POW_ASERT_HALF_LIFE = 43200` (12 hours) |
| Spacing | `nPowTargetSpacing = 600` seconds |
| Child timestamp | ignored (`(void)pblock`) |
| Exponent | fixed-point `time_error * 65536 / half_life`, clamped `ASERT_EXP_CLAMP` |
| Per-block clamp | ±12.5% (`MIN/MAX_ADJUSTMENT_FACTOR_PPM`) |
| Zero next-target | forced to 1, then `powLimit` clamp |
| Activation | always-on via `GetNextWorkRequired`; no height switch |
| Mainnet min-difficulty | `fPowAllowMinDifficultyBlocks = false` |
| Mainnet no-retarget | `fPowNoRetargeting = false` |

Regtest used by the golden vectors: `fPowNoRetargeting = true`, so height-1 `bits` stay at genesis `207fffff`. That is why the GBT vector locks genesis compact, not an ASERT step.

Extreme timestamps cannot emit a protocol-external target: exponent clamp + `powLimit` + ±12.5% `ClampTargetToPrev`. Differential / unit coverage: `pow_tests.cpp`, `rigor_asert_vectors.json` (heights 1/2, `nBits=1d008000`), fuzz `core/src/test/fuzz/rigor_asert.cpp`.

This is **not** a Coq proof of ASERT.

---

## 10. Explorer / browser

Explorer does **not** reimplement SHA3 PoW or ASERT.

| Semantic | Node | Explorer |
|----------|------|----------|
| `BLOCK_IDENTITY_HASH` | `CBlockHeader::GetHash` (SHA256d) | block hash from RPC |
| `POW_HASH` | `GetRigorPowHash` as RPC `pow_hash` | importer reads `block.opus_hash` — **RPC does not emit `opus_hash`** |
| `TARGET` / bits | header `bits` | stored `bits` from RPC |
| `DIFFICULTY` | `GetDifficulty` via `getblockchaininfo` / header JSON | `blockchainInfo.difficulty` — **NODE_AUTHORITY_VALUE**, not a frontend recalc of the digest |
| `CHAINWORK` | `CBlockIndex::nChainWork` / RPC `chainwork` | **not in explorer schema** |

`explorer/src/api/status.ts` still labels difficulty `"ASERT / OpuHash (node-reported)"`. UI shows `bits / opus_hash`. Indexed PoW hash is therefore often null, and the label names the retired digest.

Five-way semantic split is **not** cleanly presented.

```
EXPLORER_POW_SEMANTICS=FAIL
```

Difficulty display from RPC remains a node-authority number. The failure is naming and field mapping, not a second PoW implementation.

---

## 11. PoW → chainwork

`GetBlockProof` (`chain.cpp`) expands the **stored header `nBits`** (same compact `CheckProofOfWork` accepted). Negative / overflow / zero → work 0.

Best-chain selection uses `nChainWork` accumulated from those proofs (`node/blockstorage.cpp`). It does not hash OpuHash and does not reuse a retired difficulty function.

A header that is PoW-valid under `GetRigorPowHash` contributes work from **that header’s accepted compact**. A PoW-invalid header never becomes a `CBlockIndex` tip candidate.

```
CHAINWORK_CONSISTENCY=PASS
```

---

## 12. Golden vector vs PoW

```
GOLDEN_VECTOR_SCOPE = GBT_LAYOUT + CONNECTBLOCK + SERIALIZATION + REJECTION_BOUNDARY
```

Locked: layout, coinbase / witness / merkle / header bytes, ProcessNewBlock accept of the valid hex, reject of stale-merkle and the listed mutations, and `GetRigorPowHash != GetOpusHash` on that header.

**Not** a PoW mathematical proof.  
**Not** a full mining-equivalence proof.  
**Not** live mint.  
**Not** ReorgBind.

Golden Vector ≠ Formal Proof.

---

## 13. Formal verification boundary

| Surface | Classification |
|---------|----------------|
| PoW digest model (`PowDigest.v`) | **NOT_COVERED** as C++ refinement (`pow_refinement_proved = false`). Model exists; `equivalence_claimed = false`. |
| `CheckProofOfWork` accept-iff-le (`pow_boundary_accept_iff_le`) | Surface theorem registered. **NOT** a SHA3/C++ equivalence proof. Do not call this a PoW math proof. |
| ASERT (`AuthASERT`) | Registered authority, **NOT_COVERED** as proved refinement. **DIFFERENTIAL_TESTED** + **FUZZ_TESTED** + ASERT JSON **GOLDEN_VECTOR_LOCKED**. |
| ConnectBlock | `connectblock_full_proof = false`. Boundary registered. Height-1 hex **GOLDEN_VECTOR_LOCKED**. **Not PoW.** |
| GBT | Outside Kernel (`CoinbaseLayoutBridge.v`). **GOLDEN_VECTOR_LOCKED** for height-1 empty mempool. **NOT_COVERED** formally. |
| Chainwork / `GetBlockProof` | Abstract replay only. **NOT_COVERED** as equivalence. **DIFFERENTIAL_TESTED** via existing pow/chain tests + fuzz `core/src/test/fuzz/pow.cpp`. |
| Miner / Stratum OpuHash | **NOT_COVERED**. Production mismatch is outside Coq. |

```
FORMAL_POW_CLAIMED=FALSE
POW_MATHEMATICAL_SECURITY_PROVED=FALSE
```

---

## 14. Fuzz

Existing fuzz retained: `core/src/test/fuzz/pow.cpp`, `rigor_pow_target.cpp`, `rigor_asert.cpp`, `rigor_opuhash_header.cpp` (legacy digest), `miner/fuzz_gbt_solo.c`, `fuzz/pow_boundary/`.

Fresh smoke 2026-09-02:

```
bash fuzz/pow_boundary/run_smoke.sh
POW_FUZZ_STATUS = PASS
FUZZ_STATUS=PASS
CRASH_FOUND=NO
UNDEFINED_BEHAVIOR=NO
ACCEPT_INVALID=NO
```

That smoke mutates header / nBits (negative, zero, oversize) and checks the isolated digest/target harness. It is not a libFuzzer campaign and is **not** run by Release Guardian Stage 6.

```
FUZZ_STATUS=PASS
CRASH_FOUND=NO
UNDEFINED_BEHAVIOR=NO
```

---

## 15. Authority lock

Node accept path:

```
POW_HASH_AUTHORITY_COUNT=1
POW_TARGET_AUTHORITY_COUNT=1
POW_ACCEPTANCE_AUTHORITY_COUNT=1
GBT_AUTHORITY_COUNT=1
CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT=1
```

Second **production** hash implementation exists in `miner/` and `pool/`:

```
POW_AUTHORITY_DUPLICATION=FAIL
MINING_HASH_AUTHORITY_COUNT=2
```

---

## 16. Release Guardian (not modified)

`.github/workflows/genesis-release-guardian.yml` only runs `tools/run-genesis-release-guardian.sh`.

| Topic | In Guardian? |
|-------|----------------|
| PoW digest / `CheckProofOfWork` uniqueness | Partial — Stage 5 `lint-consensus-surface-closure.py` names `GetRigorPowHash` / `CheckProofOfWork`. Dedicated `lint-pow-boundary-authority.py` is **not** a Guardian stage. |
| Difficulty / ASERT | Not a dedicated stage. Inherited only if surface lints mention next-work. |
| GBT / ConnectBlock golden replay | **Missing** |
| Solo / pool mining hash | **Missing** |
| Chainwork | **Missing** |
| Stage 6 fuzz | transaction / predicate / UTXO / crypto / p2p smokes. **`fuzz/pow_boundary` is not in Stage 6.** |

```
RELEASE_GUARDIAN_POW_COVERAGE=PARTIAL
```

Recorded only. Guardian was not changed.

---

## 17. Findings (no consensus repair in this TASK)

### P0

None. An OpuHash-valid, SHA3-invalid header is rejected by `CheckProofOfWork` (`high-hash`). The node is not a dual-hash acceptor.

### P1

**P1-1 — Production solo miner grinds OpuHash**

| | |
|--|--|
| Location | `miner/sha2.c` `scanhash_sha256d` → `opus_hash`; `miner/cpu-miner.c` default `ALGO_SHA256D` |
| Production reachability | TRUE — shipped `minerd` nonce search |
| Security impact | Mainnet blocks from this miner fail node PoW. Mining liveness / wasted work. **Not** a consensus bypass. On easy regtest both hashes can meet target, which hides the break in some functional tests. |
| Recommended remediation | Point nonce search at SHA3-256(`"RGRPOW01" \|\| header_80`) matching `GetRigorPowHash`. Leave node `CheckProofOfWork` unchanged. Confine `opus_hash` to legacy / test / docs. |

**P1-2 — Production pool / Stratum grind and “block found” on OpuHash**

| | |
|--|--|
| Location | `pool/share-validator/src/opuhash.rs`, `validator.rs` (`verify_opuhash_share` / `verify_opuhash_block`); `pool/rigor-miner-agent/src/pow/worker.rs` |
| Production reachability | TRUE — pool share path and miner-agent scanner |
| Security impact | Share accounting uses the retired digest. `verify_opuhash_block` can declare a network block against OpuHash; `submitblock` then fails on the node. Pool is still not the consensus authority. Share-target ≠ consensus-target **numeric** split is implemented. |
| Recommended remediation | Share and block-found checks must use `GetRigorPowHash` (or a byte-identical SHA3 helper). Keep share target easier than, and distinct from, the consensus compact. |

### P2

| ID | Location | Impact | Remediation |
|----|----------|--------|-------------|
| P2-1 | `explorer/src/api/status.ts` `difficulty_label` | UI names OpuHash | Label as ASERT / node-reported difficulty; do not name OpuHash |
| P2-2 | `explorer/src/importer.ts` `block.opus_hash`; schema / UI | RPC field is `pow_hash`; indexed digest often null | Map `pow_hash`; do not recompute |
| P2-3 | `validation.cpp` comment at PoW check | Stale “OpuHash” wording | Comment-only |
| P2-4 | Release Guardian Stage 6 | `pow_boundary` smoke not gated | Add later; do not edit Guardian in this TASK |
| P2-5 | `CalculateNextWorkRequired` still in tree | Tooling only; not validation | Keep unused on the production path |

No consensus, ASERT, Genesis, or issuance change is recommended from this audit’s P1s — only miner/pool/explorer alignment to the existing node authority.

---

## 18. Coverage honesty

```
LIVE_MINT_GOLDEN_VECTOR=NOT_COVERED
REORGBIND_GOLDEN_VECTOR=NOT_COVERED
POW_MATHEMATICAL_SECURITY_PROVED=FALSE
FORMAL_POW_CLAIMED=FALSE
```

Height-1 empty mempool has no live mint because genesis has no parent draw winner. Do not cite these vectors as a live-mint or full mint-lifecycle proof.

---

## 19. Verdict

Covered golden-vector and node-boundary items that this increment set out to lock are **PASS**.

The mining ecosystem still has a second production hash. Required authority counts for mining therefore fail.

```
POW_SECURITY_REAUDIT=BLOCKED
```

`RIGOR_POW_MINING_DIFFICULTY_EXPLORER_SECURITY_REAUDIT_CLOSED` is **not** issued.

Reproduce:

```
./test/test_rigor --run_test=rigor_connectblock_gbt_vector_tests
python3 formal_verification/tests/test_p0_016_vector_lock.py
python3 tools/lint-pow-boundary-authority.py
bash fuzz/pow_boundary/run_smoke.sh
```
