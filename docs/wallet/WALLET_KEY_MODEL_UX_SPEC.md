# WALLET_KEY_MODEL_UX_SPEC

RIGOR dual-asset wallet key-model presentation for users and UI surfaces.

**Scope:** wallet UX / status copy only.  
**Out of scope:** consensus, W/T rules, RES issuance, descriptor formats, encryption, key-derivation algorithms.

---

## 1. Key models

| Asset | Script / address | Key model | HD ranged derivation |
|-------|------------------|-----------|----------------------|
| **ORU** | Witness v0 / Bech32 (`wpkh`, BIP84-style descriptors) | Ed25519 HD wallet | **Yes** — hardened-only (`/0'/*'`, SLIP-0010) |
| **RES** | Witness v1 / Bech32m (`res(...)`) | Ed25519 HD wallet | **Yes** — same hardened-only paths; program = `SHA256(pk)` |

- **ORU = HD derived keys.** New receive/change addresses come from a ranged `wpkh` descriptor.
- **RES = HD derived keys.** Default `res([fp/86'/coin'/0']xpub/0'/*')` (or `/1'/*'` for change). Imported non-ranged `res(<32-byte hex>)` is **dedicated key mode** (watch-only or a single imported program).

Watch-only wallets cannot expand a ranged xpub: hardened derivation needs the private key. Import `addr(...)` / `wpkh(<32-byte hex>)` / `res(<32-byte hex>)` instead.

---

## 2. Wallet-wide HD flag

Bitcoin Core’s `CWallet::IsHDEnabled()` returns true only when **every** active ScriptPubKeyMan is ranged.

Default RIGOR wallets install ranged ORU and ranged RES, so `IsHDEnabled()` is **true**.

Legacy Qt copy that said **“HD key generation disabled”** remains **forbidden**.

---

## 3. Canonical user-facing status

### 3.1 Title

**Key Management Status**

### 3.2 Lines (default spendable dual-asset wallet)

```
ORU:
HD wallet enabled

RES:
HD wallet enabled
```

### 3.3 Variants

| Condition | ORU line | RES line |
|-----------|----------|----------|
| Default dual-asset | HD wallet enabled | HD wallet enabled |
| Imported non-ranged `res(hex)` | HD wallet enabled | Dedicated key mode |
| ORU HD present, no RES SPKM | HD wallet enabled | Not configured |
| Watch-only (`private_keys_disabled`) | Show watch-only / private keys disabled; do not claim HD disabled | same |

Never use “HD key generation disabled” as a wallet-wide message for dual-asset wallets.

---

## 4. Surfaces that must follow this spec

1. **Qt status-bar key icon tooltip** (`RigorGUI::setHDStatus`) — icon stays hidden; Key Management dialog carries the copy.
2. **Settings → Advanced → Key Management**
3. **CLI wallet-tool info:** ORU HD / RES HD / RES dedicated key mode.

---

## 5. Programmatic APIs (wallet-only)

| API | Meaning |
|-----|---------|
| `CWallet::IsOruHDEnabled()` / `oruHdEnabled()` | Active BECH32 SPKMs are ranged HD |
| `CWallet::IsResHDEnabled()` / `resHdEnabled()` | Active BECH32M SPKMs are ranged HD |
| `CWallet::IsResDedicatedKeyMode()` / `resDedicatedKeyMode()` | Active BECH32M SPKMs are non-ranged dedicated keys |
| `CWallet::IsHDEnabled()` / `hdEnabled()` | All active SPKMs ranged — true on default dual-asset wallets |

Private key export (`dumporuprivkey` / `dumpresprivkey` / `ExportAddressPrivateKey`) is independent of these indicators.

---

## 6. Acceptance

- No remaining user-facing “HD key generation disabled” for normal dual-asset wallets
- Default RES receive addresses advance with HD index
- Watch-only ranged xpub import fails closed (hardened without private keys)
- Private key export tests continue to pass

---

## 7. References

- `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` — cryptographic boundary (TASK-011)
- `docs/wallet/DESCRIPTOR_IMPORT_EXPORT_SPEC.md`
- `core/src/wallet/wallet.cpp` — `IsOruHDEnabled`, `IsResHDEnabled`, `IsResDedicatedKeyMode`, `IsHDEnabled`
