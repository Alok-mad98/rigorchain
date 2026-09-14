# RIGOR Protocol Whitepaper

**A Dual-Asset Proof-of-Work Protocol with Deterministic Convergence and Zero-Trust Verification**

| Field | Value |
|-------|-------|
| Document status | Production specification |
| Date | 2026-09-04 |

This document is the official protocol narrative. It covers purpose,
consensus design, security assumptions, and implementation limits.

Protocol SSOT: [`../protocol/RFC-020.md`](../protocol/RFC-020.md),
[`../protocol/RFC-018.md`](../protocol/RFC-018.md).
Implementation facts: [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).

Normative engineering records of RFC texts are archived at `docs/archive/rfc/`.
Public mathematics is `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`.

**Core identity:**

```
Dual Asset
    +
Convergence
    +
Deterministic Fairness
    +
Zero Trust Verification
    +
Visible Mining Participation
```

**Official architecture formula:**

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    Closed monetary script templates (v0 ORU, v1 RES, mint, converge)
  +
    UTXO Local Validation
  +
    Ed25519 Authentication
  +
    OpuHash (SHA3-256 / RGRPOW01) + ASERT PoW
  +
    Deterministic Reserve Issuance
  +
    Deflationary Fee Destroy
  +
    Formal Verification Model
  +
    No Global Asset State
```

**Architecture identity:**

RIGOR is a two-asset UTXO protocol with a closed script-compliance surface.

It combines:

- ORU and RES native assets
- deterministic reserve issuance
- deflationary transaction economics
- Ed25519 authentication
- OpuHash (SHA3-256 / `RGRPOW01`) Proof-of-Work
- UTXO-local validation

without:

- token registry
- account ledger
- contract storage
- global asset state

The formal system verifies protocol models and finite vectors.
The production C++ implementation is not claimed to be formally equivalent.

---

## Abstract

RIGOR exists to exit a structural defect of single-asset proof-of-work
chains. Those chains ask **one** native asset to do three jobs at once:
circulate as a medium of exchange, remain scarce as a store of value, and
pay miners for security. The jobs conflict. Cheap transfers want
circulation. Scarcity wants withdrawal from circulation. Security wants a
miner budget that does not vanish when holders stop transacting. A single
finite-supply ticker cannot occupy all three vertices — the
**security-budget problem**. The dual-asset split is not an optional
optimisation. It is the existence reason for the protocol.

**RIGOR** separates the jobs at consensus.

- **ORU** is the utility asset: fees, miner subsidy, and Convergence entry
  weight. It has a flat subsidy of **128 ORU** per block so miner payment
  does not depend on inflating the reserve.
- **RES** is the scarce asset. Its maximum supply is **derived**:
  `RES_MAX_SUPPLY = 419430400000000` atoms (`4194304 RES`).
  It is not created by mining subsidy, not by governance, and not by a
  1:1 swap. New RES appears only in a **body transaction** that destroys
  ORU (`OP_CONVERGE`) and mints Witness v1 RES in the **same
  transaction** at protocol cost `destroy_cost(h)`. RES is **deterministically
  issued** after protocol-defined conditions. It is not mined.

**Convergence** is the core mechanism. A user destroys ORU in an
`OP_CONVERGE` output. The same transaction MAY mint RES atoms `R` when
`R · (2²⁵ + h)² ≤ B << 40` at the confirming height.
`destroy_cost(0) = 102400000000` (1024 ORU per 1 RES).
`destroy_cost(h)` is a **protocol cost**, not a market quote.

**Zero Trust** means a stranger does not accept a website or explorer
verdict. Public data is recomputed locally. Equal bytes are accepted.
Unequal bytes are rejected.

This paper explains *why* the split exists, *how* Convergence works, how
miners participate (including the **Windows** path), and how verification
is layered. It does not claim protocol-to-binary equivalence or mainnet
completion.

---

## Contents

1. Why Two Assets Must Exist (ORU and RES)
2. The Rules That Implement the Split
3. Native UTXO Identities
4. Convergence: The One-Way Bridge
5. Checkable Mathematics
6. Proof-of-Work: Shared Height, Paid in ORU
7. Recompute Locally
8. The Formal Boundary
9. Product Surfaces: A Display Remains a Display
10. Appendix

---

# Chapter 1 — Why Two Assets Must Exist

This chapter states **why RIGOR exists** before later chapters state how
it works. The claim is architectural, not a market forecast. The expanded
form is [`chapters/01-single-asset-problem.md`](./chapters/01-single-asset-problem.md)
(Chinese: [`chapters/01-single-asset-problem.zh.md`](./chapters/01-single-asset-problem.zh.md)).

A public proof-of-work chain must do three jobs at once:

```
Medium of Exchange
        +
Store of Value
        +
