# RIGOR RES Monetary Policy — Security Audit

**Task:** `TASK-RIGOR-RES-MONETARY-POLICY-SECURITY-AUDIT-P0-001`  
**Date:** 2026-08-19  
**Layer:** Consensus read-only audit (documentation alignment only)  
**Consensus impact:** **NONE** — no consensus, mint, validation, wallet, miner, or RFC files were modified  
**`equivalence_claimed`:** `false`

| Field | Value |
|-------|-------|
| Audit result | **PASS** (RC0) |
| RC0 release | **Safe** for RES monetary-policy consensus rules, with one long-horizon liveness finding that is not RC0-reachable |
| Critical findings | **None** |
| Highest open finding | **High** — post-schedule winner-must-mint deadlock (height ≥ 52,710,000) |
| Formal coverage | **Partial** (cap + schedule + winner-obligation + dual-pool **YES** as model theorems; maturity / owner-PCORB **NO**; `equivalence_claimed` **false**) |

This audit does **not** redesign RES. It determines whether the shipping RES
monetary policy is mathematically sound, consensus-safe, supply-bounded,
economically consistent, and resistant to inflation, unauthorized minting,
and state manipulation.

Normative numeric locks: `core/src/consensus/rigor_consensus_lock.h`.  
Normative mint coupling: `core/src/consensus/rigor_res.cpp`, `core/src/validation.cpp` ConnectBlock.  
Normative transfer / maturity: `core/src/consensus/tx_verify.cpp`.

---

## 1. Executive summary

RES is a native UTXO asset at the same protocol layer as ORU. It is classified
from `scriptPubKey` (Witness v1, 32-byte program, not CVG2). It is not a
colored coin, wrapped token, smart-contract token, inscription, rune, or
external asset layer.

**Implemented monetary design (RFC-017, shipping):**

- Hard cap `RES_MAX_SUPPLY = 21,000,000 RES` (`21_000_000_000_000` atoms).
- Per successful mint the paying block must create **exactly one** Witness v1
  body UTXO of amount `W(h) = GetResMintReward(h)`, not a fixed 100 RES.
- `W(0) = 40.000000 RES`. Reward decays as
  `floor(W0 · (k+1)^(-3/2))` with `k = height / 210_000`, then
  `W(k) = 0` for `k > 250`.
- At most one mint per block (`M(h) ∈ {0,1}`).
- New RES originates only from a valid Convergence draw winner, paid in the
  **block body**, never from production coinbase (except a mockable-chain test
  backdoor that is consensus-inert on mainnet).
- Ordinary transfers are conserved: `res_in ≥ res_out`.
- Body mint UTXOs are unspendable for `nCoinbaseMaturity = 100` blocks.

**Cap is stronger than an inductive runtime check.** Even if every block from
genesis through epoch 250 mints, maximum conceivable issuance is

```
210_000 × 99_450_437 atoms = 20_884_591.770000 RES  <  21_000_000 RES
```

(margin 115,408.230000 RES / 0.55%). This is a compile-time
`static_assert` and a Coq `vm_compute` lemma. The runtime
`bad-rigor-res-max-supply` gate remains as defense-in-depth.

**RC0 conclusion:** the shipping consensus rules are supply-bounded, fail-closed
on unauthorized mint, and reorg-safe for the UTXO / maturity / cumulative-supply
caches. Public whitepaper and website copy still described the pre-RFC-017
fixed 100.00 RES mint; those official surfaces were corrected in this task.
No consensus behavior was changed. Consensus/unit tests that already pin
RFC-017 **PASS**. Finding M-2 is **Resolved**: the functional harness uses
six-decimal `RES_COIN = 1_000_000`, and structured C++ fuzzers
`res_checktxinputs` and `res_mint_eval` are active in the test harness for
Witness v1 RES paths (transfer conservation, immature-mint spends, and
Owner / PCORB / domain mint evaluation). Dynamic assurance does not change
consensus rules and does not flip `equivalence_claimed`.

**Do not patch** the post-schedule liveness hole without explicit human
approval. It cannot inflate RES. It can stall block production after the
schedule terminates **if** a draw still produces a winner.

---

## 2. Current RES monetary design

```
PoW  →  ORU coinbase
         ↓
ORU Convergence (CVG2; ORU leaves circulating supply; becomes weight f_i)
         ↓
Windowed candidate set F (W = 32)
         ↓
Deterministic draw at paying height h (parent-committed σ)
         ↓
If winner: body mint of exactly W(h) RES to winner’s declared Witness v1 program
If no winner: zero RES mint
```

