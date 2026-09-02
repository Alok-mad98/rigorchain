# RIGOR Test Vector / Golden Vector / Regression Validation Audit (P0-016)

Task: `TASK-RIGOR-TEST-VECTOR-GOLDEN-VECTOR-REGRESSION-VALIDATION-AUDIT-P0-016`  
Date: 2026-09-02  
Type: **Audit (morning) + P0/P1-ASERT implementation (same day).**  
Question: is the current test corpus sufficient to freeze v0.1 consensus so that later code changes cannot silently alter transaction validity, signature meaning, asset ownership, mining reward, or convergence behavior?

```
equivalence_claimed                    = false   (full-node C++ = Coq = Python)
hash_kernel_shared_binary_with_cpp     = false
connectblock_full_not_proved           = true
ed25519_libsodium_not_refined          = true
```

Honesty note: later Kernel-boundary modules set `refinement_proved = true` for a *named boundary* (issuance, classify, draw-on-F, ledger apply on the modeled domain). That is **not** a claim that C++ accept/reject equals the Coq model on every serialized transaction or block. This audit treats a Consensus Golden Vector as a byte-locked, multi-impl, accept/reject fixture. Kernel-boundary theorems do not substitute for that lock.

---

## 0. Freeze verdict

**P0 Consensus Golden Vectors and P1 ConnectBlock/GBT layout are now locked. Do not stamp a full v0.1 Consensus Freeze yet** — ReorgBind remains a scaffold, Coq does not replay libsodium/tx hex/block hex, and height-1 GBT has no live mint (parent draw empty).

| Freeze invariant | JSON golden (byte-locked) | C++ live replay | Python harness | Coq | Rust | Freeze? |
|------------------|---------------------------|-----------------|----------------|-----|------|---------|
| Transaction validity | **`rigor_native_tx_vectors.json`** (10 rows) + **`rigor_connectblock_vectors.json`** (valid height-1 hex / stale-merkle reject) | tx-layer + `ProcessNewBlock` | Hex stability lock | Kernel boundary only | None | **Yes (tx + height-1 block)** |
| Signature meaning | Digest (3) + **11 mutation hashes** + **3 Ed25519 sig64** | `rigor_sighash_vector_tests` (4 cases) | P0-016 + P0-024 locks | Preimage lemmas | None | **Yes (digest + sig64)** |
| Asset ownership | Strong (16 + 11 SASS) + explorer JSON replay | Classify + native tx | Strong | Strong | None | **Yes (classifier path unique)** |
| Mining reward | Schedule pins + **ASERT nBits JSON** + **GBT layout JSON** (coinbasevalue, witness index, mint encoding) | `asert_golden_vectors_p0_016` + `gbt_layout_vectors_p0_016` | Pin lock | Schedule / cap | Pool only | **Yes (height-1 template; mint omitted until a parent winner)** |
| Convergence behavior | Strong (phase3 lock) + **CONVERGE spend-reject hex** | CheckTxInputs reject | Strong | Stratified | Winner-engine | **Yes (draw + unspendable UTXO)** |

**P0 gaps closed:** Ed25519 sig64; sighash field-mutation `expected_hash`; native RIGOR tx hex (Bitcoin `tx_valid`/`tx_invalid` stay skipped); CONVERGE spend-reject; canonical mint coinbase + `OP_5` mint tx reject; explorer mint encoding rejects `OP_1`–`OP_16` and replays P0-024 JSON.

**P1 closed:** height-1 serialized block through production `ProcessNewBlock`; stale-merkle extra-tx reject; GBT `coinbasevalue` / witness commitment index / `GetResMintScript` encoding. Height-1 empty-mempool templates have `mint_present=false` because genesis has no parent draw winner.

**Still open:** ReorgBind executable hex (`frozen_consensus_vectors.json` spend IDs remain labels); Coq replay of sig64 / native tx / block hex; GBT row with a live `default_res_mint` after a winning CONVERGE parent.

---

## 1. Current coverage map

### 1.1 Authority locks (already exist)

