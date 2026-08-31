# RIGOR Single-Path CONVERGE — Post-Audit Security Hardening

**Task:** `TASK-RIGOR-CONVERGE-SINGLE-PATH-HARDENING-P0-011`
**Date:** 2026-08-24
**Prior audit:** `TASK-RIGOR-CONVERGE-SINGLE-PATH-IMPLEMENTATION-SECURITY-AUDIT-P0-010`
**`equivalence_claimed`:** `false`

**Consensus status:**

```
mainnet_launched              = false
historical_consensus_burden   = none
legacy_compatibility_constraint = none
```

---

## 1. Code changes

| File | Purpose | Security impact |
|------|---------|-----------------|
| `core/src/chain.h` | Persist `m_paid_winner_txid` on `CBlockIndex` / `CDiskBlockIndex`. `GetChainConvergeSum` is O(1) via observational prefix. | **H-1:** window assembly no longer recurses to genesis. **H-7:** IBD no longer Θ(H²) on a non-consensus walk. |
| `core/src/txdb.cpp` | Load the new index field. | Required for H-1 after restart. |
| `core/src/validation.cpp` | Replace `InferMintPaidTxid` with `ValidatePaidWinnerRecord`. Write the field in `ConnectBlock`. Template path still derives the parent winner (block does not exist yet) and is bounded. | Consensus **predicate unchanged**. Derivation path is now O(W). Corrupt/stale records fail closed (`-reindex`). |
| `core/src/node/blockstorage.cpp` | Null the field on header insert. | Prevents a leftover value on a never-connected index. |
| `core/src/consensus/observational_metrics.{h,cpp}` | Prefix recurrence uses parent observational cache. Docs no longer claim a CVGE coinbase payload. | Availability only. Not a consensus predicate. |
| `core/src/node/miner.cpp` | Strip leftover CVGE by magic `CVGE`, not `FRGE`. | Dead mining-path foot-gun; no consensus change. |
| `core/src/node/miner.h` | Comment: helper rebuilds BIP141 witness commitment, not C. | Documentation. |
| `core/src/script/script.h`, `script/standard.h` | Architecture guard: UTXO is monetary truth; scripts classify; `OP_CONVERGE` is a ticket. | **H-2.** |
| `core/src/consensus/convergence_logic.h`, `convergence_commitment.h` | Remove live-CVG2 / `bad-convergence-cumulative` claims. | **H-2.** |
| `core/src/rpc/blockchain.cpp` | Comment: RPC `convergence_type=v1_cvg2` is a historical spelling. | **H-2.** JSON key unchanged (wire-stable). |
| `core/src/test/paid_winner_index_tests.cpp` | Roundtrip, fail-closed truncate, empty-chain null record, bounded draw-set read. | Locks H-1. |
| `core/src/Makefile.test.include` | Register the new test. | — |
| `explorer/src/db.ts` | Mint classification is the coinbase in `res_mints` only. | Display no longer asserts a body mint path. |
| `core/doc/mining-pool-gbt.md` | SOLO-native search space; windowed C; no Stratum extranonce in `scriptSig`. | **H-3.** |
| `core/doc/rigor_consensus_pipeline.md` | Rewrite to the shipped single path. | **H-2.** |
| `core/doc/PROTOCOL-NAMING-TRUTH-SOURCE.md` | Move retired CVGE reject codes to historical. | **H-2.** |

Consensus logic (eligibility, σ layout, mint gate, F membership) was **not** redesigned.

---

## 2. Security report

### Fixed

| ID | Sev | Fix |
|----|-----|-----|
| V-1 / H-1 | **HIGH** | Paid-winner txid stored on the block index. `GetBlockWindowContribution` is a leaf: one block + one undo, no parent-draw recursion. `ValidatePaidWinnerRecord` cross-checks the record against `CoinbaseHasResMint`. |
| V-2 / H-2 | **MEDIUM** | Inverted issuance / CVG2 / CVGE comments and pipeline docs aligned with coinbase-only `OP_RES_MINT`. |
| V-5 / H-7 | **MEDIUM** | Observational prefix is O(1). `GetChainConvergeSum` no longer walks to genesis. |
| V-7 | **LOW** | Mining stripper matches `CVGE`, not `FRGE`. |
| Explorer mint mislabel | **LOW** | Body RES transfers are no longer tagged as the mint. |

### Remaining (accepted or out of this task's required order)

