# RIGOR Script-as-Asset Consensus Root Freeze Audit (P0-019)

Task: `TASK-RIGOR-SCRIPT-ASSET-CONSENSUS-ROOT-FREEZE-AUDIT-P0-019`  
Predecessor: `TASK-RIGOR-SCRIPT-ASSET-OWNERSHIP-BOUNDARY-AUDIT-P0-018`  
Status: **Audit only. No code changed.**  
Question: freeze this as the unique v0.1 ownership model?

```
UTXO → scriptPubKey → AssetTypeFromScriptPubKey → ownership predicate
```

---

## 0. Freeze verdict

| Claim | Verdict |
|---|---|
| `CTxOut = (nValue, scriptPubKey)` with no stored `asset_type` | **Freeze. Already true.** |
| Asset class is `derive(scriptPubKey)`, never `store(asset_type)` | **Freeze. Already true in consensus.** |
| Unknown / malformed → `ASSET_INVALID`, not a fallback to ORU/RES/NONE | **Freeze as the C++ rule.** Explorer and Coq still violate it. |
| Address type, witness version, wallet metadata, explorer, descriptor are not classifiers | **Not yet true.** Wallet and explorer still classify by version / a second table. |
| Create `scriptPubKey` = spend ownership predicate | **Broken for RES mint under Option A + documented `HASH160(program)`.** |
| RES v1 program encoding | **Do not freeze Option A.** Freeze **Option B: `program = pk32`.** |

**Do not declare the consensus root frozen until Option B is implemented and the second classifiers are deleted or reduced to thin wrappers.** There is no compatibility burden. Mainnet has not launched.

---

## 1. Current implementation state

### 1.1 Consensus (the root — sound)

| Surface | State |
|---|---|
| `CTxOut` | `{nValue, scriptPubKey}` only. Serialization is those two fields. |
| `Coin` | No asset tag. Comment: “native asset is implied by scriptPubKey”. |
| `ClassifyRigorTxout` | Single matcher in `script/rigor_classify.cpp`. |
| `AssetTypeFromScriptPubKey` | Pure function of script bytes. Default `ASSET_INVALID`. |
| `Solver` / `IsResScript` | One-line wrappers of the matcher. |
| `IsAllowedScript` | Closed set: v0/20, native v1/32, `OP_RES_MINT`, `OP_CONVERGE`. |
| `CheckTransaction` | `ASSET_INVALID` → `bad-rigor-script-type`. Body cannot create mint. |
| `CheckTxInputs` | Re-derives class from prevout and output scripts. Spend of `NONE`/`INVALID` rejected. Spend of converge rejected. |
| `VerifyScript` | Three spend predicates: P2WPKH, native RES key-path, mint. No Taproot, no RES2, no P2WSH. |

Live mapping:

| scriptPubKey | AssetType | Spend predicate |
|---|---|---|
| `OP_0 <20>` = `HASH160(pk32)` | ORU | witness `<sig><pk>`; `HASH160(pk)==program` |
| `OP_1 <32>` = `SHA256(pk32)` **(Option A, shipping)** | RES | witness `<sig><pk>`; `SHA256(pk)==program` |
| `<maturity> <20> OP_RES_MINT`, 20 = `HASH160(pk32)` | RES | scriptSig `<sig><pk>`; `HASH160(pk)==20`; height |
| `<20> OP_CONVERGE`, 20 = `HASH160(pk32)` | ORU burn | none (`AddCoins` skip) |
| push-only `OP_RETURN` | NONE | unspendable, `nValue==0` |
| else | INVALID | not creatable |

There is **no** consensus fallback that turns unknown scripts into ORU.

### 1.2 Ownership equality (the freeze invariant)

Required:

```
bytes of the created scriptPubKey's commitment
        =
bytes the spender must satisfy
```

| Template | Create commits | Spend checks | Equal today? |
|---|---|---|---|
| ORU v0 | `HASH160(pk)` | `HASH160(pk)` | **Yes** |
| RES v1 (A) | `SHA256(pk)` | `SHA256(pk)` | **Yes, inside bitcoind** |
| RES mint | `HASH160(pk)` | `HASH160(pk)` | **Yes, inside bitcoind** |
| Converge → mint | create: `HASH160(pk)`; headers say `HASH160(v1 program)` | mint spend: `HASH160(pk)` | **No, if the header is followed** |

