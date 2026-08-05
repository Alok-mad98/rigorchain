# SECURITY-001-A / A-8 — σ grinding, and binding the draw to proof of work

**Status:** implemented (C++ / pool / formal / live RFCs); awaiting maintainer `make` + functional suite
**Scope:** consensus (σ preimage, draw timing, block connect, block template), RPC surface, mining
pool, formal spec, functional and unit tests, documentation
**Chain impact:** invalidating. Merges with the A-5/A-6/A-7 chain reset; no separate reset.
**Codename:** Sigma-PoW-01

---

## 1. What prompted this

Review asked whether σ could be bound to the current block's proof of work, observed that the
80-byte header cannot be extended to carry the answer, and proposed the parent block hash as the
practical substitute:

> σ = H(txid ‖ C ‖ prev_block_hash ‖ height) … advantage: no circular dependency. But the miner
> still chooses the transaction set, which moves C. C grinding remains.

That reading of the residual risk was correct, and the risk turned out to be the whole problem
rather than a leftover: **before this change the entire σ preimage was chosen, for free, by whoever
assembled the block that paid the draw.** Adding the parent hash without also moving the candidate
set would not have fixed it, because `txid` and `C` remain free variables at height h.

---

## 2. Finding A-8.1 (critical): the convergence draw was grindable at 13 ms per block

### 2.1 The defect

Under the previous rule, the block at height h drew over the convergence transactions **in its own
body**, and paid the winner with a RES mint **in that same block**. σ was

```
σ_i = Hash256( txid_i ‖ C ‖ bind_commit ‖ LE32(h) )
bind_commit = Hash256( B_{h-10} ‖ LE32(target_prev) )
```

Of these inputs, `B_{h-10}` and `target_prev` are fixed by history and `h` is fixed by position. The
remaining two are not:

| input | who chooses it | cost to change |
|---|---|---|
| `txid_i` | the producer, by choosing which convergences to include, and by authoring its own | free |
| `C` | the producer, implicitly — C is the root over the included set, so any change to the set moves it | free |

The producer therefore held a re-roll. Reshuffle the candidate set, recompute C, recompute every
σ, look at who wins; if it is not you, reshuffle again. Nothing about the attempt is published, so
failed attempts are invisible and unbounded, and the block itself only has to be mined once — after
the producer has already decided which draw outcome it wants.

### 2.2 Cost, measured

Eligibility is `σ_i < T · f_i` with `T = floor(2^240 / MIN_CONVERGENCE_AMOUNT)` and
`MIN_CONVERGENCE_AMOUNT = 1 ORU`. A candidate converging exactly the minimum passes with probability
`2⁻¹⁶`, so a self-dealing producer that includes only its own convergence needs about 65 536
re-rolls, each costing roughly two SHA-256d evaluations:

```
expected rolls for p = 2^-16:                     65 536
at 10 M SHA256d/s on one optimised core:          13.1 ms
```

Thirteen milliseconds of one core, per block, against a ten-minute block interval — a factor of
about 46 000 in slack. A producer converging past the saturation point (65 536 ORU) does not even
need the loop: it is eligible on the first roll, and only has to grind past any competitor.

The consequence is not faster RES issuance, which stays bound to the proof-of-work schedule. It is
that **a single party can take all of it**. RES supply is capped at 21 000 000 RES issued 100 RES at
a time, i.e. 210 000 mints; every one of them was available to whoever produced the block, for the
price of a rounding error in block assembly time. The lottery was not a lottery.

### 2.3 Why the obvious fixes do not work

**Include the block's own hash in σ.** Circular. The block hash commits to the merkle root, which
commits to the RES mint, whose witness names the winner, which is what σ decides. The proposal
identified this correctly.

**Add the parent hash, keep everything else.** This is the proposal as literally stated, and it
closes nothing. `txid` and `C` remain free at height h, so the re-roll survives intact at the same
13 ms. Adding a fixed input to a hash does not constrain the inputs that are still free.

**Commit to the candidate set earlier in the same block.** Any commitment the producer writes, the
producer can rewrite before publishing. A commitment only binds if it is already sealed by work.

### 2.4 Remedy: defer the draw by one block

The draw a block pays is resolved over the convergence set committed by its **parent**:

