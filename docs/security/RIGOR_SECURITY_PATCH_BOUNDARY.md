# RIGOR Security Patch Boundary

**Document status:** Engineering security review boundary.

This document creates no consensus rule.
This document creates no upgrade mechanism.
This document creates no governance layer.

It defines how security maintenance is reviewed so that a vulnerability fix cannot change RIGOR protocol identity.

**Principle:**

> RIGOR follows Bitcoin Core security research, not Bitcoin Core protocol evolution.

RIGOR may study Bitcoin Core vulnerabilities, attack classes, and engineering mitigations.

RIGOR does not automatically inherit:

- Bitcoin Core consensus changes
- Bitcoin transaction model changes
- Bitcoin script evolution
- Bitcoin monetary policy changes
- Bitcoin wallet architecture changes
- Bitcoin relay policy changes

Bitcoin Core is a security knowledge source.

It is not the protocol authority for RIGOR.

Protocol identity is recorded in [`../protocol/RIGOR_CONSENSUS_INVARIANTS.md`](../protocol/RIGOR_CONSENSUS_INVARIANTS.md). The constants below are the existing production locks. This document checks patches against those locks. It does not amend them.

---

## 1. Purpose

RIGOR is derived from Bitcoin-style UTXO architecture.

After mainnet activation, protocol identity is frozen.

Security maintenance must improve implementation robustness without changing protocol identity.

A security patch may close a crash, a race, a resource leak, a logging abuse path, or a corrupted-state path. It must leave unchanged which blocks are valid, which transactions are valid, who owns an asset, and how money is issued.

---

## 2. Security Patch Classification Model

Every external security fix must be classified into one of four categories before code is written.

### Category A — Safe Security Adaptation

Allowed.

Examples:

- memory safety fixes
- race condition fixes
- denial-of-service prevention
- logging abuse prevention
- resource exhaustion limits
- file corruption protection
- networking hardening

Requirements. The adaptation must not change:

- valid blocks
- valid transactions
- asset ownership
- monetary rules

Example. Bitcoin fixed a network crash vulnerability. RIGOR may implement an equivalent protection when that protection leaves block acceptance and transaction acceptance unchanged.

### Category B — RIGOR-Specific Adaptation

Requires independent review.

Allowed only if rewritten for RIGOR architecture.

Examples:

- assumeutxo protection
- compact block handling
- UTXO snapshot integrity
- index integrity

Requirements. The original Bitcoin patch cannot be copied blindly.

The review must prove:

- the same security goal
- a RIGOR protocol implementation, written against RIGOR components

The Bitcoin diff is evidence of the defect. It is not the patch that lands in this tree.

### Category C — Not Applicable

Ignore.

Bitcoin fixes related to components RIGOR does not contain:

- Script interpreter
- Taproot
- Schnorr
- Miniscript
- MuSig2
- package relay
- cluster mempool

Reason. No equivalent attack surface exists. These fixes are not deferred work.

### Category D — Forbidden Protocol Modification

Never import.

Any change affecting the lists below creates a different protocol.

#### Monetary Rules

Forbidden:

- subsidy changes
- halving logic
- supply calculation
- inflation schedule
- reserve issuance
- burn rules

#### Asset Model

Forbidden:

- adding asset types
- changing ORU meaning
- changing RES meaning
- allowing RES → ORU
- adding an external asset registry

#### Script / Opcode Model

Forbidden:

- changing opcode meaning
- adding opcode behavior
- modifying `OP_CONVERGE`
- changing template classification

#### Signature Model

Forbidden:

- replacing Ed25519
- adding ECDSA
- adding Schnorr
- adding alternate witness authorization

#### Transaction Model

Forbidden:

- account model
- smart contract execution
- external state
- non-UTXO consensus state

#### Proof-of-Work

Forbidden:

- changing the PoW hash
- changing the domain separator
- changing the header format
- changing target calculation

---

## 3. Frozen Consensus Boundary

RIGOR consensus identity consists of the following.

### Monetary

- ORU fixed subsidy: `ORU_FLAT_BLOCK_SUBSIDY` = 128 ORU (`128 * COIN` = 12,800,000,000 tharns) at every height ≥ 1. Genesis subsidy is 0. There is no halving term.
- RES derived issuance: RES is created by destroying ORU. Coinbase does not create RES.
- `RES_MAX_SUPPLY` = 419,430,400,000,000 atoms = 2^22 RES.
- ORU → RES only.

### Transaction

- UTXO only.
- Four accepted templates:
  - Witness v0 / 20 bytes: spendable ORU. Version byte `0x00`.
  - Witness v1 / 32 bytes: spendable RES. Version byte `0x51`.
  - `HASH160 || OP_CONVERGE`: ORU destroy. Marker byte `0xbd`.
  - zero-value `OP_RETURN`: data. Marker byte `0x6a`.
- `OP_CONVERGE`.

### Authorization

- Ed25519. Public key is 32 bytes. Signature is 64 bytes.
- `RGRORU01` for ORU spends.
- `RGRRES01` for RES spends.

### Proof-of-Work

- `SHA3-256("RGRPOW01" || header_80)`.
- Header length is 80 bytes. Domain tag length is 8 bytes. Preimage length is 88 bytes.
- Block identity remains `SHA256d(header_80)`.

### Genesis

- Mainnet genesis identity: `e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468`.

Any modification of an item above is:

> Protocol replacement, not security upgrade.

---

## 4. Security Review Requirement

Every future security patch must include this analysis before merge.

```
Security Review Record

Issue:
Affected component:
Bitcoin reference:
Attack prevented:

RIGOR equivalent component:
Same attack surface:
YES / NO

Consensus impact:
YES / NO

Protocol boundary affected:
YES / NO

Decision:
APPLY
ADAPT
IGNORE
FORBIDDEN
```

| Decision | Category | When it is used |
|----------|----------|-----------------|
| `APPLY` | A | Equivalent protection. Valid blocks, valid transactions, asset ownership, and monetary rules stay the same. |
| `ADAPT` | B | Same security goal, rewritten for RIGOR. The Bitcoin patch is not copied. |
| `IGNORE` | C | No equivalent attack surface. |
| `FORBIDDEN` | D | The change would alter consensus identity. |

No security patch may merge without this analysis. A record with a blank field is incomplete.

`Consensus impact: YES` or `Protocol boundary affected: YES` requires decision `FORBIDDEN`. This document does not authorize a protocol change.

---

## 5. Required Code Review Rules

Before merging a security patch, the reviewer must verify that the patch contains no changes to:

- `core/src/consensus/`
- `core/src/validation.cpp`
- `core/src/script/rigor_classify.cpp`
- `core/src/script/predicate_verify.cpp`
- `core/src/consensus/tx_verify.cpp`
- `core/src/pow.cpp`

unless the change is explicitly approved as a protocol change. That approval is outside this document.

`core/src/consensus/tx_verify.cpp` is listed again because it is the transaction-admission path. A reviewer opens it even when the rest of `core/src/consensus/` is untouched.

The suite `security_boundary_tests` (`core/src/test/security_boundary_tests.cpp`) pins the documentation constants: ORU subsidy, `RES_MAX_SUPPLY`, the `OP_CONVERGE` byte, witness versions, signature domains, and the PoW domain tag. A security patch that moves those constants fails the suite. A passing suite does not replace the file review above.

---

## Final statement

RIGOR does not follow Bitcoin Core protocol evolution.

RIGOR studies Bitcoin Core security research.

Security improvements may strengthen implementation.

They must not redefine the protocol.

A change to consensus identity is not a patch.

It is a different network.