Protocol-enforced properties:

| Property | Rule | Location |
|----------|------|----------|
| Native asset | Witness v1 / 32-byte program; not CVG2 | `IsResScript`, `AssetTypeFromScriptPubKey` |
| Unit | `1 RES = 1_000_000` atoms (six decimals) | `RES_COIN` |
| Cap | `S_RES ≤ 21_000_000` | `RES_MAX_SUPPLY` |
| Mint amount | `W(h) = GetResMintReward(h)` | `rigor_consensus_lock.h` |
| Genesis amount | `40.000000 RES` | `RES_REWARD_GENESIS` |
| Epoch | 210,000 blocks (same cadence as ORU halvings) | `RES_REWARD_EPOCH` |
| Termination | `W(k) = 0` for `k > 250` | `RES_REWARD_LAST_EPOCH` |
| Uniqueness | At most one scored mint per block | `EnforceResMintUnique` |
| Winner coupling | Winner ⇒ exactly one mint; no winner ⇒ no mint | ConnectBlock |
| Payee | Winner-declared program, not miner | `CheckResRewardOwner` |
| Transfer | `res_in ≥ res_out` (burn allowed; mint forbidden) | `CheckTxInputs_RES_DualPool` |
| Coinbase | Production RES coinbase value must be 0 | `bad-cb-res-amount` |
| Maturity | Body mint spendable at `nSpendHeight − nHeight ≥ 100` | `CheckTxInputs` |

Economic design (not a price promise):

- **ORU** is designed to carry the security budget, fees, and Convergence
  entry cost, including a permanent 125 ORU tail.
- **RES** is designed to be the scarcity / store-of-value asset.
- RES creation is intended to require economic cost: committed ORU leaves
  `S_ORU` and becomes draw weight. It does **not** convert 1:1 into RES.
- The RFC-017 schedule is designed so the protocol cost floor per RES,
  `C / W(k)` with `C` a frozen function of `T`, rises with height and does
  not retarget against demand.

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + GetResMintReward(h+1) · M(h+1)
S_RES(h) ≤ 21,000,000
```

`M(h)` is not calendar-guaranteed. `W(h)` is a height-indexed protocol
function. Public copy must not collapse those two facts.

---

## 3. Supply security analysis (A)

### 3.1 Limit

`Total_RES(t) ≤ 21,000,000` is protocol-enforced in atoms:

```cpp
inline constexpr CAmount RES_MAX_SUPPLY = static_cast<CAmount>(21000000LL) * RES_COIN;
// RES_COIN = 1_000_000  →  21_000_000_000_000 atoms
```

File: `core/src/consensus/rigor_consensus_lock.h`.

### 3.2 Where the limit is enforced

**Layer 1 — compile-time schedule identity (primary).**

```text
static_assert(ResScheduleSum() <= RES_MAX_SUPPLY / RES_REWARD_EPOCH);
static_assert(ResScheduleSum() == 99450437);
```

Assumptions used by the identity: at most one mint per block, epoch length
exactly 210,000, `W(k) = 0` after epoch 250. No assumption about demand.
This is consensus-critical: changing `W` without updating the asserts is a
build failure.

**Layer 2 — ConnectBlock runtime gate (defense-in-depth).**

`Chainstate::ConnectBlock` in `core/src/validation.cpp`:

1. Sum body mint `GetValueOut(ASSET_RES)` over scored mint txs.
2. Require that sum equals `GetResMintReward(pindex->nHeight)` when a mint
   exists (`bad-rigor-res-amount`).
3. `res_minted_before = GetChainResMintTotal(pindex->pprev)`.
4. Overflow / cache poison → `bad-rigor-res-minted-overflow`.
5. Range → `bad-rigor-res-mint-range`.
6. `res_minted_before > RES_MAX_SUPPLY - res_reward_expected` →
   `bad-rigor-res-max-supply` (saturating subtract; no `int64` wrap).

**Layer 3 — chain index cache.**

`GetChainResMintTotal` walks parent links, refuses negative or above-cap
partial sums, and saturating-adds per-block mint. Disconnect invalidates
`m_chain_res_mint_total = -1` so a reorg cannot reuse another branch’s
cumulative.

**Layer 4 — Coq.**

`formal_verification/coq/ResSchedule.v` theorem `max_issuance_within_cap`.  
`ReplayReorgCore/SupplyConservation.v` theorem `res_mint_respects_cap`.  
P1 (model, not C++): `ResMintObligation.v` winner-must-mint / H-1;
`DualPoolConservation.v` `res_in ≥ res_out`. Accepted mismatches:
[RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md](./RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md) §9.
`equivalence_claimed` remains **false**.

### 3.3 Overflow / integer safety

| Site | Safety |
|------|--------|
| `ResIntegerSqrt` / `ResRewardForEpoch` | `uint64` intermediates; max `(251^3 << 32) ≈ 6.79e16`, factor ~272 below `2^64`. Epoch > 250 returns 0 before the multiply. |
| `GetResMintReward` | Negative height → 0. Large height → epoch > 250 → 0. |
| `GetChainResMintTotal` | Saturating add against `RES_MAX_SUPPLY`; fail-closed `-1`. |
| `CheckTransaction` | Per-output `nValue > MAX_MONEY` rejected. `MAX_MONEY = 21e6 * 1e8` sats; entire RES cap (`2.1e13`) is inside this sanity bound. |
| Mint `res_out` accumulation | ConnectEvaluateResMintTx requires `n_res == 1`, so no multi-output sum overflow on the mint path. |
| `GetValueOut` | `MoneyRange` on each add; throws rather than wrapping. Invalid blocks never connect. |

**Failure behavior:** any failed predicate is `state.Invalid(..., BLOCK_CONSENSUS)`
or `TX_CONSENSUS`. The block / tx is not applied. Fail-closed.

**Consensus-critical:** yes. A node that skipped the ConnectBlock cap gate
would still be bounded by the schedule identity **if** it still enforced
`W(h)` and uniqueness. The runtime gate is the last explicit
`S_RES ≤ 21M` check on a connected block.

---

## 4. Mint security analysis (B)

### 4.1 Creation path

```
ORU convergence tx (body, ≥ 1 ORU, CVG2)
        ↓
