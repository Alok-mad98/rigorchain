# RIGOR Wallet RPC Requirements

**Task:** `TASK-WALLET-PROGRAM-004`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only RPC specification — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Defines all RPC and HTTP API requirements for the RIGOR wallet product. Classifies each method by availability and integration layer.

**Authority docs:** [FUTURE_GUI_RPC_SPEC.md](../rigor-qt/FUTURE_GUI_RPC_SPEC.md) · [WALLET_RPC_COMPATIBILITY.md](./WALLET_RPC_COMPATIBILITY.md) · [EXPLORER_RPC_COMPATIBILITY_MATRIX.md](../explorer/EXPLORER_RPC_COMPATIBILITY_MATRIX.md)

**Integration note:** Production `rigor-qt` uses **`interfaces::Wallet` / `interfaces::Node`** (embedded C++) for hot signing paths. JSON-RPC is used by **RPC Console**, **Observer**, and **optional** HTTP self-calls — not required for Send/Convergence in-process.

---

## 1. Classification legend

| Class | Meaning |
|-------|---------|
| **Already Exists** | Implemented in `core/`; wallet UI can call today |
| **Needs Extension** | Exists but requires new fields, GUI wiring, or reliability work |
| **Needs Observer Layer** | Required for read-only Observer product (`wallet-observer-mvp/`) |
| **Needs Explorer Layer** | Required HTTP indexer surface; non-consensus display only |

---

## 2. Wallet RPC — spending and balances

| Method | Class | rigor-qt Use | Observer Use | Implementation File |
|--------|-------|--------------|--------------|---------------------|
| `getbalances` | **Already Exists** | ORU + RES per-asset totals (via `interfaces::Wallet`) | Balance proxy | `wallet/rpc/coins.cpp` |
| `getbalance` | **Already Exists** | ORU-only legacy; prefer `getbalances` | — | `wallet/rpc/coins.cpp` |
| `listunspent` | **Already Exists** | Coin control; includes `asset` field per UTXO | — | `wallet/rpc/coins.cpp` |
| `sendtoaddress` | **Already Exists** | ORU send | — | `wallet/rpc/rpcspend.cpp` |
| `sendres` | **Already Exists** | RES send | — | `wallet/rpc/rpcspend.cpp` |
| `createconvergence` | **Already Exists** | Convergence ORU commit | — | `wallet/rpc/rpcspend.cpp` |
| ~~`createresclaim`~~ | **Removed (SECURITY-001-A / A-7)** | Drove the RES script path, whose signature covered the σ digest instead of the spending transaction, so a broadcast claim could be re-pointed by anyone. RES transfers use `sendres` over the key path. | — | — |
| `sendmany` | **Already Exists** | Batch ORU (Core-compatible) | — | `wallet/rpc/rpcspend.cpp` |
| `send` | **Already Exists** | Core unified send RPC | — | `wallet/rpc/rpcspend.cpp` |
| `fundrawtransaction` | **Already Exists** | PSBT / raw funding | — | `wallet/rpc/rpcspend.cpp` |
| `signrawtransactionwithwallet` | **Already Exists** | PSBT sign | — | `wallet/rpc/rpcspend.cpp` |
| `bumpfee` / `psbtbumpfee` | **Already Exists** | Fee bump | — | `wallet/rpc/rpcspend.cpp` |
| `lockunspent` / `listlockunspent` | **Already Exists** | Coin control locks | — | `wallet/rpc/coins.cpp` |
| `settxfee` | **Already Exists** | Fee preference | — | `wallet/rpc/rpcspend.cpp` |

### Extension requirements (wallet RPC)

| Method | Extension Needed | GUI Impact | Priority |
|--------|------------------|------------|----------|
| `createconvergence` | **Needs Extension** — testnet runtime reliability; clearer error strings | ConvergencePage send failures | **P0** |
| `listunspent` | **Needs Extension** — GUI must filter on `asset` field consistently | Coin control + Send | **P1** (partially done) |
| `getbalances` | **Needs Extension** — ensure RES immature/pending breakdown exposed to Qt | Overview RES pending | **P1** |

---

## 3. Wallet RPC — history and addresses

