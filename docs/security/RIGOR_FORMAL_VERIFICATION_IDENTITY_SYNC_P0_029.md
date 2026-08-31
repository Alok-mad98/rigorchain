# RIGOR Formal Verification Identity Sync (P0-029)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-IDENTITY-SYNC-P0-029`  
Date: 2026-08-26  
Type: **Docs / operator identity only.** No Coq theorem, no sighash tag, no
`ScriptPubKey` / `AssetTypeFromScriptPubKey` / Ed25519 / serialization change.
Honesty flags stay **false**.

Ran **after** chain_id hardening, deletion of `libbitcoinconsensus`, util
vectors, wallet filename/RPC fail-closed, global-state hardening, and
mechanical source/config rename (P0-026 leftover).

Live spend path (unchanged):

```
UTXO → ScriptPubKey → AssetTypeFromScriptPubKey → Ownership predicate → Ed25519
```

---

## 1. Canonical repository

Product URLs operators and `AC_INIT` must use:

| Surface | Value |
|---------|--------|
| Bug reports | `https://github.com/rigorchain/rigorchain/issues` |
| Homepage | `https://rigorchain.org/` |
| Source | `https://github.com/rigorchain/rigorchain` |

`configure.ac` `AC_INIT` already used those URLs; regenerating `rigor-config.h`
makes `PACKAGE_BUGREPORT` / `PACKAGE_URL` match (they previously still said
`github.com/bitcoin/bitcoin` and `bitcoincore.org` because `config.status` had
not been re-run).

**Keep** HISTORICAL citations in Core-era comments that document a past Bitcoin
Core PR (for example `https://github.com/bitcoin/bitcoin/pull/1026` in
`chain.h`). Those are not the project homepage.

`formal_verification/` had **zero** product URLs pointing at
`github.com/bitcoin/bitcoin` or `bitcoincore.org`. Coq comments that say
“Bitcoin Hash256” / “FIPS 180-4 / Bitcoin CSHA256” are algorithm citations,
not a repository identity.

---

## 2. Wallet storage

| Claim | Live fact |
|-------|-----------|
| Engine | **SQLite** descriptor wallet |
| Default file | `rigor-wallet.sqlite` (`WALLET_DATABASE_FILENAME`) |
| Berkeley DB | **Not** a supported product path. No `migratewallet`. No BDB restore. |
| Typed RIGOR SQL schema | **Not** claimed. Payload is still the Core-era KV tables in SQLite. |

Do not teach `wallet.dat` or BDB salvage in FV or operator runbooks.

---

## 3. Consensus library (not Option B)

P0-028 Option B (`librigorconsensus` / `rigorconsensus.h`) was **not**
implemented. The C ABI was **deleted** because it verified in the wrong
`chain_id` domain.

FV docs must **not**:

- claim `libbitcoinconsensus` still exists
- claim `librigorconsensus` shipped as a rename of that ABI
- treat a shared-library C API as the consensus surface

Consensus verify is in-process: `VerifyScript` in `core/src/script/`. The
v0.1 Coq model (`formal_verification/rigor_v01/`) already describes that
path (classifier → ownership → Ed25519). No Coq file named the deleted C ABI.

---

## 4. Terminology (no drive-by renames)

Aligned and left as-is:

- ScriptPubKey → `AssetTypeFromScriptPubKey` → Ed25519
- Sighash domain tags and spend-model constructors **not** renamed
- `equivalence_claimed = false`
- `hash_kernel_shared_binary_with_cpp = false`
- `refinement_proved = false`

---

## 5. Files touched by this sync

| File | Change |
|------|--------|
| `formal_verification/README.md` | Product identity paragraph |
| `formal_verification/rigor_v01/README.md` | Repo / wallet / no C ABI |
| `docs/verification/FORMAL-VERIFICATION-STATUS.md` | Operator identity + link here |
| this document | Execution record |

No Coq `.v` edits. No golden-vector or honesty-flag edits.
