# RIGOR Cryptographic Boundary Security Audit (P0-080)

**Task:** `TASK-RIGOR-CRYPTOGRAPHIC-BOUNDARY-SECURITY-AUDIT-P0-080`  
**Date:** 2026-08-31  
**Nature:** Cryptographic-boundary security audit. Isolation of hash, signature,
and sighash authorities + fail-closed malformed input. This is not a SHA-256
mathematical security proof, and it is not an Ed25519 cryptographic proof.

This TASK does not modify protocol rules, Consensus Kernel, Transaction Kernel,
PoW, Genesis, issuance, supply, or draw. It does not add a second hash
implementation, a fallback hash, a wallet/miner hash authority, a signature
parser bypass, or a legacy CHECKSIG path.

```
header bytes / tx bytes / sighash preimage
        ↓
unique hash authority (per role)
        ↓
digest

scriptPubKey / witness / signature / pubkey
        ↓
VerifyRigorPredicate
        ↓
ed25519::VerifyDetached     (accept / reject only)
```

```
CRYPTOGRAPHIC_BOUNDARY_SECURITY_STATUS = CLOSED
CRYPTO_AUTHORITY_COUNT                 = 4
SHA256_AUTHORITY_COUNT                 = 1
ED25519_AUTHORITY_COUNT                = 1
SIGHASH_AUTHORITY_COUNT                = 1
FUZZ_STATUS                            = PASS
CONSENSUS_MODIFIED                     = NO

REFINEMENT_PROVED                      = TRUE
  (inherited: SignatureBoundary accept-boundary; unchanged; no new theorem)

EQUIVALENCE_PROVED                     = false
SHA256_MATH_PROOF_CLAIMED              = false
ED25519_CRYPTO_PROOF_CLAIMED           = false

FINAL_VERDICT:
RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_CLOSED
```

