# RIGOR Mining Template / SOLO Miner Security Audit (P0-014)

Task: `TASK-RIGOR-MINING-TEMPLATE-SOLO-MINER-SECURITY-AUDIT-P0-014`
Status: **Remediation implemented in tree (P0-014), plus a post-implementation review pass.** Findings H-1, M-1, M-2, M-4, M-5, M-6 (docs), M-3 (quarantine notice), L-1 addressed. The remediation was then re-reviewed (§3.1): R-1 through R-10 are **fixed**.
Scope: mining template generation, coinbase construction, `OP_RES_MINT` issuance gate, SOLO mining RPC, and template mutation. Pool / Stratum / YiiMP are **not shipped** and are reviewed only as residual tree surface.

Design constraints used as the acceptance bar:

- mainnet is not launched; there is no historical compatibility burden
- SOLO mining first; Bitcoin pool compatibility is not a goal
- security over compatibility
- **Minimal Consensus Surface** + **Solo Mining First** + **No Hidden Reward Path**

---

## 0. Verdict

**ConnectBlock does not let miner software, an untrusted GBT consumer, or transaction selection rewrite RES mint conditions, winner ownership, or the parent-draw commitment.** Those fields are re-derived from the parent block and fail closed. There is **no Critical consensus hole** on the shipped SOLO path.

The remaining defects are **integration and surface-hygiene**, not issuance bugs:

1. Honest external GBT miners are told (in live docs and a consensus header comment) to build a **two-push** coinbase `scriptSig`. Consensus requires **three pushes**: height ‖ C ‖ paid-winner txid. GBT does not return the assembled coinbase. Wrong reconstruction is rejected (`bad-cb-convergence-root`); it cannot steal RES. It can stall honest SOLO hashrate.
2. The official tree still contains a **pool / Stratum / extranonce / payout** stack that the SOLO release explicitly does not ship. That is a hidden-complexity magnet, not a live consensus path.
3. Several assembler / launcher leftovers still *look* like a second reward path (`nFeesRes`, `-miningresfeeaddr`, “mint not armed”). Production ConnectBlock does not pay them. `nFeesRes` has since been deleted from the assembler outright (§3.1 R-2).

| Freeze principle | Holds on consensus? | Blocking finding |
|---|---|---|
| Minimal Consensus Surface | **Mostly** | H-1 (GBT reconstruction), L-1 (BIP141 reserved nonce) |
| Solo Mining First | **Yes in `core/`** | M-3 (`pool/` still in tree) |
| No Hidden Reward Path | **Yes at ConnectBlock** | M-4 closed: the dead assembler fee/RES path is deleted (§3.1 R-2) |

**Mainnet freeze of the mining interface is not blocked by a consensus bug.** It *is* blocked by H-1 if any shipped miner is expected to reconstruct the coinbase from GBT JSON instead of using the node’s assembler (`generatetoaddress` / `CreateNewBlock`).

This audit **implemented** the authorized remediations in-tree. Each finding below still records technical cause, fix, consensus impact, and test plan.

---

## 1. Current mining architecture

### 1.1 Control flow (shipped SOLO)

```
mempool (untrusted tx selection)
        │
        ▼
BlockAssembler::CreateNewBlock          node/miner.cpp
  ├─ addPackageTxs (ancestor feerate)   policy, not consensus
  ├─ ORU vout[0] = subsidy + claimable fees − RFC-018 burn
  ├─ TryAppendCoinbaseResMint           parent draw only
  ├─ scriptSig = height ‖ C ‖ paid-txid
  └─ GenerateCoinbaseCommitment         BIP141 witness OP_RETURN
        │
        ▼
getblocktemplate / generatetoaddress / submitblock
        │
        ▼
ConnectBlock                            validation.cpp
  ├─ parent draw ResolveConvergenceDraw (A-8)
  ├─ C + paid-winner byte-exact in scriptSig
  ├─ ConnectEvaluateCoinbaseResMint     rigor_res.cpp
  ├─ winner-must-mint iff W(h) > 0
  └─ ORU / RES coinbase budgets
```

