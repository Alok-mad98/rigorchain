# Consensus Kernel Extraction Scope Audit (P0-046)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-EXTRACTION-SCOPE-P0-046`  
**Date:** 2026-08-30  
**Nature:** Static audit + mapping. No production consensus change. No Coq rewrite.  
**Machine inventory:** `formal_verification/consensus_kernel_scope.json`

```
CONSENSUS_KERNEL_SCOPE_STATUS = CLOSED

PRODUCTION_CONSENSUS_MODIFIED = NO
PRODUCTION_PARAMETERS_MODIFIED = NO
W_MODIFIED = NO
T_MODIFIED = NO
RES_SCHEDULE_MODIFIED = NO
ORU_SCHEDULE_MODIFIED = NO
GENESIS_ISSUANCE_MODIFIED = NO
OPUHASH_MODIFIED = NO
ASERT_MODIFIED = NO

COQ_SEMANTICS_MODIFIED = NO
COQ_ADMITTED_ADDED = 0
COQ_AXIOMS_ADDED = 0
COQ_PARAMETERS_ADDED = 0
```

This document answers one question:

> If RIGOR's consensus-critical monetary / asset logic must be lifted out of
> Bitcoin-lineage `validation.cpp` into a kernel small enough to align with Coq,
> what is that kernel?

The answer is **not** "all of `validation.cpp`". It is also **not** "only the
functions that already have Coq names". It is the smallest **correct closure**
of predicates that decide whether a block's monetary and asset effects are
legal.

---

## 0. What RIGOR actually has to prove

The object that later refinement must target is:

```
classify(scriptPubKey) → AssetType
predicate(scriptSig, scriptPubKey, witness, tx) → bool
draw(F, σ, T) → winner | none
issuance(height) → (ORU_subsidy, RES_reward)
supply_delta(block, classified_inputs, draw, issuance) → Δ(S_ORU, S_RES) | reject
pow_digest(header_80) → digest
asert(parent, anchor, params) → nBits
```

Everything else in the node is orchestration: UTXO lookup, block index, disk,
P2P, RPC, wallet. Those are consensus-*supporting*. They must stay outside the
kernel or the kernel becomes another `validation.cpp`.

```
                 CURRENT NODE
                      │
              validation.cpp (7014 LOC)
                      │
        ┌─────────────┴─────────────┐
        │                           │
 consensus-critical          orchestration / state
        │
 ┌──────┴───────────┐
 │                  │
RIGOR Consensus   RIGOR PoW
Kernel            Kernel
(classify,        (digest,
 predicate,        target,
 draw, mint,       ASERT)
 issuance,
 supply slice)
 │                  │
 Coq models         Coq PowDigest
 (structural /      (vector-direct)
  vector-only)
 │                  │
 └────────┬─────────┘
          │
   future refinement
   (flags remain false until
    extraction is linked)
```

---

## 1. Call graph (production, as of this tree)

### 1.1 Block acceptance

```
PeerManagerImpl::ProcessBlock                 net_processing.cpp:3210
  └─ ChainstateManager::ProcessNewBlock       validation.cpp:5178
       ├─ CheckBlock                          validation.cpp:4630
       │    ├─ CheckBlockHeader → CheckProofOfWork
       │    └─ CheckTransaction               consensus/tx_check.cpp:45
       ├─ Chainstate::AcceptBlock             validation.cpp:5093
       │    ├─ AcceptBlockHeader              validation.cpp:4927
       │    │    ├─ CheckBlockHeader
       │    │    └─ ContextualCheckBlockHeader  (ASERT + PoW + MTP)
       │    └─ ContextualCheckBlock           (finality, witness commitment)
       └─ ActivateBestChain → ConnectTip
            └─ Chainstate::ConnectBlock       validation.cpp:2790–3519 (730 LOC)
```

Header-only path (`ProcessNewBlockHeaders`) stops at `AcceptBlockHeader`.
UTXO and monetary effects happen only in `ConnectBlock`.

`ConnectBlock` does **not** re-run `ContextualCheckBlockHeader` /
`ContextualCheckBlock` (noted at `validation.cpp:2808`). That is an
orchestration fact, not a kernel fact: difficulty and MTP are decided
on the header path; ConnectBlock assumes they already passed.

