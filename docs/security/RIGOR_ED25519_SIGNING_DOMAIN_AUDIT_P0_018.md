# RIGOR Ed25519 Signed-Message Domain Separation Audit (P0-018)

Task: `TASK-RIGOR-ED25519-SIGNED-MESSAGE-DOMAIN-SEPARATION-AND-CONSENSUS-FREEZE-SECURITY-AUDIT-P0-018`
Status: **Remediation implemented in tree (P0-018).** Audit findings C-1, H-1, H-2, H-3, M-1, M-2, M-3, L-1 addressed in consensus, wallet, models, freeze docs, and test vectors. Freeze of Principle 3 remains contingent on this digest being the launched signing rule.
Scope: the exact message domain committed by a consensus Ed25519 signature, and whether that domain
uniquely determines asset ownership and transaction meaning.

---

## 0. Verdict

**RIGOR v0.1 Consensus Freeze cannot be finalized as-is.**

Four of the five freeze principles are implemented cleanly and can be frozen permanently. The fifth —
Principle 3, *one signature meaning* — is satisfied only within a single network and only by emergent
properties of the current script templates, not by asserted domain separation. Three of the defects are
hard-fork-only once mainnet launches. Since RIGOR has no launched chain, no historical transactions, and
no compatibility obligation, they must be fixed before the freeze, not after.

| Freeze principle | Can be frozen | Blocking finding |
|---|---|---|
| One asset type authority | Yes | — |
| One script classification path | Yes | — |
| One signature algorithm | Yes | — |
| One ownership model | Yes | — |
| **One signature meaning** | **No** | H-1, H-2, H-3 (+ C-1, M-1 structural) |

---

## 1. Current signing-domain analysis

### 1.1 What is actually signed

Ed25519 signs the raw 32-byte sighash with no additional prefix (`core/src/key.cpp:70-80` →
`ed25519::SignDetached`; verification `core/src/script/interpreter.cpp:1234-1237` →
`crypto_sign_verify_detached`). The message is therefore exactly the digest produced by:

```1194:1220:core/src/script/interpreter.cpp
template <class T>
uint256 SignatureHash(const CScript& scriptCode, const T& txTo, unsigned int nIn, const CAmount& amount, SigVersion sigversion, const PrecomputedTransactionData* cache)
{
    assert(nIn < txTo.vin.size());

    // Unique commitment: all prevouts, all sequences, all outputs. No hashtype
    // field exists on the wire or in the hash. RES v1 is domain-separated.
    HashWriter ss{};
    const bool cacheready = cache && cache->m_bip143_segwit_ready;
    const uint256 hashPrevouts = cacheready ? cache->hashPrevouts : SHA256Uint256(GetPrevoutsSHA256(txTo));
    const uint256 hashSequence = cacheready ? cache->hashSequence : SHA256Uint256(GetSequencesSHA256(txTo));
    const uint256 hashOutputs = cacheready ? cache->hashOutputs : SHA256Uint256(GetOutputsSHA256(txTo));

    ss << txTo.nVersion;
    ss << hashPrevouts;
    ss << hashSequence;
    ss << txTo.vin[nIn].prevout;
    ss << scriptCode;
    ss << amount;
    ss << txTo.vin[nIn].nSequence;
    ss << hashOutputs;
    ss << txTo.nLockTime;
    if (sigversion == SigVersion::WITNESS_V1_RES) {
        ss << uint32_t{1}; // domain tag separating RES v1 from ORU witness v0
    }
    return ss.GetHash();
}
```

### 1.2 Field-by-field commitment matrix

