# RES Six-Decimal Display — Final Global Audit

**Task:** `TASK-RIGOR-RES-DECIMAL6-FINAL-GLOBAL-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** User-visible RES unit display and parse vs RFC-017 six-decimal lock.  
**Consensus changes in this task:** **NONE** (audit only).  
**Honesty:** `equivalence_claimed = false`

Required user display:

```
1 RES  →  1.000000 RES
```

Forbidden user display:

```
1.00 RES
```

Canonical shipping scale (`core/src/consensus/rigor_consensus_lock.h`):

```
RES_COIN = 1_000_000
1.000000 RES = 1_000_000 atoms
```

There is **no** C++ symbol `ParseResAmount`. Parse paths are `BitcoinUnits::parseRes` (Qt) and `AmountFromValue(..., decimals)` (RPC). Format is `FormatResAmount` / `BitcoinUnits::formatRes`.

---

## 1. Verdict

| Surface | Runtime display of 1 RES | Status |
|---------|--------------------------|--------|
| Qt wallet balance / send / receive / history | `BitcoinUnits::formatRes` → `1.000000` (+ ` RES`) | **PASS** |
| Qt Convergence Network panel | `QString::number(res, 'f', 2)` → `1.00 RES` | **FAIL** |
| RPC **output** (`getbalances`, `gettransaction`, `decoderawtransaction`, `listunspent`) | `ValueFromAmountAsset` → `FormatResAmount` → `1.000000` | **PASS** |
| RPC **input** (`sendres`, `sendtoaddress asset=RES`, `sendmany` RES, `createrawtransaction` RES) | `AmountFromValue(..., /*decimals=*/2)` | **FAIL** (parse still 2 dp; `1.00` ≠ 1 RES) |
| Wallet RPC **help** for several `amount_res` / `sendres` fields | still says “2 decimal places” | **FAIL** (docs) |
| Explorer balance / tx / RES pages | `atomsToResDisplay` / `1e6` + `padStart(6)` → `1.000000 RES` | **PASS** |
| Official whitepaper / freeze / explorer indexing rules | `1.000000 RES` / `RES_COIN = 1e6` | **PASS** |
| Public website zero-trust formatter | `RES_BASE = 100`, `padStart(2)` → `1.00 RES` | **FAIL** |
| Historical RFC / archive / pool / old release reports | `1.00 RES` / `100.00 RES` / `RES_COIN = 100` | **Keep labeled historical** (not live UI) |

**Global result: NOT unified.** Canonical formatters are six decimals. User-visible leftovers remain: RPC **parse** at 2 decimals (mis-scales amounts), stale RPC help, one Qt network-stats widget, and the public website zero-trust `formatRes`.

This is **not** a consensus-parameter bug. `RES_COIN` in `rigor_consensus_lock.h` is already `1_000_000`. Do not retarget consensus to “fix” display.

---

## 2. Search notes

| Term | Shipping meaning |
|------|------------------|
| `100` / `10000` | Many hits are heights, epoch `210000`, mint-era **100.00 RES reward** (RFC-015 historical), or `100 * RES_COIN` in comments. Not automatically a 2-decimal display bug. |
| `0.00` | Mix of ORU 8-dp zeros, historical RES copy, and live 2-dp leftovers listed below. |
| `RES_COIN` | Consensus lock `1000000`. No `RES_COIN = 100` in `core/src/consensus/`. |
| `FormatResAmount` | Six fractional digits (`%06`). Pins: `rigor_fee_tests.cpp` `FormatResAmount(RES_COIN) == "1.000000"`. |
| `BitcoinUnits::formatRes` | `num_decimals = 6`, `coin = RES_COIN`. |
| `ParseResAmount` | **Does not exist.** |

---

## 3. Qt wallet

### 3.1 PASS — primary money UI

| UI | Formatter |
|----|-----------|
| Send dialog balance | `sendcoinsdialog.cpp` → `formatResWithPrivacy` |
| Amount field (send/receive) | `bitcoinamountfield.cpp` → `formatRes` / `parseRes` (6 fractional digits) |
| Receive request table | `recentrequeststablemodel.cpp` → `formatRes` |
| RES balance panel | `resbalancepanel.cpp` → `formatResWithPrivacy` |
| Transaction list / HTML desc | `transactiontablemodel.cpp` / `transactiondesc.cpp` → `formatAmountForAsset` / `formatAmountHtmlForAsset` → `formatRes` |
| Convergence start / history / lifecycle | `formatRes(...) + " RES minted"` |
| Payment URI amount | `guiutil.cpp` `parseRes` / `formatRes` |

`formatResWithPrivacy` emits `formatRes` then `" RES"`, e.g. `1.000000 RES`. Fee rows in transaction HTML use ORU formatters (`formatFeeOruHtml`), not RES.

### 3.2 FAIL — Convergence Network panel

`core/src/qt/rigorwallet/widgets/convergence_networkpanel.cpp`:

```
FormatResAtoms: QString::number(res, 'f', 2) + " RES"
```

A 40 RES mint displays as `40.00 RES`. Forbidden form. Bypass of `formatRes`.

### 3.3 Help / debug copy (non-balance)

`-reswalletbootstrap` (`init.cpp`) still says “extra **1.00 RES**”. Mockable-chain only. User-visible arg help; should read `1.000000 RES`. Not a consensus rule.

---

## 4. RPC

### 4.1 PASS — values returned to the user

`ValueFromAmountAsset` (`core_write.cpp`) for `ASSET_RES` returns `FormatResAmount` (exactly six fractional digits).

| RPC | RES field |
|-----|-----------|
| `getbalances` | `res.trusted` / pending / immature |
| `getbalance` | ORU only (help points at `getbalances`) |
| `gettransaction` / `listtransactions` | `amount` / `amount_res` via `ValueFromAmountAsset` |
| `listunspent` | `amount` by output asset |
| `decoderawtransaction` / `TxToUniv` | `vout[].value` via `ValueFromAmountAsset` |
| `getreceivedbyaddress` / `getreceivedbylabel` | `amount_res` |

`getbalances` help already says “RES with **6** decimals.”

### 4.2 FAIL — parse still two decimals (user input)

`AmountFromValue(x, 2)` interprets `"1.00"` as **100 atoms** = **0.000100 RES** at `RES_COIN = 1e6`. `"1.000000"` is rejected (too many fractional digits). Users cannot type the required six-decimal form on these calls.

| Call site | Code |
|-----------|------|
| `sendtoaddress` with `asset=RES` | `rpcspend.cpp`: `amount_decimals = send_asset == ASSET_RES ? 2 : 8` |
| `sendres` | `constexpr int amount_decimals = 2` |
| `sendmany` RES address | `SendmanyAmountContextFromScript` returns `{2, ASSET_RES}` |
| `createrawtransaction` / `fundrawtransaction` object amount | `rawtransaction_util.cpp`: `decimals = asset == ASSET_RES ? 2 : 8` |

Help for `sendtoaddress` `asset` already says `"RES" (6 decimals)` — **contradicts** the `2` parse. `sendres` help still says “Amount uses **2 decimal places**.”

### 4.3 FAIL — help still advertising 2 dp on **output** fields

Runtime JSON is six decimals; help is stale:

| File | Field |
|------|-------|
| `wallet/rpc/coins.cpp` | `getreceivedbylabel` `amount_res` “2 decimal places” |
| `wallet/rpc/transactions.cpp` | `getreceivedbyaddress` / `listreceivedbyaddress` / `gettransaction` `amount_res` “2 decimal places” |
| `core_io.h` | comment: “RES uses 2-decimal RES units” (implementation already calls `FormatResAmount`) |

Stale help is user-visible (`help gettransaction`). It does not change consensus.

---

## 5. Explorer

| Path | Scale |
|------|-------|
| `explorer/src/amount.ts` | `RES_COIN = 1e6`, `RES_DISPLAY_DECIMALS = 6`; `atomsToResDisplay` → `"1.000000 RES"` |
| `amount.test.ts` | Pins `1.000000 RES`, `0.100000 RES`, `0.000001 RES`; `RES_COIN != 100` |
| Address / status / mint / lineage | `atomsToResDisplay` |
| `public/res.html` | `/ 1000000n`, `padStart(6)` |
| `public/tx.html` | `atomsToRes` 1e6 / 6 dp |
| `uiSsr.ts` tx vouts | `/ 1000000n`, `padStart(6)` |
| `docs/explorer/RES_INDEXING_RULES.md` | `RES_COIN = 1_000_000` |

**PASS** for live explorer display and parse of RPC six-decimal strings.

Leftover: `uiSsr.test.ts` fixture `network_res_minted_display: "200.00 RES"` is mock input, not a formatter. It does not assert the forbidden pattern on rendered output, but it should not be copied as product copy.

Older audit `RIGOR_ORU_CIRCULATING_SUPPLY_ACCOUNTING_AUDIT.md` §7.2 still describes explorer `base = 100` / 2 dp; that description is **stale** (indexer was aligned in the supply-layer task). Trust `amount.ts` + tests, not that paragraph.

---

## 6. Documentation

### 6.1 PASS — live product / spec / whitepaper

| Doc | RES unit |
|-----|----------|
| `README.md` | No `1.00 RES` display |
| `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | `1_000_000 atoms per 1.000000 RES`; `W(0) = 40.000000 RES` |
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` | `40.000000 RES` at genesis |
| `docs/whitepaper/chapters/02-two-assets.md` | RFC-017 formula with `RES_COIN` |
| `docs/explorer/RES_INDEXING_RULES.md` | 6 decimals |
| `docs/wallet/CONVERGENCE_NETWORK_STATS_SPEC.md` | `1.000000 RES = 1000000` |
| `docs/specification/SPEC-002-ECONOMIC-MODEL.md` | RFC-015 100 RES mint labeled **Historical RFC**; no live `1.00 RES` display lock |

### 6.2 Historical / archive (keep labeled; not shipping UI)

RFC-001 / RFC-015 Occam, `docs/archive/proof/PROOF-*`, old LMS/release/audit reports, `contrib/alpha`, `pool/share-validator` (`RES_COIN = 100`, `100.00 RES`). Those describe a retired atom scale or a retired fixed mint. They must not be treated as the live display contract.

RFC-017 comparison table still shows old `RES_COIN = 100` vs `10^6` as a **labeled historical row**. Keep.

---

## 7. Adjacent user-visible leftover (website)

`web/rigorchain-site/src/lib/zeroTrust/constants.ts`: `RES_BASE = 100`  
`web/rigorchain-site/src/lib/zeroTrust/format.ts`: `padStart(2)` → `1.00 RES`

Public zero-trust portal is user-visible. Not consensus. Not Qt/RPC/explorer.

---

## 8. What must not be confused

| Item | Not a 6-decimal display failure |
|------|----------------------------------|
| `GetBlockSubsidy` / ConnectBlock / `RES_COIN` lock | Consensus already 1e6 |
| `100` in `RES_REWARD_LAST_EPOCH`, heights, `100 * RES_COIN` comments | Integers, not `1.00 RES` |
| `getbalance` excluding RES | Asset split, not decimal scale |
| ORU `1.00000000` (8 dp) | Different asset |
| Explorer RFC-018 `oruResTransferFeeBurnShare(1000n)` | ORU atoms, not RES display |
| Pool `100.00 RES = 10000 atoms` | Operator leftover vs RFC-017 mint `W(h)`; separate from wallet UI |

---

## 9. Required follow-up (display/parse only; no consensus)

Priority is user-facing **parse + display**, not `rigor_consensus_lock.h`.

1. **P0 — RPC parse:** `sendres` / `sendtoaddress` RES / `sendmany` RES / `createrawtransaction` RES → `AmountFromValue(..., 6)`. Then `1.000000` is 1 RES.
2. **P0 — RPC help:** replace remaining “2 decimal places” on RES amount fields; fix `core_io.h` comment.
3. **P1 — Qt:** `ConvergenceNetworkPanel::FormatResAtoms` → `BitcoinUnits::formatRes` (or `'f', 6`).
4. **P1 — website:** `RES_BASE = 1_000_000n`, `padStart(6)`.
5. **P2 — copy:** `-reswalletbootstrap` help; explorer SSR test fixture; do not rewrite labeled historical RFCs.

Do **not** change ConnectBlock, `RES_COIN`, subsidy, or mint rules to “fix” this audit.

---

## 10. Conclusion

Shipping **consensus** and the **canonical formatters** (`FormatResAmount`, `BitcoinUnits::formatRes`, explorer `atomsToResDisplay`) already implement `1 RES = 1.000000 RES`.

User-visible unification is **incomplete** because:

- several wallet/rawtx RPCs still **parse** RES as 2 decimals (wrong atom count);
- some RPC **help** still says 2 decimals while JSON prints 6;
- one Qt network panel and the public website formatter still print `1.00 RES`.

```
equivalence_claimed = false
```

No consensus change is required or recommended for this finding set.
