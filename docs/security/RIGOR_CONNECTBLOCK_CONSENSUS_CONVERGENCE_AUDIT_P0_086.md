# RIGOR ConnectBlock / Consensus Convergence Point / Single-Path Security Audit (P0-086)

**Task:** `TASK-RIGOR-CONNECTBLOCK-CONSENSUS-CONVERGENCE-AUDIT-P0-086`  
**Date:** 2026-09-02  
**Nature:** Audit-first. This TASK does **not** modify consensus rules, the
Consensus Kernel, Transaction Kernel, Ledger, genesis, issuance, RES, draw,
PoW, ASERT, protocol parameters, or Coq theorems.

The question is not “does ConnectBlock pass tests”. The question is:

> Before a block becomes local consensus state, do all required consensus
> rules travel a single, non-bypassable, auditable acceptance path?

```
CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT = OPEN
CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT  = 1
CONSENSUS_KERNEL_AUTHORITY_COUNT         = 1
CONSENSUS_BYPASS_COUNT                   = 5
DUPLICATE_AUTHORITY_COUNT                = 1
STATE_MUTATION_BEFORE_ACCEPTANCE         = TRUE
CONNECT_DISCONNECT_SYMMETRY              = PARTIAL
REORG_COVERAGE                           = PARTIAL
DATABASE_BOUNDARY_STATUS                 = INHERITED_BASE_LAYER_OPEN
DATABASE_CONSENSUS_IMPLEMENTATION        = INHERITED_BASE_LAYER
DATABASE_CONSENSUS_RISK                  = OPEN
FORMAL_COVERAGE                          = PARTIAL
FUZZ_STATUS                              = PARTIAL
MUTATION_STATUS                          = PASS
GOLDEN_VECTOR_STATUS                     = PARTIAL
RELEASE_GUARDIAN_COVERAGE                = PARTIAL
LIVE_MINT_GOLDEN_VECTOR                  = NOT_COVERED
REORGBIND_GOLDEN_VECTOR                  = NOT_COVERED

CONSENSUS_CODE_CHANGED                   = FALSE
PROTOCOL_PARAMETERS_CHANGED              = FALSE
FORMAL_THEOREMS_CHANGED                  = FALSE
```

Reproduce: `bash tools/run-connectblock-consensus-convergence-audit.sh`

The audit stays **OPEN**. Do not edit production consensus to force CLOSED.

---

## Executive answer

**Node acceptance of a connected tip is unique:**

```
NODE_ACCEPTANCE_AUTHORITY =
  Chainstate::ConnectBlock
    → RigorConsensusKernel::EvaluateConnect
        → rigor::ConnectEvaluateCoinbaseResMint
        → RigorSupplyDelta
```

`MINER_ACCEPTANCE_AUTHORITY = NONE`  
`POOL_ACCEPTANCE_AUTHORITY = NONE`  
`EXPLORER_INTERPRETATION = NONE`

Miner / GBT / pool / explorer may *assemble or display*. They must not be
confused with node accept/reject. Production economic formulas are not
re-coded inside `ConnectBlock` (P057 lint still holds).

That uniqueness does **not** close the audit. Five bypass / skip paths
remain. Contextual header/block rules are **not** re-executed in
`ConnectBlock`. Per-tx `UpdateCoins` mutates a stack-local UTXO cache
**before** the Kernel economic gate. `DisconnectBlock` restores UTXO and
invalidates observational caches, but does not clear RIGOR index memos.
ReorgBind and live-mint ConnectBlock hex goldens are **NOT_COVERED**.
The coins database is the inherited Bitcoin base layer and remains OPEN
in the sibling P0-086 persistence audit.

---

## 1. Block acceptance call graph

Actual order, from code (not the hoped-for stencil):