| Consensus element | Committed | Mechanism |
|---|---|---|
| Transaction version | Yes | `nVersion`, 4 bytes |
| Input count | Yes | implied by `hashPrevouts` (36-byte fixed elements) |
| All previous output references | Yes | `hashPrevouts` |
| This input's prevout | Yes | explicit, 36 bytes |
| All sequences | Yes | `hashSequence` |
| This input's sequence | Yes | explicit |
| Output count, amounts, scriptPubKeys | Yes | `hashOutputs` over `CTxOut` (`nValue` + length-prefixed script) |
| Spent amount | Yes | `amount`, 8 bytes |
| Locktime | Yes | `nLockTime` |
| Spent script | **Partially** | derived `scriptCode`, not the real `scriptPubKey` — see H-2 |
| Asset type of outputs | Indirectly | outputs' scriptPubKeys are inside `hashOutputs`; asset is a pure function of script |
| Asset type of the spent input | **Weakly** | only via the RES-v1 4-byte tag; the ORU and RES `scriptCode` are byte-identical |
| Script classification | Indirectly | same as above |
| Destination public key | Yes | inside `hashOutputs` |
| Hashtype byte | **Absent by design** | correct |
| Network / chain identifier | **Absent** | H-3 |
| Input index `nIn` | No | safe: see 1.3 |
| Witness / scriptSig of any input | No | standard, non-authorizing |

No `HashWriter` prefix, no `TaggedHash`, and no `RigorTags` value enters `SignatureHash`. The only
domain separator in the entire signing model is the trailing `uint32_t{1}`.

### 1.3 Primary audit question

> **Can the same signature be interpreted as two different valid transactions?**

**Within one RIGOR network: NO.**

The digest commits to `nVersion`, the full ordered input set, the full ordered output set, this input's
outpoint and sequence, the spent amount, and the locktime. Two transactions that agree on all of those
are the same transaction modulo witness data, which carries no consensus meaning of its own. The
serializations are self-delimiting — `COutPoint` is 36 fixed bytes, `nSequence` is 4, and `CTxOut`
carries a varint-length script — so no splicing or boundary-shift ambiguity exists in
`hashPrevouts` / `hashSequence` / `hashOutputs`.

The absence of `nIn` is not a defect: `prevout` *is* committed, and `CheckTransaction` rejects duplicate
outpoints, so a signature is pinned to exactly one input slot.

```132:136:core/src/consensus/tx_check.cpp
    std::set<COutPoint> vInOutPoints;
    for (const auto& txin : tx.vin) {
        if (!vInOutPoints.insert(txin.prevout).second)
            return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputs-duplicate");
    }
```

**Across networks or a chain split: YES.** See H-3. This is the one affirmative answer, and it is the
single most consequential finding in this audit because it is unfixable after launch.

**Across asset domains: NO today, by one line of margin.** See H-2 — the separation is real but rests on
an unasserted 4-byte suffix plus accidental byte-disjointness of script templates.

---

## 2. Consensus risk classification

### C-1 — Critical (structural / latent): a full script interpreter is retained off the consensus path, containing a second OP_RES_MINT authorization implementation

`VerifyScript` never calls `EvalScript`. Consensus spend dispatch is entirely structural:

```1554:1584:core/src/script/interpreter.cpp
    if (IsResMintScript(scriptPubKey)) {
        if (!scriptSig.IsPushOnly()) {
            return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
        }
        return VerifyResMintSpend(scriptSig, scriptPubKey, *witness, flags, checker, serror);
    }
    // ... witness program dispatch, else SCRIPT_ERR_RIGOR_SCRIPT_TYPE
```

Non-test callers of `EvalScript` are `core/src/script/sign.cpp:604` (parsing scriptSig pushes with a
`BaseSignatureChecker` that always returns false) and `core/src/bench/verify_script.cpp:92`. Neither is
consensus.

Yet `EvalScript` (`interpreter.cpp:353-1100`) remains a complete programmable execution engine —
conditionals, arithmetic, altstack, `FindAndDelete`, `OP_CODESEPARATOR`, `OP_CHECKSIG` — and it contains
an independent reimplementation of the mint spendability rule:

```1026:1071:core/src/script/interpreter.cpp
                case OP_RES_MINT:
                {
                    // Bare scriptPubKey only. Not a witness-script opcode.
                    if (sigversion != SigVersion::BASE)
                        return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
                    // ... re-derives maturity via CScriptNum(stacktop(-2), fRequireMinimal, 5)
                    // ... re-derives pubkey-hash binding
                    if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptCode, SigVersion::WITNESS_V0)) {
```

This is a second source of truth for one spendability rule (Principle 4) and a retained hidden execution
path (Principle 1). It parses maturity with `CScriptNum` minimal-encoding semantics while
`VerifyResMintSpend` uses `ParseResMintScript` — two parsers that are not proven equivalent. It also
carries an internal contradiction that only makes sense as vestigial code: the branch *requires*
`SigVersion::BASE` to execute, then signs under `SigVersion::WITNESS_V0`.

Not exploitable today — it is unreachable. Classified Critical because the purpose of this audit is a
*permanent* freeze, and freezing a consensus surface with a live second interpreter compiled in means the
freeze guarantees nothing that a later refactor cannot silently undo.

### H-1 — High: `SigVersion::BASE` and `SigVersion::WITNESS_V0` are sighash aliases

`SignatureHash` branches only on `WITNESS_V1_RES`. `BASE` and `WITNESS_V0` therefore produce
byte-identical preimages: three declared signing domains, two distinct digests.

Consequences:

1. ORU P2WPKH spends and OP_RES_MINT spends share one digest domain. They are distinguishable only
   because their `scriptCode` byte strings happen to be disjoint — the 25-byte P2PKH template ends in
   `OP_CHECKSIG` (`0xac`), a mint script ends in `OP_RES_MINT`, and `CScript` is length-prefixed. That is
   an emergent property of today's templates, not an invariant the code states or tests.
2. The wallet signs mint inputs with `SignatureHash(..., SigVersion::WITNESS_V0, ...)`
   (`sign.cpp:278-279`) while `ProduceSignature` drives `SignStep` with `SigVersion::BASE`
   (`sign.cpp:439,450`). This is only correct *because* of the aliasing.
3. The amount guard is version-gated and so does not fire on a `BASE` call:

```1259:1259:core/src/script/interpreter.cpp
    if ((sigversion == SigVersion::WITNESS_V0 || sigversion == SigVersion::WITNESS_V1_RES) && amount < 0) return HandleMissingData(m_mdb);
```

   `CreateSig`'s `MoneyRange(amount)` guard is gated the same way (`sign.cpp:110`). A `BASE` call hashes
   an unvalidated `amount` into the signed message.

### H-2 — High: ORU ↔ RES separation rests on one untagged `uint32_t{1}`, over a byte-identical `scriptCode`

The ORU path builds `scriptCode` from the witness program, which is `Hash160(pk)`:

```1373:1386:core/src/script/interpreter.cpp
    const uint160 expect_hash = Hash160(vchPubKey);
    if (expect_hash.size() != program.size() || ...
    CScript scriptCode;
    scriptCode << OP_DUP << OP_HASH160 << program << OP_EQUALVERIFY << OP_CHECKSIG;
    if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptCode, SigVersion::WITNESS_V0)) {
```

The RES v1 key path builds the same template from `Hash160(pk)`, even though the actual witness program
is `SHA256(pk)`:

```1428:1430:core/src/script/interpreter.cpp
            CScript scriptCode;
            scriptCode << OP_DUP << OP_HASH160 << ToByteVector(uint160(Hash160(vchPubKey))) << OP_EQUALVERIFY << OP_CHECKSIG;
            if (!checker.CheckEd25519Signature(vchSig, vchPubKey, scriptCode, SigVersion::WITNESS_V1_RES)) {
```

For a single public key holding both an ORU and a RES output, these two `scriptCode` values are
**identical byte strings**. Two distinct defects follow:

1. **The tag is positional, not tagged.** `uint32_t{1}` occupies BIP143's old `nHashType` slot at the very
   end of the preimage. The ORU preimage is therefore a strict *prefix* of the RES preimage. Separation
   holds only because SHA-256 pads by length. It is not an asserted domain tag, it is a length artifact,
   and the value `1` is indistinguishable from a legacy `SIGHASH_ALL` byte to any reviewer or
   reimplementer.
2. **The signature never commits to the spent output's real script.** RES v1 commits `Hash160(pk)` —
   160 bits — while the output it is spending is locked to `SHA256(pk)` — 256 bits. The witness program
   equality is checked separately (`interpreter.cpp:1421-1424`), so authorization is not currently
   broken. But the signed message does not identify the script it is unlocking, which is precisely the
   binding this audit exists to verify.

Not exploitable today: `prevout` and `amount` are committed and differ between any two real UTXOs. The
margin is one line.

### H-3 — High: no network or chain identifier in the signed message

`core/src/script/` contains no reference to a genesis hash, chain id, or network magic. A signature is
valid on mainnet, testnet, signet, and regtest, and on both sides of any future chain split, for any
outpoint that exists on both chains. This is the SIGHASH_FORKID / BIP-115 problem class, and it is the
one affirmative "yes" to the primary audit question.

RIGOR has not launched. Adding a chain tag is free now and a hard fork later. There is no argument for
deferring it.

### M-1 — Medium: SIGHASH mode plumbing survives with structural side effects and zero cryptographic effect

`nHashType` is stored on the signature creator (`sign.h:44,57`), parsed from RPC
(`wallet/rpc/rpcspend.cpp:1297,1990`, `rpc/rawtransaction_util.cpp:367`), parsed from CLI
(`bitcoin-tx.cpp:528-534,565`), and threaded through PSBT (`psbt.h:1217`). It never reaches
`SignatureHash` — but it still changes behavior:

```787:787:core/src/script/sign.cpp
    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);
```

```828:829:core/src/script/sign.cpp
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mtx.vout.size())) {
```

with the same pattern at `bitcoin-tx.cpp:656,669`, and `SIGHASH_ANYONECANPAY` altering PSBT
`non_witness_utxo` stripping at `wallet/wallet.cpp:2545-2546`.

So RIGOR still exposes optional signing modes at the API boundary (Principle 3). A caller requesting
`SINGLE|ANYONECANPAY` receives a signature that in fact commits to every input and every output, while
some inputs may have been silently left unsigned. Every failure direction is closed (invalid or
incomplete transaction), so this is not exploitable — but it is a signature-meaning ambiguity presented to
integrators, and retaining the `SIGHASH_*` enum in a consensus header invites re-hooking:

```27:37:core/src/script/interpreter.h
/** Signature hash types/flags */
enum
{
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80,

    SIGHASH_DEFAULT = 0, //!< Reserved; treated as SIGHASH_ALL where used
    SIGHASH_OUTPUT_MASK = 3,
    SIGHASH_INPUT_MASK = 0x80,
};
```

### M-2 — Medium: the freeze declaration specifies behavior the code does not implement

`docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` freezes:

> `SIGHASH_SINGLE` out of range | Hash is `uint256::ZERO` (not `ONE`); signature rejected

There is no hashtype in the code, so this frozen rule has nothing behind it. Conversely
`docs/archive/rfc/RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL.md`,
`docs/audit/ED25519_TRANSACTION_VALIDATION_FLOW.md`,
`docs/audit/ALPHA1_CONSENSUS_DESIGN_DECISION_REPORT.md`, and
`docs/security/SECURITY_001_A7_REPORT.md` all still specify `64 + 1 hashtype`, contradicting
`interpreter.cpp:182-187` and `docs/security/RIGOR_SCRIPT_TYPE_REGISTRY.md`. A freeze declaration that
describes non-existent behavior cannot serve as the normative artifact for a permanent freeze.

### M-3 — Medium: the verification models are not equivalent to the C++ classifier

