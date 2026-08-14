# RFC-016-RIGOR-ALPHA1-GENESIS-MIGRATION

**Title:** RIGOR Alpha1 Mainnet Genesis Migration  
**Document ID:** `RFC-016-RIGOR-ALPHA1-GENESIS-MIGRATION`  
**Status:** FROZEN (Alpha1 genesis identity)  
**Date:** 2026-08-14  
**Task:** `TASK-ALPHA1-GENESIS-FINAL-DECISION-AUDIT-001`  
**Decision:** **Route B** — Alpha1 introduces a new mainnet genesis block  
**Companion audit:** `docs/release/ALPHA1_GENESIS_DECISION_REPORT.md`  
**Prerequisite freeze:** `docs/rfcs/RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL.md` (lock v2, Ed25519-only, 1 MiB / 20k sigops)

> **Numbering.** RFC-013’s scope statement names RFC-000 through RFC-016. This document occupies **RFC-016**. It is not Occam economics (that remains `RFC-015-OCCAM-REBALANCE.md`) and not the Alpha1 crypto/block-policy freeze (that remains `RFC-015-RIGOR-ALPHA1-CONSENSUS-MODEL.md`).

---

## 0. Freeze statement

RIGOR Alpha1 mainnet **MUST NOT** use the genesis block published in `v0.1.0-alpha` and still committed at official-tree HEAD `055b866ddd04d02dc9f179df5a32345dec751afe`.

Alpha1 mainnet identity is the genesis defined in §2. That genesis is a **new chain**, not a hard fork of `v0.1.0-alpha`.

This RFC does **not** change Ed25519, lock version (remains **2**), block caps, ORU/RES rules, Fee Rate Auction, W, T, or subsidy. It supplies the chain-identity half of the Alpha1 generation that RFC-015 declared but did not re-genesis.

**Normative code (when implemented; this RFC itself does not patch):**

| Surface | Location |
|---|---|
| Mainnet header fields | `core/src/kernel/chainparams.cpp` namespace `GenesisLockMainnet` |
| Hash pin | `assert(consensus.hashGenesisBlock == uint256S("0x1c2d64…d758"))` |
| IBD work floor | `CMainParams::nMinimumChainWork` |
| Human table | `core/doc/genesis-lockdown.md` |
| Genesis PoW exemption | `CheckProofOfWork` in `core/src/pow.cpp` |

---

## 1. Reason for change

### 1.1 Why the original genesis cannot be Alpha1

Published `v0.1.0-alpha` (`434357dacd1dd034ce6b45991e6bf604af5ed433`) and official HEAD `055b866` share:

```
nTime  = 1778224715        // 2026-05-08 07:18:35 UTC
nBits  = 0x207fffff        // powLimit class; GetBlockProof ≈ 2
nNonce = 0
hash   = 0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de
```

Keeping that hash while enforcing RFC-015 rules would mean:

1. **Shared UTXO at height 0** with `v0.1.0-alpha`, contradicting RFC-015 §1.2.
2. **An undeclared hard fork:** `055b866` already has lock v2, 1 MiB caps, and Ed25519-only CHECKSIG on that same genesis. Shipping it is a rule change on a released chain ID, under the same P2P magic `ce f1 db fa` and port 8760.
3. **Unusable public mining initialization:** `nBits 0x207fffff` is the easiest legal target. ASERT, genesis-anchored, sees a May 2026 clock as ~49 half-lives overdue at 2026-08-14 and stays at powLimit. Blocks are hardware-rate, not ~10 minutes.
4. **False generation boundary.** Lock version 2 names a new protocol generation; genesis hash is what nodes actually follow.

### 1.2 Why a new genesis (not a fork height)

RFC-014 / RFC-000: mainnet consensus constants activate **genesis-only**. Alpha1 is pre-public-economic-mainnet relative to the calibrated chain. There is no honest public tip that must be preserved on `5dd90c30…`.

A height-N rule change would still share history, UTXOs, and P2P identity with `v0.1.0-alpha`. Alpha1 requires **no common ancestor**.

### 1.3 What is not a reason

- Not a subsidy or premine change (reward and placeholder `scriptPubKey` unchanged).
- Not a manifesto / timestamp-string change.
- Not a lock bump to 3.
- Not a requirement to grind nonce until header SHA256d or OpuHash meets `nBits` (genesis remains PoW-exempt; §4).

---

## 2. Alpha1 mainnet genesis (normative)