Security Incentive
```

A typical single-asset design, especially one with a declared finite
supply, asks **one** native unit to perform all three. Those jobs are not
the same job. Their users do not share a time horizon, a fee tolerance, or
a reason to transact. The result is the **security-budget problem**: after
the subsidy ends, nobody who benefits from long-run security is the party
who generates the fees that are supposed to pay for it.

## 1.1 Two users, two motives

A **medium of exchange** is the unit people use to move value. MoE users
treat the network as a peer-to-peer transfer system. They care about fee
cost in an external unit and confirmation. Exposure to network-security
risk is often momentary. Payment services can buy, send, and sell in one
motion. For them, the asset must **circulate**. They will not voluntarily
fund a security budget whose benefit they barely consume.

A **store of value** is the unit people hold because it is scarce and the
rules are stable. SoV users treat the network as the protocol that
protects a scarce native stock across time. Transfer is only the on-ramp
and off-ramp. They care about long-term security and decentralisation.
Transaction costs can be amortised over long holding periods. These users
**would** pay for continuous protection — but they almost never transact.

| Role | Time horizon | What they want | What they contribute to miner payment |
|------|--------------|----------------|---------------------------------------|
| Medium of exchange | Minutes to hours | Cheap, frequent transfers | Fees — and they resist high fees |
| Store of value | Years | Scarcity and permanent protection | Almost no transactions, so almost no fees |
| Miners | Every block, forever | Predictable payment for hashpower | Protection of the *entire* history, not only the current block |

A single unit is asked to be, at once: the thing people spend, the thing
people hold, and the thing that pays for hashpower. Making it “something
people are willing to spend” fights making it “something people are
willing to hold and pay to secure.”

## 1.2 Finite supply locks the store-of-value story

A declared hard cap does not merely limit issuance. It **selects a
narrative**. Once the market treats the unit as a scarce reserve, rational
holders prefer to accumulate rather than spend. Velocity falls. The fee
market that was supposed to replace the subsidy never appears at the scale
the security budget requires.

The mechanism is not classical Gresham (which needs a legally fixed
exchange rate between two monies). Here there is one unit. Circulation is
suppressed by an **appreciation expectation**: the same object is more
valuable held than spent.

The deeper mismatch is temporal. Miner hashpower protects the
immutability of every prior block, not only today’s. A store-of-value
user consumes that protection for as long as they hold. A one-shot
inclusion fee cannot cover a perpetual service. Asking SoV users to pay
for continuous security with a payment that only occurs when they *stop*
holding is incentive-incompatible.

## 1.3 Two paths, one constraint

Bitcoin is the canonical single-asset, finite-supply design. It has been
asked to become a medium of exchange and, separately, a store of value.
Both paths fail for complementary reasons. They are two faces of the same
constraint.

**Path A — Bitcoin as MoE.** Users need not hold the asset; they price
the external cost of the fee. Fixed block time and size make successful
adoption raise fees and lose MoE competitiveness. Faster or larger blocks
buy cheaper inclusion at the cost of security and decentralisation.

**Path B — Bitcoin as SoV.** Holders will tolerate fees. They will not
tolerate an insecure or captured chain. After the last subsidy, miners
are paid only by fees. A successful SoV culture is exactly a culture that
does not transact. Whether fees replace issuance is an open question. It
is not a protocol rule. It is a hope.

The large-block attempt to force Path A does not escape Path B. Supply is
still finite, so the market still attaches some reserve premium. The
result is empty extra block space, a valuation discount relative to the
scarce-reserve story, and the same unpaid security budget.

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
spend it and willing to hold it for decades.

## 1.4 Who pays the miners

Network security is a **flow** (continuous protection of history). A fee
is a **spot** payment for inclusion. Funding the first with the second is
a category error.

Inflation-based block reward is the incentive-compatible instrument for a
store-of-value network: holders pay, in proportion to duration, by
accepting dilution. Placed on the **same** unit that is supposed to be
the store of value, that instrument destroys the reserve story. Placed on
no unit at all, it leaves miners unpaid after the subsidy.

```
            cheap circulation
                 / \
                /   \
               /     \
        finite      perpetual
        scarcity    miner budget