| Corpus | Count | What it actually locks | Layers |
|--------|-------|------------------------|--------|
| `formal_verification/golden_lock.json` | **433** shared / **226** Coq lemmas | Selection, σ, roots, negative mint/cvg/script, ScriptAsset classifier, EdgeCross | Python + C++; Coq subset |
| `formal_verification/vectors/rigor_classification_vectors.json` | **16** | `scriptPubKey → AssetType` (incl. `OP_5` mint reject) | C++ + Python + Coq `VectorAgreement.v` |
| `formal_verification/vectors/rigor_sighash_vectors.json` | **3** | ORU / RES / RES_MINT preimage + SHA256d | C++ live `SignatureHash` + Python harness + Coq hash |
| `core/src/test/data/rigor_script_asset_classifier.json` | **11** (`SASS-000`…`010`) | Asset + `IsAllowedScript` + `IsResScript` | C++ + phase3 + Coq |
| `formal_verification/frozen_consensus_vectors.json` | Labels only | Spend `valid` / `invalid` / `bad_signature` with **no hex** | Governance pin, not a golden tx |

Phase3 family breakdown (lock counts):

| Family | Vectors | Coq-enforced | Role vs this task |
|--------|---------|--------------|-------------------|
| RunSelection | 84 | 7 | Convergence winner |
| WindowExclusion | 59 | 59 | Already-paid filter |
| SigmaPoW01 | 50 | 5 | σ preimage |
| DrawCandidateRoot | 36 | 36 | Eligible root |
| Inequality | 25 | 25 | H·T vs f |
| ConvergenceRoot | 25 | 9 | C = Hash256(F) |
| EdgeCross | 20 | 20 | Cross-family reject |
| NegativeCore | 16 | 16 | Dual-pool / mint / cap |
| InvalidCvg | 14 | 14 | Bad CONVERGE shape |
| ReorgBind / BlockTrace | 12+12 | 0 | Scaffold, not ConnectBlock |
| ScriptMintNeg | 12 | 12 | Claimed-asset vs script |
| ScriptAsset | 11 | 11 | Classifier |
| HashKernel / Sha256Fips | 29+8 | 2+0 | Hash, not tx |
| SupplyScaffold / Replay / WindowIncomplete | 5+3+2 | 0 | Python/C++ only |

### 1.2 Consensus Golden Vectors (scope item 1)

| Required case | Status | Evidence |
|---------------|--------|----------|
| Valid transaction | **Unit, not golden hex** | `rigor_witness_tx_tests` (ORU v0, RES v1, mint spend round-trip). `transaction_tests::tx_valid` **SKIPPED** (“legacy JSON hex may need regeneration”). |
| Invalid transaction | **Unit, not golden hex** | Dual-pool rejects, P2PKH/P2SH/P2WSH/v2, mixed assets, RES without ORU fee. `tx_invalid` **SKIPPED**. |
| Signature failure | **Unit** | Wrong sig, secp256k1 pubkey, 65-byte hashtype suffix, cross-network checker. No JSON `{seed, msg, sig64, expect:false}`. |
| Asset classification | **Locked** | 16 P0-024 rows + 11 SASS + nongate first-byte ∀ tails. |
| RES mint | **Classify + monetary locked; mint tx hex not** | `NEG-*`, `SMW-*`, `GetResMintReward` pins, `mint_supply_reorg_p0_036_tests`. No serialized mint-coinbase golden. |
| CONVERGE | **Classify + unspendable unit** | Classify → ORU. `script_tests` / opcode security reject spend. No JSON spend-reject vector. |
| Block validation | **Kernel-boundary, not block hex** | `test_connectblock_kernel_boundary.cpp` pins issuance/classify/draw-on-empty. Coq: `connectblock_full_not_proved`. No `{block_hex, expect}`. |

`core/src/test/data/tx_valid.json` and `tx_invalid.json` are **Bitcoin CHECKMULTISIG / DER** fixtures. They are not RIGOR consensus goldens. Leaving them skipped is honest; treating them as freeze coverage is not.

### 1.3 Signature Golden Vectors (scope item 2)

Required invariant:

```
same message + same private key  =>  same Ed25519 signature
any committed field change       =>  signature (via digest) MUST change
```

**Digest path (locked).** C++ `SignatureHash` preimage is:

```
tag || chain_id || nVersion || hashPrevouts || hashSequence
|| hashOutputs || this prevout || spent scriptPubKey || amount
|| nSequence || nLockTime
```

Tags: `RGRORU01` / `RGRRES01` / `RGRMNT01`. Three golden preimages + digests replay in C++, Python `python/rigor_sighash.py` (harness, not consensus), and Coq `sighash_hash_vectors_agree`.

