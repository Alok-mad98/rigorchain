# RIGOR Convergence Draw Security Audit (P0-079)

**Task:** `TASK-RIGOR-CONVERGENCE-DRAW-SECURITY-AUDIT-P0-079`  
**Date:** 2026-08-31  
**Nature:** Convergence Draw lottery security audit. Isolation of winner
selection + fail-closed candidate assembly. This is not a proof of true
randomness, and it does not recode the draw formula.

This TASK does not modify `ResolveConvergenceDraw`, the draw formula,
issuance, supply, ConsensusKernel, or TransactionKernel. It does not add
a second winner selector, a miner/wallet/RPC lottery, or a new Coq
theorem.

```
F = candidate set          (assembled by GetConvergenceDrawSetForBlock)
T = threshold / draw parameter
        ↓
ResolveConvergenceDraw(F, T)     (unique draw authority)
        ↓
winner | none
```

Forbidden entries: `SelectWinner` legacy path, miner local selection,
wallet selection, RPC selection.

```
CONVERGENCE_DRAW_SECURITY = CLOSED
DRAW_AUTHORITY_COUNT      = 1
DRAW_DETERMINISM          = PASS
DRAW_TIEBREAK             = PASS
DRAW_WEIGHT_SECURITY      = PASS
MANIPULATION_RESISTANCE   = PASS
CONSENSUS_MODIFIED        = NO

FINAL_VERDICT:
RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_CLOSED
```