```

Pick two. Bitcoin picks finite scarcity and hopes the third vertex
appears as fees. Perpetual issuance on the same ticker picks the miner
budget and gives up the reserve. RIGOR refuses to pick two on one ticker.
It assigns the vertices to two assets and couples them.

## 1.5 The split — RFC-020

**ORU** is the medium of exchange and the miner-payment asset. Flat
subsidy: `subsidy(0) = 0`, `subsidy(h ≥ 1) = 128` ORU. No halvings. No
declared ORU cap. Inflation *rate* decays as `1/t`. Residual inflation
*level* is `1 − ρ`, where `ρ` is the fraction of emitted ORU ever
destroyed to create RES. Hardness is a function of demand, not of a
number written at genesis. Users can spend ORU without sacrificing a
reserved position.

**RES** is the store of value. It is not mined. There is no reverse
conversion. It is created only in a body transaction that destroys ORU
(`OP_CONVERGE`) and mints Witness v1 RES when
`R > 0 → R · (2²⁵ + h)² ≤ B << 40`. Display form:
`destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰` ORU per whole RES. The cost
is strictly increasing, so an earlier mint pays less per RES for a longer
remaining span of protection — duration-proportional payment, encoded as
a height function. The ceiling `2²² = 4 194 304` RES is the closed-form
integral of the per-block mint ceiling. It needs **no consensus
counter**: any delay yields strictly less RES. Realised stock is
`ρ · 2²²`.

```
work                      →  ORU subsidy (128 / block)
RES demand at the ceiling →  ORU destroy          (RFC-020)
RES transfers             →  ORU fee destroy      (RFC-018)
ρ                         →  realised RES stock and residual ORU inflation
```

`destroy_cost(h)` is a protocol destroy requirement, not a market quote.

## 1.6 After the split

Hoarding is isolated on RES. ORU can develop a fee market without
competing with a reserve narrative on the same unit. The miner budget has
a protocol floor — 128 ORU every block — that does not depend on SoV
holders transacting. When RES demand exists at the ceiling, that demand
is an ORU sink. The protocol does not invent the demand. It converts
demand, when it exists, into destroyed ORU and a paid miner.

ORU alone is a work-payment asset with no scarce stock. RES alone is a
capped stock with no way to pay hashpower. The pair is not an optional
optimisation. It is the exit from the defect.

This chapter does not target an external price, does not assert that `ρ`
will be high, and does not claim C++ equivalence to the kernel model.
Year-1 RES issuance is at most about 6 564 even at `ρ = 100%`. A market
too thin to discover any ratio cannot transmit demand into destroy. That
is an adoption risk, not a consensus hole.

**RIGOR’s answer is not a market promise. It is an asset split at
consensus.** Chapter 2 states the rules that implement the split.

---

# Chapter 2 — The Rules That Implement the Split

Chapter 1 assigned the three vertices to two assets. This chapter writes
that split as frozen rules. The claim is still architectural: the protocol
fixes a **transform**, not a price and not a participation rate. The
expanded form is [`chapters/02-two-assets.md`](./chapters/02-two-assets.md)
(Chinese: [`chapters/02-two-assets.zh.md`](./chapters/02-two-assets.zh.md)).

```
PoW discovers and orders
        ↓
ORU enters circulation (coinbase, 128 per block)
        ↓
ORU is destroyed (OP_CONVERGE)
        ↓
the same body transaction MAY mint RES
```

Proof-of-work discovers blocks and gives irreversible height order. It
**does not** create RES. No Convergence → no new RES.

## 2.1 One mechanism, not two isolated tickers

Free convertibility would re-merge the pair into one asset. No coupling
at all would again detach the miner budget from the scarce stock. The
protocol admits only this path:

```
work → ORU
ORU destroy → (may) RES
RES → ORU    illegal
```

`destroy_cost(h)` is a protocol destroy requirement, not a market quote.
Users **mine ORU**. Users **destroy ORU**. The same transaction **may**
mint RES. That sentence must not be shortened to “mine RES”.

## 2.2 ORU — rules for the flow

```
subsidy(0) = 0
subsidy(h) = 128 ORU          for every h ≥ 1
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

No halvings. No declared ORU cap. The inflation **rate** decays as
`1/t`. Residual inflation **level** is `1 − ρ`. Under `ρ = 100%`,
circulating ORU is exactly constant. No fixed cap can express that. That
is why ORU does not get one.

The 128 is the miner-budget floor in the utility asset. It does not
depend on RES demand or on fees. Spending ORU is not spending the scarce
stock. A positive atom is a legal plain destroy. The destroy that mints
one RES atom at genesis is 1 024 ORU atoms.

## 2.3 RES — rules for the stock

RES is not mined, not issued by governance, not swapped 1:1, and not
convertible back. Consensus evaluates

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
R = 0  →  B unconstrained
```

Display form: `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰`,
`destroy_cost(0) = 1024` ORU per whole RES. The cost is strictly
increasing. An earlier mint pays less per RES for a longer remaining
span of protection. Receiver is the minting transaction’s RES outputs,
**not** the miner. Mint maturity 100 is reorg protection. There is no
per-block mint cap.

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + body_mint_atoms(h+1)
S_RES(h) ≤ 419430400000000
```

Whether a mint occurs is demand-driven. The cost **if** it occurs is a
pure function of height.

## 2.4 Where the parameters come from