`HASH160(SHA256(pk)) ≠ HASH160(pk)`. Option A makes those two 20-byte values different. That is the mint-succeeds-but-cannot-spend hole.

### 1.3 Non-consensus paths (second sources)

| Path | Calls `AssetTypeFromScriptPubKey`? | Independent rule? |
|---|---|---|
| validation / tx_check / tx_verify / GetValueOut | Yes | No |
| wallet spend / receive / listunspent / TxToUniv | Mostly yes | **`WitnessRFCBindingOk` maps witness version → asset** |
| RPC `asset` field (`AssetTypeToString`) | Yes (W1.1C) | Integrators told not to infer from version; wallet still does |
| descriptor `wpkh()` / `res()` | Emits a script, then consensus classifies | `res()` **implements Option A** (`SHA256(pk)`). Not a class enum, but it *is* a second encoding of RES identity |
| address codec | Decode → destination → script | Version/checksum binding only; **`ValidateAddressAssetBinding` maps version → asset** |
| explorer `classifyScriptPubKey` | **No** | Own table: v0/32 → ORU, RES2 → RES, unknown → NONE |
| explorer indexer (`importer.ts`) | Uses explorer classifier | Persists `asset_kind` from that table |
| functional `asset_type_from_scriptpubkey` | Intended copy | Looser mint match; can label non-canonical mint as RES |
| Coq `asset_kind_of_script` | Shape model | empty → ORU, CVG2 → ORU, unknown → None, still `AssetCVG` |

---

## 2. Findings

### F-1 — RES Option A makes create-identity ≠ documented mint-identity (High, freeze-blocking)

Shipping: v1 program = `SHA256(pk)`. Mint/converge 20-byte = `HASH160(pk)`.

Headers (`interpreter.h`, `res_owner.h`, GBT `res_payout_hash`) say mint/converge 20-byte = `HASH160(RES program)` = `HASH160(SHA256(pk))`.

If a miner hashes the Bech32m program (the natural reading of “RES program”):

1. `CheckResRewardOwner` succeeds (declaration hash equals mint hash).
2. `VerifyResMintSpend` fails for the winner’s `pk` (`HASH160(pk)` ≠ `HASH160(program)`).
3. Result: **mint succeeds, owner cannot spend.**

This is exactly the invariant this freeze forbids.

### F-2 — Wallet classifies by witness version (High)

`WitnessRFCBindingOk` / `ValidateAddressAssetBinding`: `ver==0 → ORU`, `ver==1 → RES`. That is a second asset system. Forbidden by this task.

### F-3 — Explorer / indexer is a second registry with fallbacks (High)

Unknown → `NONE` (fallback). v0/32 → `ORU` (false asset). RES2 → `RES` (false asset). Indexed `asset_kind` is stored in Postgres from this table — `store(asset_type)` at the indexer, derived from the wrong function.

### F-4 — Coq / tests are not fail-closed (Medium)

Coq empty/CVG2 → ORU. Python helper fail-opens mint. Test name `cvg2_is_oru_not_res` now wraps `OP_CONVERGE` (correct class, wrong name). Freeze text that cites Coq as the classifier would freeze the wrong function.

### F-5 — Dead identifiers invite a second path (Medium)

`IsORBMintTx` (unused, ORU-funded RES). `IsV1Program` (unused, any v1/32 including CVG2/RES2). `PUBKEYHASH` inside `ClassifyRigorTxout` (signing-only; one `case` away from becoming ORU).

### F-6 — Freeze declaration currently locks Option A (High for this task)

`MAINNET_PROTOCOL_FREEZE_DECLARATION.md` already writes “Witness v1 RES program = SHA256(pk)”. That sentence must be **replaced**, not preserved. No compatibility argument applies.

---

## 3. Risk levels

