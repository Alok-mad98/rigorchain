# RIGOR Script Structure White Paper

**Version**: 1.0  
**Date**: 2026-04-27  
**Purpose**: Global script structure specification (disassembly level)

---

## Table of Contents

1. [Data Structure Definitions (IDL Layer)](#data-structure-definitions-idl-layer)
2. [Execution Logic and Control Flow (Logic Layer)](#execution-logic-and-control-flow-logic-layer)
3. [Serialization Specification (Storage Layer)](#serialization-specification-storage-layer)
4. [RIGOR-Specific Script Types](#rigor-specific-script-types)

---

## Data Structure Definitions (IDL Layer)

### 1. Core Script Structures

#### 1.1 CScript (Script Container)

**Definition location**: `core/src/script/script.h:420-570`

**C++ definition**:
```cpp
typedef prevector<28, unsigned char> CScriptBase;

class CScript : public CScriptBase {
protected:
    CScript& push_int64(int64_t n);
public:
    SERIALIZE_METHODS(CScript, obj) { READWRITEAS(CScriptBase, obj); }
    CScript& operator<<(opcodetype opcode);
    CScript& operator<<(const std::vector<unsigned char>& b);
    bool GetOp(const_iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>& vchRet) const;
    bool IsWitnessProgram(int& version, std::vector<unsigned char>& program) const;
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| (inherited from CScriptBase) | prevector<28, unsigned char> | N/A | Variable, max 10000 bytes | Script byte stream container; uses pre-allocated vector optimized for small scripts |

**Memory layout**:
- Internally uses `prevector<28, unsigned char>`; small scripts (<=28 bytes) use stack storage, large scripts use heap allocation
- Serialized directly as byte stream with no extra header

---

#### 1.2 CScriptWitness (Witness Data)

**Definition location**: `core/src/script/script.h:572-586`

**C++ definition**:
```cpp
struct CScriptWitness {
    std::vector<std::vector<unsigned char>> stack;
    bool IsNull() const { return stack.empty(); }
    void SetNull() { stack.clear(); stack.shrink_to_fit(); }
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| stack | vector<vector<unsigned char>> | N/A | Variable | Witness stack; each element is a byte vector |

**Serialization specification**:
- In `CTxIn` controlled by `SERIALIZE_TRANSACTION_NO_WITNESS` flag for whether to serialize
- Serialization format: CompactSize(stack.size()) + CompactSize of each element + data

---

#### 1.3 CTxOut (Transaction Output)

**Definition location**: `core/src/primitives/transaction.h:167-205`

**C++ definition**:
```cpp
class CTxOut {
public:
    CAmount nValue;
    CScript scriptPubKey;
    
    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);
    SERIALIZE_METHODS(CTxOut, obj) { READWRITE(obj.nValue, obj.scriptPubKey); }
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| nValue | int64_t (CAmount) | Little Endian | 8 bytes | Output amount (satoshis) |
| scriptPubKey | CScript | N/A | Variable, max 10000 bytes | Locking script; defines spending conditions |

**Serialization specification**:
```
[8 bytes] nValue (Little Endian)
[variable] scriptPubKey (CScript serialization)
```

---

#### 1.4 Coin (UTXO Entry)

**Definition location**: `core/src/coins.h:49-118`

**C++ definition**:
```cpp
class Coin {
public:
    CTxOut out;
    unsigned int fCoinBase : 1;
    uint32_t nHeight : 31;
    uint32_t nFlags{0};
    
    template<typename Stream>
    void Serialize(Stream &s) const {
        uint32_t code = nHeight * uint32_t{2} + fCoinBase;
        ::Serialize(s, VARINT(code));
        ::Serialize(s, Using<TxOutCompression>(out));
        ::Serialize(s, VARINT(nFlags));
    }
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| out | CTxOut | N/A | Compressed variable length | Unspent transaction output |
| fCoinBase | uint32_t:1 | N/A | 1 bit | Whether this is a coinbase transaction output |
| nHeight | uint32_t:31 | N/A | 31 bits | Block height containing this output |
| nFlags | uint32_t | Little Endian | 4 bytes | Extended flags (UTXO set only, non-consensus) |

**LevelDB serialization format**:
```
[VARINT] code = (nHeight << 1) | fCoinBase
[variable] TxOutCompression(out)  // compressed CTxOut
[VARINT] nFlags (optional, only when non-zero)
```

**TxOutCompression** (compressor.h:113-119):
```
[VARINT] CompressAmount(nValue)
[variable] ScriptCompression(scriptPubKey)
```

---

### 2. Rigor-Specific Structures

#### 2.1 ConvergeProof (RES Mint / Claim Witness Proof)

**Definition location**: `core/src/consensus/res_proof.h` (`ConvergeProof`)

**C++ definition (simplified)**:
```cpp
struct ConvergeProof {
    uint256 convergence_txid;
    uint64_t convergence_amount_sat{0};
    uint256 bind_hash;
    uint64_t height{0};
    uint32_t target_prev{0};
    uint256 convergence_root{};
    std::array<unsigned char, ed25519::SIGNATURE_SIZE> sig{};
    std::array<unsigned char, ed25519::PUBKEY_SIZE> pubkey{};
    bool has_sig{false};
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| convergence_txid | uint256 | Little Endian | 32 bytes | Convergence transaction ID |
| convergence_amount_sat | uint64_t | Little Endian | 8 bytes | Convergence amount (satoshis) |
| bind_hash | uint256 | Little Endian | 32 bytes | Bind hash |
| height | uint64_t | Little Endian | 8 bytes | Block height |
| target_prev | uint32_t | Little Endian | 4 bytes | Previous target value |
| convergence_root | uint256 | Little Endian | 32 bytes | Convergence root hash |
| sig | array<unsigned char, 64> | N/A | 64 bytes | Ed25519 signature |
| pubkey | array<unsigned char, 32> | N/A | 32 bytes | Ed25519 public key |
| has_sig | bool | N/A | 1 byte | Whether signature is included |

**Witness stack layout**:
```
stack[0]: convergence_txid (32 bytes)
stack[1]: convergence_amount_sat (8 bytes, Little Endian)
stack[2]: bind_hash (32 bytes)
stack[3]: height (8 bytes, Little Endian)
stack[4]: target_prev (4 bytes, Little Endian)
stack[5]: convergence_root (32 bytes)
stack[6]: sig (64 bytes, optional)
stack[7]: pubkey (32 bytes, optional)
stack[8]: leaf_script (variable, optional)
stack[9]: control_block (variable, optional)
```

---

#### 2.2 ConvergenceCommitment (Convergence Commitment)

**Definition location**: `core/src/consensus/convergence_commitment.h:47-59`

**C++ definition**:
```cpp
struct ConvergenceDrawCandidates {
    uint256 hash{};
    uint256 root{};
};

struct ConvergenceCommitment {
    static constexpr uint8_t EXPECTED_VERSION = 2;
    uint8_t m_version{EXPECTED_VERSION};
    uint64_t m_cumulative_convergence{0};
    ConvergenceDrawCandidates m_draw_candidates{};
};
```

**Field table**:

| Field Name | Data Type | Byte Order | Length Limit | Description |
|-----------|-----------|------------|-------------|-------------|
| m_version | uint8_t | N/A | 1 byte | Version number (must be 2) |
| m_cumulative_convergence | uint64_t | Little Endian | 8 bytes | Cumulative convergence amount |
| m_draw_candidates.hash | uint256 | Little Endian | 32 bytes | Per-tx ConvergenceDrawCandidates result (runtime only) |
| m_draw_candidates.root | uint256 | Little Endian | 32 bytes | Candidate set Merkle root (on-chain data) |

**RIGOR_MARKER_RETURN script format**:
```
RIGOR_MARKER_RETURN (0x6a)
[CompactSize] data length
[1 byte] "CVGE" (Magic)
[1 byte] version number (2)
[8 bytes] m_cumulative_convergence (Little Endian)
[32 bytes] m_draw_candidates.root (Little Endian)
```

---

### 3. Opcode Enumeration

**Definition location**: `core/src/script/script.h:79-219`

**Key opcode table**:

| Opcode | Hex | Type | Description |
|--------|-----|------|-------------|
| RIGOR_PUSH_0 | 0x00 | Constant | Push empty vector |
| RIGOR_PUSHDATA1 | 0x4C | Data push | Followed by 1-byte length |
| RIGOR_PUSHDATA2 | 0x4D | Data push | Followed by 2-byte length |
| RIGOR_PUSHDATA4 | 0x4E | Data push | Followed by 4-byte length |
| RIGOR_RESERVED_LEGACY_1NEGATE | 0x4F | Constant | Push -1 |
| RIGOR_PUSH_1..RIGOR_PUSH_16 | 0x51-0x60 | Constant | Push 1-16 |
| RIGOR_RESERVED_LEGACY_DUP | 0x76 | Stack op | Duplicate top stack element |
| RIGOR_RESERVED_LEGACY_HASH160 | 0xA9 | Crypto | SHA256 + RIPEMD160 |
| RIGOR_RESERVED_LEGACY_EQUAL | 0x87 | Compare | Equality check |
| RIGOR_RESERVED_LEGACY_EQUALVERIFY | 0x88 | Compare | Equality check and verify |
| RIGOR_RESERVED_LEGACY_CHECKSIG | 0xAC | Crypto | ECDSA signature verification |
| RIGOR_RESERVED_LEGACY_CHECKMULTISIG | 0xAE | Crypto | Multi-signature verification |
| RIGOR_MARKER_RETURN | 0x6A | Control flow | Mark data carrier output |

---

## Execution Logic and Control Flow (Logic Layer)

### 1. Script Interpreter Architecture

#### 1.1 Execution Flow Overview

```mermaid
graph TD
    A[Start] --> B[Pre-checks]
    B --> C{Witness version?}
    C -->|v0| D[Witness v0 execution]
    C -->|v1| E[Rigor Witness v1 (ORU/RES routing)]
    C -->|other| F[Legacy script execution]
    D --> G[Stack initialization]
    E --> G
    F --> G
    G --> H[Script parsing loop]
    H --> I{Opcode type?}
    I -->|Push| J[Data push to stack]
    I -->|Arithmetic| K[Top-of-stack computation]
    I -->|Crypto| L[Signature verification]
    I -->|Control flow| M[Conditional branch]
    J --> H
    K --> H
    L --> N{Verification success?}
    N -->|Yes| H
    N -->|No| O[Script failure]
    M --> H
    H --> P{Script end?}
    P -->|No| H
    P -->|Yes| Q{Top-of-stack true?}
    Q -->|Yes| R[Script success]
    Q -->|No| O
    O --> S[Return failure]
    R --> T[Return success]
```

#### 1.2 Pre-checks

**Check items**:

1. **Script size limits**:
   - Max script length: 10000 bytes
   - Max single push element: 520 bytes
   - Max stack depth: 1000 elements

2. **Opcode counting**:
   - Max non-push opcodes: 201
   - Max multi-sig pubkeys: 20

3. **Witness version check**:
   ```cpp
   bool IsWitnessProgram(int& version, std::vector<unsigned char>& program) const {
       if (size() < 4 || (*begin()) != RIGOR_PUSH_1) return false;
       int witnessversion = (*begin()) - RIGOR_PUSH_1;
       // parse program content
   }
   ```

4. **Witness v1 (Rigor) special checks**:
   - CVG2 Convergence detection: first 4 bytes are "CVG2" (asset domain treated as ORU)
   - Native RES detection: 32-byte program, not CVG2; contains `RES2` wire prefix or historical unmarked layout (asset domain is RES)

---

### 2. Standard Script Type Execution

#### 2.1 P2PKH (Pay-to-Public-Key-Hash)

**Script structure**:
```
scriptPubKey: RIGOR_RESERVED_LEGACY_DUP RIGOR_RESERVED_LEGACY_HASH160 <pubKeyHash> RIGOR_RESERVED_LEGACY_EQUALVERIFY RIGOR_RESERVED_LEGACY_CHECKSIG
scriptSig: <signature> <pubkey>
```

**Execution flow**:

```
Step  Stack state              Operation
----  ------------------  ------------------------------------
Init  []                  Initialize empty stack
1     []                  Execute scriptSig
2     [<signature>]       Push signature
3     [<signature>, <pubkey>]  Push pubkey
4     [<signature>, <pubkey>]  Execute scriptPubKey
5     [<signature>, <pubkey>, <pubkey>]  RIGOR_RESERVED_LEGACY_DUP (duplicate top)
6     [<signature>, <pubkey>, <pubkey>, <pubkey>]  RIGOR_RESERVED_LEGACY_DUP (repeated)
7     [<signature>, <pubkey>, <pubkey>]  RIGOR_RESERVED_LEGACY_HASH160 (compute pubkey hash)
8     [<signature>, <pubkey>, <pubkeyHash>]  Push hash from scriptPubKey
9     [<signature>, <pubkey>, <pubkeyHash>, <pubKeyHash>]  RIGOR_RESERVED_LEGACY_EQUALVERIFY
10    [<signature>, <pubkey>]  Terminate if verification fails
11    [true/false]         RIGOR_RESERVED_LEGACY_CHECKSIG (verify signature)
12    [true/false]         Top-of-stack true means success
```

**Signature verification steps**:
1. Pop signature and pubkey from top of stack
2. Compute signature hash: `SignatureHash(scriptCode, tx, nIn, nHashType, amount, sigversion)`
3. Verify signature validity against hash using pubkey
4. Script terminates immediately if verification fails

---

#### 2.2 P2SH (removed)

P2SH is **not** a RIGOR consensus lock. `VerifyScript` rejects `RIGOR_RESERVED_LEGACY_HASH160 <20> RIGOR_RESERVED_LEGACY_EQUAL` scriptPubKeys with `SCRIPT_ERR_RIGOR_SCRIPT_TYPE`. `SCRIPT_VERIFY_P2SH` remains an unused enum bit; it is not set by `GetBlockScriptFlags` and does not enable redeem-script execution. Address encoding and descriptors do not expose `ScriptHash` / `sh()`.

---

#### 2.3 Witness v0 P2WPKH

**Script structure**:
```
scriptPubKey: RIGOR_PUSH_0 <pubKeyHash>
witness: <signature> <pubkey>
```

**Execution flow**:

```
Step  Stack state              Operation
----  ------------------  ------------------------------------
Init  []                  Initialize empty stack
1     []                  Load witness stack
2     [<signature>]       witness[0]
3     [<signature>, <pubkey>]  witness[1]
4     [<signature>, <pubkey>]  Execute implicit script: RIGOR_RESERVED_LEGACY_DUP RIGOR_RESERVED_LEGACY_HASH160 <pubKeyHash> RIGOR_RESERVED_LEGACY_EQUALVERIFY RIGOR_RESERVED_LEGACY_CHECKSIG
5     ...                 Same as P2PKH execution flow
```

**Key characteristics**:
- scriptSig must be empty
- Witness data loaded from separate witness field
- Implicit script built dynamically at execution time
- CLEANSTACK rule enforced

---

### 3. Rigor-Specific Script Execution

#### 3.1 Witness v1 CVG2 Convergence

**Script structure**:
```
scriptPubKey: RIGOR_PUSH_1 <32-byte program>
Program format: "CVG2" (4 bytes) || amount (8 bytes BE) || padding (20 bytes)
```

**Execution flow**:

```
Step  Stack state              Operation
----  ------------------  ------------------------------------
Init  []                  Initialize empty stack
1     []                  Detect Witness v1
2     []                  Check first 4 bytes are "CVG2"
3     []                  Extract convergence amount (big-endian)
4     []                  Mark as Convergence output
5     []                  Reject spending (bad-txns-spend-convergence)
```

**Consensus rules**:
- Convergence outputs never enter UTXO set
- Only ORU domain transactions allowed to create
- CVG2 in coinbase transaction used for ConvergenceRoot commitment

---

#### 3.2 Witness v1 Native RES (Ed25519)

**Script structure**:
```
scriptPubKey: RIGOR_PUSH_1 <32-byte Ed25519 pubkey>
```

**Key-path spend flow**:

```
Step  Stack state              Operation
----  ------------------  ------------------------------------
Init  []                  Initialize empty stack
1     []                  Detect Witness v1
2     []                  Check program is 32 bytes and not CVG2 convergence prefix
3     []                  Parse as Ed25519 pubkey
4     []                  Load witness stack
5     [<signature>]       witness[0] (64-byte Ed25519 signature)
6     [true/false]         Ed25519 signature verification
7     [true/false]         Spend succeeds if verification passes
```

**Script-path spend flow (Mint/Claim)**:

```
Step  Stack state              Operation
----  ------------------  ------------------------------------
Init  []                  Initialize empty stack
1     []                  Detect Witness v1
2     []                  Parse ConvergeProof structure
3     []                  Verify Convergence binding
4     []                  Verify PCORB proof
5     []                  Check input asset domain (ORU=Mint, RES=Claim)
6     [true/false]         Spend succeeds if verification passes
```

**ConvergeProof verification steps**:
1. Parse first 6 witness stack elements as proof fields
2. Verify corresponding Convergence output exists for `convergence_txid`
3. Verify `convergence_amount_sat` matches output amount
4. Compute bind digest (see `res_proof.cpp` / `draw_digest.cpp`)
5. Verify signature using Ed25519
6. Verify `convergence_root` matches block ConvergenceRoot commitment

---

### 4. Stack Operation Specifications

#### 4.1 Data Push

**Push rules**:

| Data length | Opcode | Format |
|------------|--------|--------|
| 0 bytes | RIGOR_PUSH_0 | 0x00 |
| 1-75 bytes | [length] | 0x01-0x4B |
| 76-255 bytes | RIGOR_PUSHDATA1 [length] | 0x4C [1-byte length] |
| 256-65535 bytes | RIGOR_PUSHDATA2 [length] | 0x4D [2-byte LE length] |
| 65536+ bytes | RIGOR_PUSHDATA4 [length] | 0x4E [4-byte LE length] |

**Minimum encoding rules**:
- 1-16: must use RIGOR_PUSH_1-RIGOR_PUSH_16
- -1: must use RIGOR_RESERVED_LEGACY_1NEGATE
- 0: must use RIGOR_PUSH_0
- Other numbers: must use shortest byte representation

#### 4.2 Arithmetic Operations

**CScriptNum encoding**:
- Uses little-endian
- Most significant bit is sign bit
- Must be minimally encoded (no leading zero bytes unless sign bit needed)

**Operation limits**:
- All arithmetic operations limited to 4-byte integer range
- Results may overflow, but overflow values cannot be used in subsequent arithmetic operations

---

### 5. Signature Verification Logic

#### 5.1 ECDSA Signature Verification

**Verification steps**:

```cpp
bool CheckECDSASignature(const std::vector<unsigned char>& scriptSig,
                         const std::vector<unsigned char>& vchPubKey,
                         const CScript& scriptCode,
                         SigVersion sigversion) {
    // 1. Parse signature
    CPubKey pubkey(vchPubKey);
    if (!pubkey.IsValid()) return false;
    
    // 2. Verify signature encoding (DER format)
    if (!IsValidSignatureEncoding(scriptSig)) return false;
    
    // 3. Compute signature hash
    uint256 sighash = SignatureHash(scriptCode, txTo, nIn, nHashType, 
                                     amount, sigversion, cache);
    
    // 4. Verify signature
    return pubkey.Verify(sighash, scriptSig);
}
```

**Signature hash computation**:
- BASE: entire transaction hash
- WITNESS_V0: BIP143 signature hash
- Includes scriptCode, amount, sequence, etc.

#### 5.2 Ed25519 Signature Verification

**Verification steps**:

```cpp
bool CheckEd25519Signature(const std::vector<unsigned char>& scriptSig,
                           const std::vector<unsigned char>& vchPubKey,
                           const CScript& scriptCode,
                           SigVersion sigversion) {
    // 1. Check pubkey length (32 bytes)
    if (vchPubKey.size() != ed25519::PUBKEY_SIZE) return false;
    
    // 2. Check signature length (64 bytes)
    if (scriptSig.size() != ed25519::SIGNATURE_SIZE) return false;
    
    // 3. Compute signature hash
    uint256 sighash = SignatureHash(scriptCode, txTo, nIn, nHashType,
                                     amount, sigversion, cache);
    
    // 4. Ed25519 verification
    return ed25519_verify(vchPubKey.data(), sighash.data(), 
                          sighash.size(), scriptSig.data());
}
```

**Key differences**:
- Ed25519 uses deterministic signatures
- No DER encoding check needed
- Signature fixed at 64 bytes

---

## Serialization Specification (Storage Layer)

### 1. Transaction Serialization

#### 1.1 Base Transaction Format

**Serialization format**:
```
[4 bytes] nVersion (Little Endian)
[variable] vin (CompactSize + CTxIn array)
[variable] vout (CompactSize + CTxOut array)
[4 bytes] nLockTime (Little Endian)
```

**CTxIn serialization**:
```
[32 bytes] prevout.hash (Little Endian)
[4 bytes] prevout.n (Little Endian)
[variable] scriptSig (CScript serialization)
[4 bytes] nSequence (Little Endian)
```

#### 1.2 Witness Extended Format

**Transaction with witness**:
```
[4 bytes] nVersion (Little Endian)
[1 byte] marker (0x00)
[1 byte] flags (0x01)
[variable] vin (CompactSize + CTxIn array)
[variable] vout (CompactSize + CTxOut array)
[variable] witness (CScriptWitness per input)
[4 bytes] nLockTime (Little Endian)
```

**CScriptWitness serialization**:
```
[CompactSize] stack.size()
For each element:
    [CompactSize] element.size()
    [variable] element.data()
```

---

### 2. UTXO Serialization

#### 2.1 LevelDB Storage Format

**Key format**:
```
[32 bytes] COutPoint hash (txid + n combination)
```

**Value format** (Coin):
```
[VARINT] code = (nHeight << 1) | fCoinBase
[variable] TxOutCompression(out)
[VARINT] nFlags (optional, only when non-zero)
```

**TxOutCompression detail**:
```
[VARINT] CompressAmount(nValue)
    - 0: 0x00
    - 1-16: 0x01-0x10
    - others: variable-length encoding

[variable] ScriptCompression(scriptPubKey)
    - P2PKH (21 bytes): 0x00 + 20-byte hash
    - P2SH (21 bytes): 0x01 + 20-byte hash
    - Compressed pubkey (33 bytes): 0x08-0x09 + 32-byte pubkey
    - Others: CompactSize + raw script
```

---

### 3. Undo Data Serialization

#### 3.1 CTxUndo Format

**Definition location**: `core/src/undo.h:61-68`

**Serialization format**:
```
[CompactSize] vprevout.size()
For each element:
    [VARINT] code = (nHeight << 1) | fCoinBase
    [1 byte] dummy (only when nHeight > 0)
    [variable] TxOutCompression(out)
```

**Key constraints**:
- nFlags not serialized (Undo format excludes extended flags)
- dummy field for backward compatibility
- No dummy field when nHeight = 0

---

### 4. Serialization Examples

#### 4.1 P2PKH Script Example

**Script**: `RIGOR_RESERVED_LEGACY_DUP RIGOR_RESERVED_LEGACY_HASH160 <pubKeyHash> RIGOR_RESERVED_LEGACY_EQUALVERIFY RIGOR_RESERVED_LEGACY_CHECKSIG`

**Byte stream**:
```
76 A9 14 <20-byte pubKeyHash> 88 AC
```

**Byte-by-byte explanation**:
```
76  = RIGOR_RESERVED_LEGACY_DUP
A9  = RIGOR_RESERVED_LEGACY_HASH160
14  = Push 20-byte data
    = pubKeyHash (20 bytes)
88  = RIGOR_RESERVED_LEGACY_EQUALVERIFY
AC  = RIGOR_RESERVED_LEGACY_CHECKSIG
```

**Complete scriptPubKey** (33 bytes):
```
76 A9 14 A3 3F A8 5C 7B 2E 3B 1A 2C 5F 8A 9D 1E 2F 3A 4B 5C 6D 7E 8F 9A 0B 1C 2D 3E 4F 5A 6B 7C 88 AC
```

---

#### 4.2 Witness v0 P2WPKH Example

**Script**: `RIGOR_PUSH_0 <pubKeyHash>`

**Byte stream**:
```
00 14 <20-byte pubKeyHash>
```

**Byte-by-byte explanation**:
```
00  = RIGOR_PUSH_0 (Witness version 0)
14  = Push 20-byte data
    = pubKeyHash (20 bytes)
```

**Complete scriptPubKey** (22 bytes):
```
00 14 A3 3F A8 5C 7B 2E 3B 1A 2C 5F 8A 9D 1E 2F 3A 4B 5C 6D 7E 8F 9A 0B 1C 2D 3E 4F 5A 6B 7C
```

---

#### 4.3 Witness v1 Native RES (Ed25519 Program) Example

**Script**: `RIGOR_PUSH_1 <32-byte Ed25519 pubkey>`

**Byte stream**:
```
51 20 <32-byte pubkey>
```

**Byte-by-byte explanation**:
```
51  = RIGOR_PUSH_1 (Witness version 1)
20  = Push 32-byte data
    = Ed25519 pubkey (32 bytes)
```

**Complete scriptPubKey** (34 bytes):
```
51 20 1A 2B 3C 4D 5E 6F 7A 8B 9C 0D 1E 2F 3A 4B 5C 6D 7E 8F 9A 0B 1C 2D 3E 4F 5A 6B 7C 8D 9E 0F 1A 2B 3C 4D 5E
```

---

#### 4.4 Witness v1 CVG2 Convergence Example

**Script**: `RIGOR_PUSH_1 <32-byte CVG2 program>`

**Program format**: "CVG2" (4 bytes) || amount (8 bytes BE) || padding (20 bytes)

**Byte stream**:
```
51 20 43 56 47 32 <8-byte amount BE> <20-byte padding>
```

**Byte-by-byte explanation**:
```
51  = RIGOR_PUSH_1 (Witness version 1)
20  = Push 32-byte data
43  = 'C'
56  = 'V'
47  = 'G'
32  = '2'
    = amount (8 bytes, Big Endian)
    = padding (20 bytes, all zeros)
```

**Complete scriptPubKey** (34 bytes):
```
51 20 43 56 47 32 00 00 00 00 00 00 0E D8 90 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

**Amount parsing**: 0x0000000000000ED890 = 1,000,000,000 satoshis (10 BTC)

---

#### 4.5 ConvergenceCommitment RIGOR_MARKER_RETURN Example

**Script**: `RIGOR_MARKER_RETURN <CONV data>`

**Data format**: "CVGE" (4 bytes) || version (1 byte) || cumulative (8 bytes LE) || root (32 bytes LE)

**Byte stream**:
```
6A 33 43 56 47 45 02 <8-byte cumulative LE> <32-byte root LE>
```

**Byte-by-byte explanation**:
```
6A  = RIGOR_MARKER_RETURN
33  = Push 51-byte data
43  = 'C'
56  = 'V'
47  = 'G'
45  = 'E'
02  = Version 2
    = m_cumulative_convergence (8 bytes, Little Endian)
    = m_draw_candidates.root (32 bytes, Little Endian)
```

**Complete scriptPubKey** (53 bytes):
```
6A 33 43 56 47 45 02 00 00 00 00 00 00 0E D8 90 1A 2B 3C 4D 5E 6F 7A 8B 9C 0D 1E 2F 3A 4B 5C 6D 7E 8F 9A 0B 1C 2D 3E 4F 5A 6B 7C 8D 9E 0F 1A 2B 3C 4D 5E
```

---

## RIGOR-Specific Script Types

### 1. Script Type Classification

**TxoutType enumeration** (core/src/script/standard.h):

| Type | Value | Script Format | Asset Type |
|------|-------|---------------|------------|
| WITNESS_V0_KEYHASH | 7 | RIGOR_PUSH_0 <20 bytes> | ASSET_ORU |
| WITNESS_V0_SCRIPTHASH | — | RIGOR_PUSH_0 <32 bytes> | **Not a consensus type** (P2WSH removed) |
| WITNESS_V1_RES | 9 | RIGOR_PUSH_1 <32-byte native Ed25519 pubkey> | ASSET_RES |
| RES_MINT | — | `<maturity> <20-byte pkh> RIGOR_MARKER_RES_MINT` (coinbase) | ASSET_RES |
| CONVERGE | — | `<20-byte hash> RIGOR_MARKER_CONVERGE` | ASSET_ORU (burn lock) |
| NULL_DATA | 4 | RIGOR_MARKER_RETURN <data> | ASSET_NONE |
| *(unrecognized)* | — | anything else | **ASSET_INVALID** (consensus reject) |

### 2. Asset Type Inference

**AssetTypeFromScriptPubKey logic** (authoritative C++: `script/rigor_classify.cpp`):

```cpp
AssetType AssetTypeFromScriptPubKey(const CScript& scriptPubKey) {
    switch (ClassifyRigorTxout(scriptPubKey, sols)) {
    case TxoutType::WITNESS_V0_KEYHASH:
    case TxoutType::CONVERGE:
        return ASSET_ORU;
    case TxoutType::WITNESS_V1_RES:
    case TxoutType::RES_MINT:
        return ASSET_RES;
    case TxoutType::NULL_DATA:
        return ASSET_NONE;      // push-only RIGOR_MARKER_RETURN, nValue must be 0
    default:
        return ASSET_INVALID;   // fail-closed: never ORU, never NONE
    }
}
```

Unrecognized scripts are **`ASSET_INVALID`**, not `ASSET_ORU` and not `ASSET_NONE`. `ASSET_NONE` is only a legal zero-value RIGOR_MARKER_RETURN. `CheckTransaction` rejects `ASSET_INVALID` with `bad-rigor-script-type`.

### 3. Constant Definitions

**Witness program sizes** (core/src/script/interpreter.h):

| Constant | Value | Description |
|----------|-------|-------------|
| WITNESS_V0_KEYHASH_SIZE | 20 | v0 P2WPKH program size |
| WITNESS_V0_SCRIPTHASH_SIZE | 32 | Historic P2WSH program size (not a consensus output) |
| WITNESS_V1_RES_SIZE | 32 | v1 native RES program size |

On-chain ASCII tags (consensus bytes; do not confuse with product names)

| Tag | Bytes | Purpose |
|-----|-------|---------|
| RIGR | `RIGR` | Coinbase `RIGOR_MARKER_RETURN` compact draw-target prefix (`rigor_tags.h`; **not** the five-byte string `RIGOR`) |
| CVG2 | `CVG2` | Retired witness-v1 Convergence prefix. Not creatable; live Convergence is `RIGOR_MARKER_CONVERGE`. |
| RES2 | `RES2` | Retired witness-v1 RES wire prefix. Not creatable; native RES program is the 32-byte Ed25519 pubkey. |

---

## Appendix

### A. Byte Order Conventions

| Data Type | Byte Order | Description |
|-----------|-----------|-------------|
| uint256 | Little Endian | Hash values |
| uint64_t (amount) | Little Endian | Amount (satoshis) |
| uint64_t (height) | Little Endian | Block height |
| uint32_t (sequence) | Little Endian | Transaction sequence number |
| uint32_t (version) | Little Endian | Transaction version |
| Convergence amount | Big Endian | Amount in CVG2 program |

### B. Variable-Length Encoding

**CompactSize format**:

| Value range | Encoding format |
|-------------|----------------|
| 0-252 | 1 byte |
| 253-65535 | 0xFD + 2 bytes Little Endian |
| 65536-4294967295 | 0xFE + 4 bytes Little Endian |
| 4294967296+ | 0xFF + 8 bytes Little Endian |

### C. Error Codes

**Script verification errors** (core/src/script/script_error.h):

| Error Code | Description |
|-----------|-------------|
| SCRIPT_ERR_OK | Verification success |
| SCRIPT_ERR_INVALID_STACK_OPERATION | Invalid stack operation |
| SCRIPT_ERR_INVALID_OPCODE | Invalid opcode |
| SCRIPT_ERR_UNBALANCED_CONDITIONAL | Unbalanced conditional branch |
| SCRIPT_ERR_OP_RETURN | RIGOR_MARKER_RETURN execution |
| SCRIPT_ERR_SIG_PUSHONLY | scriptSig must be push-only |
| SCRIPT_ERR_SIG_HIGH_S | S value too high |
| SCRIPT_ERR_SIG_DERSIG | Non-DER encoded signature |
| SCRIPT_ERR_MINIMALDATA | Non-minimal encoding |
| SCRIPT_ERR_WITNESS_MISMATCHED | Witness mismatch |
| SCRIPT_ERR_WITNESS_UNEXPECTED | Unexpected witness |

---

**End of document**

*This document describes the complete specification of the Rigor chain script system, including data structures, execution logic, and serialization specifications. All definitions are based on actual source code implementation to ensure complete consistency with the consensus layer.*