Recommended SOLO grind: `nNonce` (primary), `nTime` within MTP / future window, optional `nVersion`. Coinbase `scriptSig` has **zero free bytes**.

### 1.2 What the miner is allowed to choose

| Field | Miner-controlled? | Consensus effect if mutated |
|---|---|---|
| `nNonce` | Yes | PoW only |
| `nTime` in `[MTP+1, now+2h]` | Yes | Header / MTP / next-block ASERT parent time. **Not** a σ input |
| `nVersion` (without clobbering required bits) | Yes | Version bits; not mint/winner |
| `nBits` | No | `bad-diffbits` |
| `hashPrevBlock` | Competing tip only | Pays *that* parent’s draw |
| Body tx set / order | Yes (policy) | Changes **this** block’s F / C for the *next* draw. Invalid order / stale C → reject |
| ORU `vout[0]` script / split | Yes, if total ≤ claimable | Miner’s SOLO payout. Cannot exceed `subsidy + fees − RFC-018 burn` |
| `OP_RES_MINT` amount | No | `bad-rigor-res-amount` |
| `OP_RES_MINT` HASH160 | No | `bad-rigor-res-owner-mismatch` |
| Omit mint when parent has a winner and `W(h) > 0` | No | `bad-rigor-res-winner-mismatch` |
| Mint when no winner / `W(h) = 0` | No | `bad-rigor-res-without-winner` / `bad-rigor-res-amount` |
| C in `scriptSig` | Must equal window F | `bad-cb-convergence-root` |
| Paid-winner push | Must equal parent winner iff minting, else 32 zero bytes | `convergence-root-cb-paid-winner-mismatch` |
| Extranonce in `scriptSig` | Forbidden | trailing-data / parse reject |
| Body `OP_RES_MINT` | Forbidden | `bad-txns-res-mint-noncoinbase` |
| Body creates RES | Forbidden | `mixed-asset-output` |
| PCORB carrier | Forbidden | `bad-pcorb-carrier` |

σ for the draw **this** template pays is:

`σ_i = Hash256(txid_i ‖ C_{h-1} ‖ B_{h-1} ‖ LE32(h))`

Every input except the payout height is already in the parent. Template mutation cannot re-roll the winner (SECURITY-001-A8).

### 1.3 Coinbase layout the assembler actually builds

Order in `CreateNewBlock` (`core/src/node/miner.cpp`):

1. Dummy coinbase slot, then body packages from the mempool (if any).
2. `vout[0]` ORU: `(nFeesOru - nFeesOruResTransferBurn) + GetBlockSubsidy(h)` to `scriptPubKeyIn`.
3. Optional mockable-chain RES bootstrap (witness v1, **not** `OP_RES_MINT`; mainnet amount is 0).
4. Optional `OP_RES_MINT` via `TryAppendCoinbaseResMint` — parent draw winner, amount `GetResMintReward(h)`, maturity `h + nCoinbaseMaturity`. **Not gated on `-miningresfeeaddr`.**
5. `scriptSig = BuildCoinbaseCommitmentScriptSig(h, C, paid_txid)` — paid_txid is the parent winner, or 32 zero bytes if this block does not mint.
6. BIP141 witness commitment appended; reserved nonce is 32 zero bytes.

GBT **does not** return `coinbasetxn`. It returns `coinbasevalue` (ORU `vout[0]` only), `default_res_mint`, `convergence_root`, `convergence_txids`, and `convergence_draw`. An external miner must reconstruct the coinbase.

### 1.4 Issuance gate (single path)

New RES exists only as a coinbase `OP_RES_MINT` output:

| Layer | Rule |
|---|---|
| `CheckTransaction` | non-coinbase `OP_RES_MINT` → `bad-txns-res-mint-noncoinbase` |
| `CheckTxInputs` | body tx creating RES → `mixed-asset-output` |
| `ConnectEvaluateCoinbaseResMint` | 0 or 1 canonical mint; owner = parent winner HASH160; amount = `W(h)` |
| ConnectBlock | winner ∧ `W(h) > 0` ⇒ mint must exist; no winner / `W(h)=0` ⇒ mint must not exist |
| RES budget | `res_coinbase_value == nFeesRes + bootstrap + mint` (exact). Production `nFeesRes` is accumulated as 0 |
| ORU budget | `oru_coinbase_value ≤ claimable` (underpay allowed; overpay `bad-cb-amount`) |

