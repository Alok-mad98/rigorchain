# RIGOR Single-Path CONVERGE Architecture — Implementation Security Audit

**Task:** `TASK-RIGOR-CONVERGE-SINGLE-PATH-IMPLEMENTATION-SECURITY-AUDIT-P0-010`
**Date:** 2026-08-23
**Type:** Implementation security audit + formal verification consistency review
**Layer:** Consensus, script, mining template, mining/pool software, Coq model
**`equivalence_claimed`:** `false`

**Consensus status at time of audit:**

```
mainnet_launched              = false
historical_consensus_burden   = none
legacy_compatibility_constraint = none
```

No historical chain state constrains this review. Every finding below may be
acted on without a protocol upgrade path.

---

## 1. Audit scope

This audit verifies that the shipped implementation realises the intended
single-path architecture:

```
ORU burn  →  ticket weight f_i  →  σ < T · f_i  →  next-block RES mint
```

and that it does so without global state, without a second ticket
representation, and without an alternative issuance path.

### 1.1 Code read

| Area | Files |
|---|---|
| Ticket script | `core/src/script/script.h`, `core/src/script/interpreter.cpp`, `core/src/script/standard.cpp`, `core/src/consensus/res_owner.cpp` |
| Set / root | `core/src/consensus/convergence_root.cpp`, `core/src/consensus/convergence_root_eligibility.cpp`, `core/src/consensus/convergence_logic.cpp` |
| σ / selection | `core/src/consensus_rigor/sigma_hash.cpp`, `core/src/consensus/rigor_math.h` |
| Mint gate | `core/src/consensus/rigor_res.cpp`, `core/src/connect_block_rigor_checks.cpp`, `core/src/consensus/tx_verify.cpp`, `core/src/consensus/tx_check.cpp` |
| Chain layer | `core/src/validation.cpp`, `core/src/chain.h`, `core/src/coins.cpp` |
| Constants | `core/src/consensus/rigor_consensus_lock.h`, `core/src/consensus/params.h` |
| Template / GBT | `core/src/node/miner.cpp`, `core/src/rpc/mining.cpp`, `core/src/node/auto_res_mint.cpp` |
| Formal model | `formal_verification/coq/**` (222 `.v` files) |
| Explorer | `explorer/src/db.ts`, `explorer/src/script.ts` |

### 1.2 Out of scope

Economic parameter selection (`W(h)` schedule, `RES_PROBABILITY_TARGET`
magnitude), PoW difficulty adjustment, P2P layer, wallet UX.

---

## 2. Current architecture summary

### 2.1 The single path, as implemented

```
1.  ORU tx creates ONE output:  <20-byte HASH160> OP_CONVERGE     (22 bytes)
      ├─ value nValue = f_i (weight, satoshis, ≥ 1 ORU)
      ├─ owner  = the HASH160 in the script — nothing else
      └─ AddCoins skips it: the value is DESTROYED, not locked

2.  Block h-1 commits F over the 32-block window in its coinbase scriptSig:
      scriptSig = <BIP34 height> <OP_PUSHBYTES_32 C> <OP_PUSHBYTES_32 paid-winner>  — exactly three pushes
      (P0-014 corrected an earlier two-push description; consensus has required the paid-winner push since RIG-05.)

3.  Block h resolves the draw committed by h-1:
      σ_i = Hash256( txid_i ‖ C ‖ B_{h-1} ‖ LE32(h) )      — 100 bytes exactly
      eligible_i  ⟺  σ_i < T · f_i                          — 512-bit, no division
      winner      = argmin_i σ_i/f_i by cross-multiplication, txid tie-break

4.  Block h pays it in ONE place:
      coinbase output  <h+100> <winner.HASH160> OP_RES_MINT  at W(h)
```

### 2.2 Verified constants

| Constant | Value | Location |
|---|---|---|
| `MIN_CONVERGENCE_AMOUNT` | `COIN` (1 ORU) | `rigor_consensus_lock.h:99` |
| `CONVERGENCE_DRAW_WINDOW` | `32` | `rigor_consensus_lock.h:135` |
| `CONVERGENCE_DRAW_WINDOW_LOG2` | `5` | `rigor_consensus_lock.h:138` |
| `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` | `1` | `rigor_consensus_lock.h:102` |
| `RES_PROBABILITY_TARGET` | `floor(2²⁴⁶ / 1e8) >> 5` | `params.h:104` |
| `OP_CONVERGE` | `0xbd` | `script.h:232` |
| `OP_RES_MINT` | `0xbb` | `script.h:220` |
| `OP_CVG_CLAIM` | `0xbc` **burned** | `script.h:222-227` |

### 2.3 Old vs new

| Item | Old | New (verified) |
|---|---|---|
| Ticket representation | CVG2 witness v1 + RESO sibling | `OP_CONVERGE` script, 22 bytes |
| Ownership | multi-object declaration | HASH160 inside the script |
| Mint path | covenant / PCORB / body self-proof | coinbase `OP_RES_MINT` only |
| Coinbase commitments | scriptSig C **+** CVGE `OP_RETURN` | scriptSig C only |
| Draw window | 1 block (producer-exclusive) | 32 blocks |
| Global state in mint decision | CVGE cumulative vs `GetChainConvergeSum` | none |

---

## 3. Code findings

### 3.1 PASS — RES issuance has exactly one path

Three independent gates, all verified:

**Gate 1 — no body transaction can create RES.** `Consensus::CheckTxInputs`:

```391:399:core/src/consensus/tx_verify.cpp
        if (out_asset == ASSET_RES) {
            if (!has_res_in) {
                // No body transaction creates RES. There is no ORU-funded mint shape to make an
                // exception for: the only new RES in a block is in its coinbase, authorized by
                // the parent's resolved draw.
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "mixed-asset-output");
            }
            continue;
        }
```

**Gate 2 — the coinbase carries at most one mint, bound to the parent's winner.**
`rigor::ConnectEvaluateCoinbaseResMint` (`consensus/rigor_res.cpp:50-91`) scans
only `coinbase.vout`, rejects a second `OP_RES_MINT` output
(`bad-rigor-res-amount`), pins the maturity height to `h + nCoinbaseMaturity`,
requires `parent_draw_set.source_height + 1 == nBlockHeight`, requires a winner,
and requires `payout->nValue == GetResMintReward(h)` exactly.

**Gate 3 — the coinbase's total RES is pinned, so no extra RES can ride along.**

```29:36:core/src/connect_block_rigor_checks.cpp
    const CAmount res_bootstrap_coinbase =
        params.IsMockableChain() ? params.GetConsensus().res_wallet_bootstrap_coinbase : 0;
    const CAmount expected_res_coinbase = nFeesRes + res_bootstrap_coinbase + res_mint_coinbase;
    if (res_coinbase_value != expected_res_coinbase) {
```

With `Assert(nFeesRes == 0)` (`validation.cpp:3177`) and
`res_bootstrap_coinbase == 0` off mockable chains, this reduces to
**`res_coinbase_value == res_mint_coinbase`** — an exact equality, not a cap.
There is no room for a hidden subsidy output.

