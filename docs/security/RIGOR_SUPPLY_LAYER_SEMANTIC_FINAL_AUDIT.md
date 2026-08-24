# RIGOR Supply-Layer Semantic Final Audit

**Task:** `TASK-RIGOR-SUPPLY-LAYER-SEMANTIC-CONSISTENCY-P1-001`  
**Date:** 2026-08-20  
**Consensus impact:** **NONE**  
**Push / merge / tag:** **NONE** (local commit only)  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Canonical glossary: [RIGOR_SUPPLY_ACCOUNTING_MODEL.md](./RIGOR_SUPPLY_ACCOUNTING_MODEL.md)  
Prior P0 read: [RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md](./RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md)

---

## 1. Verdict

Shipping consensus already distinguished RFC-002 ledger `S_ORU` from UTXO circulating ORU. This task **normalized product semantics** (RPC help, explorer circulating SUM, RES 6-decimal display, wallet parse/help, formal names, golden vectors) without changing ConnectBlock, subsidy, RFC-002/017/018 rules, PoW, signatures, wallet signing, or monetary parameters.

| Check | Result |
|-------|--------|
| Consensus / monetary parameters | Unchanged |
| Four canonical ORU quantities named | Yes |
| Explorer circulating excludes CVG2 | Yes |
| RFC-018 fee burn counted as circulating | No (never an output) |
| RES display 6 decimals on active shipping surfaces touched here | Yes |
| C++ equivalence claimed | **false** |

---

## 2. Canonical definitions (locked for product copy)

1. **Total issued ORU** — `Σ R(h)` — `oru_subsidy_cumulative_derived`
2. **Convergence destroyed ORU** — `Σ C(h)` — `cumulative_convergence` / `m_block_converge_sum`
3. **RFC-018 fee-burn destroyed ORU** — floor share of RES-transfer ORU fees never claimed into coinbase — **not persisted**
4. **Circulating spendable ORU** — asset-filtered UTXO ORU (CVG2 absent; fee burn absent; immature included)

`m_chain_oru_total` remains RFC-002 `S_ORU = issued − converge`. It is **not** (1) or (4).

---

## 3. What changed (non-consensus)

### RPC naming and help

- `getruntimesupply`: help text distinguishes ledger vs circulating vs issued vs Convergence vs missing fee-burn accumulator. Additive JSON object `supply_semantics` (`equivalence_claimed: false`).
- `gettxoutsetinfo.total_amount`: help states mixed ORU+RES nValue, not circulating ORU.
- `getblockchaininfo.cumulative_convergence`: help states Convergence destruction, not fee burn.
- Wallet RPC help (`sendtoaddress` / `sendmany` / already-fixed `getbalances`): RES **6** decimals.

No existing JSON **value** keys were renamed (Coq hooks / `oru_supply_cumulative` stay).

### Explorer circulating supply

- Importer no longer inserts CVG2 into `oru_utxos`.
- Dashboard SUM / COUNT uses `spent = FALSE AND NOT CVG2`.
- Address spendable ORU excludes CVG2.
- Alias `oru_circulating_spendable_display`.

### Wallet display precision

- Qt `BitcoinUnits::parseRes` accepts 6 fractional digits (matches `formatRes`). Display/input only; signing paths unchanged.
- `formatRes` was already 6 decimals.

### RES six-decimal formatting

Explorer `amount.ts` is the indexer/UI scale SSOT: `RES_COIN = 1e6`, 6 dp parse/display. HTML/SSR helpers updated. Tests pin `1.000000 RES`.

### Historical `RES_COIN = 100`

RFC-001 §4.1 and RFC-015 §2.1 **numbers unchanged**, labeled historical / not live. Archive and RFC-017 comparison table left as historical records.

---

## 4. Formal boundary checks

Module: `formal_verification/coq/fee-burn/CirculatingSemantics.v`

| Theorem | Meaning |
|---------|---------|
| `fee_burn_cannot_enter_circulating_supply` | Burn share has no UTXO (`fs_burn_utxo = None`) |
| `cvg2_locked_outputs_excluded_from_circulating` | CVG2 contributes 0 to circulating |
| `wallet_balance_matches_spendable_utxo_model` | Unspendable coin (CVG2-class) is 0 in wallet balance |

Python: `formal_verification/python-spec/supply_layer.py`  
Golden: `formal_verification/golden-vectors/supply_layer_semantics.json`  
Gate: `formal_verification/tests/test_supply_layer_semantics.py`

Not claimed: `circulating_supply_matches_utxo_ledger` as C++ refinement.

---

## 5. Remaining gaps

| ID | Gap |
|----|-----|
| G-1 | Cumulative RFC-018 fee burn still not stored on `CBlockIndex` (needs a dedicated observational-cache task). |
| G-2 | `gettxoutsetinfo.total_amount` still **computes** mixed nValue; help now warns. Asset-split fields would be a follow-up RPC. |
| G-3 | Existing explorer DBs may still contain CVG2 rows until reindex; live SUM filter hides them. |
| G-4 | Qt/explorer charts that still read `oru_supply_cumulative` as “circulating” need GUI copy updates beyond this task. |
| G-5 | Archive / whitepaper-audit docs that cite `RES_COIN=100` remain historical. |

None of these is a consensus inflation bug.

---

## 6. Tests executed

| Gate | Result |
|------|--------|
| `python3 formal_verification/tests/test_supply_layer_semantics.py` | **PASS** |
| `python3 formal_verification/tests/test_rfc018_fee_burn_parity.py` | **PASS** |
| Explorer `npm run test:unit` (62 tests, including amount + circulating) | **PASS** |
| `coqc` `fee-burn/CirculatingSemantics.v` + `Properties.v` | **PASS** (named theorems typecheck) |

---

## 7. Consensus impact

**None.** No edits to ConnectBlock rules, `OruResTransferFeeBurnShare`, subsidy, RFC-002 `S_ORU` update, PoW, signatures, or wallet transaction construction beyond Qt RES **parse** decimal count and RPC/GUI help strings.
