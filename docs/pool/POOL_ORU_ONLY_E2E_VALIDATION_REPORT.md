# POOL_ORU_ONLY_E2E_VALIDATION_REPORT

**Task:** `TASK-POOL-ORU-ONLY-END-TO-END-VALIDATION-001`  
**Date:** 2026-08-12  
**Overall:** **PARTIAL**  
**Consensus / W / T / RES rules:** **Unchanged**  
**Acceptance plan:** layered — see `contrib/testing/pool_oru_only_e2e/LAYERED_VALIDATION.md`  
**Runtime artifacts (prior full pass):** `.runtime/pool-oru-only-e2e/20260812T115137Z/`  
**Phase-1 correct-binary re-run (devtest):** `.runtime/pool-oru-only-e2e/20260812T121055Z/`  
**Layer-1 first smoke (regtest):** `.runtime/pool-oru-only-e2e/layer1_regtest-20260812T122720Z/`  
**Harness:** `contrib/testing/pool_oru_only_e2e/`

---

## 0. Layered acceptance (adopted)

| Layer | Env | Status |
|-------|-----|--------|
| **1** Local product-shaped sim (`regtest` + LMS knobs: `bech32m` RES fee, automint, Convergence) | **Default** via `run_validation.sh` | First smoke: same merkle FAIL; boundary OK |
| **2** Read-only real mainnet observation | Ops-only | After Layer 1 green on submit |
| **3** Real mainnet pool | Live funds | **Blocked** until merkle/`submitblock` fixed |

```bash
export CARGO_TARGET_DIR=/tmp/cursor-sandbox-cache/05199d473af6aca404e5d6709bc4f355/cargo-target
bash contrib/testing/pool_oru_only_e2e/run_validation.sh          # Layer 1 (default)
POOL_ORU_E2E_LAYER=legacy_devtest bash contrib/testing/pool_oru_only_e2e/run_validation.sh  # optional smoke
```

### Layer-1 first smoke (`layer1_regtest-20260812T122720Z`)

| Check | Result |
|-------|--------|
| Chain / RES fee | `regtest`; `-miningresfeeaddr=rgrt1p…` (bech32m) |
| Auth / GBT / shares / block.found | **PASS** (`shares_accepted=1`, `blocks_found=1`) |
| `submitblock` | **FAIL** `bad-txnmrklroot` (`blocks_submit_failed=1`) |
| Pool RES | **NONE** (`res_mint_events=0`, `oru_only=true`) |
---

## 1. Architecture diagram

```text
Miner (rigor-miner-agent)
        │  Stratum v1 (opushash)
        ▼
Stratum Pool (rigor-pool-pilot / stratum-server)
        │  GBT poll  (opaque txs; ORU-only assembly)
        ▼
rigord CreateNewBlock() / getblocktemplate
        │
        ├─► ORU coinbase subsidy/fees  ──► Wallet
        │
        └─► RES body mint (auto_res_mint) ONLY when draw winner
              present + automint funded  —— NOT constructed by pool

Wallet (user)
        │  createconvergence
        ▼
W=32 blocks
        │
        ▼
100.00 RES (node consensus mint path; pool never issues)
```

**Product rule verified in this run:** pool `res_mint_events == 0`; audit has **no** `res.mint` / mint-engine events.

---

## 2. Test environment

| Item | Value |
|------|-------|
| Host | WSL2 Linux |
| Chain (historical full run) | `devtest` |
| Chain (going forward) | Layer 1 `regtest` |
| `rigord` | `./core/src/rigord` |
| Pool binary | ORU-only `rigor-pool-pilot` (built 2026-08-12; CARGO_TARGET_DIR) |
| Miner binary | `rigor-miner-agent` (built 2026-08-12) |
| RPC (legacy) | `127.0.0.1:49871` user `oruonly` |
| Stratum (legacy) | `127.0.0.1:13334` |
| Datadir | `.runtime/pool-oru-only-e2e/20260812T115137Z/datadir` |
| Wallet | `oru_e2e` |
| Node flags | `-maxtipage=999999999 -listen=0 -connect=0` (solo IBD bypass) |

### Commands executed

