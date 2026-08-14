# WALLET_SEND_LOGIC_PARITY_FINAL_REPORT

**Tasks:**  
- `TASK-WALLET-SEND-LOGIC-BITCOIN-PARITY-AUDIT-001` (development)  
- `TASK-OFFICIAL-TREE-WALLET-SEND-PARITY-SYNC-001` (official import)

**Date:** 2026-08-14  
**Trees:** `~/ring` → `~/rigor-official-tree`  
**Layer:** Wallet only (script-classified assets; no `CTxOut` / consensus storage changes)

---

## Final verdict

| Criterion | Status |
|-----------|--------|
| Root cause identified | **PASS** |
| Fix matches Bitcoin Core CreateTransaction fee/SFFO order | **PASS** |
| Official tree source synced (wallet-only) | **PASS** |
| No consensus / W-T / RES issuance / PoW / chainparams / tx-format edits by this work | **PASS** |
| Development regression suites green | **PASS** |
| Official clean rebuild + suites green | **PENDING_OPERATOR** |

**Overall:** **CONDITIONAL PASS** until official-tree operator rebuild confirms §Acceptance.

---

## 1. Problem

Symptoms observed in wallet send:

1. Balance appears sufficient, but send reports insufficient funds (especially near-full sends with “Subtract fee from amount”).
2. SFFO checkbox ineffective when change exists — checked vs unchecked recipients identical.

---

## 2. Root cause

`FinalizeOruNetworkFeeAndChange()` always absorbed underpayment from the ORU **change** output **before** the SFFO block in `CreateTransactionInternal`.

That forced `to_reduce = fee_needed - current_fee` to ~0 whenever finalize succeeded, and failed closed on near-full SFFO sends with no change left to shrink.

---

## 3. Fix (Bitcoin Core parity)

In `core/src/wallet/spend.cpp`:

- Add `bool subtract_fee_from_outputs = false` to `FinalizeOruNetworkFeeAndChange`.
- When SFFO is set: **overpay→change only**; do **not** reduce change for underpay; do **not** early-return Insufficient funds.
- ORU `CreateTransactionInternal` passes `m_subtract_fee_outputs`.
- RES dual-pool path keeps default `false` (unchanged ORU fee / POOL-F behavior).

Order after fix:

1. Coins + change  
2. Overpay → change  
3. SFFO → reduce recipients by remaining gap  
4. Else (!SFFO) underpay absorbed from change / fail  

---

## 4. Official sync summary

Imported into `~/rigor-official-tree`:

| File | Action |
|------|--------|
| `core/src/wallet/spend.cpp` | Synced (SHA-256 `689ddbb9…77059`) |
| `core/src/test/wallet_send_balance_parity_tests.cpp` | Added (SHA-256 `35929d6d…457514`) |
| `core/src/Makefile.test.include` | Suite registration line |

Preserved: UI fixes, ORU-only pool, private-key export, HD UX.  
Details: `docs/release/WALLET_SEND_FIX_SYNC_REPORT.md`.

---

## 5. Test evidence

### Development (`~/ring`) — executed 2026-08-14

```text
wallet_send_balance_parity_tests — 9 cases — No errors detected
wallet_oru_fee_dominance_tests,wallet_oru_fee_dominance_chain_tests,wallet_rpc_spend_oru_fee_tests
  — 12 cases — No errors detected
```

### Official (`~/rigor-official-tree`) — operator

```bash
cd ~/rigor-official-tree/core
./autogen.sh
make clean || true
./configure
make -j$(nproc)

./src/test/test_bitcoin --run_test=wallet_send_balance_parity_tests
./src/test/test_bitcoin \
  --run_test=wallet_oru_fee_dominance_tests,wallet_oru_fee_dominance_chain_tests,wallet_rpc_spend_oru_fee_tests
./src/qt/test/test_rigor-qt
```

---

## 6. Regression matrix

| Check | Result |
|-------|--------|
| Send amount ≤ balance | PASS (parity suite) |
| SFFO ON → recipient reduced | PASS |
| SFFO OFF → change pays fee | PASS |
| Near-full send with SFFO | PASS |
| ORU fee domain / dominance | PASS (dev suites) |
| RES dual-pool / no RES SFFO | Unchanged (finalize default false) |

---

## 7. Architecture compliance

| Rule | Compliance |
|------|------------|
| No `CTxOut` / parallel asset field | Yes |
| Asset type from `scriptPubKey` | Yes |
| Wallet-only change | Yes |
| UTXO model | Yes |
| No admin / freeze / mint backdoors | Yes |

---

## 8. Acceptance

**PASS** only when all are true:

1. Official `wallet_send_balance_parity_tests` green  
2. Official ORU fee suites green  
3. Official `test_rigor-qt` green (or explicitly waived with reason)  
4. No consensus files modified by this task  
5. Wallet send fee/SFFO order matches Bitcoin Core as described above  
6. Release tree task-touched set remains wallet/docs only (no push/tag)

Until operator rebuild completes: **CONDITIONAL PASS (source synced)**.

---

## References

- `docs/release/WALLET_SEND_FIX_SYNC_REPORT.md`
- `~/ring/docs/wallet/WALLET_SEND_LOGIC_PARITY_AUDIT.md`