```
INGRESS
  → HEADER_CHECK          AcceptBlockHeader / CheckBlockHeader
  → POW_CHECK             ContextualCheckBlockHeader (nBits + CheckProofOfWork)
  → BLOCK_STRUCTURE       CheckBlock (merkle, size, coinbase shape, CheckTransaction)
  → CONTEXTUAL_BODY       ContextualCheckBlock (finality, witness commitment, weight)
  → STORE                 AcceptBlock → SaveBlockToDisk (no UTXO)
  → FORK_CHOICE           ActivateBestChain / ActivateBestChainStep
  → CONNECT               ConnectTip → ConnectBlock
       → BIP30
       → DRAW_ASSEMBLY    GetConvergenceDrawSetForBlock + Kernel::ResolveDraw
       → PREDICATE        CheckTxInputs → SequenceLocks → CheckInputScripts
                          → VerifyRigorPredicate → MakeKernelInput
       → UTXO_MUTATE      UpdateCoins   ← cache write, still unflushed
       → F_CONSISTENCY    live vs undo draw set
       → KERNEL           EvaluateConnect (RES / supply / issuance / coinbase)
       → INDEX_MEMO       pindex RIGOR fields + observational ledgers
       → STATE_COMMIT     WriteUndoDataForBlock + view.SetBestBlock
       → FLUSH            ConnectTip view.Flush() only if ConnectBlock returned true
```

If a later rule is added only to `ContextualCheckBlock*`, IBD / reindex /
already-stored blocks will **not** see it in `ConnectBlock`. That is a
code-fact upgrade hazard (`validation.cpp` 2776–2787).

### Ingress → ConnectBlock

| Ingress | First functions | Reaches ConnectBlock? |
|---------|-----------------|------------------------|
| P2P `BLOCK` / `CMPCTBLOCK` | `ProcessMessage` → `ProcessBlock` → `ProcessNewBlock` | Yes, via `ActivateBestChain` if stored |
| P2P `HEADERS` | `ProcessNewBlockHeaders` → `AcceptBlockHeader` | Header only |
| RPC `submitblock` | `ProcessNewBlock(force=true, min_pow_checked=true)` | Yes |
| RPC `submitheader` | `ProcessNewBlockHeaders` | Header only |
| GBT proposal | `TestBlockValidity` → `ConnectBlock(fJustCheck=true)` | Dry-run only |
| GBT / `CreateNewBlock` | assembler; optional `TestBlockValidity(fCheckPOW=false, fCheckMerkleRoot=false)` | Template, not accept |
| `generatetoaddress` / solo | `CreateNewBlock` + `ProcessNewBlock` | Yes |
| Pool | `submitblock` to `rigord` | Yes (same as RPC) |
| IBD | headers first, then same `ProcessNewBlock` / `ActivateBestChain` | Yes |
| Reorg | `ActivateBestChainStep`: `DisconnectTip`* then `ConnectTip`* | Yes |
| Startup / `-reindex` | `ReplayBlocks` then `ActivateBestChain` | Replay uses `RollforwardBlock` (**no** ConnectBlock) |
| `VerifyDB` | `CheckBlock` + optional reconnect via `ConnectBlock` | Partial |
| `PreciousBlock` / `InvalidateBlock` / `reconsiderblock` | fork-choice / disconnect / `ActivateBestChain` | Indirect |
| Regtest | same graph; `IsMockableChain` / `fPowNoRetargeting` / `IsTestChain` relax *policy*, not Kernel formulas | Yes |

```
P2P / RPC / generate / pool
        │
        ▼
ProcessNewBlock
  CheckBlock ─────────────────────────── reject (no AcceptBlock on MUTATED)
        │
        ▼
AcceptBlock
  AcceptBlockHeader → CheckBlockHeader + ContextualCheckBlockHeader
  CheckBlock + ContextualCheckBlock
  SaveBlockToDisk                       (index + blk, not UTXO)
        │
        ▼
ActivateBestChain → ActivateBestChainStep
  DisconnectTip → DisconnectBlock       (reorg unwind)
  ConnectTip → ConnectBlock             (UTXO + Kernel)
        │
        ▼
view.Flush / FlushStateToDisk / SetTip
```

`TestBlockValidity` is the only dry-run that *does* re-run contextual
header/body checks plus `ConnectBlock(fJustCheck=true)`.

---

## 2. ConnectBlock consensus-rule inventory

Legend: **Auth** = who rejects; **Unique** = one production body;
**In CB** = executed inside `ConnectBlock`; **Repeat** = also elsewhere;
**Bypass** = skip path; **Pre-mut** = before persisted UTXO commit;
**Classifier** = ScriptError vs accept predicate.