Coupling rules are complete in both directions (`validation.cpp:3138-3154`):
winner ∧ `W(h)>0` ⇒ mint required; no winner ⇒ mint forbidden; `W(h)=0` ⇒ mint
forbidden; `|F| < 1` ⇒ mint forbidden.

**Verdict: PASS.** No PCORB mint, no CVGE mint, no RIGR mint, no
transaction-level RES creation, no hidden subsidy path.

### 3.2 PASS — CONVERGE ownership exists only in the script

The lock is fixed-length and parsed positionally, with no optional fields:

```98:105:core/src/script/interpreter.cpp
bool ParseConvergeScript(const CScript& script, uint160& owner_hash160)
{
    if (script.size() != kConvergeScriptSize) return false;
    if (script[0] != 20) return false;
    if (script[21] != OP_CONVERGE) return false;
    std::copy(script.begin() + 1, script.begin() + 21, owner_hash160.begin());
    return true;
}
```

The owner declaration is the script and nothing else
(`consensus/res_owner.cpp:16-27`), and the payout check compares that HASH160
directly against the `OP_RES_MINT` pubkey hash
(`consensus/res_owner.cpp:34-41`). There is no account table, no registry, no
CVGE object store, and no sibling `OP_RETURN`.

Unspendability is enforced twice — belt and braces:

* `script/interpreter.cpp:1230-1232` — `OP_CONVERGE` always returns
  `SCRIPT_ERR_CONVERGE_UNSPENDABLE`.
* `coins.cpp:130-132` — `AddCoins` skips the output entirely, so it never
  enters the UTXO set.

Exactly one positive-value `OP_CONVERGE` output per transaction is allowed
(`tx_verify.cpp:250-253`, `invalid-convergence-structure`); a transaction with
two is rejected outright, and `GetResOwnerDeclaration` independently returns
`nullopt` for an ambiguous declaration, so an ambiguous ticket can never enter
**F**. **No hidden second output remains, and no parallel ticket
representation is reachable on-chain.**

### 3.3 PASS — one C commitment root

```81:98:core/src/consensus/convergence_root_eligibility.cpp
    // RFC-008 §10.2 / RFC-009 §3.4: empty F commits to 32 zero bytes. A singleton is hashed.
    if (convergence_txids.empty()) {
        return uint256();
    }
    ...
    std::sort(convergence_txids.begin(), convergence_txids.end());
    ...
    CHash256 hasher;
    for (const uint256& id : convergence_txids) {
        hasher.Write(MakeUCharSpan(id));
    }
```

| Property | Status | Note |
|---|---|---|
| Empty set → `C = 0` | PASS | returned **before** hashing; a 32-zero `Hash256` output is computationally unreachable, so the sentinel is unambiguous |
| `n ≥ 1` aggregation | PASS | singleton hashes normally; the old `n < 2` boundary is gone |
| Ordering | PASS | `std::sort` on `uint256` = unsigned byte-wise `memcmp` order |
| Serialization | PASS | raw 32-byte memory order, no `SerializeHash`, no endian swap |
| Collision resistance | PASS | fixed-width elements ⇒ concatenation is unambiguous without a length prefix |
| Same inputs ⇒ same C | PASS | pure function of a `std::vector<uint256>`; no platform-dependent input |

Duplicate handling is discussed under **V-8** — currently unreachable, but
unasserted.

### 3.4 PASS — σ has exactly one byte layout

`consensus_rigor/sigma_hash.cpp` builds a `std::array<unsigned char, 100>` with
offsets pinned by `static_assert` (lines 21-26) and writes the height with
explicit little-endian shifts (lines 72-75) rather than relying on host
endianness. There is no serializer, no length prefix, and no delimiter.

**Endian mismatch: not reachable. Hash ambiguity: not reachable.**

### 3.5 PASS — selection arithmetic is exact and overflow-free

Eligibility (`consensus/rigor_math.h:189-213`) rejects `f < 1` and
`!MoneyRange(f)` before multiplying, then computes `T · f` via
`Mul256x256Full` into a `uint512`. Because eligibility gates the argmin, every
weight entering the winner comparison is in `[1, MAX_MONEY]`, so
`H_i · f_j < 2²⁵⁶ · 2⁵¹` cannot overflow 512 bits.

The argmin (`convergence_logic.cpp:92-108`) uses the same 512-bit
cross-multiplication with an exact `memcmp` txid tie-break. No division, no
floating point, no truncation to 256 bits.

`Mul256x256Full` carries via `unsigned __int128` with an `assert(k < 8)` guard
on carry propagation (`rigor_math.h:65-71`), and `NDEBUG` builds are refused
outright (`util/check.h:49-51`), so that guard is live in every shipped binary.

### 3.6 PASS — no global state in the mint decision

`ConnectEvaluateCoinbaseResMint` reads only: the coinbase transaction, the
block height, `Consensus::Params`, and the parent draw set (itself derived from
block + undo data). It reads no accumulator, no index, and no database.

The chain-cumulative fields are present but provably outside the validation
predicate:

* `CBlockIndex::m_block_converge_sum` / `GetChainConvergeSum()` —
  labelled `NON-CONSENSUS observational` (`chain.h:224-232`); grep confirms the
  only readers are `observational_metrics.cpp`, `runtime_supply_*`,
  `state_snapshot`, and RPC.
* The CVGE `OP_RETURN` cumulative commitment is **no longer parsed by
  `validation.cpp` at all** — `ParseConvergenceCommitmentPayload` and
  `FindLastConvergenceCommitmentInCoinbase` have no consensus caller. The
  `bad-convergence-cumulative` reject no longer exists.
* `m_draw_candidate_root` is written locally at `validation.cpp:3210` but is
  **not** compared against anything the block producer supplied — the coinbase
  no longer carries it. It is a local recomputation for RPC and
  `state_snapshot`, not a consensus commitment.

**Verdict: PASS.** Consensus calculation is `block + transaction + UTXO +
deterministic rules` only.

---

## 4. Formal verification findings

### 4.1 Axiom and admit surface — clean

A full scan of all 222 `.v` files under `formal_verification/coq/`:

```
Axiom     : 0 occurrences (2 textual matches, both inside comments asserting absence)
Admitted  : 0 occurrences (7 textual matches, all inside comments)
admit.    : 0
```

**PASS: no new axioms are required, and no proof is left `Admitted`.**

### 4.2 The real gap is named, not hidden — three open obligations

Three theorems that were previously vacuous placeholders
(`Theorem ... : True. Admitted.`) have been rewritten as **stated but
undischarged `Prop` definitions**, with the gap documented in-line. This is the
correct engineering choice — the gap is now visible to a reader instead of
being laundered through a trivially-true `Admitted` theorem — but the audit
must record that the gap itself remains:

