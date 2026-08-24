# Pre-mainnet: window / reorg / RES monopoly attacks

**Date:** 2026-08-18  
**Tree:** `~/rigor-official-tree`  
**Mode:** Adversarial runtime / incentive tests. Formal verification of the model is frozen and is not expanded here.  
**Consensus impact:** none (tests and documentation only).  
**Operator checklist:** [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md).

## How to run

From `core/`:

```bash
python3 test/functional/feature_res_window_reorg_res_attacks.py --descriptors
```

```bash
./test/functional/res_security/run_suite.sh
```

Pre-mainnet bundle (window + solo + maturity): `./test/functional/res_security/run_pre_mainnet.sh`.

```bash
python3 test/functional/test_runner.py feature_res_window_reorg_res_attacks.py
```

Requires a wallet-enabled `rigord` / `bitcoind` build. Chain is **devtest** (mockable PoW). Inventory of existing hooks: `core/test/functional/res_security/README.md`.

**Maturity:** ORU coinbases and body RES mints are immature for `COINBASE_MATURITY = 100` blocks. Fresh block rewards cannot be transferred. The harness mines ≥100 extra blocks, then splits *mature* ORU into confirmed transfer UTXOs. Mint checks use the payout block / chain snapshot (`mint_count`, `res_balance`), never a wallet send of newly minted RES.

On failure the harness prints block height, tip hash, `mint_count`, chain RES, `getbalances()` trusted vs immature (ORU and RES), window F, and the paid-txid walk. Consensus asserts are not weakened.

## Results

Filled from a clean run of `feature_res_window_reorg_res_attacks.py --descriptors` (2026-08-18, PRNG seed `4231433629188788207`, ~196s, **Tests successful**). Status values: **PASS** (claim held), **FAIL** (confirmed bug), **INCONCLUSIVE** (harness did not observe the damaging event but no contradiction).

| ID | Attack | Expected secure outcome | Result | Evidence |
|----|--------|-------------------------|--------|----------|
| M-1 | Miner includes only own 1 ORU, censors mempool | Buried competitors remain in F; not free RES every block from exclusivity | **PASS** | 31 censor blocks; `exclusive_F_blocks=0`; draws none/foreign/miner = 30/1/0 |
| M-2 | Exclude foreign tickets across a window | Window + `already_paid` prevent zero-cost “only my txs” F | **PASS** | Buried 100k ORU ticket survived W−1; `zero_cost_exclusive_F=false` |
| M-3 | Flood min-burn tickets | Burn cost scales with ticket count | **PASS** | 12 tickets, 12.0 ORU burned, all in F; expected lifetime wins ≈ 12/65536 |
| M-4 | Same txid / replay in window | At most one RES pay (`already_paid`) | **PASS** | Win at 718, payout at 719, `distinct_paid=1`; paid txid left F |
| M-5 | Border weights (dust, f=0, saturation) | Ineligible or correctly weighted; no panic | **PASS** | 0.99 ORU rejected (`convergence-amount-too-small`); 1.00 ORU ∈ F; non-convergence ∉ F |
| R-1 | Reorg to fork without W1 | No dangling RES; supply/undo matches fork point | **PASS** | `invalidateblock` + longer branch without W1; `mint_count` 2→2; W1 ∉ F on survivor |
| R-2 | Re-include same txid after reorg | At most one pay on the surviving chain | **PASS** | W1 restored to mempool and re-included; `pays_for_w1=[]` (≤1) |
| R-3 | Deep reorg ≥ W | Window tail rebuild; no double mint | **PASS** | Depth 32; window self-check; `mint_count` 2→2; no double-pay walk |
| R-4 | Restore disconnected branch | Draws and mint count match pre-invalidate | **PASS** | invalidate+reconsider: identical tip, σ, winner, `mint_count=2` |
| R-5 | Empty / singleton F | C null when \|F\|<2; no spurious RES | **PASS** | empty \|F\|=0; singleton \|F\|=1, winner null, C null |
| B-1 | Vary non-committed surface | σ moves only with a new PoW block | **PASS** | Rebuilt block (new coinbase address) ⇒ new hash ⇒ new σ |
| B-2 | Withhold foreign tickets | Fees forgone; exclusivity does not buy the draw | **PASS** | ~31000 sat forgone; exclusivity did not grant the draw |
| B-3 | Censor vs honest inclusion over a window | RES not essentially 100% to censoring hashrate at 1 ORU | **PASS** | 100k buried vs 31×1 ORU miner tickets; miner wins = 0 |

## Confirmed bugs and fixes

No consensus defect. Harness-only issues found while landing the suite (not protocol changes):

| Item | Class | Fix |
|------|-------|-----|
| `prioritisetransaction` is cumulative; double-censor left W1 unselectable (R-2) | harness | Deduplicate censor list; boost include txs |
| Empty remine after `invalidateblock` hashed to the invalidated child (R-3) | harness | New coinbase address + mocktime |
| `sync_all` after invalidate dropped the P2P link (R-3) | harness | Disconnect before invalidate; mine with `no_op` sync |
| Fresh coinbase ORU treated as immediately spendable | harness | Split only *mature* transfer UTXOs; 100-block wait |

Any **Critical** consensus defect found later must land as a failing test first, then the smallest fix, labelled **consensus impact**.

## Residual risks

| Residual | Severity | Why it remains |
|----------|----------|----------------|
| P2P eclipse / network-level withholding of blocks | Medium | Out of scope; two-node disconnect is a local partition, not an eclipse simulation |
| Competing-tip `submitblock` after a long prefix | Low–Medium | R-1/R-2 use `invalidateblock` + longer branch (same `DisconnectBlock` undo as `feature_rigor_reorg_winner_lock`). Live two-node `submitblock` of a sibling previously hit `bad-convergence-cumulative` / sync stalls |
| Hashrate share α over many windows | Low–Medium | A miner with α still authors ≈ αW of the window. A-9-C removes *exclusivity*, not share. B-3 refutes “100% of RES at 1 ORU”; it does not prove α-proportional capture is impossible with matching burns |
| Saturation-weight live path | Low | Unit pin remains in `res_proof_math_tests`. Live saturating ticket (2,097,152 ORU) is now funded in `feature_res_solo_security.py` S-4 |
| `generateblock` + pending automint | Info | RPC-only: now `JSONRPCError` instead of `CHECK_NONFATAL` (`PRE-MAINNET-SOLO-SECURITY.md` S-5). Tests still use `generatetoaddress` + `prioritisetransaction` |
| Immature RES / ORU wallet sends | Info | Consensus mint is visible immediately in the payout block; wallet `res.trusted` stays 0 until 100 confirms. Attack assertions must not require transferring mint |
| Mempool not equal to “what a node saw” | Info | Consensus cannot prove censorship of a never-received tx. The window is the designed substitute |
| Pool/stratum/web | — | Separate task (out of scope) |

## Go / no-go for this risk theme

**Go** (this theme only), with residuals above.

- M-1 / M-4 and R-1 / R-2 are **PASS** (highest damage if wrong).
- No Critical consensus bug was found; no consensus rule was changed.
- Monopoly-by-exclusivity (censor mempool, 1 ORU own tickets) did not yield guaranteed free RES; reorg did not double-pay or leave dangling RES.

Tokenomics marketing claims are out of scope. This is not a go for pool/stratum, P2P eclipse, or α-share capture with matching burns.