**Field sensitivity (C++ unit only, not JSON).** `sighash_tests.cpp::sighash_field_sensitivity` mutates and asserts digest inequality for:

| Field | Covered as C++ `!=` | Locked expected hash in JSON |
|-------|---------------------|------------------------------|
| amount | Yes | **No** |
| spent scriptPubKey | Yes | **No** |
| asset / SigVersion tag | Yes (`sighash_pairwise_domain_tags`) | Only the 3 happy-path rows |
| chain_id | Yes (`sighash_cross_network_replay`) | **No** (all goldens use `0x11…11`) |
| nVersion, prevout, nSequence, output value, output script, nLockTime | Yes | **No** |

**Ed25519 bytes (not locked).** `key_tests.cpp::ed25519_sign_verify_closure` signs twice and checks `sig == sig_again`. `ed25519_boundary_tests` checks small-order / S+L reject. **No RFC8032 golden 64-byte signature is stored.** A libsodium behavior change, a non-deterministic signer, or a domain-prefix bug that still hashes the same way would not fail P0-024 JSON.

CONVERGE has no sighash golden (correct: unspendable). The missing lock is the **negative**: a CONVERGE spend must fail with a stable reason, as hex.

### 1.4 Script-as-Asset Vectors (scope item 3)

Required path:

```
scriptPubKey  →  AssetType  →  OwnershipPredicate
```

**C++ consensus: single matcher.** `ClassifyRigorTxout` in `rigor_classify.cpp`. `AssetTypeFromScriptPubKey` / `ClassifyRigorAsset` / `Solver` wrap it. Lint: `tools/lint-consensus-kernel-single-authority.py`. Wallet send uses `WalletStrictSendDestinationOk` → derive(script), not witness version (`script_asset_ownership_freeze_tests`). P0-019 `WitnessRFCBindingOk` is gone from C++.

| scriptPubKey | AssetType | Ownership | Golden? |
|--------------|-----------|-----------|---------|
| v0 / 20 | ORU | Ed25519 P2WPKH | Yes |
| v1 / 32 native | RES | Ed25519 key-path (`program = pk32`) | Yes |
| `RES_MINT` canonical | RES (not a fifth asset) | Mint spend + maturity | Yes |
| `OP_CONVERGE` | ORU | None (unspendable) | Classify yes; spend-reject JSON no |
| `OP_RETURN` | NONE | Unspendable | Yes |
| else | INVALID | Not creatable | Yes (empty, P2WSH, P2PKH, CVG2, RES2, v1/20, `OP_5` mint) |

**Second classification path still exists outside consensus.** Explorer `isCanonicalNonNegIntPush` accepts `OP_1`…`OP_16` as mint height. C++ / Python / P0-024 JSON reject `OP_5 \|\| PUSH20 \|\| pkh \|\| RES_MINT` as `INVALID` because `GetResMintScript(5)` emits `PUSHBYTES_1 0x05`. Explorer would label that script **RES**. Indexer `importer.ts` persists `asset_kind` from that table. This is not a bitcoind fork, but it **is** a second registry — forbidden by the freeze text “no second classification path.”

Rust consensus does not implement `ClassifyRigorTxout`. Pool crates classify jobs, not UTXO scripts.

### 1.5 Mining / consensus regression (scope item 4)

| Surface | Status | Notes |
|---------|--------|-------|
| Block template / GBT | Functional + miner C | `mining_solo_gbt_copy_bytes.py`, `mining_basic.py`, `miner/test_gbt_solo.c`. No JSON template golden consumed by C++ / Coq / Python together. |
| Coinbase | Layout + kernel | `CoinbaseLayoutBridge.v`, `convergence_root_tests` coinbase scriptSig. Not a serialized coinbase tx golden. |
| RES reward | Strong pins | `GetResMintReward(0)=40_000_000`, `(209999)=40_000_000`, `(210000)=14_142_196`. Coq `reward_pin_table`. Full `k=0..251` via parity, not 252 JSON rows. |
| Convergence winner | Strong | phase3 RunSelection + `selection_golden_tests` + `convergence_consensus_tests` (tie-break, window, already-paid). Rust `pool/winner-engine/tests/golden_vectors.rs` replays the same JSON. |
| Difficulty / ASERT | C++ strong | `pow_tests.cpp` (`asert_mainnet_reference_vectors`, reorg parent recomputation). Not in phase3 golden lock. |
| Reorg | Unit + functional; Coq scaffold | `w32_reorg_stability_tests`, `mint_supply_reorg_p0_036_tests`, `feature_dual_pool_reorg.py`, `feature_rigor_reorg_winner_lock.py`, `feature_res_window_reorg_res_attacks.py`. `ReorgBind` is `coq_enforced: 0`. |

