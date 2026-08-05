# Branding and Terminology Lockdown

This document defines the approved naming, terminology, and safe migration boundaries for the Rigor chain dual-asset system (ORU and RES).
It is intended to prevent accidental consensus-impacting edits while standardizing user-facing language.

## 1) Locked Terminology

Use the following terms consistently in user-facing text, docs, RPC help, and UI:

- PoW algorithm: `OpuHash`
- Burn mechanism: `convergence`
- RES reward issuance: `Mint`

Required phrasing:

- `convergence transaction`
- `mint RES`

Disallowed wording for RES issuance context:

- `issue`
- `generate`
- `create`

Reference wording:

- `ORU is mined via OpuHash.`
- `RES is convergenced by burning ORU.`
- `RES is minted when a valid convergence wins.`

## 2) Unit Display Policy (UI Only)

Display-only rename:

- `satoshi` -> `Tharn`
- `satoshis` -> `Tharns`

Hard constraints:

- Do not change internal amount representation (`CAmount`, `int64_t`).
- Do not change serialization, consensus math, fee arithmetic internals.
- Equivalence remains: `1 Tharn = 1 satoshi (internal)`.

## 3) Branding Policy (User-Facing Scope)

User-facing replacement targets:

- `Bitcoin Core` -> `Rigor Core`
- `Bitcoin` -> `Rigor`
- `bitcoin` -> `rigor` (user-visible contexts only)
- Binary branding targets:
  - `bitcoind` -> `rigord`
  - `bitcoin-cli` -> `rigor-cli`
  - `bitcoin-qt` -> `rigor-qt`
- Optional UI rename:
  - `Wallet` -> `Shire Wallet`

Important:

- Do not perform blind global replace in consensus-critical source.
- Only replace user-visible strings and product naming surfaces.

## 4) Network Isolation Requirements

Network isolation must remain enabled and unique from Bitcoin:

- Unique `pchMessageStart`
- Unique genesis block
- Data directory migration target: `~/.rigor`
- Chain ports isolated from Bitcoin defaults (already customized in this project)

## 5) Address Prefix Requirements

Current locked state:

- ORU Bech32 HRP: `oru`

Design candidate (not yet activated):

- RES Bech32m HRP: `rg` (witness v1 RES program; see chainparams)

Note:

- Dual-HRP on one chain requires coordinated changes in address codec, wallet validation, and RPC surfaces.
- Treat dual-HRP as a separate feature track with dedicated tests.

## 6) convergence Transaction Policy

convergence transaction policy target:

- Must have exactly one convergence/Blackhole output.
- Must satisfy configured minimum Mint threshold by height.
- Fee policy target: higher than standard baseline (2x policy intent).

Implementation note:

- Fixed absolute fee independent of size conflicts with feerate-based mempool policy.
- Enforce via policy-compatible checks (effective feerate floor and/or dynamic constraints).

## 7) Forbidden Changes

Do not modify:

- `CTransaction` serialization format
- `CBlock` structure
- Script interpreter semantics
- Signature hashing logic
- UTXO DB format (except already-approved asset extensions)

## 8) Execution Plan (Approved Order)

1. Terminology and UI wording unification (`OpuHash`, `convergence`, `Mint`, `Tharn` display names).
2. RPC/CLI help text alignment to new terms.
3. Branding surfaces and binary naming migration.
4. Data dir and packaging consistency checks.
5. Optional RES dual-HRP feature as a separate tracked change.

## 9) Acceptance Checklist

- Node does not connect to Bitcoin network.
- Isolated Rigor network forms and syncs.
- Genesis is unique and locked.
- ORU addresses use `oru`.
- Convergence tx policy behaves as expected under mempool pressure.
- User-facing text uses locked terminology.