Parent block commits F (Convergence Root + draw candidate root in coinbase)
        ↓
Paying block h reconstructs parent_draw_set; ResolveConvergenceDraw
        ↓
CheckTxInputs: ORU-funded RES output allowed only as mint shape
  (parseable PCORB, proof inequality, exactly one RES output of W(h))
        ↓
ConnectEvaluateResMintTx: domain ORU-only; parent-bound σ; membership in F;
  binding; VerifyConvergeProof; owner program; amount == W(h)
        ↓
EnforceResMintUnique (at most one scored mint)
        ↓
Winner ⇔ mint count == 1; no winner ⇔ mint count == 0
        ↓
ConnectBlock amount == W(h); cumulative ≤ cap
        ↓
AddCoins + COIN_FLAG_RES_MINT on the single Witness v1 output
```

### 4.2 Only protocol paths originate RES

| Attempt | Result |
|---------|--------|
| Coinbase RES on mainnet / testnet | `bad-cb-res-amount` (`VerifyCoinbaseConvergenceAndResLayout`). Expected value is `nFeesRes + bootstrap`; `nFeesRes` is asserted 0; bootstrap is 0 unless `IsMockableChain() && -reswalletbootstrap`. Mainnet `m_is_mockable_chain = false`. |
| Ordinary ORU payment creating RES | `mixed-asset-output` unless mint shape passes. |
| RES transfer with `res_out > res_in` | `bad-txns-res-in-belowout`. |
| Dual-pool ORU+RES inputs minting | Domain is RES (`GetTransactionInputAsset`); mint evaluation returns without scoring; cannot create net-new RES. |
| PCORB on RES-domain (old “claim mode”) | Removed (SECURITY-001-A8). RES spends are Ed25519 key-path only. |
| Wrong amount / dust / split outputs | `bad-rigor-res-amount`. |
| Mint without winner | `bad-rigor-res-without-winner`. |
| Winner without mint while `W(h) > 0` | `bad-rigor-res-winner-mismatch`. |
| Winner without mint after schedule end (`W(h) = 0`) | Valid. Mint is not authorized; mint count must be 0 (Finding H-1 remediated). |
| Redirect payee | `bad-rigor-res-owner-mismatch`. |
| Forged σ / height / bind / membership | named `bad-rigor-res-*` / `bad-sigma-convergence-not-in-convergence-root-set`. |
| \|F\| < 1 with a mint | `bad-rigor-res-trigger-underflow`. |
| Two scored mints | `bad-rigor-multiple-res`. |
| RPC / createrawtransaction / miner template | Still pass `CheckTxInputs` then ConnectBlock. Wallet and miner cannot bypass consensus. |
| `-reswalletbootstrap` on production | `DevTestResWalletBootstrap` returns 0 when `!IsMockableChain()`. |

**No direct RES mint path exists** on production consensus. The only issuance
shape is ORU-funded body mint coupled to the parent draw.

### 4.3 Amount, rounding, boundaries

| Case | Behavior |
|------|----------|
| Amount | Integer `W(h)` atoms. Floor division; `W_int(k) ≤ W_real(k)` (safe side of the cap). No floating point. |
| Zero reward (`h` in epoch ≥ 251, or negative) | `GetResMintReward` returns 0. Mint shape and ConnectEvaluateResMintTx reject (`res_reward <= 0` → `bad-rigor-res-amount`). ConnectBlock does **not** require a mint: winner-must-mint is gated on `W(h) > 0` (Finding H-1 remediated). |
| Genesis | `W(0) = 40_000_000` atoms. |
| Epoch boundary | `GetResMintReward(210000) < W(0)` (compile-time assert). |
| Maximum per mint | `W(h) ≤ W(0)` (Coq `res_reward_for_epoch_le_genesis`). |
| Dust | RES outputs must be ≥ 1 atom (`IsValidResOutputValue`). A mint of `W(h)` with `W(h) ≥ 10058` through epoch 250 is above dust. |

CheckTxInputs mint-shape is **not** a draw authorization. A parseable mint
may enter the mempool and still fail ConnectBlock (documented solo-security
V-1). That is not an inflation path.

---

## 5. Maturity rule analysis (C)

### 5.1 State transition

```
RES mint included in block at height H
        ↓
