# RIGOR Architecture Paper v1.1

**A Minimal and Verifiable Proof-of-Work Monetary Architecture**

| Field | Value |
|-------|-------|
| Document status | Public architecture narrative (not consensus SSOT) |
| Version | 1.1 |
| Date | 2026-09-18 |

This document describes architectural intent. Live admit-or-reject is
defined by consensus code together with RFC-020 and RFC-018. This paper
does not rewrite those rules.

Protocol SSOT: [`../protocol/RFC-020.md`](../protocol/RFC-020.md),
[`../protocol/RFC-018.md`](../protocol/RFC-018.md).
Implementation facts:
[`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).
Related protocol narrative:
[`../whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`](../whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md).

## Contents

1. [Design Motivation](#chapter-1--design-motivation)
2. [Bitcoin Design Principles and RIGOR Extensions](#chapter-2--bitcoin-design-principles-and-rigor-extensions)
3. [Script-as-Asset Architecture](#chapter-3--script-as-asset-architecture)
4. [Convergence: The One-Way Monetary Bridge](#chapter-4--convergence-the-one-way-monetary-bridge)
5. [Minimal Consensus and Formal Verification Boundary](#chapter-5--minimal-consensus-and-formal-verification-boundary)
6. [Proof-of-Work: Shared Ordering and the ORU Security Budget](#chapter-6--proof-of-work-shared-ordering-and-the-oru-security-budget)
7. [Local Verification: Independent Validation of Public State](#chapter-7--local-verification-independent-validation-of-public-state)
8. [Formal Verification Boundary: Model, Implementation, and Proof Scope](#chapter-8--formal-verification-boundary-model-implementation-and-proof-scope)
9. [Product Surfaces: Wallets, Explorers, Mining Tools, and Human Interfaces](#chapter-9--product-surfaces-wallets-explorers-mining-tools-and-human-interfaces)
10. [Security Model and Threat Boundaries](#chapter-10--security-model-and-threat-boundaries)
11. [Design Trade-offs](#chapter-11--design-trade-offs)
12. [Deployment and Evolution Policy](#chapter-12--deployment-and-evolution-policy)

[Closing Statement — Architecture Before Expansion](#closing-statement--architecture-before-expansion)

---

## Abstract

RIGOR is a protocol architecture exploration based on Proof-of-Work and
the UTXO model.

It asks a simple design question:

> If a monetary system does not require a single native asset to perform
> every economic function, but instead separates different monetary
> roles at the consensus layer, what architectural properties emerge?

Bitcoin demonstrated that a permissionless digital monetary system can
be built from a small number of fundamental components:

- Proof-of-Work chain ordering
- UTXO-based ownership
- deterministic validation
- fixed consensus rules

RIGOR preserves these foundations.

It explores a different approach to native monetary roles.

The protocol defines two native asset identities:

**ORU** serves as:

- transaction medium
- network security budget
- Proof-of-Work incentive asset

**RES** serves as:

- long-term reserve asset
- scarce value preservation mechanism

RES is not created through mining.

It can only emerge through a deterministic and irreversible process:
destroying ORU and satisfying consensus-defined mathematical conditions.

The transformation is performed by the protocol itself.

It does not depend on:

- administrative issuance
- external asset registries
- token metadata
- smart-contract execution
- general-purpose scripting

In RIGOR:

- Asset identity belongs to the ledger.
- Validity belongs to consensus.

Every node evaluates the same data under the same rules.

The protocol does not attempt to maximize expressive capability.

Instead, RIGOR explores another direction: reducing consensus complexity
in order to improve protocol understandability, verifiability, and
long-term stability.

---

## Chapter 1 — Design Motivation

### 1.1 The Challenge of a Single Asset Serving Multiple Monetary Roles

Many digital monetary systems require one native asset to simultaneously
serve several functions:

- medium of exchange
- miner incentive
- network security budget
- long-term value storage

These functions can interact in complex ways.

An asset optimized for everyday exchange naturally tends toward frequent
circulation.

An asset optimized for long-term reserve characteristics naturally tends
toward reduced velocity and scarcity preservation.

Bitcoin chose a unified approach: one native asset coordinating all
monetary functions.

This is a coherent and historically significant design decision.

RIGOR explores a different design space:

> Can monetary roles be separated at the protocol layer while preserving
> the fundamental properties of Proof-of-Work and UTXO-based consensus?

### 1.2 Not a Replacement for Bitcoin, but an Extension of the Design Space

RIGOR does not consider Bitcoin's design principles incorrect.

On the contrary, RIGOR is built upon several properties that Bitcoin
demonstrated:

- decentralized transaction ordering
- UTXO ownership model
- independent node verification
- deterministic consensus rules

The difference is not the foundation.

The difference is the allocation of monetary roles.

Bitcoin assigns multiple monetary functions to one native asset.

RIGOR explores whether those functions can be separated into distinct
native asset identities while maintaining a minimal verification model.

### 1.3 Design Constraint: Separation Without Additional Consensus Complexity

A central design constraint of RIGOR is:

> Separate monetary roles without introducing a parallel accounting
> system.

Therefore, RIGOR does not introduce:

- a second ledger
- account-based balances
- external asset databases
- application-layer monetary logic

Instead:

- Both ORU and RES exist within a single UTXO state space.
- Asset identity is determined by consensus-defined `scriptPubKey`
  classification.
- The ledger does not store an additional asset registry.
- The node derives asset type from the same object that defines
  ownership and spending conditions.

### 1.4 The Core Design Principle

RIGOR follows a simple principle:

> Add monetary distinction, not consensus complexity.

The protocol attempts to preserve a narrow validation boundary:

- UTXO state transition
- script-defined asset identity
- deterministic issuance conditions
- Proof-of-Work ordering

Everything outside this boundary remains a product, wallet, or
application concern.

---

## Chapter 2 — Bitcoin Design Principles and RIGOR Extensions

### 2.1 Proof-of-Work Ordering and Domain Separation

Bitcoin introduced a fundamental idea:

A permissionless network can establish a shared ordering of events
through proof-of-work rather than trusted coordination.

RIGOR preserves this principle.

Blocks are ordered by accumulated proof-of-work. The chain history is
not determined by timestamps, validators, or external coordination, but
by verifiable computational expenditure.

However, RIGOR separates two concepts that are often coupled in
traditional proof-of-work systems:

- block identity
- proof-of-work validity

The block identity remains Bitcoin-compatible:

```
BlockHash = SHA256d(header_80)
```

The 80-byte block header remains the canonical identity commitment.

Mining validity uses a separate domain:

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
```

A valid block satisfies:

```
OpuHash(header) ≤ target
```

The prefix `"RGRPOW01"` creates an explicit cryptographic domain
separation boundary.

The purpose is not to replace Bitcoin's block identity, but to separate:

```
chain identity  ≠  proof-of-work function
```

This separation provides several properties:

- block identifiers remain stable and independent
- the mining function has an explicit protocol domain
- accidental cross-protocol hash reuse is reduced
- future analysis can reason about PoW validity independently from block
  naming

RIGOR treats proof-of-work as an ordering mechanism first.

The output of mining is not an asset allocation mechanism.

Mining creates shared height.

Mining does not create RES.

### 2.2 Block Identity and Consensus Validity Separation

A Bitcoin-derived system has two important verification questions:

1. Is this block the block being referenced?
2. Is this block valid under consensus?

RIGOR keeps these questions separate.

The block hash answers: **"What block is this?"**

Consensus validation answers: **"Is this block allowed?"**

The distinction is important because RIGOR introduces an additional
monetary transformation:

```
ORU destruction
        ↓
RES creation
```

This transformation must not depend on:

- miner preference
- pool accounting
- explorer interpretation
- coinbase construction

Therefore:

- block identity is committed by `SHA256d`
- PoW validity is checked through `OpuHash`
- monetary validity is checked through transaction consensus rules

Each layer has a single responsibility.

### 2.3 Single UTXO State Space

RIGOR retains the Bitcoin UTXO ownership model.

The ledger remains:

```
chainstate/
    coins database
```

A coin is identified by `(txid, n)` and contains:

```
Coin = (nValue, scriptPubKey)
```

There is no parallel asset ledger.

There is no `(asset_id, amount, script)` triple.

There is no:

```
ORU database
    +
RES database
```

The fundamental state object remains the Bitcoin object: **UTXO**.

The difference is that RIGOR extends what a UTXO can represent.

### 2.4 Script-as-Asset: Identity Through ScriptPubKey

The central design extension of RIGOR is:

> Asset identity is determined by the locking script itself.

The ledger does not store an external asset label.

Instead:

```
ClassifyRigorTxout(scriptPubKey)
        ↓
AssetType
```

returns:

- `ORU`
- `RES`
- `NONE`
- `INVALID`

Asset identity is therefore a consensus function.

Not:

- wallet interpretation
- explorer metadata
- address prefix
- indexer database

The same bytes must produce the same classification on every node.

The rule is:

> Identity lives in the script. Legitimacy lives in consensus.

### 2.5 Closed Consensus Templates and Fail-Closed Design

RIGOR does not introduce a general-purpose script VM.

Instead, consensus recognizes a closed set of templates.

Current templates:

| Script template | Classification |
|-----------------|----------------|
| Witness v0 / 20 bytes | ORU |
| Witness v1 / 32 bytes | RES |
| Convergence destroy script | ORU destruction |
| `OP_RETURN` zero-value | NONE |
| Everything else | INVALID |

Unknown scripts are not interpreted.

They are rejected.

This follows a Bitcoin-like conservative principle:

> What is not explicitly defined is not consensus.

The system fails closed.

- An unknown witness version does not become spendable.
- An unknown script does not become an implicit asset.
- A future asset type requires an explicit consensus change.

### 2.6 Dual-Pool Conservation

Although RIGOR uses one UTXO state space, it maintains separate
conservation domains.

Each output contains:

- one asset identity
- one amount
- one locking script

An output cannot simultaneously represent `ORU + RES`.

The accounting model is:

```
ORU pool    independent
RES pool    independent
```

Therefore:

**ORU conservation:**

```
sum(inputs ORU) ≥ sum(outputs ORU)
```

**RES conservation:**

```
sum(inputs RES) ≥ sum(outputs RES)
```

Network fees are ORU only.

RES does not become a fee medium.

This keeps the monetary roles separated:

```
ORU = medium of exchange + security budget
RES = scarcity mechanism
```

### 2.7 Native Assets Without Parallel Ledgers

Many multi-asset systems introduce:

```
base ledger
    +
asset registry
```

RIGOR intentionally avoids this model.

There is no registry saying: `UTXO X contains RES`.

The UTXO itself already contains the information:

```
scriptPubKey → asset identity
```

This removes an additional consensus object.

The node does not ask:

> Which asset database says this output belongs to?

It asks:

> What does this locking script define?

This preserves the Bitcoin property:

```
validate bytes
    not external state
```

### 2.8 Bitcoin Compatibility of Design Philosophy

RIGOR does not attempt to expand Bitcoin into a general execution
platform.

Instead, it follows several Bitcoin design principles:

- explicit state transitions
- deterministic validation
- minimal consensus surface
- local verification
- no trusted issuer
- no external registry
- no hidden parameters

The extension is not:

```
Bitcoin + more features
```

The extension is:

```
Bitcoin principles
        +
a different native monetary partition
```

RIGOR explores whether two monetary functions — medium of exchange and
store of value — can exist as separate native assets while preserving:

```
UTXO simplicity
    +
consensus determinism
    +
independent verification
```

The design intentionally sacrifices programmability to preserve
verifiability.

The objective is not maximum capability.

The objective is a smaller, explicitly defined monetary machine.

---

## Chapter 3 — Script-as-Asset Architecture

Chapter 2 described the architectural direction: preserving Bitcoin's
UTXO discipline while separating monetary functions at the native asset
layer.

This chapter defines the central structural choice of RIGOR:

> Asset identity is not stored beside the UTXO. Asset identity is the
> UTXO locking condition itself.

This is the principle of **Script-as-Asset**.

RIGOR does not introduce a parallel asset registry.

It does not attach metadata to outputs.

It does not rely on explorers, wallets, or indexers to interpret asset
ownership.

The same consensus bytes must produce the same asset classification on
every full node.

```
scriptPubKey
      ↓
ClassifyRigorTxout
      ↓
Asset Identity
      ↓
Consensus Validation
```

Identity is determined locally from the ledger itself.

### 3.1 The Ledger Object Remains a Bitcoin UTXO

RIGOR uses a single UTXO state space.

The underlying ledger object remains:

```
TxOut = (nValue, scriptPubKey)
```

The chainstate remains a single coins database:

```
datadir/chainstate
```

A coin is identified by `(txid, n)` and stores:

```
Coin {
    nValue,
    scriptPubKey
}
```

There is no second asset database.

There is no asset index required for consensus.

There is no additional `asset_id` field inside `CTxOut` or `Coin`.

The state model remains:

```
one UTXO
one amount
one locking condition
```

The difference is that the locking condition determines which native
monetary identity that UTXO represents.

### 3.2 Script-as-Asset: The Asset Identity Function

In many multi-asset systems, asset identity is introduced as additional
state:

```
UTXO
    +
asset identifier
    +
amount
```

RIGOR intentionally avoids this.

Instead:

```
scriptPubKey → AssetType
```

The consensus classifier is `ClassifyRigorTxout(scriptPubKey)`, which
determines:

- `ORU`
- `RES`
- `NONE`
- `INVALID`

The asset function is therefore:

```
AssetTypeFromScriptPubKey(scriptPubKey)
```

This function is the single source of truth.

Every consensus component derives from this classification:

```
ClassifyRigorTxout
        ↓
AssetTypeFromScriptPubKey
        ↓
Solver
IsAllowedScript
CheckTxInputs
VerifyRigorPredicate
```

Wallets and RPC interfaces do not independently infer assets.

They do not determine asset identity from:

- address prefix
- wallet metadata
- explorer labels
- witness version alone

They call the same classification logic.

The rule is:

> Asset identity is a property of consensus bytes, not a property of
> software interpretation.

### 3.3 Native Asset Templates

RIGOR deliberately uses a closed template system.

Consensus does not execute arbitrary scripts.

Consensus recognizes explicitly defined output forms.

The current template set is:

| Script Shape | Asset Identity | Consensus Role |
|--------------|----------------|----------------|
| Witness v0, 20-byte key hash | ORU | Spendable ORU output |
| Witness v1, 32-byte Ed25519 program | RES | Spendable RES output |
| Convergence destroy script | ORU destruction | Not a UTXO |
| `OP_RETURN` zero-value | NONE | Data carrier |
| Anything else | INVALID | Rejected |

The classification is deterministic.

The same script bytes always produce the same result.

#### ORU Output

ORU is represented by Witness v0, 20-byte key hash.

Example shape:

```
0x00 0x14 <20 bytes>
```

This is the normal transferable monetary output.

ORU serves:

- exchange medium
- miner compensation
- transaction fee payment

#### RES Output

RES is represented by Witness v1, 32-byte Ed25519 program.

Example shape:

```
0x51 0x20 <32 bytes>
```

This is not Bitcoin Taproot.

The witness version is used as a minimal classification mechanism, not
as a generalized script execution environment.

RES ownership is defined by the locking program.

#### Convergence Destroy Output

Convergence uses a special irreversible destruction output:

```
<HASH160> 0xbd
```

with positive value.

This output:

- destroys ORU
- does not enter the UTXO set
- cannot be spent
- cannot become a hidden reserve

It is a visible consensus event.

#### NONE

The protocol recognizes `0x6a` with `nValue = 0` as a non-asset data
carrier.

It has no monetary identity.

#### INVALID

Everything outside the defined templates is `INVALID`.

Examples:

- legacy P2PKH
- P2SH
- P2WSH
- unknown witness versions
- unsupported programs

Unknown does not mean spendable.

### 3.4 Fail-Closed Consensus

RIGOR adopts a conservative rule:

> Undefined behavior is invalid behavior.

The protocol does not attempt to interpret unknown scripts.

This prevents a common source of complexity:

```
unknown script
        ↓
maybe asset
        ↓
external interpretation
        ↓
consensus ambiguity
```

Instead:

```
unknown script
        ↓
INVALID
        ↓
rejected
```

Consensus errors become visible failures rather than hidden
interpretations.

Therefore:

- `ASSET_INVALID` outputs are consensus-invalid.
- Spending `ASSET_NONE` or `ASSET_INVALID` coins is invalid.

A future asset type requires:

- new consensus specification
- new classifier rule
- new conservation rule

It cannot appear accidentally.

### 3.5 One Amount Per Output

RIGOR does not create multi-dimensional UTXOs.

A single output contains:

```
one asset identity
    +
one amount
```

It does not contain:

```
{
    ORU amount,
    RES amount
}
```

inside the same output.

Therefore `TxOut` is never `(ORU, RES, script)`.

It remains `(nValue, scriptPubKey)`.

The asset is determined by the script.

The amount is determined by `nValue`.

This keeps the state transition model close to Bitcoin's original UTXO
model.

### 3.6 Dual-Pool Conservation

Although RIGOR uses one UTXO database, monetary accounting occurs in
separate conservation domains.

The ledger contains:

```
ORU UTXOs
    +
RES UTXOs
```

but the accounting rules remain independent.

#### ORU Conservation

Normal ORU transactions:

```
sum(ORU inputs) ≥ sum(ORU outputs)
```

The difference becomes the transaction fee.

#### RES Conservation

RES transfers:

```
sum(RES inputs) ≥ sum(RES outputs)
```

RES cannot create additional supply through ordinary transfers.

RES also does not pay network fees.

Fees remain an ORU function.

#### ORU → RES Conversion

The only permitted cross-domain transition is:

```
ORU destruction
        ↓
RES creation
```

through Convergence.

The transaction must contain:

- ORU inputs
- exactly one `OP_CONVERGE` destruction
- RES outputs

and satisfy:

```
CheckResMintArithmetic(R, B, height)
```

#### RES → ORU Conversion

The reverse direction does not exist.

`RES → ORU` is invalid.

This is a fundamental monetary boundary.

The two assets are related by a one-way transformation, not an exchange
pair.

### 3.7 Spend Authorization Follows the Lock

RIGOR does not maintain a global asset permission table.

The spending rule follows from the locking script.

An output says "I am an ORU output" or "I am a RES output" through its
script.

The corresponding spend authorization is derived from that lock.

Different domains use different signature contexts:

- `RGRORU01`
- `RGRRES01`
- `RGRMNT01`

The signature domain separation prevents accidental authorization
overlap between:

- ORU spending
- RES spending
- mint-related operations

Ownership remains local to the UTXO.

### 3.8 Why Script-as-Asset Instead of a Registry?

A registry model would introduce:

```
UTXO state
    +
asset state
```

The node would need to answer:

> According to which external table does this output represent RES?

RIGOR avoids this question.

The node asks only:

> What does this output's locking script define?

This preserves a Bitcoin-like verification philosophy:

```
public bytes
        ↓
local calculation
        ↓
accept / reject
```

No trusted metadata.

No issuer database.

No explorer consensus.

No external asset map.

### 3.9 Extensibility Through Consensus Templates

Script-as-Asset does not mean unlimited assets.

It means assets are introduced through explicit consensus design.

The current rule is:

```
unknown template = invalid
```

A future native asset could theoretically use the same UTXO database by
adding:

```
new script template
    +
new conservation rule
```

No database migration would be required because the UTXO schema does not
contain an asset field.

However, adding such an asset would be a consensus change.

The classifier is the boundary.

### 3.10 The Architectural Result

RIGOR's Script-as-Asset architecture produces a specific design
outcome:

```
Single UTXO state space
        +
Script-defined asset identity
        +
Closed consensus templates
        +
Independent asset conservation
        +
No parallel registry
```

The result is not a general asset platform.

It is a deliberately constrained monetary architecture.

The design choice is:

```
less implicit state
        ↓
less interpretation
        ↓
smaller consensus surface
        ↓
more direct verification
```

RIGOR does not attempt to maximize what the ledger can represent.

It attempts to minimize what every node must understand.

Identity lives in the script.

Legitimacy lives in consensus.

Verification lives in the node.

---

## Chapter 4 — Convergence: The One-Way Monetary Bridge

Chapter 3 defined how ORU and RES exist as native ledger objects.

They are not entries in a registry.

They are not annotations attached to ordinary outputs.

They are two consensus-defined monetary identities inside one UTXO state
space.

However, a dual-asset architecture requires a clearly defined
relationship between these identities.

RIGOR does not create a market exchange mechanism between ORU and RES.

It does not define a swap.

It does not define a conversion pool.

It does not define a miner allocation.

Instead, RIGOR defines a single irreversible monetary transformation:

```
ORU destruction
        ↓
RES creation
```

This mechanism is called **Convergence**.

The purpose of Convergence is not to create a second payment system.

The purpose is to separate two monetary functions:

```
ORU
    medium of exchange
    +
    security budget

RES
    scarcity-oriented reserve asset
```

The protocol does not assume these functions should share the same
issuance path.

### 4.1 Why a One-Way Bridge Exists

Bitcoin combines several monetary functions into one asset:

- transaction medium
- miner compensation
- long-term scarcity
- reserve characteristics

This simplicity is one of Bitcoin's strengths.

However, it also creates a design question:

> Can different monetary functions be separated while preserving
> Bitcoin's verification principles?

RIGOR explores this question by separating security payment and scarcity
creation at the consensus layer.

The result is:

```
Mining
    ↓
ORU

Convergence
    ↓
RES
```

The two mechanisms are intentionally independent.

Mining does not create RES.

Convergence does not create ORU.

Each monetary function has a single origin.

### 4.2 RES Is Not Miner-Issued

A critical design choice: **RES is not a mining reward.**

A traditional dual-reward system might define:

```
Block found
    ↓
Miner receives
ORU + RES
```

RIGOR intentionally rejects this model.

If miners directly receive RES:

- security production and scarcity production become coupled
- miner strategy influences reserve distribution
- the store-of-value asset becomes a security subsidy
- monetary roles become mixed again

Instead:

```
Block production
    ↓
ORU subsidy
    only
```

The coinbase creates ORU and never creates RES.

RES appears only through:

- body transaction
- visible ORU destruction
- deterministic consensus predicate

This creates a clear boundary:

> PoW creates ordering.
>
> Convergence creates reserve supply.

### 4.3 The Convergence Transaction

A valid Convergence transaction contains three logical components:

```
ORU inputs
        ↓
OP_CONVERGE destruction
        ↓
RES outputs
```

The structure is:

```
ORU inputs
        |
        v
exactly one Convergence destroy output B
        |
        v
Witness v1 RES outputs R
```

The destroy output is not a UTXO.

It is consumed by consensus.

The network observes:

- B = destroyed ORU amount
- R = newly created RES amount
- h = confirming block height

The mint is admitted only by the integer implication:

```
R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

`R = 0` is a legal plain destroy: ORU is removed and no RES is minted.

This is the entire monetary bridge.

No hidden parameter.

No external rate.

No oracle.

No miner decision.

### 4.4 The Deterministic Conversion Formula

The visible display form is:

```
destroy_cost(h) = 1024(2²⁵ + h)² / 2⁵⁰
```

This describes the ORU destruction requirement per RES unit.

However, consensus does not calculate a floating-point cost.

Consensus evaluates the exact integer form:

```
R · (2²⁵ + h)² ≤ B << 40
```

The transformation is mathematically equivalent.

Starting from:

```
R × destroy_cost(h) ≤ B
```

substitute:

```
destroy_cost(h) = 1024(2²⁵ + h)² / 2⁵⁰
```

therefore:

```
R × 1024(2²⁵ + h)² ≤ B × 2⁵⁰
```

because 1024 = 2¹⁰, the expression becomes:

```
R × (2²⁵ + h)² ≤ B × 2⁴⁰
```

or:

```
R · (2²⁵ + h)² ≤ B << 40
```

The consensus implementation therefore performs:

- one multiplication
- one squaring operation
- one integer comparison

No rounding.

No approximation.

No consensus counter.

### 4.5 Height as the Monetary Time Index

The Convergence curve uses:

```
h = confirming block height
```

not:

- wall-clock time
- miner timestamp
- external oracle

This follows the Bitcoin principle:

> Proof-of-work produces a shared ordering of events.

The height exists because blocks exist.

The chain provides the ordering.

The ordering provides the monetary schedule.

A participant cannot claim:

> My local clock says this conversion should cost less.

The network evaluates:

```
same R
same B
same h
        ↓
same decision
```

### 4.6 Mint Is a Public Predicate

Convergence evaluates one public inequality on the confirming
transaction:

```
(R, B, h)
        ↓
consensus predicate
        ↓
accept / reject
```

The question is: **Does this transaction satisfy the public monetary
rule?**

The result is deterministic. Every node reaches the same conclusion.
A miner cannot create RES by finding a block. A pool cannot allocate
RES through payout policy.

### 4.7 Why There Is No 1:1 Exchange Rate

A direct exchange model would look like:

```
1 ORU
    ↓
1 RES
```

or a fixed ratio.

RIGOR intentionally does not do this.

A fixed exchange rate would make RES simply another representation of
ORU.

The distinction between medium of exchange and reserve scarcity would
disappear.

Instead, Convergence introduces a protocol-defined increasing cost
schedule.

Early participation and later participation are evaluated differently
because height is part of the rule.

The protocol does not create a market price.

It creates a deterministic transformation cost.

### 4.8 Why Destruction Must Be Visible

A key requirement: destroy and mint must happen in the same transaction.

If destruction and creation happened separately:

```
Transaction A
    destroy ORU

Transaction B
    create RES
```

the ledger would contain a period where credit exists without visible
backing.

The bridge would become an accounting promise.

RIGOR avoids this.

The transaction itself contains:

```
proof of destruction
    +
requested creation
```

Therefore:

```
no destroy
        ↓
no mint
```

The relationship exists only inside one consensus transition.

### 4.9 Why the Miner Cannot Create RES

A miner controls:

- block selection
- transaction ordering
- coinbase construction

A miner does not control asset creation rules.

If RES were allowed in coinbase:

```
miner finds block
        ↓
miner creates RES
```

then RES supply would depend on miner discretion.

Instead:

```
miner finds block
        ↓
receives ORU

user transaction
        ↓
may create RES
```

The miner includes a valid state transition.

The miner does not authorize one.

### 4.10 Monetary Separation as a Bitcoin Design Extension

The Bitcoin design philosophy can be summarized as:

```
rules first
trust minimized
verification local
state explicit
```

RIGOR applies the same principle to monetary roles.

Instead of one asset with many economic functions, RIGOR explores two
native assets, two defined functions, and one verification system.

The architecture becomes:

```
Proof-of-work
        ↓
shared height
        ↓
ORU issuance

Visible destruction
        ↓
Convergence predicate
        ↓
RES creation
```

Each mechanism has one purpose.

### 4.11 What Convergence Does Not Claim

Convergence defines:

- a deterministic issuance rule
- a visible destruction mechanism
- a one-way monetary relationship

It does not define:

- market value
- exchange price
- demand
- profitability
- guaranteed adoption

The protocol determines the transformation.

The market determines whether participants use it.

### 4.12 The Architectural Result

Convergence creates a monetary bridge with strict boundaries:

```
ORU    can become    RES
RES    cannot become ORU
```

The bridge is:

- visible
- deterministic
- one-way
- locally verifiable

The result is not a token exchange layer.

It is a consensus-defined monetary transformation.

Mining secures the ledger.

Convergence transforms monetary function.

Consensus defines the boundary.

---

## Chapter 5 — Minimal Consensus and Formal Verification Boundary

Chapter 4 described Convergence as a deliberately narrow monetary
transformation.

This chapter addresses the corresponding engineering principle:

> RIGOR does not attempt to make consensus understand more. It attempts
> to make consensus understand only what is necessary.

This is an important distinction.

Minimalism here is not a stylistic preference.

It is a constraint on the protocol's computational and semantic
boundary.

The objective is not to make the paper short.

The objective is to make the consensus system small enough that its
important rules can be inspected, independently implemented, tested, and
formally modelled.

RIGOR therefore treats the consensus surface itself as a protocol
resource.

### 5.1 Consensus Is the Smallest Necessary Boundary

A consensus rule is different from an application feature.

A wallet may contain:

- address management
- transaction construction
- balance presentation
- fee estimation
- transaction history

An explorer may contain:

- indexing
- search
- charts
- historical analytics

A mining pool may contain:

- worker accounting
- share tracking
- payout logic
- dashboards

None of these should determine whether a block is valid.

Consensus has a narrower responsibility:

```
public block bytes
        +
public transaction bytes
        +
previous consensus state
        ↓
valid / invalid
```

RIGOR deliberately keeps the monetary rules inside this boundary and
leaves everything else outside it.

The principle is:

> If a rule does not need to determine ledger validity, it should not
> become a consensus rule.

### 5.2 A Closed Monetary Machine

RIGOR's production consensus does not contain a general-purpose
execution environment.

There is no arbitrary Script VM.

There is no contract state.

There is no global asset registry.

There is no external oracle.

There is no consensus database containing application-level balances.

Instead, the monetary surface consists primarily of:

```
UTXO classification
    +
spend authorization
    +
asset conservation
    +
Convergence predicate
    +
PoW validation
```

This creates a relatively explicit dependency structure:

```
scriptPubKey
      ↓
asset classification
      ↓
allowed spend / conservation
      ↓
transaction validity
      ↓
block validity
```

The Convergence path is similarly bounded:

```
R
B
h
        ↓
integer predicate
        ↓
accept / reject
```

There is no hidden execution path between these operations.

### 5.3 The Consensus Surface

The consensus surface is the collection of inputs and rules that can
change the validity decision.

For RIGOR, the important inputs include:

- previous UTXO state
- transaction bytes
- output scripts
- input amounts
- block height
- block header
- proof-of-work target
- relevant consensus parameters

The objective is to avoid unnecessary sources of validity.

For example, the following are not consensus authorities:

- wallet metadata
- explorer database
- pool accounting
- web UI
- external price
- displayed balance
- local application state

They may interpret public data.

They do not define the ledger.

### 5.4 Classification Before Interpretation

The Script-as-Asset architecture provides an important simplification.

Instead of allowing multiple consensus components to independently
determine what an output means, RIGOR defines a single classification
boundary:

```
ClassifyRigorTxout(scriptPubKey)
```

Conceptually:

```
scriptPubKey
      ↓
┌───────────────────────┐
│ ClassifyRigorTxout    │
└───────────────────────┘
      ↓
ORU / RES / NONE / INVALID
```

Consensus logic then operates on that classification.

This matters because duplicated interpretation creates divergence risk.

If one subsystem says `Witness v1 = RES` while another subsystem
independently interprets the same bytes differently, the protocol has
multiple definitions of identity.

RIGOR instead attempts to establish one classification function and
derive downstream checks from it.

The architectural rule is:

> Classify once; validate according to the classification.

### 5.5 Fail-Closed Is Part of the Security Boundary

Minimal consensus does not mean permissive consensus.

In fact, the opposite is often required.

RIGOR uses a closed template set.

Therefore:

```
known valid template
        ↓
classified
        ↓
validated
```

while:

```
unknown template
        ↓
INVALID
        ↓
rejected
```

This is important for long-term protocol evolution.

An unknown script must not acquire accidental monetary meaning merely
because some implementation happens to parse it.

Future extensibility therefore requires an explicit consensus change.

The boundary is visible.

### 5.6 Asset Conservation Is Local

RIGOR's dual-asset accounting does not require a global balance table.

For an ordinary transaction, the node examines the relevant inputs and
outputs.

ORU conservation is checked independently.

RES conservation is checked independently.

The important distinction is:

```
one ledger state
        ≠
one combined monetary pool
```

The node does not evaluate `ORU + RES` as one interchangeable quantity.

Instead:

```
ORU pool → ORU rules
RES pool → RES rules
```

The only cross-pool operation is the explicitly defined Convergence
path.

This keeps the transformation boundary visible.

### 5.7 Convergence Has a Small Mathematical Surface

The RES issuance rule is intentionally expressed through a compact
integer predicate:

```
R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

Its public inputs are R, B, and h.

There is no:

- random seed
- oracle value
- market price
- miner-selected rate
- global issuance counter

The node does not ask an external system how much RES should be created.

It evaluates the rule.

This makes the central monetary transformation particularly suitable for
independent implementations and mathematical analysis.

### 5.8 Minimality and Attack Surface

Every consensus feature introduces potential implementation surface.

A general execution engine introduces:

```
more opcodes
    +
more state
    +
more execution paths
    +
more edge cases
```

A global registry introduces:

```
more state
    +
more synchronization requirements
    +
more lookup semantics
```

An external oracle introduces:

```
external dependency
    +
availability assumptions
    +
trust boundary
```

RIGOR intentionally does not introduce these mechanisms for its monetary
layer.

The intended relationship is:

```
less consensus semantics
        ↓
fewer paths requiring agreement
        ↓
smaller implementation surface
        ↓
more tractable verification
```

This is an engineering objective, not a claim that smaller code
automatically means greater security.

### 5.9 Formal Verification Has a Boundary

RIGOR includes a Coq-based formal model.

The purpose is to establish machine-checked properties of the specified
mathematical system.

But the project explicitly distinguishes three different objects:

```
Protocol model
        ≠
C++ implementation
        ≠
Running binary
```

These distinctions are essential.

A theorem about a Coq function does not automatically prove that the
corresponding C++ function is equivalent.

A successful compilation does not establish refinement.

A finite test vector does not establish universal equivalence.

A running node is still software.

### 5.10 What the Coq Model Can Establish

Within its stated model, formal verification can establish properties
such as:

- ORU conservation
- RES conservation
- RES supply ceiling
- classifier partition
- chain-state induction
- selected consensus invariants

The exact inventory of proved properties is maintained separately from
this architectural paper.

This distinction is deliberate.

The paper describes the verification boundary.

The living inventory records the current engineering state.

Therefore:

```
"the model proves X"
        must not silently become
"the production node is proven correct"
```

Those are different statements.

### 5.11 The C++ Boundary

The production implementation is the running C++ consensus system.

It remains the source of actual network behavior.

Conceptually:

```
RFC rules
        ↓
consensus design
        ↓
C++ implementation
        ↓
running node
```

The Coq model is a separate formal object:

```
RFC / mathematical specification
        ↓
Coq model
        ↓
machine-checked properties
```

The two paths can be compared, tested, and audited.

But comparison is not automatically refinement.

RIGOR therefore does not claim:

> The C++ implementation has been formally proved equivalent to the Coq
> model.

unless and until such a refinement proof actually exists.

This is an important technical boundary.

### 5.12 Differential Verification and Golden Vectors

RIGOR can use finite test vectors to compare implementations.

For example:

```
(R, B, h)
        ↓
expected result
```

can be evaluated independently.

Likewise:

```
scriptPubKey
        ↓
expected classification
```

can be tested against multiple implementations.

Golden vectors are valuable because they detect:

- regressions
- serialization mistakes
- arithmetic mistakes
- classification divergence
- implementation drift

But:

> Testing is not proof.

A finite set of examples cannot establish universal equivalence between
two arbitrary programs.

The distinction remains explicit.

### 5.13 Cryptographic Primitives Remain Assumptions

Formal verification of the monetary predicate does not prove the
security of:

- SHA256
- SHA3
- Ed25519
- cryptographic hash collision resistance
- signature unforgeability

The model may represent cryptographic operations abstractly.

For example, an abstract function `H(x)` does not constitute a proof
of the real-world security of the hash function.

Likewise, `sig_ok(message, signature, key)` does not itself prove
Ed25519 unforgeability.

These are cryptographic assumptions at a different layer.

RIGOR keeps them outside the claims of its consensus model.

### 5.14 Formal Verification Is Not a Security Certificate

A formal result can establish a property under stated assumptions.

It does not establish:

- absence of all implementation bugs
- compiler correctness
- network security
- honest-majority assumptions
- hardware reliability
- wallet security
- economic viability

Therefore the appropriate statement is precise:

> Inside the stated Coq consensus kernel model, selected critical
> invariants have been machine-checked, with explicit extraction and
> runtime trust boundaries.

The qualification is part of the result.

It is not an afterthought.

### 5.15 The Verification Boundary

The architecture can therefore be represented as:

```
                    ┌───────────────────────┐
                    │   Protocol Rules      │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │   Formal Coq Model    │
                    └───────────┬───────────┘
                                │
                     machine-checked
                        properties
                                │
                ┌───────────────┴───────────────┐
                │                               │
       implementation tests             differential tests
                │                               │
                └───────────────┬───────────────┘
                                │
                    ┌───────────▼───────────┐
                    │   C++ Consensus Node  │
                    └───────────┬───────────┘
                                │
                    actual network validity
```

No arrow in this diagram should be interpreted more strongly than it has
been demonstrated to be.

In particular:

```
Coq theorem  ≠  C++ theorem
test suite   ≠  formal equivalence
```

### 5.16 What RIGOR Intentionally Does Not Verify

The formal boundary excludes several domains.

These include:

- network-level behavior
- honest-majority assumptions
- economic incentives
- market demand
- exchange rates
- wallet key custody
- hardware reliability
- operating-system behavior
- compiler correctness
- cryptographic primitive security
- universal C++/Coq refinement

These exclusions are not hidden limitations.

They are explicitly declared boundaries.

A protocol becomes easier to understand when its claims are no larger
than its evidence.

### 5.17 Minimal Consensus as an Engineering Discipline

The result of these choices is a particular engineering philosophy.

RIGOR attempts to keep the consensus machine composed of explicit
mechanisms:

```
UTXO state
    +
closed script classification
    +
asset conservation
    +
signature authorization
    +
Convergence arithmetic
    +
PoW validation
```

Everything else is pushed outward whenever possible.

This creates a hierarchy:

```
Consensus
        ↓
must agree

Formal model
        ↓
must precisely describe selected properties

Tests
        ↓
must detect implementation divergence

Wallet / Explorer / Pool
        ↓
may interpret and display

Website
        ↓
may explain
```

The hierarchy matters because each layer has a different authority.

### 5.18 The Architectural Principle

RIGOR's approach can be summarized without claiming that minimalism is
universally superior:

> The consensus layer should contain only the rules necessary to
> establish and validate the monetary state.

That principle leads naturally to:

- no general VM
- no asset registry
- no external oracle
- no miner-issued RES
- no probabilistic RES allocation
- no hidden issuance counter

and instead:

```
closed templates
    +
local UTXO validation
    +
deterministic Convergence
    +
PoW ordering
    +
explicit formal boundaries
```

The protocol does not attempt to eliminate complexity from the entire
system.

It attempts to prevent unnecessary complexity from entering the
consensus boundary.

### 5.19 The Boundary Is Part of the Design

A protocol specification is not made stronger by claiming more than it
proves.

RIGOR therefore treats the following distinction as architectural:

```
What the protocol specifies
        ≠
what the model proves
        ≠
what the implementation executes
        ≠
what the product displays
```

Each layer has a defined responsibility.

- The protocol defines rules.
- The formal model proves properties of the model.
- The implementation executes the rules.
- Testing compares finite behaviors.
- Products display public information.

The user remains able to verify the underlying data independently.

### 5.20 Closing Principle

The purpose of minimal consensus is not minimal ambition.

It is the opposite.

A protocol can only be inspected as deeply as its boundaries permit.

RIGOR therefore chooses to make its monetary machine deliberately
narrow:

```
small state vocabulary
    +
closed asset classification
    +
explicit conservation
    +
one-way monetary transformation
    +
deterministic arithmetic
    +
proof-of-work ordering
    +
declared verification boundaries
```

The resulting system does not claim to prove everything.

It attempts to make the things it does claim as explicit, reproducible,
and checkable as possible.

Consensus should not understand everything.

It should understand exactly what the ledger requires.

And a formal proof should say exactly what it proves.

---

## Chapter 6 — Proof-of-Work: Shared Ordering and the ORU Security Budget

### 6.1 Introduction — Why RIGOR Keeps Proof-of-Work at the Center

RIGOR inherits the core insight of Bitcoin:

> A permissionless monetary system requires a publicly verifiable
> ordering mechanism.

Transactions do not become valid because a participant declares
ownership, because an issuer approves them, or because an external
service records them. They become valid because every node independently
verifies the same history.

Proof-of-Work provides that shared history.

RIGOR does not replace Proof-of-Work with a different trust assumption.
It does not introduce staking, validator committees, delegated security,
or external settlement layers. The chain order remains established
through computational work.

The purpose of Proof-of-Work in RIGOR is deliberately narrow:

- establish canonical block ordering
- provide Sybil resistance
- secure the ORU monetary system
- provide the shared height required by the Convergence issuance rule

Mining does not create RES.

This separation is intentional.

```
Proof-of-Work
        ↓
shared ordering + ORU security budget

Convergence
        ↓
conditional RES creation
```

The two mechanisms interact through block height, but they serve
different monetary functions.

### 6.2 Proof-of-Work as a Time-Ordering Mechanism

Bitcoin's fundamental innovation was not merely finding hashes. It was
using computational cost to create an objective ordering of events.

RIGOR preserves this principle.

A block does not prove that a miner is trusted.

It proves that a miner has expended measurable external resources to
extend the chain.

The network accepts the chain with the greatest accumulated
proof-of-work because rewriting history requires reproducing the
corresponding computational work.

The security model is therefore:

```
external energy expenditure
            ↓
computational proof
            ↓
shared historical ordering
```

This ordering is required because RIGOR's monetary transformation
depends on height:

```
R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

The variable h is not a local clock.

It is not a wallet estimate.

It is not an oracle value.

It is the height produced by the Proof-of-Work chain.

Without a shared ordering mechanism, the same Convergence transaction
could be evaluated differently by different nodes.

Proof-of-Work provides the common reference.

### 6.3 RIGOR Proof-of-Work Design

RIGOR uses a dedicated Proof-of-Work validity hash:

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
```

A block satisfies Proof-of-Work when:

```
OpuHash(header) ≤ target
```

The design deliberately separates two concepts:

```
Block identity
    SHA256d(header_80)

Proof-of-Work validity
    SHA3-256("RGRPOW01" || header_80)
```

This distinction preserves the Bitcoin block-header model while allowing
RIGOR to define its own mining function.

The 80-byte header remains the historical identity boundary.

The Proof-of-Work algorithm operates on that header rather than
modifying the structure of the block identity.

This separation follows a Bitcoin-like design principle:

> Consensus innovations should avoid unnecessary changes to the
> fundamental object that nodes already identify and relay.

The block header remains a compact commitment to:

- previous block
- transaction commitment
- timestamp
- difficulty target
- nonce space

The mining function changes.

The object being mined does not.

### 6.4 Why RIGOR Does Not Use Mining to Issue RES

Many monetary systems combine:

```
security issuance
    +
monetary issuance
```

into one mechanism.

Bitcoin historically combines miner compensation and monetary issuance
through block subsidy.

RIGOR separates these functions.

Mining creates ORU.

Convergence creates RES.

The distinction is consensus-level:

```
Coinbase transaction
        creates
        ORU

Body transaction
        may create
        RES
```

A miner cannot create RES simply by finding a block.

A pool cannot allocate RES through payout policy.

A modified coinbase cannot bypass Convergence.

The reason is monetary separation.

RES exists as a consequence of a voluntary destruction process:

```
ORU removed from circulation
             ↓
deterministic conversion rule
             ↓
new RES UTXO
```

If RES were miner-issued, the store-of-value function would become
directly tied to mining power.

That would combine two different economic roles:

- paying for network security
- creating scarce reserve assets

RIGOR intentionally separates them.

### 6.5 Mining Rewards and the ORU Security Budget

RIGOR's production coinbase creates **128 ORU per block** after genesis.

The subsidy does not halve.

This is an explicit design choice.

The purpose is not to create an unlimited inflationary asset.

The purpose is to maintain a predictable security budget denominated in
the medium-of-exchange asset.

The protocol separates:

```
ORU
    medium of exchange
    miner compensation
    transaction fees

RES
    reserve-oriented asset
    created through destruction
```

The security budget is therefore paid in the asset designed for
circulation.

This follows a simple monetary separation:

- the asset used for everyday economic activity funds security
- the asset intended for reserve scarcity is not consumed as miner
  compensation

RIGOR does not claim that a fixed ORU subsidy guarantees external
economic security.

The protocol defines issuance.

The market determines value.

### 6.6 Difficulty Adjustment — Keeping Height Independent from Time

RIGOR uses ASERT difficulty adjustment:

| Parameter | Value |
|-----------|-------|
| target spacing | 600 seconds |
| half-life | 43200 seconds |
| adjustment clamp | ±12.5% |

The purpose of difficulty adjustment is maintaining approximately stable
block production.

However, difficulty adjustment does not define monetary issuance.

It does not enter the Convergence equation.

It does not determine RES supply.

It does not convert block time into monetary time.

The protocol intentionally separates difficulty from issuance cost.

Height is the consensus index.

Difficulty only maintains the process that generates that index.

This distinction prevents a common design mistake: using external time
assumptions inside monetary rules.

RIGOR uses:

```
Proof-of-Work ordering
        ↓
block height
        ↓
deterministic monetary evaluation
```

not:

```
wall clock
        ↓
monetary decision
```

### 6.7 Mining Does Not Expand the Consensus Surface

A key design objective of RIGOR is limiting the number of consensus
decisions.

Mining validity requires only:

```
header bytes
    +
PoW calculation
    +
target comparison
```

The miner does not participate in asset classification.

The miner does not decide RES issuance.

The miner does not approve Convergence.

The miner only proposes a block.

Full nodes independently verify:

- block validity
- transaction validity
- asset classification
- Convergence predicate
- PoW validity

The miner provides ordering.

The network provides verification.

### 6.8 Comparison with Bitcoin Design Principles

RIGOR follows several Bitcoin design principles:

**1. No trusted issuer**

RES is not allocated by a foundation, company, or miner.

Creation is determined by public rules.

**2. No external accounting layer**

The ledger does not rely on:

- asset registries
- centralized databases
- explorer interpretation

The node verifies the same bytes.

**3. No privileged block producer**

Finding a block does not grant the ability to create arbitrary monetary
objects.

Mining power creates ordering rights, not monetary authority.

**4. Monetary rules are explicit**

The protocol defines:

- ORU issuance
- RES creation conditions
- asset separation
- conversion restrictions

No hidden supply mechanism exists outside consensus.

### 6.9 The Design Boundary

RIGOR's Proof-of-Work design makes a narrow claim:

> Proof-of-Work provides the shared ordering required for a
> deterministic UTXO monetary system.

It does not claim:

- that mining is economically guaranteed forever
- that ORU has a predetermined market value
- that external security conditions cannot change
- that Proof-of-Work eliminates all operational risks

The protocol defines the mechanism.

The surrounding economy determines the outcome.

### 6.10 Summary

RIGOR retains Proof-of-Work because the problem it solves remains
fundamental:

> How can independent participants agree on monetary history without a
> trusted coordinator?

The answer remains:

```
Proof-of-Work
        ↓
shared ordering

UTXO validation
        ↓
shared state transition rules

Convergence
        ↓
deterministic reserve creation
```

Mining secures the ledger.

Convergence transforms monetary roles.

Neither replaces the other.

---

## Chapter 7 — Local Verification: Independent Validation of Public State

### 7.1 Introduction — Verification Is the Source of Truth

Bitcoin introduced a fundamental principle:

> A monetary system should not require participants to trust a display
> of the ledger. They should be able to independently verify the ledger.

RIGOR follows the same principle.

A wallet interface, explorer, mining dashboard, or website may present
information. However, presentation is not consensus.

Consensus exists only at the full-node validation layer.

The protocol boundary is:

```
Public bytes
      ↓
Independent calculation
      ↓
Consensus decision
```

Not:

```
Website display
      ↓
Trust decision
```

RIGOR does not create a privileged information source.

Every node receives the same block data and independently evaluates the
same rules.

### 7.2 The Node, Not the Interface, Defines Reality

A blockchain system naturally produces many views:

- wallets display balances
- explorers display transactions
- mining pools display shares
- websites display statistics
- documentation explains mechanisms

These are useful surfaces.

They are not authorities.

The consensus authority remains:

```
rigord
     ↓
ConnectBlock
     ↓
transaction validation
     ↓
UTXO state transition
```

A displayed RES balance is not the source of truth.

The UTXO state is.

A displayed Convergence result is not the source of truth.

The transaction bytes and consensus predicate are.

A displayed block confirmation is not the source of truth.

The node's validation result is.

This follows the original Bitcoin design philosophy:

> Verify, don't trust.

### 7.3 Public Data Must Be Sufficient for Verification

A deterministic monetary protocol requires that all information needed
for validation exists in the public ledger.

RIGOR therefore avoids hidden state.

The Convergence decision depends only on:

```
R
        ↓
requested RES amount

B
        ↓
destroyed ORU amount

h
        ↓
confirming block height
```

The consensus function is:

```
R · (2²⁵ + h)² ≤ B << 40
```

There is no:

- private issuance table
- administrator-controlled quota
- hidden exchange rate
- external oracle
- miner allocation rule

The same inputs produce the same result.

```
same (R, B, h)
        ↓
same accept / reject decision
```

### 7.4 Script Classification Is Locally Reproducible

RIGOR's asset model depends on one principle:

> Asset identity is derived from bytes.

The node does not ask:

> Does this explorer label this output as RES?

It asks:

> What does the `scriptPubKey` classify this output as?

The classification path is:

```
scriptPubKey
      ↓
ClassifyRigorTxout
      ↓
AssetTypeFromScriptPubKey
      ↓
ORU / RES / NONE / INVALID
```

Every node performs the same classification.

There is no separate asset database.

There is no external asset registry.

There is no wallet-defined asset interpretation.

The same output bytes must produce the same asset identity on every
full node.

### 7.5 Verification of Convergence

A Convergence transaction is accepted only if the complete chain of
conditions is satisfied.

The node verifies:

#### Step 1 — Destroy exists

The transaction contains:

```
OP_CONVERGE
        ↓
ORU destruction amount B
```

The destroyed output is visible in the transaction.

It is not inferred.

It is not calculated from a balance difference.

#### Step 2 — RES creation is visible

The same transaction contains:

```
Witness v1 RES outputs
        ↓
sum = R
```

The RES output is not created elsewhere.

It cannot appear in coinbase.

It cannot appear through miner policy.

#### Step 3 — Arithmetic predicate passes

The node evaluates:

```
R > 0 → R · (2²⁵ + h)² ≤ B << 40
```

If the condition fails, the transaction is rejected.

No explorer, pool, or wallet can override this result.

### 7.6 Independent Verification of Mining Data

Mining information is another example where display and consensus must
remain separate.

A pool may display:

- worker name
- submitted shares
- estimated reward
- current difficulty
- found blocks

These are operational statistics.

They are not consensus facts.

Consensus verifies:

```
header_80
        ↓
OpuHash
        ↓
target comparison
```

A block is valid because every node can reproduce the Proof-of-Work
calculation.

Not because a mining pool reports success.

### 7.7 The Commitment Chain

RIGOR's public verification path can be represented as:

```
Transaction body
        ↓
OP_CONVERGE destroy B
        ↓
Witness v1 RES outputs R
        ↓
RES predicate
        ↓
block inclusion
        ↓
Merkle commitment
        ↓
header
        ↓
SHA256d block identity
        ↓
OpuHash Proof-of-Work
```

Every step is publicly observable.

Every step is independently recomputable.

A failure at any point invalidates the state transition.

### 7.8 Why RIGOR Avoids Consensus by Interpretation

Many blockchain systems require additional interpretation layers:

- indexers
- token registries
- contract states
- application databases

Those systems may be useful.

However, they introduce another question:

> Which interpretation is the authoritative one?

RIGOR deliberately reduces this dependency.

The protocol attempts to keep monetary identity close to the base
ledger:

```
UTXO
    +
scriptPubKey
    +
consensus rules
```

The fewer external interpretations required, the smaller the gap
between:

```
what the ledger contains
        and
what participants believe it contains
```

This is a design choice, not a claim that every system should follow
the same path.

### 7.9 Zero-Trust Verification Surfaces

RIGOR allows additional verification tools without making them
consensus authorities.

#### Explorer

May show:

- transactions
- balances
- asset classification
- verification results

Cannot:

- approve invalid blocks
- create assets
- override node decisions

#### Browser Verification Tools

May independently recompute selected formulas:

```
(R, B, h)
        ↓
mint predicate
```

Useful for transparency.

Not a replacement for a full node.

#### Documentation

May explain:

- protocol rules
- mathematical derivations
- implementation boundaries

It cannot redefine consensus.

### 7.10 The Relationship Between Mathematics and Software

A critical principle of RIGOR is:

```
Mathematical model
        ≠
Running implementation
```

The protocol specification defines intended rules.

The C++ implementation realizes those rules.

Verification must therefore respect boundaries.

A mathematical proof may establish properties of a model.

It does not automatically prove:

- compiler correctness
- production binary correctness
- network behavior
- hardware reliability

This distinction is not a weakness.

It is a necessary engineering boundary.

Strong systems make their assumptions visible.

### 7.11 Verification as a Design Constraint

RIGOR treats verification not as an additional feature, but as a design
constraint.

Each major mechanism is evaluated through a simple question:

> Can an independent participant reproduce the decision from public
> information?

| Mechanism | Independent verification |
|-----------|--------------------------|
| ORU transfer | UTXO input/output conservation |
| RES transfer | RES conservation + ORU fee rules |
| RES creation | (R, B, h) predicate |
| Asset identity | script classification |
| Mining | PoW hash comparison |
| Block validity | full-node validation |

The goal is not maximum functionality.

The goal is maximum reproducibility within a deliberately limited
scope.

### 7.12 Design Boundary

RIGOR's verification model makes a limited architectural claim:

> Consensus should be determined by reproducible computation over
> public data, not by trusted interpretation.

It does not claim:

- that users never need software
- that wallets cannot contain bugs
- that explorers cannot display incorrect information
- that formal models automatically prove production code
- that every application should avoid higher-level systems

The protocol defines the verification path.

Software implements interfaces around that path.

### 7.13 Summary

RIGOR extends the Bitcoin verification philosophy into a
dual-native-asset architecture:

```
Public bytes
        ↓
Deterministic rules
        ↓
Independent verification
        ↓
Consensus decision
```

The protocol does not ask participants to trust:

- an issuer
- a website
- a pool
- an indexer
- a displayed balance

It asks participants to verify.

The design principle is simple:

> Information may be displayed.
> Validity must be independently established.

---

## Chapter 8 — Formal Verification Boundary: Model, Implementation, and Proof Scope

### 8.1 Introduction — Verification Requires Clear Boundaries

Formal verification is often misunderstood.

A mathematical proof can establish that a defined model satisfies
certain properties.

It cannot automatically prove that:

- every line of production software is correct
- every compiler transformation preserves behavior
- every hardware environment behaves correctly
- every network participant follows the protocol

A rigorous system must therefore define not only what is proven, but
also what is outside the proof boundary.

RIGOR adopts this principle:

> A proof is meaningful only when its scope is explicitly stated.

The formal boundary is:

```
Formal consensus model
        ≠
Production C++ implementation
        ≠
Running network behavior
```

The purpose of formal verification is not to create a marketing
statement.

Its purpose is to reduce ambiguity about the properties that have been
mathematically specified.

### 8.2 The Three-Layer Verification Model

RIGOR separates three different objects.

#### Layer 1 — Protocol Specification

The specification defines:

- monetary rules
- asset classification
- Convergence predicate
- conservation rules
- invalid states

This layer answers:

> What should the protocol mean?

Example:

```
R · (2²⁵ + h)² ≤ B << 40
```

defines the mathematical condition under which RES creation is
permitted.

#### Layer 2 — Formal Consensus Model

The formal model represents selected consensus properties in a
machine-checkable environment.

This layer answers:

> Does the mathematical model preserve the stated properties?

Examples:

- ORU conservation
- RES ceiling
- script classification partition
- valid state transitions
- chain induction properties

The model reasons about abstract objects.

It does not directly execute the production node.

#### Layer 3 — Production Implementation

The running node is the actual system participants use.

This layer contains:

- C++ consensus code
- transaction parsing
- UTXO database operations
- networking
- wallet interaction
- RPC interfaces

This layer answers:

> Does the software correctly realize the protocol?

The relationship is:

```
Specification
        ↓
Formal model
        ↓
Implementation
        ↓
Running network
```

Each transition introduces a separate engineering responsibility.

### 8.3 What Formal Verification Can Establish

Within the defined model, formal verification can establish properties
such as:

#### 1. Monetary Conservation

For valid state transitions:

```
ORU_in ≥ ORU_out
RES_in ≥ RES_out
```

except for the explicitly defined Convergence path.

The model can verify that arbitrary inflationary transitions are
impossible inside the modeled rules.

#### 2. RES Supply Ceiling

The model can verify the mathematical relationship:

```
S_RES ≤ 2²²
```

assuming the defined Convergence rules.

The ceiling is derived from the issuance curve, not stored as a mutable
counter.

#### 3. Asset Classification Separation

The model can verify that:

```
scriptPubKey
        ↓
asset classification
```

produces a deterministic partition:

- ORU
- RES
- NONE
- INVALID

This prevents ambiguous asset identity inside the model.

#### 4. Chain State Transition Rules

The model can verify that accepted transitions preserve required
invariants.

For example:

```
valid state
        +
valid transition
        ↓
valid state
```

### 8.4 What Formal Verification Does Not Establish

Formal verification does not automatically prove:

#### C++ Equivalence

A verified mathematical model does not automatically prove:

```
Coq model  =  core/src C++ code
```

A refinement proof or verified extraction process would be required.

#### Compiler Correctness

Even correct source code depends on:

- compiler behavior
- optimization
- build environment

#### Runtime Environment

Formal models do not prove:

- operating system correctness
- hardware reliability
- memory safety of all dependencies
- network conditions

#### Economic Outcomes

Formal verification cannot prove:

- market demand
- exchange value
- miner profitability
- user adoption

The protocol defines rules.

The economy determines outcomes.

### 8.5 Why the Distinction Matters

Many blockchain systems make broad claims around formal methods.

However, the useful question is narrower:

> Which exact property has been proven, under which exact assumptions?

RIGOR intentionally avoids expanding the claim beyond the model.

The correct statement is:

> Within the formal consensus model, selected invariants have been
> machine-checked under the stated assumptions.

The incorrect statement would be:

> The entire blockchain has been mathematically proven.

The difference is not rhetorical.

It is a difference in engineering precision.

### 8.6 Formal Verification and Minimal Consensus

Formal verification becomes more practical when the consensus surface
is intentionally limited.

A general execution environment may contain:

- arbitrary programs
- complex state interactions
- unpredictable execution paths

RIGOR intentionally avoids this direction.

The consensus model focuses on:

```
UTXO transitions
    +
closed script templates
    +
integer predicates
    +
asset conservation
```

This smaller surface allows clearer reasoning.

The design principle is:

> Reduce what must be proven by reducing what consensus is required to
> do.

This is not a statement that more expressive systems are invalid.

It is a design choice for a monetary protocol focused on verifiability.

### 8.7 Model Versus Implementation: An Engineering Discipline

The separation between model and implementation is not a weakness.

It is a normal engineering boundary.

A mathematical specification provides:

- precision
- invariants
- expected behavior

Implementation provides:

- performance
- compatibility
- operational functionality

The engineering task is maintaining correspondence between them.

RIGOR treats this correspondence as an ongoing responsibility.

Useful methods include:

- consensus test vectors
- differential testing
- regression tests
- independent verification tools
- code review

Formal verification is one layer of assurance.

It is not the only layer.

### 8.8 The Role of Golden Vectors

Because the formal model and implementation are separate objects,
deterministic test vectors become important.

A golden vector records:

```
public input
        ↓
expected result
```

Examples:

- valid Convergence transaction
- invalid underpaid mint
- forbidden `RES → ORU` conversion
- invalid script classification
- boundary arithmetic cases

Golden vectors provide a practical bridge:

```
mathematical expectation
        ↓
implementation behavior
```

They do not replace formal verification.

They complement it.

### 8.9 Security Honesty as a Design Principle

A protocol gains credibility not by claiming unlimited certainty.

It gains credibility by clearly stating boundaries.

RIGOR therefore distinguishes:

| Claim | Status |
|-------|--------|
| Mathematical properties of the defined model | Within formal scope |
| Correctness of production C++ | Engineering responsibility |
| Compiler correctness | Outside protocol proof |
| Network security | External assumption |
| Market success | External outcome |

A smaller, accurate claim is stronger than a larger, unprovable claim.

### 8.10 The Formal Verification Philosophy

RIGOR follows a simple principle:

> Do not prove more than the model contains.
>
> Do not claim more than the proof establishes.

The purpose of formal verification is not to replace engineering.

It is to make engineering assumptions visible.

### 8.11 Design Boundary

RIGOR's formal verification approach makes a limited architectural
claim:

> A minimal consensus system allows more precise mathematical reasoning
> about its defined rules.

It does not claim:

- that formal methods eliminate all bugs
- that the running network is mathematically perfect
- that the model covers every operational component
- that formal verification guarantees adoption or value

The proof boundary is part of the protocol design.

Knowing what is proven is as important as knowing what is not.

### 8.12 Summary

RIGOR separates three concepts:

```
Protocol rules
        ↓
Formal model
        ↓
Implementation
```

The formal model provides mathematical confidence within its scope.

The implementation provides a usable network.

The network operates under real-world conditions.

The strength of formal verification is not that it removes uncertainty.

Its strength is that it identifies exactly where uncertainty begins.

Clear rules.

Clear proofs.

Clear boundaries.

That is the foundation for a verifiable monetary protocol.

---

## Chapter 9 — Product Surfaces: Wallets, Explorers, Mining Tools, and Human Interfaces

### 9.1 Introduction — Interfaces Must Remain Interfaces

A decentralized monetary system requires human interfaces.

Users need:

- wallets to create and manage transactions
- explorers to inspect history
- mining tools to participate in Proof-of-Work
- documentation to understand the protocol

However, interfaces introduce a fundamental risk:

> A representation of the ledger can be mistaken for the ledger itself.

RIGOR therefore maintains a strict boundary:

```
Consensus rules
        ↓
Node validation
        ↓
Public interfaces
```

Not:

```
Interface display
        ↓
Consensus authority
```

A wallet may display a balance.

An explorer may display an asset type.

A website may explain a rule.

None of them define validity.

The node does.

### 9.2 The Bitcoin Principle: Verify, Do Not Trust

Bitcoin's original design introduced a powerful separation.

The user does not need to trust:

- a bank
- an issuer
- a payment processor
- a ledger operator

Instead, the user can verify the chain independently.

RIGOR extends this principle to its dual-native-asset model.

The user should not need to trust:

- an explorer database
- an asset registry
- a website calculation
- a mining pool interpretation

The source of truth remains:

```
Raw blockchain data
        +
Consensus verification
```

Interfaces improve accessibility.

They do not replace verification.

### 9.3 Wallets — Transaction Construction, Not Consensus Authority

A RIGOR wallet has a practical role:

- manage keys
- select UTXOs
- construct transactions
- display balances
- communicate with nodes

The wallet does not decide:

- whether RES creation is valid
- whether a script represents ORU or RES
- whether a transaction is accepted

Those decisions belong to consensus.

The correct relationship is:

```
Wallet constructs
        ↓
Transaction
        ↓
Node verifies
        ↓
Consensus decision
```

The wallet is a user tool.

The node is the verification authority.

### 9.4 Wallet Asset Display and Script-as-Asset

Because RIGOR uses Script-as-Asset architecture, wallets must follow
the same classification logic as consensus.

The wallet should not determine: "This address looks like RES."

Instead:

```
scriptPubKey
        ↓
ClassifyRigorTxout
        ↓
Asset identity
```

This avoids a common failure mode: a user interface creating an
interpretation that differs from consensus.

The principle is:

> Wallets may display asset identity. They do not define asset identity.

### 9.5 Explorers — Observation, Not Authority

Blockchain explorers are important public tools.

They help users inspect:

- blocks
- transactions
- UTXOs
- asset balances
- Proof-of-Work information

However, an explorer is an observer.

It is not a consensus participant.

An explorer may say:

> According to the data I indexed, this transaction appears valid.

The node determines:

> This transaction satisfies consensus rules.

The distinction matters.

An explorer database can contain:

- indexing errors
- synchronization issues
- software bugs
- incorrect interpretations

A full node independently validates.

### 9.6 The Explorer Problem in Multi-Asset Systems

Multi-asset systems often introduce additional interpretation layers.

For example:

- token databases
- contract indexing
- external asset metadata

RIGOR avoids making these layers consensus-critical.

Asset identity exists directly in `scriptPubKey` bytes, not in:

- explorer tags
- external registries
- centralized metadata

The explorer can read the same classification function:

```
scriptPubKey
        ↓
AssetTypeFromScriptPubKey
```

But the explorer is still only displaying the result.

### 9.7 Mining Pools — Coordination Without Monetary Authority

Mining pools provide useful operational services:

- worker coordination
- share accounting
- payout distribution
- mining statistics

However, a pool does not become a monetary issuer.

In RIGOR:

```
Miner finds block
        ↓
Receives ORU reward
```

Mining cannot:

- create RES
- select RES recipients
- bypass Convergence rules
- approve invalid transactions

A pool may display:

> A miner submitted a possible Convergence transaction.

The node decides:

> Does this transaction satisfy consensus?

This preserves the separation:

```
Mining power  ≠  Monetary authority
```

### 9.8 Zero-Trust Verification Surfaces

RIGOR introduces a useful category of product surface: verification
tools.

These tools do not replace nodes.

They help users independently reproduce selected calculations.

#### Convergence verification

Given R, B, and h, a user can verify:

```
R · (2²⁵ + h)² ≤ B << 40
```

#### Script classification verification

Given `scriptPubKey`, a user can verify:

- ORU
- RES
- NONE
- INVALID

#### Proof-of-Work verification

Given `header_80` and `target`, a user can independently calculate:

```
OpuHash(header) ≤ target
```

These tools increase transparency.

They do not become consensus authorities.

### 9.9 Documentation as a Verification Surface

Documentation has a unique responsibility.

Technical documentation can explain:

- design philosophy
- mathematical derivation
- implementation boundaries
- verification methods

However, documentation cannot change consensus.

A document saying "This transaction is valid" does not make it valid.

The correct statement is:

> According to the published rules, independent verification produces
> this result.

The difference preserves the Bitcoin principle:

> Rules are authoritative.
>
> Descriptions explain rules.

### 9.10 Product Design Principle: Reduce Trust, Not Add Trust

Many products attempt to simplify blockchain interaction by adding
convenience layers.

Convenience is valuable.

However, every additional interpretation layer creates a question:

> Who must be trusted?

RIGOR follows a different approach: reduce unnecessary trust
assumptions.

Instead of "Explorer says this is RES":

```
Explorer shows:
scriptPubKey
        ↓
consensus classification
```

Instead of "Website calculates supply":

```
Public data
        ↓
reproducible calculation
```

The interface becomes a window.

Not a gatekeeper.

### 9.11 Product Completeness and Consensus Completeness

A mature protocol requires both.

**Product completeness** includes:

- usable wallets
- explorers
- mining tools
- documentation
- educational resources

**Consensus completeness** includes:

- deterministic validation
- complete rules
- correct implementation
- independent verification

They are related but different.

A beautiful explorer cannot compensate for unclear consensus.

A perfect consensus design still requires usable products.

RIGOR treats both as important, while maintaining their boundaries.

### 9.12 Design Boundary

RIGOR's product philosophy makes a limited claim:

> Interfaces should expose public information without becoming sources
> of trust.

It does not claim:

- that users never need interfaces
- that interfaces cannot contain bugs
- that every user will operate a full node
- that presentation layers are unnecessary

The goal is not removing interfaces.

The goal is preventing interfaces from becoming hidden authorities.

### 9.13 Summary

RIGOR applies the Bitcoin verification philosophy beyond consensus code
and into product design:

```
Consensus
        defines reality

Interfaces
        explain reality

Users
        verify reality
```

The protocol does not ask users to trust:

- a website
- an explorer
- a pool
- a wallet
- a document

It provides public rules that can be independently checked.

The design principle is:

> A product should make verification easier, not make trust easier.

---

## Chapter 10 — Security Model and Threat Boundaries

Chapters 1–9 stated the dual-asset rules, local verification, the
formal boundary, and the product surfaces. This chapter states which
failures the protocol is designed to detect, and which risks remain
outside consensus.

The claim is architectural.

The protocol defines **detect-and-reject conditions**.

It does not claim that attacks cannot occur, that hashpower cannot
rewrite recent history, or that market participation will fund
security.

```
full-node validation
        ↓
admit a legal state transition
        or
reject an illegal one
        ≠
the attack surface is empty
        ≠
external systems are in scope
```

Live reject codes and parameter tables remain in RFC-020, RFC-018, and
the protocol whitepaper. This chapter does not add a rule.

### 10.1 Consensus Attack Boundaries

Full-node validation is designed to reject invalid state transitions.

The following cases are already named by consensus classification and
the live reject path. This section restates the architectural
response, not a new list of codes.

| Case | Protocol response |
|------|-------------------|
| Invalid RES mint | Predicate `R > 0 → R · (2²⁵ + h)² ≤ B << 40` fails; the transaction is invalid |
| Missing `OP_CONVERGE` | A RES output without a visible destroy is invalid |
| Underpaid RES mint | Destroyed ORU does not cover requested R |
| Illegal `RES → ORU` conversion | Reverse path, or spending a Convergence destroy output, is invalid |
| Invalid asset classification | Closed templates: unlisted script shapes are not monetary outputs |
| Chain reorganisation | Most-work order; mint and coinbase maturity 100 as reorg protection |

A reorganisation can still occur.

The protocol does not claim that history cannot be rewritten under
sufficient hashpower.

Maturity 100 is a confirmation-depth convention already used for
coinbase. It is not a proof that a reorg of that depth is impossible.

An underpaid mint that a pool or explorer already displays as credited
remains invalid.

Display does not admit a state transition. Chapter 7.

### 10.2 Economic Attack Boundaries

The protocol specifies a conversion mechanism.

It does not guarantee market participation.

| Pressure | What the protocol does | What it does not do |
|----------|------------------------|---------------------|
| ORU inflation pressure | Coinbase continues to pay ORU; hardness rises only if destroy occurs | Guarantee a terminal ORU cap, or that destroy will be frequent |
| Insufficient RES demand | Minting simply does not occur; realised supply stays below the derived ceiling | Create demand for RES |
| Low destroy participation | The ceiling `S_RES ≤ 2²²` remains an upper bound | Fill the ceiling |
| Weak fee market | Selected ORU fee flows may be destroyed (RFC-018); subsidy remains 128 ORU | Guarantee a fee market that replaces issuance |
| Miner incentive pressure | Coinbase pays 128 ORU per block after genesis | Guarantee the external purchasing power of that 128 ORU |

The security budget in protocol units has a floor.

The security budget in an external unit does not.

Whether miners find the ORU payment sufficient is a market outcome.

### 10.3 External Dependency Boundaries

The following are outside consensus, as already implied by Chapters 8
and 9:

- wallet software correctness and UX
- user key custody
- network connectivity and P2P honesty
- hardware reliability and side channels
- third-party services (pools, explorers, websites, indexers)

A full node may reject an illegal block.

It cannot make a stolen key unspendable, a disconnected peer honest,
or an explorer's display authoritative.

Formal verification of the Coq model does not cover these surfaces.

It does not prove the running C++ binary.

### 10.4 Assumptions and Limits

This is not a list of defects.

It is the explicit model boundary.

The protocol does not prove:

- honest majority
- network behavior
- hardware availability
- market adoption
- exchange rate
- wallet safety
- compiler correctness

Those remain assumptions or external facts.

Detect-and-reject conditions in §10.1 still apply inside the model.

They do not convert the items above into theorems.

### 10.5 What This Chapter Does Not Claim

- It does not claim that listed attacks cannot occur.
- It does not claim a security comparison with Bitcoin.
- It does not claim that 128 ORU is a sufficient real-world security
  budget.
- It does not claim that C++ is equivalent to the kernel model.
- It does not add a reject code, a maturity rule, or a hashpower
  assumption beyond what earlier chapters and the live RFCs already
  stated.

The protocol defines detection and rejection.

It does not define market demand.

Chapter 11 states what this architecture intentionally does not
provide.

---

## Chapter 11 — Design Trade-offs

Chapter 10 bounded consensus, economic, and external risks.

This chapter states what RIGOR intentionally does not provide, and
what those omissions are intended to buy.

### 11.1 Every Architecture Chooses a Boundary

A blockchain architecture is not defined only by the features it
provides.

It is equally defined by the complexity it refuses to introduce.

RIGOR is built around a specific design question:

> How much monetary functionality can be expressed while keeping
> validation deterministic, transparent, and independently
> reproducible?

The answer is not maximum capability.

The answer is a narrower monetary protocol with explicit boundaries.

RIGOR intentionally chooses:

```
closed monetary primitives
        +
UTXO ownership model
        +
deterministic asset identity
        +
one-way monetary transformation
```

This choice creates advantages and limitations.

Both are part of the architecture.

### 11.2 The Trade-off Between Programmability and Verifiability

General-purpose blockchain systems often maximize programmability.

This allows:

- arbitrary applications
- user-defined logic
- complex financial instruments
- programmable assets

RIGOR takes a different approach.

The protocol does not attempt to become a general computation platform.

It does not include:

- a smart contract VM
- arbitrary execution environments
- dynamic asset logic
- contract-defined monetary rules

The reason is not that programmable systems are invalid.

The reason is that every additional execution capability introduces
additional consensus complexity.

A general execution environment requires nodes to agree on:

- execution order
- state transitions
- resource limits
- contract behavior
- failure conditions
- upgrade paths

RIGOR narrows the problem.

Instead of asking "Can the ledger execute arbitrary logic?", RIGOR
asks:

> Can the ledger verify a small number of monetary rules with maximum
> clarity?

The trade-off is intentional:

```
less programmability
        ↓
smaller consensus surface
        ↓
simpler independent verification
```

### 11.3 The Trade-off of Script-as-Asset

RIGOR uses a script-as-asset model.

Asset identity is derived from `scriptPubKey`, not from:

- a separate asset database
- an external registry
- wallet metadata
- explorer interpretation

This creates a direct relationship between ownership and asset
identity.

The advantage:

```
UTXO
    +
locking condition
    =
native asset identity
```

The limitation is equally clear.

Adding a new asset is not an application-level action.

It requires consensus changes.

A new monetary identity requires:

- a new classification rule
- new conservation rules
- new validation logic

This is deliberately different from systems where anyone can deploy new
assets through application layers.

RIGOR accepts reduced flexibility in exchange for stronger native
verification boundaries.

### 11.4 The Trade-off of Dual Native Assets

RIGOR separates two monetary functions:

```
ORU
        ↓
medium of exchange
security payment

RES
        ↓
reserve-oriented asset
scarcity mechanism
```

This separation introduces additional complexity.

A single-asset system has simpler accounting.

With two assets, nodes must verify:

- asset classification
- separate conservation
- conversion restrictions
- independent spending rules

The benefit is monetary role separation.

The protocol does not force one asset to perform conflicting functions.

Instead:

```
transaction flow
        ≠
reserve preservation
        ≠
security payment
```

The cost is additional consensus logic.

RIGOR considers this complexity acceptable because the separation
happens at the protocol layer, not through external accounting
systems.

### 11.5 The Trade-off of One-Way Monetary Transformation

Convergence introduces a one-way relationship:

```
ORU
        |
        | destroy
        ↓
RES
```

The reverse path does not exist.

This creates a stronger monetary boundary.

There is no `RES → ORU` conversion mechanism.

The advantage is avoiding a hidden exchange-rate protocol inside
consensus.

If both directions existed, the ledger would need additional rules for:

- conversion pricing
- liquidity assumptions
- timing
- arbitrage behavior
- monetary feedback loops

RIGOR avoids embedding these questions into consensus.

The protocol defines transformation.

The market determines valuation.

### 11.6 The Trade-off of Deterministic Issuance

RES issuance is not:

- miner-selected
- randomly allocated
- governance-adjusted
- controlled by a central issuer

It follows a public mathematical predicate.

The advantage: every node can independently verify the same result.

The limitation: the protocol does not optimize for every possible
economic scenario.

For example:

- demand may be lower than expected
- destroy activity may be limited
- the derived ceiling may never be reached

RIGOR intentionally separates:

```
protocol rule
        ≠
economic outcome
```

The protocol defines the mechanism.

Participants determine whether the mechanism is used.

### 11.7 The Trade-off of Minimal Consensus

A larger protocol can support more functionality.

A smaller protocol can reduce ambiguity.

RIGOR chooses a minimal consensus philosophy.

Consensus should answer:

- Is this output a valid asset?
- Is this transaction valid?
- Is this monetary transition permitted?
- Is this block correctly ordered?

Consensus should not answer:

- What application should exist?
- What contract should run?
- What business logic should users adopt?

This boundary reduces the number of situations where nodes must
interpret complex external behavior.

The result is a narrower but more clearly defined machine.

### 11.8 The Trade-off of Formal Verification Scope

Formal verification is powerful because it forces precision.

However, precision requires boundaries.

RIGOR explicitly separates:

```
formal model
        ↓
implementation
        ↓
deployment
```

The advantage is intellectual honesty.

The limitation is that formal verification does not automatically
prove:

- deployed software correctness
- network behavior
- economic security
- user safety

A formal model is valuable because it verifies what it actually
represents.

Not because it creates a broader claim.

### 11.9 The Trade-off of Bitcoin Compatibility and Divergence

RIGOR inherits several Bitcoin principles:

- proof-of-work ordering
- UTXO ownership
- block validation by independent nodes
- deterministic consensus
- no trusted issuer

However, RIGOR does not attempt to be a Bitcoin clone.

It changes specific monetary assumptions:

```
Bitcoin:
one native asset
        ↓
multiple economic functions

RIGOR:
two native monetary identities
        ↓
separated protocol roles
```

The goal is not to replace Bitcoin's design.

The goal is to explore another point in the design space while
preserving Bitcoin's core verification philosophy.

### 11.10 The Trade-off of Saying No

Many blockchain designs compete by adding capabilities.

RIGOR competes through reduction.

It intentionally says no to:

- arbitrary execution
- unlimited issuance
- flexible asset creation
- consensus-level application logic

Those omissions are not missing features.

They are architectural decisions.

A smaller system is easier to explain.

A smaller system is easier to verify.

A smaller system has fewer places where hidden assumptions can enter.

### 11.11 What RIGOR Optimizes For

RIGOR does not optimize for:

- maximum developer flexibility
- maximum application diversity
- maximum transaction complexity

It optimizes for:

```
clear monetary objects
    +
simple validation rules
    +
independent verification
    +
long-term protocol readability
```

The design objective is not to create the most powerful blockchain.

It is to create a monetary protocol where the important rules remain
visible.

### 11.12 Final Design Principle

RIGOR's design trade-offs can be summarized as:

> A monetary protocol gains strength not only from what it can do, but
> from what it refuses to make ambiguous.

The architecture deliberately accepts constraints:

```
less execution
        for
less interpretation

less flexibility
        for
more deterministic rules

less surface area
        for
more independent verification
```

This is not a universal blockchain design philosophy.

It is a specific architectural choice.

RIGOR chooses a smaller monetary machine because the protocol's
primary objective is not unlimited capability.

It is verifiable monetary behavior.

---

## Chapter 12 — Deployment and Evolution Policy

### 12.1 Protocol Evolution Begins With Restraint

A consensus protocol is not only defined by its initial design.

It is also defined by how it changes.

Many blockchain systems face a fundamental tension:

```
innovation
      vs.
stability
```

New capabilities can expand usefulness.

Frequent changes can weaken predictability.

RIGOR approaches this question from a conservative monetary
perspective:

> Consensus rules should change only when the benefit is greater than
> the loss of established verification assumptions.

The purpose of deployment policy is therefore not to prevent all
evolution.

It is to preserve the property that makes the protocol valuable:

> The rules defining monetary validity remain clear, public, and
> independently reproducible.

### 12.2 The Consensus Boundary

RIGOR separates three categories of change.

#### Consensus changes

These affect whether nodes accept or reject blocks and transactions.

Examples:

- asset classification rules
- transaction validity rules
- Convergence conditions
- ORU and RES conservation rules
- issuance rules
- script templates
- signature verification domains

These changes modify the monetary machine itself.

They require:

- explicit specification
- public review
- implementation
- testing
- activation process

#### Product changes

These improve interaction without changing consensus.

Examples:

- wallet interfaces
- explorer presentation
- documentation
- visualization tools
- mining interfaces
- user experience improvements

These changes do not modify what the network accepts.

A wallet can become better.

An explorer can become clearer.

The consensus rules remain unchanged.

#### Verification tooling

These help users independently inspect public data.

Examples:

- verification utilities
- audit tools
- reproducible calculation tools
- formal model documentation

These tools improve transparency.

They do not become consensus authorities.

### 12.3 Frozen Monetary Principles

RIGOR's core monetary principles are designed to remain stable.

The architecture is built around several fixed ideas:

#### One UTXO state space

The ledger remains:

```
coins database
      +
script-defined identity
```

There is no separate asset registry.

#### Script-as-Asset

Asset identity remains determined by `scriptPubKey`, not by:

- external metadata
- explorer interpretation
- wallet assumptions

#### Domain Separation

ORU and RES remain separate monetary domains.

The protocol maintains:

```
ORU
        ↓
security payment
medium of exchange

RES
        ↓
reserve-oriented asset
```

The separation exists at consensus level.

#### One-Way Convergence

The monetary bridge remains:

```
ORU destruction
        ↓
RES creation
```

The protocol does not introduce a reverse conversion path.

### 12.4 Why Minimalism Requires Stability

A minimal protocol gains value from being understandable.

If a protocol frequently changes fundamental rules, the benefit of
simplicity decreases.

Each consensus modification introduces:

- new implementation paths
- new validation cases
- new testing requirements
- new verification obligations

Therefore, RIGOR treats simplicity as something that must be
preserved.

Not because change is impossible.

Because unnecessary change increases the cost of trust.

### 12.5 RFC-Based Evolution

Future consensus changes should follow a public RFC process.

A consensus RFC should contain:

**Problem definition**

What limitation or requirement motivates the change?

**Precise specification**

The new rule must be expressed without ambiguity.

**Security analysis**

The change must describe:

- new state transitions
- new validation paths
- new attack considerations

**Implementation reference**

The implementation must correspond clearly to the specification.

**Verification strategy**

The change should identify:

- test cases
- invariants
- compatibility considerations

An RFC is not automatically accepted because it exists.

A specification is the beginning of evaluation.

### 12.6 The Implementation Boundary

RIGOR maintains an important distinction:

```
Specification
        ≠
Implementation
        ≠
Deployment
```

The specification describes intended consensus behavior.

The implementation realizes that behavior.

The deployed network operates that implementation in real
environments.

Each layer requires independent attention.

A precise specification does not remove the need for engineering.

A tested implementation does not remove the need for clear
specifications.

A running network does not remove the need for ongoing review.

### 12.7 Preserving the Verification Model

RIGOR's design objective is that independent participants can
reproduce consensus decisions.

Future evolution should preserve:

```
public input
      ↓
deterministic computation
      ↓
same result
```

A future change should be evaluated not only by "Can this feature be
implemented?", but also:

> Can this rule remain independently verified?

This question is central to the architecture.

### 12.8 No Governance Layer Inside Consensus

RIGOR does not introduce an internal governance mechanism that can
dynamically modify monetary rules.

The protocol does not rely on:

- privileged issuers
- administrator keys
- automatic monetary parameter adjustment
- hidden policy decisions

Consensus changes are software and protocol changes.

They require explicit publication and adoption.

The purpose is to keep monetary rules separate from operational
control.

### 12.9 The Role of Documentation

Documentation is part of transparency.

It explains:

- why the rules exist
- what the protocol guarantees
- what the protocol does not guarantee

However: documentation is not consensus.

A paper cannot create a valid transaction.

A website cannot authorize issuance.

A dashboard cannot override a node.

The final authority remains independent node validation.

### 12.10 Long-Term Compatibility Philosophy

RIGOR does not define success as the number of future features added.

It defines success as preserving the clarity of the original design.

A mature monetary protocol should allow future participants to
understand:

- what the assets are
- how ownership changes
- how issuance occurs
- how validation works
- what assumptions exist

Long-term reliability requires continuity of understanding.

### 12.11 What RIGOR Does Not Promise

RIGOR does not promise:

- no future protocol changes
- no software defects
- guaranteed adoption
- guaranteed market value
- guaranteed economic outcomes

These are outside the protocol boundary.

RIGOR provides rules.

Participants determine whether those rules become useful.

### 12.12 Final Deployment Principle

RIGOR's deployment philosophy can be summarized as:

> A protocol should evolve carefully enough that future participants
> can still understand why the original rules existed.

The architecture prefers:

```
measured evolution
        over
continuous modification

explicit rules
        over
implicit policy

verification
        over
assumption
```

The goal is not to freeze software forever.

The goal is to preserve the reason the software exists:

> A small, transparent, independently verifiable monetary protocol.

---

## Closing Statement — Architecture Before Expansion

RIGOR begins from a simple principle:

> The strongest monetary rules are the ones that remain visible.

The protocol deliberately narrows its scope:

- fewer execution paths
- fewer hidden states
- fewer interpretation layers
- fewer trusted assumptions

This does not make RIGOR universal.

It makes its purpose clear.

RIGOR is an exploration of a different monetary architecture:

> A Bitcoin-inspired proof-of-work system where monetary roles are
> separated at the consensus layer, asset identity is native to the
> UTXO model, and validity is determined by rules that every
> participant can independently verify.

The architecture does not ask participants to trust the system.

It asks them to inspect it.