### 1.2 ConnectBlock monetary / asset order

1. Parent draw: `GetConvergenceDrawSetForBlock` → `ResolveConvergenceDraw`
   → `convergence_winner_opt` (mint authorization for *this* block).
2. Own draw set F + C: `BuildConvergenceDrawSet` +
   `ValidateCoinbaseConvergenceRootScriptSig` (commitment for block *h+1*).
3. Per-tx: `CheckTxInputs` (asset domain + fees) → fee-burn share →
   converge C(t) → `CheckInputScripts` → `UpdateCoins`.
4. Live/undo F consistency (C-2).
5. RES mint: `ConnectEvaluateCoinbaseResMint` + winner-coupling gates.
6. Coinbase budgets: ORU cap (`subsidy + fees − burn`); RES exact layout.

### 1.3 Three kernels, not one

| Kernel | Decides | Must not absorb |
|--------|---------|-----------------|
| **Consensus Kernel** | asset type, spend, draw, mint, issuance, supply Δ | UTXO store, P2P, IBD |
| **PoW Kernel** | digest, target, ASERT, `nBits` legality | monetary rules |
| **Transaction Kernel** | sighash domain, witness layout, Ed25519 | draw / issuance |

PoW is consensus-critical. Putting it inside the monetary kernel would
rebuild a giant validation object. Transaction/sighash is also
consensus-critical and already isolated in `predicate_verify.cpp`; it is
a third primitive, not a ConnectBlock helper.

---

## 2. Component evaluation (A–J)

| ID | Component | Consensus-critical? | Same kernel? | Independent primitive? | Coq today | C++ today | Large Bitcoin state? | Pure? |
|----|-----------|---------------------|--------------|------------------------|-----------|-----------|----------------------|-------|
| A | Asset classification | Yes | **Yes — core** | Yes (already a file) | STRUCTURAL + VECTOR_ONLY | `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` | No | Yes |
| B | Spend predicate | Yes | Same *family*, not first cut | Yes | STRUCTURAL + VECTOR_ONLY (sighash) | `VerifyRigorPredicate` | No (needs tx + prevout) | Yes given those |
| C | Draw / selection | Yes | **Yes — core** | Yes | STRUCTURAL / PARTIAL / VECTOR_ONLY | `ResolveConvergenceDraw`, σ, root | Window assembly needs index | Predicate is pure; assembly is not |
| D | Canonical mint | Yes | **Yes — core** | Yes | STRUCTURAL | `ParseResMintScript`, `ConnectEvaluateCoinbaseResMint` | Height + winner | Mostly |
| E | Issuance / subsidy | Yes | **Yes — core** | Yes | STRUCTURAL | `GetBlockSubsidy`, `GetResMintReward` | Height + params | Yes |
| F | ConnectBlock monetary slice | Yes | **Yes — core** | Must become a named function | STRUCTURAL (abstract `apply_delta`) | **inline in ConnectBlock** | Needs classified fees / mint / C(t) | Yes *after* inputs are materialised |
| G | Tx / sighash | Yes | **No** — Transaction Kernel | Yes | VECTOR_ONLY | `SignatureHash` in `predicate_verify.cpp` | No | Yes |
| H | PoW digest | Yes | **No** — PoW Kernel | Yes (already `rigor_kernel/`) | DIRECT on vectors | `GetRigorPowHash` | No | Yes |
| I | Target / nBits | Yes | PoW Kernel | Yes | NO_CORRESPONDENCE | compact decode in `CheckProofOfWork` | No | Yes |
| J | ASERT | Yes | PoW Kernel | Yes | NO_CORRESPONDENCE | `GetNextWorkASERT` | Parent + anchor index | Yes given those times/heights |

**Do not exclude F to make the kernel look small.** The missing named
function *is* the gap. The formulas already exist; they are not a file.

---

## 3. State boundary

### Pure inputs (kernel may take these)