Every issuance parameter is a power of two. The cap is the product of
three of them:

```
RES_MAX_SUPPLY = (S / P0) · H = 2⁷ / 2¹⁰ · 2²⁵ = 2²² = 4 194 304 RES
```

`RES_DESTROY_SHIFT = 40` is forced: `40 = 50 − log₂(1024)`. Because
`destroy_cost` is strictly increasing, the closed-form integral is an
upper bound that **needs no consensus counter**. Delaying a destroy
yields strictly less RES. Realised stock is `ρ · 2²²`. Cumulative
through height `h` is `cap · h / (h + 2²⁵)`. The pole `h = 2²⁵` is the
median of the `ρ = 100%` distribution (year 638; about 4 096 ORU per
RES).

The `ρ = 100%` year-1 ceiling is about 6 564 RES. Thin early float pins
any working secondary ratio to the mint ceiling. A market too thin to
discover any ratio cannot transmit demand into destroy.

## 2.5 The second sink — RFC-018

RFC-020 prices RES creation. RFC-018 destroys `floor(fee / 2)` of the
ORU fee on RES **transfers**. Pure ORU spends and Convergence itself do
not take this half. Four ORU quantities must stay separate: ledger
supply (issued minus `C(h)`), circulating UTXO, Convergence destroy, and
fee destroy. Consensus does not keep a global supply table.

## 2.6 Enforced vs not guaranteed

**Enforced:** the mint predicate; no RES→ORU; `S_RES` at or below the
derived ceiling. **Not guaranteed:** that anyone destroys ORU, that the
ceiling is filled, that a secondary market exists, or that `ρ` lands in
any band.

**The rules fix the transform. Whether demand arrives is not a rule.**
How the two identities exist on the ledger as native UTXO scripts — not
as a registry, and not as an overlay — is Chapter 3.

---

# Chapter 3 — Native UTXO: How the Two Identities Exist on the Ledger

Chapter 2 wrote the three vertices as frozen rules. This chapter states
how those two identities exist as ledger objects. The protocol fixes
**objects and classification**, not an off-chain registry and not a
colour on a single UTXO. The expanded form is
[`chapters/03-native-utxo.md`](./chapters/03-native-utxo.md)
(Chinese: [`chapters/03-native-utxo.zh.md`](./chapters/03-native-utxo.zh.md)).
Engineering audit:
[`chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`](./chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md).

If ORU and RES were only annotations on one output, Chapter 1’s split
would not have happened. A registry can credit RES without a destroy. An
inscription can stick a store-of-value story back onto the medium of
exchange. The split must be the same classification every full node
reads from the same bytes.

## 3.1 The ledger object is still a UTXO

```
TxOut = (nValue, scriptPubKey)
```

No account model as consensus truth. No parallel `asset` field. No
`(asset, value, script)` triple. Ownership changes only by spending
previous outputs and creating new ones. The same accepted block bytes
must produce the same validity decision.

## 3.2 Script classifies; consensus legitimates

`AssetTypeFromScriptPubKey` reads witness version and program shape:

```
script shape → ORU | RES | NONE | INVALID
```

**Script defines identity; consensus defines legitimacy.** Parsing
Witness v1 is not a legal mint. Birth of RES still requires
`OP_CONVERGE` in the same transaction and
`R > 0 → R · (2²⁵ + h)² ≤ B << 40`. Script alone does not authorize
asset creation.

## 3.3 Closed templates

Production has **no Script VM**. Bytes are classified. Spends go through
`VerifyRigorPredicate`.

| Shape | Asset | Spendable |
|-------|-------|-----------|
| Witness v0, 20-byte | ORU | Yes |
| Witness v1, 32-byte | RES | Yes |
| `<HASH160> 0xbd` | ORU (destroy) | No — not a UTXO |
| Zero-value `0x6a` | NONE | No |
| Anything else | INVALID | — |

Coinbase `0xbb` is not the RFC-020 mint path. P2PKH, P2SH, P2WSH,
Taproot, and secp256k1 spends are consensus-invalid. **Witness v1 is
not Bitcoin Taproot.**

Conservation is per asset pool: ORU inputs cannot fund RES outputs;
RES inputs cannot fund ORU outputs. The only one-way exception is
Chapter 2’s destroy-to-mint. The new RES is a new UTXO, not a colour
on the old output.

## 3.4 Not a registry, not an overlay

ORU and RES are not colored coins, inscriptions, Ordinals, Runes,
contract units, or wrapped assets. Neither is a secondary representation
of the other. The split happens in consensus, not in the explorer.

```
Less capability → less complexity → smaller attack surface
→ more feasible mathematical verification
```

The goal is maximum verifiability, not maximum programmability.

**Identity lives in the script. Legitimacy lives in consensus.**
How Convergence is verified as that one-way bridge is Chapter 4.

