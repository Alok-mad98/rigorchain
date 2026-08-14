# RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL

**Title:** RIGOR Alpha1 Independent Consensus Model  
**Document ID:** `RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL`  
**Status:** FROZEN (Alpha1 protocol generation)  
**Date:** 2026-08-14  
**Task:** `TASK-CONSENSUS-ALPHA1-DESIGN-DECISION-001`  
**Design route:** **Route B** — RIGOR Alpha1 is an independent cryptographic system  
**Companion audit:** `docs/audit/ALPHA1_CONSENSUS_DESIGN_DECISION_REPORT.md`

> **Disambiguation.** This document is **not** `RFC-015-OCCAM-REBALANCE.md`. Occam Rebalance froze RES economics (`RES_REWARD_AMOUNT`, `|F|`, `RES_MAX_SUPPLY`). This RFC freezes Alpha1 **protocol identity**, **cryptography**, **block policy**, and **signature verification**. Both remain in force; they govern different surfaces.

---

## 0. Freeze statement

RIGOR Alpha1 is hereby defined as an **independent blockchain protocol**.

It shares a Bitcoin-descended **storage and serialization ancestry** (`CTxOut = (nValue, scriptPubKey)`, 80-byte header layout, witness v0/v1 program envelopes). It does **not** share Bitcoin’s consensus signature model, Bitcoin’s block-space policy, or Bitcoin’s fee-as-consensus semantics.

This RFC freezes that independence. It does **not** restore ECDSA/secp256k1. It does **not** authorize a revert to `v0.1.0-alpha` binaries.

**Normative code (this freeze):**

| Surface | Location |
|---|---|
| Block space + lock version | `core/src/consensus/rigor_consensus_lock.h` (`RIGOR_CONSENSUS_LOCK_VERSION = 2`) |
| Block-space aliases | `core/src/consensus/consensus.h` |
| Spend verification | `core/src/script/interpreter.cpp` (`EvalChecksigPreTapscript`, `OP_CHECKMULTISIG`, `VerifyWitnessProgram`) |
| Key/sign primitives | `core/src/key.cpp`, `core/src/pubkey.cpp`, `core/src/crypto/ed25519_provider.h` |
| Fee identity | `core/src/consensus_rigor/fee.h`; `docs/rfc/RFC-FEE-RATE-AUCTION-CONSENSUS-MODEL.md` |
| Convergence / RES | RFC-000, RFC-014 v1.2, RFC-015 Occam, `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` |

---

## 1. Protocol identity

### 1.1 Statement (MUST)

```
RIGOR Alpha1 is an independent blockchain protocol.
```

- Chain name: `RIGOR` (`Consensus::CHAIN_NAME`).
- Mainnet Bech32 HRP: `rg`.
- Mainnet P2P magic: `ce f1 db fa`.
- Mainnet P2P port: `8760`.
- Consensus lock generation: `RIGOR_CONSENSUS_LOCK_VERSION = 2`.

### 1.2 What independence means

| Claim | Alpha1 position |
|---|---|
| Bitcoin-compatible spend signatures | **NO** |
| secp256k1 / ECDSA consensus verification | **NO** |
| Bitcoin Core binary / wallet interop | **NO** |
| Shared UTXO set with Bitcoin or with `v0.1.0-alpha` | **NO** (see §10) |
| Native storage shape (`CTxOut`) unchanged | **YES** (architecture constitution) |
| Script-as-compliance-layer classification | **YES** |
| Witness surface | **v0 and v1 only** (no Taproot / BIP341) |

Asset **classification** is inferred from `scriptPubKey`. Asset **legitimacy** still requires the full consensus path (UTXO commitments, script rules, economic proofs, deterministic verification). Parsing a script is not the whole asset definition.

### 1.3 Route B (selected)

```
ORU transaction signatures:  Ed25519
RES operations:              Ed25519
ECDSA / secp256k1:           MUST NOT exist on the consensus spend path
```

Route A (retain Bitcoin ECDSA for ORU) is **rejected**. Alpha1 does not implement a dual-curve consensus.

---

## 2. Cryptography

### 2.1 Algorithm lock

| Domain | Algorithm | Public key | Signature |
|---|---|---|---|
| **ORU** spends (Witness v0) | Ed25519 (detached, RFC 8032) | 32-byte Ed25519, or ORU wrapper `0x00 \|\| pk32` (33 bytes) | 64 bytes + 1 hashtype byte |
| **RES** spends / native v1 key path | Ed25519 (detached, RFC 8032) | 32-byte Ed25519 program | 64 bytes + 1 hashtype byte |
| **RES** mint / convergence proofs | Ed25519 for any spend authorization; σ is **not** a signed spend digest | See RFC-000 / SECURITY-001-A7 | Spend signatures never authorize via σ |