The C++ classification authority is clean and single-sourced (see section 3). The models that exist to
*verify* it are not:

| Model | Divergence from `ClassifyRigorTxout` |
|---|---|
| `formal_verification/spec/rigor_consensus_spec.py:342-384` | `is_allowed_script`, `is_res_script`, `asset_from_script` all omit `RES_MINT` and `CONVERGE` |
| `pool/share-validator/src/res_mint.rs:211-214` | omits the RES2 prefix exclusion enforced by `IsRigorResNativeWitnessProgram` |
| `formal_verification/coq/ScriptMintWitness.v:223-243` | maps unclassified scripts to `SmwNone` rather than INVALID |
| `core/test/functional/test_framework/script_util.py:43-64` | witness-only; omits `RES_MINT` and `CONVERGE` |

Principle 4 demands one classification authority. The C++ side honors it; the proof and cross-check layer
does not, which weakens every assurance derived from it.

### L-1 — Low: sighash test coverage is a single randomized test

`core/src/test/sighash_tests.cpp` is 73 lines total and asserts three things on one random transaction:
determinism, sensitivity to an output mutation, and `WITNESS_V0 != WITNESS_V1_RES`. There are no golden
vectors, no per-field sensitivity matrix, and no assertion that the three `SigVersion` values are
pairwise distinct — an assertion that would have caught H-1 immediately.
`core/src/test/data/sighash.json` is still the inherited Bitcoin vector file, carrying a `hashType`
column the implementation ignores.

---

## 3. Areas that pass

### Asset type binding and Script-as-Asset (audit areas 2 and 3): PASS

`AssetTypeFromScriptPubKey` is fail-closed. It does not silently return a valid asset type when
classification fails, and `ASSET_INVALID` is deliberately distinct from `ASSET_NONE` so that OP_RETURN
cannot be confused with a classification failure:

```67:82:core/src/script/rigor_classify.cpp
AssetType AssetTypeFromScriptPubKey(const CScript& scriptPubKey)
{
    std::vector<std::vector<unsigned char>> sols;
    switch (ClassifyRigorTxout(scriptPubKey, sols)) {
    case TxoutType::WITNESS_V0_KEYHASH:
    case TxoutType::CONVERGE:
        return ASSET_ORU;
    case TxoutType::WITNESS_V1_RES:
    case TxoutType::RES_MINT:
        return ASSET_RES;
    case TxoutType::NULL_DATA:
        return ASSET_NONE;
    default:
        return ASSET_INVALID;
    }
}
```

Unknown scripts are rejected at the consensus layer, not merely by relay policy
(`consensus/tx_check.cpp:85-86` and `:99-100`, both `bad-rigor-script-type`). `ClassifyRigorTxout`
(`rigor_classify.cpp:15-65`) is the single source of truth: `Solver`, `IsResScript`, `IsAllowedScript`,
`IsStandard`, `GetDustThreshold`, `AreInputsStandard`, and `CheckTxOutAssetWitnessBinding` all delegate
to it. The retired `ASSET_CVG = 3` slot is explicitly documented as non-reassignable. The fuzz harness
asserts the fail-closed property directly (`test/fuzz/script.cpp:99-111`).

`CTxOut` carries no asset tag, so asset type cannot be independently forged — it is a pure function of
`scriptPubKey` (`primitives/transaction.cpp:101-112`). Per-asset conservation is enforced with separate
pools, and cross-asset reinterpretation is rejected by name: `cross-asset-input`, `mixed-asset-output`,
`illegal-res-to-oru-conversion`, `bad-txns-spend-non-asset-input` (`consensus/tx_verify.cpp:186-232`,
`:235-321`, `:378-409`).

### SIGHASH model removal (audit area 4): PASS in effect

Confirmed removed or sealed at consensus:

- No hashtype byte on the wire or in the hash. 65-byte signatures are rejected with
  `SCRIPT_ERR_SIG_DER` (`interpreter.cpp:182-198`; test `rigor_witness_tx_tests.cpp:114-131`).
