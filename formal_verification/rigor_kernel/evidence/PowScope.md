# PoW Scope

What the Coq PoW kernel proves, and what it does not.

**Public claim:** Formally verified critical consensus invariants
inside a Coq consensus kernel model, with explicit extraction and
runtime trust boundaries.

## Verified

- Mathematical target relation: digest-as-integer compared with a
  target (`hash_le_target` / `pow_valid_rejects_above_target`)
- Compact target encode/decode **lower bound**
  (`compact_decode_lower_bound`): `decode(encode(t)) <= t` for `t >= 0`
- Deterministic function-ness of the abstract relation (class B)
- Target ordering (`target_monotonic`, `pow_acceptance_monotonic`)
  on the abstract integer comparison. **Not** a hash implementation
  proof. SHA3 is not introduced.

Digest `H` is an uninterpreted `list byte -> list byte`.

## Not verified

- Hash implementation
- keccak
- sha256 / SHA3
- miner implementation
- C++ `CheckProofOfWork` equivalence
- Concrete `"RGRPOW01"` digest values as a refinement

`pow_valid_deterministic` is a class-B supporting lemma (abstract
relation only). It is **not** a digest implementation proof.

Encoding contract and extraction source-identity are proved
(`Kernel/SerializationBoundary.v`, `Kernel/ExtractionBoundary.v`).
Cryptographic primitives and a full extraction refinement are **not**
proved. Inventory: `Kernel/CryptoAssumptions.v`.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`

See `TheoremClassification.md` and `proofs/Pow_Correctness/PowCorrectness.v`.