| Obligation | Location | Status |
|---|---|---|
| `bridge_cpp_equivalence_obligation` | `Phase4FConsensus/ConsensusOracleBridge.v:159` | **OPEN** (PD-4F-OB-02) — no C++ oracle is wired |
| `oracle_agreement_obligation` | `Phase4FConsensus/ConsensusDeterminism.v:69` | **OPEN** |
| `registry_oracle_bridge_obligation` | `Phase4FConsensus/ConsensusTraceExecution.v:111` | **OPEN** |

Each is a bare `Definition ... : Prop`; nothing in the tree proves any of them.
What *is* proved is the fail-closed direction — with no oracle present the
bridge can never report a match:

```169:176:formal_verification/coq/Phase4FConsensus/ConsensusOracleBridge.v
Theorem bridge_cpp_equivalence_placeholder :
  forall r : BridgeRow,
    (br_oracle r).(oo_present) = false ->
    status_of_agreement (row_agreement r) = BS_Blocked.
Proof.
  intros r Habs. unfold row_agreement.
  apply absent_oracle_not_match. exact Habs.
Qed.
```

**Consequence for the acceptance criterion "Coq model matches C++ behavior":
this is NOT proved. It is asserted by golden-vector agreement and by C++ unit
tests, not by a machine-checked refinement theorem.** The audit records this as
an honest, correctly-scoped limitation rather than a defect — but the claim
must not be overstated in public material.

### 4.3 The draw itself IS modelled at byte level — PASS

Contrary to what an "abstract block" model would imply, the byte pipeline is
modelled:

`ConvergeRoot.v:11-18` matches `convergence_root_eligibility.cpp` exactly —
empty ⇒ `bytes32_zero`, otherwise `hash256_bytes (concat (sort txids))`, with
the singleton case explicitly called out. `SigmaPoW01.v:1-28` pins the same
100-byte `txid ‖ C ‖ B ‖ LE32(h)` layout as `sigma_hash.cpp`.

`WindowExclusion.v` proves the properties that the C++ window layer relies on:

| Theorem | Line | Property |
|---|---|---|
| `exclude_already_paid_drops_paid` | 105 | a paid ticket leaves **F** |
| `exclude_already_paid_preserves_unpaid` | 113 | nothing else leaves **F** |
| `exclude_already_paid_order_independent` | 191 | **F** does not depend on traversal order |
| `assemble_F_excludes_window_paid` | 225 | window assembly excludes every paid ticket |
| `incomplete_window_is_no_draw` | 239 | an incomplete window yields `None`, never a short window |

That last one is the load-bearing match for `ConvergenceWindowTail::complete`
(`convergence_logic.h:91-101`) and is exactly the fail-closed behaviour the C++
implements.

### 4.4 Mint coupling — PASS

`ReplayReorgCore/ConnectBlockMint.v:52-79` mirrors the C++ acceptance rules
one-for-one:

```52:59:formal_verification/coq/ReplayReorgCore/ConnectBlockMint.v
Definition valid_res_mint_obligation (b : BlockAbs) : bool :=
  let authorized := res_mint_authorized b in
  let won := cm_draw_won (blk_mint b) in
  let n := cm_res_mint_count (blk_mint b) in
  if authorized then
    if won then Nat.eqb n 1 else Nat.eqb n 0
  else
    Nat.eqb n 0.
```

with `winner_mismatch_path`, `unauthorized_mint_path` and
`without_winner_mint_path` naming the three C++ rejects
(`bad-rigor-res-winner-mismatch`, `bad-rigor-res-amount`,
`bad-rigor-res-without-winner`). `ResMintObligation.v` discharges these against
`valid_block_connect`.

Note the scope boundary, which is documented in the source
(`ConnectBlockMint.v:47-48`): `cm_draw_won` is an **unconstrained field**. The
mint-coupling proof therefore says *"given a winner, exactly one mint"*; the
*derivation* of the winner is proved separately in the Sigma / ConvergeRoot /
WindowExclusion modules. The two halves are not yet joined by a single
end-to-end theorem.

### 4.5 Legacy modules are vacuous, not stale — PASS

`ReplayReorgCore/CvgCovenant.v` and `AssetFromScript.v` retain the retired
covenant model and both explicitly state `Honesty flags stay false. No [Axiom] /
[Admitted].` The retired `OP_CVG_CLAIM` (`0xbc`) is modelled as unreachable,
matching `script.h:222-227` where the opcode is burned and rejected by
`HasValidOps`. Keeping these as proved-unreachable is correct; deleting them
would remove the proof that the path is dead.

---

## 5. Mining template findings

### 5.1 Can miners manipulate F? — Structurally no, with one priced residual

**The template builder has no discretion that consensus does not re-derive.**
`CreateNewBlock` writes a placeholder scriptSig
(`node/miner.cpp:259`), appends the mandatory mint, and only then computes C
over the **final** vtx:

```329:337:core/src/node/miner.cpp
    {
        CCoinsViewCache view_convergence_root_at_entry(&m_chainstate.CoinsTip());
        const uint256 convergence_root =
            GetTemplateConvergenceRoot(*pblock, pindexPrev, view_convergence_root_at_entry, consensus);
        CMutableTransaction mtx_cb(*pblock->vtx[0]);
        mtx_cb.vin[0].scriptSig =
            CScript() << nHeight << std::vector<unsigned char>(convergence_root.begin(), convergence_root.end());
        pblock->vtx[0] = MakeTransactionRef(std::move(mtx_cb));
    }
```

`ConnectBlock` recomputes the same root from its own window walk and rejects a
mismatch as `bad-cb-convergence-root` (`validation.cpp:2835-2845`). The template
is a *suggestion*; the commitment is checked.

| Miner capability | Result |
|---|---|
| Observe winner probability before committing | **No** — the draw a block pays is fixed by its parent, already mined |
| Modify F after seeing σ | **No** — σ for the draw at `h` needs `B_{h-1}`, which is a solved header |
| Reorder tickets | **No effect** — C sorts lexicographically; `FinalizeWindowedDrawSet` order affects only candidate indices |
| Selectively exclude tickets | **Delay only** — an excluded ticket stays in the mempool and enters the window when any later block confirms it |
| Remove a *buried* ticket | **No** — the tail is rebuilt from ancestors' undo data (`validation.cpp:2483-2499`) |
| Bias candidate construction | **No** — membership is `IsConvergenceTx ∧ f ≥ MIN ∧ HasResOwnerDeclaration ∧ CheckTxInputs`, all objective |

**Residual (V-10, priced, accepted by design):** the producer of block `h`
chooses which convergence transactions from its *own* block enter **F**, which
changes `C_h` and hence all 32 σ values at `h+1`. It learns `B_h` only by
solving PoW. So one re-roll of the `h+1` draw costs one full block solution plus
the forgone subsidy. This is precisely the SECURITY-001-A8 (Sigma-PoW-01) design
trade and is correctly documented at `rigor_consensus_lock.h:130-133`. It is a
priced attack, not a free one.

### 5.2 W = 32 security rule — PASS, with a stated bound