`OP_RES_MINT` is a **lock template**, not a mint opcode a miner can execute. Spend is `VerifyResMintSpend` after maturity; it cannot create additional RES.

---

## 2. Attack-surface review (audit questions)

### 2.1 Can the miner modify consensus fields?

**Header:** `nBits` is forced. `nTime` is miner-chosen inside the Bitcoin-style window; ASERT scores the **parent** timestamp, so rolling `nTime` does not retarget *this* block. σ does not read `nTime`. This is residual PoW time-warp, not a mint bug.

**Coinbase `scriptSig`:** byte-exact three-push. Extra bytes fail. Height must match. C must match the windowed unpaid set. Paid-winner push must match the mint bit.

**Witness commitment:** last BIP141 `OP_RETURN` must match `SHA256d(witness merkle ‖ reserved)`. Mutating txs or the reserved nonce without rebuilding the commitment fails `bad-witness-merkle-match`.

### 2.2 Can the miner modify RES mint conditions?

No. `W(h)` is a height schedule (`GetResMintReward`). Eligibility is `σ_i < T · f_i` over the **parent** set. T is a constitutional constant, not parent `nBits`. The miner cannot change T, f_i of already-committed tickets, C_{h-1}, or B_{h-1} without mining a different parent.

### 2.3 Can the miner modify winner ownership?

No. Owner is HASH160 inside the winning `OP_CONVERGE` output, authenticated by the ticket signer. `CheckResRewardOwner` requires the mint script’s 20-byte payload to equal that hash. Redirecting to the miner’s RES address is `bad-rigor-res-owner-mismatch`.

### 2.4 Can the miner modify the convergence commitment?

Only by changing **which** eligible tickets are in this block (and thus in window F). That is ordinary miner policy (censorship / selection). The committed C must then be the Hash256 of the sorted surviving txids. Serving a stale C from GBT while dropping a convergence tx is rejected. There is no free re-roll of the draw **this** block pays.

### 2.5 Can the miner affect final reward attribution?

| Reward | Who gets it | Miner can redirect? |
|---|---|---|
| ORU subsidy + claimable ORU fees | Coinbase `vout[0]` (and any extra ORU outputs under the cap) | **Yes — this is SOLO** |
| RFC-018 RES-transfer fee burn | Nobody (must not be claimed) | Claiming it → `bad-cb-amount` |
| RES mint `W(h)` | Parent-draw winner’s declared HASH160 | **No** |

There is no pool payout layer, no second mint, and no body-funded mint on the consensus path.

### 2.6 Coinbase script injection

`CheckTransaction` allows only witness v0 P2WPKH, witness v1 RES, `OP_RES_MINT`, `OP_CONVERGE`, and `OP_RETURN` (value 0). Bare P2PKH / P2SH / P2WSH are `bad-rigor-script-type`. Extra `OP_RETURN` tags (historical CVGE / RIGR) are ignored by ConnectBlock and are not mint authorization. Trailing `scriptSig` data is not a graffiti slot.

### 2.7 Transaction ordering

Assembler sorts packages by ancestor count. Consensus F is a **sorted txid set**, not block order. Same-block chains still need topological order for the overlay (`RIG-02`); a miner who permutes that invalidly fails `CheckTxInputs`. Fee ordering is policy.

### 2.8 Timestamp manipulation

Covered in 2.1. Not a winner-rewrite primitive.

### 2.9 Witness commitment manipulation

Covered in 2.1. BIP141 left the reserved 32-byte nonce unconstrained. **P0-014 L-1 freeze:** `ContextualCheckBlock` requires 32 zero bytes (`bad-witness-reserved-nonce`, after merkle-match so a mismatched nonce without a rebuilt commitment stays `BLOCK_MUTATED`).

### 2.10 Template mutation

