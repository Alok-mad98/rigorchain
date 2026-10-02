# MAINNET PROTOCOL FREEZE DECLARATION

**Document status:** Production specification  
**Date:** 2026-09-12  
**Realization:** `core/src/consensus/res_issuance.h`, `core/src/consensus/rigor_consensus_lock.h`

The frozen protocol defines these rules. Those headers realize them. A shipping node realizes RFC-020 issuance and RFC-018 fee burn. If this declaration misstates a frozen property, the declaration is corrected. If the implementation diverges from the frozen protocol, the implementation contains the defect. The defect does not redefine the protocol. Changing a value below is a protocol change, not a software upgrade.

---

## 1. Frozen parameters

| Lock | Value |
|------|-------|
| Issuance | RFC-020. Display `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰`. Consensus uses the inequality. |
| Mint predicate | `R > 0 → R · (2²⁵ + h)² ≤ B << 40`. `R = 0` is a legal plain burn. |
| `RES_COIN` | `100_000_000` atoms per whole RES |
| `RES_DESTROY_COST_GENESIS` | `1024 * COIN` (`102_400_000_000` atoms) |
| `RES_DESTROY_POLE` | `2²⁵ = 33_554_432` |
| `RES_DESTROY_SHIFT` | `40` |
| `RES_MAX_SUPPLY` | `419_430_400_000_000` atoms (`2²²` RES) |
| ORU subsidy | `R(0) = 0`; `128` ORU per block for `h ≥ 1`; no halving |
| Mint locus | Body transaction: unique positive `OP_CONVERGE` burn `B` + same-tx Witness v1 RES summing to `R` |
| Mint / coinbase maturity | `100` |
| RFC-018 | Half of RES-transfer ORU fees unclaimable: `Σ floor(fee(tx) / 2)` |
| Reverse conversion | RES → ORU forbidden |
| Activation | Genesis-only. No fork height, versionbits, or dual-rule transition. |
| `MIN_CONVERGENCE_AMOUNT` | `1` atom (`nValue > 0`). Not a mint price. |
| Chain name / HRP | `"RIGOR"` / `"rg"` |
| Block space | 1 MiB serialized / weight; 20_000 sigops |
| Lock version | `RIGOR_CONSENSUS_LOCK_VERSION = 3` |

Identical economics on mainnet, testnet, regtest, devtest, and slowtest.
Numeric witness: `tools/res_continuous_price_schedule.py`.

### Genesis

`GetBlockHash(0)` must match `core/src/kernel/chainparams.cpp`.

| Network | Genesis hash |
|---------|--------------|
| main | `e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468` |
| test | `0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce` |
| regtest | `3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63` |
| devtest | `9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0` |
| slowtest | `18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8` |

### Proof-of-Work

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid           ⇔  OpuHash(header) ≤ target
```

- Primitive: FIPS 202 SHA3-256 (domain suffix `0x06`)
- Preimage: exactly 88 bytes
- Block identity: `SHA256d(header_80)` — not OpuHash
- Difficulty: aserti3-2d; mainnet half-life **43200** s; per-block clamp ±12.5%
- Changing the digest, domain tag, or SHA3 suffix is a protocol change, not a software upgrade

---

## 2. Immutability

> Any modification of the mint inequality, `RES_COIN`, or `RES_MAX_SUPPLY`
> is a protocol change, not a software upgrade.

The following are not maintenance. Each one is a different protocol:

- Changing `RES_COIN` or `RES_DESTROY_COST_GENESIS`
- Replacing RFC-020 with any other issuance rule
- Sourcing destroy cost from `nBits`, ASERT, tip, time, or any header field
- Introducing an activation height, feature flag, or dual-rule transition for RES issuance
- Treating the display destroy cost as zero, free, or infinite

Permitted as maintenance: documentation that does not change consensus
meaning; non-consensus tooling; RPC field documentation.

---

## 3. Cryptographic boundary

Changing any of the following is a protocol change, not a software upgrade.

| Rule | Frozen value |
|------|----------------|
| Proof-of-Work | **OpuHash** = SHA3-256(`"RGRPOW01" \|\| header_80`); block id remains SHA256d |
| Signature algorithm | Ed25519 only (`libsodium` ≥ 1.0.18); secp256k1 CHECKSIG is disabled |
| HD derivation | Hardened-only (SLIP-0010). Unhardened BIP32 and public `CPubKey::Derive` fail closed |
| Default wallet paths | `wpkh` / `res` use `/0'/*'` or `/1'/*'` (not `/0/*`) |
| Watch-only HD | Ranged xpub cannot expand hardened children; import `addr()` / 32-byte hex |
| `res()` public form | Prints the 32-byte pubkey; never the 64-byte secret |
| Witness v1 RES program | 32-byte program. Spend is `<sig><pk>` with `pk == program`. Classification is a pure byte predicate. |
| On-curve Ed25519 (Scheme A) | **Not consensus.** `ed25519::IsCanonicalOnCurvePubkey` applies to Bech32m encode/decode, `IsStandard` / relay, and `IsValidDestination`. An off-curve v1/32 output is `ASSET_RES`, consensus-legal, and unspendable. |
| ORU P2WPKH program | `HASH160(32-byte Ed25519 pubkey)` |
| `OP_CONVERGE` owner | `HASH160(pk32)`, equal to `HASH160(v1 program)` |
| Asset class | `AssetTypeFromScriptPubKey(scriptPubKey)` only; never stored on `CTxOut`. Unknown scripts are `ASSET_INVALID` |
| Signature encoding | Exactly 64-byte RFC 8032 detached signature; any other length (including a 65-byte hashtype suffix) is rejected |
| Unique sighash domain | `SHA256d(tag \|\| chain_id \|\| nVersion \|\| hashPrevouts \|\| hashSequence \|\| hashOutputs \|\| this prevout \|\| spent scriptPubKey \|\| amount \|\| nSequence \|\| nLockTime)` with tags `RGRORU01` / `RGRRES01` / `RGRMNT01`. `chain_id` is genesis hash, passed as an explicit `SignatureHash` argument or `PrecomputedTransactionData::m_chain_id` — never read as a consensus global. |

---

## 4. Cross-references

| Document | Role |
|----------|------|
| [`RFC-020.md`](./RFC-020.md) | Shipping issuance specification |
| [`RFC-018.md`](./RFC-018.md) | RES-transfer ORU fee burn |
| `core/src/consensus/res_issuance.h` | Mint predicate |
| `core/src/consensus/rigor_consensus_lock.h` | Numeric locks |
| `core/src/kernel/chainparams.cpp` | Genesis hashes |
| [`../economics/RFC-020-ADOPTION-STATUS.md`](../economics/RFC-020-ADOPTION-STATUS.md) | Adoption tracker |