| Rule | Auth | Unique? | In CB? | Repeat? | Bypass? | Pre-persist? | Coverage | Classifier mix? |
|------|------|---------|--------|---------|---------|--------------|----------|-----------------|
| Header version ≥ 4 | `ContextualCheckBlockHeader` | Y | **N** | Accept / TBV | reindex-chainstate; not re-run in CB | Y (accept) | — | N |
| Previous block | `AcceptBlockHeader` + CB `assert(view.GetBestBlock())` | Y | Partial | Header accept | — | Y | — | N |
| Height | `CBlockIndex::nHeight` | Y | Used | Index | — | Y | — | N |
| Timestamp MTP / +2h | `ContextualCheckBlockHeader` | Y | **N** | Accept / TBV | same contextual gap | Y (accept) | CONDITIONAL-HEX | N |
| nBits / ASERT | `GetNextWorkRequired` via contextual | Y | **N** | Accept / TBV | same | Y (accept) | HEX-HEADER | N |
| PoW | `CheckProofOfWork` | Y | Partial (`CheckBlock` only if `!fJustCheck`) | Contextual + CheckBlock | `fJustCheck`; template `fCheckPOW=false` | Y | CONDITIONAL-HEX | N |
| Chainwork / min work | `AcceptBlockHeader` `min_pow_checked` | Y | **N** | Header anti-DoS | untrusted P2P headers | N/A | — | N |
| Merkle + CVE-2012-2459 | `CheckBlock` | Y | Y | Accept / PNB | `fJustCheck` skips merkle in CB | Y | VECTOR-LOCKED | N |
| Tx count / empty | `CheckBlock` | Y | Y | Accept | — | Y | — | N |
| Coinbase first / only | `CheckBlock` | Y | Y | Accept | — | Y | LIVE-MUTATION | N |
| Coinbase scriptSig size | `CheckTransaction` | Y | Y | Mempool | — | Y | — | N |
| Tx ordering | implicit (coinbase first; merkle) | Y | Y | — | — | Y | LIVE-MUTATION | N |
| Duplicate txids in block | BIP30 / spent inputs after first `UpdateCoins` | Partial | Y | — | — | Hybrid | LIVE-MUTATION | N |
| Duplicate vin | `CheckTransaction` | Y | Y | Mempool | — | Y | native tx vectors | N |
| Input/output / asset / range | `CheckTransaction` + `CheckTxInputs` | Y | Y | Mempool | — | Before that tx’s `UpdateCoins` | fuzz / economic tests | N |
| Spent outputs | `CheckTxInputs` | Y | Y | Mempool | — | Before that tx’s `UpdateCoins` | — | N |
| Maturity | `CheckTxInputs` / `CoinbaseMature` | Y (same inequality) | Y | — | — | Before spend | LIVE-MUTATION | N |
| BIP68 | `SequenceLocks` in CB | Y | Y | Mempool (other path) | — | Before `UpdateCoins` | — | N |
| nLockTime finality | `ContextualCheckBlock` | Y | **N** | Accept / TBV | contextual gap | Y (accept) | — | N |
| Script / predicate / sighash / witness | `CheckInputScripts` → `VerifyRigorPredicate` → `MakeKernelInput` → `VerifyRigorPredicateBoundary` | Y | Y | Mempool | **assumevalid** | Before `UpdateCoins` | differential + LIVE-MUTATION | **No** — classifier cannot flip reject→accept |
| Witness commitment | `ContextualCheckBlock` | Y | **N** | Accept / TBV | contextual gap | Y (accept) | LIVE-MUTATION | N |
| Fees / ORU conservation | `CheckTxInputs` + Kernel supply | Y | Y | Mempool | — | Hybrid | — | N |
| Coinbase value / subsidy / ASERT reward | `RigorSupplyDelta` via Kernel | Y | Y (via Kernel) | — | — | After loop, before persist | KERNEL-ONLY | N |
| Issuance schedule | `GetBlockSubsidy` / `GetResMintReward` inside Kernel path | Y | Y (via Kernel) | Miner packaging | — | After loop | KERNEL-ONLY | N |
| RES mint | `ConnectEvaluateCoinbaseResMint` via Kernel | Y | Y | — | — | After loop | UNIT-NOT-GOLDEN | N |
| Draw | `Kernel::ResolveDraw` → `ResolveConvergenceDraw` | Y | Y | Miner `CreateNewBlock` (assembly) | I/O → AbortNode | Before persist | KERNEL-ONLY | N |
| Supply | `RigorSupplyDelta` | Y | Y | — | Observational WARNING is not a reject | After loop | KERNEL-ONLY | N |
| Convergence root C | `ValidateCoinbaseConvergenceRootScriptSig` | Y | Y | — | — | Before loop mutate | unit | N |
| Live F vs undo F | CB post-loop | Y | Y | — | — | After cache mutate | TASK-010 | N |
| Chainstate persist | `SetBestBlock` + `ConnectTip::Flush` | Y | Y | — | Flush only on success | Commit | LIVE-ASSERT | N |

