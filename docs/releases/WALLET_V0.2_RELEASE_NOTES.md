# RIGOR Wallet v0.2 — Public testnet release notes

**Product:** Rigor Wallet v0.2  
**Channel:** First public **testnet** wallet drop  
**Date:** 2026-09-14  
**Not a mainnet release.**

```
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
equivalence_claimed=false
```

This document does **not** claim that the C++ node is formally verified,
that Coq equals the running binary, or that the protocol is Level 5.

Packed `CLIENT_VERSION` remains `25.0.0` (wallet file / P2P subversion).
The product tag is **v0.2**.

---

## What this release is

Wallet v0.2 is the first intended **public GitHub** installable for the
current RFC-020 / RFC-018 wallet source. There is no v0.2.1 upgrade
history. Do not look for a v0.2.1 package.

Use this build to exercise Convergence and dual-asset flows on
**testnet** (or a dedicated test chain). It is not a mainnet launch
announcement.

---

## RFC-020 — wallet support

The wallet constructs and displays **continuous destroy-cost** mints:

- Convergence burns **ORU** (`OP_CONVERGE`).
- The same body transaction may **mint RES** when
  `R · (2²⁵ + h)² ≤ B << 40` at the confirming height `h`.
- The GUI and `createconvergence` show **destroy_cost**, RES minted, and
  ORU burned. There is no epoch pad and no `RATE(k)` table.

Display `destroy_cost` is not the exact burn `B`. Use the wallet preview
or `min_whole_res_burn` / `min_atom_burn` from `res_mint_unit_cost` when
constructing a precise mint.

ORU block subsidy on the node is 0 at genesis and 128 ORU for every
height ≥ 1. The wallet does not redefine that rule.

---

## RFC-018 — RES-transfer fee burn

On **RES transfers**, the node destroys half of the ORU network fee
(`floor(fee / 2)`). That share never enters coinbase. Pure ORU spends,
Convergence, and body mints are **not** this sink (Convergence already
destroys the declared ORU burn).

The v0.2 GUI still shows the **full** network fee on Send. It does not
split “miner-claimable vs destroyed”. The rule is enforced by the node,
not by a second wallet formula.

---

## Known limitations

- **Not mainnet.** Do not treat balances or test coins as mainnet value.
- **Unsigned** Windows installer (testnet drop).
- About / `--version` may still print the compiled codename
  “Production Release”; this **channel** is testnet — trust these notes
  for purpose.
- RFC-018 half-fee burn is not explained in the Send dialog.
- Qt internal widget names may still contain “Epoch” / “rate”; on-screen
  mint cost is destroy_cost at height.
- Explorer / old scripts that read mint RPC fields `rate` or `epoch`
  must switch to `destroy_cost` / `destroy_cost_next`.
- Wallet file is `rigor-wallet.sqlite` (descriptors). There is no
  `wallet.dat` import path.
- Formal verification of the running C++ binary is **not** claimed.

---

## What this is not

- Not Wallet v0.2.1  
- Not a consensus-parameter change  
- Not CPP verified  
- Not “formally verified production binary”  
- Not Level 5  

Protocol authority remains `docs/protocol/RFC-020.md` and
`docs/protocol/RFC-018.md`.
