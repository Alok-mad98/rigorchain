# Transaction Kernel Spend-Predicate Refinement (P0-058)

**Task:** `TASK-RIGOR-TRANSACTION-KERNEL-SPEND-PREDICATE-REFINEMENT-P0-058`  
**Date:** 2026-08-30  
**Nature:** Formal spend-predicate boundary. Not a full-node or UTXO proof.

```
TRANSACTION_KERNEL_REFINEMENT   = CLOSED
SPEND_PREDICATE_ALIGNMENT       = PASS
SIGHASH_ALIGNMENT               = PASS
SIGNATURE_BOUNDARY_ALIGNMENT    = PASS
SIGHASH_DOMAIN_CONNECTED        = PASS
MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH = PASS
FAIL_CLOSED                     = PASS
SCRIPT_VM_ABSENT                = PASS
MULTISIG_ABSENT                 = PASS
PREDICATE_BOUNDARY_VS_PRODUCTION = SAME-FUNCTION
COQ_ADMITTED                    = 0
COQ_AXIOMS                      = 0
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
EXTRACTED_KERNEL_TEST_ORACLE       = PASS
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
COMPCERT_VST_DEFERRED              = TRUE
COMPCERT_VST_CANDIDATE             = MakeKernelInput
REFINEMENT_PROVED               = TRUE
```

`Print Assumptions` on the predicate / sighash / signature theorems is
`Closed under the global context`.

This is the Transaction Kernel boundary only. It is not CompCert of the
binary, not VST of `predicate_verify.cpp`, and not node equivalence.
CompCert/VST stays deferred. The only future candidate is
`MakeKernelInput` (pure marshalling). Extracted OCaml from
`MakeKernelInput.v` is a test-only oracle (`extract_makekernelinput_oracle.sh`);
it is not linked into production and does not change authority.
P053 / golden-lock JSON `refinement_proved` stays `false`.
`equivalence_proved` stays `false`.

---

## Path

```
scriptPubKey
    → ClassifyRigorAsset
    → VerifyRigorPredicate
    → sighash
    → Ed25519_verify          (external primitive)
    → accept / reject
```

Production spend authority is `VerifyRigorPredicate()`. There is no
second script parser, no `EvalScript`, no `VerifyScript`, no opcode VM,
and no CHECKMULTISIG spend path.

---

## Input

`TransactionKernelInput` is explicit:

- `scriptPubKey`, `scriptSig`, `witness`
- tx digest fields (`SighashFields` / C++ tag, chain_id, version, hashes,
  prevout, script, amount, sequence, locktime)
- public key, signature, flags

It does not hold `CCoinsView`, `CBlockIndex`, `ChainstateManager`, RPC,
wallet, or miner state.

HASH160 and Ed25519 are external: `keyhash_match` and `crypto_accept`.
Coq does not reimplement curve mathematics. The four oracle fields
(`scriptSig_push_only`, `stack_clean`, `keyhash_match`, `crypto_accept`)
are inputs to the theorem, not results of it. The signed message of a
marshalled spend is `sighash_digest` of `tki_sighash` (F-3). Path tags
ORU / RES / MINT are those of `SighashPreimage.v`. `empty_sighash` is
gone. `make_crypto_accept` requires `sighash_path_consistent` (L-8):
an accepting mint cannot carry an ORU tag. That gate is load-bearing on
`tag_mismatch_pins` (mint+ORU, v0+RES, v1+ORU), not on `all_raw`, which
always writes the assigned tag. Unknown / non-native scripts get an
empty tag (`unknown_version_empty_tag`), matching C++
`KernelAssignedSighashTag`. `VerifyRigorPredicateBoundary` rejects a
hand-built TKI whose tag does not match the script path.
C++ `MakeKernelInput` fills path tags only for mint / v0 / native RES;
missing tx context does not forge zero fields (`sighash_fields_present`),
and `CppSighashPreimage` is empty unless those fields are present.
Byte equality with production `SignatureHash` is VECTOR-TESTED
(`MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH`), not a Coq ∀-tx proof.
The 9216-state cube is `BOUNDED_GATE_DOMAIN`; it is not C++↔Coq exhaustive
equivalence. `DOMAIN_CRYPTO_TRUE=300` is pinned to the extracted oracle.

Definitional (unfold / `reflexivity`) results in `MakeKernelInput.v`
are marked `DEFINITIONAL` in source. They are not ∀-content:

| Name | Kind |
|---|---|
| `to_tki_crypto_accept`, `to_tki_projects_*` | field projection |
| `spend_message_is_sighash_digest` | unfolding |
| `spend_message_is_coq_sighash_digest` | unfolding |
| `path_tags_are_eight` | concrete ASCII length |
| `domain_raw_uses_path_tag` | constructor |
| `pin_tags_domain_separated` | pin witness |

∀-content on the marshalling boundary includes
`crypto_accept_implies_path_tag`, `mint_oru_tag_rejected` /
`tag_mismatch_all_reject`, `unknown_version_empty_tag`,
`different_tags_different_preimages`,
`mint_oru_preimages_differ` / `oru_res` / `mint_res`,
`crypto_accept_false_blocks_spend`, and the decode/flag gates.
`SignatureDomainSeparation.different_tags_different_preimages` is a
separate theorem over pre-serialized field widths; it is not the same
statement as `MakeKernelInput.different_tags_different_preimages`.

The C++ header lives at `core/src/consensus/txkernel/` and is included by
production `predicate_verify.cpp` (still not its own `Makefile.am`
translation unit). `validation.cpp` calls `VerifyRigorPredicate`, which
is `MakeKernelInput` + `VerifyRigorPredicateBoundary`. ScriptError codes
are classified only after the boundary rejects. The S2 differential
test remains a lock that the public entry and the boundary agree:

```
PREDICATE_BOUNDARY_VS_PRODUCTION = SAME-FUNCTION
```

`core/src/test/predicate_boundary_bridge.h` is the test adaptor (stub
checker + concrete input). Marshalling of the four oracle fields lives
in production `MakeKernelInput`.

---

## Proven

| File | Theorem |
|---|---|
| `SignatureBoundary.v` | `valid_signature = signature_accept_boundary` |
| `TransactionKernelRefinement.v` | `CXX_VerifyRigorPredicate = Coq_spend_allowed` |
| `TransactionKernelRefinement.v` | `CppSighash = CoqSighash` |

Sighash layout has no `SIGHASH_ALL` byte, no sighash-flag mutation, and
no `scriptCode` mutation. The spent script is the real `scriptPubKey`.

Fail-closed: empty, P2PKH, P2WSH, unknown version, bad length, malformed
witness, wrong pubkey, wrong signature.

---

## Not proven

- Full-node / ConnectBlock / UTXO mutation / LevelDB
- PoW / ASERT / miner / GBT / wallet / P2P
- libsodium Ed25519 correctness
- CompCert / VST of the C++ binary (deferred; candidate = `MakeKernelInput` only)
- identity of `VerifyRigorPredicate` with `VerifyRigorPredicateBoundary`
  as C++ functions (the production *boolean* is the boundary after
  `MakeKernelInput`; ScriptError mapping is a reject-only classifier,
  not a Coq theorem)