Because `CONVERGENCE_DRAW_MIN_TXS_FOR_RES == 1` (`rigor_consensus_lock.h:102`),
**the entire exclusivity guarantee rests on `W = 32`**. Verified behaviour:

* **Minimum ticket cost:** `MIN_CONVERGENCE_AMOUNT = 1 ORU`, enforced at three
  independent points — `tx_verify.cpp:254-259` (structure),
  `convergence_root.cpp:137` (**F** membership), and
  `rigor_math.h:200` (weight validity).
* **Weight calculation:** `f_i` is the raw satoshi value, untransformed
  (`convergence_logic.cpp:121`). No β exponent, no normalisation.
* **Saturation:** per-draw win probability is `p = n / 32768` for `n` ORU,
  saturating at `n = 32768` ORU. A **1 ORU ticket does not dominate**: it draws
  at `1/32768` per block, `1/1024` over the window.
* **Overflow:** `T · MIN_CONVERGENCE_AMOUNT < 2²⁵⁶` with a 2¹⁵ margin, pinned by
  `static_assert`-adjacent invariants documented at `params.h:94-102` and guarded
  by `res_proof_math_tests`. This is the exact A-6 defect (a vacuous eligibility
  filter) and it is now closed.
* **Integer rounding:** `T = floor(2²⁴⁶/1e8) >> 5` discards a residue of 27,
  giving a relative neutrality error of ≈2.4e-65 — far below the smallest
  expressible quantity.

**Bound that must be stated:** `W = 32` removes exclusivity for a *minority*
miner, because buried candidates are reconstructed from ancestor undo data and
cannot be censored retroactively. It does **not** protect against a **majority
miner censoring convergence transactions for 32 consecutive blocks**, which
would reduce **F** to that miner's own tickets. With `MIN_TXS_FOR_RES = 1`
there is no cardinality floor to fall back on. See **H-5**.

### 5.3 Winner payment binding — PASS

The reward is bound to the ticket's HASH160 and to nothing else:

```84:86:core/src/consensus/rigor_res.cpp
    if (!Consensus::CheckResRewardOwner(winner.res_payout_hash160, payout->scriptPubKey)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-rigor-res-owner-mismatch");
    }
```

`winner.res_payout_hash160` comes from `MakeDrawCandidate`
(`convergence_logic.cpp:117-127`), which reads it out of the ticket script. The
miner address, coinbase preference and any pool database are irrelevant — a
mint paying anyone else is `bad-rigor-res-owner-mismatch`, and *omitting* the
mint is `bad-rigor-res-winner-mismatch` (`validation.cpp:3138-3140`).
**The payout is mandatory and non-redirectable.**

### 5.4 GBT surface — PASS, with one misleading comment

`getblocktemplate` publishes the full resolved parent draw — every σ, every
eligibility bit, and the winner (`rpc/mining.cpp:1032-1077`). This is safe:
every published value is a function of the already-mined parent block, so
disclosure grants no grinding advantage. It is *necessary*, because an external
builder cannot otherwise construct the mandatory mint.

The comment at `rpc/mining.cpp:1010-1011` — *"F spans the whole draw window, so
it is not a function of this template's body"* — is **incorrect**. **F** is the
window tail **plus this block's own candidates** (`convergence_logic.cpp:179-186`),
so an external builder that alters the body must recompute C. It has the data to
do so (`convergence_txids` is served), but the comment would mislead an
implementer into thinking C is body-independent. See **H-2**.

---

## 6. Mining software findings

### 6.1 The coinbase scriptSig has zero free bytes — a hard integration constraint

This is the single most important operational fact for anyone integrating pool
software, and it is a deliberate consequence of putting C in the scriptSig:

```106:115:core/src/consensus/convergence_root.cpp
        if (opcode != CONVERGENCE_ROOT_PUSH_OPCODE || vch.size() != 32) {
            err_reason = "convergence-root-cb-convergence-root-not-op-pushbytes-32";
            return false;
        }
        out.convergence_root = uint256{Span{vch}};

        if (scriptSig.GetOp(pc, opcode, vch)) {
            err_reason = "convergence-root-cb-trailing-script-data";
            return false;
        }
```

The coinbase scriptSig must be **exactly** `<BIP34 height> <PUSHBYTES_32 C>`
and nothing more. Consequences:

* **Standard Stratum extranonce placement is consensus-invalid on RIGOR.** Any
  pool that writes `extranonce1 ‖ extranonce2` into the coinbase scriptSig
  produces blocks rejected as `convergence-root-cb-trailing-script-data`.
* Per-miner search space must come from `nNonce`, `nTime` rolling, version
  rolling, or a coinbase **output** — never the scriptSig.
* This is a **security-positive** property: the block producer cannot smuggle
  bytes into the one field that carries the set commitment. But it must be
  documented as a mandatory integration constraint. See **H-3**.

`RefreshCoinbaseWitnessAndconvergence` (`node/miner.cpp:125-145`) exists to
rebuild commitments after a scriptSig change, but on RIGOR a changed scriptSig
also invalidates C, so the helper cannot make an edited coinbase valid.

### 6.2 Pool software cannot bias selection or redirect the reward

Independent of what any pool implementation does, consensus re-derives every
value that matters:

| Pool capability | Consensus response |
|---|---|
| Modify the commitment C | recomputed; `bad-cb-convergence-root` |
| Fake a winner | winner is derived from the parent; `bad-rigor-res-owner-mismatch` |
| Redirect the reward | payout HASH160 is the ticket's; `bad-rigor-res-owner-mismatch` |
| Omit the mint | `bad-rigor-res-winner-mismatch` |
| Pay the wrong amount | `bad-rigor-res-amount` (exact equality, not a cap) |
| Add extra RES to the coinbase | `bad-cb-res-amount` (exact equality) |
| Remove competing tickets from the window | buried tickets come from ancestor undo data |

**A pool DB can therefore only ever be a mirror.** No SQL table can be
authoritative for a winner or a payout, because a block whose coinbase
disagrees with the node's re-derivation is simply not a block. This is the
"script is the asset" property holding end-to-end through the mining stack.

### 6.3 Explorer classification — one real mislabelling bug

The explorer classifies a **non-coinbase** transaction as the RES mint:

```243:243:explorer/src/db.ts
    const isResMint = mintTxids.has(t.txid) || (resMinted === 1 && hasRes && !t.is_coinbase);
```

The `!t.is_coinbase` disjunct is inverted relative to the shipped architecture:
the mint is *always* the coinbase and *never* a body transaction. In any block
that mints, an ordinary body RES transfer will be mislabelled as the mint. This
is display-only (the explorer is a read-only mirror and is not a consensus
input), but it directly contradicts the single-path invariant this audit exists
to enforce. See **H-6**.

`explorer/src/script.ts:44-49` classifies the `OP_CONVERGE` lock correctly
(22 bytes, `0x14` prefix, `0xbd` terminator) and labels the retired CVG2 program
as historical — that part is right.

---

## 7. Security vulnerabilities discovered