- ECDSA is a sealed stub that always returns false, deliberately so a future re-hook cannot accept
  Ed25519 material under the ECDSA name (`interpreter.cpp:1223-1251`).
- `OP_CHECKMULTISIG` / `OP_CHECKMULTISIGVERIFY` → `SCRIPT_ERR_BAD_OPCODE` (`interpreter.cpp:1077-1080`).
- `OP_CHECKSIGADD` → `SCRIPT_ERR_BAD_OPCODE`; Taproot hashers removed (`interpreter.cpp:1179-1180`).
- P2WSH is not a consensus spend path (`interpreter.cpp:1403-1404`).
- The RES v1 second spend path (11-item PCORB) is deleted, leaving exactly one key path
  (`interpreter.cpp:1436-1453`).
- Fixed 64-byte signature, fixed 32-byte pubkey, two-item witness.

The residual SIGHASH surface is M-1, which lives entirely above consensus.

### One ownership path: PASS

RES v1 outputs have exactly one spend path; any witness shape other than the 2-item key path returns
`SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH`. ORU P2WPKH requires exactly 2 items. Both bind
`Hash160`/`SHA256` of a single 32-byte Ed25519 key. There is no multisig path, no alternative algorithm,
and no script-based ownership logic on the consensus path.

---

## 4. Domain separation recommendation (audit area 5)

The brief proposes `RIGOR_TX_V1 || transaction_hash || asset_type || script_class || amount` and asks for
an independent evaluation. My assessment: **the tag and a chain identifier are necessary; explicit
`asset_type` and `script_class` fields are not, and should be rejected.**

Reasoning:

- **An explicit tag is necessary.** Today's separation is a length artifact (H-2), and the current tag
  value is indistinguishable from a legacy `SIGHASH_ALL` byte. A named, prefix-position, per-path tag
  makes disjointness a stated invariant rather than something a reviewer must derive from script template
  lengths.
- **A chain identifier is necessary.** H-3 is the only affirmative answer to the primary audit question,
  and it is hard-fork-only after launch.
- **Committing the real spent `scriptPubKey` is necessary.** This is the correct fix for H-2's second
  defect, and it subsumes both proposed redundant fields: asset type and script class are pure functions
  of `scriptPubKey`. Once the actual script is committed, the signature is bound to the complete
  classification by construction.
- **An explicit `asset_type` byte should be rejected.** Adding it creates a *second* representation of
  asset classification inside the signed message, which must then be kept consistent with
  `AssetTypeFromScriptPubKey` forever. That is precisely the duplicated classification authority
  Principle 4 forbids. The same argument rejects `script_class`.
- **`transaction_hash` should be rejected** in favor of the existing decomposed commitment. A single
  txid would either be circular (txid depends on witness) or require a second hashing convention.
  `hashPrevouts` / `hashSequence` / `hashOutputs` already commit the whole transaction and are cacheable.
- **`amount` is already committed** and stays.

### Recommended final Ed25519 signing model

```
sighash = SHA256d(
    tag                        //  8 bytes, per-spend-path, ASCII, disjoint
 || chain_id                   // 32 bytes, genesis block hash of this network
 || nVersion                   //  4
 || SHA256d(all prevouts)      // 32
 || SHA256d(all nSequence)     // 32
 || SHA256d(all txout)         // 32
 || this input's prevout       // 36
 || spent scriptPubKey         //  length-prefixed, the REAL script, not a derived template
 || amount                     //  8
 || this input's nSequence     //  4
 || nLockTime                  //  4
)
```

Tags, one per consensus spend path, no `BASE`:

| Spend path | Tag |
|---|---|
| ORU P2WPKH (witness v0 / 20) | `RGRORU01` |
| RES native key path (witness v1 / 32) | `RGRRES01` |
| OP_RES_MINT | `RGRMNT01` |

