# DESCRIPTOR_IMPORT_EXPORT_SPEC

**Task:** TASK-WALLET-DESCRIPTOR-COLD-STORAGE-001  
**Consensus impact:** None

## Principles

1. Descriptors define spendability / watchability.
2. ORU = Witness v0 / Bech32 / `wpkh(...)`.
3. RES = Witness v1 / Bech32m / `res(...)` (Ed25519). **Never `tr()`.**
4. Checksums are mandatory on import (`desc#checksum`).
5. No account-model balances; UTXO + script verification only.

## Import RPCs

### `importdescriptors` (canonical)

```json
[{
  "desc": "wpkh([fpr/84'/0'/0']xpub.../0/*)#checksum",
  "timestamp": "now",
  "active": true,
  "range": [0, 1000],
  "internal": false,
  "label": "optional"
}]
```

RES single-key example (watch-only pubkey):

```json
[{
  "desc": "res(<64-hex-pubkey>)#checksum",
  "timestamp": "now",
  "active": true
}]
```

RES spendable (64-byte secret = seed‖pubkey, 128 hex chars):

```json
[{
  "desc": "res(<128-hex-secret>)#checksum",
  "timestamp": 1
}]
```

Notes:

- Active ranged descriptors required for HD ORU receive/change.
- Active non-ranged `res()` is allowed (fixed Witness v1 program).
- Watch-only wallets (`disable_private_keys`) reject descriptors that embed private keys.

### `importrigorprivkey` (convenience)

| Arg | Meaning |
|-----|---------|
| `privkey` | WIF (ORU) or 128-hex Ed25519 secret (RES) |
| `label` | Optional |
| `rescan` | Default true |
| `asset` | `auto` \| `ORU` \| `RES` |

Auto detection:

- Valid WIF → `wpkh(WIF)#…` → ORU
- 128 hex chars validating as Ed25519 seed‖pubkey → `res(hex)#…` → RES

Result:

```json
{
  "success": true,
  "asset": "ORU",
  "descriptor": "wpkh(...)#...",
  "address": "bc1q..."
}
```

### `importprivkey` (compat alias)

On descriptor wallets, same auto mapping as `importrigorprivkey` (returns null on success).  
Prefer `importrigorprivkey` for an explicit `asset` hint and structured result.  
Legacy SPKM path remains only if a non-descriptor wallet is loaded (not creatable on RIGOR).

### `dumpprivkey` (deprecated on descriptor wallets)

**Not supported** for RIGOR descriptor wallets. Callers receive an explicit error redirecting to:

- Export: `listdescriptors true` (or GUI Key Management → Export Descriptor, include private)
- Re-import: `importrigorprivkey` / `importdescriptors`

Address-to-key dump remains only for migrated legacy wallets. Do not document it as a Bech32/ORU/RES workflow.

## Export RPC

### `listdescriptors` / `listdescriptors true`

- Public export: safe for online watch-only import.
- Private export (`true`): **supported replacement for dumpprivkey** — cold-storage / offline backup only.

Example public lines:

```
wpkh([abcd1234/84'/0'/0']xpub.../0/*)#checksum
res(<32-byte-pubkey-hex>)#checksum
```

## GUI

**Settings → Advanced → Key Management**

- Import Private Key → `importrigorprivkey`
- Import Descriptor → `importdescriptors`
- Export Descriptor → `listdescriptors` (optional private)

## Watch-only wallet

```
createwallet "watch" true true   # disable_private_keys, blank
importdescriptors '[{ "desc": "<public desc>#cs", "timestamp": "now" }]'
```

Document alias: `watch_only=true` means `disable_private_keys=true` (plus typically `blank=true`).

## Forbidden

- Seed servers / remote key custody
- Admin / freeze / mint keys in wallet consensus path
- Treating indexer balances as ownership truth
- Importing `tr()` / Taproot semantics for RES
