# WALLET HD STATUS AUDIT REPORT

**Task:** TASK-WALLET-HD-STATUS-AUDIT-001  
**Scope:** Wallet key architecture + Qt HD indicator only  
**Consensus / W·T / RES issuance / tx format:** untouched  
**Date:** 2026-08-13  
**Verdict:** **PASS** — root cause of “HD key generation disabled” is conclusively identified

---

## Executive summary

The crossed HD indicator is **mechanically correct** for RIGOR’s dual-asset descriptor wallet, not a broken seed or failed keypool init.

| Layer | Reality |
|--------|---------|
| ORU (`wpkh` / Witness v0 / `OutputType::BECH32`) | **Ranged HD** — BIP84-style `wpkh([fp/84'/coin'/0']/xpub/{0,1}/*)` from a transient BIP32 master at wallet create |
| RES (`res` / Witness v1 / `OutputType::BECH32M`) | **Non-ranged single-slot** — random Ed25519 key embedded as `res(<64-byte secret hex>)`; not BIP32-derived |
| Wallet-level `CWallet::IsHDEnabled()` | **false** whenever *any* active ScriptPubKeyMan is non-ranged — including every default wallet that has active RES SPKMs |
| Qt status bar | Calls `wallet().hdEnabled()` and shows “HD key generation is **disabled**” + `hd_disabled` icon |

**Classification of the UI symptom:**

| Code | Meaning | Applies? |
|------|---------|----------|
| **A. Correct status display** | UI matches `CWallet::IsHDEnabled()` | **Yes (primary)** |
| B. UI bug | Wrong binding / stale icon | No |
| C. Wallet initialization bug | Seed/keypool failed to set up | No |
| **D. Missing feature / UX gap** | No dual-asset HD messaging; Bitcoin Core “all SPKMs HD” wording misleads users | **Yes (secondary, product)** |

Private-key export (`dumporuprivkey` / `dumpresprivkey`) does **not** depend on wallet-level HD status and is compatible with the current architecture.

---

## 1. Current HD wallet architecture

### 1.1 Wallet creation flow

Default create path (Qt and RPC) forces descriptor wallets:

1. `CreateWalletActivity::createWallet` sets `WALLET_FLAG_DESCRIPTORS` when the descriptor checkbox is on (descriptor-only product path).
2. `CWallet` create/load path calls `SetupDescriptorScriptPubKeyMans()` when the wallet is not blank and private keys are enabled.
3. That function:
   - Generates a fresh secp256k1 seed key → BIP32 `CExtKey` master
   - Calls `SetupDescriptorScriptPubKeyMans(master_key)`
   - **Wipes the transient master/seed from memory** after SPKMs persist account/master material they need (`WALLET_HD_V1` comment in `wallet.cpp`)

Active managers are installed for every `(OutputType ∈ OUTPUT_TYPES, internal ∈ {false,true})`, where `OUTPUT_TYPES` includes `LEGACY`, `P2SH_SEGWIT`, `BECH32`, `BECH32M`.

### 1.2 Descriptor initialization (ORU vs RES)

`DescriptorScriptPubKeyMan::SetupDescriptorGeneration` (`scriptpubkeyman.cpp`):

| Output type | Descriptor built | HD / ranged? |
|-------------|------------------|--------------|
| LEGACY | `pkh(origin + xpub + /{0,1}/*)` | Yes |
| P2SH_SEGWIT | `sh(wpkh(.../*))` | Yes |
| **BECH32 (ORU)** | `wpkh([fp/84'/coin'/0']xpub/{0,1}/*)` | **Yes** |
| **BECH32M (RES)** | `res(<random 64-byte Ed25519 secret hex>)` | **No** — comment: “Generate a random Ed25519 key pair for non-HD wallet” |

RES path uses `GetStrongRandBytes` + `ed25519::SeedToKeypair`. It still derives a BIP32 purpose-86 account key and stores master/account secp keys for that SPKM, but the **active descriptor string is non-ranged `res(...)`**, so those secp keys do not make the RES SPKM “HD” under `IsHDEnabled()`.