Reproduce: `bash tools/run-cryptographic-boundary-security-audit.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| unique hash roles | Four production hash roles, each with one defining body. Lint: `tools/lint-crypto-authority.py`. |
| single SHA256d primitive | Header hash, transaction digest, and sighash digest compose `CSHA256` as SHA256d (`CHash256` / `HashWriter::GetHash`). Hardware `Transform` variants are the same primitive (`SHA256AutoDetect`), not a second authority. |
| single PoW hash authority | `CheckProofOfWork` calls `rigor_kernel::GetRigorPowHash` only. Digest is SHA3-256(`"RGRPOW01" \|\| header_80`). The removed `CheckProofOfWork(uint256)` overload cannot return. |
| single Ed25519 verify authority | Spend verify is `VerifyRigorPredicate` → `CheckEd25519Signature` → `ed25519::VerifyDetached` → `crypto_sign_verify_detached`. Encoding lengths fail closed before libsodium. |
| single SignatureHash authority | Production preimage is `SignatureHash` in `predicate_verify.cpp`. No hashtype byte. Domain tags `RGRORU01` / `RGRRES01` / `RGRMNT01`. `chain_id` is explicit. |
| mutation sensitivity | Header byte, nonce, nBits, tx byte, sighash field, and endian reversal change the digest or reject. Tests: `core/src/test/test_crypto_boundary_security.cpp`. |
| Ed25519 fail closed | Wrong key, wrong signature, modified message/digest, replay, truncated signature, oversized signature, and malformed public key are REJECT. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| SHA256 mathematical security proof | Collision / preimage resistance of SHA-256 is assumed from the primitive. This audit does not prove FIPS 180. |
| Ed25519 cryptographic proof | Curve arithmetic, SUF-CMA, and libsodium correctness are assumed. Coq `SignatureBoundary` proves the *accept boundary* (`encoding_ok ∧ crypto_accept`), not edwards25519. |
| C++ ↔ Coq equivalence | `EQUIVALENCE_PROVED=false`. `PowDigest.v` states `refinement_proved = false` for the SHA3 model versus C++. Hash-kernel golden vectors are a differential lock, not `⟦C++⟧ = Coq`. |
| miner search correctness | Solo miner `sha2.c` / historical OpuHash is not a consensus authority. Consensus accepts only `GetRigorPowHash`. |
| hardware security | Side channels, fault injection, and host compromise are out of scope. |

This note is a boundary + uniqueness + rejection audit. It inherits the
existing `SignatureBoundary.valid_signature_eq_accept_boundary` theorem
(`REFINEMENT_PROVED = TRUE` for that accept-boundary). It does not add a
theorem, and it does not claim SHA-256 or Ed25519 as proved cryptosystems.

---

## 1. Hash authority inventory

Manifest: `formal_verification/crypto_boundary_audit_manifest.json`

`CRYPTO_AUTHORITY_COUNT = 4`

| Role | Production symbol | Primitive | Defining file |
|------|-------------------|-----------|---------------|
| PoW hash | `GetRigorPowHash` | SHA3-256(`RGRPOW01` \|\| header_80) | `core/src/rigor_kernel/pow_digest.cpp` |
| Block header hash | `CBlockHeader::GetHash` | SHA256d via `SerializeHash` | `core/src/primitives/block.cpp` |
| Transaction digest | `CTransaction::ComputeHash` / `CMutableTransaction::GetHash` | SHA256d, `SER_GETHASH`, no witness | `core/src/primitives/transaction.cpp` |
| Sighash preimage hash | `SignatureHash` | SHA256d via `HashWriter::GetHash` | `core/src/script/predicate_verify.cpp` |

Each role has exactly one production definition. Wallet, miner, and RPC must
not define these symbols. Python `python/rigor_sighash.py` reconstructs
vectors and is labeled **not consensus**.

`SHA256_AUTHORITY_COUNT = 1`

The SHA256d primitive is `CSHA256` composed twice (`CHash256::Finalize` /
`HashWriter::GetHash`). Inner midstates `GetPrevoutsSHA256` / `GetSHA256` /
`SHA256Uint256` are single-SHA256 *components of that same composition*, not
a second hash authority. SIMD / SHA-NI `Transform` pointers selected by
`SHA256AutoDetect` must match the portable Transform; they are not a fallback
hash and not a wallet/miner authority.

PoW is **not** SHA256d. Domain separation from header id:

- header id = SHA256d(header_80)
- PoW digest = SHA3-256(`RGRPOW01` \|\| header_80)

`CheckProofOfWork` compares the SHA3 digest to the compact target. Genesis
(header id == `params.hashGenesisBlock`) skips the work check after the
compact-target check. Failures are not cached.

Forbidden:

- second hash implementation used as consensus authority
- fallback hash
- wallet or miner self-hash as authority

Miner `miner/sha2.c` still names historical OpuHash for nonce search. That
path is not `CheckProofOfWork`. A header whose miner-local digest meets a
target is still REJECT unless `GetRigorPowHash` meets `nBits`.

---

## 2. SHA256 / PoW boundary

Checked:

| Property | Status |
|----------|--------|
| double SHA256 consistency | SHA256d = SHA256(SHA256(x)); single SHA256 of the same bytes differs |
| endian conversion | header / nBits / amounts are little-endian on the wire; reversing bytes changes the digest |
| byte serialization | 80-byte header layout: LE32 version, 32 prev, 32 merkle, LE32 time, LE32 nBits, LE32 nonce |
| length encoding | compact-size prefixes scripts in the sighash preimage; length change changes the digest |
| domain separation | PoW tag `RGRPOW01`; sighash tags `RGRORU01` / `RGRRES01` / `RGRMNT01`; `chain_id` |
| hash input mutation | any listed field flip changes the digest |

Attack tests (must change hash or reject):

| Mutation | Result |
|----------|--------|
| header byte mutation | hash changes |
| nonce mutation | header hash and PoW hash change |
| nBits mutation | header hash and PoW hash change |
| tx byte mutation | txid changes |
| sighash field mutation | digest changes |
| endian reversal | hash changes |

---

## 3. Ed25519 signature boundary

```
VerifyRigorPredicate
        ↓
CheckEd25519Signature
        ↓
