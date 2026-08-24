# RIGOR Minimal Script Architecture — Consistency Audit

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-CONSISTENCY-AUDIT-001`  
**Date:** 2026-08-19  
**Layer:** Documentation consistency (read-only vs implementation)  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

This audit does **not** redesign the script system. It checks whether
whitepaper Chapter 3, `docs/security/minimal-script-architecture.md`, and
the website Security Architecture pages describe the shipping protocol.

Normative rules remain in `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`, `docs/archive/rfc/`, and `docs/specification/`.

Code read (not modified):

| File | Role |
|------|------|
| `core/src/consensus/tx_check.cpp` | `IsAllowedScript`, `CheckTxOutAssetWitnessBinding` |
| `core/src/script/rigor_classify.cpp` | `ClassifyRigorTxout` |
| `core/src/primitives/transaction.cpp` | `AssetTypeFromScriptPubKey` |
| `core/src/script/standard.cpp` | `IsConvergenceScript`, `IsResScript` |
| `core/src/script/interpreter.cpp` | `VerifyWitnessProgram`, Ed25519 / secp256k1 disable |
| `core/src/consensus/tx_verify.cpp` | Per-asset pools, `cross-asset-input`, CVG2 spend reject |
| `core/src/coins.cpp` | CVG2 excluded from UTXO set |
| `core/src/validation.cpp` | Taproot/BIP341 path removed (commented) |

---

## 1. Verified items

### A. Script classification

Combined consensus gates for **new chain outputs**:

1. `IsAllowedScript`: witness program, version 0 (any BIP141 length) **or** version 1 with a 32-byte program.
2. `CheckTxOutAssetWitnessBinding` / `ClassifyRigorTxout`: only `WITNESS_V0_KEYHASH` (20 bytes), `WITNESS_V0_SCRIPTHASH` (32 bytes), `WITNESS_V1_RES` (v1 / 32 bytes), or `NULL_DATA` (`OP_RETURN`).
3. Unspendable `OP_RETURN` allowed only at `nValue == 0`.

| Family | Code | Documentation after this audit |
|--------|------|--------------------------------|
| Witness v0 ORU spending | P2WPKH 20-byte; P2WSH or native 32-byte Ed25519 | Match |
| Witness v1 RES key-path | Non-CVG2 32-byte program; Ed25519 `<sig><pubkey>` | Match |
| Witness v1 CVG2 | Prefix `CVG2`; `AssetTypeFromScriptPubKey` → `ASSET_ORU`; not in UTXO set | Match |
| P2PKH / P2SH as outputs | Rejected (`bad-rigor-script-type` / `bad-txns-asset-script-nonstandard`) | Match |
| BIP341 Taproot | Removed from `VerifyWitnessProgram` / mempool path | Match |
| secp256k1 CHECKSIG | `SCRIPT_ERR_SECP256K1_DISABLED` | Match |
| Arbitrary contract VM | Not implemented | Match |

**Witness v1 is not Bitcoin Taproot.** Sharing BIP141 version byte 1 is not inheriting BIP341 control blocks, annex, or tapscript.

### B. Asset identity

`CTxOut` has no parallel asset field. Class is inferred from `scriptPubKey`. Conservation is per pool (`CheckTxInputs_ORU` rejects `cross-asset-input` when `res_in > 0` on an ORU-domain tx).

ORU and RES are native UTXO identities at the same protocol layer. They are not colored coins, inscriptions, Ordinals, Runes, wrapped assets, or smart-contract tokens. CVG2 is an ORU-domain commitment, not a child token of RES.

### C. Script limitation (wording)

After correction, public copy states:

**Does not have:** general-purpose VM; arbitrary smart contracts; programmable execution layer; Turing-complete execution; unrestricted programmable state.

**Does have:** constrained transaction validation; asset-specific script rules; deterministic verification logic.

Required sentence is present on the chapter, bound whitepaper, security note, and both website pages:

> RIGOR minimizes script complexity rather than eliminating scripts.

### D. Security surface

Actual execution / validation surfaces (not “zero scripts”):

| Surface | Where |
|---------|--------|
| Witness v0 | `VerifyWitnessProgram` v0 20-byte and 32-byte native Ed25519 |
| P2WSH | v0 32-byte when SHA256(witness script) matches; inner script at `SigVersion::WITNESS_V0` — largest inherited surface |
| Witness v1 | RES2-prefixed and native key-path; two-item witness |
| Signature verification | Ed25519; secp256k1 disabled |
| CVG2 validation | Prefix/structure; `bad-txns-spend-convergence` if spent; omitted from `AddCoins` |

### E. Website and GitHub links

| Surface | Status |
|---------|--------|
| `/documentation/security` | Live `SecurityArchitecturePage` |
| `/documentation/security/minimal-script-architecture` | Live `MinimalScriptArchitecturePage` |
| Whitepaper Chapter 3 | Bound in `RIGOR_PROTOCOL_WHITEPAPER.md`; site `/documentation/whitepaper` `#minimal-script` |
| GitHub | `docs/security/minimal-script-architecture.md`; this audit |

