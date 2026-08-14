# Genesis Lockdown Audit Attachment

This document records **consensus-locked** genesis parameters for each public-style network variant, as defined in `src/kernel/chainparams.cpp`. Any change requires regenerating the genesis block, updating `consensus.hashGenesisBlock` asserts, checkpoints, `defaultAssumeValid` policy (when bumped past genesis), and this file.

## Shared policy

- **Coinbase reward at height 0:** `Consensus::ORU_INITIAL_BLOCK_SUBSIDY` (`1000 * COIN`) on mainnet, testnet, signet, regtest, devtest, and slowtest where that constant is used in `CreateGenesisBlock`.
- **Subsidy schedule:** `nBlockSubsidyBase = ORU_INITIAL_BLOCK_SUBSIDY`, `nBlockSubsidyMaxHalvingShifts = 3`, `nSubsidyHalvingInterval = 210000` (regtest uses interval **150** per `CRegTestParams`).
- **Coinbase vout[0]:** witness v0 P2WPKH-style placeholder (`0x0014` + 20 bytes); **no** bare P2PK in consensus-valid blocks.
- **Coinbase scriptSig layout:** `CreateGenesisBlock` retains Bitcoin-era height/nBits preamble pushes (`486604799`, `4`) plus the timestamp bytes — **wire layout only**; timestamps and hashes are chain-specific (see below).
- **Merkle root:** single coinbase tx, no witness; `assert(genesis.hashMerkleRoot == BlockMerkleRoot(genesis))` in code pins determinism.

## Mainnet (`CMainParams`)

Constants live in namespace `GenesisLockMainnet` in `chainparams.cpp`.
**Normative freeze:** RFC-016 (Alpha1 / `v0.1.1-alpha`). This is a new chain, not a hard fork of `v0.1.0-alpha`.

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00141111111111111111111111111111111111111111` |
| nVersion | `1` |
| nTime | `1786125600` (2026-08-07 18:00:00 UTC) |
| nBits | `0x1e010000` (~10 min/block at ~28 kH/s OpuHash, typical 8-thread PC) |
| nNonce | `0` |
| genesisReward | `1000 * COIN` |
| hashMerkleRoot | `0xccdba6059e3d93d9f9eddc83e17cf358d8181155d39ed4b0526d8cd4439528a8` (unchanged vs `v0.1.0-alpha`; coinbase bytes identical) |
| hashGenesisBlock | `0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758` |

**RFC-003 / convergence:** Height 0 must carry **zero** non-coinbase convergence weight; optional CVGE commitment must match cumulative 0 (`ContextualCheckBlock` in `validation.cpp`). Genesis coinbase is classified as ordinary coinbase (not spendable via standard wallet paths; RPC rejects `getrawtransaction` on it).

**`defaultAssumeValid`:** Pinned to `hashGenesisBlock` until a deep canonical tip exists; refresh per release policy in `chainparams.cpp` comments.

## Testnet (`CTestNetParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `Selection not computation. Convergence defines state. Naming is consensus.` |
| scriptPubKey (hex) | `00142222222222222222222222222222222222222222` |
| nTime / nNonce / nBits / nVersion | `1778224715` / `0` / `0x207fffff` / `1` |
| hashGenesisBlock | `0xdc14491196188457f5e2d9f5951e38b788a1c714dd606225b7490dafa9ef8bca` |

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
| hashGenesisBlock | `0xcc31d596fd01d27134f1ad83e50dc93699c984683acc420477ad986ffa5f348e` |

## Devtest (`CDevTestParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `[Rigor Devtest] In V0 we trust, in V1 we execute — fast validation chain.` |
| scriptPubKey (hex) | `00143333333333333333333333333333333333333333` |
| nTime / nNonce / nBits / nVersion | `1774915600` / `2` / `0x207fffff` / `1` |
| hashGenesisBlock | `0x407381b8179c92027826909806920e3633d5487cac816e8fff419223d30641c7` |

Devtest retains **Bitcoin-era buried heights and one `script_flag_exceptions` entry** for compatibility testing; **do not** treat as mainnet deployment truth.

## Slowtest (`CSlowTestParams`)

| Field | Value |
|--------|--------|
| pszTimestamp | `[Rigor Slowtest] In V0 we trust, in V1 we execute — low-speed validation chain.` |
| scriptPubKey (hex) | `00146666666666666666666666666666666666666666` |
| nTime / nNonce / nBits / nVersion | `1777177700` / `0` / `0x1e0fffff` / `1` |

## Audit notes

- `test/functional/test_framework/blocktools.py` subsidy helpers must stay aligned with `GetBlockSubsidy` (regtest base **1000 ORU**, halving interval **150**, max **3** right-shifts).
- `pow_tests.cpp` ASERT numeric vectors use a **synthetic** anchor time `1774915200` for frozen arithmetic regression; they are **not** the deployed mainnet genesis clock (`1786125600`). See comments in that test.
