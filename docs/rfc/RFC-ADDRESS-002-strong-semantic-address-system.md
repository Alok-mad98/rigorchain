# RFC-ADDRESS-002: Strong Semantic Address System

**Version:** 1.0
**Status:** FINAL LOCK
**Layer:** Wallet and Address Specification

---

## 1. Abstract

RFC-ADDRESS-002 defines the final address specification for the RIGOR blockchain system. This specification establishes a strong semantic binding between witness versions and asset domains, ensuring that addresses serve as immutable safety boundaries. The system prevents cross-asset errors through deterministic validation rules and clear implementation requirements for wallet software.

---

## 2. Final Address Specification

### 2.1 ORU (Base Asset)

**Address Type:** V0
**Encoding:** Bech32 (BIP-173)
**HRP:** rg (chain-defined)
**ScriptPubKey:** OP_0 <20|32 bytes>

### 2.2 RES (Derived Asset)

**Address Type:** V1
**Encoding:** Bech32m (BIP-350)
**HRP:** rg (chain-defined)
**ScriptPubKey:** OP_1 <32 bytes>

### 2.3 Core Semantic Binding

The following rules MUST be enforced at consensus level:

```
IF witness_version == 0 THEN asset = ORU
IF witness_version == 1 THEN asset = RES
```

### 2.4 Critical Notes

**HRP (Human Readable Part):**
- HRP = rg is a prefix only
- The character 'q' or 'p' in addresses is encoding result
- HRP is NOT the semantic source

**Semantic Source:**
- The ONLY semantic source is witness_version
- All asset domain inference MUST derive from witness version

---

## 3. ScriptPubKey Byte-Level Specification

### 3.1 V0 (ORU)

**Format:**
```
0x00 <20 or 32 bytes>
```

**Constraints:**
- Program size: 20 or 32 bytes
- Witness version: 0
- Encoding: Bech32

### 3.2 V1 (RES)

**Format:**
```
0x51 0x20 <32 bytes>
```

**Constraints:**
- Program size: MUST be exactly 32 bytes
- Witness version: 1
- Encoding: Bech32m

### 3.3 Strict Prohibitions

The following features are FORBIDDEN in V1 addresses:

- Taproot tweak
- Internal key
- Merkle tree
- Control block
- Annex
- MAST (Merklized Abstract Syntax Trees)

V1 is a simple container for 32-byte programs only.

---

## 4. Address and Semantic Relationship

### 4.1 Cognitive Model

The system implements the following conceptual model:

```
Address      -> Locks location (WHERE)
Asset Domain -> Defines behavior (WHAT)
Witness      -> Provides proof (HOW)
Consensus    -> Makes selection (WHICH)
```

### 4.2 Address as Safety Boundary

Addresses serve as immutable safety boundaries. Crossing asset domains MUST be impossible by design, not merely discouraged.

---

## 5. V1 Dual-Channel Relationship

### 5.1 Correct Understanding

**Incorrect Understanding (MUST AVOID):**
- V1 address determines Mint vs Spend

**Correct Understanding (MUST IMPLEMENT):**
- V1 address locks RES only
- Mint vs Spend is determined by input asset

### 5.2 Execution Logic

```
asset = GetInputAsset(tx);

if (asset == ORU) {
    // Channel A
    ExecuteMint();
}
else if (asset == RES) {
    // Channel B
    ExecuteSpend();
}
```

---

## 6. Wallet Implementation Requirements

### 6.1 Address Generation

```
GenerateAddress(asset):
    if asset == ORU:
        return Bech32(v0)
    if asset == RES:
        return Bech32m(v1)
```

### 6.2 Send Validation (MANDATORY)

```
ValidateSend(address, asset):
    if address.version == 0 and asset != ORU:
        REJECT
    if address.version == 1 and asset != RES:
        REJECT
```

### 6.3 Automatic Routing (RECOMMENDED)

Wallet software SHOULD:
- Automatically recognize asset type from user-input address
- Automatically select appropriate UTXOs
- Prevent manual asset selection errors

### 6.4 UI Mandatory Prompts

Wallet UI MUST display clear error messages for invalid combinations:

**Example:**
```
Send ORU -> rg1p... (RES)
INVALID: This address is for RES, not ORU
```

---

## 7. Address Test Vectors

### 7.1 Vector 1 (ORU)

**Input:**
```
version = 0
program = 20 bytes
```

**Expected:**
```
valid ORU address
```

### 7.2 Vector 2 (RES)

**Input:**
```
version = 1
program = 32 bytes
```

**Expected:**
```
valid RES address
```

### 7.3 Vector 3 (Invalid)

**Input:**
```
version = 1
program = 20 bytes
```

**Expected:**
```
REJECT (invalid program size)
```

### 7.4 Vector 4 (Cross-Asset)

**Input:**
```
Send ORU to V1 address
```

**Expected:**
```
REJECT (asset mismatch)
```

---

## 8. RPC Specification

### 8.1 New Interfaces (MANDATORY)

```
getnewaddress "ORU"
getnewaddress "RES"
```

### 8.2 Send Interface Validation

```
sendtoaddress address amount asset
```

**Required Behavior:**
```
if asset mismatch:
    return error
```

### 8.3 Error Messages

RPC MUST return specific error messages for:
- Invalid address type
- Asset mismatch
- Unsupported address format

---

## 9. Anti-Accident Design

### 9.1 Safety Requirements

Wallet software MUST ensure:
- Users do not need to understand V0/V1 technical details
- Cross-asset errors are impossible by design
- All validation occurs before transaction creation

### 9.2 Design Philosophy

The system implements:
```
"Errors are impossible"
NOT
"Users should be careful"
```

### 9.3 Implementation Safety

All wallet paths MUST:
- Validate address-asset binding before transaction creation
- Provide clear, actionable error messages
- Prevent silent failures or ambiguous states

---

## 10. Final Principles

### 10.1 Address as Safety Boundary

```
Address is a safety boundary.
Crossing it must be impossible,
not just discouraged.
```

### 10.2 System Guarantees

The final address system provides:

- Strong asset isolation (V0/V1)
- Unambiguous semantics
- Complete consistency with consensus
- Prevention of user errors
- Prevention of developer misuse
- No script dependencies

---

## 11. Consensus Enforcement

### 11.1 Validation Rules

Nodes MUST reject transactions that violate:
- Witness version to asset binding
- ScriptPubKey format constraints
- Cross-asset send attempts

### 11.2 Rejection Codes

Standard rejection codes:
- `bad-witness-version` - Invalid witness version
- `bad-asset-mismatch` - Asset type mismatch
- `bad-script-format` - Invalid ScriptPubKey format

---

## 12. Implementation Checklist

Wallet implementations MUST verify:

- [ ] Address generation follows V0/V1 specification
- [ ] Send validation enforces asset binding
- [ ] UI displays clear error messages
- [ ] RPC interfaces validate asset parameters
- [ ] Test vectors pass all checks
- [ ] Cross-asset sends are rejected
- [ ] Automatic routing prevents user errors

---

## 13. Migration Requirements

Existing wallet software MUST:
1. Implement V0/V1 address generation
2. Add asset validation to all send paths
3. Update UI to prevent cross-asset errors
4. Implement RPC interface changes
5. Pass all test vectors

---

**END OF RFC-ADDRESS-002**
