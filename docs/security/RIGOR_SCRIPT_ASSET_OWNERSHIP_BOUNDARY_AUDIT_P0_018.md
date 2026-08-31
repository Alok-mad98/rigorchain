# RIGOR Script-as-Asset Ownership Boundary Audit (P0-018)

Task: `TASK-RIGOR-SCRIPT-ASSET-OWNERSHIP-BOUNDARY-AUDIT-P0-018`  
Status: **Audit complete. Consensus classification is freeze-capable. Freeze is blocked on duplicate classifiers and a documented-but-false owner-hash contract.**  
Scope: asset identity and ownership as a function of `scriptPubKey` only. Address formats, descriptors, UI, explorer, Coq, and test helpers are in scope as *alternative identification paths*.

Companion canvas: open beside chat as `RIGOR-script-asset-ownership-boundary-audit.canvas.tsx`.

---

## 0. Verdict

**Do not redesign the consensus asset model.** The live chain already implements Script-as-Asset:

```
scriptPubKey
    → ClassifyRigorTxout          (single matcher)
    → AssetTypeFromScriptPubKey   (ORU | RES | NONE | INVALID)
    → ownership predicate         (one Ed25519 key commitment per spendable template)
```

`CTxOut` is `(nValue, scriptPubKey)` with no parallel asset field. `Coin` disk layout stores no asset tag. Conservation, mint, and Convergence all re-derive class from the same function.

That root is suitable for RIGOR v0.1 freeze **after** the items in §6 are applied. They are deletions, comment/API honesty, and wrapper collapse — not a new asset registry.

| Freeze principle | Can be frozen | Blocking finding |
|---|---|---|
| Asset identity originates from `scriptPubKey` | **Yes** (consensus) | — |
| Exactly one authoritative classifier | **No** | H-2, H-3, M-1 |
| Fail-closed unknown scripts | **Yes** in C++; **No** in explorer / Coq | H-3, M-1 |
| One ownership predicate per spendable output | **Yes** | — |
| Owner hash contract is unique and named correctly | **No** | **H-1** |
| RES program encoding (Option A vs B) | **Yes, with an explicit freeze sentence** | A-1 (non-blocking) |

**RIGOR v0.1 consensus freeze cannot be finalized as-is**, because a future implementer following `interpreter.h` / `res_owner.h` / GBT RPC comments will commit `HASH160(SHA256(pk))` while the wallet and mint spend path commit `HASH160(pk)`. That is a second ownership path created by documentation, and it makes a winning mint unspendable.

---

## 1. Current classification (consensus)

Authoritative implementation: `core/src/script/rigor_classify.cpp`. Declaration lives in `primitives/transaction.h` so every translation unit that sees `CTxOut` can call it; the body is in libbitcoin_consensus.

```15:82:core/src/script/rigor_classify.cpp
TxoutType ClassifyRigorTxout(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    // witness v0/20 → WITNESS_V0_KEYHASH
    // witness v1/32 native (not CVG2, not RES2) → WITNESS_V1_RES
    // canonical OP_RES_MINT → RES_MINT
    // canonical OP_CONVERGE → CONVERGE
    // push-only OP_RETURN → NULL_DATA
    // 25-byte implied P2WPKH template → PUBKEYHASH (signing only; not a chain output)
    // else NONSTANDARD
}

AssetType AssetTypeFromScriptPubKey(const CScript& scriptPubKey)
{
    switch (ClassifyRigorTxout(...)) {
    case WITNESS_V0_KEYHASH:
    case CONVERGE:     return ASSET_ORU;
    case WITNESS_V1_RES:
    case RES_MINT:     return ASSET_RES;
    case NULL_DATA:    return ASSET_NONE;
    default:           return ASSET_INVALID;
    }
}
```

`Solver` is a one-line wrapper (`standard.cpp`). `IsResScript` is a one-line wrapper (v1 native or mint). `IsAllowedScript` admits `{v0/20, v1 native, mint, converge}` and rejects everything else, including OP_RETURN (which is admitted only as `nValue==0` unspendable via `IsUnspendable`).