---

# Chapter 4 — Convergence: How the One-Way Bridge Is Verified

Chapter 3 gave two native identities. Without a public, deterministic
bridge they are two unrelated ledger objects. This chapter states how
every full node verifies that bridge. The expanded form is
[`chapters/04-convergence.md`](./chapters/04-convergence.md)
(Chinese: [`chapters/04-convergence.zh.md`](./chapters/04-convergence.zh.md)).

The protocol fixes **one visible destroy, and an integer predicate in
the same transaction**. It does not fix that anyone will destroy.

```
ORU inputs
        ↓
exactly one OP_CONVERGE output, amount B (destroyed, not a UTXO)
        ↓
Witness v1 RES outputs summing to R, same body transaction
        ↓
at confirming height h: R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

No Convergence → no new RES.

## 4.1 Three requirements that do not come apart

If destroy and mint sit in different transactions, the ledger has a
window of credit. If destroy is inferred from
`inputs − outputs − fee`, it collapses into the fee remainder. If a
miner writes RES into coinbase, the bridge becomes miner discretion.

1. **Destroy is visible** — `<HASH160> 0xbd`, value `B`, not inserted
   into the UTXO set, ORU inputs only, at most one positive Convergence
   output per transaction.
2. **Mint is same-transaction** — `R` is the sum of Witness v1 outputs
   in that body transaction. The miner who includes it receives no RES.
3. **The predicate is integer** — same `R`, `B`, `h` ⇒ same
   admit-or-reject. No unpublished rate.

`R = 0` is a legal plain destroy. Ordinary RES transfers cannot create
RES (`res_in ≥ res_out`). RES cannot convert back to ORU.

## 4.2 Not a 1:1 counter, not a draw

A 1:1 swap would rename flow into stock and collapse Chapter 1’s
triangle. The protocol cost rises with height. The new RES is a new
UTXO, not a colour on the old output.

A draw would hand “who receives RES” to a hash. Convergence has no
random term. Pool policy cannot allocate RES. Changing a miner payout
address cannot create RES. Fairness is the same public inequality, the
same decision.

`destroy_cost(h)` is a protocol destroy requirement, not a market quote.
Display form is `1024 · (2²⁵ + h)² / 2⁵⁰`. Consensus never evaluates
it. There is no per-block mint cap.

## 4.3 When verification fails

`bad-txns-res-mint-underpaid` — destroy does not cover `R`.
`bad-txns-res-mint-no-burn` — RES output, no `OP_CONVERGE`.
`illegal-res-to-oru-conversion` — wrong direction, or spending the
destroy. Underpayment invalidates the whole transaction.

Mint maturity 100 is reorg protection, the same depth as coinbase. It
is not a wait for a draw.

**Destroy is visible. Mint is an integer predicate in the same
transaction.** Chapter 5 unfolds that predicate as checkable
mathematics.

---

# Chapter 5 — Checkable Mathematics

Chapter 4 fixed the verification act. This chapter unfolds the
predicate: display and consensus are one identity; the ceiling is an
integral; fairness is the same public inputs yielding the same
decision. The expanded form is
[`chapters/05-mathematical-fairness.md`](./chapters/05-mathematical-fairness.md)
(Chinese: [`chapters/05-mathematical-fairness.zh.md`](./chapters/05-mathematical-fairness.zh.md)).

## 5.1 Fairness is a public decision function

```
same (R, B, h)  →  same admit or reject
```

No fourth hidden parameter. No unpublished rate. No random term. Anyone
who re-evaluates a confirmed transaction must get the same yes or no as
the network. That is not “outcomes look even.” It is that the decision
function is public, deterministic, and exact.

## 5.2 The shift is forced

Display: `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰`. Consensus never
divides. Clearing the division is an identity:

```
R · 1024 · (2²⁵+h)²  ≤  B · 2⁵⁰
R · (2²⁵+h)²         ≤  B · 2⁴⁰
```

so `RES_DESTROY_SHIFT = 50 − log₂(1024) = 40`. Change genesis cost and
leave the shift, and the predicate no longer expresses that curve.
Consensus evaluates one multiplication, one squaring, one shift. No
rounding direction: `check_mint` is exact. `destroy_cost(0)` is exactly
1024. No free mint is representable.

## 5.3 The ceiling is derived

`S = 2⁷`, `P0 = 2¹⁰`, `H = 2²⁵`. The exponent 2 makes

```
∫_0^∞ S / destroy_cost(h) dh = (S / P0) · H = 2²²
```

with no residual. Checking the cap is checking exponents.
`cumulative(h) = 2²² · h / (h + 2²⁵)`. Because cost is strictly
increasing, the exchange argument says any delay yields less RES: the
cap needs no consensus counter. Realised stock is `ρ · 2²²`.

`128 = 2⁷` is the numerator of that identity, not an independent lucky
number. It is the miner-budget floor and the cap numerator in the same
list of powers of two.

## 5.4 Four checkable layers

| Layer | What is public |
|-------|----------------|
| Predicate | `(R, B, h)` — no hash, no hidden rate |
| Identity | Display form ↔ `<< 40` — shift forced by genesis cost |
| Ceiling | `(S / P0) · H = 2²²` — read off the integral |
| Exchange | Strictly increasing cost — delay yields less |

Earlier minters pay a lower unit cost. That is duration-proportional
payment, not a hole. Fairness is that later arrivals face the same
public curve. Height `h` is the order proof-of-work produces.

**The decision is public. The ceiling is derived. The shift is
forced.** Chapter 6 states how mining supplies that shared height —
it pays ORU and does not mint RES.

---

# Chapter 6 — Proof-of-Work: The Shared Height, Paid in ORU

Chapter 5’s predicate contains `h`. If nodes disagree on height, the
same destroy is judged differently. This chapter states how that height
becomes shared, and what the finder is paid. The expanded form is
[`chapters/06-mining.md`](./chapters/06-mining.md)
(Chinese: [`chapters/06-mining.zh.md`](./chapters/06-mining.zh.md)).
Product path: [`chapters/07-mining-windows.md`](./chapters/07-mining-windows.md).

Mining **pays ORU**. Mining **does not mint RES**.

```
hashpower → valid header → shared height h
        ↓