### Per-rule notes that matter

1. **PoW / target / timestamp / weight / finality / witness commitment
   are not ConnectBlock’s unique authority.** They live in
   `ContextualCheckBlock*` / `CheckBlock` at accept time. ConnectBlock
   re-runs `CheckBlock` only when `!fJustCheck`.
2. **Issuance / RES / supply / draw economics are ConnectBlock-only
   (via Kernel).** That is the designed single acceptance authority,
   not an accident.
3. **`MakeKernelInput` is unique marshalling, not a second predicate.**
4. **`ClassifyRigorPredicateError` fills `ScriptError` after a reject.
   If the classifier would accept, production fail-closes with
   `SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE`.** Classifier boolean never
   turns a reject into an accept.

---

## 3. Single-path / duplicate-authority

### A. Rules that only ConnectBlock truly enforces

```
RULE → CONNECTBLOCK
  BIP30 overwrite
  Parent draw assembly + hash match
  Kernel::ResolveDraw consumption
  Coinbase convergence root
  CheckTxInputs (UTXO / asset / maturity / fees)
  SequenceLocks
  CheckInputScripts / VerifyRigorPredicate   (unless assumevalid)
  Live/undo F consistency
  EvaluateConnect (RES mint, supply, coinbase budget, winner coupling)
```

These are **designed** unique authorities, except scripts under
`assumevalid`.

### B. Rules that must not depend only on ConnectBlock

Header, PoW, nBits, timestamp, merkle, basic structure, witness
commitment, weight, nLockTime: enforced at accept /
`TestBlockValidity`. **They are not re-run in ConnectBlock.** That is
the P0 upgrade / `-reindex-chainstate` gap.

### C. Duplicates

| Pair | Same reject? | `DUPLICATE_AUTHORITY` |
|------|--------------|------------------------|
| `CheckBlock` at Accept + Connect | Yes | **FALSE** — check-then-connect, one body |
| `ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout` vs `RigorSupplyDelta` | Yes (`bad-cb-res-amount`) | **TRUE** (dead / unwired) |
| `CreateNewBlock` `ResolveConvergenceDraw` vs `Kernel::ResolveDraw` | Same function body | **FALSE** for acceptance — miner assembly only |
| `LedgerApply` vs ConnectBlock | N/A | **FALSE** — LedgerApply is verify-only (`LEDGER_ACCEPTANCE_AUTHORITY_COUNT=0`) |
| Shadow / canary / migration vs Kernel | Observe-only | **FALSE** |
| Historical `SelectWinner` | Unused | not production |

`DUPLICATE_AUTHORITY_COUNT = 1` (the dead RES-layout helper).

---

## 4. State mutation ordering

**Persisted path (ConnectTip):**

```
stack CCoinsViewCache view(&CoinsTip())
  ConnectBlock
    CheckBlock / draw / per-tx validate
    UpdateCoins          → dirty cache + undo object
    EvaluateConnect      → may still return false
    index memos
    WriteUndo + SetBestBlock
  if ConnectBlock failed: return; view destroyed; CoinsTip unchanged
  if success: view.Flush()
```

**Finding P0-086-CB-06 — `STATE_MUTATION_BEFORE_ACCEPTANCE = TRUE`.**

Inside ConnectBlock the order is **not** validate-all → compute →
commit. It is:

```
validate tx i → UpdateCoins(tx i) → … → EvaluateConnect
```

A Kernel / F-consistency failure happens **after** the cache already
contains the block’s spends and creates. The cache is stack-local, so
a consensus failure does **not** Flush. That is fail-closed for
*persisted* UTXO, and still mutate-then-validate for *in-function*
state.

**Minimal reproduce (no consensus change):**

1. Build a block whose txs spend/create valid UTXOs but whose coinbase
   ORU exceeds `RigorSupplyDelta` (`bad-cb-amount`).
2. `TestBlockValidity` / `ConnectBlock(fJustCheck)` returns false.
3. `CoinsTip().GetBestBlock()` and chain height are unchanged
   (`rigor_connectblock_convergence_audit_tests`).