Reproduce: `bash tools/run-convergence-draw-security-audit.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| single draw authority | Production winner selection is `ResolveConvergenceDraw` only (`convergence_logic.cpp`). `RigorConsensusKernel::ResolveDraw` forwards. Lint: `tools/lint-convergence-draw-authority.py`. |
| deterministic winner | Identical `(F, T)` yields the same winner txid on every node, independent of candidate order, transaction arrival order, and serialization order. Tests: `core/src/test/test_convergence_draw_security.cpp`. |
| overflow safety | Eligibility and argmin use 512-bit `Mul256x256Full`. `f < 1` and `!MoneyRange(f)` are ineligible. `weight = 0` and `weight = MAX_UINT64` produce `none`. Overflow wraps to no winner, not a forged winner. |
| tie-break safety | Equal cross-product `H_i * f_j == H_j * f_i` breaks to the smaller txid (`memcmp`). Reverse vector order, equal weight, equal timestamp (`source_height`), and equal score do not change the winner txid. |
| candidate integrity | `GetConvergenceDrawSetForBlock` assembles F. It does not select a winner. Invalid / unknown-asset / malformed members fail `IsConvergenceDrawMember` and do not enter F. Reconstructed C must match the coinbase C. |
| adversarial resistance | Against a committed `(F, T)`: ordering, flooding with weight-0 extras, duplicate members, and `UINT64_MAX` inflation cannot change the winner txid. σ is seeded by the parent block hash (Sigma-PoW-01). |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| true randomness proof | σ is `Hash256(txid \|\| C \|\| B_{h-1} \|\| LE32(h))`. This audit treats SHA-256d as a deterministic function. It does not prove cryptographic randomness. |
| economic game theory equilibrium | Window width, burn cost, and hashrate exclusivity are outside this TASK. |
| miner behavior prediction | Template construction calls `ResolveConvergenceDraw`; this audit does not model miner inclusion strategy. |

This note is a boundary + determinism audit. The existing Coq theorem
`ConsensusKernelRefinement.draw_refinement` is unchanged.
`COQ_ADMITTED = 0`, `COQ_AXIOMS = 0`, `COQ_PARAMETERS = 0`. No new
theorem was added.

---

## 1. Draw authority inventory

Manifest: `formal_verification/convergence_draw_audit_manifest.json`

| Role | Symbol |
|------|--------|
| Unique draw authority | `ResolveConvergenceDraw` |
| Inputs | `F` = candidate set, `T` = `RES_PROBABILITY_TARGET` |
| Output | `winner \| none` |
| Assembly (not selection) | `GetConvergenceDrawSetForBlock` |
| Kernel wrapper | `RigorConsensusKernel::ResolveDraw` → `ResolveConvergenceDraw` |
| Forbidden | `SelectWinner`, miner local selection, wallet selection, RPC selection |

`DRAW_AUTHORITY_COUNT = 1`

Production ConnectBlock calls `RigorConsensusKernel::ResolveDraw` after F
is assembled. Miner GBT and RPC introspection call
`ResolveConvergenceDraw` on that same body. They do not define a second
formula.

Historical residue: `SelectWinner` / `RunSelection` in
`core/src/consensus/selection.cpp`. Lint fails if validation, miner,
wallet, or RPC call it.

---

## 2. Candidate set audit

`GetConvergenceDrawSetForBlock` (validation.cpp) is candidate assembly:

- read the index-cached F, or reconstruct F from blk/rev + window tail
- require reconstructed C to equal the source coinbase C
- return the set; do not compute `winner_index`

Winner selection is a separate call to `ResolveDraw(F, T)`.

`IsConvergenceDrawMember` admits a transaction into F only when it is
not coinbase, is a convergence tx, meets `MIN_CONVERGENCE_AMOUNT`,
carries an A-5 owner declaration, and `CheckTxInputs` succeeds.

| Input | Result |
|-------|--------|
| candidate mutation (C mismatch) | REJECT |
| duplicate candidate | REJECT (block duplicate txid; resolver winner txid still unique) |
| empty candidate / empty F | none |
| invalid candidate | REJECT (not a member of F) |
| unknown asset | REJECT |
| malformed tx | REJECT |

Forbidden: crash, undefined behavior, silent window shortening.

---

## 3. Determinism audit

Same `(F, T)` → identical winner on every node.

Covered:

- same candidate order
- different candidate order (all permutations of a 3-set)
- different transaction arrival order (vector reverse)
- different serialization order (same permutation family)
- `source_height` permutation (not a σ input)

`DRAW_DETERMINISM = PASS`

---

## 4. Tie-break audit

RFC-000 §9 / Implementation Bible §11: if `H_i * f_j == H_j * f_i`,
the smaller txid wins. Comparison is unsigned byte-wise `memcmp` on the
32-byte txid. `ConvergenceWinnerTieBreakCompare` ignores
`convergence_root`.

Covered: reverse ordering, equal weight, equal timestamp, equal score
(duplicate txid). Map order, vector order, and memory order do not
change the winner txid.

`DRAW_TIEBREAK = PASS`

---

## 5. Weight security audit

```
weight = 0          → ineligible / none
weight = MAX_UINT64 → ineligible / none  (casts out of MoneyRange)
weight overflow     → NONE (512-bit product; no 256-bit wrap)
large candidate set → winner in range, order-stable
```

`ConvergenceDrawInequalityHolds` rejects `f_sats < 1` and
`!MoneyRange(f_sats)` before multiplication.

`DRAW_WEIGHT_SECURITY = PASS`

---

## 6. Threshold boundary audit

| T | Result |
|---|--------|
| T = 0 | none (determinate) |
| T = max (2^256-1) | winner if F is non-empty and weights are valid |
| T below winner | none |
| T above winner | the saturating-weight candidate wins |

---

## 7. Adversarial manipulation audit

Against a committed `(F, T)`:

| Attack | Result |
|--------|--------|
| txid grinding (reorder / extra grind bytes) | winner unchanged; σ binds `B_{h-1}` so a new txid is a new F and costs parent PoW |
| candidate flooding (weight-0 extras) | winner unchanged |
| duplicate candidate | winner txid unchanged |
| weight inflation (`UINT64_MAX`) | extra member ineligible; winner unchanged |
| serialization manipulation | winner unchanged |
| ordering manipulation | winner unchanged |

`MANIPULATION_RESISTANCE = PASS`

---

## 8. Fuzz

`fuzz/convergence_draw/` feeds random candidate sets, weights, txids,
and thresholds into `ResolveConvergenceDraw`.

```
NO CRASH
NO NONDETERMINISM
NO INVALID WINNER
```

An order-dependent winner txid is `NONDETERMINISM`. A winner index
outside F, or a winner that is not eligible, is `INVALID_WINNER`.

---

## 9. Formal boundary

Existing theorem: `ConsensusKernelRefinement.draw_refinement`.

This TASK does not add a theorem. Counts on
`ConsensusKernelRefinement.v` and `DrawResolverBridge.v`:

```
COQ_ADMITTED   = 0
COQ_AXIOMS     = 0
COQ_PARAMETERS = 0
```

---

## Gates

```
bash tools/run-convergence-draw-security-audit.sh
```

Runs: authority lint, formal boundary check, malformed / determinism /
tie-break / weight / threshold / adversarial tests, fuzz smoke,
`run_differential_gate.sh`, `run_consensus_kernel_refinement_gate.sh`.