Ordered by severity. None of these breaks the single-path invariant; the
consensus core is sound. The high-severity finding is an availability defect.

### V-1 — HIGH — Unbounded backward recursion in draw-window reconstruction

**Availability / DoS. Pruned nodes cannot resume validation.**

`GetBlockWindowContribution` calls `InferMintPaidTxid`, which resolves the
*parent's* draw set, which builds *another* 31-block window, each element of
which recurses again:

```
GetBlockWindowContribution(X)                       validation.cpp:2401
  └→ InferMintPaidTxid(X)                           validation.cpp:2425
       └→ GetConvergenceDrawSetForBlock(X.pprev)    validation.cpp:2389
            └→ BuildConvergenceWindowTail(X-2)      validation.cpp:2518
                 └→ 31 × GetBlockWindowContribution(X-32 … X-2)   ← recurses
```

The recursion terminates only at genesis, not after `W` blocks. Because every
minting block in a window re-enters the recursion, the transitive closure on a
cold cache is **every block from genesis to the tip**, at two disk reads each
(block + undo).

This directly contradicts the code's own claims:

* `validation.cpp:2300-2302` — *"costs W block reads plus W undo reads"*
* `validation.cpp:2548-2549` — *"a cold restart costs at most O(W) block/undo
  reads — never a full-chain scan"*

Both are false. The memoisation caches are FIFO-bounded at
`4 × CONVERGENCE_DRAW_WINDOW = 128` entries
(`validation.cpp:2335`, `2370`), which is large enough to keep the depth-first
traversal from going exponential, but does nothing about the Θ(H) total.

**Impact:**

1. **Pruned nodes abort.** On a pruned node the recursion eventually calls
   `ReadBlockFromDisk` on a pruned height, which fails → `GetBlockWindowContribution`
   returns `false` → `BuildConvergenceWindowTail` returns `false` →
   `ConnectBlock` calls `AbortNode("Failed to reconstruct the parent block's
   convergence set")` (`validation.cpp:2811`) or
   `AbortNode("Failed to assemble the convergence draw window")`
   (`validation.cpp:2833`). **The node shuts down.**
2. **Full-chain scan on the first block after every restart.**
   `RefreshConvergenceNetworkStatsCache` runs on every connect
   (`validation.cpp:3653`, `3784`) and the cache is empty at startup.
3. Same cost from `getblock` on any historical block (`rpc/blockchain.cpp:239`),
   `getblocktemplate` (`rpc/mining.cpp:1034`), and `getconvergencestats`.

**Root cause:** the paid-winner txid is *derived* on every read instead of being
*recorded* at connect time — even though `ConnectBlock` already has it in hand
at `validation.cpp:2826-2829`. `CBlockIndex` persists `has_res` and
`last_res_mint_block_hash` (`chain.h:469-470`) but **not** the paid txid.

**This is a defect of the derivation path, not of the consensus rule.** The rule
itself — a paid ticket leaves F — is correct and is proved in
`WindowExclusion.v:225`.

### V-2 — MEDIUM — Inverted normative documentation in a consensus-locked header

`consensus/convergenceres.h` carries `#define CONSENSUS_LOCK_CONVERGENCE_V3` and
a block headed *"Normative rules"* that states **the exact opposite** of the
shipped rule:

```32:33:core/src/consensus/convergenceres.h
 *   - New Res issuance is validated only on **non-coinbase** transactions via `rigor_res.cpp` + `res_proof.cpp`
 *     (convergence txid, bind hash, height, parent compact target, witness proof). Coinbase is never an Res mint program source.
```

Issuance is coinbase-**only**. Companion inversions:

| Location | Stale claim | Reality |
|---|---|---|
| `convergenceres.h:1-2` | "witness v1 **CVG2** convergence outputs" | `OP_CONVERGE`, 22-byte script |
| `convergenceres.h:30-31` | coinbase CVGE root enforced as `bad-convergence-draw-candidate-root` | not enforced; parameter is `(void)`-cast away |
| `convergenceres.cpp:112` | "RES is minted only via self-proof transactions in the block body" | coinbase only |
| `convergenceres.h:96-97` | `bad-convergence-cumulative` via `GetChainConvergeSum` | reject does not exist |
| `convergence_logic.h:32-37` | consensus checks a coinbase CVGE cumulative against a chain prefix sum | no such check |
| `convergence_root.cpp:20` | `fee(tx) > 0` is an **F** membership condition | contradicted 129 lines later at `convergence_root.cpp:149` |

An implementer trusting these comments would reintroduce a body mint path. In a
file that declares itself a consensus lock, this is a security defect in its own
right.

### V-3 — LOW — A block-local convergence-root API shares the consensus name

`ComputeConvergenceRootFromConvergenceSet` (`convergence_root.cpp:157`) and the
`vtx`-taking overload of `ValidateCoinbaseConvergenceRootScriptSig`
(`convergence_root.cpp:196-205`) compute C from **one block's transactions
only** — the pre-A-9-C single-block **F**. Consensus does not use them:
`validation.cpp:2841` calls the window overload with a root derived from
`BuildConvergenceDrawSet`.

They are **not dead code**. They are used by `bench/wallet_create_tx.cpp:62`,
`bench/duplicate_inputs.cpp:59`, `test/util/mining.cpp:54` and eleven cases in
`test/convergence_root_tests.cpp`. The most load-bearing call site is careful
about it and asserts the equivalence condition:

```46:56:core/src/test/util/mining.cpp
        // A-9-C: elsewhere a coinbase must commit the WINDOW root via GetTemplateConvergenceRoot,
        // because C spans CONVERGENCE_DRAW_WINDOW blocks. The block-local call is correct here and
        // only here: these blocks are coinbase-only, so F is empty at every height and the window
        // root and the block-local root are both null. There is also no CBlockIndex to walk — this
        // chain is synthesised before anything is connected. Do not copy this into a producer that
        // can carry convergence transactions.
        ...
        assert(convergence_root.IsNull());
```

**Residual risk is naming, not behaviour.** The function names do not encode
"block-local", so a future producer-side caller would silently reintroduce the
single-block **F** that `W = 32` exists to remove — without a compile error, and
without the `assert` that protects the current call site. The fix is a rename,
not a deletion.

### V-4 — MEDIUM — Retired alternative-mint machinery still compiles into the consensus library

Two modules implementing the *removed* authorization path remain linked, with
no non-test callers:

* `consensus/res_mint_carrier.cpp` — `BuildPcorbCarrierScript` /
  `BuildPcorbCarrierOutput`, which construct the `PCOR ‖ 116` tagged carrier.
  Referenced only from `test/res_orum_tests.cpp` and
  `test/res_proof_math_tests.cpp`.
* `consensus/res_proof.cpp` (166 lines) — `VerifyConvergeProof`, the retired
  proof-based mint authorization. The only remaining mentions in
  `script/interpreter.cpp:1902` and `consensus/statetag.cpp:62` are **comments**.

