# Transaction Kernel Spend-Predicate Refinement (P0-058)

**Task:** `TASK-RIGOR-TRANSACTION-KERNEL-SPEND-PREDICATE-REFINEMENT-P0-058`  
**Date:** 2026-08-30  
**Nature:** Formal spend-predicate boundary. Not a full-node or UTXO proof.

```
TRANSACTION_KERNEL_REFINEMENT   = CLOSED
SPEND_PREDICATE_ALIGNMENT       = PASS
SIGHASH_ALIGNMENT               = PASS
SIGNATURE_BOUNDARY_ALIGNMENT    = PASS
FAIL_CLOSED                     = PASS
SCRIPT_VM_ABSENT                = PASS
MULTISIG_ABSENT                 = PASS
COQ_ADMITTED                    = 0
COQ_AXIOMS                      = 0
PRODUCTION_KERNEL_SWITCHED      = NO
REFINEMENT_PROVED               = TRUE
```

`Print Assumptions` on the predicate / sighash / signature theorems is
`Closed under the global context`.

This is the Transaction Kernel boundary only. It is not CompCert of the
binary, not VST of `predicate_verify.cpp`, and not node equivalence.
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
Coq does not reimplement curve mathematics.

The C++ header lives at `core/src/consensus/txkernel/` and is
verify-only (not in `Makefile.am`).

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
- CompCert / VST of the C++ binary