- Block `h-1` **commits** a convergence set `F` and a convergence root `C` over it. Both are sealed
  by its merkle root, and its header is sealed by its proof of work.
- Block `h` **pays** the winner of that set. Its coinbase CVGE output commits the eligible-subset
  root of the resolved draw, and its body carries the RES mint.

σ becomes

```
σ_i = Hash256( txid_i ‖ C_{h-1} ‖ B_{h-1} ‖ LE32(h) )
```

Every input other than the payout height now comes from block `h-1`: `txid_i` and `C` are committed
by its merkle root, and `B_{h-1}` is its hash. **Deviating on any of them costs a full proof-of-work
solution.** The re-roll went from 13 ms to one block.

The producer of block `h` has no input to σ at all. It cannot add a candidate, cannot remove one,
cannot move C, and cannot move the seed. `ResolveConvergenceDraw` takes only the draw set, with no
height and no seed parameter, precisely so that no caller can even express resolving a set against a
context it did not come from.

### 2.5 Why this is the elegant form rather than a patch

The 100-byte σ layout is unchanged. Slot `[64..95]` previously held `bind_commit`; it now holds
`B_{h-1}` verbatim. Three things fall out rather than being added:

- **`target_prev` leaves the preimage.** It was folded in to bind σ to the parent's difficulty. The
  parent block hash commits to the parent header, which contains its nBits, so the separate input
  was redundant. It remains a PCORB witness field, still checked against the parent header by
  `ConnectBlock`, but it no longer feeds σ.
- **The `k = 10` bind depth disappears.** There is no ancestor to look up; the seed is the parent.
- **The RFC-000 §5 bootstrap window disappears with it.** `B_{h-10}` is undefined for `h < 10`,
  which is why a frozen zero preimage existed for heights 1..9. The parent block hash is defined at
  every height from 1 upward, so the special case, its documentation and its tests are all deleted
  rather than migrated.

Net effect on the specification surface: one input removed, one constant removed, one bootstrap
special case removed, and the security property strengthened from "free" to "one PoW per attempt".

---

## 3. Finding A-8.2 (latent, medium): a permissive `VerifyConvergeProof` overload

Sweeping for the last stale definitions turned up one more, in `consensus/statetag.h`. It declared
an RFC-003 `MintWitness` struct — carrying a `bind_hash`, a `target_prev` and a 64-byte
`signature`, i.e. a layout that A-7 and A-8 have each independently falsified — together with

```cpp
bool VerifyConvergeProof(const CTransaction& tx, const MintWitness& witness);
```

whose implementation returned `true` for any witness with non-empty fields.

That name is not free. `Consensus::VerifyConvergeProof` is the real mint verifier, declared in
`consensus/res_proof.h` in the same namespace. The stub was therefore a third overload of it, and
overload resolution rather than the author's intent would decide which one a call site got. A
translation unit including both headers and calling it with a `MintWitness` would have received an
unconditional pass from something named exactly like the consensus check.

It was unreachable in the shipped tree: only `wallet/tx_semantic.cpp` includes `statetag.h`, no
translation unit includes both headers, and nothing constructs a `MintWitness`. This is therefore a
latent hazard rather than a live vulnerability, and it is reported as such. But it was one
`#include` away from being live, in a file whose other four functions *are* live, and the stub's own
comment invited a future implementer to wire it up.

**Remedy.** `MintWitness`, `SpendWitness` (likewise describing a spend witness A-7 replaced) and the
stub are removed. `statetag.h` now points the reader at `res_proof.h` as the sole definition of the
mint witness and records why. The four live classifiers — `GetInputAsset`, `IsRESMintTx`,
`IsRESSpendTx`, `IsORUTransferTx` — are untouched, and `wallet/tx_semantic.cpp` is unaffected.

Left in place deliberately, and flagged for review rather than removed: `StateOutput`,
`StateTagType`, the `LOCK_PROGRAM_*` constants, `IsValidStateTransition` and `VerifySelectionProof`
are also dead, but they are dead-and-accurate rather than dead-and-false, and several are locked by
`static_assert` under the RFC-013 nomenclature freeze. Removing them is a governance question, not a
security fix, so it is not bundled into A-8.

---

## 4. Consequences that had to be handled

### 4.1 Reading the parent's set during block connect