All values are those already present in the audited working tree `GenesisLockMainnet` / `CMainParams`. Further mutation after this freeze requires a new RFC.

### 2.1 Header and coinbase

| Field | Value |
|---|---|
| `pszTimestamp` | `Selection not computation. Convergence defines state. Naming is consensus.` |
| coinbase `scriptPubKey` (hex) | `00141111111111111111111111111111111111111111` |
| `genesisReward` | `Consensus::ORU_INITIAL_BLOCK_SUBSIDY` (`1000 * COIN`) |
| `nVersion` | `1` |
| **`nTime` / timestamp** | **`1786125600`** (2026-08-07 18:00:00 UTC) |
| **`nNonce`** | **`0`** |
| **`nBits`** | **`0x1e010000`** |
| `hashPrevBlock` | null |
| construction | `CreateGenesisBlock(...)` in `kernel/chainparams.cpp` |

Coinbase `scriptSig` layout remains the Bitcoin-era preamble pushes (`486604799`, `4`) plus the timestamp bytes. That is wire ancestry, not Bitcoin genesis identity.

### 2.2 Hash

```
consensus.hashGenesisBlock =
  0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758
```

This is `CBlockHeader::GetHash()` = SHA256d (`SerializeHash`) of the 80-byte header. It is **not** the OpuHash PoW digest.

`defaultAssumeValid` and checkpoint height 0 MUST equal this hash until a later release pins a buried tip.

### 2.3 Difficulty initialization

| Item | Value |
|---|---|
| Genesis `nBits` | `0x1e010000` |
| Compact target | mantissa `0x010000`, size `0x1e` → `0x000001000000…0000` |
| Approximate `GetBlockProof` | `0xffffff` (16,777,215) |
| Design intent | ~10 minutes / block at ~28 kH/s OpuHash on a typical 8-thread PC |
| ASERT | aserti3-2d, genesis-anchored; mainnet τ = 172800 s; spacing 600 s; ±25% per-block clamp |
| `powLimit` | unchanged `7fff…ffff` |
| `fPowAllowMinDifficultyBlocks` | `false` on mainnet |

Block 1+ MUST satisfy OpuHash ≤ ASERT target. Genesis MUST NOT be used as evidence that header SHA256d itself is under `0x1e010000` (§4).

### 2.4 Related chainparams (not header bytes, shipped with this genesis)

```
consensus.nMinimumChainWork =
  0x0000000000000000000000000000000000000000000000000000000000100000
```

This floor is **below one** `0x1e010000` block’s proof so Alpha IBD/solo can leave the header-work gate. Genesis work under the new `nBits` already exceeds it. It is **not** a 51% substitute. Do not restore `0x33540` (legacy year-scale floor calibrated for proof=2).

Assumed blockchain disk hints (`m_assumed_blockchain_size = 5`, `m_assumed_chain_state_size = 1`) are UI-only and not consensus.

### 2.5 Unchanged network overlay (explicit)

| Item | Value | Note |
|---|---|---|
| P2P magic | `ce f1 db fa` | Unchanged vs `v0.1.0-alpha`. Overlay mixing possible; chain follow is not. |
| Port | 8760 | Same |
| HRP | `rg` | Same |
| DNS seed names | `seed{1,2,3}.rigor.network.` | MUST serve Alpha1 genesis or be removed |

A later magic bump is **out of scope**. It is not required for this RFC to take effect.

---

## 3. Legacy genesis (historical, not Alpha1)

| Field | `v0.1.0-alpha` / `055b866` |
|---|---|
| `nTime` | `1778224715` (2026-05-08 07:18:35 UTC) |
| `nNonce` | `0` |
| `nBits` | `0x207fffff` |
| `hashGenesisBlock` | `0x5dd90c30e88b90a1a6ce0b371d4abf8d7283d8a1275f70f9d51a8821048221de` |
| `nMinimumChainWork` | `0x33540` |

Alpha1 nodes MUST treat that hash as **foreign**. No replay, no UTXO import, no “upgrade in place” of a `v0.1.0-alpha` datadir.

---

## 4. Genesis proof-of-work exemption (normative)

```
CheckProofOfWork(header_hash, nBits, params):
  if header_hash == params.hashGenesisBlock:
      return nBits is a sane compact target  // work comparison skipped
  return OpuHash(header_hash) <= target(nBits)
```

Consequences:

1. Nonce `0` is legal for Alpha1 genesis even though SHA256d `0x1c2d64…` is **greater** than compact target `0x1e010000`.
2. This is constructed genesis, not a mined OpuHash solution. Reproducibility is the `CreateGenesisBlock` bytes plus the pinned hash, not a nonce search.
3. **MUST NOT** remine nonce after this freeze to “make genesis valid PoW.” That would change §2.2 and is a new RFC.
4. Test coverage: `pow_tests.cpp` `CheckProofOfWork_genesis_exempt_from_opus_pow`.

In-tree `core/gen_genesis.py` and `core/mine_genesis.py` are **non-normative** and MUST NOT be used to override §2.

---

## 5. Network separation and operator rules

### 5.1 Consensus isolation

Different `hashGenesisBlock` ⇒ different chain. `v0.1.0-alpha` binaries and Alpha1 binaries MUST NOT be described as peers on one ledger.

### 5.2 Overlay residual risk

Same magic and port allow a version handshake. Nodes then reject foreign genesis / headers. Operators MUST still:

1. Wipe or abandon any mainnet datadir produced by `v0.1.0-alpha` (hash `5dd90c30…`) before starting Alpha1.
2. Point seeds and `addnode` at Alpha1-genesis peers only.
3. Not mix product tags `v0.1.0-alpha` and `v0.1.1-alpha` in one advertised mesh expecting a single tip.

### 5.3 No asset migration

ORU/RES created on `5dd90c30…` have **no** validity on `1c2d6436…`. There is no snapshot, no claim transaction, and no premine matching prior balances.

---

## 6. Reproducibility

### 6.1 Construction

Inputs in §2.1 fully determine the coinbase, merkle root, header, and SHA256d hash. `CMainParams` MUST assert:

- `genesis.hashMerkleRoot == BlockMerkleRoot(genesis)`
- `genesis.GetHash() == consensus.hashGenesisBlock`
- `consensus.hashGenesisBlock == uint256S("0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758")`

### 6.2 Operator cross-check

After building the release `rigord` (operator-local `make`; not performed by this RFC’s authoring task):

```text
getblockhash 0
= 1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758
```

Mismatch is a release-blocker, not a “close enough” genesis.

### 6.3 Timestamp freeze

`nTime = 1786125600` is already earlier than this RFC’s calendar date. That induces a bounded ASERT ease (clamped ±25%/block), not powLimit collapse. **Further timestamp refresh is forbidden** by this RFC: it would mint a third hash and undo the freeze.

If wall-clock at public launch is months later, difficulty will ease; that is accepted Alpha1 behavior, not a license to edit genesis.

---

## 7. Hard-fork and versioning

| Question | Answer |
|---|---|
| Is Alpha1 a hard fork of `v0.1.0-alpha`? | **No.** New genesis, no shared history. |
| What would a hard fork have been? | Keeping `5dd90c30…` while enforcing lock v2 / 1 MiB / Ed25519-only. |
| Lock version | **Remains 2.** Genesis migration completes lock-v2 identity; it is not lock 3. |
| Product tag | **`v0.1.1-alpha`**. Do not retag `v0.1.0-alpha`. |
| Shared UTXO with `v0.1.0-alpha` | **No** (RFC-015 §1.2; this RFC makes it true in chainparams). |

---

## 8. Testnet and other networks

This RFC **does not** regenerate testnet, signet, regtest, devtest, or slowtest genesis hashes. Those remain as in `genesis-lockdown.md` at the time of this freeze, except that documentation of the **mainnet** clock in that file MUST match §2.

---

## 9. Forbidden behavior

| Forbidden | Reason |
|---|---|
| Shipping `055b866` genesis as Alpha1 | Alpha1 rules on `v0.1.0-alpha` chain ID |
| Restoring `nBits 0x207fffff` on Alpha1 mainnet | powLimit public mining |
| Restoring `nTime 1778224715` | ASERT collapse |
| Editing §2 hash/time/nonce/nBits without a new RFC | Identity unfreeze |
| Importing `v0.1.0-alpha` UTXOs / blk files as Alpha1 | Shared-history smuggling |
| Treating `fee_rate`, magic bump, or lock 3 as part of this RFC | Wrong surface |
| Using `gen_genesis.py` / `mine_genesis.py` as authority | Incorrect serialization / clocks |

---

## 10. Status

**FROZEN** for Alpha1 release engineering.

Implementation is a later commit of the already-audited working-tree constants. This RFC does not itself modify code, commit, or push.

Amendment of §2 requires a new RFC and a new genesis hash. Silent restoration of `5dd90c30…` is a protocol-identity violation, not a bugfix.