`scriptPubKey`; serialized tx; height; consensus params (`W`, `T`, subsidy
base, epoch length); draw set F (list of `{txid, weight}`); σ seed;
`header_80`; parent/anchor `(nTime, nHeight, nBits)`; classified
prevouts `{scriptPubKey, value}` (already fetched).

### Derived values (kernel produces these)

`AssetType` / `TxoutType`; predicate bool; winner txid or none; ORU
subsidy; RES reward; fee; burn share; C(t); coinbase ORU/RES caps;
`pow_digest`; next `nBits`.

### External state (kernel must not own)

UTXO set (`CCoinsViewCache`); chain tip / `CBlockIndex` graph; blk/rev
undo; mempool; wallet; RPC; P2P peer state; observational supply
monitor (`CaptureRuntimeSupplyState` — **not a reject predicate**).

### Can the kernel be `Kernel(Input) → Result`?

**The predicates can. ConnectBlock as a whole cannot.**

| Object | Form |
|--------|------|
| `classify` | `Kernel(scriptPubKey) → AssetType` |
| `issuance` | `Kernel(height, params) → (ORU, RES)` |
| `pow_digest` | `Kernel(header_80) → digest` |
| `draw` | `Kernel(F, σ, T) → winner` |
| `asert` | `Kernel(parent, anchor, params) → nBits` |
| `supply_delta` | `Kernel(Input, MinimalState) → Δ \| reject` |
| `ConnectBlock` | orchestration over the above + UTXO apply |

**Blocker for a fully pure ConnectBlock:** constructing F requires walking
the last W blocks' contributions (index cache or undo). That walk is
*state access*. Once F is in hand, `ResolveConvergenceDraw` is pure.
P047 must cut *there*, not pretend the walk is part of the math.

`MinimalState` for `supply_delta` is: height, classified input/output
bags, winner option, computed subsidy/reward, fee, burn, C(t), coinbase
legs. No `CCoinsViewCache` inside the function.

---

## 4. Second matcher

```
SECOND_MATCHER_ASSET_TYPE     = NO
SECOND_MATCHER_SPEND_DISPATCH = YES
ClassifyRigorSpendPredicate   = DOES_NOT_EXIST (docs-only name)
```

**Asset type** has one production path:

```
scriptPubKey
  → ClassifyRigorTxout          script/rigor_classify.cpp:112
  → AssetTypeFromScriptPubKey   script/rigor_classify.cpp:154
  → {ORU, RES, NONE, INVALID}
```

`Solver`, `LookupRigorPredicate`, `IsResScript`, `IsAllowedScript` all
delegate. There is no second asset enum.

**Spend dispatch** re-matches templates in `VerifyRigorPredicate`
(`predicate_verify.cpp:420`) via `IsResMintScript` / `IsWitnessProgram`
instead of `LookupRigorPredicate`. On today's valid UTXO set the two
cannot disagree (fail-closed on shapes `CheckTransaction` already
rejects). They are still two parsers of one concept.

| Shape | Classifier | Spend matcher | On-chain split today? |
|-------|------------|---------------|------------------------|
| v0 / 20 (P2WPKH) | `WITNESS_V0_KEYHASH` → ORU | witness v0 path | No |
| v0 / ≠20 (incl. P2WSH) | `NONSTANDARD` → INVALID | `WITNESS_PROGRAM_WRONG_LENGTH` | No (tx rejected earlier) |
| v1 / 32 native RES | `WITNESS_V1_RES` → RES | witness v1 path | No |
| v1 / 32 CVG2 or RES2 | `NONSTANDARD` | `DISCOURAGE_UPGRADABLE_WITNESS` | No |
| unknown witness v2+ | `NONSTANDARD` | `RIGOR_SCRIPT_TYPE` | No |
| RES_MINT canonical | `RES_MINT` → RES | `VerifyResMintSpend` | No |
| CONVERGE / OP_RETURN | classified, unspendable | `RIGOR_SCRIPT_TYPE` | N/A |
| P2SH / P2PKH / Taproot / bare pk | `NONSTANDARD` | rejected | No |

**P046 does not fix this.** P047/P048 recommendation: dispatch spend
through `LookupRigorPredicate(ClassifyRigorTxout(...))`. That is a
convergence of *matchers*, not a protocol change, but it is still a
behavioural edit and must not ride inside a mechanical extraction.