`fJustCheck` still writes dummy `CBlockIndex` fields on the stack
index used by `TestBlockValidity`; that index is never inserted.

**Other writers**

| Surface | When | On consensus fail |
|---------|------|-------------------|
| Undo file | After Kernel accept, `!fJustCheck` | Not written |
| Observational supply / runtime mirror | After Kernel accept | Not written |
| Mempool `removeForBlock` | ConnectTip after Flush | Not reached |
| Wallet `BlockConnected` | After tip move | Not reached |
| `DisconnectBlock` on FAILED | Comment: view left indeterminate | **P1** |

Undo write failure after Kernel accept (`WriteUndoDataForBlock`
returns false) is a **storage** failure, not a consensus reject, and
can leave a Kernel-accepted block without undo. That is a database
boundary risk, not a second Kernel.

---

## 5. Reorg / DisconnectBlock symmetry

| State | Connect writes | Disconnect restores? |
|-------|----------------|----------------------|
| Non-convergence UTXO | `UpdateCoins` | Yes, via undo |
| Convergence outputs | Not in UTXO (`AddCoins` skip) | Symmetric skip |
| `view.best` | `SetBestBlock(this)` | `SetBestBlock(parent)` |
| Observational W1/W2 | `Observational().Update` | `InvalidateLedgerCaches` |
| Runtime supply mirror/witness | store | invalidate by hash |
| `m_block_res_mint_sum` / `m_block_converge_sum` | yes | **Not cleared** |
| `m_paid_winner_txid` / `m_draw_set` | yes | **Not cleared** (ancestor re-read) |
| `has_res` / `last_res_mint_block_hash` / `m_draw_candidate_root` | yes | **Not cleared** |
| Window contribution cache | `RememberBlockWindowContribution` | hash-keyed; other fork misses |

`CONNECT_DISCONNECT_SYMMETRY = PARTIAL`.

Live unit coverage exists (`mint_supply_reorg_p0_036_tests`,
`connectblock_res_mint_p0_041_tests`,
`connect_disconnect_connect_restores_utxo` in this TASK).

```
LIVE_MINT_GOLDEN_VECTOR = NOT_COVERED
REORGBIND_GOLDEN_VECTOR = NOT_COVERED
```

ReorgBind rows in `phase3_golden_vectors.json` are Python-sigma
scaffold (`coq_enforced: 0`). This TASK does not invent PASS.

---

## 6. Multi-ingress consistency

| Path | Accept authority |
|------|------------------|
| P2P block | `ProcessNewBlock` → ConnectBlock |
| RPC `submitblock` | same |
| GBT / miner template | **not** acceptance; `TestBlockValidity` optional and may skip PoW/merkle |
| Solo `generate*` | `ProcessNewBlock` |
| Pool | `submitblock` → same node path |
| IBD | same ConnectBlock once a block is chosen |
| Reorg | DisconnectBlock + ConnectBlock |
| Regtest | same Kernel; mockable bootstrap RES / no-retarget are chain params |

`CreateNewBlock` calls `ResolveConvergenceDraw` and
`GetBlockSubsidy` to *build* a template. That is miner assembly.
A pool or explorer that “validates” a block locally is
interpretation, not `NODE_ACCEPTANCE_AUTHORITY`.

Regtest shortcuts: `IsTestChain` GBT peer/IBD skip,
`MineBlocksOnDemand`, `-reswalletbootstrap` on mockable chains,
`TestBlockValidity(fCheckPOW=false)`. None of these replace
`EvaluateConnect` on a connected tip.

---

## 7. Consensus Kernel ↔ ConnectBlock boundary

P057 remains: ConnectBlock is orchestration. Production authority is
`RigorConsensusKernel(C++)`. `EXTRACTED_KERNEL_PRODUCTION = false`.

```
PRODUCTION_CONSENSUS_SURFACE              = RigorConsensusKernel(C++) via ConnectBlock
CONSENSUS_KERNEL_AUTHORITY_COUNT          = 1
CONNECTBLOCK_ACCEPTANCE_AUTHORITY_COUNT   = 1
POW_ACCEPTANCE_AUTHORITY_COUNT            = 1   (CheckProofOfWork)
PREDICATE_ACCEPTANCE_AUTHORITY_COUNT      = 1   (VerifyRigorPredicateBoundary)
LEDGER_ACCEPTANCE_AUTHORITY_COUNT         = 0   (LedgerApply verify-only)
SUPPLY_ACCEPTANCE_AUTHORITY_COUNT         = 1
ISSUANCE_ACCEPTANCE_AUTHORITY_COUNT       = 1
RES_ACCEPTANCE_AUTHORITY_COUNT            = 1
DRAW_ACCEPTANCE_AUTHORITY_COUNT           = 1
```