**P0-014 M-1:** SOLO GBT advertises `mutable: ["time"]` only plus `rigor_coinbase_immutable: true`. Mutating `transactions` without recomputing C is still a guaranteed invalid block, not a stealth fork. GBT now also serves `default_coinbase_scriptsig` and `paid_winner_txid` so clients copy bytes instead of reconstructing the three-push `scriptSig` (H-1).

---

## 3. Findings

| ID | Sev | Remediation |
|---|---|---|
| H-1 | High | **Applied.** GBT serves `default_coinbase_scriptsig`, `paid_winner_txid`, `rigor_coinbase_immutable`. Docs/comments are three-push. Still no dummy `coinbasetxn`. |
| M-1 | Med | **Applied.** `mutable: ["time"]` only. |
| M-2 | Med | **Applied (revised by R-5).** `generateblock` calls `RebuildTemplateCommitments` (scriptSig + witness + merkle in one entry). |
| M-3 | Med | **Quarantine notice** in `pool/README.md`. Directory not deleted. |
| M-4 | Med | **Applied (revised by R-2 / R-6).** `nFeesRes` deleted from the assembler. `-miningresfeeaddr` is **rejected at startup** on non-mockable chains and parsed into `BlockAssembler::Options` on mockable chains. |
| M-5 | Med | **Applied.** Windows launcher no longer greps “mint not armed” / funding UTXO. |
| M-6 | Med | **Docs only.** `-solomining` remains RPC policy; help text says it is not consensus. |
| L-1 | Low | **Applied.** Reserved nonce must be 32 zero bytes. |
| L-2–L-4 | Low | Not in this pass. |

### H-1 — External GBT miners must reconstruct a 3-push `scriptSig` that docs still describe as 2-push

**Severity:** High (SOLO integration / hashrate stall). **Not** a consensus theft bug.

**Technical cause.** Consensus `BuildCoinbaseCommitmentScriptSig` / `ParseCoinbaseConvergenceRootScriptSig` require exactly:

1. BIP34 height
2. `OP_PUSHBYTES_32` C
3. `OP_PUSHBYTES_32` paid-winner txid (all-zero iff this coinbase does not mint)

ConnectBlock checks C **and** the paid-winner push (`validation.cpp` around the `ValidateCoinbaseConvergenceRootScriptSig(..., own_paid_txid)` call).

GBT does not return `coinbasetxn` or a `default_coinbase_scriptsig` hex (`mining_basic.py` asserts `'coinbasetxn' not in tmpl`). `core/doc/mining-pool-gbt.md` §2.0 still says “exactly two data pushes” and “no third push”. `consensus/convergenceres.h` V1 container comment still says `<BIP34 height> <OP_PUSHBYTES_32 C>`. P0-011 §5 still says `scriptSig` is `<height> <C>`. Functional tests (`blocktools.coinbase_height_convergence_script`) already use three pushes.

A miner who copies `convergence_root` after height and stops — following the live mining doc — is consensus-invalid. A miner who omits the zero paid-winner push on non-minting blocks is also invalid.

**Fix (applied).** Docs and `convergenceres.h` are three-push. GBT serves `default_coinbase_scriptsig` + `paid_winner_txid` + `rigor_coinbase_immutable`. Dummy `coinbasetxn` is still omitted so miners do not copy the all-zero P2WPKH. No fourth push / extranonce slot.

**Consensus impact:** Documentation / RPC shape only, if the served scriptSig is the same bytes `CreateNewBlock` already puts on the template. No rule change.

**Test plan.**

- Unit: GBT JSON contains a scriptSig that `ParseCoinbaseConvergenceRootScriptSig` accepts and that matches `CreateNewBlock`.
- Negative: two-push coinbase → `bad-cb-convergence-root` / `convergence-root-cb-missing-paid-winner-push`.
- Functional: external reconstruction from GBT fields (including the all-zero third push when `default_res_mint` is omitted) mines and connects.
- Doc test or lint: mining-pool-gbt.md must not say “two pushes”.

---

### M-1 — GBT still advertises BIP22 transaction / prevblock mutability

**Severity:** Medium (attack surface invitation). Fail-closed today.