predicate at that h; coinbase 128 ORU at that h
```

## 6.1 The predicate needs a clock

`destroy_cost(h)` is strictly increasing. Height cannot be a local
clock or a wallet guess. Proof-of-work supplies an order that is not
cheap to rewrite. Confirming height is the block that includes the
transaction. Without that clock, “same `(R, B, h)`” has no public
third component.

## 6.2 What is paid, and where

Genesis subsidy is 0. Every later block pays 128 ORU to the Witness v0
coinbase script. RES, if it appears, appears only in a body
Convergence transaction. Paying RES as a mining reward would inflate
the stock to buy security and undo Chapter 1.

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid           ⇔  OpuHash(header) ≤ target
```

Block identity remains SHA256d of the header. Coinbase states this
block’s height and pays ORU only. Pool policy cannot allocate RES. Changing a payout
address cannot create RES. Finding a header does not legalise an
underpaid mint.

## 6.3 Difficulty is not mint cost

Target spacing 600 s. ASERT half-life 43 200 s, clamp ±12.5%, parent
time/height only. ASERT lets `h` advance with wall time so
`destroy_cost(h)` moves slowly. It does not enter the predicate.

The kernel models most-work finite-tip selection on `GetBlockProof`.
It does not prove `ActivateBestChain`.

**Height is shared. Payment is ORU. RES does not come from mining.**
Validity is not a pool dashboard. Public data can be recomputed —
Chapter 7.

---

# Chapter 7 — Recompute Locally: Do Not Trust a Pool or Explorer Verdict

Chapter 6 said a pool dashboard is not a source of validity. This
chapter states the substitute: public bytes recomputed locally. The
expanded form is
[`chapters/07-self-proving.md`](./chapters/07-self-proving.md)
(Chinese: [`chapters/07-self-proving.zh.md`](./chapters/07-self-proving.zh.md)).

The protocol does not claim that cheating is impossible. It claims that
unequal bytes fail the published checks.

```
public data → independent calculation → compare → accept / reject
```

A pool, explorer, wallet, or website may **display** a conclusion. None
of them may **be** that conclusion.

## 7.1 A display is not a decision

Outsourcing validity to a screen reintroduces the counterparty Chapter
1 refused. The substitute is not a more trustworthy website. It is:
take the public bytes, run the same function, compare.

The node `rigord` does this for every accepted block. A browser Zero
Trust page does it for selected fields. Paper evaluation of
`(R, B, h)` is the same standard: equal passes, unequal fails.

## 7.2 The commitment chain

Destroy `B`, mint `R`, height `h`, coinbase, Merkle root, OpuHash, and
block identity (SHA256d) are all in broadcast data. There is no
internal transaction path.

```
body tx → OP_CONVERGE B → Witness v1 sum R
        → R > 0 → R · (2²⁵ + h)² ≤ B << 40
        → coinbase (this block’s height; ORU only)
        → merkle root → header
        → OpuHash = SHA3-256("RGRPOW01" || header_80)
```

If any step fails to match, the block is invalid. An underpaid mint
that a pool already shows as credited is still invalid.

A fingerprint over public fields is observational. It does not accept
or reject blocks. A match is not a substitute for a full node.

**A conclusion may be displayed. Validity must be recomputed.**
Chapter 8 draws the boundary of formal proof.

---