Unrecognized scripts are `ASSET_INVALID`, never ORU, never silently `NONE`. `CheckTransaction` rejects them with `bad-rigor-script-type`. `CheckTxInputs` rejects spending `NONE`/`INVALID` with `bad-txns-spend-non-asset-input`.

This is the correct fail-closed root.

### 1.1 Spendable ownership predicates

| Template | `scriptPubKey` commitment | Spend | SigVersion |
|---|---|---|---|
| ORU P2WPKH | `OP_0 <20>` = `HASH160(pk32)` | witness `<sig><pk32>` | `WITNESS_V0` |
| RES native v1 | `OP_1 <32>` = `SHA256(pk32)` | witness `<sig><pk32>` | `WITNESS_V1_RES` |
| RES mint | `<maturity> <20> OP_RES_MINT`, 20 = `HASH160(pk32)` | scriptSig `<sig><pk32>` | `RES_MINT` |
| OP_CONVERGE | `<20> OP_CONVERGE`, 20 = `HASH160(pk32)` | **none** — `AddCoins` skips; `bad-txns-spend-convergence` | — |
| OP_RETURN | push-only | unspendable; `ASSET_NONE`; `nValue` must be 0 | — |

There is no Taproot, no RES2 spend, no P2WSH, no P2PKH/P2SH chain output, no annex, no script path.

### 1.2 What OP_CONVERGE is (and is not)

OP_CONVERGE is an ORU-domain **burn plus owner declaration**. The UTXO is destroyed at connect (`AddCoins` skip). The 20-byte payload is copied into the coinbase `OP_RES_MINT` of the winning block (`CheckResRewardOwner`). Ownership of the *future mint* is therefore still a function of a `scriptPubKey` — the converge output's, then the mint output's — not of an address, descriptor, or extra table.

It is not a spendable ORU coin. Classifying it `ASSET_ORU` is correct for pooling (`GetValueOut(ASSET_ORU)` includes the burn so it is not treated as fee).

---

## 2. Answers to the five questions

### Q1. Can any transaction create an asset whose ownership is not completely determined by `scriptPubKey`?

**No, at consensus.**

- Asset class is `AssetTypeFromScriptPubKey(scriptPubKey)` with no stored tag.
- Spend of ORU / native RES / mint requires the Ed25519 public key that is the preimage of the commitment **inside that same script**.
- Body transactions cannot create `OP_RES_MINT` (`bad-txns-res-mint-noncoinbase`). New RES only appears in coinbase, and that mint script's 20-byte hash must equal the winning converge script's 20-byte hash.
- Address / descriptor / UI never enter `CheckTransaction` / `CheckTxInputs` / `VerifyScript`.

Caveat (H-1): a miner or wallet that follows the **wrong documented contract** (`HASH160(RES program)` = `HASH160(SHA256(pk))`) can produce a mint that classifies as RES and pays the “declared” hash, yet cannot be spent, because `VerifyResMintSpend` checks `HASH160(pk)`. Ownership is still determined by the mint `scriptPubKey`; the declaration and the spend predicate have silently diverged.

### Q2. Can the same `scriptPubKey` bytes represent different assets under different contexts?

**No, at consensus.** `AssetTypeFromScriptPubKey` is a pure function of bytes. Height, tx type, and network do not change the class.

Context changes *existence*, not class: the same OP_CONVERGE bytes are always `ASSET_ORU`, but they never occupy the UTXO set.

### Q3. Can wallet address encoding introduce a second classification layer?

**Not on the wire.** Decode produces a destination; `GetScriptForDestination` produces a script; consensus classifies the script. v0 must be Bech32 / 20 bytes; v1 must be Bech32m / 32-byte native RES (`IsRigorResNativeWitnessProgram`). P2PKH / P2SH / v0-P2WSH / CVG2 / RES2 encode to empty or fail decode. `ExtractDestination` refuses mint and converge (no address for those templates).

**Yes, as a client-side duplicate.** `WitnessRFCBindingOk` maps witness *version* to expected asset without calling `AssetTypeFromScriptPubKey`. `ValidateAddressAssetBinding` is the same mapping on `CTxDestination`. RPC comments tell integrators not to infer asset from witness version (`core_io.h` W1.1C), while the wallet still does exactly that as a pre-check. See H-2.

### Q4. Can future developers accidentally create another ownership path?