**Technical cause.** `aMutable` is `time`, `transactions`, `prevblock`. A GBT client that drops a foreign convergence ticket must rebuild C over the windowed set (`convergence_txids` comment in `rpc/mining.cpp` already says this). Forgetting that is not exploitable; it is rejected. Advertising mutability still points integrators at the most dangerous reconstruction job.

**Fix (applied).** SOLO GBT advertises `mutable: ["time"]` and `rigor_coinbase_immutable: true`. `nNonce` remains the grind. Custom tx selection must request a new template.

**Consensus impact:** None (RPC advertisement).

**Test plan.** GBT fixture asserts `mutable` policy. Existing G-2 censorship test still passes via `prioritisetransaction` / empty mempool, not via silent GBT mutation.

---

### M-2 — `generateblock` does not rebuild C after inserting caller txs

**Severity:** Medium (RPC footgun). Fail-closed.

**Technical cause.** `generateblock` calls `CreateNewBlock` with a **null** mempool (coinbase-only template, mint already on the coinbase if the parent named a winner), then appends the caller’s txs and only `RegenerateCommitments` (witness merkle). C and paid-winner in `scriptSig` are not recomputed. `TestBlockValidity` then runs; adding a convergence tx yields `bad-cb-convergence-root` as an RPC error.

The `vtx.size() != 1` guard is a leftover from a **body** automint design. Mint is now a coinbase **output**, so `vtx.size()` stays 1 and the guard never fires. S-5 in `feature_res_solo_security.py` correctly uses `generateblock([], ...)` as a valid payout block.

**Fix (applied, revised by R-5).** `RebuildTemplateCommitments` rewrites C / paid-winner **and** regenerates the witness commitment / merkle root. Wrong call order is no longer expressible at the `generateblock` site. The `vtx.size() != 1` automint-as-second-tx guard is gone.

**Consensus impact:** None (RPC).

**Test plan.** `generateblock` with a mempool convergence txid must produce a connecting block (or keep failing closed until the fix lands). Empty-tx payout (S-5) must keep working.

---

### M-3 — `pool/` still implements Stratum extranonce and a payout layer

**Severity:** Medium (repo / product-boundary). Out of SOLO release scope.

**Technical cause.** `pool/block-assembler/src/coinbase.rs` writes extranonce into an `OP_RETURN` gap; `pool/stratum-server`, share-validator, and mint-engine reconstruct `coinb1‖extranonce1‖extranonce2‖coinb2`. Docs (`docs/mining/RIGOR_POOL_STACK_ARCHITECTURE.md`, `docs/architecture/RIGOR_EXECUTION_MODEL.md`) still describe pool-authoritative assembly. PRE-MAINNET-SOLO-INVENTORY correctly marks this **not shipped**.

This is exactly the class of surface the task says not to introduce: extranonce expansion, pool payout, a second reward path. Extra `OP_RETURN` is consensus-legal and would **not** by itself steal RES — but wiring it would reintroduce Stratum job state the SOLO design spent effort removing from `scriptSig`.

**Fix (applied as quarantine notice).** `pool/README.md` states the directory is not part of the shipped SOLO / `rigord` release. Directory not deleted. Do not add OP_RETURN extranonce to `core/`.

**Consensus impact:** None unless pool output is later treated as consensus.

**Test plan.** Release inventory: shipped tarball / installer does not contain `pool/stratum-server`. Optional CI grep that `core/src/node/miner.cpp` never grows an extranonce gap.

---

### M-4 — Dead assembler RES-fee path still looks like a second reward

**Severity:** Medium (hidden-path hygiene). Not payable on production ConnectBlock.

**Technical cause.** `CreateNewBlock` still has `nFeesRes` / `-miningresfeeaddr` / mockable bootstrap outputs. Production asserts `nFeesRes == 0`. ConnectBlock never increments `nFeesRes` in the tx loop; RES coinbase must equal `0 + bootstrap + mint`. Extra miner-chosen witness-v1 RES on mainnet is `bad-cb-res-amount`.

Operators can still believe `-miningresfeeaddr` is “where I get the RES prize.” It is only an arming / bootstrap sink on mockable chains. The prize pays the winner.