Implementation: `ed25519::SignDetached` / `ed25519::VerifyDetached` (`libsodium` ≥ 1.0.18). There is **no** `secp256k1_*` consensus call in `core/src`.

### 2.2 Message digest (inherited serialization, new verifier)

Witness v0 and the native v1 key path compute a 32-byte message via the existing `SignatureHash` construction (BIP143-shaped for `SigVersion::WITNESS_V0`).

That digest is **the message**. It is **not** an ECDSA signature payload.

```
sighash_32 = SignatureHash(scriptCode, tx, nIn, hashtype, amount, WITNESS_V0, txdata)
valid      = Ed25519_Verify(sig[0..64), sighash_32, pk32)
```

Hashtype is the 65th byte (`SIGHASH_ALL` / `SINGLE` / `NONE`, optionally `ANYONECANPAY`). Compact recoverable ECDSA signatures (`SignCompact`) are **not** defined.

### 2.3 Pubkey shapes accepted by CHECKSIG

Dispatch order in `EvalChecksigPreTapscript` is consensus-material and MUST NOT be reordered without a new lock version:

| Step | Predicate | Verifier |
|---|---|---|
| 1 | 33 bytes and `pubkey[0] == 0x00` (`IsRigorOruWitnessV0PubKey`) | `CheckEd25519Signature` on `pubkey[1..33)` |
| 2 | 32 bytes (`IsEd25519PubKey`) | `CheckEd25519Signature` |
| 3 | Anything else, including compressed secp `0x02` / `0x03` | `SCRIPT_ERR_SECP256K1_DISABLED` |

The same order applies inside `OP_CHECKMULTISIG`.

### 2.4 Forbidden cryptography (MUST NOT)

- secp256k1 signing on the consensus path
- ECDSA verification of spend signatures
- Schnorr/BIP340/BIP341
- Bitcoin witness-v0 ECDSA P2WPKH (`0x02`/`0x03` + DER)
- DER / low-S as a consensus acceptance condition for spend signatures
- Dual-curve “compatibility mode”

`CheckECDSASignature` / `VerifyECDSASignature` may remain as **dead symbols** for DummySignatureChecker / PSBT extractors. `EvalChecksig` MUST NOT call them. `CPubKey::Verify` MUST continue to reject any key whose first byte is not `0x00`.

---

## 3. Consensus changes from Bitcoin ancestry

Alpha1 is not a Bitcoin soft fork. The following are intentional protocol divergences, all consensus-breaking versus Bitcoin Core and versus published `v0.1.0-alpha` where noted.

### 3.1 From Bitcoin Core

| Surface | Bitcoin | RIGOR Alpha1 |
|---|---|---|
| Spend signatures | secp256k1 ECDSA (DER) | Ed25519 detached |
| Witness v0 P2WPKH | Hash160(compressed secp) | Hash160(`0x00 \|\| ed25519_pk`) or native 32-byte program |
| Witness v1 | Taproot / BIP341 | RES / CVG2 only; no Taproot |
| Block serialized cap | 4_000_000 | **1_000_000** |
| Block weight cap | 4_000_000 | **1_000_000** |
| Block sigops cost | 80_000 | **20_000** |
| Fee identity | often vsize / weight feerate as policy, conservation of satoshis | `fee_i = ORU_in − ORU_out`; wire bytes; feerate **not** consensus |
| PoW | SHA256d | OpuHash (locked in `rigor_consensus_lock.h`) |
| Second asset | none | RES via deterministic convergence draw |
| Address HRP | `bc` | `rg` |
| P2P magic / ports | Bitcoin | RIGOR (`ce f1 db fa` / 8760) |

### 3.2 From published `v0.1.0-alpha` (tag `434357dacd1dd034ce6b45991e6bf604af5ed433`)

| Surface | `v0.1.0-alpha` | Alpha1 tree |
|---|---|---|
| `RIGOR_CONSENSUS_LOCK_VERSION` | `1` | **`2`** |
| `MAX_BLOCK_SERIALIZED_SIZE` | `4_000_000` (hardcoded) | `Consensus::BLOCK_SERIALIZED_SIZE_LIMIT` = **1_000_000** |
| `MAX_BLOCK_WEIGHT` | `4_000_000` | **1_000_000** |
| `MAX_BLOCK_SIGOPS_COST` | `80_000` | **20_000** |
| Witness v0 secp `0x02`/`0x03` CHECKSIG | still dispatched to `CheckECDSASignature` | **hard-fail** `SCRIPT_ERR_SECP256K1_DISABLED` |
| `CKey::Sign` / `CPubKey::Verify` | already Ed25519 | unchanged (Ed25519) |

