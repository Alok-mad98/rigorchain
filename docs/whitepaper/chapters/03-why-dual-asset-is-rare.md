# Note — Why Dual-Asset Architecture Is Rare

Narrative Chapter 3 is now
[`03-native-utxo.md`](./03-native-utxo.md).
This file keeps the shorter “why overlays are cheaper” note.

Most public chains launched as **one ticker**: one unit for fees, one unit
for miner or staker payment, and one unit for the reserve narrative.

That design is historically dominant because it is easy to explain, easy to
list, and easy to implement on a single UTXO ledger.

---

## The simplicity tradeoff

A single token minimizes:

- wallet surface
- listing complexity
- consensus accounting
- user vocabulary

The cost of that simplicity is Chapter 1. The same finite-supply unit cannot
occupy cheap circulation, finite scarcity, and a perpetual miner budget at
once.

RIGOR accepts a harder public story — two assets, one transformation path —
so those jobs are not the same consensus object.

---

## What economic separation buys

1. **ORU can keep a tail subsidy** without claiming that the reserve inflates.
2. **RES can keep a derived finite cap** (RFC-020: `2²²` RES) without starving miner payment.
3. **RES entry is costly and public.** It requires ORU Convergence — not an
   admin mint, and not a hidden premine.

This is an architectural advantage. It is not a price forecast.

Dual assets are rare because they are operationally harder. RIGOR takes that
cost so medium of exchange and store of value are not forced to share one
incentive surface.