**Fix (applied, revised by R-2).** The first attempt gated the `nFeesRes` output branch on `IsMockableChain()`. Post-implementation review found that gate was unreachable: `Assert(nFeesRes == 0)` runs unconditionally *before* the branch, and `nFeesRes` is never incremented anywhere in the assembler. Gating dead code made it look live.

The branch, the `BlockAssembler::nFeesRes` member, its `resetBlock()` initialiser, the unconditional `Assert`, and the `res_fees` log field are now **deleted**. `ResolveMiningResFeeScript` survives only to place the mockable-chain `-reswalletbootstrap` output, and `-miningresfeeaddr` is ignored (with a warning) on non-mockable chains. `ConnectBlock`'s `nFeesRes` term is deliberately **kept**: it is an explicit line in the `bad-cb-res-amount` budget equation, and replacing it with a literal `0` would make that equation unreadable.

**Consensus impact:** None if ConnectBlock predicates stay exact-match.

**Test plan.** Mainnet params: GBT coinbase reconstruction never includes a non-mint RES output. Attempted extra RES output → `bad-cb-res-amount`.

---

### M-5 — Windows SOLO launcher still checks retired “mint not armed” / funding-UTXO errors

**Severity:** Medium (operator). `installer/windows/rigor-miner.cmd`.

**Technical cause.** The launcher greps GBT stderr for “mint assembly is not armed” / “no RES receive script” / “automint ORU fund UTXO is missing”. Current `CreateNewBlock` appends mint from the parent draw without a funding input and without requiring `-miningresfeeaddr`. Stale checks train operators to think mint is an optional miner-funded extra.

**Fix (applied).** Launcher no longer greps “mint not armed” / automint funding UTXO. Failure hints tell operators to copy `default_coinbase_scriptsig`.

**Consensus impact:** None.

**Test plan.** Launcher dry-run against a node that has a parent winner and no `-miningresfeeaddr` must not refuse to start.

---

### M-6 — `-solomining` skips peer and IBD gates

**Severity:** Medium (operational). RPC policy only.

**Technical cause.** On non-test chains, GBT requires peers and not-IBD unless `-solomining`. That flag does not change ConnectBlock. An operator can mine a valid-looking private fork from genesis / a stale tip.

**Fix (docs only).** Flag kept. `-solomining` help and PRE-MAINNET inventory state it is RPC policy, not a consensus bypass. Did not add a MinimumChainWork gate.

**Consensus impact:** None.

**Test plan.** Existing GBT connection tests. Checklist item on PRE-MAINNET-CHECKLIST.

---

### L-1 — BIP141 witness reserved value remains an unconstrained extra nonce

**Severity:** Low / freeze choice.

**Technical cause.** BIP141 leaves the 32-byte coinbase witness reserved value unconstrained. The node writes zeros. Changing it requires regenerating the witness commitment. It does not touch C or mint. `mining-pool-gbt.md` already says SOLO should leave it.

Given no compatibility burden, RIGOR *could* require the reserved value to be 32 zero bytes (shrink miner-controlled surface). That is a consensus freeze, not a bugfix.

**Fix (applied).** Consensus: reserved nonce must be 32 `0x00` (`bad-witness-reserved-nonce`, `BLOCK_CONSENSUS`). Checked after merkle match so a mutated nonce without a rebuilt commitment stays `bad-witness-merkle-match` / `BLOCK_MUTATED`.

**Consensus impact:** Hard rule, pre-mainnet only. Safe because there is no historical chain.

**Test plan.** `feature_res_solo_security.py` case L-1: (1) mutate reserved nonce without rebuilding the commitment → `bad-witness-merkle-match`; (2) `add_witness_commitment(nonce=1)` → `bad-witness-reserved-nonce`. `p2p_segwit.py` cannot run on RIGOR (`-testactivationheight` is rejected) and is not evidence.

---

### L-2 — Test helper still describes CVGE / RIGR coinbase tags as layout

**Severity:** Low. `core/test/functional/test_framework/rigor_block_builder.py`.

**Fix.** Strip leftover OP_RETURN mint/draw language from the helper module docstring so tests do not reintroduce a second commitment.

**Consensus impact:** None.

---