| Method | Class | rigor-qt Use | Observer Use |
|--------|-------|--------------|--------------|
| `listtransactions` | **Already Exists** | History fallback | Per-wallet |
| `listsinceblock` | **Already Exists** | History sync | — |
| `gettransaction` | **Already Exists** | Tx detail | Tx viewer |
| `listreceivedbyaddress` | **Already Exists** | Receive tab | — |
| `getreceivedbyaddress` | **Already Exists** | Address stats | — |
| `getnewaddress` | **Already Exists** | Receive | — |
| `getaddressinfo` | **Already Exists** | Address type (v0/v1) | Watch validation |
| `importaddress` / `importmulti` | **Already Exists** | Watch-only | Watch Registry |
| `listaddressgroupings` | **Already Exists** | — | — |
| `abandontransaction` | **Already Exists** | Context menu | — |
| `rescanblockchain` | **Already Exists** | Wallet menu | — |

| Method | Class | Notes |
|--------|-------|-------|
| `getresmintstatus` | **Needs Extension** | Referenced in prototype; **not registered** in production `core/src/rpc/`. Either add RPC or use explorer-only path per FUTURE_GUI_RPC_SPEC |

**Recommendation:** Do **not** add `getresmintstatus` for testnet RC unless operator testing requires it. Use Explorer `/api/res/mints` as primary; `listconvergencehistory` as RPC fallback.

---

## 4. Blockchain / convergence RPC (node)

| Method | Class | rigor-qt Use | Observer Use | File |
|--------|-------|--------------|--------------|------|
| `getblockchaininfo` | **Already Exists** | Sync, height | Dashboard | `rpc/blockchain.cpp` |
| `getnetworkinfo` | **Already Exists** | Peers | Network card | `rpc/net.cpp` |
| `getconvergencestatus` | **Already Exists** | RES total, last winner | Convergence tab | `rpc/blockchain.cpp` |
| `getconvergenceresult` | **Already Exists** | \|F\|, roots, winner, `res_probability_target` | Convergence detail | `rpc/blockchain.cpp` |
| `listconvergencehistory` | **Already Exists** | RES mint RPC fallback | History | `rpc/rawtransaction.cpp` |
| `getblock` | **Already Exists** | Block detail | — | `rpc/blockchain.cpp` |
| `getrawtransaction` | **Already Exists** | Tx proof fallback | Tx viewer | `rpc/rawtransaction.cpp` |
| `gettxoutsetinfo` | **Already Exists** | ORU supply fallback (UTXO set total) | Dashboard fallback | `rpc/blockchain.cpp` |
| `getblockhash` | **Already Exists** | Height → hash | — | `rpc/blockchain.cpp` |
| `getbestblockhash` | **Already Exists** | Tip | — | `rpc/blockchain.cpp` |

### Extension requirements (blockchain RPC)

| Method | Extension Needed | Detail |
|--------|------------------|--------|
| `getconvergenceresult` | **Needs Extension** (GUI wiring only) | Fields exist; ConvergencePage must display `res_probability_target`, root mismatch |
| `getconvergencestatus` | **Needs Extension** (GUI wiring only) | Partial display today; complete \|F\| and root on tip |
| `getblockchaininfo` | **Already Exists** | Includes `res_probability_target` at chain level — usable on Dashboard About panel |

### Embedded C++ equivalents (rigor-qt preferred)

| RPC | C++ API | Used By |
|-----|---------|---------|
| `getconvergenceresult` | `interfaces::Node::getConvergenceBlockResult` | `ConvergencePage`, `WalletModel::tryGetConvergenceBlockResult` |
| `getbalances` | `interfaces::Wallet::getBalances` | `WalletModel`, `OverviewPage` |
| `createconvergence` | `WalletModel::sendConvergenceOru` | `ConvergencePage` |

---

## 5. Wallet management RPC

| Method | Class | rigor-qt Use |
|--------|-------|--------------|
| `createwallet` | **Already Exists** | Create wallet dialog |
| `loadwallet` | **Already Exists** | Open wallet |
| `unloadwallet` | **Already Exists** | Close wallet |
| `encryptwallet` | **Already Exists** | Encrypt menu |
| `walletpassphrase` | **Already Exists** | Unlock |
| `walletlock` | **Already Exists** | Lock |
| `walletpassphrasechange` | **Already Exists** | Change passphrase |
| `backupwallet` | **Already Exists** | Backup menu |
| `getwalletinfo` | **Already Exists** | Diagnostics |
| `listwallets` | **Already Exists** | Multi-wallet |
| `getdescriptorinfo` | **Already Exists** | Advanced |
| `importdescriptors` | **Already Exists** | Descriptor import |

**Class:** All **Already Exists** — **KEEP**; no protocol changes.

---

## 6. Explorer HTTP API (Needs Explorer Layer)