This is a **parallel ticket/mint representation that still exists as buildable
code**, which is precisely what the single-path objective forbids. Object files
`libbitcoin_consensus_a-res_mint_carrier.o` and
`libbitcoin_consensus_a-res_proof.o` confirm both are compiled.

### V-5 — MEDIUM — O(H) chain walk on every block connect

```27:27:core/src/consensus/observational_metrics.cpp
    m_cumulative_convergence = pindexPrev->GetChainConvergeSum() + block_new_convergence;
```

`GetChainConvergeSum()` walks `pprev` to genesis on every call
(`chain.h:238-247`), and `ObservationalMetrics::Update` is called on every
`ConnectBlock` (`validation.cpp:3195`). Total cost over a chain of height H is
**Θ(H²)**.

The walk is entirely avoidable: the cumulative is already persisted on the
parent index (`chain.h:462-467`, read back at `txdb.cpp:337`), so the parent's
own value is available without walking. The recomputed value feeds only RPC and
`state_snapshot` — it is not a consensus predicate — so the fix carries no
consensus risk.

Minor sub-issue: the walk silently skips negative values
(`chain.h:242`), tolerating a state that should be impossible rather than
asserting against it.

### V-6 — LOW — The convergence-root algorithm has two implementations

```371:379:core/src/consensus/convergence_logic.cpp
static uint256 HashConcatenatedSortedTxids(std::vector<uint256> txids)
{
    std::sort(txids.begin(), txids.end());
    HashWriter hw;
    for (const uint256& id : txids) {
        hw << id;
    }
    return hw.GetHash();
}
```

This duplicates `ComputeConvergenceRootFromEligibilityTxids` using `HashWriter`
instead of `CHash256`. The two are byte-identical **today** (both are SHA256d
over concatenated raw 32-byte values), and this one is used for the eligible
root and draw-merkle root rather than for C itself — but maintaining two
implementations of a consensus commitment algorithm is a divergence risk with
no upside.

### V-7 — LOW — Dead coinbase stripper matches the wrong magic

```109:109:core/src/node/miner.cpp
    return vch.size() >= 4 && vch[0] == 0x46 && vch[1] == 0x52 && vch[2] == 0x47 && vch[3] == 0x45;
```

`0x46 0x52 0x47 0x45` is ASCII `"FRGE"`. The actual CVGE magic is
`0x43 0x56 0x47 0x45` = `"CVGE"` (`consensus/convergence_commitment.cpp:13`).
The function named `VoutLooksLikeConvergenceCommitment` would not strip a
convergence commitment. Harmless today because nothing emits either tag, but it
is a stale constant sitting in the mining path.

### V-8 — LOW — No duplicate-txid assertion when assembling F

`FinalizeWindowedDrawSet` (`convergence_logic.cpp:168-193`) concatenates the
window tail and the source block's own candidates with no duplicate check. A
duplicate txid would be counted twice in C, would appear twice in the draw with
its weight effectively doubled, and could in principle be paid twice.

**Currently unreachable**: a transaction cannot be confirmed twice on one chain,
because its inputs are consumed on first confirmation. But **F** is assembled
from 32 independently-read blocks and the invariant that makes duplicates
impossible lives entirely outside this function. A cheap `Assert` would pin it.

### V-9 — LOW — Domain separation is implicit, resting on preimage length disjointness

Every consensus digest uses bare `Hash256` with no domain tag:

| Digest | Preimage length |
|---|---|
| σ | 100 bytes |
| C / eligible root | 32·n bytes |
| Block header | 80 bytes |
| Merkle node | 64 bytes |

Cross-domain collisions are impossible today **only because these lengths are
disjoint** (100 is not a multiple of 32, and is neither 64 nor 80). That is a
correct but fragile invariant, and it is nowhere written down as an invariant.
Adding a 32-byte input to the σ preimage, or a length prefix to C, would break
it silently.

### V-10 — INFORMATIONAL — Priced re-roll of the next block's draw (accepted design)

Restated from §5.1 for the vulnerability register. The producer of block `h`
controls its own contribution to **F**, hence `C_h`, hence every σ at `h+1`; and
it learns `B_h` only by solving PoW. Because the winner is paid to the ticket's
HASH160 regardless of who mines `h+1`, a miner *can* grind `C_h` in favour of
its own ticket — but each sample costs one full block solution plus the forgone
subsidy. Correctly documented at `rigor_consensus_lock.h:130-133`. **Priced, not
free. No action required; must not be described publicly as "impossible".**

### V-11 — INFORMATIONAL — `OP_RES_MINT` is a dual-use lock

`IsAllowedScript` (`tx_check.cpp:26-27`) permits an `OP_RES_MINT` output in a
*body* transaction. Such an output is not inflationary — it requires RES inputs
(`tx_verify.cpp:391-397`) — and is not counted as issuance, because
`ConnectEvaluateCoinbaseResMint` scans only `coinbase.vout` and
`m_block_res_mint_sum` is set from `res_mint_coinbase` alone
(`validation.cpp:3184`). But it means a RES holder can create an
`OP_RES_MINT`-locked output at an arbitrary maturity height, so "this script is
an issuance event" is **not** a valid inference for indexers. This is the likely
origin of the explorer bug in §6.3.

---

## 8. Required hardening changes

Priority order follows the audit's stated principle: consensus safety, formal
verifiability, deterministic execution, script-as-asset correctness, simplicity.

### H-1 — Record the paid-winner txid on `CBlockIndex` (fixes V-1) — **P0**

Eliminate the recursion by storing what `ConnectBlock` already computes.

* Add `uint256 m_paid_winner_txid{}` to `CBlockIndex`, serialised in
  `CDiskBlockIndex::SERIALIZE_METHODS` next to `last_res_mint_block_hash`
  (`chain.h:470`).
* Write it in `ConnectBlock` alongside `pindex->m_block_res_mint_sum`
  (`validation.cpp:3184`), from the `own_paid_txid` already resolved at
  `validation.cpp:2826-2829`.
* Change `InferMintPaidTxid` to read the recorded value and recurse **only** as a
  fallback for an index entry written before the field existed.

Result: `GetBlockWindowContribution` becomes leaf-level, window reconstruction
returns to a genuine Θ(W), pruned nodes stop aborting, and the comments at
`validation.cpp:2300-2302` and `2548-2549` become true.

This is a **local block-index format addition**, not a consensus rule change —
the consensus predicate is unchanged, only the derivation path. Pre-mainnet is
the correct time to make it. Requires a block-index reindex, which is
acceptable while `mainnet_launched = false`.

### H-2 — Correct the inverted consensus documentation (fixes V-2) — **P0 — APPLIED**

**Applied during this audit.** The normative blocks now state the shipped rule:

* issuance is **coinbase `OP_RES_MINT` only**;
* the ticket is the **22-byte `OP_CONVERGE` script**, not CVG2;
* there is **no** CVGE cumulative check and **no** `bad-convergence-cumulative`
  or `bad-convergence-draw-candidate-root` reject;