Counts are 1 (or 0) because wrappers forward to a single body.
The dead `ConnectBlockRigor` helper is unused, so it is counted under
`DUPLICATE_AUTHORITY_COUNT`, not under RES.

`MakeKernelInput` is unique. PoW is unique. Supply / issuance / draw
formulas are unique. Shadow / canary / migration do not decide
production acceptance.

---

## 8. Golden vectors

Existing locked hex (`rigor_connectblock_vectors.json`):

1. `valid_height1_coinbase_only` — `ProcessNewBlock` accept
2. `invalid_stale_merkle_extra_tx` — `bad-txnmrklroot`

This TASK adds
`formal_verification/vectors/rigor_connectblock_convergence_catalog.json`
and header hex mutations (merkle xor, nBits=0, truncated). It does
**not** fabricate live-mint or ReorgBind hex.

| # | Case | Status |
|---|------|--------|
| 1 | valid block | VECTOR-LOCKED |
| 2 | invalid merkle | VECTOR-LOCKED + HEX-HEADER |
| 3 | invalid tx | LIVE-MUTATION |
| 4 | invalid coinbase | LIVE-MUTATION |
| 5 | invalid witness | LIVE-MUTATION (P0-085 suite) |
| 6 | invalid predicate | LIVE-MUTATION (this TASK) |
| 7 | invalid PoW | CONDITIONAL-HEX (regtest easy target) |
| 8 | invalid target | HEX-HEADER |
| 9 | invalid timestamp | CONDITIONAL-HEX; authority is contextual, not CB |
| 10 | invalid issuance | KERNEL-ONLY |
| 11 | invalid RES | UNIT-NOT-GOLDEN / LIVE_MINT **NOT_COVERED** |
| 12 | invalid draw | KERNEL-ONLY |
| 13 | invalid supply | KERNEL-ONLY |
| 14 | invalid maturity | LIVE-MUTATION |
| 15 | duplicate tx | LIVE-MUTATION |
| 16 | malformed block | HEX-DECODE |
| 17 | state mutation failure | LIVE-ASSERT |
| 18 | Connect → Disconnect → Connect | LIVE-ASSERT |
| 19 | reorg | PARTIAL / ReorgBind **NOT_COVERED** |
| 20 | invalid after valid | LIVE-ASSERT |

`GOLDEN_VECTOR_STATUS = PARTIAL`.

---

## 9. Mutation audit

Source-level mutants of ConnectBlock gates are killed by
`tools/run_connectblock_convergence_mutation.py` (lint fails if the
token is deleted).

| Mutant | Result |
|--------|--------|
| Delete `CheckBlock` (PoW/merkle gate in CB) | KILLED |
| Delete `bad-txns-BIP30` (coinbase overwrite) | KILLED |
| Delete `CheckTxInputs` (tx / maturity) | KILLED |
| Delete `CheckInputScripts` (predicate) | KILLED |
| Delete `UpdateCoins` (ledger apply) | KILLED |
| Delete `EvaluateConnect` (supply / issuance / RES) | KILLED |
| Delete `ResolveDraw` | KILLED |
| Delete `WriteUndoDataForBlock` (reorg undo) | KILLED |
| Delete `view.SetBestBlock` | KILLED |

`EQUIVALENT_MUTANTS` (logical, not “untested”):

| Mutant | Reason |
|--------|--------|
| `CheckRigorDrawTargetCommitment` | Constant `true`; not on ConnectBlock accept path |
| `ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout` | Unreachable; `RigorSupplyDelta` already enforces the same reject |
| Re-invoke `ContextualCheckBlock` in ConnectBlock | There is no statement to delete; the *absence* is an OPEN finding |

`MUTATION_STATUS = PASS` means the *listed production gates* are
lint-killed. It does not mean ConnectBlock is formally closed.

---

## 10. Fuzz

`fuzz/connectblock_convergence/` models:

* reject reason always present on failure
* `state_mutation_count` stays 0 on reject
* accept → disconnect returns the model to origin