Bitcoin `VerifyScript` / `interpreter.cpp` are **absent**. Consensus
spend is Ed25519 + push-only parsers only.

---

## 5. PoW / difficulty boundary (separate kernel)

Live path on this tree:

```
header_80
  → rigor_kernel::GetRigorPowHash     SHA3-256("RGRPOW01" ‖ header)
  → UintToArith256(digest) ≤ target(nBits)
  → CheckProofOfWork                  pow.cpp:404
```

`GetOpusHash*` (`pow_opus.cpp`) is **not** called from `CheckProofOfWork`.
It remains in the tree (miner/pool still compile against it). Coq
`PowDigest.v` models the live SHA3 construction, not OpuHash.

| Piece | File | In PoW Kernel? | In Monetary Kernel? |
|-------|------|----------------|---------------------|
| Keccak-f[1600] / SHA3-256 short | `rigor_kernel/keccak.cpp` | Yes | No |
| Domain tag + preimage | `rigor_kernel/pow_digest.cpp` | Yes | No |
| Compact target decode / `powLimit` | `CheckProofOfWork` | Yes | No |
| Genesis PoW exemption | `CheckProofOfWork` | Yes (policy of the predicate) | No |
| ASERT (`GetNextWorkASERT`) | `pow.cpp:254` | Yes | No |
| `PermittedDifficultyTransition` | `pow.cpp:358` | Yes | No |
| Header sync / `nMinimumChainWork` | `headerssync.cpp` | Support | No |
| OpuHash leftover | `pow_opus.cpp` | Dead on node path | No |

ASERT inputs are parent/anchor *times and heights*, not hashes. That is
why the startup-time audit is hash-independent. Still: ASERT belongs
with PoW, not with issuance.

---

## 6. C++ ↔ Coq correspondence

Honesty flags in production Coq remain `false`. This table is **not**
a refinement claim.