| ID | Risk | If frozen as-is |
|---|---|---|
| F-1 | **High** | Independent mint implementations produce unspendable RES. Irreversible after first mainnet blocks. |
| F-2 | **High** | Wallet remains a second classifier; future RPC/UI will keep teaching version = asset. |
| F-3 | **High** | Indexer stores a wrong `asset_kind`. Operators treat explorer as truth. |
| F-4 | **Medium** | Formal claims and tests ratify fail-open / empty-as-ORU. |
| F-5 | **Medium** | One-line consensus expansion. |
| F-6 | **High** | Constitutional doc freezes the encoding that *causes* F-1. |

No Critical *in-tree bitcoind* split: `bitcoind` create-convergence recovers `pk` and hashes that. The Critical-class failure is **post-freeze multi-implementation**, not a current node bug.

There is **no** consensus `store(asset_type)` and **no** C++ fallback from `INVALID` to ORU.

---

## 4. Recommended final model (freeze this)

### 4.1 Constitutional rules

1. `CTxOut = (nValue, scriptPubKey)`. Adding `asset_type` is a Level-5 violation.
2. `AssetType = AssetTypeFromScriptPubKey(scriptPubKey)`. The only assigning function.
3. Unrecognized / malformed / retired (CVG2, RES2, v0/32, P2PKH, P2SH, empty) = `ASSET_INVALID`. Not ORU. Not NONE. Consensus-invalid to create. Not a fallback.
4. Address, Bech32 version, descriptor, wallet metadata, explorer labels are **views** of a script. They must not map version→asset. If they report an asset, they call the same function or a byte-identical table that includes `INVALID`.
5. Ownership: the commitment bytes in the created `scriptPubKey` are the bytes the spend path checks. No documented alias that hashes a different object.

### 4.2 Frozen templates

| Asset | scriptPubKey | Ownership predicate |
|---|---|---|
| ORU | `OP_0 \|\| HASH160(pk32)` | reveal `pk32`; `HASH160(pk)==program`; Ed25519 |
| RES (transfer) | `OP_1 \|\| pk32` **(Option B)** | Ed25519 under `program` (the program **is** the key) |
| RES (mint) | `<maturity> \|\| HASH160(pk32) \|\| OP_RES_MINT` | reveal `pk32`; `HASH160(pk)==20`; height; coinbase-only create |
| ORU burn / draw | `HASH160(pk32) \|\| OP_CONVERGE` | unspendable; winner mint 20-byte **equals this 20-byte** |
| data | push-only `OP_RETURN` | `ASSET_NONE`; not spendable |

Under Option B, `HASH160(RES program) = HASH160(pk32)`. Converge, mint, ORU, and the documented name collapse to **one** 20-byte identity. Create-converge = mint script = mint spend. The F-1 hole disappears by identity, not by a warning comment.

### 4.3 Why Option B, not A (compatibility ignored)

| Criterion | A: `program = SHA256(pk)` | B: `program = pk32` |
|---|---|---|
| Minimal surface | Extra SHA256 on create and spend | Equality |
| Script-as-asset | Identity is a hash of the key | Identity **is** the key |
| Formal verification | `SHA256(pk)==program ∧ Verify(sig,pk,m)` | `Verify(sig, program, m)` |
| Ownership clarity | Three commitments: HASH160(pk), SHA256(pk), and documented HASH160(SHA256(pk)) | Two: `pk32` (v1) and `HASH160(pk)` (20-byte slots) |
| Create = spend | True in node; false if “HASH160(program)” is used for mint | True in node **and** in the natural API |
| Long-term safety | Invites a second hiding hash / program-vs-pk split | Changing encoding requires a new witness version |
| Privacy | Unspent RES hides pk | Pubkey in UTXO set |

Privacy is the only score for A. It is not a freeze criterion in this task. Ed25519 keys are already 32 bytes; v1 programs are 32 bytes. **B is the encoding that falls out of the types.**

Do not “fix” A by minting to `HASH160(SHA256(pk))`. That keeps a third hash and desynchronizes mint from ORU’s `HASH160(pk)`.

### 4.4 Descriptor / address after B

- `res()` emits `OP_1 || pk32`. Public form already prints pk32 — after B it matches the chain program.
- Bech32m v1 data **is** the pubkey.
- Watch-only import of 32-byte hex is the program, not a preimage search.

---

## 5. Modification plan (do not implement in this task)

Order is deletion and encoding change. No new asset registry.