Helper used elsewhere:

```cpp
// scriptpubkeyman.cpp
static bool IsSingleSlotNativeResDescriptor(const Descriptor& d)
{
    const std::optional<OutputType> ot = d.GetOutputType();
    return ot && *ot == OutputType::BECH32M && !d.IsRange();
}
```

This matches freeze language: RES is a **single-slot** witness-v1 program, not a derivation-ranged Taproot `tr()` wallet.

### 1.3 Seed, keypool, private keys

| Concern | Behavior |
|---------|----------|
| Seed | Transient at create; ORU continuity is via persisted descriptor keys + ranged `wpkh` expansion cache, not a single legacy `hdseedid` row |
| Keypool | ORU: `TopUp` expands ranged descriptors (`-keypool`). RES: single-slot cache (`range_end` treated as 1); `CanGetAddresses` has an explicit single-slot branch |
| Private keys present | Default wallet: yes (unless `disable_private_keys`) |
| Encrypted | Supported; locked wallet blocks signing and address privkey export |
| `disable_private_keys` | Watch-only / cold online observer; no spend keys stored |
| Blank wallet | No auto `SetupDescriptorScriptPubKeyMans`; import descriptors/keys later |

`getwalletinfo.hdseedid` is only emitted for **legacy** SPKM (`LegacyScriptPubKeyMan::GetHDChain()`). Descriptor wallets (RIGOR default) do **not** expose `hdseedid` even when ORU descriptors are ranged HD — same as Bitcoin Core descriptor wallets.

### 1.4 Why HD is reported disabled

Causal chain:

```
DescriptorScriptPubKeyMan::IsHDEnabled()
  → return m_wallet_descriptor.descriptor->IsRange();

CWallet::IsHDEnabled()
  → for each active SPKM:
        if (!spk_man->IsHDEnabled()) return false;  // ALL must be HD
  → true only if at least one active SPKM and none failed

Default wallet has active BECH32M SPKMs with non-ranged res()
  → those SPKMs IsHDEnabled() == false
  → CWallet::IsHDEnabled() == false

Qt RigorGUI::setHDStatus(privkeyDisabled, hdEnabled)
  → hdEnabled false + privkeys enabled
  → icon hd_disabled
  → tooltip "HD key generation is disabled"
```

