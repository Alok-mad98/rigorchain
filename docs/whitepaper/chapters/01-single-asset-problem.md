# Chapter 1 — Why Two Assets Must Exist

This chapter states **why RIGOR exists** before later chapters state how it
works. The claim is architectural, not a market forecast.

A public proof-of-work chain must do three jobs at once:

```
Medium of Exchange
        +
Store of Value
        +
Security Incentive
```

A typical single-asset design, especially one with a declared finite supply,
asks **one** native unit to perform all three. Those jobs are not the same
job. Their users do not share a time horizon, a fee tolerance, or a reason
to transact. The result is the **security-budget problem**: after the subsidy
ends, nobody who benefits from long-run security is the party who generates
the fees that are supposed to pay for it.

RIGOR’s answer is not a hope that price will rise forever, and not a hope
that fee volume will appear after issuance stops. It is an **asset split at
consensus**. ORU takes exchange and miner payment. RES takes scarcity.
Neither role is optional. The pair is the reason the protocol exists.

---

## 1.1 Two users, two motives

A **medium of exchange** (MoE) is the unit people use to move value.

MoE users treat the network as a peer-to-peer transfer system. They care
about fee cost in an external unit, confirmation, and whether the asset is
easy to obtain just in time to pay. Their exposure to network-security risk
is often momentary. Many never hold the native unit as a reserve: payment
services can buy, send, and sell in one motion. For them, the asset must
**circulate**. Price level and price volatility are secondary. They will not
voluntarily fund a security budget whose benefit they barely consume.

A **store of value** (SoV) is the unit people hold because it is scarce and
the rules are stable.

SoV users treat the network as the protocol that protects a scarce native
stock across time. Transfer is only the on-ramp and off-ramp. They care
about long-term security and decentralisation: if the chain becomes cheap to
attack, the stock is no longer a store of value; if validation and mining
centralise, the stock carries counterparty risk and is no longer independent.
Transaction costs can be amortised over long holding periods. Circulation is
secondary. These users **would** pay for continuous protection — but they
almost never transact.

| Role | Time horizon | What they want | What they contribute to miner payment |
|------|--------------|----------------|---------------------------------------|
| Medium of exchange | Minutes to hours | Cheap, frequent transfers | Fees — and they resist high fees |
| Store of value | Years | Scarcity and permanent protection | Almost no transactions, so almost no fees |
| Miners | Every block, forever | Predictable payment for hashpower | Protection of the *entire* history, not only the current block |

A single unit is then asked to be, at once:

1. the thing people spend
2. the thing people hold
3. the thing that pays for hashpower

Those incentives conflict. Making the unit “something people are willing to
spend” fights making it “something people are willing to hold and pay to
secure.”

---

## 1.2 Finite supply locks the store-of-value story

A declared hard cap does not merely limit issuance. It **selects a
narrative**. Once the market treats the unit as a scarce reserve, rational
holders prefer to accumulate rather than spend. Velocity falls. The fee
market that was supposed to replace the subsidy never appears at the scale
the security budget requires.

This is sometimes described as Gresham’s law. The mechanism is different.
Classical Gresham requires a legally fixed exchange rate between two monies.
Here there is only one unit. The suppression of circulation comes from an
**appreciation expectation**: the same object is more valuable held than
spent. The observable result is the same — the “good” money leaves the
payment circuit — but the cause is SoV pricing, not a mint ratio.

The deeper mismatch is temporal. Miner hashpower does not protect only
today’s block. It protects the immutability of every prior block. A
store-of-value user consumes that protection for as long as they hold. A
one-shot inclusion fee cannot cover a perpetual service. Asking SoV users to
pay for continuous security with a payment that only occurs when they
*stop* holding is incentive-incompatible.

---

## 1.3 Two paths, one constraint

Bitcoin is the canonical single-asset, finite-supply design. It has been
asked to become a medium of exchange and, separately, a store of value. Both
paths are visible. They fail for complementary reasons. They are two faces
of the same constraint.

### Path A — Bitcoin as a medium of exchange