* fee is **relay policy**, not an **F** membership condition;
* the GBT comment no longer claims **F** is body-independent.

See §10 for the exact edits. Comment-only; all touched translation units
recompile clean.

### H-3 — Document the zero-free-byte coinbase scriptSig constraint (fixes §6.1) — **P0**

Add to `core/doc/mining-pool-gbt.md` an explicit integration rule: the coinbase
scriptSig is byte-exact, standard Stratum extranonce placement is invalid, and
per-miner search space must come from `nNonce` / `nTime` / version rolling or a
coinbase output. Any pool integrating against RIGOR without this will produce
100% rejected blocks with a non-obvious error string.

### H-4 — Delete the retired mint machinery; rename the block-local root API (fixes V-3, V-4) — **P1**

* **Rename**, do not delete, the block-local pair in
  `consensus/convergence_root.{h,cpp}`:
  `ComputeConvergenceRootFromConvergenceSet` →
  `ComputeBlockLocalConvergenceRoot`, and the `vtx`-taking
  `ValidateCoinbaseConvergenceRootScriptSig` overload →
  `ValidateCoinbaseConvergenceRootScriptSigBlockLocal`. They have twenty
  legitimate bench and test callers; the defect is that the names do not say
  "block-local", so a producer-side caller would compile silently. A rename
  makes every future misuse self-evident at the call site and costs one
  mechanical edit pass.
* Delete `consensus/res_mint_carrier.{h,cpp}` and `consensus/res_proof.{h,cpp}`
  and the tests that exist only to exercise them, or reduce those tests to
  "the retired path is unreachable" assertions.
* Delete the CVGE parser (`consensus/convergence_commitment.{h,cpp}`) or mark it
  explicitly non-consensus if the RPC surface still needs it to read a legacy
  devnet block.

This is the "removing unnecessary complexity" mandate applied literally: a
retired mint path that still compiles is a retired mint path that can be
recalled.

### H-5 — Decide `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` explicitly (addresses §5.2 bound) — **P1**

With the floor at 1, `W = 32` is the *sole* barrier to a majority miner reducing
**F** to its own tickets. Either:

* raise the floor so a draw needs genuine multi-party participation, accepting
  that early-mainnet blocks will mint nothing; or
* keep it at 1 and record in `rigor_consensus_lock.h:101-102` that
  majority-miner window censorship is a **known, accepted** residual, so the
  next reader does not mistake `W = 32` for a complete defence.

The choice is economic and belongs to the protocol owner. The audit's
requirement is only that it be **explicit** — right now the floor's weakness is
implicit.

### H-6 — Fix the explorer mint classification (fixes §6.3, V-11) — **P1**

At `explorer/src/db.ts:243`, drop the `(resMinted === 1 && hasRes && !t.is_coinbase)`
disjunct. The mint is exactly the transaction in `res_mints` for that block,
which is always the coinbase. As written, the explorer asserts a body mint path
that consensus forbids.

### H-7 — Replace the O(H) cumulative walk (fixes V-5) — **P2**

In `consensus/observational_metrics.cpp:27`, use the parent's already-persisted
cumulative instead of re-walking the chain. Non-consensus, one line, removes an
O(H²) term from initial block download.

### H-8 — Defensive pins (fixes V-6, V-8, V-9) — **P2**

* Make `HashConcatenatedSortedTxids` call
  `Consensus::ComputeConvergenceRootFromEligibilityTxids` so one function
  defines the algorithm.
* Add a duplicate-txid `Assert` in `FinalizeWindowedDrawSet`.
* Add a comment block at `consensus_rigor/sigma_hash.h` recording that
  cross-domain collision resistance rests on preimage-length disjointness
  (100 vs 32·n vs 80 vs 64), so a future field addition is forced to confront it.
* Fix or delete the `"FRGE"` magic at `node/miner.cpp:109`.

### H-9 — Close or publicly scope the Coq ↔ C++ gap (addresses §4.2) — **P2**

The three open obligations mean *"the Coq model matches C++ behaviour"* is not a
machine-checked claim. Either wire the C++ oracle so
`bridge_cpp_equivalence_obligation` can be discharged, or ensure every public
statement of formal-verification coverage is scoped to what is actually proved:
the byte-level root/σ pipeline, the window exclusion properties, and the mint
coupling — **not** end-to-end C++ refinement.

Additionally, no single theorem yet joins the draw derivation
(`Sigma.v` / `ConvergeRoot.v` / `WindowExclusion.v`) to the mint coupling
(`ConnectBlockMint.v`), because `cm_draw_won` is an unconstrained field. An
end-to-end theorem *"the winner derived from F is the ticket the coinbase must
pay"* would be the highest-value proof to add next.

---

## 9. Final security verdict

### 9.1 Acceptance criteria

**Consensus**

| Criterion | Verdict | Evidence |
|---|---|---|
| RES mint has exactly one path | **PASS** | `tx_verify.cpp:391-397` + `rigor_res.cpp:50-91` + `connect_block_rigor_checks.cpp:31-36` (exact equality) |
| CONVERGE ownership exists only in script | **PASS** | `interpreter.cpp:98-105`, `res_owner.cpp:16-41`; no registry, no sibling output |
| No global state dependency | **PASS** | mint reads block + undo + params only; cumulative fields have no consensus reader |
| Miner cannot modify winning condition | **PASS** | every σ input for the draw paid at `h` is fixed by the solved header `h-1` |

**Formal verification**

| Criterion | Verdict | Evidence |
|---|---|---|
| Coq model matches C++ behavior | **PARTIAL** | byte-level C and σ match exactly (`ConvergeRoot.v:11-18`, `SigmaPoW01.v:1-28`); end-to-end refinement is an **open obligation**, not a theorem |
| No new axioms required | **PASS** | 0 `Axiom`, 0 `Admitted`, 0 `admit.` across 222 `.v` files |
| Proof obligations remain local and deterministic | **PASS** | `WindowExclusion.v` proves order-independence and fail-closed incomplete windows |

**Mining stack**

| Criterion | Verdict | Evidence |
|---|---|---|
| Template generation is deterministic | **PASS** | C computed over the final vtx and re-derived by `ConnectBlock`; `bad-cb-convergence-root` |
| Pool software cannot bias selection | **PASS** | buried candidates come from ancestor undo data; membership is objective |
| Winner payout cannot be redirected | **PASS** | `bad-rigor-res-owner-mismatch` / `bad-rigor-res-winner-mismatch` |

### 9.2 Verdict