### 1.6 Formal model synchronization (scope item 5)

Required: no “model accepts / C++ rejects” or the reverse, on the freeze surface.

| Layer | Role | Sync with C++ today |
|-------|------|---------------------|
| C++ | Production authority | Unique Kernel + `VerifyRigorPredicate` |
| Python `rigor_consensus_spec.py` | Spec / L1 oracle | Classify + schedule + draw families. Sighash harness is **not** consensus. |
| Rust | Pool winner-engine | ConvergenceRoot / Sigma / Inequality / RunSelection only. **No** tx, sighash, classify, ConnectBlock. |
| Coq | Model + finite harness | Classify/sighash JSON; 226 golden lemmas; Kernel-boundary refinement. Ed25519 is a model predicate, not libsodium. SHA256d not a shared binary with `CSHA256`. |

Documented, acceptable splits (do not “fix” by unfolding): see `docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md`. SHA-heavy selection rows are Python/C++ only by cost.

**Unacceptable for freeze:** a production accept/reject that has no byte vector and no Coq boolean. That is the current state of native valid/invalid transactions and ConnectBlock.

P0-023 freeze certificate: `PARTIAL_READY`, G-1…G-4 still open (C++ classify ∀, predicate ∀, SHA256d, Ed25519). P0-074 later claimed Kernel-boundary `REFINEMENT_PROVED = TRUE` while keeping `EQUIVALENCE_PROVED = FALSE` and `connectblock_full_not_proved`. **Cite the boundary, not the filename.**

---

## 2. Missing Golden Vectors

IDs are recommendations. Do not add them in this task.

| ID | Vector | Why it is missing | Freeze invariant |
|----|--------|-------------------|------------------|
| GV-SIG-01 | RFC8032: `seed \|\| 32-byte msg → sig64` (ORU/RES/MINT sighash messages) | Sign-twice tests are not a golden | Signature meaning |
| GV-SIG-02 | Same as GV-SIG-01 with one-bit message / key / sig mutation → reject | Unit only | Signature failure |
| GV-SH-01 | Mutated **amount** → locked digest (not merely `!=`) | Only C++ inequality | Signature meaning |
| GV-SH-02 | Mutated **spent scriptPubKey** → locked digest | Same | Signature meaning |
| GV-SH-03 | Mutated **chain_id** → locked digest | Goldens all use `0x11…11` | Cross-network replay |
| GV-SH-04 | Mutated **output script / value / nVersion / nLockTime / prevout / nSequence** → locked digests | Same | Any field change |
| GV-TX-01 | Native RIGOR `tx_valid` hex (ORU spend, RES spend, dual-pool fee) | Bitcoin JSON skipped | Transaction validity |
| GV-TX-02 | Native RIGOR `tx_invalid` hex (bad sig, P2PKH, P2WSH, mixed assets, RES without ORU fee, duplicate vin) | Skipped | Transaction validity |
| GV-CVG-01 | CONVERGE **spend** serialized tx → reject reason | Classify-only | Ownership |
| GV-MINT-01 | Coinbase + canonical `RES_MINT` (maturity 100, reward = W(h)) accept hex | Schedule pinned, tx not | Mining reward |
| GV-MINT-02 | `OP_5` mint encoding in a tx → `ASSET_INVALID` / not creatable | Classify JSON exists; tx-level does not | No second encoding |
| GV-BLK-01 | One valid block hex: coinbase + optional mint + CONVERGE set + merkle + nBits | Kernel tests are not blocks | Block validation |
| GV-BLK-02 | Invalid blocks: wrong subsidy, wrong W(h), spend CONVERGE, wrong C | Same | Block validation |
| GV-DIFF-01 | ASERT reference rows in the golden lock (height, parent time, expected nBits) | C++ only | Mining |
| GV-XPL-01 | Explorer must fail closed on P0-024 corpus including `res_mint_op_5_rejected` | Explorer accepts OP_n mint height | Second classifier |

---

## 3. High-risk test gaps

