# RIGOR — Formal Verification Verdict


> **Shipping issuance (RFC-020).** Continuous `destroy_cost(h)`, pole predicate
> `R · (2²⁵+h)² ≤ B << 40`, cap `2²²` RES, flat 128 ORU. Numeric SSOT:
> `tools/res_continuous_price_schedule.py`. Formal status:
> `audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`
> (`REFINEMENT_PROVED=false`).


**Status**: Final
**Subject**: `formal_verification/coq` — the machine-checked model of the RIGOR consensus protocol
**Prover**: Coq 8.15.0
**Scope of this document**: what has been proved, what has *not*, and exactly where the boundary lies.

**Addendum / freeze (2026-08-16)** plus **tightening and σ/root CLI (2026-08-18)**: A-9-C is modeled;
324 shared vectors; FIPS-locked SHA-256; extracted OCaml hasher; portable C++ hash CLI;
tripartite gate; production σ/root CLI in the differential gate; replay created-outpoints use `tx_id`.
Full refinement is still **not** proved. Current correspondence status:
[FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)
([FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md)).

This is the authoritative statement. Where an earlier `docs/proof/PROOF-0xx` note disagrees with
this document, this document governs.

---

## 1. Method

Three checks were run against the development, all reproducible from a clean checkout:

```bash
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq clean && make -f Makefile.coq -j$(nproc)
```

1. **Full rebuild from zero.** Every `.vo` and `.glob` deleted, then rebuilt. 196 modules,
   ~28,000 lines, 3m08s wall on 8 cores. Zero errors. The only warnings emitted are Coq's benign
   notice about large `nat` literals in the SHA-256 test vectors.

2. **Syntactic audit for trust holes.** Every `.v` file was stripped of comments and searched for
   `Axiom`, `Parameter`, `Hypothesis`, `Admitted.`, and `admit`. **Zero occurrences remain.**
   (Occurrences that a naive `grep` reports are all inside prose comments describing placeholders
   that were previously removed.)

3. **Semantic audit via `Print Assumptions`.** A generated file imports all 196 modules
   simultaneously and calls `Print Assumptions` on every one of the **167** `Theorem`s and
   `Corollary`s in the development. Every single one reports:

   ```
   Closed under the global context
   ```

Check 3 is the one that matters. It is strictly stronger than check 2: it walks the entire proof
term transitively and reports any axiom reachable from it. "Closed under the global context" means
the theorem depends on **nothing** beyond Coq's own kernel — no classical logic
(`classic`), no functional extensionality, no proof irrelevance, no `Admitted` lemma anywhere in
its dependency cone, and no abstract hash oracle.

The last point deserves emphasis, because it is the usual weak spot in blockchain formalizations.
`formal_verification/coq/RigorSha256.v` is a **complete, executable SHA-256 written in Coq**
(FIPS 180-4: message schedule, the 64 round constants, compression function, padding). σ and the
convergence root are computed by *running* it, not by reasoning about an uninterpreted symbol
`hash : bytes -> bytes`. The golden-vector lemmas discharge by `vm_compute`, which means Coq
actually evaluates double-SHA-256 on concrete bytes and checks the 32 output bytes.

As part of this audit, the last vestige of the old approach — a legacy `Hash.v` declaring
`Parameter hash256 : Z -> Z` plus an axiom — was deleted. It was reachable from no theorem
(`Print Assumptions` already ignored it) and was imported only by a dead line in `Extraction.v`.
Its removal makes the "zero axioms" claim true syntactically as well as semantically.

---

## 2. What is proved

### 2.1 Core consensus

**The σ digest is byte-exact, not schematic.** `SigmaPoW01.v` fixes the 100-byte preimage
`σ = Hash256(txid‖C‖B_{h-1}‖LE32(h))` at the byte level, with each field's offset and width pinned.
There is no `target_prev` input: `B_{h-1}` is the parent header hash, which already commits the
parent's `nBits`, so the compact target is committed transitively rather than folded in twice.
This is SECURITY-001-A8 (deferred draw), and the model carries it verbatim.

