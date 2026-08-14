# PRIVATE KEY EXPORT — IMPLEMENTATION REPORT

**Task:** TASK-WALLET-PRIVATE-KEY-EXPORT-001  
**Branch:** `feature/wallet-key-export` (from Alpha.1 RC / `mainnet-release-candidate-001`)  
**Scope:** Wallet-only. No consensus / W·T / RES issuance / convergence / tx / block / PoW changes.

## Summary

Descriptor-first address private key export is implemented as:

- RPC `dumporuprivkey` → ORU WIF
- RPC `dumpresprivkey` → RES Ed25519 secret hex
- Shared helper `wallet::ExportAddressPrivateKey` (address → descriptor SPKM → signing provider → key)
- Qt path: **Settings → Advanced → Security → Export Private Key** (Key Management Security tab)
- Unit + functional tests + `PRIVATE_KEY_EXPORT_SPEC.md`

Legacy `dumpprivkey` remains an explicit error redirect on descriptor wallets (now mentions the new RPCs).

## Changed files

| Path | Change |
|------|--------|
| `core/src/wallet/exportprivkey.h` | Export helper API |
| `core/src/wallet/rpc/backup.cpp` | `ExportAddressPrivateKey`, `dumporuprivkey`, `dumpresprivkey`; update `dumpprivkey` help |
| `core/src/wallet/rpc/rpcwallet.cpp` | Register new RPCs |
| `core/src/Makefile.am` | Add `wallet/exportprivkey.h` |
| `core/src/Makefile.test.include` | Add `wallet_privkey_export_tests.cpp` |
| `core/src/qt/keymanagementdialog.h/.cpp` | Security / Export Private Key tab |
| `core/src/qt/moc_keymanagementdialog.cpp` | Regenerated moc |
| `core/src/qt/walletview.h/.cpp` + moc | `gotoExportPrivateKey` |
| `core/src/qt/walletframe.h/.cpp` + moc | `gotoExportPrivateKey` |
| `core/src/qt/bitcoingui.h/.cpp` | Advanced → Security → Export Private Key menu |
| `core/src/qt/rpcconsole.cpp` | History filter for new dump RPCs |
| `core/src/test/wallet_privkey_export_tests.cpp` | Unit tests (`wallet_privkey_export_tests`) |
| `core/test/functional/wallet_exportprivkey.py` | Functional ORU/RES + negatives |
| `core/test/functional/test_runner.py` | Register functional test |
| `docs/wallet/PRIVATE_KEY_EXPORT_SPEC.md` | Spec |
| `docs/wallet/DESCRIPTOR_IMPORT_EXPORT_SPEC.md` | Cross-links |
| `docs/wallet/PRIVATE_KEY_EXPORT_IMPLEMENTATION_REPORT.md` | This report |

## RPC examples

```bash
# ORU
rigor-cli dumporuprivkey "rgrt1q..."
# → { "address", "asset":"ORU", "private_key":"<WIF>", "format":"WIF" }

# RES
rigor-cli dumpresprivkey "rgrt1p..."
# → { "address", "asset":"RES", "private_key":"<128-hex>", "format":"ed25519" }
```

Errors (message substrings):

| Case | Message |
|------|---------|
| Locked | `Wallet is locked` |
| Watch-only | `Private key unavailable for watch-only descriptor` |
| Unknown | `Address not found in wallet` |
| Wrong asset | `Only ORU addresses are accepted` / `Only RES addresses are accepted` |

## Architecture preserved

```
Address → scriptPubKey → DescriptorScriptPubKeyMan
       → GetSigningProvider(..., include_private=true)
       → ORU EncodeSecret | RES res_v1_program_to_ed_secret
```

Not implemented (by design):

- Legacy `importprivkey` as primary wallet model
- Parallel private-key DB / `CTxOut.asset` storage
- Private key display on Overview / Receive / Transactions / Convergence

## Tests

**Unit** (`core/src/test/wallet_privkey_export_tests.cpp`):

- ORU WIF export round-trip address match
- RES 128-hex secret format
- Wrong asset / unknown address
- Watch-only flag rejection

**Functional** (`core/test/functional/wallet_exportprivkey.py`):

- ORU export → `importrigorprivkey` external → address match
- RES export → secret equals `listdescriptors true` → re-import
- Unknown / locked / watch-only / wrong asset / `dumpprivkey` redirect

## Security review

| Control | Status |
|---------|--------|
| Unlock required before export | PASS (`IsLocked` → `Wallet is locked`) |
| Watch-only blocked | PASS (`WALLET_FLAG_DISABLE_PRIVATE_KEYS` + missing material) |
| Ownership check | PASS (`IsMine == ISMINE_NO`) |
| Asset gating | PASS (ORU/RES dedicated RPCs) |
| Advanced-only UI + warning | PASS (Security menu / tab; confirm dialog) |
| RPC console history filter | PASS (`dumporuprivkey` / `dumpresprivkey`) |
| No consensus path changes | PASS (see below) |

## Consensus diff verification

This task did **not** edit:

- `core/src/consensus/`
- `core/src/validation*`
- `core/src/kernel/`
- `core/src/node/auto_res_mint.cpp`
- `core/src/chainparams*`

Note: the parent RC working tree may already contain unrelated dirty files under some of those paths. They are outside this feature’s change set. Verify for this branch with:

```bash
git diff mainnet-release-candidate-001 -- \
  core/src/consensus/ \
  core/src/validation.cpp core/src/validation.h \
  core/src/kernel/ \
  core/src/node/auto_res_mint.cpp \
  core/src/chainparams.cpp core/src/chainparamsbase.cpp
# Expected for this task: empty (or only pre-existing RC dirt if not cleaned)
```

## Validation steps (run locally)

Per project policy, full rebuild is user-run:

```bash
cd ./core
make -j$(nproc)

# Wallet unit tests (includes wallet_privkey_export_tests)
./src/test/test_bitcoin --run_test=wallet*

# Functional
./test/functional/wallet_exportprivkey.py --descriptors

# Qt tests
./src/qt/test/test_rigor-qt
```

### Agent smoke compile (already done)

Succeeded for:

- `wallet/rpc/libbitcoin_wallet_a-backup.o`
- `test/test_bitcoin-wallet_privkey_export_tests.o`
- `qt/libbitcoinqt_a-keymanagementdialog.o` (+ moc)
- `qt/libbitcoinqt_a-walletview.o` / `walletframe.o` (+ moc)

## Acceptance checklist

| Criterion | Status |
|-----------|--------|
| ORU address can export private key | Implemented + unit/functional coverage |
| RES address can export private key | Implemented + unit/functional coverage |
| Descriptor architecture preserved | Yes |
| Watch-only protected | Yes |
| Locked wallet protected | Yes |
| No consensus changes in this task | Yes |
| Wallet tests PASS | Pending user `make` + `test_bitcoin --run_test=wallet*` |
| Qt tests PASS | Pending user `test_rigor-qt` |