# Chapter 8 — The Formal Boundary: The Model Is Not the Running Binary

Chapter 7 requires local recomputation. This chapter states the
current final formal conclusion: one public sentence; “yes” stops
inside the model. The expanded form is
[`chapters/08-formal-boundary.md`](./chapters/08-formal-boundary.md)
(Chinese: [`chapters/08-formal-boundary.zh.md`](./chapters/08-formal-boundary.zh.md)).

```
formal verification of the protocol model
        ≠
C++ equivalence
        ≠
running-binary proof
```

## 8.1 Three layers that must not collapse

| Layer | What it is | What it is not |
|-------|------------|----------------|
| Running consensus | C++ `core/src/` realization of RFC-020 / RFC-018 | RFC / Coq rewriting running rules |
| Coq kernel model | Machine-checked properties of the model | A C++ refinement; `ConnectBlock` |
| Differential / extraction | Finite golden vectors; declared runtime path | Universal equivalence |

## 8.2 Snapshot of results (not a protocol parameter)

Kernel: `formal_verification/rigor_kernel/`. Living inventory:
`release/FinalProofInventory.md`. Coq 8.15.0. Classified theorems
**254** (Category A **119**, B **102**, C **33**). `Print Assumptions`
Closed. Maturity **4.9**. Mutation isolation list **127**. Six
extracted names are definitionally the Coq validators. Codec
differential: three declared divergences against production
`CDataStream`. These figures move. They are not consensus parameters.
They are not “254 theorems cover the running node.”

## 8.3 Side-by-side reading is not refinement

The names that receive “yes” in §8.5 share names with this paper.
Side-by-side reading is not a refinement proof. Hash in the model is
uninterpreted `H`. Ed25519 / SHA3 **security** is not proved.
Abstract `sig_ok` is not unforgeability. Those “yes” answers do not
skip Chapter 7.

## 8.4 Outside the boundary

Not proved: C++ ↔ Coq equivalence; `ConnectBlock` refinement;
extracted code computing the Coq function; compiler correctness;
k-deep / honest majority; ASERT envelope; primitive security;
network, hardware, wallet. Finite golden vectors catch regressions.
They do not rewrite agreement at locked points as universal
equivalence. Testing is not proof.

## 8.5 The final formal conclusion

The only public sentence:

> Inside the Coq consensus kernel model, critical consensus
> invariants have been machine-checked, with explicit extraction
> and runtime trust boundaries.

**Yes:** selected invariants inside this Coq model hold — kernel
soundness, ORU conservation, the RES ceiling, classifier partition,
chain induction. `Print Assumptions` is Closed on the public
classification.

**Not yes:** the running C++ node equals the model.

**What is proved is the model. What runs is C++.** Product surfaces
must not read this chapter as “the chain has been proved” — Chapter 9.

---

# Chapter 9 — Product Surfaces: A Display Remains a Display

Chapter 8 drew the line between the model and the running binary.
This chapter states how wallets, explorers, pools, documentation, and
the website live with those rules. The expanded form is
[`chapters/09-product-surfaces.md`](./chapters/09-product-surfaces.md)
(Chinese: [`chapters/09-product-surfaces.zh.md`](./chapters/09-product-surfaces.zh.md)).

A surface may **display** a conclusion. It may not **be** that
conclusion. A green Chapter 8 is not “the chain has been proved.”
Product completeness is not consensus completeness.

```
rules accepted or rejected in the node
        ↓
a surface displays public facts
        ≠
the surface becomes a source of validity
        ≠
a formal-verification page becomes an equivalence proof
```

## 9.1 Why coexistence must be stated

Products exist so people can see balances, shares, height, and
explanation. Sight is a condition of use. Sight is not decision.
If products become authority, the counterparty Chapter 1 refused
moves from the pool dashboard to the homepage copy.

Formal documentation is the most dangerous surface. What it says is
true: the model contains machine-checked invariants. Readers then
supply the unwritten half-sentence — “so the chain has been proved.”
Chapter 8 does not issue that half-sentence. Product copy may not
issue it on Chapter 8’s behalf. Shortening the allowed sentence
rewrites the honesty flags.

## 9.2 What a surface may do

| Surface | It may | It may not |
|---------|--------|------------|
| `rigord` | Accept or reject blocks | Be overridden by a website opinion |
| Wallet | Construct, spend, show balances | Be a validity oracle |
| Pool / mining | Show shares, height, ORU pay | Allocate RES; legalise an underpaid mint |
| Explorer | Retrieve; show fingerprints | Accept or reject blocks |
| Zero Trust | Recompute selected fields | Replace `ConnectBlock` |
| Docs / this paper | Bind the argument | Rewrite `core/src/` |
| Formal verification page | Publish the boundary | Write `equivalence_claimed = true` |
| Lab / experiment | Observe | Act as a refinement proof |
| Website | Entry and explanation | Consensus authority; “bug free” |