**Yes, and the tree currently invites it.**

1. Headers name `GetConvergeScriptForProgram(program32)` and state the payload is `HASH160(RES program)`. The body hashes the 32-byte argument as a **pubkey**. Wallet `spend.cpp` already contains a comment warning not to hash the SHA256 program.
2. `ClassifyRigorTxout` still recognizes the 25-byte P2PKH template (`PUBKEYHASH`) so signing can solve the implied v0 script. A one-line `case PUBKEYHASH: return ASSET_ORU` would re-open a rejected lock as an asset.
3. Retired prefixes CVG2 / RES2 are excluded only inside `IsRigorResNativeWitnessProgram`. Explorer currently labels RES2 as live RES (H-3).
4. Dead `IsORBMintTx` still encodes “ORU-funded RES creation”, which consensus now rejects as `mixed-asset-output`.
5. Coq `asset_kind_of_script` still maps empty scripts to ORU, CVG2 to ORU, and unknown to None — the opposite of fail-closed.

### Q5. Is `AssetTypeFromScriptPubKey` suitable as a permanent consensus root?

**Yes**, provided it remains the *only* function that assigns `AssetType`, wrappers stay one-line, and unknown → `ASSET_INVALID` forever. Do not add an asset field to `CTxOut`. Do not infer class from Bech32 version in consensus.

It is not yet safe to freeze the *surrounding* ecosystem as if that uniqueness already held.

---

## 3. Security findings

Severity: **C** consensus-fatal / freeze-blocking, **H** high (wrong owner or second registry), **M** medium, **L** low, **A** architectural (Option A/B).

### H-1 — Documented owner hash is not the implemented owner hash (freeze-blocking)

**Where**

- `core/src/script/interpreter.h`: “`<20-byte HASH160(RES program)> OP_CONVERGE`”; `GetConvergeScriptForProgram(Span program32)`; `GetResMintScriptForProgram` “HASH160 of the program”.
- `core/src/consensus/res_owner.h`: “HASH160 of the 32-byte witness v1 RES program”.
- `core/src/script/script.h`: “Owner is the HASH160 in the script — the same digest OP_RES_MINT pays” plus “HASH160(RES program)”.
- `core/src/rpc/mining.cpp`: GBT field `res_payout_hash` = “HASH160 of the RES program”.

**What the code actually does**

```94:128:core/src/script/interpreter.cpp
CScript GetResMintScriptForProgram(int64_t maturity_height, Span<const unsigned char> pubkey32)
{
    // HASH160 of the 32-byte Ed25519 pubkey (not SHA256(pk) witness program).
    return GetResMintScript(maturity_height, Hash160(pubkey32));
}

CScript GetConvergeScriptForProgram(Span<const unsigned char> pubkey32)
{
    // HASH160 of the 32-byte Ed25519 pubkey, matching OP_RES_MINT.
    return GetConvergeScript(Hash160(pubkey32));
}
```

Wallet create-convergence recovers the 32-byte pubkey from the `res()` descriptor because `WitnessV1Res` is `SHA256(pk)` and hashing that program would not match mint spend:

```1771:1799:core/src/wallet/spend.cpp
// OP_CONVERGE / OP_RES_MINT bind HASH160(ed25519_pk), not HASH160(SHA256(pk)).
// WitnessV1Res is the SHA256 program; recover the 32-byte pubkey from the wallet.
...
const CScript burn = GetConvergeScript(Hash160(pk32));
```

Mint spend checks `HASH160(pk)` against the 20 bytes in the mint script (`VerifyResMintSpend`). Native RES spend checks `SHA256(pk)` against the v1 program.

**Impact.** Two 20-byte identities exist for one key:

| Commitment | Value |
|---|---|
| Implemented (wallet + mint verify) | `HASH160(pk32)` |
| Documented / API name | `HASH160(SHA256(pk32))` = `HASH160(v1 program)` |

If a pool, miner, or second client follows the header, `CheckResRewardOwner` still passes (both sides hash the same 32-byte blob they were given). The blob they were given is the **program**, so the mint script contains `HASH160(program)`. The owner then cannot spend: revealed `pk` hashes to a different 20 bytes. Funds minted to the “winner” are stuck.