This is **expected** under current RES single-slot design + Core’s all-active-SPKM HD gate (Bitcoin Core #22781 behavior inherited).

It is **not**:

- missing seed generation for ORU,
- `disable_private_keys` (that shows the eye / “Private key disabled” path instead),
- a failure of `CanGetAddresses` for ORU (ORU remains able to mint new addresses from the ranged keypool).

---

## 2. Bitcoin Core descriptor wallet comparison

| Topic | Bitcoin Core (v25-era descriptor wallet) | RIGOR (current) |
|-------|------------------------------------------|-----------------|
| Default wallet | Descriptor SQLite | Same; legacy create rejected |
| HD seed at create | Transient master → ranged descriptors | Same for secp output types |
| BECH32 | Ranged `wpkh(.../*)` | Same → **ORU** |
| BECH32M | Ranged Taproot `tr(.../*)` (BIP341) | **Replaced** by non-ranged native `res(...)` Ed25519; `tr()` hard-rejected |
| Wallet `IsHDEnabled` | True when **all** active SPKMs are ranged (typical Core wallet: all ranged → HD on) | False for every default dual-asset wallet because RES SPKMs are non-ranged |
| Keypool | Per ranged descriptor | ORU: ranged; RES: single-slot |
| `dumpprivkey` | Unsupported / redirected on descriptor wallets | Same; plus RIGOR address RPCs |
| Address private export | Generally discouraged; use `listdescriptors true` | **`dumporuprivkey` / `dumpresprivkey`** via descriptor signing provider |
| ORU key mgmt | N/A (single BTC asset) | secp256k1 WIF through `wpkh` provider |
| RES key mgmt | N/A | Ed25519 64-byte hex through `res()` / `res_v1_program_to_ed_secret` |
| Watch-only | `disable_private_keys` + public descriptors | Same |
| Backup unit | Descriptors (+ checksum) | Same; address export is convenience only |

**Key delta:** Core’s status bar assumes “wallet HD” ≈ “every active descriptor is ranged.” RIGOR intentionally keeps RES non-ranged, so the inherited indicator permanently shows HD disabled on healthy default wallets even though ORU HD generation works.

---

## 3. Qt HD indicator audit

| Item | Location |
|------|----------|
| Icon widget | `core/src/qt/bitcoingui.cpp` — `labelWalletHDStatusIcon` |
| Update entry | `RigorGUI::updateWalletStatus()` → `setHDStatus(privateKeysDisabled(), hdEnabled())` |
| Status API | `interfaces::Wallet::hdEnabled()` → `CWallet::IsHDEnabled()` |
| Icons | `:/icons/hd_enabled`, `:/icons/hd_disabled`, `:/icons/eye` (watch-only) |
| Tooltip (disabled) | `tr("HD key generation is <b>disabled</b>")` |

**Does the UI reflect reality?**

- Reflects **wallet-level** `IsHDEnabled()`: **yes**.
- Reflects **user-facing ORU capability** (“can I keep generating ORU addresses from an HD descriptor?”): **poorly** — that capability is on; the label says “disabled.”
- Reflects **RES**: RES was never HD; “disabled” is accurate for RES but not explained.

**Classification:** **A** (correct vs API) + **D** (missing dual-asset status / clearer copy). Not B/C.

---

## 4. Private key export compatibility

Path (unchanged by HD flag):

```
Address → DecodeDestination / asset from witness version
  → scriptPubKey → DescriptorScriptPubKeyMan
  → GetSigningProvider(script, include_private=true)
  → ORU: EncodeSecret (WIF) | RES: Ed25519 secret hex
```

Implementation: `ExportAddressPrivateKey` in `core/src/wallet/rpc/backup.cpp`; RPCs `dumporuprivkey` / `dumpresprivkey`. Spec: `docs/wallet/PRIVATE_KEY_EXPORT_SPEC.md`. Functional coverage: `core/test/functional/wallet_exportprivkey.py`.

| Scenario | Expected / audited behavior |
|----------|-----------------------------|
| Generated ORU | Export WIF from ranged `wpkh` provider — works |
| Generated RES | Export 128-hex from single-slot `res` — works |
| Imported keys (`importrigorprivkey` / descriptors) | Export if wallet holds private material for that script — works |
| Restored / blank + import | Works after descriptors with keys are imported |
| Encrypted + unlocked | Works |
| Encrypted + locked | `RPC_WALLET_UNLOCK_NEEDED` (“Wallet is locked”) |
| Watch-only (`disable_private_keys`) | Rejected (“Private key unavailable for watch-only descriptor”) |
| Wrong asset RPC | Rejected |
| Unknown address | “Address not found in wallet” |
| `dumpprivkey` on descriptor wallet | Still errors with redirect to descriptor/address export |

**Independence from HD indicator:** Export uses per-script signing providers. Wallet-level `IsHDEnabled()==false` does **not** block export of ORU or RES keys.

**Note on encrypt-wallet:** Encrypting a non-blank descriptor wallet regenerates descriptors via `SetupDescriptorScriptPubKeyMans()` (Core-inherited). Address export after encrypt applies to the **new** descriptor set; pre-encrypt addresses require prior backup (`listdescriptors true` / address dumps). This is orthogonal to the HD icon issue but relevant for recovery UX.

---

## 5. Root cause (conclusive)

> **Root cause:** Default RIGOR wallets install active non-ranged `res()` ScriptPubKeyMans for RES. `CWallet::IsHDEnabled()` requires every active SPKM to be ranged. RES fails that check, so the wallet reports HD disabled. Qt faithfully displays that boolean as “HD key generation disabled.”

ORU HD key generation remains enabled and used for receive/change. RES single-slot non-HD is intentional architecture (freeze / descriptor policy), not a create-wallet regression.

---

## 6. Recommended product direction

### 6.1 Should “HD” be enabled by default?

| Interpretation | Recommendation |
|----------------|----------------|
| ORU ranged HD descriptors at create | **Already yes** — keep |
| Wallet-level `IsHDEnabled()==true` for default wallets | **No**, not by making RES ranged |
| RES BIP32/Ed25519 HD derivation | **Do not enable by default** — conflicts with frozen single-slot RES semantics (`res()` non-ranged, reuse-normal, INV-A-4 style invariants) |
| Status-bar wording | **Change product copy / indicator model**, not consensus or RES issuance |

### 6.2 Preferred UX options (wallet-only, post-audit)

Ordered by least architecture risk:

1. **Clarify tooltip / status (recommended for official tree):** e.g. “ORU: HD key generation enabled · RES: single-key (non-HD)” when ORU SPKMs are ranged and RES is single-slot; keep eye icon for watch-only.
2. **Split indicator semantics in Qt only:** drive icon from ORU active SPKMs’ `IsHDEnabled()` (or “any ranged spendable ORU SPKM”), and document that the Core wallet flag remains all-SPKM.
3. **Avoid** changing `CWallet::IsHDEnabled()` globally without a careful audit — other call sites use it for range-end tracking / rescan filters (`FastWalletRescanFilter` only tracks ranged SPKMs today, which is already correct for RES).

**Implemented:** TASK-WALLET-HD-UX-CLARIFICATION-001 — see [`WALLET_KEY_MODEL_UX_SPEC.md`](./WALLET_KEY_MODEL_UX_SPEC.md) (`IsOruHDEnabled` / `IsResDedicatedKeyMode`, Qt Key Management Status).

### 6.3 Are code changes required before merging key export?

| Change | Required for export merge? |
|--------|----------------------------|
| Consensus / W·T / RES issuance / tx format | **No** (out of scope) |
| Fix “broken” HD seed init | **No** — not broken |
| Private key export | Already implemented; HD status does not block it |
| Qt HD copy / dual-asset status | **Recommended** for release UX clarity; **not** a correctness blocker for export RPCs |

---

## 7. Acceptance

| Criterion | Result |
|-----------|--------|
| Reason for “HD key generation disabled” identified | **PASS** — RES non-ranged active SPKMs ⇒ `CWallet::IsHDEnabled()==false` ⇒ Qt `hd_disabled` |
| Export vs HD coupling | Independent; export OK on default dual-asset wallets |
| Merge readiness (export into official tree) | HD icon is **not** evidence of a failed HD wallet; treat as known dual-asset UX debt |

---

## 8. Key source references

| Area | Path |
|------|------|
| Wallet HD gate | `core/src/wallet/wallet.cpp` — `CWallet::IsHDEnabled` |
| Descriptor HD / RES setup | `core/src/wallet/scriptpubkeyman.cpp` — `IsHDEnabled`, `SetupDescriptorGeneration`, `CanGetAddresses` |
| Create descriptors | `core/src/wallet/wallet.cpp` — `SetupDescriptorScriptPubKeyMans` |
| Qt indicator | `core/src/qt/bitcoingui.cpp` — `setHDStatus`, `updateWalletStatus` |
| Interface | `core/src/wallet/interfaces.cpp` — `hdEnabled()` |
| Address export | `core/src/wallet/rpc/backup.cpp` — `ExportAddressPrivateKey` |
| Prior descriptor audit | `docs/wallet/WALLET_DESCRIPTOR_ARCHITECTURE_AUDIT.md` |
| RES single-slot freeze | `docs/freeze/WALLET_SEMANTIC_FREEZE.md`, `docs/freeze/W1_1B_DESCRIPTOR_POLICY_AUDIT.md` |

---

## 9. Restrictions honored

- No consensus changes  
- No W/T or RES issuance rule changes  
- No transaction format changes  
- No merge into official tree performed  
- Audit findings and recommendations only  