An MoE reading treats Bitcoin as a peer-to-peer value-transfer network. The
valuable object is the transfer, not the inventory. Users need not hold the
asset: specialised payment services can buy, send, and sell so that neither
sender nor receiver keeps a long position. These users do not price
volatility. They price the external cost of the fee.

If block time and block size are fixed, successful adoption rations scarce
block space and raises fees. That is fatal to MoE competitiveness against
other chains and against forks that offer cheaper inclusion.

If the protocol instead raises throughput — faster blocks or larger blocks —
it buys cheaper inclusion at the cost of security and decentralisation:
higher stale rates, higher validation cost, fewer people who can run a
full node. The MoE user who wanted cheap transfers is then asked to accept
a weaker settlement layer.

### Path B — Bitcoin as a store of value

An SoV reading treats Bitcoin as the protocol that secures a scarce native
stock. MoE is only the corridor in and out of that stock. Holders will
tolerate fees. They will not tolerate an insecure or captured chain, because
that is a direct attack on the asset’s independent value.

Success on this path requires a stable monetary rule **and** a security
budget that does not vanish. Bitcoin has a hard cap. After the last subsidy,
miners are paid only by fees. Whether that is sustainable is an open
question — especially because a successful SoV culture is exactly a culture
that does not transact. Fee share of miner revenue has, for long stretches,
been a small fraction of the block subsidy. The post-cap hypothesis is that
fees will grow enough to replace issuance. That hypothesis is not a protocol
rule. It is a hope.

### The same coin

The large-block attempt to force Path A does not escape Path B. Supply is
still finite, so the market still attaches some reserve premium. The unit is
then valued as “cash that is trying not to be digital gold”: not enough
organic transfer demand to fill the extra block space, and a persistent
valuation discount relative to the scarce-reserve story. Fees still do not
appear. The security budget is still a problem. The empty block space is
the symptom, not a separate disease.

```
finite supply + one asset
        │
        ├─ emphasise scarcity  →  hoarding  →  thin fees  →  post-subsidy security gap
        │
        └─ emphasise throughput →  weaker SoV premium
                                   + still-finite supply
                                   →  neither a working fee market
                                   nor a clean reserve
```

A single finite-supply asset cannot simultaneously make people willing to
spend it and willing to hold it for decades. Either transactions are too
few to pay for security, or scarcity is diluted to stimulate transactions
and both stories fail.

---

## 1.4 Who pays the miners

Network security and decentralisation are expensive. They must be paid to
operators, every block, without a calendar on which the need ends.

In the single-asset fee-only model:

- MoE users have almost no duration of security exposure, so they will not
  pay for it.
- SoV users have the duration, so they are the natural payers — but they
  produce almost no transactions.

The category error is to fund a **flow** (continuous protection of history)
with a **spot** payment (inclusion of a transaction). Inflation-based block
reward is the incentive-compatible instrument for a store-of-value network:
holders pay, in proportion to how long they enjoy the service, by accepting
dilution of the unit they hold.

That instrument, placed on the **same** unit that is supposed to be the
store of value, destroys the store-of-value story. Placed on no unit at
all, it leaves miners unpaid after the subsidy. This is the triangle a
single finite-supply asset cannot occupy:

```
            cheap circulation
                 / \
                /   \
               /     \
        finite      perpetual
        scarcity    miner budget
```

Pick two. Bitcoin picks finite scarcity and hopes the third vertex appears
as fees. Perpetual issuance on the same ticker picks the miner budget and
gives up the reserve. RIGOR refuses to pick two on one ticker. It assigns
the vertices to two assets and couples them.

---

## 1.5 The split — RFC-020

**ORU** is the medium of exchange and the miner-payment asset.

- Flat subsidy: `subsidy(0) = 0`, `subsidy(h ≥ 1) = 128` ORU. No halvings.
  No declared ORU cap.
- Inflation *rate* decays as `1/t` and never reaches zero. The **level** of
  residual inflation is not a schedule. It is `1 − ρ`, where `ρ` is the
  fraction of emitted ORU that is ever destroyed to create RES.
