# Pre-mainnet: solo-mining path (no pool)

**Date:** 2026-08-18  
**Tree:** `~/rigor-official-tree`  
**Mode:** Mempool↔consensus consistency, RES mint authorization, consensus-path robustness, solo GBT/submit. Formal verification is frozen.  
**Release scope:** `rigord` / wallet / `getblocktemplate` / `submitblock`. **Pool / stratum / YiiMP / pool web are not shipped** (see [PRE-MAINNET-SOLO-INVENTORY.md](./PRE-MAINNET-SOLO-INVENTORY.md)).  
**Operator checklist:** [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md).

## How to run

From `core/` (wallet-enabled `rigord` build; chain is **devtest**). Use `python3` so the script does not need the execute bit:

```bash
python3 test/functional/feature_res_solo_security.py --descriptors
```

Solo wrapper (same test):

```bash
./test/functional/res_security/run_solo_suite.sh
```

Window/reorg, then solo, then maturity:

```bash
./test/functional/res_security/run_pre_mainnet.sh
```

Pull tester:

```bash
python3 test/functional/test_runner.py feature_res_solo_security.py
```

**Maturity:** ORU coinbases and body RES mints are immature for `COINBASE_MATURITY = 100`. Assert mint via payout block / `mint_count` / chain `res_balance`, never a wallet send of newly minted RES.

Saturating eligibility for the winner path (S-4 / S-5 / G-2 / G-4) consolidates **2,097,152 ORU** (`T·f ≥ 2^256`) in batched transfers so a single standard tx is not overweight.

## Results

Filled from a clean run of `feature_res_solo_security.py --descriptors` (2026-08-18, PRNG seed `1622602736300357947`, ~173s, **Tests successful**). Status: **PASS** (claim held), **FAIL** (confirmed bug), **INCONCLUSIVE** (not observed live; pin documented).

### Phase 1 — Mempool ↔ consensus (V-*)

| ID | Scenario | Result | Evidence |
|----|----------|--------|----------|
| V-1 | Tx mempool-accepted but invalid at ConnectBlock (forged ORU→RES mint, parseable PCORB, unbound height) | **PASS** | `testmempoolaccept` allowed=true; connect `bad-rigor-res-proof-height`; supply unchanged; no split |
| V-2 | Policy-invalid, consensus-includable (zero-fee ORU) | **PASS** | mempool `min relay fee not met (orum)`; `generateblock` accepted the hex |
| V-3 | Convergence fee / amount floor / script form | **PASS** | 0.99 ORU builder-reject; 1.00 ORU `is_convergence`; floor also `bad-converge-amount` at connect |
| V-4 | RBF of a convergence ticket | **PASS** | old txid leaves mempool and F; no stale F pollution |
| V-5 | Mempool reorg: re-accept after disconnect | **PASS** | tx re-accepted; remine (new address); `mint_count=0`; no extra mint / missing undo |

### Phase 2 — RES mint authorization & supply (S-*)

| ID | Scenario | Result | Evidence |
|----|----------|--------|----------|
| S-1 | Forged RES mint without winning draw | **PASS** | connect `bad-rigor-res-proof-height`; `mint_count=0` |
| S-2 | Wrong payout script / not winner binding | **PASS** | live mint atoms = `RES_REWARD_ATOMS` (40.000000 RES); owner mismatch unit-pinned `res_proof_math_tests` `bad-rigor-res-owner-mismatch` |
| S-3 | Mint when RES cap reached | **PASS** | live snapshot `mint_count=1`, `res_balance=0.00010000`; cap pin `runtime_supply_invariant_tests` / ConnectBlock `bad-rigor-res-max-supply` (cannot mint 21e6 live) |
| S-4 | Duplicate mint / same draw after pay | **PASS** | saturating ticket + companion (`\|F\|≥2`); win → pay at next height; `distinct_paid=1` |
| S-5 | Solo coinbase-only vs templates with convergence / automint | **PASS** | `generateblock([])` JSONRPCError (automint already in template); GBT `tx_count=1`; payout 40000000 atoms |

### Phase 3 — Consensus path robustness (A-*)