Terminology is aligned: native UTXO assets, Witness v1 ≠ Taproot, CVG2 is not a smart contract, `equivalence_claimed = false`.

---

## 2. Inconsistencies found

These were **documentation** issues, not protocol bugs.

| ID | Finding | Severity |
|----|---------|----------|
| I-1 | Required sentence “minimizes script complexity rather than eliminating scripts” was absent | Medium (wording) |
| I-2 | “Does not provide general-purpose **scripting**” could be read as no scripts, while P2WSH still executes an inner script | Medium (wording) |
| I-3 | Positive triad (constrained validation / asset-specific rules / deterministic verification) was implied, not stated | Low |
| I-4 | “Programmable execution layer” was not listed among the explicit non-features | Low |
| I-5 | Tables labeled ORU as “Witness v0” only. Spendable ORU is v0; **CVG2 is Witness v1 ORU-domain** | Medium (classification) |
| I-6 | Security hub and condensed whitepaper site section omitted the execution-surface inventory (P2WSH, signatures, CVG2) | Medium |
| I-7 | Website GitHub path was `<code>` text, not a hyperlink | Low |
| I-8 | Condensed whitepaper page omitted “wrapped assets” from the overlay-rejection list | Low |

Not treated as documentation errors (accurate as written, or code-comment only):

- `IsAllowedScript(v0)` is wider than 20/32 bytes; the binding classifier closes it. Combined gate matches the allowed-output table.
- 36-byte “claim program” exists in headers; not a consensus output (`IsAllowedScript` requires 32-byte v1).
- Interpreter comments still say “ORB2” / “P2TR” while tags/behavior are `RES2` and non-Taproot. Behavior matches docs; comments are stale (**code**, out of scope).

---

## 3. Corrections made

Documentation only. No consensus, kernel, validation, wallet, miner, or RFC edits.

| File | Change |
|------|--------|
| `docs/whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md` | Required sentence; does/does-not lists; execution-surface table; ORU v0 vs CVG2 v1 |
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` Chapter 3 | Same wording in the bound paper |
| `docs/security/minimal-script-architecture.md` | Same; execution-surface table; ORU/CVG2 row |
| `web/rigorchain-site/src/pages/MinimalScriptArchitecturePage.tsx` | Same; GitHub `<a href>` |
| `web/rigorchain-site/src/pages/SecurityArchitecturePage.tsx` | Required sentence; execution surfaces; GitHub/audit links |
| `web/rigorchain-site/src/pages/WhitepaperPage.tsx` | Chapter 3 snippet: required sentence, surfaces, wrapped assets, CVG2 |
| `docs/security/README.md` | Index this audit |
| This file | Audit record |

---

## 4. Remaining risks

| Risk | Why it remains | Mitigation in docs |
|------|----------------|--------------------|
| **P2WSH inner scripts** | Largest inherited opcode surface (timelocks, etc.) with Ed25519-only CHECKSIG | Named explicitly; not called a VM |
| **Stale interpreter comments** | `IsRes2AssetProgram` comment says ORB2; native path comment says “P2TR” | Docs use `RES2` / “not Taproot”; comments need a later code-comment pass (not this task) |
| **Parse default ORU** | `AssetTypeFromScriptPubKey` returns `ASSET_ORU` for non-witness leftovers | New outputs cannot use those scripts; remnant parse path |
| **Two v1 RES spend encodings** | `RES2` prefix vs native 32-byte pubkey | Classification is “non-CVG2 v1/32 = RES”; spend rules differ by prefix |
| **36-byte program constant** | `WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE` in headers / wallet semantic code | Not an `IsAllowedScript` chain output |
| **`equivalence_claimed = false`** | Coq model ≠ C++ refinement | Unchanged honesty flag |
| **Website GitHub URLs** | Point at `rigorchain/rigorchain` `main`; local docs may be ahead of remote until a later push | In-tree paths remain canonical |

---

## Verdict

**Documentation is technically accurate** after the corrections in §3, relative to the audited implementation.

The protocol has constrained scripts, not zero scripts, and not a general-purpose contract platform. Public copy now says that in one sentence, lists the real surfaces, and classifies CVG2 as ORU-domain Witness v1 rather than collapsing all ORU onto Witness v0.