```bash
export CARGO_TARGET_DIR=/tmp/cursor-sandbox-cache/.../cargo-target
$CARGO_TARGET_DIR/release/rigor-pool-pilot 2>&1 | head -1
# → profile=Production (ORU-only; no pool RES mint)

contrib/testing/pool_oru_only_e2e/run_validation.sh   # 20260812T121055Z Phase-1 re-run
# Going forward (Layer 1):
# bash contrib/testing/pool_oru_only_e2e/run_validation.sh
python3 contrib/testing/pool_oru_only_e2e/continue_validation.py  # uses LATEST
```

---

## 3. Phase results

### Phase 1 — Local pool stack

| Check | Result | Evidence |
|-------|--------|----------|
| Miner connects | **PASS** | `handshake OK (agent=rigor-miner/1.0-opushash)` |
| GBT jobs published | **PASS** | `active_jobs=1`, `jobs_notified=1`, tip genesis |
| Shares accepted | **PASS** | pool metrics `shares_accepted=1` |
| Block candidate found | **PASS** | `block.found` job `0000000000000001` |
| Block accepted by node | **FAIL** | `submitblock rejected: bad-txnmrklroot` |
| Pool RES mint | **NONE** | `res_mint_events=0`, no mint audit events |

#### Phase-1 correct-binary re-run (`20260812T121055Z`)

| Check | Result | Evidence |
|-------|--------|----------|
| Binary banner | **PASS** | `(ORU-only; no pool RES mint)` — no `ORU_FUNDING_TXID` |
| Auth smoke | **PASS** | first unauthorized (expected), second `e2e.miner1` OK |
| GBT jobs | **PASS** | `active_jobs=1` |
| Shares / block.found | **PASS** | `shares_accepted=1`, `blocks_found=1` |
| `submitblock` | **FAIL** | `blocks_submit_failed=1`; `rigord.log`: `bad-txnmrklroot` |
| Pool RES | **NONE** | `res_mint_events=0` |
| Tip advance via pool | **FAIL** | tip remained genesis during mine window |

**Conclusion:** Correct ORU-only binary reproduces the same merkle submit blocker. Further acceptance moves to **Layer 1 regtest**, not real mainnet.

**Miner log (excerpt):**

```text
rigor-miner-agent connecting to 127.0.0.1:13334 as e2e.miner1 (threads=2)
handshake OK (agent=rigor-miner/1.0-opushash)
mining started with 2 thread(s)
status worker=e2e.miner1 hashrate=4979.4 H/s hashes=24910 shares_found=8 ...
```

**Pool log (excerpt):**

```text
rigor-pool-pilot: rpc=http://127.0.0.1:49871/ stratum=127.0.0.1:13334 profile=Production (ORU-only; no pool RES mint)
```

**Pool audit (excerpt):**

```text
job.publish ... oru_only=true draw_winner=None
share.accepted worker=e2e.miner1 job=0000000000000001
block.found worker=e2e.miner1 job=0000000000000001
block.submit_failed ... submitblock rejected: bad-txnmrklroot
```

**Finding (non-consensus):** ORU-only `assemble_solved_block` produced a block the node rejected with `bad-txnmrklroot`. This blocks end-to-end tip advance via pool submit until merkle assembly is fixed. It does **not** involve RES mint construction.

### Phase 2 — ORU payout validation

| Check | Result | Evidence |
|-------|--------|----------|
| Wallet receives ORU | **PASS** | balance `950.00000000` after generate maturity; later trusted ORU ≫ 0 |
| Pool pays/handles ORU only | **PASS** | PPLNS ORU ledger; no RES fields |
| Pool RES involvement | **NONE** | `pool_res_scan.json` |

Because pool `submitblock` failed, coinbase ORU for the *pool-found* candidate did not land on tip. ORU wallet funding for Convergence used `generatetoaddress` (documented fallback) after Phase-1 pool evidence was captured.

**ORU tx example (generate coinbase):**  
`txid=05aebe3e581cffcd27d3b56061858bf07e7b163dd696e848e6d1e09f735c8a2a` · height `59` · semantic `oru_coinbase`

### Phase 3 — Convergence validation

| Check | Result | Evidence |
|-------|--------|----------|
| `createconvergence` | **PASS** | txid `059f92851a8b7e35aca4c3909c5732b9354b769dc8705cc24b10d39e2a67bfb4` |
| Advance ≥32 blocks | **PASS** | height `101 → 135` (+34) |
| `100.00 RES` observed in wallet this run | **FAIL / not observed** | `getbalances.res.trusted = 0.00` |
| RES source if minted | Node path only | Pool cannot create RES (`res_mint_events=0`) |