This preserves every stated principle: minimal surface (one formula, one tag per path), no global
variables (`chain_id` is a compile-time chainparams constant, not consensus state read at validation
time), script is asset (the script itself is committed, nothing derived from it), one key, one signature,
one ownership path.

---

## 5. Required code changes

Ordered by dependency. None of these redesign consensus; each closes a specific finding.

1. **H-3 + H-2 + H-1 — rewrite `SignatureHash`** (`core/src/script/interpreter.cpp:1194-1220`,
   declaration in `interpreter.h`).
   - Add a `tag` prefix derived from `SigVersion`, remove the trailing `uint32_t{1}`.
   - Add `chain_id` from chainparams. It must be threaded as an explicit parameter or a
     `PrecomputedTransactionData` member — **not** a global, per the no-global-variables constraint.
   - Replace the `scriptCode` parameter with the spent output's real `scriptPubKey` at the three call
     sites (`interpreter.cpp:1386`, `:1430`, `:1522`), removing the synthetic P2PKH templates.
2. **H-1 — delete `SigVersion::BASE`** (`interpreter.h:186-191`). Remove it from
   `CheckEd25519Signature`, `CreateSig`'s assertion (`sign.cpp:86`), and `SignStep`'s entry version
   (`sign.cpp:439,450`). Make the amount guard unconditional (`interpreter.cpp:1259`).
3. **C-1 — delete `EvalScript`** and its opcode dispatch, including the `OP_RES_MINT` branch
   (`interpreter.cpp:353-1106`) and `EvalChecksig` / `EvalChecksigPreTapscript`
   (`interpreter.cpp:301-350`). Replace the one non-consensus caller
   (`sign.cpp:604`, `Stacks`) with the existing `EvalScriptPushes` helper
   (`interpreter.cpp:1463-1481`), which is push-only and cannot execute logic. Update
   `bench/verify_script.cpp:92`.
4. **M-1 — remove the SIGHASH enum and `nHashType` plumbing.** Delete `interpreter.h:27-37`; drop
   `nHashType` from `MutableTransactionSignatureCreator` (`sign.h:44,57`) and `SignTransaction`
   (`sign.cpp:784-787,828-829`); delete `fHashSingle` skip logic in both `sign.cpp` and
   `bitcoin-tx.cpp:656,669`; delete `ParseSighashString` (`core_read.cpp:257-266`) and
   `findSighashFlags` (`bitcoin-tx.cpp:528-534`); make the RPC `sighashtype` argument either removed or
   accept only `"ALL"`; drop the `SIGHASH_ANYONECANPAY` branch at `wallet/wallet.cpp:2545-2546`; remove
   the sighash decode table at `core_write.cpp:104-109`.
5. **M-3 — align the models.** Fix `formal_verification/spec/rigor_consensus_spec.py:342-384`,
   `pool/share-validator/src/res_mint.rs:211-214`, and
   `core/test/functional/test_framework/script_util.py:43-64` to match `ClassifyRigorTxout` exactly.
   Either extend `ScriptMintWitness.v` to model `INVALID` or document the abstraction gap explicitly.
6. **M-2 — rewrite the freeze declaration's cryptographic boundary table**
   (`docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md`) to the model in section 4; remove the
   `SIGHASH_SINGLE` out-of-range row; mark RFC-015 and the three audit documents as superseded on
   signature format.

---

## 6. Required test vectors

Presently missing; each maps to a finding.

1. **Golden sighash vectors** — a committed JSON file with, per spend path (ORU / RES / MINT), the full
   preimage hex and the expected digest, so any reimplementation can be checked byte-for-byte. Replaces
   the inherited `core/src/test/data/sighash.json`.
2. **Per-field sensitivity matrix** — mutate each of `nVersion`, each prevout, each sequence, each output
   amount, each output script, the spent amount, the spent script, and `nLockTime`; assert the digest
   changes in every case. This is the machine-checkable form of the section 1.2 table.
