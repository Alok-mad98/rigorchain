# ORU Blackhole Address Audit Checklist

## For Third-Party Auditors and Exchanges

### 1. Address Verification

| Check | Method | Expected Result |
|-------|--------|----------------|
| Address format | Base58 decode | Valid, 25 bytes |
| Script type | `validateaddress` RPC | `isscript: true` |
| Spendability | Inspect scriptPubKey | OP_RETURN or unspendable pattern |

### 2. ScriptPubKey Inspection

```bash
# Decode the address
core/src/rigor-cli -chain=mainnet decodescript <hex>
```

The decoded script must match one of the known unspendable templates:

- `OP_RETURN <data>` (data may vary)
- `OP_FALSE OP_RETURN` (variant)

### 3. Supply Audit

| Field | Source | Check |
|-------|--------|-------|
| Total ORU minted | Sum of all coinbase vout[0] | Must equal sum of all convergence inputs to blackhole |
| Blackhole balance | `getaddressinfo` or block explorer | Must monotonically increase |
| No outflow | Transaction graph | No spend from blackhole address ever |

### 4. Consensus Verification

- Verify that `validation.cpp` enforces the blackhole as the **only** valid ORU mint target.
- Verify that any deviation (alternative mint target) is rejected at the consensus layer.

### 5. Key Security

- Confirm **no private key** exists for this address.
- Confirm the address is derived from a hash of public constants, not from a key pair.

## Sign-Off Template

Auditor: _________________
Date: _________________
Commit: _________________

- [ ] Address verified unspendable
- [ ] Supply audit passed
- [ ] Consensus rule verified in source
- [ ] No private key risk

*This checklist must be completed before exchange listing approval.*