It does **not** instantiate `Chainstate::ConnectBlock` (needs a full
node). Inherited `fuzz/block.cpp` only calls `CheckBlock`.

`FUZZ_STATUS = PARTIAL`.

Live Boost tests enforce the real contract:

```
invalid block → ProcessNewBlock / TestBlockValidity failure
             → height, tip, CoinsTip best-block unchanged

valid tip → InvalidateBlock → ResetBlockFailureFlags → ActivateBestChain
         → UTXO and tip restored
```

---

## 11. Database boundary

```
DATABASE_CONSENSUS_IMPLEMENTATION = INHERITED_BASE_LAYER
DATABASE_CONSENSUS_RISK           = OPEN
```

This TASK does not refactor CoinsDB. Sibling
`RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086` stays
**BLOCKED**: a corrupt Coin value still deserializes as missing, and
missing is treated as consensus-invalid.

ConnectBlock ↔ DB facts:

* Consensus failure does not `Flush` (good).
* Undo is written only after Kernel accept.
* Undo write failure is a storage error after economic accept.
* `DisconnectBlock` FAILED leaves the view indeterminate.
* Cache vs DB divergence after a crash uses `ReplayBlocks` /
  `RollforwardBlock` **without** re-running consensus.
* Restart recovery is rebuild-from-`blk*.dat`, not a second Kernel.

Inherited Bitcoin code is **not** treated as safe by default.

---

## 12. Formal verification coverage

Do not read differential / fuzz / golden as proof.

| Consensus Component | Production Authority | Coq | Refinement | Differential | Fuzz | Golden |
|---------------------|----------------------|-----|------------|--------------|------|--------|
| ConnectBlock | `Chainstate::ConnectBlock` | **NOT-COVERED** (`connectblock_full_proof=false`) | Boundary **PROVED** | PARTIAL | PARTIAL | PARTIAL |
| PoW | `CheckProofOfWork` | NOT-COVERED (math) | — | — | FUZZ-TESTED (boundary) | CONDITIONAL |
| Predicate | `VerifyRigorPredicateBoundary` | MakeKernelInput oracle | PARTIAL | DIFFERENTIAL-TESTED | FUZZ-TESTED | tx vectors |
| LedgerApply | **not production** | verify-only | — | — | — | — |
| Supply | `RigorSupplyDelta` | ConnectBlockSupply slice | PARTIAL | KERNEL | FUZZ-TESTED | KERNEL-ONLY |
| Issuance | `GetBlockSubsidy` via Kernel | freeze | PARTIAL | KERNEL | — | KERNEL-ONLY |
| RES | `ConnectEvaluateCoinbaseResMint` | mint slice | PARTIAL | — | mint fuzz | **NOT_COVERED** live hex |
| Draw | `ResolveConvergenceDraw` | DrawResolverBridge | PARTIAL | — | FUZZ-TESTED | KERNEL-ONLY |
| ASERT | `GetNextWorkASERT` | partial | — | — | — | pin vectors |
| Genesis | genesis authority | freeze | — | — | — | genesis vectors |
| Database boundary | inherited LevelDB | **NOT-COVERED** | — | harness | PARTIAL | sibling audit BLOCKED |

`FORMAL_COVERAGE = PARTIAL`.

---

## 13. Findings

### P0

| ID | Finding |
|----|---------|
| P0-086-CB-01 | `ContextualCheckBlock` / `ContextualCheckBlockHeader` are not re-run in ConnectBlock. New contextual rules will not apply to already-stored / reindex-chainstate connects. |
| P0-086-CB-02 | `RollforwardBlock` mutates UTXO with no consensus checks (crash recovery). |
| P0-086-CB-06 | Per-tx `UpdateCoins` precedes `EvaluateConnect` (`STATE_MUTATION_BEFORE_ACCEPTANCE=TRUE`). Persist is still fail-closed; in-function order is mutate-then-validate. |
| P0-086-CB-07 | Database carrier remains inherited Bitcoin LevelDB with known fail-closed hole (sibling audit BLOCKED). |

### P1

