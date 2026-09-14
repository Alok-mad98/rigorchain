# RIGOR_COLD_STORAGE_GUIDE

**Document status:** Production documentation  
**Philosophy:** Bitcoin Core descriptor wallet + Electrum-style offline signing + RIGOR dual-asset scripts  
**Never:** seed servers, admin recovery, account-model balances as truth

## Roles

| Machine | Holds | Does |
|---------|-------|------|
| Offline signer | Private descriptors / keys | Generate keys, sign PSBTs |
| Online watch-only | Public descriptors only | Monitor ORU/RES, build unsigned PSBTs, broadcast |

## Asset mapping

| Asset | Script | Descriptor | Address |
|-------|--------|------------|---------|
| ORU | Witness v0 P2WPKH | `wpkh(...)` | Bech32 |
| RES | Witness v1 Ed25519 | `res(...)` | Bech32m |

Do **not** use Bitcoin Taproot `tr()` for RES.

## Workflow A — Offline key generation + online watch

### Offline machine

1. Install air-gapped RIGOR Wallet / `rigor-cli` (same release family as online).
2. Create a spendable descriptor wallet (private keys enabled).
3. Generate ORU receive addresses (`getnewaddress "" "bech32"`).
4. Generate RES receive addresses (`getnewaddress "" "bech32m"`).
5. Export **public** descriptors:

```bash
rigor-cli -rpcwallet=cold listdescriptors
```

Or GUI: **Settings → Advanced → Key Management → Export Descriptor** (private unchecked).

6. Optionally export **private** descriptors to paper/encrypted USB (never to the online host):

```bash
rigor-cli -rpcwallet=cold listdescriptors true
```

7. Shut down offline machine before moving any private material.

### Online machine

1. Create watch-only wallet:

```bash
rigor-cli createwallet "watch" true true   # disable_private_keys, blank
```

GUI: Create Wallet → **Watch-only (disable private keys)** + blank.

2. Import public descriptors:

```bash
rigor-cli -rpcwallet=watch importdescriptors '[
  {"desc":"wpkh([...]xpub.../0/*)#cs","timestamp":"now","active":true,"range":[0,1000]},
  {"desc":"wpkh([...]xpub.../1/*)#cs","timestamp":"now","active":true,"internal":true,"range":[0,1000]},
  {"desc":"res(<pubkeyhex>)#cs","timestamp":"now","active":true}
]'
```

GUI: **Key Management → Import Descriptor**.

3. Receive ORU / RES to addresses derived from those descriptors.
4. Verify balances and convergence mint status on the online node (script + chain rules; not an external “asset DB”).

### Spend (PSBT air-gap)

1. Online: build funded PSBT (`walletcreatefundedpsbt` / Send UI → create PSBT).
2. Transfer PSBT to offline signer (QR/USB).
3. Offline: `walletprocesspsbt` / sign.
4. Online: finalize + `sendrawtransaction`.

## Workflow B — Import an existing private key

Use when recovering a single key (not preferred vs HD descriptor backup).

```bash
# ORU WIF
rigor-cli -rpcwallet=spend importrigorprivkey "<WIF>" "" true ORU

# RES 64-byte Ed25519 secret hex (seed||pubkey)
rigor-cli -rpcwallet=spend importrigorprivkey "<128 hex chars>" "" true RES
```

`importprivkey` on descriptor wallets is a **compatibility alias** with the same auto-detection (prefer `importrigorprivkey`).

**Do not use `dumpprivkey`.** On descriptor wallets it errors and redirects to `listdescriptors true`.

GUI: **Key Management → Import Private Key**.

## Exporting private material (replaces dumpprivkey)

```bash
rigor-cli -rpcwallet=spend listdescriptors true
```

GUI: **Key Management → Export Descriptor** (include private). Keep offline only.

## Workflow C — Full wallet restore from descriptors

1. Create blank spendable wallet (or watch-only if public-only).
2. `importdescriptors` with saved descriptor list (`timestamp` = original creation time or `0`/`1` for full rescan).
3. Wait for rescan; confirm addresses and UTXOs.
4. Prefer restoring private descriptors on an offline machine, then exporting public descriptors to online watch-only.

## RES / convergence note

RES minting and W=32 convergence rules are **consensus**. The wallet only:

- Holds `res()` descriptors
- Watches Bech32m outputs
- Signs RES spends when private material is present

Cold storage does not change issuance parameters.

## Security checklist

- [ ] Offline machine never networked after private key generation
- [ ] Public descriptors only on online watch-only wallet
- [ ] Private descriptor backups encrypted / paper; multiple copies
- [ ] No seed uploaded to any server
- [ ] Verify receive addresses on both machines before large transfers
- [ ] Test restore with a small amount before retiring the old wallet