### Step 1 — Consensus encoding (Option B)

- `ed25519::ResWitnessProgram`: copy `pk32` → `out32` (or delete the helper and memcmp program to pk).
- `VerifyWitnessProgram` v1: require `stack[1] == program` (or drop the revealed pk and verify using `program` as the key; prefer **program is the key**, witness `<sig>` only if that stays two-item compatible — keep `<sig><pk>` with `pk == program` to avoid a second stack shape).
- `CPubKey::GetResWitnessProgram` / `DeriveResWitnessProgramFromPubKey`: return the 32-byte payload.
- `IsRigorResNativeWitnessProgram`: still reject CVG2/RES2 prefixes (a raw pk that happens to start with those ASCII bytes is astronomically rare; still reject to keep retired tags uncreatable).
- Negative tests: `SHA256(pk)` as program is `ASSET_INVALID` or unspendable mismatch; mint paying `HASH160(SHA256(pk))` fails spend for `pk`.

Consensus fork relative to current tree: **yes**. Relative to launched mainnet: **n/a**.

### Step 2 — Create = spend for mint/converge

- Rename `Get*ForProgram` → `Get*ForPubKey`.
- Every comment and GBT string: 20-byte = `HASH160(pk32)` = `HASH160(v1 program)` **because program is pk32**.
- `CheckResRewardOwner` unchanged (byte equality of the 20-byte field).
- Wallet create-convergence: `GetConvergeScript(Hash160(pk32))` can equally be `Hash160(v1_program)` after B — add a test that they are equal, and a test that they are unequal under leftover SHA256 programs.

### Step 3 — Delete second classifiers

- Delete `WitnessRFCBindingOk`, `ValidateAddressAssetBinding`. Send: `AssetTypeFromScriptPubKey(spk) == send_asset` only.
- Explorer: copy C++ table; add `INVALID`; never persist unknown as `NONE`; never label v0/32 or RES2 as an asset. Indexer `asset_kind` is derived at read time from script, or stored only after the C++-identical function.
- Python helper: canonical mint only.
- Coq: `{ORU, RES, NONE, INVALID}`; empty/CVG2/RES2/unknown → INVALID, not ORU/None. Drop `AssetCVG` from any lemma claimed to track C++.

### Step 4 — Delete footguns

- `IsORBMintTx`, `IsV1Program`.
- Rename `IsORBClaimTx`.
- Move implied P2PKH template out of `ClassifyRigorTxout` into a signing-only helper if it can be done without changing spend.

### Step 5 — Freeze text

Replace the TASK-011 line in `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`:

> Witness v1 RES program = **the 32-byte Ed25519 public key**. Spend is Ed25519 over that program.  
> OP_CONVERGE / OP_RES_MINT 20-byte = `HASH160(pk32)`, which equals `HASH160(v1 program)`.  
> Asset class is never stored. Unknown scripts are `ASSET_INVALID`.

Do not freeze Option A “for now”.

---

## 6. What this audit does not change

- Dual-pool conservation, mint amount `W(h)`, Convergence draw, sighash domain (P0-018 signing audit).
- Ed25519-only CHECKSIG, hardened HD.
- The existence of two *templates* for RES (v1 transfer vs mint). That is one asset, two locks, both `derive(scriptPubKey)`. After B they share one key identity.

---

## 7. Suitability for v0.1 freeze

| Object | Freeze now? |
|---|---|
| No `CTxOut.asset_type` | **Yes — already** |
| `AssetTypeFromScriptPubKey` as unique consensus assigner | **Yes — already, pending wrapper deletion** |
| Fail-closed `INVALID` | **Yes in C++; not until explorer/Coq match** |
| Option A (`SHA256(pk)`) | **No** |
| Option B (`pk32`) | **Yes — this is the freeze choice** |
| Unique classification across wallet/explorer/tests/Coq | **No, until Steps 3–4** |

**Recommended freeze sentence (after Steps 1–5):**

The unique RIGOR v0.1 ownership model is UTXO → scriptPubKey → `AssetTypeFromScriptPubKey` → one Ed25519 predicate. RES witness v1 program is the public key. There is no stored asset type, no version-based class, and no mint that a documented owner cannot spend.