This is not a current consensus split inside `bitcoind` (createconvergence and `Get*ForProgram` both hash whatever 32 bytes they are passed; the wallet passes `pk`). It is the highest-probability way to mint a second, unspendable ownership path after freeze, when independent implementations appear.

**Fix (deletion/honesty, not a new mechanism).** Rename the helpers to `*ForPubKey`. Change every comment and GBT string to `HASH160(32-byte Ed25519 pubkey)`. Add a unit test that `Hash160(ResWitnessProgram(pk)) != Hash160(pk)` and that a mint paying the former fails `VerifyResMintSpend` for that `pk`.

### H-2 — Wallet maps witness version to asset (second classifier)

`WitnessRFCBindingOk` (`wallet/asset_domain.cpp`) and `ValidateAddressAssetBinding` (`standard.cpp`) treat `ver==0 → ORU`, `ver==1 → RES`. They return `true` for any non-witness script (mint, converge, garbage) and only then does `WalletStrictSendDestinationOk` consult `AssetTypeFromScriptPubKey`.

`rpcspend.cpp` can fail a send on RFC binding alone. That is a second authority: “witness version is the asset.”

W1.1C already forbids this for RPC consumers (`AssetTypeToString` / `listunspent`). The wallet must obey the same rule.

**Fix.** Delete `WitnessRFCBindingOk` and `ValidateAddressAssetBinding`. The only send check is `AssetTypeFromScriptPubKey(spk) == send_asset` (and `IsAllowedScript` / dust as today). Address codec stays a bijection of `(version, program)` — it must not assign `AssetType`.

### H-3 — Explorer classifier fail-opens (second registry)

`explorer/src/script.ts` `classifyScriptPubKey`:

| Script | Consensus | Explorer |
|---|---|---|
| v0 / 32-byte (P2WSH) | `ASSET_INVALID`, not creatable | **ORU**, encodes a Bech32 address |
| v1 / RES2 prefix | `ASSET_INVALID` | **RES** (only CVG2 is special-cased) |
| unknown / malformed | `ASSET_INVALID` | **NONE** |
| OP_CONVERGE | ORU burn | ORU burn (correct) |
| v0 / 20 | ORU | ORU (correct) |

Comment in that file still says “v0 witness (P2WPKH/P2WSH) => ORU”. That is a second asset classification system, which this audit forbids even outside consensus: explorers and wallets become the place operators learn “what asset is this,” and they will disagree with the node.

**Fix.** Port the C++ table one-for-one. Unknown and retired prefixes are not a displayable asset. Do not encode addresses for scripts `IsAllowedScript` would reject.

### M-1 — Coq / spec drift claims C++ alignment it does not have

`formal_verification/coq/ReplayReorgCore/AssetFromScript.v`:

- empty script → `AssetORU` (C++: `INVALID`)
- CVG2 → `AssetORU` (C++: `INVALID`)
- unknown collapsed to `AssetNone` (C++: `INVALID` ≠ `NONE`)
- `AssetCVG` / `OP_CVG_CLAIM` still in the kind enum (C++ opcode 0xbc burned)

`ScriptMintWitness.v` already documents the gap honestly. `AssetFromScript.v` lemmas still look like a refinement of `AssetTypeFromScriptPubKey`. Freeze documents must not treat Coq pooling kinds as the consensus classifier.

**Fix.** Either change the Coq classifier to four kinds `{ORU, RES, NONE, INVALID}` matching C++, or stop claiming shape-level equality in freeze text. Prefer the former: empty and CVG2 must not pool as ORU.

### M-2 — Python functional helper is a looser mint matcher

`core/test/functional/test_framework/script_util.py` `asset_type_from_scriptpubkey` treats any script whose last byte is `OP_RES_MINT` and whose byte at `-22` is `20` as RES. C++ `ParseResMintScript` requires the exact encoding `GetResMintScript` emits (minimal `CScriptNum`, `OP_PUSHBYTES_20`, no trailing opcodes).

Tests can classify a consensus-invalid mint as RES.

**Fix.** Mirror `ParseResMintScript` (round-trip equality with the constructor), or call into a single testdata table generated from C++.

### M-3 — Dead and misleading alternate identifiers

