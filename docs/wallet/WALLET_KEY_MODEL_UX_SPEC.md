# WALLET_KEY_MODEL_UX_SPEC

RIGOR dual-asset wallet key-model presentation for users and UI surfaces.

**Scope:** wallet UX / status copy only.  
**Out of scope:** consensus, W/T rules, RES issuance, descriptor formats, encryption, key-derivation algorithms.

---

## 1. Key models

| Asset | Script / address | Key model | HD ranged derivation |
|-------|------------------|-----------|----------------------|
| **ORU** | Witness v0 / Bech32 (`wpkh`, BIP84-style descriptors) | Bitcoin-style HD wallet | **Yes** — ranged ScriptPubKeyMan |
| **RES** | Witness v1 / Bech32m (`res(...)`) | Dedicated Ed25519 asset key | **No** — single-slot ownership |

- **ORU = HD derived keys.** New receive/change addresses come from a ranged descriptor keychain.
- **RES = dedicated asset key.** The active `res()` descriptor is intentionally non-ranged; repeated `getnewaddress` for RES returns the same receive slot (change may use a separate internal slot).

These models coexist in one descriptor wallet. That is expected architecture, not a misconfiguration.

---

## 2. Why Core’s wallet-wide HD flag misleads

Bitcoin Core’s `CWallet::IsHDEnabled()` returns true only when **every** active ScriptPubKeyMan is ranged.

Default RIGOR wallets install:

1. Ranged ORU (`wpkh`) SPKMs → HD  
2. Non-ranged RES (`res`) SPKMs → not HD  

Therefore `IsHDEnabled() == false` on healthy dual-asset wallets even though ORU HD generation works.

Legacy Qt copy tied the status bar to that boolean and showed **“HD key generation disabled”**. That string is **forbidden** in RIGOR UI.

`IsHDEnabled()` remains unchanged for internal range-end / rescan behavior. UI must not treat it as the user-facing ORU HD indicator.

---

## 3. Canonical user-facing status

### 3.1 Title

**Key Management Status**

### 3.2 Lines (default spendable dual-asset wallet)

```
ORU:
HD wallet enabled

RES:
Dedicated key mode
```

Equivalent one-line / tooltip form:

```
Key Management Status
ORU: HD wallet enabled
RES: Dedicated key mode
```

### 3.3 Variants

| Condition | ORU line | RES line |
|-----------|----------|----------|
| Default dual-asset | HD wallet enabled | Dedicated key mode |
| ORU HD present, no RES SPKM | HD wallet enabled | Not configured |
| Watch-only (`private_keys_disabled`) | Show watch-only / private keys disabled; do not claim HD disabled | same |
| ORU SPKMs missing or non-ranged | HD wallet not available | Dedicated key mode / Not configured as applicable |

Never use “HD key generation disabled” as a wallet-wide message for dual-asset wallets.

---

## 4. Surfaces that must follow this spec

1. **Qt status-bar key icon tooltip** (`RigorGUI::setHDStatus`)  
   - Icon: `hd_enabled` when ORU HD is available (and not watch-only); eye for watch-only.  
   - Tooltip: Key Management Status with ORU / RES lines above.

2. **Settings → Advanced → Key Management** (and Security / Export Private Key entry points into the same dialog)  
   - Persistent **Key Management Status** group with the same wording.

3. **Any other tooltip or banner** that previously said HD was disabled wallet-wide.

4. **CLI `bitcoin-wallet` / rigor wallet-tool info** (optional consistency): report ORU HD and RES dedicated mode rather than a single misleading HD yes/no.

---

## 5. Programmatic APIs (wallet-only)

| API | Meaning |
|-----|---------|
| `CWallet::IsOruHDEnabled()` / `interfaces::Wallet::oruHdEnabled()` | Active BECH32 SPKMs are ranged HD |
| `CWallet::IsResDedicatedKeyMode()` / `interfaces::Wallet::resDedicatedKeyMode()` | Active BECH32M SPKMs are non-ranged dedicated keys |
| `CWallet::IsHDEnabled()` / `hdEnabled()` | Unchanged Core semantics (all active SPKMs ranged) — **not** for dual-asset UX copy |

Private key export (`dumporuprivkey` / `dumpresprivkey` / `ExportAddressPrivateKey`) is independent of these indicators and must remain available on default dual-asset wallets when unlocked.

---

## 6. Acceptance

- No remaining user-facing “HD key generation disabled” for normal dual-asset wallets  
- ORU HD derivation behavior unchanged  
- RES single-slot dedicated key model unchanged  
- Private key export tests continue to pass  
- No consensus / descriptor / encryption changes

---

## 7. References

- `docs/wallet/WALLET_HD_STATUS_AUDIT_REPORT.md` — root-cause audit  
- `docs/freeze/WALLET_SEMANTIC_FREEZE.md` — RES single-slot freeze  
- `core/src/wallet/wallet.cpp` — `IsOruHDEnabled`, `IsResDedicatedKeyMode`, `IsHDEnabled`  
- `core/src/qt/bitcoingui.cpp` — status-bar presentation  
- `core/src/qt/keymanagementdialog.cpp` — Key Management Status panel  