| ID | Risk | If frozen as-is |
|----|------|-----------------|
| R-1 | **Critical** — no native tx golden hex | A CheckTransaction / CheckTxInputs / witness change can ship while “golden vectors still pass” (they never saw a RIGOR tx). |
| R-2 | **Critical** — Ed25519 signature bytes unlocked | Digest lock survives a signer that emits a different 64-byte encoding still accepted by a looser verifier, or a non-deterministic sign. Freeze of “same key + same message = same signature” is a claim without a vector. |
| R-3 | **High** — field sensitivity not JSON | Someone can drop a field from `SignatureHash` and “fix” the three happy-path goldens together. Mutation hashes would catch that. |
| R-4 | **High** — ConnectBlock without block hex | Reward, winner, and convergence can be locally correct and still assemble a different block. |
| R-5 | **High** — explorer OP_5 mint = RES | Operators treat explorer `asset_kind` as truth. Second classification path after freeze. |
| R-6 | **High** — four-impl split | Rust is not a consensus impl. Python sighash is a harness. Coq does not run libsodium. A green Coq lemma is not a C++ accept. |
| R-7 | **Medium** — skipped Bitcoin `tx_valid` still in tree | Future contributors re-enable it and silently reintroduce Script-VM / CHECKMULTISIG expectations. |
| R-8 | **Medium** — `frozen_consensus_vectors.json` placeholder spend IDs | Looks like freeze coverage; is not. |
| R-9 | **Medium** — ReorgBind / BlockTrace `coq_enforced: 0` | Window walk / undo off-by-one can agree with every SHA vector (already documented residual in GOLDEN-VECTOR-DISCREPANCIES.md). |

No in-tree evidence of “Coq accepts a live C++ reject” on the **locked classify/sighash/selection** rows. The danger is the **unlocked** surface, not a known byte mismatch on locked rows.

---

## 4. Recommended new tests

### P0 — implemented 2026-09-02 (see §9)

1. **Ed25519 signature goldens (GV-SIG-01/02).** `rigor_ed25519_signature_vectors.json` — seed, pubkey, message = P0-024 digest, sig64, reject mutant. C++ `SignDetached` / `VerifyDetached` + Python libsodium ctypes.
2. **Sighash mutation JSON (GV-SH).** `rigor_sighash_mutation_vectors.json` — 11 ORU field mutations with `expected_hash` and `parent_hash`.
3. **Native RIGOR tx goldens (GV-TX).** `rigor_native_tx_vectors.json`. Bitcoin `tx_valid.json` / `tx_invalid.json` remain skipped (legacy CHECKMULTISIG).
4. **CONVERGE spend-reject (GV-CVG-01).** `converge_spend_rejected` → `bad-txns-spend-convergence`.
5. **Mint coinbase accept + `OP_5` mint reject (GV-MINT).** `canonical_mint_coinbase_accepted` / `op5_mint_output_rejected`.
6. **Explorer consumes P0-024 classification JSON** so `OP_5` mint is INVALID and `PUSHBYTES_1 0x05` mint is RES.

### P1 — ASERT / ConnectBlock / GBT implemented 2026-09-02; ReorgBind still open

7. Serialized block goldens (GV-BLK-01/02). **Done:** `rigor_connectblock_vectors.json` — valid height-1 coinbase-only hex through `ChainstateManager::ProcessNewBlock`; invalid extra-tx with stale merkle (`bad-txnmrklroot` path).
8. ASERT rows (GV-DIFF-01). **Done:** `rigor_asert_vectors.json` (`nBits=1d008000`, genesis time `1786125600`).
9. GBT coinbase layout golden. **Done:** `rigor_gbt_layout_vectors.json` — `coinbasevalue=100000000000`, `witness_commitment_index=1`, `mint_present=false`, canonical `GetResMintScript(2, pkh)` bytes.
10. Promote one ReorgBind row family from scaffold to an executable C++ undo/view pair with hex txids. **Open.**

### P2 — hardening, not freeze-blocking

11. Do not force every RunSelection SHA row into Coq. Keep the documented split.
12. Do not add 252 JSON rows for W(k); keep `reward_pin_table` + C++/Python parity.
13. Rust: either explicitly out of consensus freeze, or add classify/sighash replay. Do not imply a four-way impl lock that does not exist.

---

## 5. v0.1 Freeze must-pass set

A freeze gate is **not** “`test_rigor` is green.” It is this named set, plus the P0 goldens above once they exist.

### 5.1 Must pass today (existing) — classification / ownership