### L-3 — Multiple BIP141 commitments: last one wins

**Severity:** Low. Same as Bitcoin. Extra zero-value `OP_RETURN` cannot pay RES.

**Fix.** Optional: reject more than one witness commitment. Not required for issuance safety.

---

### L-4 — GBT dummy coinbase pays `WitnessV0KeyHash{}` (20 zero bytes)

**Severity:** Low. A miner who submits the dummy without replacing `vout[0]` burns ORU subsidy to an all-zero P2WPKH. RES mint is unaffected.

**Fix.** If `coinbasetxn` is served (H-1), the dummy is internal only. Otherwise document “must replace `vout[0]`”.

---

## 3.1 Post-implementation review (R-series)

The remediation above was re-reviewed against `ConnectBlock` after it landed. All R-series items are now fixed.

| ID | Sev | Item | State |
|---|---|---|---|
| R-1 | High | `generateblock` released `cs_main` between template creation and the commitment rebuild | **Fixed** |
| R-2 | High | The M-4 gate was unreachable dead code; `nFeesRes` deleted from the assembler | **Fixed** |
| R-3 | Med | Window/parent failure wrote an all-zero C into `scriptSig`. `CreateNewBlock` and `RebuildTemplateCommitments` now require `IsSet()` before writing | **Fixed** |
| R-4 | Med | Parent-draw → winner derivation shared as `GetTemplatePaidWinner`. Remaining copies: `ConnectBlock` (authoritative) and `CreateNewBlock` (also appends mint) | **Fixed** |
| R-5 | Med | `RebuildTemplateCommitments` rewrites scriptSig then witness+merkle. Wrong order is no longer expressible at the `generateblock` call site | **Fixed** |
| R-6 | Med | `-miningresfeeaddr` / `-blockversion` parsed into `BlockAssembler::Options`. Non-mockable `-miningresfeeaddr` rejected in `AppInitParameterInteraction` | **Fixed** |
| R-7 | Med | GBT `paid_winner_txid` is `CHECK_NONFATAL` on parse of the node-written scriptSig. Field kept (copy-bytes path is still `default_coinbase_scriptsig`) | **Fixed** |
| R-8 | Med | Dead L-1 case removed from `mining_basic.py` and `p2p_segwit.py`. `mine_chain` genesis-time fix kept. `scantxoutset` RPCResult gap is out of this task; P0-014 regression evidence is `feature_res_solo_security.py` + `miner_tests` | **Fixed** |
| R-9 | Low | `feature_res_solo_security.py` L-1 asserts nonce=1 without a rebuilt commitment → `bad-witness-merkle-match`, and with a rebuilt commitment → `bad-witness-reserved-nonce` | **Fixed** |
| R-10 | Low | Public assembler API is `RebuildTemplateCommitments` (bool+err). `RegenerateCommitments` skips erase when there is no commitment. `RefreshCoinbaseWitnessAndconvergence` moved to `test/util/mining.{h,cpp}` | **Fixed** |

**Regression evidence:** `miner_tests`, `validation_block_tests`, `blockfilter_index_tests`, `paid_winner_index_tests`, `convergence_root_tests`, `convergence_consensus_tests`, and `feature_res_solo_security.py` (V/S/A/G + L-1 including reverse order).

---

## 4. What was verified sound (no finding)

| Claim | Evidence |
|---|---|
| Body cannot forge `OP_RES_MINT` issuance | `CheckTransaction` + `CheckTxInputs` |
| Miner cannot change mint amount | `payout->nValue != GetResMintReward(h)` |
| Miner cannot replace winner | `CheckResRewardOwner` + parent `ResolveConvergenceDraw` |
| Winner-must-mint / must-not-mint | ConnectBlock after evaluator |
| C and paid-winner are PoW-committed | byte-exact `scriptSig`; no extranonce |
| This block cannot re-roll its own draw | A-8 parent-only σ inputs |
| RFC-018 burn cannot be claimed | `oru_coinbase_value > limit` → `bad-cb-amount` |
| Extra mainnet coinbase RES | exact `bad-cb-res-amount` |
| PCORB not a mint restatement | `bad-pcorb-carrier` |
| SOLO RPC path does not speak Stratum | `getblocktemplate` / `submitblock` / `generatetoaddress` |
| Censorship ≠ free draw | G-2 in `feature_res_solo_security.py` (policy) |

