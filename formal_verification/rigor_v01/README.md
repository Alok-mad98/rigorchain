# RIGOR v0.1 Formal Model

This directory is the **live** formal model of the RIGOR v0.1 consensus spend
surface. Historical proofs remain under `formal_verification/coq/` (not deleted)
and are classified in `formal_verification/archive/`.

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Canonical repository: https://github.com/rigorchain/rigorchain
Wallet (node, not this model): SQLite `rigor-wallet.sqlite`, not BDB.
No `libbitcoinconsensus` C ABI — this model is the in-process spend surface.

## Unique consensus path

```
UTXO
  → scriptPubKey
  → AssetTypeFromScriptPubKey     (classify)
  → fixed OwnershipPredicate
  → VerifyEd25519(signature, SHA256d(tagged preimage), pk)
```

There is no Script VM, no opcode execution, no Miniscript, no P2WSH, no
CHECKMULTISIG, no Taproot, no ECDSA spend.

## Modules

| File | Property |
|------|----------|
| `AssetClassification.v` | P1 — unique classifier; unknown → INVALID; Option B `program = pk` |
| `ScriptAssetBoundary.v` | P2 — same script ⇒ same asset/ownership; spendable templates are disjoint |
| `Ed25519Ownership.v` | P3 / P5 — three spend paths, all Ed25519; no bypass constructor |
| `SignatureDomainSeparation.v` | P4 — tag \|\| chain_id \|\| tx commitments; prefix injectivity |
| `ResMintCorrectness.v` | RES_MINT — create = spend = HASH160(pk); two maturity layers; mint tag |
| `NoInterpreterConsensus.v` | No interpreter / no obsolete auth surface in the model |
| `AssetStateTransition.v` | P0-023 — INVALID rejected from domain; dual-pool supply identity |
| `RigorUpgradeNamespace.v` | Byte classes Marker/Reserved/Vacant; predicate registry; no VM on future slots |
| `RigorUpgradeNamespace.v` | Byte classes Marker/Reserved/Vacant; predicate registry; no VM on future slots |
| `RigorByteNamespace.v` | P0-026-V2 — `ClassifyRigorByte` completeness; no VM; unknown predicate unspendable |
| `NetworkCapabilityBoundary.v` | P0-029 — capability ≠ state transition; unknown cap has no effect; P2P cannot bypass VerifyRigorPredicate |
| `NetworkSecurityBoundary.v` | P0-030 — P2P message ≠ predicate transition; invalid relay rejected; CAP_X ≠ PredicateVersion |
| `WindowGeneric.v` | P0-034 — W-generic height-anchored sliding window; `W_ok := W > 1`; power-of-two **not** required. **Not a C++ refinement.** |
| `WindowTarget.v` | P0-034 — `T(W) = floor(UNWINDOWED / W)`; A-9C pair; `T_pinned_old_W_is_not_generic`. Production T not changed. |
| `WindowSpecialization.v` | P0-034 — W=32 shipping instance + W=36 **formal candidate only** (`PRODUCTION_W36_LANDED = false`). |
| `W32ReorgStability.v` | P0-032 properties recovered as the W=32 specialization of `WindowGeneric`. **Not a C++ refinement.** |
| `MintSupplyConservation.v` | P0-036 — unified `circulating' = circulating + subsidy + mint − converge − fee_burn`. **Not a C++ refinement.** |
| `ConnectBlockSupply.v` | P0-036 — abstract ConnectBlock monetary slice; `cpp_refinement_proved = false`. |
| `CanonicalMintEncoding.v` | P0-036 — unique mint encoding / inverse parser; malformed fail-closed. |
| `ClassifierRefinementBoundary.v` | P0-039 — `classify = classify_cpp_aligned`; `classifier_refinement_proved = false`. |
| `ReorgIssuanceBound.v` | P0-036/039 — surviving-chain issuance ≤ schedule; orphan mint not in canonical sum. |
| `GenesisZeroIssuance.v` | P0-037/038 — `GenesisReward(0)=0` landed in C++ `GetBlockSubsidy(0)`. `cpp_refinement_proved = false`. |
| `AmountBoundary.v` | P0-039 — CAmount/MAX_MONEY in-range add does not wrap int64. `cpp_refinement_proved = false`. |

## What is proved vs assumed

**Proved (about this model):** classifier is a pure function of script bytes;
forbidden sources return `None`; Option B is definitional; spend type has
exactly three constructors, all Ed25519; sighash preimage is injective in tag,
chain_id, script (compact-size), and amount under well-formed field widths;
mint create predicate equals spend predicate.

**Not proved:** RFC 8032 correctness; HASH160 collision resistance; SHA256d of
the preimage (C++ `HashWriter::GetHash`); refinement to `core/src/**`.

Ed25519 golden vectors for the preimage live in
`core/src/test/data/rigor_sighash_p0_018.json` (C++), not in this cone.

## Build

Listed in `formal_verification/coq/_CoqProject` as `-R ../rigor_v01 RigorV01`.
Logical names: `RigorV01.AssetClassification`, …

```
Window model:                 W-generic
Current production instance:  W=32
Candidate instance:           W=36
Production W=36:              NOT LANDED
refinement_proved:            false
```