- `core/src/test/test_rigor --run_test=rigor_classification_vector_tests`
- `core/src/test/test_rigor --run_test=script_asset_ownership_freeze_tests`
- `python3 formal_verification/tests/test_p0_024_vector_lock.py`
- `python3 formal_verification/tests/classify_triple_diff.py` (if in the FV gate)
- Coq `VectorAgreement.v` (`classification_vectors_agree`)
- `tools/lint-consensus-kernel-single-authority.py`

### 5.2 Must pass today — signatures / spend

- `rigor_sighash_vector_tests`
- `sighash_tests` (field_sensitivity, domain tags, cross-network, P0-018 goldens)
- `rigor_witness_tx_tests`
- `ed25519_consensus_determinism_tests`
- `ed25519_boundary_tests`
- `key_tests` (`ed25519_sign_verify_closure`)
- Coq `sighash_hash_vectors_agree` / `sighash_tag_vectors_agree`

### 5.3 Must pass today — mint / reward / dual-pool

- `res_proof_math_tests`
- `mint_supply_reorg_p0_036_tests`
- `issuance_height_boundary_p0_041_tests`
- `genesis_zero_issuance_p0_037_tests` / `alpha1_genesis_tests`
- `transaction_tests` native dual-pool cases (not the skipped `tx_valid`)
- Kernel pins: `test_consensus_kernel_freeze`, `test_connectblock_kernel_boundary`, `test_transaction_kernel_refinement`

### 5.4 Must pass today — convergence / reorg

- `selection_golden_tests`
- `convergence_consensus_tests`
- `convergence_root_tests`
- `w32_reorg_stability_tests`
- `python3 formal_verification/scripts/check_golden_lock.py`
- Rust `pool/winner-engine` `golden_vectors.rs`
- Functional: `feature_res_core_convergence_draw.py`, `feature_res_core_convergence_window.py`, `feature_res_window_reorg_res_attacks.py`, `feature_rigor_reorg_winner_lock.py`, `feature_dual_pool_reorg.py`

### 5.5 Must pass today — mining / PoW

- `pow_tests`
- `rigor_pow_digest_tests`
- `test_pow_boundary_security`
- Functional: `mining_solo_gbt_copy_bytes.py`, `mining_pow_pipeline_p0_042.py`, `mining_prev_nbits.py`
- `miner/test_gbt_solo`

### 5.6 Must not be cited as freeze evidence

- Skipped `tx_valid` / `tx_invalid` / `basic_transaction_tests`
- Placeholder spend IDs in `frozen_consensus_vectors.json`
- Coq `refinement_proved = true` on a Kernel boundary, as if it were tx/block equivalence
- Pool Rust tests, as if they were `CheckTransaction`

### 5.7 Freeze gate rule (when P0 goldens exist)

```
C++ accept  =  Python spec accept  =  Coq boolean
on every Consensus Golden Vector hex
AND
Ed25519(sig, seed, msg) bytes match JSON
AND
explorer.classify(script) = AssetTypeFromScriptPubKey(script)
on the classification corpus
```

P0 of that gate now holds for tx / signature / classifier (C++ live functions + Python harness + explorer JSON). Coq still pins classify/sighash **digests**, not libsodium sig64 and not native tx hex. ConnectBlock remains Kernel-boundary.

Correct public sentence after P0:

> Selection, classifier, sighash digests **and** Ed25519 sig64, native RIGOR tx accept/reject hex, CONVERGE unspendability, explorer mint encoding, height-1 ConnectBlock hex, and GBT layout are golden-locked. ReorgBind hex, Coq sig64/tx/block replay, and a GBT row with a live parent-draw mint are not.

---

## 6. Execution priority

| Priority | Action | Owner surface | Blocks freeze? | Status 2026-09-02 |
|----------|--------|---------------|----------------|-------------------|
| P0 | Native tx valid/invalid hex + keep Bitcoin JSON skipped | `transaction_tests` | Tx-layer freeze | **Done** |
| P0 | Ed25519 signature byte goldens | `ed25519_*` + JSON | Yes | **Done** |
| P0 | Sighash mutation JSON | `rigor_sighash_mutation_vectors.json` | Yes | **Done** |
| P0 | CONVERGE spend-reject + mint tx hex | witness / mint tests | Yes | **Done** |
| P0 | Explorer consumes classification JSON (`OP_5` = INVALID) | `explorer/src/script.ts` | Path uniqueness | **Done** |
| P1 | Block hex goldens through ConnectBlock | validation | Yes for “block validation covered” | **Done** (height-1) |
| P1 | ASERT JSON | pow | Partial mining freeze | **Done** |
| P1 | GBT layout JSON | mining | Partial mining freeze | **Done** (no live mint) |
| P2 | Rust classify/sighash or explicit non-authority | pool | No, if documented | Open |
| — | Do not densify SHA selection into Coq for freeze | FV | No | Unchanged |