| Symbol | Problem |
|---|---|
| `IsORBMintTx` | Defined, unused. Encodes ORU-input + RES-output “mint”, which `CheckTxInputs` now rejects. |
| `IsORBClaimTx` | Used; means “semantic domain is RES”. Name is a third vocabulary (ORB). |
| `IsV1Program` | Defined, unused. True for *any* v1/32, including CVG2/RES2. |
| `CheckTxOutAssetWitnessBinding` | Same switch as `IsAllowedScript` plus `NULL_DATA`. Two names for one closed set. |
| `PUBKEYHASH` arm in `ClassifyRigorTxout` | Required for implied-P2WPKH signing; classified `INVALID` on chain. Easy to “promote” to ORU. |

**Fix.** Delete `IsORBMintTx` and `IsV1Program`. Rename `IsORBClaimTx` to a domain helper that calls `GetTransactionInputAsset`. Keep the implied P2PKH template in a signing-only function, not in the consensus matcher if it can be split without breaking `ProduceSignature`.

### M-4 — Stale security docs still teach a second model

`docs/security/RIGOR_SCRIPT_SECURITY_BOUNDARY.md` still lists P2WSH as a live surface and CVG2 as ORU-domain. `RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md` still mentions a “parse default ORU” leftover. Freeze readers will implement those docs.

**Fix.** Rewrite those pages to the table in §1.1. Do not leave “historical RFC-014” as if it were still classified ORU.

### L-1 — Policy is correctly a subset, but `AreInputsStandard` omits CONVERGE by construction

`AreInputsStandard` allows only v0/20, v1 native, and mint as *prevouts*. Converge cannot appear as a prevout because it never enters the UTXO set. Policy is not a second asset system; it is a subset of the classifier. No change required beyond keeping it a wrapper of `ClassifyRigorTxout`.

### A-1 — RES program: Option A (SHA256(pk)) vs Option B (pk bytes)

See §5. Not a demonstrated exploit. Freeze must pick one sentence and stop.

---

## 4. Consensus risks (if freeze proceeds without §6)

1. **Unspendable minted RES** after a second implementation follows `HASH160(RES program)` (H-1). No hard fork can unstick those outputs if the mint script is already in a block.
2. **Explorer / indexer disagreement** treated as “the asset” by operators (H-3). Not a consensus split, but it is exactly the second classification system the constitution forbids.
3. **Coq pooling inflation/deflation lemmas** prove facts about a classifier that admits empty scripts as ORU (M-1). Using those lemmas as freeze evidence would ratify the wrong function.
4. **Footgun promotion** of `PUBKEYHASH` or `IsV1Program` into `IsAllowedScript` (M-3). One-line consensus expansion.

There is **no** current path by which a well-formed `bitcoind` transaction creates ORU or RES whose spend key is not the preimage of its own `scriptPubKey`.

---

## 5. Special review: RES Option A vs Option B

Do not assume the current encoding is privileged. There is no launched mainnet.

### 5.1 What each option is

| | Option A (current) | Option B |
|---|---|---|
| v1 `scriptPubKey` | `OP_1 \|\| SHA256(pk32)` | `OP_1 \|\| pk32` |
| Spend | reveal `pk`, check hash, verify sig | verify sig under the program bytes |
| Address | Bech32m of the 32-byte program | Bech32m of the pubkey |

### 5.2 Score (freeze criteria)

| Criterion | Option A | Option B | Winner |
|---|---|---|---|
| Script-as-asset | Class still from script. Identity is a *hash* of the key. | Class from script. Identity **is** the key. Matches “one ScriptPubKey → one Ed25519 public key commitment” literally. | **B** |
| Privacy | Unspent RES hides `pk` (P2WPKH-like). | Pubkey is in the UTXO set. | **A** |
| Simplicity | Extra SHA256 on create and spend; third commitment function in the protocol. | Equality. | **B** |
| Formal verification | Must model `SHA256(pk)==program ∧ Verify(sig,pk,m)`. | `Verify(sig, program, m)` with `\|program\|=32`. | **B** |
| Replay resistance | Neither encoding is a replay control; sighash domain is. | Same. | tie |
| Future security | Can change the hiding hash without changing program length (temptation to add a second hash). | Changing the key encoding requires a new witness version — which is the right friction. | **B** |
| Alignment with ORU / mint | ORU and mint/converge already use `HASH160(pk)`. A adds `SHA256(pk)` as a **third** identity for the same key. Wallet must recover `pk` from a hashed program to build OP_CONVERGE (H-1 is a direct consequence). | v1 program = `pk`; mint/converge/ORU stay `HASH160(pk)`. Two identities, both already required by 20-byte vs 32-byte slots. | **B** |