UTXO created with nHeight = H, nFlags bit 0 = COIN_FLAG_RES_MINT
        ↓
Immature while nSpendHeight − H < nCoinbaseMaturity
  (nCoinbaseMaturity = 100 = RES_MINT_MATURITY = COINBASE_MATURITY)
        ↓
First spendable height: nSpendHeight ≥ H + 100
        ↓
Mempool uses nSpendHeight = tip + 1
  → first mempool accept when tip ≥ H + 99
  → mint tx depth (Bitcoin convention) = 100
```

Ordinary RES transfers and change are **not** delayed by this flag. They
still cannot mint (`res_in ≥ res_out`).

### 5.2 Consensus enforcement

Predicate in `AccumulateTxInputPools` (`tx_verify.cpp`), used by both
`AcceptToMemoryPool` and `ConnectBlock` via `CheckTxInputs`:

```
coin.RequiresMaturity() && (nSpendHeight - coin.nHeight < nCoinbaseMaturity)
  → coinbase: bad-txns-premature-spend-of-coinbase
  → else:     bad-txns-premature-spend-of-res-mint
```

`Coin::RequiresMaturity()` is `IsCoinBase() || IsResMint()`.

Flag is set in `ApplyBodyResMintFlag` / `TxOutputIsBodyResMint` when the
creating tx is a non-coinbase parseable mint with exactly one RES output.

### 5.3 Bypass checks

| Path | Bypass? |
|------|---------|
| Wallet coin selection | No. `IsTxImmatureResMint` excludes mint UTXOs. Wallet is non-consensus. |
| RPC / createrawtransaction | No. Mempool `CheckTxInputs` rejects premature spends. |
| Miner-created txs in a block | No. ConnectBlock calls the same `CheckTxInputs`. |
| Reorg | No. Flag restored from undo; maturity follows surviving mint height. |

Wallet UI uses `(nCoinbaseMaturity + 1) - depth`, so Overview can still show
“immature” for one confirmation after the mempool would accept the spend.
That is the same Bitcoin coinbase UI lag. It is **more** conservative than
consensus, not a bypass. Do not loosen consensus to match the wallet.

Mockable `-reswalletbootstrap` coinbase RES is a **coinbase** UTXO
(`fCoinBase`), gated by coinbase maturity, not `COIN_FLAG_RES_MINT`.
Production coinbase RES is 0.

---

## 6. Reorganization safety analysis (D)

### 6.1 Disconnect

`Chainstate::DisconnectBlock`:

- Restores spent inputs from undo, including `COIN_FLAG_RES_MINT` in the
  dummy undo byte.
- Requires unspent mint outputs to still match `TxOutputIsBodyResMint`
  (unclean disconnect otherwise).
- Sets `m_chain_res_mint_total = -1` and `m_chain_oru_total = -1`.
- Invalidates runtime supply mirror / witness for that height.

A disconnected mint UTXO is gone. A spend of it is missing-inputs.

### 6.2 Competing / orphan blocks

Cumulative RES is per `CBlockIndex` (per block hash), not per height.
Connecting an alternative block at the same height writes that block’s own
`m_block_res_mint_sum`. An orphan’s mint does not exist on the active chain.

To mint on a competing branch the branch must contain a valid parent draw,
winner, mint amount `W(h)`, and owner. Invalid convergence claims fail
ConnectEvaluateResMintTx on that branch.

### 6.3 Maturity after reorg

Maturity is `nSpendHeight - coin.nHeight`. After reorg, `nHeight` is the
height at which the surviving mint was connected. A mint cannot “skip”
maturity by moving to a shorter branch: if the mint block is not in the
surviving chain, the coin does not exist; if it is, depth is computed from
its new chain position.

Mempool entries that spend mint or coinbase are re-checked
(`GetSpendsCoinbase` covers `RequiresMaturity()`).

### 6.4 Duplicate mint across branches

There is one UTXO set per chainstate. Two branches cannot both credit the
same node’s active supply. Replay of an alternative history must re-satisfy
draw + unique mint + cap on that history. Coq `ReplayReorgCore` models
supply rollback + mint delta; it does not replace C++ UTXO undo tests.

Invalid claims (wrong F, wrong σ, wrong owner) cannot create RES on any
honest-valid chain.

---

## 7. Formal verification coverage (F)

`equivalence_claimed = false`. The Coq corpus is a strongly verified **model**,
not a C++ refinement proof.

| Property | Implemented formally | Artifact |
|----------|----------------------|----------|
| `RES_MAX_SUPPLY` 21M atoms | **YES** | `ResSchedule.v` `RES_MAX_SUPPLY` |
| Schedule `W(k)` / `GetResMintReward` | **YES** | `ResSchedule.v`; pins `schedule_sum = 99450437` |
| `max_issuance_within_cap` | **YES** | `ResSchedule.v` (`vm_compute`) |
| Runtime cap on mint delta | **YES** | `res_mint_respects_cap` |
| Per-block amount = schedule at header height | **YES** (model) | `ConnectBlockMint.v` `cb_res_mint_amount` |
| Reorg supply rollback | **YES** (model) | `ReplayReorgCore/SupplyConservation.v` |
| Winner-must-mint obligation (incl. H-1) | **YES** (model) | `ResMintObligation.v` `valid_res_mint_obligation` on `valid_block_connect`; `h1_winner_zero_reward_block_valid` |
| `res_in ≥ res_out` | **YES** (model) | `DualPoolConservation.v` `dual_pool_conservation`; mint exception `res_mint_shape` (not PCORB) |
| Golden vectors GV-RES-0001..0003 | **YES** (C++ harness) | `runtime_vector_harness.cpp`; GV-RES-0003 is a defensive cap-boundary state, not chain-producible under RFC-017 |
| Invalid mint **block reject** (owner, PCORB, domain) | **NO** as Coq theorems; **YES** as C++ unit/functional tests **and** structured fuzz | `res_proof_math_tests`, solo-security; `res_mint_eval` / `res_checktxinputs` |
| 100-block maturity | **NO** | C++ `res_orum_tests` + `feature_res_maturity.py` |
| Production coinbase RES = 0 | **NO** (Coq) | C++ `bad-cb-res-amount` |

**Missing formal properties (clear list):**

1. Body RES mint maturity (`nSpendHeight - nHeight ≥ nCoinbaseMaturity`).
2. Full mint predicate: ORU-only domain, F membership, owner program, unique mint as a composition of `ConnectEvaluateResMintTx` (P1 keeps `cm_draw_won` abstract).
3. Production coinbase RES must be zero.
4. C++ refinement / `equivalence_claimed` — still **false**. P1 accepted mismatches (over-cap delta zeroing, missing trigger-underflow, dual-pool predicate not wired into `apply_block`) and abstract `cm_res_mint_count` are documented in [RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md](./RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md) §9. They do not flip this flag.

P1 closed as **model YES**: winner exists ⇒ exactly one mint of `W(h)` while `W(h)>0`; `W(h)=0` ⇒ zero mints even if a winner exists (H-1); `res_in ≥ res_out` on ordinary txs. Not a C++ correspondence proof.

### 7.1 Accepted Model Abstractions & Mismatches (P1 Cycle)

Cross-check vs ConnectBlock / `CheckTxInputs`. Full tables:
[RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md](./RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md) §9.

These mismatches make the Coq model wider than C++. Since C++ strictly
rejects these cases, there is no production inflation risk. Full refinement
is deferred to a future cycle.

| Kind | Item |
|------|------|
| MISMATCH | Over-cap: Coq zeros `cb_res_mint_delta`; C++ `bad-rigor-res-max-supply` rejects (`validation.cpp`) |
| MISMATCH | No `bad-rigor-res-trigger-underflow` (`N < CONVERGENCE_DRAW_MIN_TXS_FOR_RES`) |
| MISMATCH | `dual_pool_conservation` is a predicate, not a conjunct of `apply_block` |
| ABSTRACT | `cm_res_mint_count` is a block field; C++ counts scored body txs after `ConnectEvaluateResMintTx` |

Do **not** set `equivalence_claimed` true. Do **not** patch C++ consensus for these model gaps.

---

## 8. Whitepaper and website consistency (G, H)

### 8.1 Pre-audit mismatch (now corrected on official surfaces)

Shipping consensus is RFC-017. Official whitepaper and `/documentation/whitepaper`
still described RFC-015 Occam **100.00 RES**, `1 RES = 100` atoms, “no calendar
curve” for the mint **amount**, and “≤ 210,000 successes”.

| Claim (old public copy) | Implementation |
|-------------------------|----------------|
| 100.00 RES per mint | `W(h)`; genesis 40.000000 RES |
| `1 RES = 100` base units | `RES_COIN = 1_000_000` |
| `RES_REWARD_AMOUNT` constant | Removed; `GetResMintReward(height)` |
| ≤ 210,000 successes to fill 21M | Variable `W(h)`; schedule cannot fill 21M even if every block mints |
| “No calendar curve” for amount | Amount is height-indexed; **whether** a mint occurs is still not calendar-guaranteed |

This task updated:

- `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`
- Bound chapter `docs/whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`
- Technical-paper structure `docs/whitepaper/RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md`
- Prior chapter drafts `02`, `04`, `07`, `09` (monetary figures only)
- Website `WhitepaperPage.tsx`, `WalletTopicPage.tsx` (maturity), `ProtocolCard.tsx`
- `docs/security/RES-ORU-MATURITY.md` issuance sentence

RFCs were **not** edited.

### 8.2 Residual stale surfaces (not blocking RC0)

Historical audit reports in `docs/whitepaper/` and some older security notes
still mention 100.00 RES as a past freeze. They are reports, not the live
paper. `docs/security/PRE-MAINNET-SOLO-SECURITY.md` S-2 still says
`RES_REWARD_ATOMS (100.00)` — residual.

Website Mining and Security Architecture pages do not state a 100.00 mint
amount. Mining correctly states that mining does not mint RES. Wallet
maturity is now documented as a 100-block consensus rule.

ProtocolCard previously wrote `Σ RES_in = Σ RES_out`. Transfers may burn
(`res_in > res_out`). Copy was aligned to `res_in ≥ res_out`.

Live draw economics: `E = 246`, expected wins per committed ORU ≈ `n / 1024`.
The whitepaper’s `2^240 / 2^256 = 1/65536` line remains a **teaching identity**
from RFC-014’s unit-corrected exponent and is labeled as such; it is not the
shipping `T`.

---

## 9. Identified risks

| ID | Severity | RC0 | Title | Notes |
|----|----------|-----|-------|-------|
| H-1 | **High** | **Mitigated** | Post-schedule winner-must-mint deadlock | After epoch 250, `W(h) = 0`. ConnectBlock now treats mint as unauthorized (`mint count` must be 0). A mint is still `bad-rigor-res-amount`. Miner omits assembly without throwing. Not a new issuance path. |
| M-1 | Medium (docs) | Mitigated | Public 100.00 RES copy vs RFC-017 | Official whitepaper + website corrected this task. Residual historical reports remain. |
| M-2 | Medium (tests) | **Resolved** | Functional harness two-decimal atoms; Witness v1 RES test gap | Harness `RES_COIN = 1_000_000`, `RES_REWARD_ATOMS = 40_000_000`, `res_atoms_in_tx_vouts` multiplies by `RES_COIN`. Structured C++ fuzzers (`res_checktxinputs`, `res_mint_eval`) implemented in test harness to cover Witness v1 RES paths. |
| L-1 | Low | Accept | Formal gaps (residual) | Maturity and Owner/PCORB/domain remain **not** Coq theorems. C++ dynamic assurance for those mint paths is now active (`res_mint_eval`, `res_checktxinputs`). Winner-obligation + `res_in ≥ res_out` are **YES** as Coq *model* theorems (P1). `equivalence_claimed` remains false. |
| L-2 | Low | Accept | Wallet maturity UI +1 | Same as Bitcoin coinbase; wallet more conservative than consensus. |
| I-1 | Info | Accept | `-reswalletbootstrap` | Mockable chains only; 1.00 RES coinbase. Mainnet `IsMockableChain() == false`. |
| I-2 | Info | Accept | Mempool mint shape without draw | ConnectBlock still fail-closed (solo V-1). |
| I-3 | Info | Accept | Runtime cap redundant under RFC-017 | Keep as defense-in-depth. |

No unauthorized mint, cap bypass, maturity bypass, or reorg double-mint path
was found in consensus.

---

## 10. Recommended actions

**Do now (documentation — this task):** keep official paper and site aligned
with `GetResMintReward` / six decimals / 40.000000 genesis reward.

**Do not do without human approval:**

1. Change `W(h)`, `T`, cap, uniqueness, owner binding, or maturity.

**H-1 (approved and implemented):** when `GetResMintReward(h) == 0`, a draw winner does
not authorize a mint. ConnectBlock requires `res_mint_tx_count == 0`. Miner skips
assembly without throwing. RFC-017 §4.4.

**Before mainnet (not RC0 blockers):**

1. Optional Coq P2: 100-block maturity (after Coin height/flags, if ever added).
2. Sweep residual “100.00 RES” strings in historical security reports.
3. Keep `equivalence_claimed = false`. Do not treat P1 YES cells as C++ refinement.

**RC0:** RES monetary policy is safe to ship relative to inflation,
unauthorized minting, supply bound, transfer conservation, maturity
enforcement, and reorg accounting.

---

## 11. Remediation (post-audit)

Human-approved follow-up. Not a new mint path. Cap, `T`, and `W(h)` are unchanged.

| Item | Change |
|------|--------|
| H-1 | `ConnectBlock`: mint required iff `GetResMintReward(h) > 0`. After epoch 250, winner + `mint_count == 0` is valid; a mint is still `bad-rigor-res-amount`. `CreateNewBlock` omits assembly without throwing. Unit pin: `rfc017_exhausted_schedule_rejects_mint`. RFC-017 §4.4. Coq (P1 model): `valid_res_mint_obligation` / `h1_winner_zero_reward_block_valid` plus supply half `cb_res_mint_delta_zero_when_amount_zero`. |
| M-2 | Functional harness `RES_COIN = 1_000_000` (six decimals; ORU remains eight). `RES_REWARD_ATOMS = 40_000_000`. `res_atoms_in_tx_vouts` multiplies RPC `value` by `RES_COIN`. Structured C++ fuzzers (`res_checktxinputs`, `res_mint_eval`) in `core/src/test/fuzz/` cover Witness v1 RES `CheckTxInputs` and `ConnectEvaluateResMintTx` (Owner / PCORB / domain). Consensus unchanged. |

---

## Appendix A — Validation performed

The original audit did **not** modify consensus. Existing tests were executed against
that tree. Section 11 records the subsequent H-1 / M-2 patch.

### Unit (`core/src/test/test_bitcoin`)

| Suite | Cases | Result |
|-------|-------|--------|
| `res_orum_tests` | 16 | PASS |
| `res_proof_math_tests` | 24 | PASS |
| `runtime_supply_invariant_tests` | 6 | PASS |
| `runtime_supply_state_tests` | 8 | PASS |
| `runtime_vector_harness_tests` | 8 | PASS |
| `convergence_consensus_tests` | 18 | PASS |
| `runtime_supply_coq_chas_adapter_tests` | 7 | PASS |
| `runtime_supply_witness_coq_adapter_tests` | 6 | PASS |
| `runtime_supply_witness_chain_tests` | 7 | PASS |
| `wallet_tx_semantic_tests` | 6 | PASS |
| `wallet_res_single_slot_tests` | 7 | PASS |
| `wallet_listunspent_asset_field_tests` | 3 | PASS |
| `rigor_fee_tests` | 16 | PASS |
| `alphaplus_security_boundary_tests` | 7 | PASS |
| `coins_tests/txundo_formatter_res_mint_flag_roundtrip` | 1 | PASS |

### Structured fuzz (post-audit; consensus unchanged)

Targets live in `core/src/test/fuzz/`. Inherited `coins_view` already called
`CheckTxInputs` but was not structured for Witness v1 RES. These two targets
are the dynamic assurance for Owner / PCORB / domain and dual-pool transfer
invariants. They hunt unnamed state combinations; they do not re-pin named
BOOST rejects.

| Target | Role | Local smoke (2026-08-19) |
|--------|------|--------------------------|
| `res_checktxinputs` | ORU / RES / CVG2 / immature-mint coins; assert no transfer inflation, no premature mint spend, no RES→ORU | **PASS** — 14 corpus files + empty stdin |
| `res_mint_eval` | Fixed parent draw set; mutate witness / owner / domain / amount / height; score honest shape or a named reject | **PASS** — 14 corpus files + empty stdin |

```
FUZZ=res_checktxinputs ./src/test/fuzz/fuzz /path/to/seeds
FUZZ=res_mint_eval ./src/test/fuzz/fuzz /path/to/seeds
```

### Functional (pre-mainnet RES security)

`SKIP_UNIT=1 ./test/functional/res_security/run_pre_mainnet.sh` and the suites run individually.

| Suite | Result | Notes |
|-------|--------|-------|
| `feature_res_window_reorg_res_attacks.py` | **FAIL** at M-1 | R-5 / M-5 passed. M-1 asserted a buried foreign ticket left F without `foreign_award_height`. Dump showed `mint_count=1` and `chain_res` corresponding to 40.000000 RES, with `draw_winner_txid` null (likely deferred-draw field = next draw). Not an inflation path. Needs a dedicated harness pass, not a consensus patch. |
| `feature_res_solo_security.py` | **FAIL** at S-4 amount pin | V-1, V-2, V-3, V-4, V-5, S-1, A-2, A-2b, G-1, G-3 **PASS** (forged mint / supply unchanged / GBT). S-4 hit `not(4000 == 10000)` — stale two-decimal atom conversion. Mint occurred (`mint_count=1`). |
| `feature_res_maturity.py` | **FAIL** at mint atom pin | T-ORU-1 **PASS** (`bad-txns-premature-spend-of-coinbase`). RES mint helper expects `10_000` atoms; node produced 40.000000 RES (`4000` under `* 100`). Maturity predicate was not reached. |

**No consensus regression from this audit** (no consensus files changed). Finding M-2 (harness atoms + Witness v1 RES fuzz gap) is **Resolved**; see §11.

---

## Appendix B — Code map

| Concern | File | Symbol / reject |
|---------|------|-----------------|
| Cap constant + schedule | `core/src/consensus/rigor_consensus_lock.h` | `RES_MAX_SUPPLY`, `GetResMintReward` |
| Mint evaluation | `core/src/consensus/rigor_res.cpp` | `ConnectEvaluateResMintTx` |
| Unique mint | `core/src/consensus/rigor_res.cpp` | `EnforceResMintUnique` |
| Transfer / maturity / mint shape | `core/src/consensus/tx_verify.cpp` | `CheckTxInputs`, `bad-txns-premature-spend-of-res-mint`, `bad-txns-res-in-belowout` |
| Winner coupling | `core/src/validation.cpp` | `bad-rigor-res-max-supply`, `bad-rigor-res-winner-mismatch` |
| Coq P1 obligation / dual-pool (model) | `formal_verification/coq/ReplayReorgCore/ResMintObligation.v`, `DualPoolConservation.v` | `valid_res_mint_obligation`, `dual_pool_conservation` |
| Coinbase RES | `core/src/connect_block_rigor_checks.cpp` | `bad-cb-res-amount` |
| Mockable bootstrap | `core/src/node/devtest_res_bootstrap.cpp` | `DevTestResWalletBootstrap` |
| Mint flag | `core/src/coins.h`, `core/src/coins.cpp` | `COIN_FLAG_RES_MINT` |
| Classification | `core/src/script/standard.cpp`, `core/src/primitives/transaction.cpp` | `IsResScript`, `AssetTypeFromScriptPubKey` |
| Auto-mint (non-consensus) | `core/src/node/auto_res_mint.cpp` | skips assembly when `W(h) <= 0` |

---

## Appendix C — Economic consistency (E)

| Design intent | Protocol-enforced? |
|---------------|-------------------|
| ORU pays security (subsidy + fees) | Yes — coinbase ORU schedule + fee claim |
| ORU is the liquidity / MoE asset | Yes — Witness v0 domain; fees always ORU |
| RES is scarce / SoV | Designed to be: hard cap + decaying `W(h)` + draw, not 1:1 swap |
| RES creation absorbs ORU | Yes — Convergence removes ORU from `S_ORU` (`C(h)`); burned ORU is weight only |
| Uncontrolled RES dilution | Protocol-enforced: unique mint, `W(h)`, cap identity, transfer conservation |
| Mining does not mint RES | Yes — production coinbase RES must be 0; winner ≠ miner |

This audit does **not** claim guaranteed price appreciation, guaranteed
value increase, or a calendar fill of 21 million RES.