- ORU’s only jobs are circulation, fees, and the cash-flow that pays
  hashpower today. Users can spend it without sacrificing a reserved
  position. When — and only when — the market wants a store of value, ORU
  is destroyed and circulating supply falls. Hardness is a function of
  demand, not of a number written at genesis.

**RES** is the store of value.

- RES is not mined. Coinbase never creates it. There is no reverse
  conversion (RFC-020 §6).
- RES is created only in a body transaction that destroys ORU
  (`OP_CONVERGE`) and mints Witness v1 RES in the same transaction when

  ```
  R > 0  →  R · (2²⁵ + h)² ≤ B << 40
  ```

  Display form: `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰` ORU per whole
  RES. Genesis cost is 1024 ORU per RES. The cost is strictly increasing
  in height.
- The unit cost rises with confirmation height, so an earlier mint pays
  less per RES for a longer remaining span of protection, and a later mint
  pays more. That is duration-proportional payment, encoded as a height
  function, not as a recurring tax on RES balances.
- The ceiling `RES_MAX_SUPPLY = 2²² = 4 194 304` RES is the closed-form
  integral of the per-block mint ceiling `(S / P0) · H`. It is not a
  declared 21-million-style parameter. It needs **no consensus counter**:
  because `destroy_cost` is strictly increasing, any delay yields strictly
  less RES. No hoarding schedule reaches the bound from above. Realised
  stock is `ρ · 2²²`. The cap is a hard bound, not a promise that it will
  be filled.

The coupling is one mechanism, not two isolated tickers:

```
work                    →  ORU subsidy (128 / block)
RES demand at the ceiling →  ORU destroy          (RFC-020)
RES transfers             →  ORU fee destroy      (RFC-018, half of the ORU fee)
ρ                         →  realised RES stock and residual ORU inflation
```

`destroy_cost(h)` is a protocol destroy requirement, not a market quote.
While minting is open, a secondary RES/ORU ratio above that requirement is
bounded by mint arbitrage. A ratio below it simply stops minting.

---

## 1.6 After the split

Hoarding is isolated on RES. It no longer suppresses ORU velocity or the
ORU fee market.

ORU can develop throughput and a fee market without competing with a
reserve narrative on the same unit. Spending ORU does not spend the scarce
stock.

The miner budget has a protocol floor in the utility asset — 128 ORU every
block — that does not depend on SoV holders transacting. When RES demand
exists at the ceiling, that demand is an ORU sink: residual inflation falls
and ORU hardness rises endogenously. The single-asset loop

```
fewer transactions → thinner fees → weaker security → more hoarding
```

is replaced by a demand-led coupling. The protocol does not invent the
demand. It converts demand, when it exists, into destroyed ORU and a paid
miner.

The two user groups stop dragging each other. They meet at Convergence:
MoE users keep a unit they can spend; SoV users pay, in ORU destroyed, a
height-rising cost to enter a stock that is never inflated.

ORU addresses insufficient transaction velocity. RES addresses the
post-subsidy security budget. Each is incomplete alone:

| If only ORU exists | If only RES exists |
|--------------------|--------------------|
| Perpetual work payment, no scarce stock, no endogenous hardness from reserve demand | A capped stock with no work-payment asset and no way to pay hashpower |
| A medium of exchange without a store of value | A store of value without a security budget |

The pair is not an optional optimisation of a single-asset chain. It is the
exit from a structural defect. That is the existence reason for the
protocol.

---

## 1.7 What this chapter does not claim

- It does not target an external price for either asset.
- It does not assert that `ρ` will be high. Lifetime RES issuance still
  spans near zero to `2²²` depending on participation (RFC-020 §8).
- It does not assert that a secondary RES/ORU market exists on day one.
  Year-1 issuance is at most about 6 564 RES even at `ρ = 100%`. A market
  too thin to discover any ratio cannot transmit RES demand into ORU
  destroy. That is an adoption risk, not a consensus hole.
- It does not claim that C++ is formally equivalent to the kernel model.

**RIGOR’s answer is not a market promise. It is an asset split at
consensus.** Chapter 2 states the rules that implement the split.
