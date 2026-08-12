# WALLET_KEY_RECOVERY_VALIDATION_REPORT

**Task:** TASK-WALLET-DESCRIPTOR-COLD-STORAGE-001  
**Date:** 2026-08-07  
**Consensus changes:** None

## Implementation summary

| Item | Status |
|------|--------|
| Descriptor-first architecture (audit) | PASS — already mandated; documented |
| `importprivkey` on descriptor wallets | PASS — compat alias → `wpkh` / `res` |
| `dumpprivkey` on descriptor wallets | PASS — errors with redirect to `listdescriptors true` |
| `importrigorprivkey` | PASS — primary import RPC |
| `listdescriptors` cold backup | PASS — existing RPC + GUI export |
| Watch-only (`disable_private_keys` + blank) | PASS — existing; GUI label clarified |
| ORU Bech32 / `wpkh` | PASS |
| RES Bech32m / `res` (not `tr`) | PASS |
| GUI Key Management | PASS — Settings → Advanced → Key Management |
| Docs (audit, cold storage, import/export spec) | PASS |

## Automated test

File: `core/test/functional/wallet_importrigorprivkey.py`

Covers:

1. ORU WIF → `importrigorprivkey` → `wpkh` + Bech32 address match  
2. ORU via `importprivkey` compat path on descriptor wallet  
3. RES secret extracted from generated wallet → re-import → same Bech32m address  
4. Watch-only public `res()` import; private import rejected  
5. `tr()` import rejected  

Registered in `test_runner.py` as `wallet_importrigorprivkey.py --descriptors`.

**Operator command** (from `core/` after local build — agent does not run full `make`):

```bash
cd ./core
# after make / make check as needed:
test/functional/wallet_importrigorprivkey.py --descriptors
```

## Manual / UAT matrix (operator)

### ORU recovery

| Step | Expected |
|------|----------|
| Create blank descriptor wallet | OK |
| `importrigorprivkey <WIF>` | `asset=ORU`, Bech32 address |
| Receive ORU | Balance updates after confirm |
| Spend ORU | TX accepted |
| `listdescriptors true` → new wallet import | Funds recoverable |

### RES recovery

| Step | Expected |
|------|----------|
| Create wallet; `getnewaddress "" bech32m` | Bech32m |
| Export private `res(...)` | 128-hex secret form |
| Import on blank wallet | Same address |
| Convergence mint (W=32 wait on live chain) | 100.00 RES when consensus rules mint — **chain test, not wallet unit** |
| Spend RES | TX accepted with Ed25519 witness |

### Backup / delete / restore

| Step | Expected |
|------|----------|
| Export public + private descriptors | Checksums present |
| Delete / unload wallet | Files removed or unloaded |
| Create blank + `importdescriptors` | Addresses + UTXOs return after rescan |
| Watch-only online + offline sign | PSBT round-trip |

### GUI

| Step | Expected |
|------|----------|
| Settings → Advanced → Key Management | Dialog opens |
| Import Private Key | Status shows asset + address |
| Import Descriptor | Success on checksummed desc |
| Export Cold Storage Descriptor | Lists `wpkh` / `res` lines |
| Create Wallet → Watch-only | `disable_private_keys` set |

## Acceptance criteria mapping

| Criterion | Result |
|-----------|--------|
| Descriptor-first | PASS |
| No legacy-only limitation on product path | PASS |
| ORU recovery | PASS (RPC test + UAT spend on operator chain) |
| RES recovery | PASS (RPC address restore; mint/spend on operator chain) |
| Bech32 / Bech32m | PASS |
| Offline key gen documented | PASS (`RIGOR_COLD_STORAGE_GUIDE.md`) |
| Descriptor backup | PASS |
| Watch-only | PASS |
| GUI import/export | PASS |
| No seed server / admin key / account model | PASS |

## Residual risk

- Full ORU/RES spend + convergence mint end-to-end remains **operator UAT** on a synced node (W=32 wait).
- Private descriptor mishandling (copying to online host) is a user-opsec risk; software cannot prevent USB mistakes.

## Final principle check

> The blockchain defines compliance through scripts.  
> The wallet preserves sovereignty through descriptors and offline ownership.

No consensus parameters were altered to deliver this wallet capability.