**The convergence root is a function of the candidate *set*, not of arrival order.**
`OrderIndependence.convergence_root_order_independent` proves that for any two lists that are a
`Permutation` of each other, `convergence_root_from_txid_bytes` returns the same 32 bytes. This is
the load-bearing anti-fork property: the coinbase commits `C`, and `C` feeds σ for the next block's
draw, so a `C` that depended on the order transactions arrived in would split the network on nothing
worse than propagation timing.

The model also reproduces the `N < 2 ⇒ C = 32 zero bytes` boundary exactly as
`ComputeConvergenceRootFromEligibilityTxids` implements it — including the singleton case, which is
easy to get wrong in either direction.

**The winner is a function of the candidate set too.**
`OrderIndependence.select_winner_order_independent` proves the same permutation invariance for the
draw itself. This is not a one-line corollary: `select_winner` folds a pairwise comparison
`pick_better_tx` over a list, so the proof has to establish that the comparison
(cross-product `σ_x·f_y < σ_y·f_x`, ties broken by `memcmp` on txid) is a **strict total order** —
irreflexive, asymmetric, transitive, and total — under the hypotheses that weights are positive and
txids are distinct. Only then is the fold an argmin rather than an order-sensitive accumulation.

Together these two theorems close finding **G-1**: two honest nodes holding the same candidate set
commit the same `C` and pay the same transaction, no matter what order the candidates reached them.

**The eligibility filter is non-vacuous, and the historical defect is proved to have been real.**
`Selection.v` proves three things about the target constant `T`:

- `res_probability_target_not_vacuous` — the active `T` satisfies `0 < T·f_min < 2^256`, so the
  inequality `σ < T·f` can actually fail.
- `res_probability_target_rejects_max_digest` — the maximal digest `2^256 − 1` is rejected at the
  minimum weight, i.e. the filter demonstrably filters.
- `pre_a6_target_was_vacuous` and `pre_a6_every_digest_was_eligible` — the *original* RFC-014 v1.0
  constant `T = 2^240` was vacuous: **every** digest in `[0, 2^256)` passed at the minimum weight.

That third item is unusual and worth calling out. The development does not merely assert the fixed
constant is correct; it carries a machine-checked proof that the pre-fix constant was exploitable,
in the precise sense that `eligible` carried zero information and candidate admission was entirely
block-producer controlled. The A-6 finding is a theorem, not a claim.

**A-9-C windowing is modeled and its neutrality argument is checked.** `Selection.v` carries
`CONVERGENCE_DRAW_WINDOW = 32` and `T = T_unwindowed / W`, with:

- `window_is_a_power_of_two`, `window_widens_the_candidate_set` — sanity bounds.
- `target_is_unwindowed_divided_by_window` — a two-sided bracket
  `T·W ≤ T_unwindowed < (T+1)·W` that states exactly how much the floor division loses (the
  residue is 28, i.e. one part in `T_unwindowed`). Issuance neutrality is therefore quantified,
  not hand-waved.
- `windowing_preserves_non_vacuity` — dividing a non-vacuous target by a positive window cannot
  make it vacuous, so A-9-C provably cannot reintroduce the A-6 degeneracy.

### 2.2 Economic policy

**RES is capped, and the cap is enforced inductively.** `res_mint_respects_cap` proves that if
`res_supply ≤ MAX_RES_SUPPLY` holds before a block, it holds after — for every block, winning or
not. `MAX_RES_SUPPLY = 21,000,000 × 100` (RES carries two decimals), matching
`cb_MAX_RES_SUPPLY` and the C++ constant.

**ORU is not capped, and the model says so explicitly.** This is the resolution of finding **G-2**,
and it is the most intellectually honest part of the development. G-2 originally asked for
`total_oru_supply ≤ ORU_MAX_SUPPLY`. That statement is **false for RIGOR and cannot be proved**: the
subsidy halves only three times and then pays 125 ORU per block forever. ORU has tail emission by
design.

Rather than quietly drop the finding or prove a weaker thing under a misleading name,
`SupplyConservation.v` states the property that actually rules out the failure mode G-2 was worried
about:

- `apply_block_oru_bounded` — one block adds at most one genesis-era subsidy.
- `oru_supply_bounded_by_block_count` — *n* blocks add at most *n* subsidies.

Issuance is therefore linear in chain length and bounded per block. The only lever an attacker has
is producing blocks, which costs proof of work. Unbounded or attacker-chosen minting is excluded.
The two theorems `res_mint_respects_cap` and `oru_supply_bounded_by_block_count` are stated
adjacently and commented so the RES/ORU asymmetry reads as the deliberate design choice it is
rather than as an oversight.

### 2.3 State, replay, and reorg

- `apply_block_supply_correct` — supply moves **only** through the ConnectBlock mint rules. No
  other code path in the model can change it.
- `supply_deterministic` / `valid_chain_supply_deterministic` — applying the same block to the same
  state yields the same supply. Replay is deterministic.
- `supply_monotonic` — supply never decreases on a successful block application.
- `rollback_preserves_supply`, `rollback_snapshot_supply_unique` — rollback returns to a unique,
  supply-consistent snapshot.
- `reorg_preserves_supply` — rollback followed by replay of the winning chain preserves tip supply.
  Note the history here: this theorem's final conjunct used to read
  `total_supply s_end = total_supply s_end`, a reflexivity that asserts nothing. It was found and
  strengthened under SECURITY-001-F/F-4. It now genuinely relates the reorg endpoint to the
  candidate state.
- `genesis_supply_zero` — the chain starts from zero, not from an assumed premine.

### 2.4 Trust anchors

| Anchor | Status |
|---|---|
| Axioms the protocol theorems rest on | **None.** All 167 verified `Closed under the global context`. |
| Hash function | Executable SHA-256/SHA256d in pure Coq, not an oracle. `hash256_bytes_length` proves the 32-byte output. |
| Classical logic / choice / extensionality | Not used anywhere. |
| Admitted or sorry-style holes | None. |
| Trusted computing base | Coq 8.15 kernel only. |
| Golden vectors | **324** shared vectors in `phase3_golden_vectors.json`. **117** are build-breaking `vm_compute` lemmas in `Kernel/GoldenHarness.v` (original kernel SHA-256 set plus WindowExclusion, SigmaEncoder, extra Inequality, short HashKernel). The remainder are Python/C++ differential cases. A regressed *enforced* vector fails the Coq build; a regressed Python/C++ vector fails `run_differential_gate.sh`. |

---

## 3. What is **not** proved

This section is the point of the document. A verification result is only worth what its boundary
statement is worth.

### 3.1 Coq ⇄ C++ refinement is NOT established

**Everything in §2 is a theorem about the Coq model of RIGOR, not about the C++ node that will run
on mainnet.** There is no mechanized refinement proof connecting `core/src/consensus/*.cpp` to
`formal_verification/coq/*.v`. A bug that exists in the C++ but not in the model — a signed/unsigned
slip, a `uint256` endianness inversion, an off-by-one in the window walk — is entirely outside
what §2 rules out.

The development is candid about this rather than papering over it, which is why
`Phase5Equivalence/` exists. That layer does not attempt the refinement; it *formalizes the fact
that the refinement is not closed*, and formalizes why:

- `DrawClosureImpossibilityCatalog` / `...Audit` prove `draw_closure_status = DCS_Impossible`
  unconditionally under the current architecture, with the missing prerequisites enumerated as a
  non-empty list — principally the absence of a canonical draw encoder and a runtime hash kernel
  usable as a shared oracle.
- The C++ runtime harness agrees from its own side: `runtime_vector_harness_tests` asserts
  `batch["equivalence_claimed"] == false`.

Two consequences for any reader assessing this project:

1. **`Phase5Equivalence/` is bookkeeping, not evidence.** It contains 31 lemmas explicitly labeled
   `VACUOUS-PLACEHOLDER ... NOT EVIDENCE` — statements of the form `forall s, True`, provable by
   `exact I`, retained only to keep a catalog's shape. They constrain nothing. They are all
   confined to that one directory: `Kernel/`, `Proofs/`, `ReplayReorgCore/`, `ConnectBlock/`,
   `Phase4FConsensus/`, `Phase4GRefinement/`, and the root modules contain **zero** of them.