Prior solo suite (`docs/security/PRE-MAINNET-SOLO-SECURITY.md`, 2026-08-18) already PASSed V/S/A/G themes. This audit is a code-level re-read of the template/mint boundary, not a re-run of that functional file.

---

## 5. Principle score

### Minimal Consensus Surface

**Mostly holds.** The consensus objects a block must get right are: header PoW, merkle/witness commitments, window C, paid-winner push, at most one canonical `OP_RES_MINT` bound to the parent winner, ORU/RES coinbase budgets. Body tx selection is not a mint oracle.

**Gaps closed by this pass:** GBT reconstruction (H-1) and BIP141 reserved nonce (L-1). Remaining product-surface gap is `pool/` (M-3, not shipped).

### Solo Mining First

**Holds in `core/`.** Native grind is `nNonce` (+ `nTime`). No Stratum session, no extranonce in `scriptSig`, no pool share validator on the connect path. `pool/` remaining in the official tree (M-3) is the main contradiction with the *product* principle, not with consensus.

### No Hidden Reward Path

**Holds at ConnectBlock.** One ORU miner payout budget; one RES mint budget equal to `W(h)` or zero. No body mint, no PCORB prize, no pool intermediate. The assembler no longer has a RES fee accumulator to re-arm (§3.1 R-2); `-miningresfeeaddr` places only the mockable-chain bootstrap output.

---

## 6. Implementation order (done this pass)

Did **not** start from pool compatibility.

1. **H-1** — Three-push docs + GBT `default_coinbase_scriptsig` / `paid_winner_txid`.
2. **M-2** — `RebuildTemplateCommitments` after `generateblock` tx insert.
3. **M-4 / M-5** — Delete `nFeesRes` from the assembler (see R-2); fix Windows launcher.
4. **M-1** — GBT `mutable: ["time"]`.
5. **M-3** — Quarantine notice in `pool/README.md` (directory kept).
6. **L-1** — Consensus: reserved nonce must be 32 zeros.

---

## 7. Code map

| Topic | Location |
|---|---|
| Template assembly | `core/src/node/miner.cpp` `CreateNewBlock` |
| Mint append | `core/src/node/auto_res_mint.cpp` |
| GBT / submit / generate | `core/src/rpc/mining.cpp` |
| scriptSig parse / C | `core/src/consensus/convergence_root.cpp` |
| Mint evaluator | `core/src/consensus/rigor_res.cpp` |
| Winner / must-mint | `core/src/validation.cpp` ConnectBlock |
| RES coinbase budget | `core/src/connect_block_rigor_checks.cpp` |
| Body mint ban | `core/src/consensus/tx_check.cpp`, `tx_verify.cpp` |
| Owner bind | `core/src/consensus/res_owner.cpp` |
| GBT documentation (three-push, copy-bytes fields) | `core/doc/mining-pool-gbt.md` |
| Prior solo functional | `core/test/functional/feature_res_solo_security.py` |

---

## 8. Out of scope

- P2P eclipse / header withholding
- Majority-hashrate window censorship (economic; `W = 32` is not a minority-exclusivity proof)
- Pool share accounting, Stratum difficulty, YiiMP web
- Coq refinement of the mining RPC (ConnectBlock mint coupling is modeled elsewhere; GBT JSON is not)

---

## 9. Go / no-go

| Question | Answer |
|---|---|
| Can miner software steal RES or rewrite the winner via template tricks? | **No** |
| Can untrusted tx selection become a consensus mint oracle? | **No** |
| Is the SOLO path conceptually simple (nonce grind + node coinbase)? | **Yes, if miners use the node assembler** |
| Is the GBT-reconstruction path simple and safe to document as-is? | **Yes after H-1** — copy `default_coinbase_scriptsig` |
| Should pool extranonce / payout be added to make Bitcoin miners happier? | **No** |
| Code changes in this task? | **Applied (P0-014 remediation).** |