`ConnectBlock` at height `h` needs `F_{h-1}`, which is not in the block it is validating. It is
recovered by `GetConvergenceDrawSetForBlock`, which reads the source block plus its undo data and
rebuilds the set with `BuildConvergenceDrawSetFromUndo` — the undo data is required because the
convergence inputs have already been spent, and weights come from those inputs.

Blocks connect in order and every block already builds its own set for the coinbase commitment, so
the last one built is kept in a one-entry cache keyed by source block hash. The disk path is then
taken only on a cold start or immediately after a reorg. Keying by hash means a switch to a
different parent can never hit a stale entry; a miss costs a read, not correctness. Several
chainstates share the one slot and may evict each other, which likewise costs performance only.

A failure to read the parent is reported through `AbortNode`, not through `BlockValidationState`. A
local storage failure is not a statement about the chain, and must not mark a possibly-valid chain
invalid.

No storage format changed. This is a read of data already on disk in its existing layout.

### 4.2 The `BuildConvergenceDrawSet` / `…FromUndo` pair is now consensus-critical in both directions

The live-view builder runs while a block is being connected; the undo builder runs when a later
block reads it back. They must agree candidate for candidate or the chain forks. That agreement is
asserted directly in `convergence_consensus_tests.cpp` (`candidate_set_deterministic`), comparing
both the root and every `(txid, weight)` pair.

### 4.3 External miners could not see the draw they must pay

This is the one place where A-8 required a genuinely new interface rather than a change to an
existing one. A `getblocktemplate` client assembles block `h` and must include a RES mint settling a
draw over transactions that are **not in its template** — they are in the parent. Nothing in the GBT
response described them.

`getblocktemplate` now returns a `convergence_draw` object carrying the resolved parent draw: the
seed, the payout height, `C`, `target_prev`, the eligible-subset root the coinbase must commit, the
full candidate list with per-candidate σ and eligibility, and the winner with the 32-byte RES
program the mint must pay. Every field is a function of the parent alone, so serving it grants the
miner no discretion it did not already lack.

The existing `convergence_txids` field is unchanged in content but has changed in meaning: it is the
set this template **commits**, drawn by the next block, not the set this template pays. Both are now
documented as such, because reading one for the other is exactly the mistake A-8 makes possible.

### 4.4 A rebuilt block is a different draw, on purpose

`feature_rigor_reorg_winner_lock.py` previously asserted that rebuilding a block with the same
convergence transactions reproduced the same winner. Under A-8 that is false, and must be: the
rebuild has a different coinbase, therefore a different block hash, therefore a different seed. The
test now asserts both halves of the real property — the *same* block always awards the same draw
(disconnect and reconnect it, bit for bit), and a *different* block awards a different one.

That distinction is the security property stated operationally. Anyone who wants a different winner
must pay for another block.

---

## 5. Changes

### 5.1 Consensus

| File | Change |
|---|---|
| `consensus_rigor/sigma_hash.{h,cpp}` | Slot `[64..95]` is the parent block hash verbatim. `target_prev` parameter removed. Layout still 100 bytes with the same static asserts. |
| `consensus/convergence_logic.{h,cpp}` | New `ConvergenceDrawCandidate`, `ConvergenceDrawSet`, `ConvergenceDrawOutcome`. New `ResolveConvergenceDraw` (no height, no seed parameter), `BuildConvergenceDrawSet`, `BuildConvergenceDrawSetFromUndo`, `ConvergenceDrawEligibleRoot`. Removed the per-candidate predicates that took a caller-supplied bind and height. |
| `consensus/draw_digest.h`, `consensus/selection.{h,cpp}` | Digest and golden-reference selection follow the new preimage. |
| `consensus/res_proof.{h,cpp}` | `ConvergeProofDigest` drops `target_prev`. `proof.convergence_root` now binds to the parent of the including block. |
| `consensus/rigor_res.{h,cpp}`, `consensus/res_owner.{h,cpp}` | `ConnectEvaluateResMintTx` and `CheckResRewardOwner` consume the resolved parent draw instead of rederiving from the block. |
| `consensus/convergenceres.{h,cpp}`, `convergenceres_verify.cpp` | `CONVERGENCE_DRAW_BIND_DEPTH`, `ConvergenceDrawBindHash`, `ConvergenceDrawBindHashFromParent` removed. |
| `consensus/bootstrap_bind.h` | **Deleted.** No bootstrap gap exists. |
| `consensus/statetag.{h,cpp}` | Dead `ComputeRigorSigHash` / `VerifyRigorSignature` removed, and the RFC-003 stub witness surface with them (§3). |
| `consensus/rigor.{h,cpp}` | **Deleted** — a third, unreachable σ implementation. Removed from `Makefile.am`. |
| `validation.{h,cpp}` | `ConnectBlock` resolves the parent's draw; `ParentDrawSetCache` and `GetConvergenceDrawSetForBlock` added. |
| `node/miner.cpp`, `node/auto_res_mint.{h,cpp}` | Template coinbase commitment and the automatic mint both read the parent draw. `EvaluateAutoResConvergenceDraw` removed. |

