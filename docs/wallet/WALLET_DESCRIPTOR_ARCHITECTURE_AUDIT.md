# WALLET_DESCRIPTOR_ARCHITECTURE_AUDIT

**Task:** TASK-WALLET-DESCRIPTOR-COLD-STORAGE-001  
**Scope:** Wallet layer only (no consensus / RFC / W=32 / RES issuance changes)  
**Date:** 2026-08-07

## Executive verdict

RIGOR Core already mandates **descriptor wallets**. Legacy wallet creation is disabled (`descriptors=false` rejected). The product gap was not “missing descriptors,” but **legacy-shaped import UX** (`importprivkey` → `EnsureLegacyScriptPubKeyMan` → “Only legacy wallets are supported”) plus missing GUI key-management for cold storage.

This task closes that gap while keeping:

- Script classification for assets (ORU Witness v0 / RES Witness v1)
- No `CTxOut` asset fields
- No Taproot / BIP341 (`tr()` permanently rejected; RES uses `res()`)

## Ownership model (target = current direction)

```
Private Key
    → Public Key Derivation
    → Descriptor (ownership definition)
    → ScriptPubKey
    → Address (Bech32 ORU / Bech32m RES)
```

Descriptors are the primary wallet ownership record. Addresses are derived views.

## Audit by area

### `src/wallet/`

| Topic | Finding |
|--------|---------|
| Default wallet type | Descriptor-only create path (`WALLET_FLAG_DESCRIPTORS` forced) |
| ScriptPubKeyMans | `DescriptorScriptPubKeyMan` for ORU (`wpkh` / Bech32) and RES (`res` / BECH32M) |
| Legacy SPKM | Still present for migrated/import legacy edge cases; not a supported create path |
| RES descriptors | `res(<32-byte pubkey hex\|64-byte secret hex>)` — Ed25519 Witness v1, **not** `tr()` |
| ORU descriptors | `wpkh(...)` ranged HD or single-key |
| Watch-only | `createwallet` + `disable_private_keys` + `blank` + `importdescriptors` |
| Private key import (pre-fix) | `importprivkey` required Legacy SPKM → failed on modern wallets |
| Private key import (post-fix) | Descriptor path: `importprivkey` / `importrigorprivkey` → `wpkh` or `res` via `importdescriptors` machinery |
| Export | `listdescriptors` (public or private) for cold-storage backup |

### `src/rpc/` / wallet RPC

| RPC | Role |
|-----|------|
| `importdescriptors` | Canonical import |
| `listdescriptors` | Canonical export / backup |
| `getdescriptorinfo` | Checksum / parse helper |
| `importprivkey` | Compat alias on descriptor wallets (same auto path as `importrigorprivkey`); legacy path only if a migrated non-descriptor wallet is loaded |
| `dumpprivkey` | **Deprecated on descriptor wallets** — errors with redirect to `listdescriptors true`; legacy dump retained only for migrated wallets |
| `importrigorprivkey` | Explicit ORU/RES mapping + structured result |
| `createwallet` | `disable_private_keys` = watch-only / online cold-storage observer |

### `src/script/`

| Item | Behavior |
|------|----------|
| `wpkh` | Witness v0 P2WPKH (ORU) |
| `res` | Witness v1 Ed25519 RES |
| `tr` / `rawtr` | Hard-rejected (architecture freeze) |
| Address codecs | Bech32 (v0) / Bech32m (v1) |

### `src/qt/`

| Item | Pre-task | Post-task |
|------|----------|-----------|
| Create Wallet | Descriptor default; “Disable Private Keys” | Relabeled watch-only; same flags |
| Key import/export GUI | Missing (RPC only) | **Settings → Advanced → Key Management** |
| Cold storage UX | Undocumented in GUI | Import key / import descriptor / export descriptors |

## Legacy assumptions removed (product path)

1. “Import private key” must not require a legacy wallet.
2. RES must not be described as Taproot `tr()` in wallet UX.
3. Watch-only wallets import **public** descriptors only.
4. Backup unit of account is the **descriptor (+ checksum)**, not an account balance table.

## Gaps intentionally out of scope

- Consensus / `res_proof` / convergence / W=32 parameters
- Seed servers, admin recovery, external indexer as truth source
- Account-model balances as ownership source

## Security notes

- Private descriptor export is offline-only material.
- Watch-only wallets reject private key import.
- No centralized recovery path is introduced.
