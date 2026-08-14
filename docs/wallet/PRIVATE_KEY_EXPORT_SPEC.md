# PRIVATE KEY EXPORT SPEC

**Task:** TASK-WALLET-PRIVATE-KEY-EXPORT-001  
**Consensus impact:** None (wallet-only)

## Why descriptor-first

RIGOR wallets are descriptor wallets. Spendability is defined by imported/generated descriptors
(`wpkh(...)` for ORU, `res(...)` for RES), not by a legacy key pool or a parallel private-key
database on `CTxOut`.

Address-level export is a **convenience** over the same descriptor signing path used for spending:

```
Address
  → DecodeDestination / asset from witness version
  → scriptPubKey
  → DescriptorScriptPubKeyMan
  → GetSigningProvider(script, include_private=true)
  → ORU: EncodeSecret (WIF) | RES: Ed25519 secret hex
```

This preserves:

- Script-as-compliance-layer classification (asset from `scriptPubKey` / address type)
- Cold-storage and watch-only workflows via public descriptors
- No revival of legacy `importprivkey`-centric wallet design as the primary model

## Difference vs legacy `importprivkey` / `dumpprivkey`

| Legacy Bitcoin Core | RIGOR descriptor-first |
|---------------------|------------------------|
| `dumpprivkey` dumps from key store by address | `dumpprivkey` **errors** on descriptor wallets |
| `importprivkey` writes keys into legacy SPKM | Prefer `importdescriptors` / `importrigorprivkey` (creates `wpkh`/`res` descriptors) |
| Key DB is the source of truth | Descriptor + signing provider is the source of truth |

RIGOR address export RPCs:

- `dumporuprivkey` — ORU only
- `dumpresprivkey` — RES only

They **extract** key material already present in descriptors; they do **not** create a new key DB
or bypass descriptors.

Full-wallet offline backup remains: `listdescriptors true` (GUI Key Management → Export Descriptor).

## ORU key format

- Address: Witness v0 / Bech32 (e.g. regtest `rgrt1q…`)
- Descriptor family: `wpkh(...)`
- Export: WIF (`format: "WIF"`)
- Re-import: `importrigorprivkey <WIF> "" false ORU` → `wpkh(WIF)#checksum`

## RES key format

- Address: Witness v1 / Bech32m (e.g. regtest `rgrt1p…`) — same HRP as ORU; asset from witness version
- Descriptor family: `res(...)` (never `tr()`)
- Export: 64-byte Ed25519 secret hex = seed‖pubkey (`format: "ed25519"`, 128 hex chars)
- Re-import: `importrigorprivkey <128-hex> "" false RES` → `res(hex)#checksum`

## RPC results

### `dumporuprivkey`

```json
{
  "address": "rgrt1q...",
  "asset": "ORU",
  "private_key": "<WIF>",
  "format": "WIF"
}
```

### `dumpresprivkey`

```json
{
  "address": "rgrt1p...",
  "asset": "RES",
  "private_key": "<128-hex>",
  "format": "ed25519"
}
```

## Security requirements

1. **Wallet unlocked** — encrypted + locked → `Wallet is locked` (`RPC_WALLET_UNLOCK_NEEDED`)
2. **Watch-only** — `disable_private_keys` or missing private material →
   `Private key unavailable for watch-only descriptor`
3. **Ownership** — address not in wallet → `Address not found in wallet`
4. **Wrong asset** — ORU RPC with RES address (or reverse) → rejected
5. **UI** — export only under Settings → Advanced → Security → Export Private Key  
   (also available as Key Management tab). Not on Overview / Receive / Transactions / Convergence.

### Warning (must show before GUI export)

```
Warning:
Anyone with this private key can control this address.

Never share this key.
```

## Forbidden

- Implementing `importprivkey` as a return to legacy non-descriptor wallets
- Creating a parallel private-key database outside descriptors
- Consensus / W·T / RES issuance / convergence / tx / block / PoW changes
- Displaying private keys on primary wallet pages
