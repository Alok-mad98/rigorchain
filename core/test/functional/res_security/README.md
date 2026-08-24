# Pre-mainnet RES security functional suites (solo; no pool)

Adversarial (incentive / runtime) tests for pre-mainnet claims that formal
verification of the *model* does not cover. This directory does **not** add
consensus rules. **Pool / stratum / web are out of scope** and are not run here.

**Operator checklist:** `docs/security/PRE-MAINNET-CHECKLIST.md`

## Consolidated runner (window + solo + maturity)

Wallet-enabled `rigord`/`bitcoind` build required. From `core/`:

```bash
./test/functional/res_security/run_pre_mainnet.sh
```

Fail-fast, section headers. Order:

1. `feature_res_window_reorg_res_attacks.py --descriptors` — window / reorg / RES monopoly
2. `feature_res_solo_security.py --descriptors` — solo mempool/mint/GBT
3. `feature_res_maturity.py --descriptors` — immature RES mint and ORU coinbase spends

Optional: runs `./src/test/test_bitcoin --run_test=res_orum_tests` if that binary exists (`SKIP_UNIT=1` to skip). Formal verification is **not** in this script:

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

Help: `./test/functional/res_security/run_pre_mainnet.sh --help`.

---

# Window / reorg / monopoly attack suite

Adversarial (incentive / runtime) tests for pre-mainnet claims that formal
verification of the *model* does not cover. Consensus rules are not modified.

## Inventory (Phase 0)

### Functional tests this suite extends

| Path | What it already pins |
|------|----------------------|
| `test/functional/feature_res_core_convergence_draw.py` | σ preimage, argmin winner, A-8 pay-at-h+1, mint amount, CSM1 |
| `test/functional/feature_res_core_convergence_window.py` | Sliding W, buried tickets stay in F, no double-pay walk, C from window |
| `test/functional/feature_rigor_reorg_winner_lock.py` | Same block ⇒ same draw; rebuilt block ⇒ new σ (A-8) |
| `test/functional/rigor_attack_blocks.py` | `submitblock` reject strings (commitment / CVGE / draw root) |
| `test/functional/rigor_reorg_consistency.py` | Two-node competing tips; draw fields agree on the survivor |
| `test/functional/feature_res_lifecycle_controlled_devtest_001.py` | Invalidate/reconsider mint chain (not default runner) |
| `test/functional/feature_reorg_determinism_audit.py` | Multi-node rollback vs golden tip |

### Consensus paths exercised

| Symbol / function | File |
|-------------------|------|
| `FinalizeWindowedDrawSet` / `already_paid` | `src/consensus/convergence_logic.cpp` |
| `BuildConvergenceWindowTail` / `paid_winners` | `src/validation.cpp` |
| RES mint at connect (`RES_REWARD_AMOUNT`, `has_res`) | `src/validation.cpp` ConnectBlock |
| Auto mint in templates | `src/node/miner.cpp` `CreateNewBlock` |
| `CONVERGENCE_DRAW_WINDOW = 32` | `src/consensus/rigor_consensus_lock.h` |
| Unit: `a9c_already_paid_*`, reorg branch paid-set | `src/test/convergence_consensus_tests.cpp` |

### Maturity (do not spend fresh block rewards)

`COINBASE_MATURITY = 100` applies to **ORU coinbases** and **body RES mints** at **consensus**
(`CheckTxInputs`, same depth predicate). `getbalances().oru.trusted` / `res.trusted` exclude
immature amounts (wallet only). This suite:

- mines ≥100 extra blocks before treating subsidy as spendable
- splits mature ORU into confirmed *transfer* UTXOs (`large_addr` / `small_addr`)
- never sends minted RES (asserts mint in the payout block / chain snapshot only)

### Miner / policy knobs for excluding convergence txs

There is **no** dedicated “exclude convergence” consensus or policy flag.
Exclusion in this suite is done with:

- `prioritisetransaction` with a large negative `fee_delta` (template skip, tx stays in mempool)
- `generateblock`-style inclusion lists are **not** used after a pending RES mint: `generateblock` assembles with a null mempool; when automint is already in the template it now returns `JSONRPCError` (RPC only). Use `generatetoaddress` + `prioritisetransaction` instead.
- Two-node disconnect so a private branch never learns foreign tickets (R-1)

`-blockmaxweight` / `-blockmintxfee` are generic and were not required here.

## How to run

From `core/` (wallet descriptors required):

```bash
python3 test/functional/feature_res_window_reorg_res_attacks.py --descriptors
```

or:

```bash
./test/functional/res_security/run_suite.sh
```

or via the pull tester:

```bash
python3 test/functional/test_runner.py feature_res_window_reorg_res_attacks.py
```

Chain is **devtest** (mockable PoW, local-only). No public network. On failure the
assert message includes height, tip, `mint_count`, `res_balance`, F, and the
paid-winner walk.

## Case map

| ID | File method | Claim under attack |
|----|-------------|--------------------|
| M-1 | `case_m1_m2_b2_b3_censorship` | Own-only 1 ORU inclusion + mempool censorship ⇒ free RES every block |
| M-2 | same | Private-branch exclusion across a window is zero-cost dominant |
| M-3 | `case_m3_flood` | Min-burn flood packs F with negligible cost |
| M-4 | `case_m4_no_double_pay` | Same txid paid twice in the window |
| M-5 | `case_m5_border_weights` | Dust / f=0 / non-convergence enter F or panic |
| R-1 | `case_r1_r2_fork_undo_and_reinclude` | RES dangling after reorg to a branch without W1 |
| R-2 | same | Re-include same txid after reorg ⇒ double pay |
| R-3 | `case_r3_deep_window_reorg` | Deep (≥W) reorg leaves inconsistent `already_paid` / extra mint |
| R-4 | `case_r4_restore` | Disconnect+reconsider changes draws or mints extra RES |
| R-5 | `case_r5_empty_singleton` | Empty/singleton F produces spurious RES or non-null C |
| B-1 | `case_b1_sigma_binding` | σ re-roll without new PoW |
| B-2 | `case_m1_m2_b2_b3_censorship` | Censorship is dominant vs fees forgone |
| B-3 | same | RES ≈ 100% to censoring hashrate at negligible burn |

Results narrative: `docs/security/PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md`.
Matrix fragment: `docs/security/SECURITY_TEST_MATRIX.md`.

## Solo-mining suite (no pool)

`test/functional/feature_res_solo_security.py` covers V-1…V-5, S-1…S-5, A-1…A-4, G-1…G-4.
Pool / stratum / web are out of scope.

```bash
python3 test/functional/feature_res_solo_security.py --descriptors
```

```bash
./test/functional/res_security/run_solo_suite.sh
```

Primary pre-mainnet bundle (window, then solo, then maturity):

```bash
./test/functional/res_security/run_pre_mainnet.sh
```

Helpers: `solo_helpers.py` (forged PCORB mint, `submitblock`).  
Narrative: `docs/security/PRE-MAINNET-SOLO-SECURITY.md`.  
Inventory: `docs/security/PRE-MAINNET-SOLO-INVENTORY.md`.

## Maturity suite

`test/functional/feature_res_maturity.py` — T-RES-1…6, T-ORU-1, T-ISSUE-1. Consensus mint flag + `CheckTxInputs`.

```bash
python3 test/functional/feature_res_maturity.py --descriptors
```

Included in `run_pre_mainnet.sh` (step 3). Narrative: `docs/security/RES-ORU-MATURITY.md`.