Three parallel σ implementations existed before this change (`consensus/rigor.cpp`,
`consensus/selection.cpp`, `consensus/statetag.cpp` alongside the real one). Two were dead. Leaving
them would have meant three places for the next reader to disagree with, so they are gone.

### 5.2 RPC and interfaces

| File | Change |
|---|---|
| `rpc/mining.cpp` | New `convergence_draw` object on `getblocktemplate` (§4.3). `convergence_txids` documented as the committed, not the paid, set. |
| `rpc/blockchain.cpp` | `getconvergenceresult` reports `sigma_seed` and `payout_height` in place of `bind_height` / `bind_hash` / `bind_blockhash`, and resolves the draw through the new draw-set path. |
| `rpc/rawtransaction.cpp`, `node/interfaces.cpp`, `interfaces/chain.h` | Winner lookups follow the simplified `GetBlockWinnerFromBlockUndo` signature. |
| `qt/convergencepage.cpp` | Displays the tip as the seed for the next draw. |

### 5.3 Tests

| File | Change |
|---|---|
| `test/convergence_consensus_tests.cpp` | Ported to the draw-set API. Two new cases: the draw is a function of the committing block alone, and re-rolling σ requires a new proof of work. Inline RFC-001 vectors regenerated. |
| `test/res_proof_math_tests.cpp` | Fixture rebuilt around `ConvergenceDrawSet`. New cases: a mint must name a member of the parent's set, and the set must be the immediate parent's. |
| `test/selection_golden_tests.cpp`, `test/compute_rfc001_vectors.cpp` | Golden σ column regenerated; C values unchanged, since A-8 did not touch how C is built. |
| `test/canonical_layer_tests.cpp`, `test/convergence_root_tests.cpp` | Signature updates. |
| `test/bootstrap_bind_tests.cpp` | **Deleted**, with its `Makefile.test.include` entry. |
| `test/functional/feature_bootstrap_deterministic_blocks.py` | Inverted: asserts heights 1..10 carry **no** special case and each block seeds on itself. |
| `test/functional/feature_rigor_reorg_winner_lock.py` | Rewritten per §4.4. |
| `test/functional/feature_res_core_convergence_draw.py`, `…_correctness_suite.py`, `…_functional_longrun.py`, `feature_res_lifecycle_controlled_devtest_001.py` | Mint assertions moved to the payout block; each also asserts the awarding block mints nothing. |
| `test/functional/rigor_attack_blocks.py` | Case B inverted: σ no longer depends on an assumed `target_prev`, and does depend on the seed. |
| `test/functional/feature_rigor_rng_createrngclaim.py` | **Deleted** — an A-7 leftover exercising the removed `createresclaim` RPC. Unregistered from `test_runner.py`. |
| `test/rigor/`, `test/rigor_attack/` | Python invariant models updated; `test_target_manipulation.py` inverted for the same reason as case B. `test_preimage_attack.py` renamed its subject from "bind hash" to "seed": the property it asserts is unchanged, but the guarantee behind it is now stronger, because the seed no longer exists until the parent has been mined. The `bind_hash` / `bind` parameter and fixture names were renamed to `prev_block_hash` / `seed` throughout both suites, so no code in the test layer still names a concept A-8 deleted. |

---

## 6. What this does not change

- **No storage format changed.** No new field, no new index, no re-serialisation. The parent's set
  is rebuilt from block and undo data already on disk.