3. **Pairwise domain disjointness** — for one fixed transaction, one fixed key, and one fixed amount,
   assert all three tagged digests are pairwise distinct. This is the test whose absence hid H-1.
4. **Cross-asset binding** — construct a key holding both an ORU output and a RES output with identical
   amounts; assert neither signature verifies against the other input.
5. **Cross-network replay** — same transaction skeleton, same key, two different `chain_id` values;
   assert the digests differ and that a mainnet signature fails validation under regtest chainparams.
   This is the direct regression test for H-3.
6. **Signature encoding** — 63, 64, 65 (`64 + SIGHASH_ALL`), and 66-byte signatures; only 64 accepted.
   Extends the existing `rigor_witness_tx_tests.cpp:114-131`.
7. **Classification equivalence** — a shared corpus of scriptPubKeys (all four valid templates, CVG2 and
   RES2 prefixes, P2WSH, legacy P2PKH/P2SH, malformed witness programs) run through the C++ classifier,
   the Python spec, the functional test util, and the Rust validator, asserting identical results. This is
   the regression test for M-3.
8. **No-second-path** — assert `VerifyScript` rejects every witness shape other than the two-item key
   path for both ORU and RES, and that no bare-script input can be spent.

---

## 7. Formal verification implications

- The digest change invalidates any existing golden vector derived from `SignatureHash`. Regenerate
  `formal_verification/golden_vectors.json` and `rigor_golden_vectors.json` where signature-dependent.
- `chain_id` in the preimage requires the model to carry a network parameter. It must be modeled as a
  fixed constant of the chain instance, not as mutable validation state, or the model will imply that
  RIGOR reads global state on the signature path — which it must not.
- Committing the real `scriptPubKey` instead of a derived template *simplifies* the proof obligation: the
  authorization predicate becomes a function of the coin being spent rather than of a template
  reconstruction, so `sighash` binding and `AssetTypeFromScriptPubKey` share one input. The proof that a
  signature is bound to an asset class becomes a corollary of script commitment rather than a separate
  lemma about tag values.
- Deleting `EvalScript` removes the entire opcode-semantics surface from the model's obligations. This is
  the single largest reduction in verified surface available, and it is what makes
  `equivalence_claimed = true` a reachable future goal rather than an aspiration.
- M-3 must be fixed *before* any freeze claim rests on the models: today the Python spec and the Coq
  model classify `RES_MINT` and `CONVERGE` differently from consensus, so any invariant they establish
  about asset conservation does not transfer to the implementation.

---

## 8. Freeze recommendation

**Do not finalize RIGOR v0.1 Consensus Freeze yet.**

Freeze now, permanently — these are correct and single-sourced:

- one asset classification authority (`ClassifyRigorTxout`), fail-closed on unknown scripts
- one asset type derivation (`AssetTypeFromScriptPubKey`), no default, no fail-open
- one signature algorithm (Ed25519, 64 bytes fixed, secp256k1 sealed)
- one ownership model (one key, one signature, one spend path per output type)
- no multisig, no hashtype byte, no P2WSH, no Taproot, no second RES spend path

Fix before freezing — all three are hard-fork-only after launch:

- **H-3** add `chain_id` to the signed message
- **H-2** explicit per-path tags; commit the real spent `scriptPubKey`
- **H-1** delete `SigVersion::BASE`

Fix in the same change, because they are what makes the freeze credible rather than nominal:

- **C-1** delete `EvalScript`
- **M-1** delete the SIGHASH enum and `nHashType` plumbing
- **M-2** correct the freeze declaration
- **M-3** align the verification models
- **L-1** add the section 6 vectors

The digest-affecting work (H-1, H-2, H-3) is a single rewrite of one function plus three call sites. The
remainder is deletion. Given no launched chain, no historical transactions, and no compatibility
obligation, protocol correctness is the cheaper choice by a wide margin — and the freeze that follows will
actually mean what it says.