| C++ symbol | Coq | Grade | Describes production? |
|------------|-----|-------|------------------------|
| `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` | `rigor_v01/AssetClassification.v`, `ClassifierRefinementBoundary.v`, `VectorAgreement.v` | STRUCTURAL + VECTOR_ONLY | Intent, unproved ∀ |
| `GetBlockSubsidy` | `IssuanceHeightBoundary.v`, `GenesisZeroIssuance.v` | STRUCTURAL | Yes (schedule) |
| `GetResMintReward` / `ResRewardForEpoch` | `coq/ResSchedule.v` | STRUCTURAL | Yes (schedule) |
| ConnectBlock monetary formulas | `rigor_v01/ConnectBlockSupply.v`, `MintSupplyConservation.apply_delta` | STRUCTURAL | Abstract slice; `cpp_refinement_proved = false` |
| UTXO replay supply | `ReplayReorgCore/StateTransition.v` | STRUCTURAL | Separate replay model |
| `ResolveConvergenceDraw` / inequality | `legacy_account_model/Selection.v`, `Kernel/*`, `WindowExclusion.v` | PARTIAL + VECTOR_ONLY | Constants match; state machine is list-of-tx, not UTXO F |
| `ComputeSigmaHash` | `RigorSha256.v` + Kernel golden harness | VECTOR_ONLY | Digest bytes, not full draw |
| `VerifyRigorPredicate` / `SignatureHash` | `SighashPreimage.v`, `SighashByteCorrespondence.v` | VECTOR_ONLY | 3 golden preimages |
| `GetRigorPowHash` | `PowDigest.v`, `RigorKeccak.v` | DIRECT on concrete vectors | Executable model; not linked |
| Compact nBits / ASERT | — | NO_CORRESPONDENCE | — |
| `GetOpusHash*` | — | NO_CORRESPONDENCE | Superseded on node path |
| Phase5 `*Equivalence.v` | Phase5Equivalence/* | DOCUMENTED_ONLY | Does not constrain C++ |
| `legacy_account_model/ConnectBlockScaffold.v` | same | NO_CORRESPONDENCE | Account model |

**Grade meaning**

- DIRECT: same function, executable on the same bytes (still not a ∀ proof).
- STRUCTURAL: same decision tree / formula; no `⟦C++⟧ = Coq` theorem.
- PARTIAL: some subpredicates match; the C++ closure is larger.
- VECTOR_ONLY: finite locked inputs.
- DOCUMENTED_ONLY: comments / catalogs.
- NO_CORRESPONDENCE: no Coq object for this C++ symbol.

---

## 7. Phase5Equivalence (audit only)

**89** modules under `formal_verification/coq/Phase5Equivalence/`.
Not in `_CoqProject.protocol-core` load-bearing cone as *constraints on
C++*; they are observational catalogs. They do **not** constrain
production consensus.

| Class | Count | Role |
|-------|------:|------|
| SEMANTIC_CONSTRAINT | 25 | Real encoding / projection math (`SigmaIntegerRefinement`, `UInt512OverflowRefinement`, `FSetMembershipRefinement`, draw/replay/reorg projection chains, canonical hash bridges) |
| BOOKKEEPING | 43 | `*Audit.v` / `*Catalog.v` status rows, `negb equivalence_claimed` |
| VACUOUS_PLACEHOLDER | 21 | `Definition … : Prop := True` then `exact I` |

All `equivalence_claimed` / `refinement_proved` definitions in this tree
are `false`. Many Phase5 lemmas *prove the flag stays false*. That is
honesty enforcement, not equivalence.

**Do they constrain production C++?** No.  
**Refinement targets?** Only the 25 semantic modules, and only *after*
a kernel exists to refine against.  
**P048 cleanup (do not delete in P046):** vacuous `True`/`exact I`
scaffolds; duplicate audit/catalog pairs that only replay the false
flag; freeze/terminal catalogs.  
**Keep even if weak:** `DrawBridgeProgress.v` (honest inventory),
`AntiEquivalenceSafety.v`, `DrawClosureImpossibilityCatalog.v`,
`PD4FCPP01NonClosableCatalog.v`.

---

## 8. `legacy_account_model`

```
BUILD_TIER = DEFAULT_BUILD
```

Listed in both `_CoqProject` and `_CoqProject.protocol-core` (Types,
Selection, State, ConnectBlockScaffold, Transition). CI compiles it
via `run_protocol_core_coq.sh`.

It **does not constrain the UTXO consensus kernel**. The module header
says so: selection over a list-of-tx `Block`, not the C++ draw set.
It remains because `Kernel/` / `Proofs/` / `Execution.v` still import
it.

**P048 (not P046):** stop citing it in UTXO security claims; migrate
those imports to `ReplayReorgCore` + golden harness; then move it to
DEAD_BUILD. Do not delete in P046 or P047.

---

## 9. Scale

LOC is an aid, not a target. Counted with `wc -l` on this tree.

```
CURRENT_VALIDATION_LOC              = 7014    (validation.cpp)
CURRENT_CONSENSUS_SURFACE_LOC       = 10541   (validation.cpp
                                               + classify/predicate
                                               + consensus/* listed below
                                               + connect_block_rigor_checks
                                               + pow.cpp + rigor_kernel
                                               + pow_opus leftover)
```

Consensus files included in the surface count (3527 without
`validation.cpp`):

`rigor_classify.cpp` 174, `predicate_verify.cpp` 495, `tx_check.cpp` 150,
`tx_verify.cpp` 426, `convergence_logic.cpp` 469, `convergence_root.cpp` 256,
`convergence_root_eligibility.cpp` 101, `convergenceres_verify.cpp` 45,
`convergenceres.cpp` 153, `res_owner.cpp` 43, `rigor_res.cpp` 97,
`rigor_consensus_lock.h` 395, `connect_block_rigor_checks.cpp` 32,
`pow.cpp` 456, `keccak.cpp` 101, `pow_digest.cpp` 47, `pow_opus.cpp` 87.

```
PROPOSED_CORE_KERNEL_LOC            = ~1650
    classify                          174
    lock-header issuance / W,T,fee    ~180 of 395
    GetBlockSubsidy                   35 (today inside validation.cpp)
    draw predicate (logic+math+σ+root)~400
    mint + owner + coinbase layout    172
    tx_check + CheckTxInputs subset   ~350
    monetary slice (today inline)     ~150
    (does NOT include predicate_verify 495 — Transaction Kernel)
    (does NOT include window walk / undo assembly)

PROPOSED_POW_KERNEL_LOC             = ~450
    digest (already extracted)        148
    CheckProofOfWork + compact        ~50
    ASERT + clamp + Permitted         ~250
    (pow_opus.cpp 87 is leftover, not in the proposed kernel)
```

| Metric | Core Kernel | PoW Kernel |
|--------|------------:|-----------:|
| Named functions (inventory) | 28 | 8 |
| Structs the kernel *defines* | 3 (`AssetType`, draw member, supply Δ) | 0 (uses header / compact) |
| External deps | `CScript`, `CTransaction` view of prevouts, params | `CBlockHeader`, `arith_uint256`, params |
| Global-state deps | **0** if F and prevouts are arguments | **0** if parent/anchor fields are arguments |
| Coq correspondences | 7 STRUCTURAL/VECTOR, 0 closed refinement | 1 DIRECT-on-vectors (`PowDigest`) |

`validation.cpp` after a successful extraction should be:

```
orchestration
  + UTXO access
  + block-index / undo access
  + error strings
  + network-facing glue
```

not `consensus logic + orchestration + leftover Bitcoin`.

---

## 10. Candidate layout (not created)

```
core/src/rigor_kernel/          # already exists for PoW digest
    keccak.*                    # PoW Kernel (landed)
    pow_digest.*                # PoW Kernel (landed)
    classify.*                  # P047: move from script/rigor_classify.cpp
    issuance.*                  # P047: GetBlockSubsidy + GetResMintReward façade
    draw.*                      # P047: ResolveConvergenceDraw + inequality + σ
    mint.*                      # later: ConnectEvaluateCoinbaseResMint
    supply_delta.*              # later: named ConnectBlock monetary slice

core/src/pow.cpp                # stays: CheckProofOfWork + ASERT orchestration
core/src/validation.cpp         # stays: state, UTXO, ConnectBlock driver
core/src/script/predicate_verify.cpp   # Transaction Kernel (not P047)
```

This is a **target shape**, not a move list for this task.

---

## 11. P047 / P048 / refinement target

### P047 MUST EXTRACT

Mechanical, no semantic change, already-pure or already-isolated:

1. **`GetBlockSubsidy`** — lift out of `validation.cpp:1678` into
   `consensus/` (or `rigor_kernel/issuance`). Call sites unchanged.
2. **Named monetary slice** — the ConnectBlock formulas at
   `validation.cpp:3201–3210` (burn), `3225–3252` (C(t)),
   `3369–3386` (coinbase caps), `3389` (RES mint sum), plus
   `VerifyCoinbaseConvergenceAndResLayout`. Same arithmetic, new
   function name, ConnectBlock becomes the caller.
3. **Draw predicate façade** — `ResolveConvergenceDraw` +
   `ConvergenceDrawInequalityHolds` + `ComputeSigmaHash` called through
   one header. Do **not** move window assembly / undo walks.
4. **Classification façade** — already one file; give it a kernel
   header so ConnectBlock / `tx_check` / `tx_verify` include one
   place. Do not rewrite the matcher.

### P047 SHOULD EXTRACT LATER (not first cut)

- `ConnectEvaluateCoinbaseResMint` + winner gates (needs the slice API
  to exist first).
- `CheckTxInputs` asset-domain core (keep Bitcoin-era fee accounting
  in `tx_verify.cpp` until the slice is named).
- Spend-dispatch unification (`VerifyRigorPredicate` →
  `LookupRigorPredicate`). Behavioural risk; separate change.
- ASERT out of `pow.cpp` into `rigor_kernel/asert` (PoW Kernel, not
  monetary).

### P047 MUST NOT EXTRACT

- Entire `validation.cpp` / `ConnectBlock` / `ProcessNewBlock`
- UTXO (`CCoinsViewCache`), undo, blk files
- Window tail assembly (`BuildConvergenceWindowTail`,
  `GetConvergenceDrawSetForBlock`)
- P2P, mempool, relay, RPC, wallet, miner, pool
- `predicate_verify.cpp` (Transaction Kernel)
- PoW digest (already extracted; do not fold into monetary kernel)
- `pow_opus.cpp` deletion (miner/pool still depend)
- Phase5Equivalence, `legacy_account_model`
- Any W/T/schedule/genesis/PoW parameter

### P048 CLEANUP SET (docs/Coq only)

- Vacuous Phase5 `True`/`exact I` modules (21)
- Duplicate Phase5 audit/catalog pairs that only restate false flags
- Stop citing `legacy_account_model` as UTXO evidence; plan DEAD_BUILD
- Record spend-dispatch second matcher as a known dual parser
- Do **not** flip `equivalence_claimed`

### REFINEMENT_TARGET

```
REFINEMENT_TARGET =
    rigor_kernel::classify          ↔  AssetClassification.classify
    rigor_kernel::issuance          ↔  IssuanceHeightBoundary + ResSchedule
    rigor_kernel::draw              ↔  (future UTXO draw module; NOT Selection.v)
    rigor_kernel::supply_delta      ↔  MintSupplyConservation.apply_delta
    rigor_kernel::GetRigorPowHash   ↔  PowDigest.rigor_pow_digest
```

First refinement *pilot* remains **PoW digest**: it is already a pure
88-byte function with a Coq executable model and three-implementation
vectors. That does **not** make `equivalence_claimed` true. It makes
the differential gate the right next tool (P1 extraction of
`PowDigest.v` → OCaml), which is outside P047's monetary first cut.

Do not treat Phase5 catalogs or `legacy_account_model/Selection.v` as
the refinement target for UTXO ConnectBlock.

---

## 12. Dependency cut

After P047's first cut, `ConnectBlock` should read:

```
F        = assemble_window(...)          // stays in validation (state)
winner   = kernel.draw(F, σ, T)          // extracted
classify = kernel.classify               // extracted
Δ        = kernel.supply_delta(...)      // extracted
view     = UpdateCoins(...)              // stays (UTXO)
```

`CheckProofOfWork` already reads:

```
digest = rigor_kernel::GetRigorPowHash(header)   // extracted
ok     = compact_ok(nBits) && digest ≤ target    // stays in pow.cpp
```

That is the cut. If a later PR cannot point to these five arrows, it
is not kernel extraction; it is a file move.

---

## 13. Final names

```
PROPOSED_RIGOR_CONSENSUS_KERNEL =
    classify
    + issuance (ORU subsidy, RES reward)
    + draw predicate (F, σ, T → winner)
    + mint / owner / coinbase layout checks
    + named supply_delta (burn, C(t), coinbase caps)

PROPOSED_RIGOR_POW_KERNEL =
    GetRigorPowHash
    + compact target / powLimit
    + CheckProofOfWork predicate
    + ASERT / PermittedDifficultyTransition

PROPOSED_RIGOR_TRANSACTION_KERNEL =   # not P047
    VerifyRigorPredicate
    + SignatureHash
    + Ed25519

P047_FIRST_EXTRACTION_SET =
    GetBlockSubsidy
    + named ConnectBlock monetary slice
    + draw predicate façade
    + classification façade (header only)

P048_CLEANUP_SET =
    Phase5 vacuous placeholders
    + legacy_account_model citation / build-tier plan
    + spend-dispatch second matcher (document / later unify)

REFINEMENT_TARGET =
    classify, issuance, draw, supply_delta, pow_digest
    (pilot = pow_digest vectors; flags stay false)
```

---

## 14. Safety

This task created two files and nothing else. It did not edit
`validation.cpp`, `pow.cpp`, Coq modules, miner, pool, wallet, or
parameters. Existing working-tree PoW work (SPEC-008 SHA3 kernel)
is **observed**, not modified.

`CONSENSUS_KERNEL_SCOPE_STATUS = CLOSED` because the object to prove
is now named, the three kernels are cut apart, the state boundary is
stated, C++↔Coq grades are listed, Phase5/legacy/second-matcher are
classified, and P047 has a first extraction set that is smaller than
`validation.cpp` and larger than a slogan.