- **No header change.** The 80-byte header is untouched, which was the constraint that ruled out the
  direct approach in the first place.
- **No account model.** The draw still ranges over UTXO-shaped convergence outputs and still pays a
  UTXO.
- **No external dependency.** The parent's set is recovered from local block data. No indexer, no
  oracle.
- **No administrative key.** A-7 removed the last key from the mint path; A-8 adds none.
- **The eligibility rule, the argmin rule and the tie-break are unchanged.** Only the inputs to σ and
  the block at which the draw resolves moved.

---

## 7. Residual notes for review

1. **One block of settlement latency.** A convergence now waits one extra block before it can win
   and be paid. This is inherent: the commitment must be sealed by work before the draw reads it.
2. **A reorg re-rolls affected draws.** A reorg that replaces block `h-1` replaces the seed, so the
   draw paid at `h` changes. This is the intended behaviour — it is the same rule that makes
   grinding expensive — and it is not a new class of exposure, since a reorg already invalidated the
   mint at `h` under the previous rule.
3. **The one-entry parent cache is a performance structure only.** Every path it serves falls back
   to disk on a miss, and it is keyed by source block hash so a miss is the only possible outcome of
   a wrong guess.
4. **`getblocktemplate` grew a field.** Pool operators must consume `convergence_draw`; reading
   `convergence_txids` as the draw set now silently builds an invalid block. This is called out in
   the mining documentation.

---

## 8. Verification status at time of writing

| Check | Status |
|---|---|
| Standalone syntax check (`g++ -fsyntax-only -std=c++20`) of all 35 changed non-Qt `.cpp` files | Clean |
| Shipped `rigor::ComputeSigmaHash`, linked into a standalone harness, against an independent `hashlib` recomputation of the 100-byte preimage | Byte-identical on four cases, including the boundary heights 0 and 2³²−1. The same inputs under the pre-A-8 rule produce a different digest, confirming the vectors genuinely moved. |
| `core/test/rigor/rigor_model.py` against that same harness | Byte-identical on all four cases, so the Python model used by the invariant and attack suites is not merely self-consistent but agrees with the C++ |
| `core/test/rigor` Python invariant model, 18 cases | Pass |
| `core/test/rigor_attack` Python attack suite, 18 cases | Pass, including the two new seed/grinding cases |
| Deleted test sources removed from `Makefile.test.include` | Verified, no dangling entries |
| Grep for removed symbols across `core/src` | Clean; the only remaining textual matches are the comments explaining the removals |
| Full `make -j$(nproc)`, `test_bitcoin`, `make check` | **Not run here.** Per the project's build convention these are run locally by the maintainer. |
| Functional test suite | Not run; requires the built binaries |
| Pool `winner-engine` / `mint-engine` / `stratum-server` `cargo test --offline` | Pass (A-8 fixtures use replay-derived `convergence_draw` winners) |
| Formal `test_runner.py` (16), `test_phase3_oracle.py` (57), `test_golden_vectors.py` (13), `test_cpp_selection_golden.py` | Pass |
| Live RFCs / SPEC-000 / SPEC-005 / RFC-000 / RFC-001 / RFC-008 §14 | Amended for Sigma-PoW-01; RFC-014 marked superseded-in-part |

The syntax check compiles each translation unit in isolation, so it catches signature and type
errors but not link-order or duplicate-symbol problems. Two build notes:

- A-8 removes `consensus/rigor.cpp` from `libbitcoin_consensus_a_SOURCES`, and `ar` does not delete
  a member when its source leaves the list — the same hazard A-7 §4.3 hit. `ar t
  src/libbitcoin_consensus.a` was checked and currently contains no `rigor`, `statetag` or
  `bootstrap` member, so no stale object is present in this tree. **A clean rebuild is still
  recommended for the release build**, since the hazard depends on what the incremental tree
  happened to build previously.
- `consensus/selection.cpp` is listed in both `libbitcoin_node_a_SOURCES` and
  `libbitcoin_consensus_a_SOURCES`, so six consensus functions are defined in both archives. This
  predates A-8 and is unchanged by it; it is noted here only so that a duplicate-symbol diagnostic
  during the release build is not mistaken for a regression introduced by this work.