| ID | Scenario | Result | Evidence |
|----|----------|--------|----------|
| A-1 | Formerly assert-level validation | **PASS** | peer blocks: `state.Invalid`; remaining `AbortNode` is local (corrupt/disk/window reconstruct); generateblock footgun is RPC `JSONRPCError` |
| A-2 | Malformed block / bad mint / bad CVGE cumulative | **PASS** | forged mint + `bad-convergence-cumulative`; node stayed RPC-live |
| A-3 | Deep reorg stress | **PASS** | inherited: window suite **R-3 PASS**; this file covers mid-window restart, not a second ≥W attack |
| A-4 | Restart mid-window | **PASS** | tip hash and `mint_count=1` restored after `stop`/`start` |

### Phase 4 — Solo mining RPC (G-*)

| ID | Scenario | Result | Evidence |
|----|----------|--------|----------|
| G-1 | GBT with convergence mempool | **PASS** | GBT lists ticket; mined block; window self-check; σ/root/F consistent |
| G-2 | Submit that censors foreign convergence txs | **PASS** | 2 censored via `prioritisetransaction`; block accepted; exclusivity is not a free draw (matches RES suite) |
| G-3 | Stale / wrong-prev / wrong draw commitment | **PASS** | stale GBT submit rejected (`bad-convergence-cumulative`) |
| G-4 | Restart miner node; continue SOLO | **PASS** | tip restored; `mint_count=1`; no stuck automint / duplicate mint |

## Intentional policy ≠ consensus gaps (not exploits)

| Gap | Mempool / RPC | Connect / consensus | Exploit? |
|-----|---------------|---------------------|----------|
| V-1 forged mint shape | `CheckOruFundedResMintShape` allows parseable PCORB that satisfies the inequality | `ConnectEvaluateResMintTx` binds height / winner / owner | **No** — honest nodes reject at connect; supply unchanged. Nodes that never see the block do not split. |
| V-2 min-relay | Policy may reject zero-fee | Miner may include; F-eligibility still requires consensus fee/amount | **No** — documented policy filter |
| Draw winner / `already_paid` | Mempool cannot know the parent draw of a *future* block | Connect-only | **No** — by design |

Do not weaken connect checks to match mempool.

## Confirmed bugs and fixes

| Item | Class | Consensus impact | Fix |
|------|-------|------------------|-----|
| `generateblock` + pending RES automint used `CHECK_NONFATAL(vtx.size()==1)` | RPC solo footgun | **no** | `JSONRPCError` in `core/src/rpc/mining.cpp`; caller must use `generatetoaddress` or GBT |
| Harness: 100k ORU is not saturating; 2.1M ORU in one `sendtoaddress` exceeds standard weight | test only | **no** | Batch-consolidate + `lockunspent` |

No consensus rule was changed. No Critical consensus hole was found on the solo path.

## Residual risks

Inherited from [PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md](./PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md) unless noted:

| Residual | Severity | Why it remains |
|----------|----------|----------------|
| P2P eclipse / network-level withholding | Medium | Out of scope; not simulated here |
| Hashrate share α with matching burns | Low–Medium | A-9-C removes exclusivity, not proportional share |
| Live mint-to-cap (21e6 RES) | Low | S-3 pins ConnectBlock / unit invariant; cannot fund cap in a functional test |
| Wrong-owner mint of a *real* winning draw | Low | S-2 live path checks amount + RESO owner vout; mismatch reject is unit-pinned |
| Competing-tip `submitblock` after a long prefix | Low–Medium | G-3 uses stale GBT; live two-node sibling submit remains a RES-suite residual |
| Pool / stratum / web | — | **Not shipped**; not audited in this task |

## Go / no-go for this risk theme

**Go** (solo-mining mainnet theme only), with residuals above.

- V-1, S-1, S-2, A-2 (highest damage on a public solo node) are **PASS**.
- S-4 / S-5 / G-4: real saturating winner, automint RPC error, restart without double mint — **PASS**.
- One RPC-only fix (`generateblock`); **consensus impact: no**.
- Window / reorg / RES monopoly suite remains the gate for exclusivity / `already_paid` / ≥W reorg (previously **all PASS**; not re-run in this session).

This is **not** a go for pool/stratum, P2P eclipse, or α-share capture with matching burns.