| ID | Finding |
|----|---------|
| P0-086-CB-03 | `-assumevalid` skips `CheckInputScripts`. Mainnet policy disables the flag; the validation branch remains. |
| P0-086-CB-04 | `CheckRigorDrawTargetCommitment` is a no-op stub (`return true`). |
| P0-086-CB-08 | Dead `ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout` is a second RES-layout implementation. |
| P0-086-CB-09 | `DisconnectBlock` does not clear RIGOR `CBlockIndex` memos; FAILED leaves view indeterminate. |
| P0-086-CB-10 | Observational supply invariant failure logs WARNING and does not reject. |
| P0-086-CB-11 | Parent draw I/O failure is `AbortNode`, not `BLOCK_CONSENSUS` (intentional, but it is a fail-stop distinct from reject). |

### P2

| ID | Finding |
|----|---------|
| P0-086-CB-05 | `ConnectBlock(..., fJustCheck)` skips PoW/merkle inside `CheckBlock`; GBT `TestBlockValidity` may also pass `fCheckPOW=false` / `fCheckMerkleRoot=false`. |
| P0-086-CB-12 | RFC-005 comment block claims header/PoW already done by CheckBlock; non-genesis PoW is actually contextual. Comment/code drift. |
| P0-086-CB-13 | Release guardian has no ConnectBlock-specific gate (genesis-focused). |
| P0-086-CB-14 | `IsMockableChain` bootstrap RES can appear in Kernel input on regtest/devtest. |
| P0-086-CB-15 | `ProcessNewBlock` returns true after `AcceptBlock` even if later `ConnectBlock` fails; tip does not move. RPC `submitblock` uses `BlockChecked`, not this bool. |

### Coverage gaps

* Full ConnectBlock Coq proof
* Live mint ConnectBlock hex
* ReorgBind executable undo/view hex
* Competing-fork golden (not only InvalidateBlock)
* libFuzzer on real `ConnectBlock`
* Production `-reindex` of a live datadir
* CompCert/extracted kernel production (explicitly false)

### Authority duplication

* One dead RES-layout helper (`DUPLICATE_AUTHORITY_COUNT=1`)

### Bypass paths (`CONSENSUS_BYPASS_COUNT=5`)

1. Contextual non-reinvoke in ConnectBlock  
2. `RollforwardBlock`  
3. `assumevalid` scripts  
4. `CheckRigorDrawTargetCommitment` no-op  
5. `fJustCheck` / template flags skip PoW/merkle inside CB’s `CheckBlock`

### State mutation / reorg / database risks

See §§4, 5, 11. Persist-on-failure of UTXO: **not observed** on the
ConnectTip success path. In-function mutation before Kernel: **yes**.
Reorg UTXO: **covered by unit tests**. ReorgBind golden: **open**.
DB corruption mapped to consensus-invalid: **open** (sibling audit).

---

## 14. Why this is OPEN

CLOSED would require, at least:

* Contextual rules either re-run on connect or a proved equivalent
  invariant for every upgrade/reindex path
* `RollforwardBlock` either calls ConnectBlock or is formally outside
  the acceptance surface with a fail-closed recovery proof
* `STATE_MUTATION_BEFORE_ACCEPTANCE=FALSE` or a proof that dirty-cache
  discard is equivalent
* ReorgBind + live-mint ConnectBlock goldens
* Database fail-closed ≠ missing-coin
* Full ConnectBlock not claimed as proved while `connectblock_full_proof=false`

None of those were “fixed” here. That is intentional.

```
CONSENSUS_CODE_CHANGED          = FALSE
PROTOCOL_PARAMETERS_CHANGED     = FALSE
FORMAL_THEOREMS_CHANGED         = FALSE
```

---

## Artifacts added by this TASK

| Path | Role |
|------|------|
| `docs/security/RIGOR_CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT_P0_086.md` | This report |
| `formal_verification/connectblock_convergence_audit_manifest.json` | Machine-readable verdict |
| `formal_verification/vectors/rigor_connectblock_convergence_catalog.json` | 20-case honest catalog |
| `tools/lint-connectblock-convergence-audit.py` | Gate / ordering / catalog lint |
| `tools/run_connectblock_convergence_mutation.py` | Source-level gate mutants |
| `tools/run-connectblock-consensus-convergence-audit.sh` | Gate |
| `core/src/test/test_connectblock_convergence_audit.cpp` | Standalone harness |
| `core/src/test/rigor_connectblock_convergence_audit_tests.cpp` | Live Boost tests |
| `fuzz/connectblock_convergence/` | Reject → no-commit model fuzz |

Existing hex pair `rigor_connectblock_vectors.json` is unchanged.
P057 kernel-boundary lint is unchanged and still run by the gate.