Use paths such as [`chapters/07-mining-windows.md`](./chapters/07-mining-windows.md)
are how to click, not another consensus. Living tools:
`/zero-trust-verification`, `/documentation/formal-verification`.
Noun list (not this chapter): [`chapters/09-glossary.md`](./chapters/09-glossary.md).

## 9.3 Three compressions that rewrite Chapter 8

1. Drop the word “model” from a Coq-green badge.
2. Write finite golden vectors as “C++ equals Coq.”
3. Write “do not trust the pool” as “do not run a node.”

Product copy may repeat the one public sentence in Chapter 8. It may
not shorten it.

## 9.4 Documentation work is not a consensus change

Completing guides, checksums, placeholders, and chapter HTML does not
change the predicate, the 128 ORU subsidy, the classifier, or the
derived ceiling. A product “next step” can only mean clearer surfaces,
not a dated rewrite of the rules. No investment yield is promised. No
date is given for filling the derived RES ceiling.

**A display remains a display. The rules remain in the node.** Frozen
parameters and the engineering index follow in the appendix. The
expanded form is
[`chapters/10-appendix.md`](./chapters/10-appendix.md)
(Chinese: [`chapters/10-appendix.zh.md`](./chapters/10-appendix.zh.md)).

---

# Appendix — Frozen Parameters and Engineering Index

Chapters 1–9 are the public argument. This appendix is lookup. It
does not add a claim. The expanded form is
[`chapters/10-appendix.md`](./chapters/10-appendix.md)
(Chinese: [`chapters/10-appendix.zh.md`](./chapters/10-appendix.zh.md)).

Realization lock files: `core/src/consensus/params.h`,
`core/src/consensus/rigor_consensus_lock.h`. The one public sentence
is Chapter 8 §8.5. This appendix does not repeat it.

## A. Frozen public parameters

| Symbol | Value |
|--------|-------|
| ORU subsidy | `R(0) = 0`; 128 ORU per block for `h ≥ 1`; no halving |
| Mint predicate | `R > 0 → R · (2²⁵ + h)² ≤ B << 40` |
| Display form | `destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰` |
| Genesis whole-coin cost | 1024 ORU / 1 RES |
| Pole / shift | `2²⁵` ; `40 = 50 − log₂(1024)` |
| Derived ceiling | `2²²` RES (`419430400000000` atoms) |
| Cumulative ceiling | `2²² · h / (h + 2²⁵)` |
| Mint locus | Body tx: `OP_CONVERGE` + same-tx Witness v1 RES |
| Mint maturity | 100 |
| RFC-018 | Half of RES-transfer ORU fees unclaimable |
| Reverse | RES → ORU forbidden |
| PoW | OpuHash = SHA3-256(`"RGRPOW01" \|\| header_80`) |
| Block identity | SHA256d of the same 80-byte header |
| ASERT | 600 s spacing; 43200 s half-life; ±12.5% clamp |

Probes: `destroy_cost(0) = 102400000000`;
`destroy_cost(210000) = 103685749152`;
`destroy_cost(420000) = 104979520049`.

## B. Mint reject conditions

- `bad-txns-res-mint-underpaid` — `R · (2²⁵ + h)² > B << 40`
- `bad-txns-res-mint-no-burn` — RES output, no `OP_CONVERGE`
- `bad-txns-res-mint-cap-overshoot` — would exceed `419430400000000` atoms

## C. Scripts

Witness v0 / 20 bytes = ORU. Witness v1 / 32 bytes = RES (not
Taproot). `<HASH160> 0xbd` = unspendable Convergence. Coinbase states
this block’s height and pays ORU only. Spend signatures: Ed25519.

## D. Engineering index

`core/src/` is the running consensus. Kernel inventory:
`formal_verification/rigor_kernel/release/FinalProofInventory.md`.
Verdict: `docs/verification/FORMAL-VERIFICATION-VERDICT.md`.
Reproduce: `docs/security/HOW_TO_VERIFY_RIGOR.md`. Live issuance
docs: RFC-020, RFC-018. Noun list:
[`chapters/09-glossary.md`](./chapters/09-glossary.md).

## E. Terms

| Term | Official meaning |
|------|------------------|
| ORU | Medium-of-exchange asset. Subsidy 128 after genesis |
| RES | Store-of-value asset. Derived ceiling `2²²`; not mined |
| Convergence | Same-tx destroy that MAY mint RES under the predicate |
| `destroy_cost(h)` | Display cost. Consensus uses the inequality |
| OpuHash | Live proof-of-work hash |
| Fingerprint | Observational identity. Does not accept or reject blocks |

Lookup ends here. The claims are in Chapters 1–9.
