# RFC-ADDRESS-001: Canonical Address Specification

**Version:** 1.0
**Status:** FINAL LOCK
**Layer:** Consensus and Address Specification

---

## 1. Abstract

RFC-ADDRESS-001 defines the canonical address specification for the RIGOR blockchain system. This specification establishes the fundamental principle that addresses serve as asset lock boundaries, with strong semantic partitioning between V0 and V1 witness versions. The specification ensures unique mapping between addresses and asset domains, prevents execution semantics in addresses, and provides deterministic, unambiguous encoding rules.

---

## 2. Design Objectives

The address system MUST satisfy the following design goals:

1. **Unique Asset Domain Mapping:** Each address MUST uniquely map to a single asset domain
2. **No Execution Semantics:** Addresses MUST NOT participate in execution semantics
3. **User Error Prevention:** Addresses MUST prevent user errors (cross-asset mistakes)
4. **Deterministic Encoding:** Address encoding MUST be deterministic and unambiguous

---

## 3. Address Type Definitions

### 3.1 V0 Address (ORU)

**Version:** 0
**Encoding:** Bech32 (BIP-173)
**ScriptPubKey:** OP_0 <20 or 32 bytes>
**Asset:** ORU ONLY

### 3.2 V1 Address (RES)

**Version:** 1
**Encoding:** Bech32m (BIP-350)
**ScriptPubKey:** OP_1 <32 bytes>
**Asset:** RES ONLY

---

## 4. Absolute Consensus-Level Constraints

The following rules MUST be enforced at consensus level in validation.cpp, wallet, and RPC:

```
IF ScriptPubKey.version == 0:
    asset MUST be ORU

IF ScriptPubKey.version == 1:
    asset MUST be RES
```

These constraints MUST be implemented in:
- validation.cpp
- wallet validation logic
- RPC interfaces

Failure to enforce these constraints will result in safety incidents.

---

## 5. Address is Not Semantics

### 5.1 Critical Distinction

V1 implements dual-channel execution, but this does NOT mean V1 addresses determine semantics.

### 5.2 Correct Understanding (MUST IMPLEMENT)

V1 is a resource container only. Semantics are determined by input asset.

### 5.3 Unified Expression

```
Address defines WHERE
Asset domain defines WHAT
Witness defines PROOF
ONLY asset domain defines semantic
```

---

## 6. V1 Dual-Channel Model

### 6.1 Channel A (Mint)

**Input:** ORU
**Output:** RES (V1)
**Semantic:** RES Mint
**Witness:** [Converge_txid, bind_hash, height, target_prev, Converge_root, proof_blob, signature]

### 6.2 Channel B (Spend)

**Input:** RES
**Output:** RES
**Semantic:** RES Spend
**Witness:** Free structure (signature / script path)

### 6.3 Core Locking Rule

```
Semantic = f(input_asset_domain)
```

Semantic is NOT determined by:
- address
- witness
- script path

---

## 7. HRP Specification

### 7.1 Recommended Format

For enhanced safety, strong semantic version distinction is recommended:

```
rg0... → ORU (V0)
rg1... → RES (V1)
```

Note: This is a recommendation for human readability. The actual semantic source remains witness_version.

---

## 8. Wallet Behavior Specification

### 8.1 Send Validation Logic

```
bool ValidateSend(address, asset) {
    if (address.version == 0 && asset != ORU)
        return false;

    if (address.version == 1 && asset != RES)
        return false;

    return true;
}
```

### 8.2 UI Mandatory Rules

Wallet UI MUST display:
- Address Type: ORU / RES
- Balance: ORU / RES

On send operations, the UI MUST:
- Detect cross-asset attempts
- Display warning: "You are sending ORU to RES address"
- Force rejection of invalid combinations

---

## 9. ScriptPubKey Byte-Level Definition

### 9.1 V0 (ORU)

**Format:**
```
0x00 <20 or 32 bytes>
```

### 9.2 V1 (RES)

**Format:**
```
0x51 0x20 <32 bytes>
```

**Constraints:**
- Program MUST be exactly 32 bytes
- NO Taproot tweak
- NO Merkle tree
- NO control block

---

## 10. Prohibited Actions

The following actions are FORBIDDEN:

- Using address to determine semantics
- Using HRP to determine asset type
- Mixing V0 and V1 inappropriately
- Introducing Taproot logic
- Loading addresses with execution logic

---

## 11. Core Unified Model

The entire system can be expressed as:

```
Address → Locks resources
Asset → Defines semantics
Witness → Provides proof
Consensus → Makes selection
```

### 11.1 Final Requirements

The system MUST lock three fundamental rules:

1. V0 = ORU ONLY
2. V1 = RES ONLY
3. Semantic = f(input_asset_domain)

---

## 12. Consensus Enforcement

### 12.1 Validation Rules

Nodes MUST enforce:
- Witness version to asset binding
- ScriptPubKey format constraints
- Cross-asset rejection

### 12.2 Implementation Locations

These rules MUST be implemented in:
- Consensus validation (validation.cpp)
- Wallet send validation
- RPC interface validation

---

## 13. Migration Requirements

Existing implementations MUST:
1. Enforce V0/V1 to asset binding
2. Remove any address-based semantic inference
3. Implement strict validation in all send paths
4. Update UI to prevent cross-asset errors

---

## 14. Safety Guarantees

The canonical address system provides:
- Strong asset isolation (V0/V1)
- Unambiguous semantics
- Prevention of user errors
- Prevention of developer misuse
- No script dependencies in addresses

---

**END OF RFC-ADDRESS-001**