> **The single-path CONVERGE architecture is correctly implemented.**
>
> `ORU burn → f_i → σ < T · f_i → coinbase RES mint` is the only path that
> exists in consensus. There is no second ticket representation, no alternative
> issuance path, and no global accumulator in the mint decision. The retired
> CVG2 / RESO / PCORB / RIGR / covenant model is gone from the validation path.
>
> **No consensus vulnerability was found.** The eligibility filter is
> non-vacuous, the arithmetic is exact at 512 bits, the σ preimage is
> byte-pinned, and the payout is bound to the ticket script.
>
> **One HIGH-severity availability defect (V-1)** must be fixed before mainnet:
> draw-window reconstruction recurses to genesis instead of stopping at `W`,
> which makes pruned nodes abort and costs every node a full-chain scan after
> restart. The fix (**H-1**) records data `ConnectBlock` already has, and does
> not touch a consensus rule.
>
> **One MEDIUM documentation defect (V-2)** was disproportionately dangerous:
> a header marked `CONSENSUS_LOCK_CONVERGENCE_V3` stated that issuance is
> non-coinbase-only — the exact inverse of the shipped rule. **Fixed during
> this audit (H-2, §10).**

**Status: CONDITIONAL PASS — conditional on H-1.**

H-2 is closed. H-1 is the sole remaining blocker: it is an availability defect,
not a consensus defect, and its fix records data `ConnectBlock` already holds.

The residual grinding cost (V-10) and the majority-miner window censorship bound
(H-5) are properties of the design, not defects in the implementation. They are
recorded here so that public material describes them accurately: the draw
re-roll is **priced at one block solution**, not impossible, and `W = 32`
defeats **minority** exclusivity, not majority censorship.

---

## 10. Changes applied by this audit

Scope was deliberately limited to **comment-only corrections of normative text
that contradicted the shipped consensus rule** (H-2). No consensus predicate,
no serialized format, and no executable line was altered. Every touched
translation unit was recompiled.

| File | Change |
|---|---|
| `consensus/convergenceres.h:1-5` | Header line said "witness v1 **CVG2** convergence outputs"; now describes the 22-byte `OP_CONVERGE` lock and records that CVG2 is rejected by `IsAllowedScript`. |
| `consensus/convergenceres.h` (normative rules) | Removed the claim that a coinbase CVGE v2 root is enforced as `bad-convergence-draw-candidate-root`. **Removed the inverted claim that "New Res issuance is validated only on non-coinbase transactions… Coinbase is never an Res mint program source."** Replaced with the coinbase-only rule and the `mixed-asset-output` body reject. |
| `consensus/convergenceres.h` (`GetConvergenceAmount` doc) | Removed the `bad-convergence-cumulative` / `GetChainConvergeSum` commitment claim; now states those ledgers are observational with no ConnectBlock reader. |
| `consensus/convergenceres.cpp:112` | "RES is minted only via self-proof transactions in the block body" → coinbase `OP_RES_MINT` only. |
| `consensus/convergence_logic.h:31-37` | Removed the claim that consensus checks a coinbase CVGE cumulative against a chain prefix sum; marked the ledger NON-CONSENSUS while preserving the load-bearing "must count only destroyed ORU" note. |
| `consensus/convergence_root.cpp:20` | **F** membership listed `fee(tx) > 0`, contradicting `convergence_root.cpp:149`. Now lists `HasResOwnerDeclaration` and states fee is relay policy. |
| `rpc/mining.cpp:1010` | "F … is not a function of this template's body" was false. Now states **F** = window tail + own candidates, and tells an external builder how to re-derive C after editing the body. |

Verification:

```
core/src $ make consensus/libbitcoin_node_a-convergenceres.o \
                consensus/libbitcoin_node_a-convergence_root.o \
                consensus/libbitcoin_node_a-convergence_logic.o \
                rpc/libbitcoin_node_a-mining.o \
                consensus/libbitcoin_node_a-rigor_res.o \
                libbitcoin_node_a-validation.o \
                node/libbitcoin_node_a-miner.o
  → all seven objects rebuilt, no warnings, no errors
```

**Not applied, and why.** H-1 adds a serialized `CBlockIndex` field and requires
a reindex — correct pre-mainnet, but a format decision that belongs to the
protocol owner, not to an auditor. H-5 is an economic parameter choice. H-7
touches a serialized observational ledger; the two convergence-sum definitions
were verified to agree (`validation.cpp:3078-3087` accumulates exactly what
`GetTransactionConvergenceAmount` returns for an accepted block, so the
recurrence is sound), but changing a persisted ledger's derivation is a separate,
independently-testable commit. H-4's rename is mechanical but touches twenty
call sites across bench and test.

---

## Appendix A — Reject codes exercised by the single path

| Code | Site | Meaning |
|---|---|---|
| `bad-cb-convergence-root` | `validation.cpp:2843` | committed C ≠ re-derived window C |
| `convergence-root-cb-trailing-script-data` | `convergence_root.cpp:113` | extra bytes in the coinbase scriptSig |
| `bad-rigor-res-owner-mismatch` | `rigor_res.cpp:85` | mint pays someone other than the ticket owner |
| `bad-rigor-res-winner-mismatch` | `validation.cpp:3139` | winner exists, `W(h)>0`, no mint |
| `bad-rigor-res-without-winner` | `validation.cpp:3150` | mint with no winner |
| `bad-rigor-res-amount` | `rigor_res.cpp:90`, `validation.cpp:3153` | wrong amount, second mint, or mint after schedule end |
| `bad-rigor-res-trigger-underflow` | `validation.cpp:3147` | mint below the cardinality floor |
| `bad-cb-res-amount` | `connect_block_rigor_checks.cpp:35` | coinbase RES ≠ fees + bootstrap + mint |
| `mixed-asset-output` | `tx_verify.cpp:396` | body transaction creating RES |
| `invalid-convergence-structure` | `tx_verify.cpp:252` | more than one convergence output |
| `convergence-must-be-orum` | `tx_verify.cpp:381`, `290` | convergence output on a RES-domain transaction |
| `SCRIPT_ERR_CONVERGE_UNSPENDABLE` | `interpreter.cpp:1232` | attempt to spend an `OP_CONVERGE` output |

## Appendix B — Findings index

| ID | Sev | Title | Fix |
|---|---|---|---|
| V-1 | HIGH | Unbounded backward recursion in window reconstruction | H-1 |
| V-2 | MED | Inverted normative docs in a consensus-locked header | H-2 |
| V-3 | LOW | Block-local convergence-root API shares the consensus name | H-4 |
| V-4 | MED | Retired PCORB / res_proof mint machinery still linked | H-4 |
| V-5 | MED | O(H) chain walk on every block connect | H-7 |
| V-6 | LOW | Two implementations of the root algorithm | H-8 |
| V-7 | LOW | Dead coinbase stripper matches `"FRGE"` not `"CVGE"` | H-8 |
| V-8 | LOW | No duplicate-txid assertion in F assembly | H-8 |
| V-9 | LOW | Implicit domain separation via preimage length | H-8 |
| V-10 | INFO | Priced re-roll of the next block's draw | none (design) |
| V-11 | INFO | `OP_RES_MINT` is a dual-use lock | H-6 |
| — | MED | Explorer labels body RES transfers as the mint | H-6 |
| — | LOW | Zero-free-byte coinbase scriptSig undocumented for pools | H-3 |
| — | LOW | Misleading GBT comment: "F is not a function of this template's body" | H-2 |