---

## 7. What this audit is not

- Not a request to regenerate `golden_lock.json`.
- Not a claim that current locked rows mismatch across C++ / Python / Coq.
- Not a VST / libsodium proof.
- Not permission to re-enable Bitcoin `tx_valid.json` as RIGOR consensus.

---

## 8. Conclusion (audit morning)

The repository already contained the **right architecture** for a Consensus Golden Vector system: a living JSON lock, C++ replay of live functions, a Python spec, Coq finite harnesses, and (for draw only) a Rust replay.

The morning audit found that architecture **without** the vectors that freeze v0.1 tx/signature/explorer. Same-day P0 closed the transaction layer; P1 closed height-1 ConnectBlock hex and GBT layout. Remaining honesty limits: ReorgBind scaffold, Coq not replaying libsodium/tx/block hex, height-1 GBT has no live mint.

---

## 9. Implementation evidence (2026-09-02)

SSOT JSON under `formal_verification/vectors/`, copied to `core/src/test/data/`:

| File | Rows | Replay |
|------|------|--------|
| `rigor_ed25519_signature_vectors.json` | 3 | C++ `ed25519_signature_vectors_p0_016` + Python `test_p0_016_vector_lock.py` |
| `rigor_sighash_mutation_vectors.json` | 11 | C++ `sighash_mutation_vectors_p0_016` + Python lock |
| `rigor_native_tx_vectors.json` | 10 | C++ `rigor_native_golden_vectors_p0_016` (`DecodeHexTx` / `CheckTransaction` / `CheckTxInputs` / `VerifyRigorPredicate`) |
| `rigor_asert_vectors.json` | 2 | C++ `asert_golden_vectors_p0_016` + Python pin |
| `rigor_gbt_layout_vectors.json` | 1 | C++ `gbt_layout_vectors_p0_016` (`CreateNewBlock`) |
| `rigor_connectblock_vectors.json` | 2 | C++ `ProcessNewBlock` accept + stale-merkle reject |

Explorer: `isCanonicalNonNegIntPush` accepts only `OP_0` or CScriptNum data-push. `OP_5` mint = INVALID. `GetResMintScript(5)` (`PUSHBYTES_1 0x05`) = RES. `script.test.ts` replays `rigor_classification_vectors.json`.

Generator (harness only, not consensus): `formal_verification/vectors/build_p0_016_vectors.py` with `python/rigor_hash160.py`, `python/rigor_ed25519.py`, `python/rigor_tx_wire.py`.

Fresh verification (this implementation):

```
python3 formal_verification/tests/test_p0_016_vector_lock.py   → P0-016 Python vector lock: PASS
python3 formal_verification/tests/test_p0_024_vector_lock.py   → P0-024 Python vector lock: PASS
cd explorer && npx tsc && node --test dist/script.test.js      → 12/12 pass
./src/test/test_rigor --run_test=rigor_sighash_vector_tests    → 4 cases, no errors
./src/test/test_rigor --run_test=transaction_tests/rigor_native_golden_vectors_p0_016
./src/test/test_rigor --run_test=pow_tests/asert_golden_vectors_p0_016
./src/test/test_rigor --run_test=rigor_classification_vector_tests → 3 cases, no errors
./src/test/test_rigor --run_test=rigor_connectblock_gbt_vector_tests → 3 cases, no errors
```

Compile note: `test_consensus_kernel_testnet_canary.cpp` was missing `#include <consensus/kernel_canary/canary_controller.h>` (mainnet canary already had it). One-line include so `test_rigor` links; no canary production logic change.

Out of scope (unchanged): Bitcoin `tx_valid.json` / `tx_invalid.json` stay skipped. `golden_lock.json` phase3 counts not bumped. Python sighash/tx/ed25519 modules remain harness-only. `refinement_proved` / full-node equivalence claims unchanged. ReorgBind hex and a live-mint GBT row were not added.
