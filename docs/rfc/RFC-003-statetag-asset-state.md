# RFC-003: StateTag and Asset State Model

**Version:** 1.0 (Canonical Lock)
**Status:** FINAL

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

---

## 1. Abstract

This document defines the canonical StateTag model for RIGOR.

RIGOR replaces Script execution with immutable asset states.

State is represented by:
```
StateTag = TYPE || PARAMETERS
```

StateTag is:
- non-executable
- immutable
- deterministic
- finite
- closed
- machine-verifiable

This document defines:
- asset domains
- state types
- ownership rules
- transition rules
- validation rules
- rejection rules

No programmable execution is allowed.

---

## 2. Core Principle

RIGOR does not use Script.

Consensus state is represented by:
```
UTXO + Asset Domain + StateTag
```

Each output is:
```
StateOutput = {
    asset_type,
    amount,
    lock_program,
    state_tag
}
```

---

## 3. Asset Domains

Two canonical assets exist.

### 3.1 ORU
```
ASSET_ID_ORU = 0x01
Symbol = ORU
Unit = 1e8
Role = PoW energy asset
```

Used for:
- mining
- fees
- Converge
- base transfer

### 3.2 RES
```
ASSET_ID_RES = 0x02
Symbol = RES
Unit = 1e2
Role = scarce derived asset
```

Used for:
- mint
- claim
- transfer
- scarcity storage

No additional asset types are allowed.

---

## 4. StateTag Format

Canonical format:
```
StateTag = TYPE (1 byte) || PARAMS (variable)
```

### 4.1 TYPE
Defined:
```
0x01 NORMAL
0x02 LOCK
0x03 CONVERGE
0x04 MINT
0x05 CLAIM
```

Unknown TYPE:
```
MUST reject
```

### 4.2 PARAMS
Depends on TYPE.

---

## 5. Lock Program

All outputs use:
```
OP_1 <32-byte program>
```

Bytes:
```
0x51 0x20 <32 bytes>
```

This is NOT Taproot.

Properties:
- no MAST
- no tweak
- no internal key
- no control block
- no script tree

The 32-byte program is:
- resource lock identifier

---

## 6. Transaction Classification

Classification is deterministic.

### 6.1 RES Mint Transaction
A transaction is RES Mint iff:
```
input asset == ORU
AND contains RES output
AND contains valid PCRES witness
AND contains valid Converge
```

### 6.2 RES Spend Transaction
A transaction is RES Spend iff:
```
input asset == RES
```

### 6.3 ORU Transfer
A transaction is ORU Transfer iff:
```
input asset == ORU
AND no RES output
```

Any ambiguity:
```
MUST reject
```

---

## 7. State Transition Rules

### 7.1 ORU → ORU
Allowed:
```
NORMAL -> NORMAL
NORMAL -> LOCK
LOCK -> NORMAL
```

### 7.2 ORU → Converge
Allowed:
```
NORMAL -> CONVERGE
```

This destroys ORU.
No spendable output is created.

Must use:
```
OP_RETURN
```

### 7.3 ORU → RES
Allowed only for RES Mint.

Requires:
- winner tx
- valid proof
- valid root
- valid inequality

### 7.4 RES → RES
Allowed:
```
NORMAL
LOCK
CLAIM
```

### 7.5 Forbidden
Forbidden:
- RES -> ORU
- ORU -> RES (except mint)
- multiple mint
- negative amount
- unknown state

---

## 8. Converge Definition

Converge uses:
```
OP_RETURN
```

Output format:
```
OP_RETURN
CONVERGE_TAG
amount
metadata
```

Properties:
- provable destruction
- unspendable
- consensus-accounted

Minimum:
```
1 ORU
```

---

## 9. Witness Model

Witness is proof-only.
Never mode selector.

### 9.1 Mint Witness
```
[
  converge_txid
  bind_hash
  height
  target_prev
  converge_root
  proof_blob
  RESnature
]
```

### 9.2 Spend Witness
For RES spend:
```
RESnature
pubkey
```

---

## 10. Validation Engine

State engine:
```
bool VerifyState(const Tx& tx) {
    Asset a = GetInputAsset(tx);

    if (a == ORU) {
        if (IsRESMintTx(tx))
            return VerifyMint(tx);
        return VerifyORU(tx);
    }

    if (a == RES) {
        return VerifyRES(tx);
    }

    return false;
}
```

---

## 11. Forbidden

Absolutely forbidden:
- Script VM
- stack
- loops
- branch
- opcode execution
- Taproot
- MAST
- control block
- witness semantic dispatch

---

## 12. Consensus Lock

Immutable:
```
semantic = f(input_asset_domain)
```

Never:
```
semantic = f(witness)
semantic = f(script)
semantic = f(path)
```

---

## Focus: Proof / Witness

### Delete all old naming
- SIGMA Proof
- SIG Proof
- RNGProof
- PCSIG
- CalculateSigmaSelfProofHash (still exists in code, needs removal)

### New Standard Structure
**Witness (Mint Path)**
```
[
  converge_txid      (32)
  bind_hash          (32)
  height             (LE32)
  target_prev        (LE32)
  converge_root      (32)
  proof_blob         (variable)
  signature          (64)
]
```

### Verification Function Naming (Unified)
```
bool VerifyConvergeProof(...)
bool VerifySelectionProof(...)
```

### Forbidden
- res_hash
- sigma_signature

---

**END OF RFC-003**