The `v0.1.0-alpha` ECDSA witness-v0 branch was already **unable to succeed** (`CPubKey::Verify` required `0x00 \|\| pk32` and a 64-byte Ed25519 sig). Alpha1 removes the branch so failure is an explicit consensus error rather than a CHECKSIG-false / NULLFAIL path. That is still a consensus change (§6.3).

### 3.3 RFC updates required by this freeze

| Document | Action |
|---|---|
| **This RFC** | Normative freeze of Alpha1 identity, crypto, block policy, verification |
| `RFC-015-OCCAM-REBALANCE.md` | Unchanged (economics) |
| RFC-000 | Unchanged (selection / σ); this RFC does not retarget `T` or `W` |
| RFC-ADDRESS-001 | Remains: v0 = ORU, v1 = RES; keys are Ed25519 |
| `docs/freeze/RIGOR_SIGNATURE_DOMAIN_CONSTITUTION.md` | Aligns with this RFC (wallet/harness layer) |
| Fee RFC | Remains normative for `fee_i` / `fee_rate` / `f_i` |

---

## 4. Block policy

### 4.1 Alpha1 block policy (ACCEPTED)

```
MAX_BLOCK_SIZE     = 1_000_000 bytes     // serialized (MAX_BLOCK_SERIALIZED_SIZE)
MAX_BLOCK_WEIGHT   = 1_000_000           // BIP141-shaped weight cap
MAX_SIGOPS         = 20_000              // MAX_BLOCK_SIGOPS_COST = weight / 50
```

Locked in `rigor_consensus_lock.h`:

```
BLOCK_SERIALIZED_SIZE_LIMIT = 1000000
BLOCK_WEIGHT_LIMIT          = 1000000
BLOCK_SIGOPS_COST_LIMIT     = 20000
RIGOR_CONSENSUS_LOCK_VERSION = 2
```

`WITNESS_SCALE_FACTOR = 4` remains for **legacy weight accounting** (minimum transaction weight, `GetBlockWeight`, stripped-size × 4 early check, sigops cost scaling). It MUST NOT enter fee identity or fee-rate consensus (§7).

### 4.2 Capacity interpretation

With both serialized size and weight capped at 1_000_000:

- A stripped (no-witness) body cannot exceed 250_000 bytes under `stripped_size * 4 > MAX_BLOCK_WEIGHT`.
- Total serialized bytes cannot exceed 1_000_000.
- `GetBlockWeight(block) > MAX_BLOCK_WEIGHT` is consensus-invalid (`validation.cpp`).

This is a **deliberate 4× reduction** of Bitcoin’s 1 MiB-base / 4M-weight envelope, plus a serialized 1 MiB hard cap. Witness bytes do not buy a Bitcoin-style extra 3 MiB of block space.

### 4.3 Rationale (accepted for Alpha1)

| Criterion | Verdict |
|---|---|
| Security-budget experiment | **Appropriate.** Smaller blocks bound validation, bandwidth, and orphan risk while the dual-asset / Ed25519 / OpuHash stack is proven in the field. |
| Decentralized operation | **Appropriate.** 1 MiB serialized keeps IBD and tip-following within amateur-hardware range. |
| Node accessibility | **Appropriate.** Complements on-chain-only scaling (no Lightning / rollup protocol). |
| Long-term scalability | **Frozen for Alpha1, not a promise of a later raise.** Any increase is a new lock version (hard fork). Capacity remains entirely on-chain. |

Alpha1 does **not** adopt witness-discounted fee markets as a substitute for block size.

---

## 5. Transaction validation rules

### 5.1 Storage and classification

- `CTxOut` remains `(nValue, scriptPubKey)`. No parallel `asset` field.
- Asset class is inferred from `scriptPubKey`:
  - Witness v0 → **ORU**
  - Witness v1, program prefix `CVG2` → ORU convergence (burn) output
  - Witness v1, 32-byte program, not CVG2 → **RES**
- Unknown witness versions are consensus-invalid on this path (`SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM` is not a deferred upgrade hatch for Taproot).

### 5.2 Allowed scripts

Only Witness v0 (20- or 32-byte program) and Witness v1 (32-byte program). No BIP341.

### 5.3 Conservation (dual pool)