2. Do not read a file named `...Certification.v` or `...FinalReadinessVerdict.v` as a proof of
   correctness. Those are status records over the catalog.

### 3.2 What covers the gap instead

Since refinement is not proved, the Coq↔C++ link rests on **differential testing over a finite
vector set**, and that is exactly as strong as a finite vector set:

- The same golden vectors are pinned on both sides — as `vm_compute` lemmas in Coq (117 enforced)
  and as generated C++ cases in `selection_golden_tests.cpp` / `selection_golden_bridge.inc`
  plus the original hardcoded selection cases. Agreement on 324 vectors is stronger evidence
  than 57; it is still not equivalence. A portable C++ `CSHA256` CLI is compared to hashlib and
  the extracted Coq kernel in `test_hash_kernel_tripartite.py`.
- The C++ side is additionally covered by the unit suite, the functional suite
  (`feature_res_core_convergence_draw.py`, `feature_res_core_convergence_window.py`,
  `rigor_attack_blocks.py`), and the release gates.

### 3.3 Specific model gaps

- **A-9-C's no-double-win rule is modeled (2026-08-16).** `WindowExclusion.v` defines
  `exclude_already_paid` / `paid_closed_trace` and proves `txid_wins_at_most_once`.
  `Selection.v` still carries only the width `W` and the `T/W` target. The Coq theorem is about
  the model; C++ `FinalizeWindowedDrawSet` is still connected by vectors, not by refinement.
- **Replay created-outpoints use `tx_id` (2026-08-18).** The shared-placeholder shortcut is
  gone. Spend-deletion is unconditional for distinct txids. The remaining counterexample is
  txid reuse, not a modelling collision.
- **The draw is modeled as a pure function, not as a game.** Everything in §2.1 is a determinism,
  well-definedness, and non-vacuity result. No theorem here bounds an adversary's *advantage*.
  Statements like "grinding is unprofitable at hashrate share ρ" are economic arguments made in
  RFC-014 and the A8 design review; they are not machine-checked, and the σ preimage being fixed by
  a PoW-bearing header is an argument about cost, not a theorem in this development.
- **Fairness is not a probabilistic theorem.** `Proofs/Fairness.v` proves only that pairwise
  selection prefers the lower cross-product and is total — structural facts about
  `pick_better_tx`. It previously carried a `Pr := 1` constant that made its weight-monotonicity
  lemma vacuous; that was removed. The probabilistic statement — that a candidate burning `f` wins
  with probability proportional to `f` over time — needs a probability model the development does
  not have, and is carried as open proof debt.
- **Network, mempool, and P2P layers are entirely out of scope.** The model starts at
  "a block is presented to ConnectBlock".

---

## 4. Verdict

The RIGOR consensus **model** is verified to an unusually high standard for this class of project:
196 modules, 167 theorems, **zero axioms**, zero admits, a full rebuild from clean, and an
executable SHA-256 rather than a hash oracle. The properties proved are the load-bearing ones —
order independence of both the root and the winner (G-1), non-vacuity of the eligibility filter
including a proof that the pre-A-6 constant was exploitable, issuance bounds for both assets (G-2),
and supply conservation across replay and reorg. The two findings that a formal model most often
fudges — an unprovable supply cap, and a reflexive tautology masquerading as a reorg theorem — were
both caught and corrected rather than left standing.

The RIGOR **implementation** is not formally verified, and the development says so in its own
machine-checked terms. The Coq↔C++ boundary is held by 324 shared golden vectors and a conventional
test suite. Anyone evaluating this project should treat §2 as a strong statement about the protocol
design and §3.1 as the honest limit of what that buys at runtime.

Closing the refinement gap still requires a mechanized correspondence proof. A canonical draw
encoder and an executable SHA-256d kernel now exist on both sides and agree on the golden set
(see the 2026-08-16 addendum); they are **not** a shared extracted kernel and they do **not**
constitute refinement.
