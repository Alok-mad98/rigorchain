# Genesis Lockdown Audit Attachment

> **Release identity.** Shipping product is **RIGOR v0.1.1-alphaplus** (codename **AlphaPlus Consensus Freeze**). Remaining "Alpha1" wording in this file is **historical Alpha1 naming before AlphaPlus release identity migration**. Consensus (genesis hash, Ed25519, lock v2, PoW, W/T, RES, fees) is unchanged.


This document records **consensus-locked** genesis parameters for each public-style network variant, as defined in `src/kernel/chainparams.cpp`. Any change requires regenerating the genesis block, updating `consensus.hashGenesisBlock` asserts, checkpoints, `defaultAssumeValid` policy (when bumped past genesis), and this file.

## Shared policy

- **Coinbase reward at height 0:** `Consensus::ORU_GENESIS_BLOCK_SUBSIDY` (`0`) on mainnet, testnet, regtest, devtest, and slowtest. `CreateGenesisBlock` and `GetBlockSubsidy(0)` both use this value (P0-038). Height `>= 1` keeps `ORU_INITIAL_BLOCK_SUBSIDY` (`1000 * COIN`).
- **Subsidy schedule:** `nBlockSubsidyBase = ORU_INITIAL_BLOCK_SUBSIDY`, `nBlockSubsidyMaxHalvingShifts = 3`, `nSubsidyHalvingInterval = 210000` (regtest uses interval **150** per `CRegTestParams`). Do not compensate the removed genesis 1000 ORU at later heights.
- **Coinbase vout[0]:** witness v0 P2WPKH-style placeholder (`0x0014` + 20 bytes); **no** bare P2PK in consensus-valid blocks.
- **Coinbase scriptSig layout:** `CreateGenesisBlock` retains Bitcoin-era height/nBits preamble pushes (`486604799`, `4`) plus the timestamp bytes — **wire layout only**; timestamps and hashes are chain-specific (see below).
- **Merkle root:** single coinbase tx, no witness; `assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis))` in code pins determinism.
- **Local data:** a genesis hash change invalidates existing `blocks/` and `chainstate/`. Wipe and reinitialize. There is no compatibility branch for pre-P0-038 hashes.

## Mainnet (`CMainParams`)

Constants live in namespace `GenesisLockMainnet` in `chainparams.cpp`.
**Normative freeze:** RFC-016 (Alpha1 / `v0.1.1-alphaplus`). This is a new chain, not a hard fork of `v0.1.0-alpha`.

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00141111111111111111111111111111111111111111` |
| nVersion | `1` |
| nTime | `1786125600` (2026-08-07 18:00:00 UTC) |
| nBits | `0x1d008000` (~10 min/block at ~16 MH/s optimized SHA3-256, typical 8-thread PC; SPEC-008 §8.2) |
| nNonce | `0` |
| genesisReward | `0` (`ORU_GENESIS_BLOCK_SUBSIDY`) |
| hashMerkleRoot | `0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd` |
| hashGenesisBlock | `0xf8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717` |

Pre-P0-038 mainnet hash `0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758` is **foreign**. Do not add a compatibility mapping.

**RFC-003 / convergence:** Height 0 must carry **zero** non-coinbase convergence weight; optional CVGE commitment must match cumulative 0 (`ContextualCheckBlock` in `validation.cpp`). Genesis coinbase is classified as ordinary coinbase (not spendable via standard wallet paths; RPC rejects `getrawtransaction` on it).

**`defaultAssumeValid`:** Pinned to `hashGenesisBlock` until a deep canonical tip exists; refresh per release policy in `chainparams.cpp` comments.

## Testnet (`CTestNetParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00142222222222222222222222222222222222222222` |
| nTime / nNonce / nBits / nVersion | `1778224715` / `0` / `0x207fffff` / `1` |
| hashGenesisBlock | `0x0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce` |
| hashMerkleRoot | `0x54ace6b8a887ef617d5d583c7fb326a88805b7f7afba190de470898ea2885343` |

## Signet (`SigNetParams`, default challenge)

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00144444444444444444444444444444444444444444` |
| nTime / nNonce / nBits / nVersion | `1778224715` / `0` / `0x207fffff` / `1` |
| hashGenesisBlock | `0x85b91b276d162f9fe04fec1f24ecf84c46bd027d75e4be1943412d221ff8f9a7` |

**Note:** Default signet still ships upstream Bitcoin DNS / fixed seed entries when no custom `-signetchallenge` is used — use only for **Bitcoin-compatible signet experiments**, not Rigor mainnet identity.

## Regtest (`CRegTestParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00145555555555555555555555555555555555555555` |
| nTime / nNonce / nBits / nVersion | `1778224715` / `0` / `0x207fffff` / `1` |
| hashGenesisBlock | `0x3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63` |
| hashMerkleRoot | `0xd3da525deee10cd11754b3269e919f8388184ec1f01a1222ad5419d00f0b43e3` |

## Devtest (`CDevTestParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `[Rigor Devtest] In V0 we trust, in V1 we execute — fast validation chain.` |
| scriptPubKey (hex) | `00143333333333333333333333333333333333333333` |
| nTime / nNonce / nBits / nVersion | `1774915600` / `2` / `0x207fffff` / `1` |
| hashGenesisBlock | `0x9762f59ecfe1050ca45e404dc95e32ae5a93b9a046e116f2f2f562e44f21dce0` |
| hashMerkleRoot | `0xada94f709a0bb27dfb0d1862eb4c0962c8a7c000c1c6e21dc5a7841f869c0a93` |

Devtest retains **Bitcoin-era buried heights** for compatibility testing; **do not** treat as mainnet deployment truth. Script-verify flags are compile-time constants (`GetBlockScriptFlags`); there is no `script_flag_exceptions` table.

## Slowtest (`CSlowTestParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `[Rigor Slowtest] In V0 we trust, in V1 we execute — low-speed validation chain.` |
| scriptPubKey (hex) | `00146666666666666666666666666666666666666666` |
| nTime / nNonce / nBits / nVersion | `1777177700` / `0` / `0x1e0fffff` / `1` |
| hashGenesisBlock | `0x18ddf722cf24b35ad9e3b548ddba22a2e54535fd5932d4d699fa9602f6d3efb8` |
| hashMerkleRoot | `0x46ee1198cb3aeee9160bf0f88b1c99d167024fedbbf9e926d81a9f2c49d32c85` |

## Audit notes

- `test/functional/test_framework/blocktools.py` subsidy helpers must stay aligned with `GetBlockSubsidy` (regtest base **1000 ORU** for `h >= 1`, height **0 = 0 ORU**, halving interval **150**, max **3** right-shifts).
- `pow_tests.cpp` ASERT numeric vectors use a **synthetic** anchor time `1774915200` for frozen arithmetic regression; they are **not** the deployed mainnet genesis clock (`1786125600`). See comments in that test.