- **POOL-T:** ORU conservation and RES conservation are separate.
- **POOL-F:** network fee is ORU-only: `fee_i = ORU_in − ORU_out`, `fee_i >= 0`.
- RES MUST NOT fund network fees.
- Coinbase maturity remains 100 blocks.

### 5.4 Block membership

A block is invalid if any of the following hold:

- serialized size > `MAX_BLOCK_SERIALIZED_SIZE`
- `GetBlockWeight` > `MAX_BLOCK_WEIGHT`
- stripped size × `WITNESS_SCALE_FACTOR` > `MAX_BLOCK_WEIGHT`
- sigops cost > `MAX_BLOCK_SIGOPS_COST`
- any transaction fails script verification under Alpha1 signature rules
- any transaction violates dual-pool conservation or RES mint rules

---

## 6. Signature verification rules

### 6.1 ORU (Witness v0)

| Program | Witness | Rule |
|---|---|---|
| 20-byte (P2WPKH) | `<sig+ht><pubkey>` | Implied `OP_DUP OP_HASH160 <program> OP_EQUALVERIFY OP_CHECKSIG`. Pubkey MUST be `0x00 \|\| pk32` (Hash160 of that blob equals program). CHECKSIG is Ed25519. |
| 32-byte, `SHA256(witnessScript) == program` | P2WSH | Execute witness script at `SigVersion::WITNESS_V0`; inner CHECKSIG is Ed25519-only. |
| 32-byte, hash mismatch, stack size 2, `pubkey == program` | Native Ed25519 | Direct Ed25519 key-path. P2WSH hash match takes precedence. |

Bitcoin P2WPKH (Hash160 of `0x02`/`0x03` secp) is **invalid**.

### 6.2 RES (Witness v1)

| Program | Witness | Rule |
|---|---|---|
| Native RES (`IsRigorResNativeWitnessProgram`) | 2-item `<sig+ht><pk32>` with `pk32 == program` | `CheckEd25519Signature` |
| ORB2 (`IsRes2AssetProgram`) | 2-item; 28-byte tail of program binds pubkey | `CheckEd25519Signature` |
| Any other stack shape | — | `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH` (PCORB 11-item script path **removed**, SECURITY-001-A7) |

### 6.3 Explicit disable (consensus)

```
IF pubkey is not Ed25519-shaped (32-byte or 0x00||32):
    → SCRIPT_ERR_SECP256K1_DISABLED
    → script fails entirely
```

This applies to `SigVersion::BASE` and `SigVersion::WITNESS_V0`. There is no “push false and continue” ECDSA CHECKSIG.

### 6.4 Encoding

Spend signatures MUST be exactly `64 + 1` bytes with a defined hashtype. DER ECDSA encoding (`IsValidSignatureEncoding`, 9–73 bytes, `0x30` compound) is **not** a valid spend encoding. Residual DER helpers MUST NOT be used to accept secp signatures.

---

## 7. Fee model reference

Normative: `docs/rfc/RFC-FEE-RATE-AUCTION-CONSENSUS-MODEL.md` and `core/src/consensus_rigor/fee.h`.

### 7.1 Identity (consensus)

```
fee_i = sum(ORU inputs) − sum(ORU outputs)
```

- Unit: ORU base unit (`COIN = 1e8`).
- Type: unsigned integer economics; `fee_i MUST be >= 0`.
- RES inputs/outputs MUST NOT enter `fee_i`.

### 7.2 Size metric

```
tx_size_bytes = serialized network transaction byte length
```

Forbidden as a fee metric: Bitcoin weight, vsize, witness discount, any “effective size.”

### 7.3 Fee rate (NOT consensus)

```
fee_rate is a mempool / block-assembly ordering key only.
```

Integer comparison (floating point forbidden):

```
a outranks b  ⇔  a.fee * b.size > b.fee * a.size
```

`fee_rate` MUST NOT enter:

- transaction validity
- `H_i < T · f_i`
- winner selection

### 7.4 Consensus weight `f_i`

```
f_i = Mint Cost (convergence ORU amount)   if RES mint
f_i = fee_i                                 if RES spend
```

`f_i` is a raw integer. No log, exponent, scaling, or floating point.

---

## 8. RES Convergence model reference

Unchanged by this RFC. Alpha1 inherits and restates:

| Item | Authority |
|---|---|
| Selection inequality `H_i < T · f_i` with integer cross-multiply | RFC-000, RFC-001, RFC-008 |
| σ preimage (100 bytes, parent hash `B_{h-1}`) | RFC-000 §4 (Sigma-PoW-01 / SECURITY-001-A8) |
| Draw window `W = 32`; `T` per RFC-014 v1.2 | RFC-014 v1.2, `CONVERGENCE_DRAW_WINDOW` |
| 100.00 RES / successful draw; `|F| ≥ 1`; 21M RES cap | RFC-015 Occam |
| PoW does not mint RES; miner only settles | Architecture constitution |
| Spend authorization is Ed25519 over the spending tx, never over σ | SECURITY-001-A7 |

This RFC adds only: **every RES spend and every ORU spend that authorizes a convergence-related input uses Ed25519**, matching Route B.

---

## 9. Forbidden consensus behavior

The following MUST NOT appear as consensus-valid behavior in Alpha1:

| Forbidden | Reason |
|---|---|
| **ECDSA** spend verification | Route B; independent crypto |
| **secp256k1** consensus signing / verifying | Same |
| **Bitcoin witness v0 ECDSA path** | `0x02`/`0x03` + DER CHECKSIG |
| **Bitcoin script signature assumptions** (DER, low-S as acceptance, recoverable compact sigs) | Ed25519 encoding only |
| **Witness discount in fee consensus** | Fee uses wire bytes |
| **Weight-based fee as consensus validity** | `fee_rate` is mempool-only |
| **Floating-point** in fee comparison, `f_i`, or `H_i < T · f_i` | RFC-000 integer law |
| Taproot / BIP341 | Architecture constitution |
| Account-balance truth; indexer/oracle consensus; admin mint/freeze | Dead-sin list |
| Changing `CTxOut` storage | Storage invariant |

---

## 10. Versioning and compatibility

### 10.1 Decision (this RFC)

Alpha1 is a **new protocol generation**. It MUST NOT ship under the published identity `v0.1.0-alpha`.

Recommended product / tag identity: **`v0.1.1-alpha` (Alpha1)**. See the companion audit §6.

### 10.2 Why Alpha1 is not compatible with previous alpha binaries

Published `v0.1.0-alpha` nodes and Alpha1 nodes do not implement the same consensus:

1. **Block space.** A `v0.1.0-alpha` node accepts blocks up to 4 MB / 4M weight / 80k sigops. An Alpha1 node rejects anything above 1 MB / 1M weight / 20k sigops. Mixed networks fork at the first oversized (by Alpha1) block.
2. **Lock version.** `RIGOR_CONSENSUS_LOCK_VERSION` 1 → 2 is defined as a hard-forking lock-bundle identifier.
3. **CHECKSIG semantics.** Witness v0 secp-shaped pubkeys change from “ECDSA checker returns false” to `SCRIPT_ERR_SECP256K1_DISABLED` (whole-script failure). Any script that observed CHECKSIG-false without failing is interpreted differently.
4. **Protocol claim.** Alpha1 **declares** Ed25519-only consensus. `v0.1.0-alpha` still contained a live interpreter dispatch into `CheckECDSASignature` even though `CPubKey::Verify` could not accept secp keys.

Wallet files that already use `0x00 || ed25519` ORU keys and native RES Ed25519 keys remain **byte-compatible as key material**. They are **not** network-compatible across mixed node versions. Bitcoin WIF/hardware-wallet ECDSA stacks are not Alpha1 signers.

### 10.3 What this RFC does not do

- Does not modify consensus, wallet, or RPC implementation (specification freeze only).
- Does not retarget RES `T`, `W`, subsidy, or Occam economics.
- Does not authorize deleting residual ECDSA **symbols** in this task; a later hygiene task may, without restoring secp verification.

---

## 11. Alpha1 cryptographic model (formal)

```
MODEL := Independent-Ed25519-UTXO

ORU_SIG     := Ed25519_Detached(sk, BIP143_or_legacy_sighash_32) || hashtype
RES_SIG     := Ed25519_Detached(sk, sighash_32) || hashtype
VERIFY      := ed25519::VerifyDetached
ECDSA       := ⊥
SECP256K1   := ⊥

BLOCK_SERIALIZED_SIZE_LIMIT = 1_000_000
BLOCK_WEIGHT_LIMIT          = 1_000_000
BLOCK_SIGOPS_COST_LIMIT     = 20_000
LOCK_VERSION                = 2
```

Any implementation that accepts a secp256k1 ECDSA spend, or a block exceeding the locked caps, is **not** RIGOR Alpha1.

---

## 12. Status

**FROZEN** for Alpha1 release engineering.

Amendment requires a new consensus lock version and a new RFC. Silent restoration of ECDSA is a protocol-identity violation, not a bugfix.