**Notes on missing RES in this run (ops, not consensus change):**

1. `-miningresfeeaddr` was a witness-v0 `dvt1q…` address; automint docs require witness-v1 RES (`rg1p…` / chain equivalent).  
2. Automint fund tx created: `2fa7994f1378db4d09f24211644affa7a9ceb61eddfee5a981c570dd2d373083`.  
3. Draw selection is probabilistic; a Convergence tx does not guarantee a winner every window.  
4. Prior closed LMS already demonstrated **100.00 RES** via node mint (`docs/release/local_simulation/RES_CONVERGENCE_E2E_REPORT.md`) with unchanged W=32 — that remains the reference for the Convergence→RES product path **outside the pool**.

**Confirmed:** pool did not create RES; RES issuance locus remains user Convergence + `rigord` automint/GBT.

### Phase 4 — Restart / failure recovery

| Check | Result | Evidence |
|-------|--------|----------|
| Restart pool | **PASS** | `pool_restart.log` boots ORU-only pilot |
| Reconnect worker | **PASS** | `restart_recovery.json` `ok=true` (subscribe+authorize) |
| No RES inconsistency | **PASS** | `res_mint_events` remained 0 |
| No duplicate pool RES payout | **PASS** | pool has no RES payout path |

---

## 4. Block heights / transaction IDs

| Event | Height / ID |
|-------|-------------|
| Genesis tip (Phase 1 GBT) | `407381b8179c92027826909806920e3633d5487cac816e8fff419223d30641c7` |
| After ORU generate bootstrap | 101 |
| After W=32 margin generate | **135** |
| Convergence txid | `059f92851a8b7e35aca4c3909c5732b9354b769dc8705cc24b10d39e2a67bfb4` |
| Automint fund txid | `2fa7994f1378db4d09f24211644affa7a9ceb61eddfee5a981c570dd2d373083` |
| Sample ORU coinbase txid | `05aebe3e581cffcd27d3b56061858bf07e7b163dd696e848e6d1e09f735c8a2a` |

---

## 5. Acceptance matrix

| Criterion | Result |
|-----------|--------|
| Pool mines / finds block candidates successfully | **PASS** (found) |
| Pool only handles ORU payout | **PASS** |
| Pool cannot create RES | **PASS** (`res_mint_events=0`) |
| Node creates RES body when required | **NOT OBSERVED this run** (config/draw); locus confirmed as node-only |
| Wallet receives ORU | **PASS** |
| User receives 100.00 RES through Convergence | **NOT OBSERVED this run** |
| Restart tests | **PASS** |
| Consensus unchanged | **PASS** |

**Final verdict: PARTIAL**

Blockers to full PASS (follow-ups; validation-only here):

1. Fix ORU-only `assemble_solved_block` merkle (`bad-txnmrklroot`) so pool `submitblock` lands tip.  
2. Re-run **Layer 1** with witness-v1 `-miningresfeeaddr` + funded automint until `getbalances.res` shows `100.00`.  
3. Do **not** promote to real mainnet pool (Layer 3) while submit fails.

---

## 6. Artifact index

| Path | Contents |
|------|----------|
| `artifacts/FINAL_SUMMARY.json` | Machine-readable verdict |
| `artifacts/status.json` | Pool metrics + audit (block.found / submit_failed) |
| `artifacts/stratum_auth.json` | Worker authorize OK |
| `artifacts/pool_res_scan.json` | RES involvement = NONE |
| `artifacts/createconvergence.json` | Convergence txid |
| `artifacts/res_evidence.json` | W window heights |
| `artifacts/restart_recovery.json` | Reconnect PASS |
| `logs/miner.log` | Miner hashrate / handshake |
| `logs/pool.log` / `pool_restart.log` | ORU-only boot banner |
| `logs/rigord.log` | Node |

---

## 7. Closing statement

RIGOR Pool operated as an **ORU mining coordination layer** in this validation.  
**RES was never issued by the pool.**  
RES remains a **user Convergence + node automint** product path; this run proved the pool boundary and ORU/Convergence wiring, and recorded the remaining merkle-submit and RES-observation gaps.  
**Next validation default is Layer 1 (`regtest` product-shaped), not bare `devtest` and not unrepaired MAINNET.**
