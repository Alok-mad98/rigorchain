# WALLET_PRODUCTION_READINESS_REPORT

**Task:** `TASK-OFFICIAL-TREE-WALLET-PRODUCTION-READINESS-SYNC-001`  
**Date:** 2026-08-14  
**Goal:** Make `~/rigor-official-tree` the canonical Alpha.1 release-candidate **wallet source** (source sync only — no packages/tags/GitHub release).

**Detailed sync audit:** [`docs/release/WALLET_PRODUCTION_FIX_SYNC_AUDIT.md`](../release/WALLET_PRODUCTION_FIX_SYNC_AUDIT.md)

---

## Executive verdict

| Criterion | Status |
|-----------|--------|
| Official tree contains all four validated wallet production fixes | **PASS** (source) |
| No consensus / W·T / RES issuance / PoW / chainparams / `auto_res_mint` changes by this task | **PASS** |
| Dual-asset design preserved (ORU ranged HD; RES single-slot) | **PASS** |
| Development regression suites green | **PASS** |
| Official tree builds + wallet tests green | **PENDING_OPERATOR** |
| Release packaging / tags | **NOT IN SCOPE** (intentionally skipped) |

**Overall:** **CONDITIONAL PASS** — official tree is the synchronized Alpha.1 RC wallet **source**. Promote to full PASS after operator rebuild/test results are pasted below.

---

## 1. Fixes synchronized

### 1.1 Private key export (`TASK-WALLET-PRIVATE-KEY-EXPORT-001`)

- RPC: `dumporuprivkey` (ORU → WIF), `dumpresprivkey` (RES → Ed25519 hex)
- Controls: wallet lock, watch-only, ownership, asset gating
- Qt: Settings → Advanced → Security → Export Private Key
- Wallet-only; no consensus path

### 1.2 HD status audit / UX (`TASK-WALLET-HD-STATUS-AUDIT-001`)

- Root cause: wallet-wide `IsHDEnabled()` is false whenever RES single-slot SPKMs are active — UX false alarm, not broken ORU HD
- Official UX: `IsOruHDEnabled` + `IsResDedicatedKeyMode`; status “ORU: HD wallet enabled / RES: Dedicated key mode”
- **Do not** force RES into ranged HD

### 1.3 Send logic Bitcoin parity (`TASK-WALLET-SEND-LOGIC-BITCOIN-PARITY-AUDIT-001`)

- Root cause: `FinalizeOruNetworkFeeAndChange` absorbed underpay from change before SFFO
- Fix already in official `spend.cpp` (prior sync); behavior:
  - SFFO on → recipient amount decreases
  - SFFO off → change pays fee
- Suites: `wallet_send_balance_parity_tests`, `wallet_oru_fee_dominance_*`, `wallet_rpc_spend_oru_fee_tests`

### 1.4 Chinese translation

- Source English `tr("&Advanced")` unchanged in meaning
- `bitcoin_zh_CN.ts` / `bitcoin_zh-Hans.ts`: `&Advanced` → `高级(&A)`

---

## 2. Architecture compliance

Script-is-compliance-layer / UTXO-first rules checked for this sync:

| Rule | Result |
|------|--------|
| No `CTxOut` / parallel asset storage | PASS |
| Asset type from `scriptPubKey` parse | PASS |
| Wallet-only edits (`wallet/` `qt/` locale / wallet tests) | PASS |
| No admin keys / freeze / mint backdoors | PASS |
| No account-balance consensus model | PASS |

---

## 3. Validation evidence

### Development (`~/ring`) — 2026-08-14

| Suite | Result |
|-------|--------|
| `wallet_send_balance_parity_tests` (9) | No errors detected |
| `wallet_oru_fee_dominance_tests` + `wallet_oru_fee_dominance_chain_tests` + `wallet_rpc_spend_oru_fee_tests` (12) | No errors detected |
| `wallet_privkey_export_tests` + `wallet_key_model_status_tests` (8) | No errors detected |
| `test_rigor-qt` | All tests passed |

### Official (`~/rigor-official-tree`) — operator

```bash
cd ~/rigor-official-tree/core
./autogen.sh && make clean || true
./configure
make -j$(nproc)

./src/test/test_bitcoin --run_test=wallet_send_balance_parity_tests
./src/test/test_bitcoin \
  --run_test=wallet_oru_fee_dominance_tests,wallet_oru_fee_dominance_chain_tests,wallet_rpc_spend_oru_fee_tests
./src/test/test_bitcoin \
  --run_test=wallet_privkey_export_tests,wallet_key_model_status_tests
./src/qt/test/test_rigor-qt
```

| Step | Status |
|------|--------|
| Build | **PENDING_OPERATOR** |
| Wallet unit suites above | **PENDING_OPERATOR** |
| Qt test | **PENDING_OPERATOR** |

Paste terminal output here when complete:

```
(operator results)
```

---

## 4. Manual behavior gate (Phase 4)

After official rebuild:

1. ORU send with sufficient balance → success  
2. Subtract fee **on** → recipient receives reduced amount  
3. Subtract fee **off** → change pays fee  
4. `dumporuprivkey` / `dumpresprivkey` work with lock/watch-only protections  
5. HD status matches dual-asset wording  
6. Chinese UI: Settings → **高级**

---

## 5. Out of scope (explicit)

- Creating release packages  
- Creating GitHub releases  
- Tagging versions  
- Consensus / issuance / PoW / W·T parameter changes  

This task is **source synchronization only**.

---

## 6. Canonical source statement

Pending operator rebuild PASS, `~/rigor-official-tree` holds the Alpha.1 RC wallet source including:

1. Address private-key export  
2. Dual-asset HD / dedicated-key UX  
3. Bitcoin-parity ORU send / SFFO fee order  
4. Chinese Advanced → 高级  

Use development tree `~/ring` only for further wallet iteration; promote via subsequent sync tasks.