Base URL: configurable (`OptionsModel`, default `http://127.0.0.1:8080`).

| Endpoint | Class | Consumer | Required Fields |
|----------|-------|----------|-----------------|
| `GET /healthz` | **Needs Explorer Layer** | ExplorerClient, Dashboard banner | `ok`, `chain.last_height`, `critical_root_mismatch` |
| `GET /api/status` | **Needs Explorer Layer** | Dashboard supply, integrity | `economic_snapshot.*`, `block_health.*`, `equivalence_claimed` |
| `GET /api/res/mints` | **Needs Explorer Layer** | ResMintPanel, RES page | `mint_txid`, `mint_amount`, `bind_height`, `mint_vout_index` |
| `GET /api/tx/{txid}` | **Needs Explorer Layer** | Explorer tab, tx drill-down | Standard tx JSON |
| `GET /api/block/{hash}` | **Needs Explorer Layer** | Block panels | Header + tx list |
| `GET /api/lineage/{txid}` | **Needs Explorer Layer** | Inspector *(optional P2)* | Lineage tree |
| `GET /api/address/{address}` | **Needs Explorer Layer** | Observer address view | Balance + txs |

**Constitutional rule:** Explorer responses are **display-only**. Signing, balance truth for spendable funds, and convergence validation remain **rigord** authority.

---

## 7. Observer proxy API (Needs Observer Layer)

Routes in `wallet-observer-mvp/app/api/observer/`:

| Route | Class | Backing |
|-------|-------|---------|
| `GET /api/observer/chain` | **Needs Observer Layer** | rigord RPC + explorer |
| `GET /api/observer/res/mints` | **Needs Observer Layer** | Explorer proxy |
| `GET /api/observer/convergence/events` | **Needs Observer Layer** | Explorer / rigord |
| `GET /api/observer/tx/[txid]` | **Needs Observer Layer** | Explorer / rigord |
| `GET /api/observer/address/[address]` | **Needs Observer Layer** | rigord watch |
| `GET /api/observer/compliance` | **Needs Observer Layer** | Static `equivalence_claimed: false` |

These are **not** rigor-qt dependencies; parallel product track.

---

## 8. RPC requirements by screen

| Screen | Required RPC / API | Minimum for Testnet RC |
|--------|-------------------|------------------------|
| Dashboard | `getbalances`, `getblockchaininfo`, `getconvergencestatus`; explorer `/api/status` optional | rigord only |
| ORU Send | `sendtoaddress`, `listunspent` | **Required** |
| RES Send | `sendres`, `listunspent` | **Required** |
| Convergence | `createconvergence`, `getconvergenceresult` | **Required** |
| History | Wallet tx index (embedded) | **Required** |
| RES Mint | `/api/res/mints` | **Optional** P1 |
| Explorer | Full explorer REST | **Optional** P1 |
| Diagnostics | `getwalletinfo`, `getnetworkinfo` | **Optional** P2 |
| Observer | Observer proxy routes | Parallel deliverable |

---

## 9. RPCs explicitly NOT required

| Method / Feature | Reason |
|------------------|--------|
| New consensus RPCs | Task forbids protocol changes |
| `submitblock` in GUI | Mining not in wallet product |
| Indexer-as-truth for signing | Architecture violation |
| Equivalence assertion RPC | `equivalence_claimed` stays false in UI |
| Taproot / `tr()` wallet RPCs | Out of scope (V0/V1 only) |

---

## 10. Testnet validation matrix (RPC)

| Check | Command / Path | Pass Criterion |
|-------|----------------|----------------|
| Dual balance | `rigor-cli getbalances` | `orum` and `res` objects present |
| ORU send | `sendtoaddress` + GUI Send | Tx confirms; ORU balance updates |
| RES send | `sendres` + GUI Send | RES-only tx; fee in ORU |
| Convergence | `createconvergence` + ConvergencePage | Tx broadcast; visible in history |
| Convergence read | `getconvergenceresult $(rigor-cli getblockcount)` | `res_probability_target` == shipping windowed hex `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` (RFC-017 `E = 246`; not Historical RFC-014 `2^240`) |
| Asset UTXOs | `listunspent` | Each row has correct `asset` |
| Explorer lag | `/healthz` vs node height | Banner when lag > threshold |

User executes validation per `WALLET_RUNTIME_VALIDATION.md`.

---

## 11. Output table

| Field | Status |
|-------|--------|
| RPC Requirements | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_RPC_REQUIREMENTS — TASK-WALLET-PROGRAM-004*