Ed25519 public keys are 32 bytes. Witness v1 programs are 32 bytes. Option B is the encoding that falls out of those two facts. Option A exists to copy Bitcoin’s “commit to a hash, reveal at spend” pattern onto a slot that is already key-sized.

### 5.3 Recommendation

**Prefer Option B** if the freeze still allows a consensus encoding change. Justification: deletion of SHA256 from the RES spend path, deletion of the H-1 trap, and a classifier that is `v0/20 = HASH160(pk)` / `v1/32 = pk`. That is the minimum surface.

**Keep Option A** only if the freeze sentence is explicit:

> Witness v1 RES program is `SHA256(pk32)`. OP_CONVERGE and OP_RES_MINT commit `HASH160(pk32)`, never `HASH160` of the v1 program.

Do not invent Option C (`HASH160(SHA256(pk))` in mint) to make the headers true. That adds a hash rather than removing one, and it desynchronizes mint spend from ORU’s `HASH160(pk)`.

This audit does **not** treat switching A→B as mandatory for freeze if H-1 is repaired in place. Switching is the cleaner architecture; repairing the names is the minimum.

---

## 6. Required code changes

Prefer deletion. No new asset type, no address-side enum, no Coq axiom.

| ID | Change | Consensus? |
|---|---|---|
| H-1 | Rename `GetConvergeScriptForProgram` / `GetResMintScriptForProgram` to `*ForPubKey`. Fix `interpreter.h`, `res_owner.h`, `script.h`, GBT `res_payout_hash` help text. Test `HASH160(SHA256(pk))` ≠ mint owner. | comment/API; behavior unchanged if callers already pass `pk` |
| H-2 | Delete `WitnessRFCBindingOk`, `ValidateAddressAssetBinding`. Send path: `AssetTypeFromScriptPubKey` only. | no |
| H-3 | Explorer `classifyScriptPubKey` = C++ table; RES2/CVG2/v0-32/unknown are not assets. | no |
| M-1 | Coq classifier: unknown/empty/CVG2/RES2 → invalid, not ORU/None. Drop `AssetCVG` from any lemma claimed to track C++. | no (model) |
| M-2 | Python helper must require canonical mint encoding. | no |
| M-3 | Delete `IsORBMintTx`, `IsV1Program`. Rename `IsORBClaimTx`. Consider moving implied P2PKH out of `ClassifyRigorTxout`. | no if only dead code |
| M-4 | Rewrite `RIGOR_SCRIPT_SECURITY_BOUNDARY.md` and related pages to §1.1. | no |
| A-1 | Either switch v1 program to raw `pk32`, or freeze the Option A sentence in §5.3 in `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`. | **yes** if switching |

Do not add a `CTxOut.nAsset`. Do not classify by Bech32 HRP or version in consensus.

---

## 7. Freeze suitability

| Layer | Freeze now? |
|---|---|
| `CTxOut = (nValue, scriptPubKey)` | **Yes** |
| `ClassifyRigorTxout` / `AssetTypeFromScriptPubKey` as the only `AssetType` function | **Yes**, after wrappers in H-2/M-3 are deleted or made one-line |
| Fail-closed `ASSET_INVALID` | **Yes** in C++; explorer/Coq must match before claiming uniqueness |
| Ownership = one Ed25519 commitment per spendable template | **Yes** |
| Owner-hash contract (`HASH160(pk)` vs `HASH160(program)`) | **Not until H-1** |
| RES v1 encoding | Freeze Option A *or* switch to B; do not leave both in comments |

**Overall:** the consensus model is the right model and should be frozen. The repository is not yet a single classification system. Apply §6 (H-1, H-2, H-3 at minimum) before the v0.1 freeze declaration asserts “exactly one authoritative classification source.”