| ID | Sev | Status |
|----|-----|--------|
| V-10 | INFO | Priced re-roll of C_h / the next draw. One full block solution. Design, not a bug. |
| H-5 | — | `CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1`. `W = 32` defeats **minority** exclusivity, not majority-miner window censorship. Economic parameter; unchanged. |
| H-4 | LOW/MED | `res_proof.cpp` / `res_mint_carrier.cpp` still compile into the consensus library, tests-only. Not deleted here: no live caller, and deletion is a separate mechanical pass. |
| V-3 | LOW | Block-local root API still exists under the old name. Tests/benches use it with an empty-F assert. |
| V-6, V-8, V-9 | LOW | Duplicate root hasher; no duplicate-txid assert in F; implicit domain separation by preimage length. |
| V-11 | INFO | Body `OP_RES_MINT` locks are allowed as RES-domain outputs (not issuance). Indexers must not infer minting from script shape. |
| Coq ↔ C++ | OPEN | `bridge_cpp_equivalence_obligation` and siblings remain undischarged `Prop`s. Byte-level C/σ and window exclusion are modeled; end-to-end refinement is **not proven**. |

Pre-mainnet operational note: a datadir written **before** this field cannot be loaded. `CDiskBlockIndex` appends the 32-byte txid last; a short record throws instead of reading a null winner. Recovery is **`-reindex`**. There is no compatibility decoder and none is wanted.

---

## 3. Consensus impact analysis

| Question | Answer |
|----------|--------|
| Did issuance rules change? | **No.** Coinbase `OP_RES_MINT` only; body `mixed-asset-output`; winner coupling unchanged. |
| Did transaction format change? | **No.** |
| Did F / C / σ change? | **No.** Same window, same hash, same 512-bit inequality. |
| Did replay remain deterministic? | **Yes.** The recorded txid is a memo of a value `ConnectBlock` already computed. `ValidatePaidWinnerRecord` refuses a record that disagrees with the block, so a corrupt index cannot shift F. |
| Did reorg behavior change? | **No.** Window walks `pprev` of the source block. A disconnected index keeps its record (that block's coinbase still paid that ticket) but is not in the active walk. Reconnect overwrites the field. |
| Is this global consensus state? | **No.** Per-block memo of a locally derived fact. Mint authorization still reads block + undo + params only. |
| Pruning | A pruned node now needs only **W** blocks of block+undo data, not the chain to genesis. `AbortNode` remains if those W files are missing — fail-closed, bounded. |

`state_hash` was **not** extended with `m_paid_winner_txid`. That field is explicitly non-consensus and must not become a replay identifier.

---

## 4. Formal verification impact

| Layer | Impact |
|-------|--------|
| Coq | **None.** Window exclusion (`WindowExclusion.v`) already assumes a paid-txid list. C++ now *stores* that list instead of re-deriving it. Definitions of F, C, σ, and mint coupling are unchanged. |
| Python reference / golden vectors | **None.** Vectors cover digest and selection, not block-index layout. |
| Claim honesty | Byte-level C and σ: **modeled and tested**. Mint coupling: **modeled**. C++ refinement: **not proven** (`equivalence_claimed = false`). H-1 itself is **reviewed + unit-tested**, not a Coq theorem. |

No new axiom. No public statement that this hardening is formally proven.

---

## 5. Mining impact

| Item | Result |
|------|--------|
| Template C | Unchanged: `GetTemplateConvergenceRoot` over the window, written into `scriptSig` after the mint is appended. |
| Template paid-txid | Still derived from the parent draw (the template has no index row). That derivation now bottoms out at recorded txids, so it is O(W), not O(H). |
| SOLO | `nNonce` is the native grind. `nTime` / `nVersion` optional. Documented in `core/doc/mining-pool-gbt.md`. |
| `scriptSig` | Byte-exact `<height> <C> <paid-winner-txid>`. Stratum extranonce there is consensus-invalid. **No compatibility layer added.** Corrected from a two-push description by P0-014. |
| Witness reserved value | Node writes 32 zero bytes. Changing it requires regenerating the witness commitment; SOLO does not need it. |
| `default_res_mint` | Unchanged; still mandatory copy when the parent draw has a winner and W(h) > 0. |

Miner interface: GBT fields are the same. Integrators must copy `convergence_root` and `default_res_mint`. Fail-fast remains `bad-cb-convergence-root` / `bad-rigor-res-*`.

---

## 6. Final recommendation

**READY FOR MAINNET REVIEW**

The HIGH availability defect is closed without touching a consensus rule. Inverted issuance documentation that could have reintroduced a second mint path is corrected. The mining surface is documented as SOLO-native with a locked `scriptSig`.

This is **not** a mainnet-go. Remaining work for a later pass, none of it a consensus hole:

1. Delete or quarantine retired `res_proof` / `res_mint_carrier` (H-4).
2. Rename the block-local root API (V-3).
3. Decide `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` in the open (H-5).
4. Keep public FV wording scoped to what is actually proved.

A node with a pre-H-1 block index must `-reindex` once. That is expected before mainnet.

**Tests run:** `paid_winner_index_tests` (4/4), plus `miner_tests`, `state_snapshot_tests`, `runtime_supply_state_tests`, `convergence_root_tests` — no errors. Coq/Python/golden vectors were not regenerated; they do not encode block-index layout.