ed25519::VerifyDetached     (accept / reject primitive)
```

`ED25519_AUTHORITY_COUNT = 1`

Ed25519 is only an accept/reject cryptographic primitive. Consensus does not
parse a hashtype suffix. Signatures are raw 64-byte detached values.
Public keys are 32 bytes.

`CPubKey::Verify` is a wallet/message helper that still calls
`ed25519::VerifyDetached`. It is not the spend authority.
`validation.cpp` calls `VerifyRigorPredicate`, not `CPubKey::Verify`.

Fail closed:

| Input | Result |
|-------|--------|
| wrong key | REJECT |
| wrong signature | REJECT |
| modified message | REJECT |
| modified digest | REJECT |
| replay signature (same sig, other message) | REJECT |
| truncated signature | REJECT |
| oversized signature | REJECT |
| malformed public key (length or small-order/identity) | REJECT |

`crypto_accept = false` is REJECT even when encodings are well-formed
(Coq `signature_boundary_requires_accept`).

Forbidden:

- signature parser bypass
- alternate verification path
- legacy CHECKSIG
- script VM residue as a spend path

Live CHECKSIG / `EvalScript` / `VerifyScript` are absent from
`predicate_verify.cpp` and the `validation.cpp` spend path (P0-078). Named
`RIGOR_RESERVED_LEGACY_CHECKSIG` bytes remain parser/display residue and are
not dispatched.

---

## 4. Sighash / message-digest boundary

`SIGHASH_AUTHORITY_COUNT = 1`

Unique production function: `SignatureHash`.

Frozen preimage (`predicate_verify.cpp`):

```
tag || chain_id || nVersion || hashPrevouts || hashSequence
|| hashOutputs || this prevout || spent scriptPubKey || amount
|| nSequence || nLockTime
```

`spent_script` is the real scriptPubKey. There is no hashtype field.
`SIGHASH_ALL` / `SIGHASH_DEFAULT` in the header are RPC/PSBT name aliases
and are not written into the preimage. `SIGHASH_NONE` / `SIGHASH_SINGLE` /
`SIGHASH_ANYONECANPAY` are not live flags.

The following changes must change the digest:

| Field | Mechanism |
|-------|-----------|
| input index | this prevout (`hash` + `n`); duplicate prevouts are consensus-invalid |
| amount | 8-byte LE value |
| spent output | compact-size `spent_script` |
| destination | `hashOutputs` (all `CTxOut`) |
| asset | domain tag + spent script / output scripts |
| witness | not in the preimage (non-authorizing); outputs/amount/script still bind the spend |
| transaction fields | `nVersion`, `hashPrevouts`, `hashSequence`, `nSequence`, `nLockTime`, `chain_id` |

Forbidden:

- hidden sighash flag
- mutable signing domain
- ambiguous serialization (self-delimiting compact-size + fixed-width fields)

`CppSighashPreimage` in `transaction_kernel_input.h` is a C++-shaped kernel
projection for refinement tests. It is not a second production `SignatureHash`.

---

## 5. Fuzz

`fuzz/crypto_boundary/` feeds random bytes, malformed keys/signatures, random
digests, and serialization mutations into SHA256d, SHA3-256 short (fixed
88-byte PoW preimage), sighash-shaped packing, and Ed25519 encoding (and
`VerifyDetached` when libsodium is linked).

```
NO CRASH
NO UNDEFINED BEHAVIOR
NO ACCEPT_INVALID
CRYPTO_FUZZ_STATUS = PASS
```

SHA3-256 short asserts on messages longer than the single-block maximum.
The harness only hashes the 88-byte SPEC-008 preimage, matching production.

---

## 6. Inherited formal artifacts

| Artifact | Role in this TASK |
|----------|-------------------|
| `formal_verification/coq/SignatureBoundary.v` | Accept-boundary refinement. `COQ_ADMITTED = 0`. Inherited, not extended. |
| `formal_verification/coq/PowDigest.v` | SHA3-256 PoW *model*. File states `equivalence_claimed = false` and `refinement_proved = false`. |
| Hash-kernel golden / differential gate | CSHA256 ↔ Coq `RigorSha256` vector lock. Not a SHA-256 security proof. |
| `formal_verification/vectors/rigor_sighash_vectors.json` | Finite sighash golden lock (P0-024). Not ∀ equivalence. |

`REFINEMENT_PROVED = TRUE` means those existing accept-boundary / kernel
refinements remain. This TASK does not prove `⟦GetRigorPowHash⟧ = Coq
rigor_pow_digest` for all headers, and it does not prove Ed25519.

---

## Gates

```
bash tools/run-cryptographic-boundary-security-audit.sh
```

Runs: authority lint, inherited SignatureBoundary assumption check, mutation
/ fail-closed test, fuzz smoke, differential gate.
