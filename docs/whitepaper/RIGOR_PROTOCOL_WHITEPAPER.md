# RIGOR Protocol Whitepaper

**A Dual-Asset Proof-of-Work Protocol with Deterministic Convergence and Zero-Trust Verification**

| Field | Value |
|-------|-------|
| Document | Official protocol whitepaper (architecture restructure) |
| Task | `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-WHITEPAPER-001` (chapter insert; prior consistency audit retained) |
| Product | RIGOR / AlphaPlus |
| Date | 2026-08-18 |
| Consensus impact | **NONE** |
| `equivalence_claimed` | **false** |

This document is the professional protocol whitepaper. It reorganizes existing
technical material. It does not change consensus, parameters, or RFCs.

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
    4 Core Monetary Script Families
  +
    UTXO Local Validation
  +
    Ed25519 Authentication
  +
    OpuHash + ASERT PoW
  +
    Deterministic Reserve Issuance
  +
    Deflationary Fee Burn
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
- OpuHash adaptive Proof-of-Work
- UTXO-local validation

without:

- token registry
- account ledger
- contract storage
- global asset state

The formal system verifies protocol models and finite vectors.
The production C++ implementation is not claimed to be formally equivalent.

```
equivalence_claimed = false
```

---

## Abstract

Public proof-of-work chains usually ask **one** native asset to do three
jobs at once: circulate as a medium of exchange, remain scarce as a store
of value, and pay miners for security. Those jobs conflict. Cheap
transactions want circulation. Scarcity wants withdrawal from circulation.
Security wants a sustainable miner budget. A single ticker cannot optimize
all three without a structural tension — the **security-budget problem**.

**RIGOR** separates the jobs at consensus.

- **ORU** is the utility asset: fees, miner subsidy, and Convergence entry
  weight. It has a predictable subsidy and a permanent tail of **125 ORU**
  per block so miner payment does not depend on inflating the reserve.
- **RES** is the scarce asset: maximum supply **21,000,000**. It is not
  created by mining subsidy, not by governance, and not by a 1:1 swap.
  New RES appears only after **ORU Convergence** and a deterministic draw.
  RES is **deterministically issued** after protocol-defined conditions.
  It is not mined.

**Convergence** is the core mechanism. Eligible economic actions (ORU
committed into an unspendable Convergence burn) form a public candidate set. Two roots bind that set:
**Convergence Root** and **Draw Candidate Root**. Every honest node
computes the same winner from the same public bytes. Fairness is a hash
inequality over a `2^256` space. **Deterministic Reserve Issuance** uses
shipping numbers **`E = 246`**, mint cost **`C = 1024` ORU per RES mint**,
expected wins per committed ORU ≈ `n / 1024`. Historical RFC-014 / retired
design: `2^240 / 2^256 = 1/65536` (`C = 65536` ORU). **Not an active
consensus parameter.** That identity is **not** the shipping target. Selection is
not a committee decision.

**Zero Trust** means a stranger does not accept a website or explorer
verdict. Public data is recomputed locally. Equal bytes are accepted.
Unequal bytes are rejected.

This paper explains *why* the split exists, *how* Convergence works, how
miners participate (including the **Windows** path), and how verification
is layered. It does not claim protocol-to-binary equivalence or mainnet
completion.

```
equivalence_claimed = false
```

---

## Contents

1. The Security Budget Problem of Single-Asset Blockchains
2. Dual-Asset Economic Architecture
3. Minimal Script Architecture
4. Convergence Consensus Architecture
5. Mathematical Fairness
6. Mining Architecture
7. Self-Proving Blockchain
8. Formal Verification Architecture
9. Ecosystem and Roadmap
10. Appendix

---

# Chapter 1 — The Security Budget Problem of Single-Asset Blockchains

This chapter states **why RIGOR exists** before later chapters state how
it works.

A typical public chain forces **one** asset to perform three roles:

```
Medium of Exchange
        +
Store of Value
        +
Security Incentive
```

Those roles are not the same job.

## 1.1 Medium of Exchange

A medium of exchange is the unit people use to move value.

Users in this role require:

- low transaction cost
- usability
- liquidity

They treat the network as a transfer system. Exposure is often momentary.
Many acquire the unit just in time to pay a fee or a transfer and do not
hold it as a long-term reserve. For them, the asset must **circulate**.

## 1.2 Store of Value

A store of value is the unit people hold because it is scarce and the
rules are stable.

Users in this role require:

- scarcity
- long-term security
- predictable monetary policy

Transaction costs can be amortized over long holding periods. Circulation
is secondary. The asset must **remain scarce**.

## 1.3 Security incentive

Proof-of-work security is paid in the native asset. Miners require
**sustainable rewards**. Early security is usually paid by a block
subsidy. If that subsidy declines toward zero, security is expected to be
paid by fees alone.

## 1.4 Why one asset creates tension

| Role | What they want | What they contribute to security |
|------|----------------|----------------------------------|
| Medium-of-exchange users | Cheap, frequent transfers | Fees — but they resist high fees |
| Store-of-value users | Scarcity and permanent protection | Few transactions, so few fees |
| Miners | Predictable payment for hashpower | Security of the chain |

A store-of-value culture **discourages** frequent spending. A
medium-of-exchange culture **needs** spending. Miner security needs a
budget that does not disappear when holders stop transacting.

A single asset is asked to be, at once:

1. the thing people spend
2. the thing people hold
3. the thing that pays for hashpower

If issuance continues forever, the reserve story weakens.  
If issuance ends, the miner budget must be carried by fees that
store-of-value holders have little reason to generate.  
If the same unit is both fee-fuel and digital reserve, holders are
punished for using the network.

Relying on perpetual price appreciation to enlarge a shrinking subsidy is
not a protocol rule. It is a hope. That is the security-budget problem of
single-asset proof-of-work systems.

**RIGOR’s answer is not a market promise. It is an asset split at
consensus.** ORU takes utility and miner payment, including a permanent
tail. RES takes scarcity, and is created only through Convergence.

---

# Chapter 2 — Dual-Asset Economic Architecture

```
PoW  →  ORU  →  Convergence  →  RES
```

Proof-of-work discovers blocks and orders history. Proof-of-work **does
not** create RES.

| Role | Asset | Design target |
|------|-------|----------------|
| Medium of exchange | **ORU** | Usage, fees, miner budget |
| Store of value | **RES** | Scarcity; designed to provide long-term monetary separation between transactional liquidity and a capped reserve |

## 2.1 ORU — Medium of Exchange Asset

**ORU** is optimized for **usage**.

Role:

- transaction medium
- fee payment
- miner security budget
- Convergence entry weight

| Property | Frozen rule |
|----------|-------------|
| Script path | Witness v0 |
| Unit | `1 ORU = 100,000,000` base units |
| Initial subsidy | **1000 ORU** per block |
| Halving interval | **210,000** blocks |
| Sequence | **1000 → 500 → 250 → 125** |
| Tail emission | **125 ORU** per block forever |
| Supply law | `S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)` |
| Minimum Convergence | **1 ORU** |

Issuance is **predictable**: a height-indexed function, integer-only, no
discretionary last reward. After three halvings the subsidy **locks**.
That permanent tail is the protocol’s answer to miner sustainability:
security is paid in the **utility** asset, not by inflating RES.

ORU is meant to move. Committed ORU leaves circulating supply (`C(h)`)
and becomes draw weight. It does not convert 1:1 into RES.

## 2.2 RES — Store of Value Asset

**RES** is optimized for **scarcity**.

Role:

- scarce value storage
- designed to provide long-term monetary separation between transactional
  liquidity (ORU) and a capped reserve (RES)

RES is **not** created by:

- mining subsidy
- governance decision
- arbitrary issuance
- a 1:1 ORU swap
- coinbase

RES creation requires economic cost:

```
ORU Convergence
        |
        |
Economic cost (ORU leaves supply)
        |
        |
deterministic draw
        |
        |
RES creation (body mint, if a winner)
```

Public relation (not a 1:1 mint function):

```
New RES  =  f(ORU Converged)
```

`f` is the frozen draw: committed ORU is weight `f_i`; a successful
selection mints **exactly `W(h)` RES** (`GetResMintReward`); otherwise
the block mints **0**. `W(0) = 40.000000 RES` and decays on the
Deterministic Reserve Issuance height schedule; issuance of a non-zero
`W` stops after epoch 250.
Whether a given block mints is **not** calendar-guaranteed — that still
depends on Convergence. The amount paid **if** a mint occurs is a
height-indexed protocol function.

| Property | Frozen rule |
|----------|-------------|
| Script path | Witness v1 (not CVG2) |
| Unit | `1 RES = 1,000,000` atoms (six decimal places) |
| Maximum supply | **21,000,000 RES** |
| Per successful mint | **`W(h)`** (40.000000 RES at genesis) |
| Per block | `M(h) ∈ {0,1}` |
| Ledger origin | Non-coinbase mint in the **block body** |
| Receiver | Winner’s declared RES program — **not** the miner |

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + GetResMintReward(h+1) · M(h+1)
S_RES(h) ≤ 21,000,000
```

There is no premine and no administrative mint.

### Where each asset is born

| | **ORU** | **RES** |
|--|---------|---------|
| Role | PoW security budget + fees + Convergence weight | Independent scarce asset |
| Classification | Witness v0 | Witness v1 (not CVG2) |
| Ledger origin | Coinbase subsidy `R(h)` | One non-coinbase body mint |
| Per block | 1000 → 500 → 250 → 125 forever | `W(h)` RES or 0 |
| Who receives it | Miner (coinbase script) | Draw winner — not the miner |
| Cap | No hard cap (tail inflation) | 21,000,000 RES (schedule cannot exhaust the cap even if every block mints) |
| Calendar | Height-indexed subsidy | Amount `W(h)` is height-indexed; occurrence of a mint is not |

**ORU is created in coinbase. RES is created in the block body after
Convergence.**

Protocol-enforced: `S_RES ≤ 21,000,000`, and a winner **must** be paid
`W(h)` RES or the block is invalid.  
Not guaranteed: a mint in every block, or that 21 million fills on a date.

**RES has a maximum supply of 21,000,000. Actual circulating supply depends on
Convergence participation.** Do not read the cap as a calendar promise that
21 million will necessarily be reached.

### Deterministic Reserve Issuance — formal summary

RES is **not** created by Proof-of-Work. RES is deterministically issued
after protocol-defined conditions:

- fixed issuance schedule
- bounded maximum supply
- Convergence-triggered authorization
- deterministic mint amount calculation

RES issuance is:

- **demand-driven in occurrence** — a mint happens only if Convergence produces a winner;
- **deterministic in amount** — if a mint occurs, the amount is exactly `W(h) = GetResMintReward(h)`;
- **cost-escalating** — with constant draw target `T`, the protocol cost floor `C / W(k)` weakly rises as `W(k)` decays;
- **mathematically bounded** — even if every block from genesis through epoch 250 mints, issuance stays strictly below 21M (`210000 × 99450437` atoms).

```
k = height / 210000
W(k) = floor(40·RES_COIN · (k+1)^(-3/2))   for 0 ≤ k ≤ 250
W(k) = 0                                   for k > 250
```

Formally verified **monetary invariants** live in `ResSchedule.v` (cap finite sum,
termination, reward / cost-floor monotonicity). This is **not** a claim of
C++↔Coq refinement (`equivalence_claimed = false`).

**No Convergence → No RES.**

The following is **economic analysis**, not a consensus guarantee:
more Convergence demand increases the number of public candidates in `F`,
which increases competition for the single winner slot. Individual
eligibility still depends only on the published hash-space inequality
and weight `f_i`.

## 2.3 RIGOR Monetary Architecture

RIGOR separates monetary functions. The two native assets do different
jobs, and four ORU quantities must not be collapsed into one “supply”
number.

### ORU

ORU is the PoW security asset and the transaction-fee asset. It is:

- issued by height-indexed coinbase subsidy (`1000 → 500 → 250 → 125` forever)
- destroyed through Convergence (unspendable burn / draw participation)
- affected by **Deflationary Fee Burn**

ORU has no absolute cap. The 125 ORU tail is permanent miner payment in
the utility asset. That is not an “unlimited reserve.” It is a locked
subsidy floor.

### RES

RES is the reserve asset. It has:

- deterministic issuance (not mining)
- capped supply (**21,000,000 RES**)
- creation only through protocol-defined mint authorization

Script classification alone does not create RES. A mint requires
block-level authorization, the Deterministic Reserve Issuance schedule,
and Convergence conditions.

### Deflationary Fee Burn

**Deflationary Fee Burn** is a protocol-level ORU destruction mechanism.
Eligible transaction-fee components are permanently removed from
circulation. It is:

- not stored as global consensus supply state
- not a wallet balance
- not a UTXO
- not a miner-controlled parameter
- independently observable through explorer/indexer statistics

Eligible share: `floor(fee / 2)` of **RES-transfer** ORU fees is not
claimable in coinbase. That share never becomes a UTXO. Convergence
destruction and fee-burn destruction are **two distinct sinks**.

### Four quantities that must stay separate

| Quantity | What it is | What it is not |
|----------|------------|----------------|
| ORU supply ledger | RFC-002 path: issued subsidy minus Convergence `C(h)` | Circulating UTXO, fee burn, or a wallet balance |
| Circulating UTXO supply | Spendable Witness v0 ORU still in the UTXO set | The ledger path, Convergence burned, or fee burned |
| Convergence destruction | ORU committed to an unspendable Convergence burn | Deflationary Fee Burn |
| Deflationary Fee Burn | Unclaimed RES-transfer ORU fee share | Convergence `C(h)` or a burn UTXO |

Do not mix these four. Explorer statistics may display each separately.
None of them is a consensus global asset-state table.

## 2.4 Why two assets are rare

Most public chains launched as one ticker. That design is easier:

- smaller wallet surface
- simpler listings
- simpler consensus accounting
- shorter user vocabulary

A dual-asset design adds:

- engineering complexity (two script paths, two supply laws)
- harder economic balancing (tail inflation on ORU vs hard cap on RES)
- additional consensus requirements (Convergence set, roots, body mint,
  fail-closed winner payment)

RIGOR **intentionally accepts** that complexity so medium of exchange and
store of value are not the same consensus object.

1. ORU can keep a tail subsidy without claiming that the reserve inflates.
2. RES can keep a 21 million cap without starving miner payment.
3. RES entry is costly and public — ORU Convergence, not an admin mint.

This is an architectural choice. It is not a price forecast.

How those two identities exist on the ledger — as native UTXO scripts,
not as tokens or overlays — is Chapter 3.

---

# Chapter 3 — Minimal Script Architecture

Standalone chapter file:
`docs/whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`.

RIGOR inherits the battle-tested Bitcoin UTXO architecture while reducing
execution complexity, limiting script capability, enforcing strict asset
isolation, and minimizing consensus attack surface.

## 3.1 Why minimal script architecture

RIGOR does not attempt to compete with programmable blockchain platforms.

Instead, RIGOR focuses on:

- monetary integrity
- deterministic consensus
- verifiable asset ownership
- minimized attack surface

A blockchain securing monetary value does not necessarily require
unlimited programmability. More code paths create more possible
failures. A smaller execution surface creates stronger verification
capability.

The goal is not maximum programmability. The goal is maximum
verifiability.

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

RIGOR is a Bitcoin-derived UTXO architecture, a native dual-asset system,
a restricted script environment, and a deterministic validation system.
It is not a general-purpose programmable blockchain, a smart contract
platform, an EVM-compatible chain, a Taproot extension, a token layer,
a colored coin system, or an inscription/Rune system.

Official registry: `docs/security/RIGOR_SCRIPT_TYPE_REGISTRY.md`.

```
Less capability
       ↓
Less complexity
       ↓
Smaller attack surface
       ↓
Stronger mathematical verification
```

## 3.2 Bitcoin UTXO foundation

RIGOR is based on the Bitcoin UTXO model.

```
TxOut = (nValue, scriptPubKey)
```

**Explicit ownership.** Each asset exists as an unspent transaction
output.

```
Previous UTXO
      |
      |
Transaction Validation
      |
      |
New UTXO Owner
```

No hidden balances. No global mutable account state.

**Deterministic validation.** Every node independently verifies
transaction inputs, outputs, signatures, asset rules, and consensus
conditions. The same input must produce the same validation result.

**Proven security model.** Bitcoin’s UTXO architecture has operated at
global scale for many years. RIGOR preserves this proven foundation
while reducing unnecessary complexity.

## 3.3 Script defines asset identity

In RIGOR, asset identity is not stored as external metadata. The script
structure itself defines the asset domain.

```
Script Structure
        |
        |
Asset Classification
        |
        |
Validation Rules
```

This avoids hidden asset metadata, external indexing dependency, and
inconsistent interpretation. **Script is the compliance/classification
layer.** Asset class is inferred from `scriptPubKey`. Asset ownership
and transfer are enforced through UTXO consumption, script validation,
signature verification, and consensus predicates.

Script alone does not authorize asset creation. RES creation requires
block-level authorization, the Deterministic Reserve Issuance schedule,
and Convergence conditions.

## 3.4 Dual native asset architecture

ORU and RES are **not** colored coins, Bitcoin inscriptions, Ordinals,
Runes, smart contract tokens, wrapped assets, or external accounting
layers.

They are **native UTXO assets**. Both are first-class protocol-native
assets.

> ORU and RES are two native asset identities inside the RIGOR UTXO system. Neither asset is a secondary representation of another asset.

Both exist at the same protocol layer. There is no parent-child asset
relationship.

**ORU** — Medium of Exchange. Transactional asset, mining reward,
network utility, liquidity layer. Spendable path: Witness v0.
Convergence commitment: Witness v1 CVG2 (ORU-domain, unspendable).

**RES** — Store of Value. Scarcity asset, capped supply,
convergence-based creation, long-term holding asset. Witness v1
(not CVG2).

## 3.5 Script restrictions and attack surface reduction

RIGOR intentionally limits script functionality.

RIGOR does **not** provide a general-purpose VM, arbitrary smart
contracts, a programmable execution layer, Turing-complete execution,
or unrestricted programmable state.

RIGOR **does** provide constrained transaction validation,
asset-specific script rules, and deterministic verification logic.

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

Security benefits — reduced execution complexity, consensus bugs,
unexpected state transitions, and verification burden.

> Every additional execution capability creates additional consensus responsibility.

## 3.6 RIGOR script model

Scripts exist. A general-purpose VM does not. RIGOR minimizes script
complexity rather than eliminating scripts.

Actual execution surfaces: Witness v0 (ORU P2WPKH / native Ed25519),
**P2WSH** (largest inherited inner-script surface), Witness v1 RES
key-path, Ed25519 signature verification, and CVG2 structural
validation.

**Witness v0** is used for Bitcoin-compatible UTXO spending and the
compatibility layer (ORU: P2WPKH and P2WSH). P2WSH remains the largest
inherited execution surface; inner-script CHECKSIG is Ed25519 only.

**Witness v1 reserved usage:** Convergence outputs, native asset
programs, Ed25519 ownership model.

**Witness v1 is not Bitcoin Taproot.** RIGOR does not inherit BIP341
Taproot execution semantics. Control blocks, annex, and tapscript are
absent from the consensus path.

Allowed chain outputs: witness v0 (20- or 32-byte program), witness v1
with a 32-byte program, and zero-value `OP_RETURN`. New P2PKH / P2SH /
bare outputs are consensus-invalid. secp256k1 CHECKSIG hard-fails.

## 3.7 Convergence script architecture

Convergence is not a smart contract. It is a deterministic consensus
mechanism.

```
Eligible ORU Transactions


        |


Convergence Root


        |


Winner Selection


        |


RES Creation
```

Convergence outputs are protocol-controlled objects (CVG2, ORU-domain,
unspendable, not inserted into the UTXO set). They do not introduce
arbitrary execution.

## 3.8 Why RIGOR rejects complex script systems

Complex scripting creates a larger audit scope, more edge cases, and
more formal verification difficulty.

RIGOR approach:

```
Less capability


       ↓


Less complexity


       ↓


Smaller attack surface


       ↓


Stronger mathematical verification
```

## 3.9 Formal verification relationship

Formal verification becomes more realistic when state transitions are
limited, execution paths are restricted, and rules are deterministic.

RIGOR's minimal script design is not a limitation.

It is a prerequisite for stronger mathematical assurance.

```
equivalence_claimed = false
```

Website: `/documentation/security/minimal-script-architecture`.  
GitHub: `docs/security/minimal-script-architecture.md`.  
Audit: `docs/security/RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md`.  
Registry: `docs/security/RIGOR_SCRIPT_TYPE_REGISTRY.md`.

---

# Chapter 4 — Convergence Consensus Architecture

Convergence is the **core innovation**. Dual assets without a public,
deterministic bridge would only be two tickers. Convergence is that bridge.

## 4.1 What is Convergence?

**Convergence is not:**

- a lottery UI
- a random reward assigned by an operator
- a centralized selection
- mining
- a 1:1 exchange counter

**Convergence is:**

> A deterministic cryptographic selection process over eligible economic
> actions.

The economic action is: commit **≥ 1 ORU** into a **CVG2** output. That
output is unspendable. The ORU leaves circulating supply and becomes
weight `f_i` in candidate set **F**. If the commitment is eligible and
selected, the paying block must mint **`W(h)` RES** in its **body** to
the winner’s declared Witness v1 program.

```
ORU committed (CVG2, unspendable)
        ↓
weight f_i in candidate set F
        ↓
deterministic draw
        ↓
maybe a RES mint — never a 1:1 swap
```

## 4.2 Convergence Root

**Convergence Root** is the **deterministic commitment to eligible
convergence transactions.**

It is not a user-ordered list, not a miner-chosen winner, and not a
hidden random beacon.

```
eligible transactions
        ↓
sort
        ↓
concat
        ↓
Hash256
        ↓
Convergence Root
```

```
Convergence_Root =
Hash256(
    concat(
        sort(
            eligible txids
        )
    )
)
```

Canonical form (same rule):

```
C = Hash256( concat( sort_memcmp( txid_i ) ) )
```

| Rule | Meaning |
|------|---------|
| Input | Native 32-byte txids of eligible convergence transactions |
| Order | Unsigned byte-wise sort — not arrival order, not miner preference |
| Concatenation | Sorted bytes, no delimiters, no per-leaf Merkle tree |
| Hash | Hash256 = SHA-256(SHA-256(·)) |
| Boundary | If \|F\| < 2 → **C = 32 zero bytes** (no hashing). RFC-015 still allows a singleton `F` to compete for RES; the root short-circuit is unchanged. |

**Eligible** here means the consensus membership of `F`, not mempool
gossip order. Public predicates include: CVG2 commitment, `f_i ≥ 1 ORU`,
a declared RES owner program, valid inputs at block entry, and a
positive fee. Under `W = 32`, `F` is the sliding window of those
commitments (already-paid winners in the window are excluded). Users
and miners do not choose the sort key.

**Determinism.** Same eligible set ⇒ same root ⇒ same later σ inputs ⇒
same winner (given the same parent hash and height).

**Unpredictability.** Before the parent hash and the committed set exist,
no participant assigns `C` or the result. This is a hash-space property,
not hidden randomness.

**Anti-manipulation.** Changing the transaction set, the candidate data,
or (if someone tried) the order of hashing inputs changes `C`, because
the official sort is canonical. A quiet edit that favors one transaction
while leaving `C` intact is not available. Invalid modifications are
detectable by deterministic validation.

`C` is a function of the **set**, not of gossip order. Explorers may
display `convergence_root`. That display is not a verdict. A verifier
recomputes `C` from the public txid list.

## 4.3 Draw Candidate Root

**Draw Candidate Root** is the **deterministic commitment used for
candidate selection after convergence eligibility is established.**

**`draw_candidate_root`** is an independent object. It is not another
name for `C`. The two roots must not be merged.

After `F` and `C` are fixed, each candidate is tested against
`H_i < T · f_i`. Draw Candidate Root commits the **inequality-eligible
subset** — those that satisfied that filter — for the active height /
window (`W = 32`). Winner selection (argmin) then runs only over that
subset.

| Name | Field | Commits |
|------|-------|---------|
| Convergence Root | `convergence_root` / `C` | Eligible convergence transactions (`F`) used as the σ set commitment |
| Draw Candidate Root | `draw_candidate_root` | Inequality-eligible candidates after `F` is established |

```
draw_candidate_root =
Hash256(
    concat(
        sort(
            inequality-eligible txids
        )
    )
)
```

Unlike Convergence Root, this digest does **not** use the `|F| < 2 → 0`
short-circuit. An empty eligible subset hashes the empty concatenation.

Purpose:

- prevent silent candidate injection after the universe is bound
- prevent ordering manipulation of the draw set
- provide reproducible verification of *who passed the inequality*

`C` anchors the committed set used by σ.  
`draw_candidate_root` lets a reader check the post-eligibility draw
universe independently of UI labels.

RPC and explorers may show `draw_candidate_root_committed` (bound by the
block) and `draw_candidate_root_computed` (recomputed). If they differ,
the published identity does not match the bytes.

Neither root mints RES. They make the candidate set checkable. They are
not a Merkle tree.

## 4.4 Winner determinism

Every honest node independently calculates:

```
Candidate Data
       |
       |
Convergence Root
       |
       |
Draw Candidate Root
       |
       |
σ and inequality
       |
       |
Winner
       |
       |
Block Validation
```

Selection (Sigma-PoW-01):

```
σ_i  =  Hash256( txid_i  ||  C  ||  B_{h-1}  ||  LE32(h) )
f_i  =  converge_amount(tx_i)
```

- `B_{h-1}` is the parent block hash, taken verbatim.
- Height `h` pays the set committed by block `h-1`.
- The σ preimage is exactly **100 bytes**.
- `T` is frozen `RES_PROBABILITY_TARGET`. It does **not** follow mining
  difficulty.

Eligibility:

```
eligible  ⇔  |F| ≥ 1  ∧  f_i ≥ 1  ∧  H_i < T · f_i
```

Winner (integer cross-multiplication):

```
i ≺ j   iff   H_i · f_j  <  H_j · f_i
winner  =  minimal element under ≺
```

Ties break on txid, byte-wise. The teaching form is
`winner = argmin(H_i / f_i)`. There is no `amount^{7/5}` term.

`W = 32` exists so the block producer cannot monopolize the whole
candidate set for a period (SECURITY-001-A9-C).

The draw is not a random-number oracle. It is a frozen inequality
(RFC-000 / RFC-014) plus a deterministic total order.

If a miner changes the eligible set, `C` changes, and **every**
candidate’s σ changes.

### Closed economic loop

```
ORU (PoW, medium of exchange)
        ↓
Convergence (ORU committed)
        ↓
Selection (σ)
        ↓
RES (scarce, window W = 32)
        ↓
Market demand
        ↓
(back to ORU utility)
```

### Full RES path

```
User locks ≥ 1 ORU in CVG2
        ↓
Enters candidate set F (window W = 32; already-paid winners excluded)
        ↓
Parent block h−1 commits F (Convergence Root in coinbase)
        ↓
Child block h draws with σ
        ↓
Exactly one winner, or none
        ↓
If winner: block h body contains exactly one RES mint
        ↓
New W(h) RES Witness v1 UTXO → winner’s program
```

Miners and pools cannot issue RES. A payout-address change cannot mint
RES. A winner without a mint is invalid. A mint without a winner is
invalid. Assembling the mint only **settles** a user who already won.

Over-issuance is consensus-invalid. Ordinary RES transfers cannot create
RES (`res_in ≥ res_out`). Destroying RES (`res_in > res_out`) only
tightens supply.

---

# Chapter 5 — Mathematical Fairness

Fairness in RIGOR is a published integer rule over a hash space. This
chapter states the mathematics. It does not use personal anecdotes or
external personality association.

## 5.1 Hash probability and shipping `E = 246`

A cryptographic hash such as SHA-256 maps arbitrary input to a fixed
256-bit output. The output space has

```
2^256
```

possible values (about `1.16 × 10^77`). Absent a break of SHA-256, no
participant assigns the digest.

**Deterministic Reserve Issuance.** The unwindowed exponent is **`E = 246`**.
Mint cost is **`C = 2^(256-E) = 1024` ORU per RES mint**. With draw window
`W = 32`:

```
p(n ORU)         =  min(1, n / 1024)           (aggregate identity)
p_draw(n)        =  n / (1024 · 32)            (per draw)
expected wins    =  n / 1024
```

These identities are a **probability component derived from a hash-space
condition**, not a guaranteed winning probability, a fixed mining
probability, or a marketing round number. The window changes the per-draw
chance. It does **not** change the aggregate `n / 1024` identity.

Saturation at `n ≥ 1024` in the single-draw story means linear probability
reaches 1. That is a probability bound, not a financial promise.

**There is no unpublished rate.** The published aggregate identity is
`1/1024` per ORU of committed weight (`C = 1024` ORU per mint).

**Historical RFC-014 parameter.** An earlier draft used `E = 240`:

```
2^240 / 2^256  =  1 / 2^16  =  1 / 65536
```

That implied `C = 65536` ORU per mint. **65536 = 2^16** remains a valid
description of a 16-bit slice of a 256-bit hash space. **Not an active
consensus parameter.** It is **not** the
shipping cost or shipping probability. Do not use `1/65536` as the live
aggregate identity.

## 5.2 Why 125 appears in the subsidy sequence

**125 is not presented as the reason the protocol exists.** It is
**mathematical design symbolism** and a **convergence-sequence
interpretation**. **125 represents the terminal value of the defined
reduction sequence.** It is not a protocol security guarantee, not a
consensus fairness proof, and it does not “create fairness.”

```
1000  →  500  →  250  →  125
```

| Step | Subsidy | Meaning |
|------|---------|---------|
| Genesis era | 1000 ORU | Initial miner payment |
| First halving | 500 ORU | 1000 / 2 |
| Second halving | 250 ORU | 500 / 2 |
| Terminal floor | **125 ORU** | 250 / 2 — then **lock** |

Interval: 210,000 blocks. After the third halving, `R(h) = 125` forever.
RFC-001 forbids further halving. The sequence stops so the medium of
exchange that pays miners does not decay to zero (Chapter 1).

**125 = 5³** (`5 × 5 × 5`). It is a perfect cube — structural simplicity:
the cube root is the integer 5. Among the small cubes
`1, 8, 27, 64, 125`, it is the value this sequence actually reaches.

125 is also known as the positive integer whose proper divisors
(`1`, `5`, `25`) all appear as contiguous substrings of its decimal
digits (`1`, `5`, `25` inside `125`). That is a number-theoretic
curiosity, **not** a consensus justification.

Secondary identities (not consensus):

```
10² + 5²  =  125
11² + 2²  =  125
5^(1+2)   =  125
121 = 11² ,  125 = 5³ ,  128 = 2⁷
```

**Protocol meaning of 125:** arithmetic end of three halvings of 1000;
ORU subsidy floor after the sequence locks; a parameter that remains
whole after reduction. **1024** is the shipping mint-cost identity (`E = 246`).
**65536** is a Historical RFC-014 parameter (16-bit teaching slice), not the
live aggregate. **125** is the locked tail of the utility asset. Fairness is the
published integer draw rule, not the numeral 125.

---

# Chapter 6 — Mining Architecture

Mining discovers valid headers (OpuHash) and orders blocks (ASERT
difficulty). Mining **pays ORU**. Mining **does not mint RES**.

## 6.1 Miner participation

At the protocol / node layer, a miner:

```
Miner
 |
 |
Receive transactions
 |
 |
Validate candidates
 |
 |
Perform mining (OpuHash)
 |
 |
Produce block
 |
 |
Receive rewards (ORU coinbase)
```

- Transactions enter the mempool and, if they are valid convergence
  commitments, the candidate universe.
- The miner (or pool assembler) builds a template that binds
  Convergence Root / draw-candidate commitment in the coinbase payload.
- Proof-of-work finds a header that meets the current target.
- On acceptance, coinbase pays **ORU** to the miner’s script.
- If the deferred draw has a winner, the **block body** must include the
  RES mint to the **winner**, not to the miner.

Pool policy cannot allocate RES. Changing a miner payout address cannot
create RES.

## 6.2 Windows miner experience

The public ecosystem path is **visible Windows miner participation**,
not hidden background / SOAP / soak mining.

Primary user path:

```
Windows miner
      |
Wallet
      |
ORU convergence
      |
RES generation
      |
Pool participation
```

Expanded product flow:

```
Windows user
    ↓
Windows miner software
    ↓
Connect wallet
    ↓
Connect pool
    ↓
Mine ORU (visible shares, height, maturity)
    ↓
Converge ORU (wallet)
    ↓
Observe Convergence / RES
    ↓
Pool dashboard + Explorer + Zero Trust
```

| Step | What the user sees |
|------|--------------------|
| Download miner | Windows miner package; checksum when published |
| Wallet connection | Receive / payout address (`rg1q…`) |
| Pool | Stratum connection; workers and shares on a dashboard |
| Mine ORU | Immature coinbase → trusted ORU |
| ORU Convergence | Wallet convergence transaction (≥ 1 ORU) |
| RES generation | Waiting / selected; `W(h)` RES only if the draw wins |
| Verify | Block hash in Zero Trust — not “trust the pool UI” |

Mining should be understandable **visually**: lifecycle, shares,
maturity, convergence state, RES result. Operator logs are not the
primary UX.

Windows scenario (product, not SOAP):

1. Start Windows Wallet (`installer/windows/start-wallet.cmd`)
2. Start Windows Miner (`installer/windows/rigor-miner.cmd` or pool agent)
3. Connect the miner to the pool
4. Observe shares and height
5. Observe ORU maturity
6. Observe Convergence
7. Observe RES
8. Verify a block hash independently

---

# Chapter 7 — Self-Proving Blockchain

Traditional observer habit:

```
Trust software
        |
        |
Accept result
```

RIGOR Zero Trust:

```
Public Data
       |
       |
Independent Calculation
       |
       |
Verification
       |
       |
Accept / Reject
```

> You do not need to trust a node. You can independently verify the
> result yourself.

Zero Trust is public verification, deterministic validation, and
independent node (or browser) recomputation. There is no trust
assumption that a website, explorer, wallet, or pool is correct.
Invalid modifications are detectable by deterministic validation.
The protocol does not claim that cheating is “impossible”; it claims
that unequal bytes fail the published checks.

## 7.1 Self-Proving Block Commitment

```
Convergence tx
    → CVG2 commitment
    → Convergence Root / Draw Candidate Root
    → coinbase commitment
    → coinbase txid
    → merkle root
    → block header
    → OpuHash
```

If any step fails to match, the block is invalid. Validity is equality
of commitment and recomputation, not an explorer flag.

## 7.2 Consensus Fingerprint

The **Consensus Fingerprint** is a **verification identity layer**.
It is **not** a consensus-rule replacement.

The Consensus Fingerprint Library publishes a deterministic identity
over already-public fields. It is **observational**. It does not accept
or reject blocks. A wrong fingerprint cannot make an invalid block
valid, and a matching fingerprint is not a substitute for full-node
validation.

```
ConsensusFingerprint =
  Hash256(ConvergenceRoot || winner_txid || Sigma || height || target_prev)
```

An explorer may display a fingerprint. A verifier recomputes it.

## 7.3 Explorer, public vectors, and surfaces

| Surface | Role |
|---------|------|
| Full node (`rigord`) | Applies consensus rules |
| Website Zero Trust portal | Recomputes TX / block fields in the browser |
| Explorer | Viewer / indexer; outbound “Verify independently” |
| Wallet | Spending tool, not an oracle |
| Pool dashboard | Operator observer |

Public vectors for the fingerprint library live in
`test_vectors/fingerprint_vectors.json`. Golden-vector families for
runtime archive work are listed in the Appendix.

Live site (keep): `/zero-trust-verification`,
`/zero-trust-verification/what-is-zero-trust`.

```
equivalence_claimed = false
```

---

# Chapter 8 — Formal Verification Architecture

Formally verified protocol invariants with explicit verification boundaries.
Three layers. They must not be collapsed into one “fully verified”,
“formally proven blockchain”, “bug free”, or “zero risk” claim.

## 8.1 Why verification boundaries matter

A proof about a mathematical model is not automatically a proof about every
line of a shipping node, every network path, or every wallet. RIGOR therefore
publishes **what is inside the verified cone** and **what is outside it**.
Boundaries prevent silent over-claiming: green Coq builds and green
differential gates strengthen confidence in named invariants; they do not
license `equivalence_claimed = true`.

Public map for external reviewers: `verification/README.md`.
Reproduce: `docs/security/HOW_TO_VERIFY_RIGOR.md`.
Claim table: `docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md`.

## 8.2 What is formally verified

In the Coq *model* and/or the finite differential gate (living lock **422 /
215**):

1. **Deterministic Reserve Issuance schedule** — deterministic `W(k)`, termination after epoch
   250, nonincreasing pins, cost-floor behavior along the schedule.
2. **RES supply cap** — finite-sum max issuance within 21,000,000 RES;
   overshoot rejected.
3. **Convergence consensus model** — σ layout, order-independent Convergence
   root, selection determinism, A-9-C window exclusion, draw-candidate-root
   pipeline (eligible-list digest).
4. **UTXO asset / dual-pool invariants** — ordinary RES conservation; mint
   increases RES by the schedule term only (model).
5. **Script shape restrictions** — disallowed created templates, spend-CVG2,
   and PCORB field-size shape on accepted model transactions / blocks.

Honesty flags:

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

## 8.3 What remains outside the proof boundary

- Complete C++ ↔ Coq ↔ Python equivalence
- Cryptographic primitive security theorems (beyond an executable hash model
  and FIPS / tripartite vector checks)
- Hardware / side-channel security
- Network availability, P2P honesty, mempool resistance
- Mining templates, difficulty retarget / PoW hardness, wallet assumptions
- Owner / PCORB mint authorization as identical to abstract `cm_draw_won`
- Coinbase / Merkle / header bind of `draw_candidate_root`

## 8.4 How executable vectors connect specification and implementation

| Layer | Artifact | Role |
|-------|----------|------|
| Spec | RFCs + `formal_verification/spec/rigor_consensus_spec.py` | Normative byte / predicate SSOT |
| Model | `formal_verification/coq/` | Machine-checked properties of the model |
| Vectors | `phase3_golden_vectors.json` + `golden_lock.json` | Finite shared cases |
| Gate | `scripts/run_differential_gate.sh` | Python ↔ harness ↔ portable hash ↔ σ/root CLI |

Vectors do **not** prove universal equivalence. They fail the build or gate
when a locked case regresses. That is how specification and implementation
stay connected without claiming a refinement proof.

## Formal Verification Status (freeze 2026-08-18)

Authoritative correspondence page:
`docs/verification/FORMAL-VERIFICATION-STATUS.md`
(checklist: `docs/verification/FORMAL-VERIFICATION-FREEZE.md`).
Verdict (what the Coq *model* proves):
`docs/verification/FORMAL-VERIFICATION-VERDICT.md`.

RIGOR follows a three-layer verification approach:

1. **Protocol specification layer** — frozen RFC / SPEC.
2. **Scientific model layer** — strongly verified Coq model.
3. **Runtime verification layer** — differential / golden-vector verification
   (Python oracle, tripartite hash, production σ/root CLI). PROOF-034 is a
   separate empirical archive campaign on this layer.

**Current milestone (honest):** the Coq model is strongly verified; the
correspondence boundary is tighter than the original 57-vector bridge
(**422** shared golden vectors, **215** Coq-enforced, tripartite hash,
σ/root CLI in the gate; living lock in `formal_verification/golden_lock.json`).
Formally verified protocol invariants with explicit verification boundaries.
This **does not claim full refinement**.

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Public site page: `/documentation/formal-verification`.

## Layer 1 — Frozen Protocol Specification

Rules are deterministic, consensus-defined, and treated as immutable
under the freeze declaration (RFC-000 … RFC-015,
`MAINNET_PROTOCOL_FREEZE_DECLARATION.md`).

This layer **is** the protocol. A website, archive, or model does not
rewrite it.

## Layer 2 — Scientific / Mathematical Model

Mathematical modeling and state-machine representation (specifications,
Coq model, science / lab narrative). The model can prove properties of
**the model**. It does not automatically prove that a shipping binary is
identical to that model.

See `docs/verification/FORMAL-VERIFICATION-VERDICT.md`.

## Layer 3 — Runtime Verification

Empirical execution traces **and** differential correspondence checks.

**Differential gate (living lock).** Shared JSON golden vectors (422),
Coq-enforced harness subset (215), portable CSHA256 × hashlib × extracted
Coq hash, production σ/root CLI. Finite. Not a shared production hash
binary. Not a refinement proof. (Historical operator freeze recorded 324/117;
RC0 FV freeze 414/207.)

### PROOF-034

**PROOF-034 is a runtime archive — an empirical execution trace.**

It materializes a contiguous execution through catalogued golden-vector
heights, including:

| Height | Vector | Phenomenon |
|--------|--------|------------|
| 630,000 | GV-ORU-0003 | ORU halving / floor region |
| 2,100,000 | GV-RES-0003 | RES cap boundary |
| 2,100,001 | GV-DRAW-0003 | DRAW reject fixture |

**PROOF-034 is not:**

- a protocol equivalence proof
- a completion claim
- a mainnet readiness proof
- a license to set `equivalence_claimed = true`
- a substitute for RFC / SPEC

```
equivalence_claimed = false
```

| Grade | What it is | What it is not |
|-------|------------|----------------|
| RFC / SPEC | Frozen rules | A running chain |
| Formal model (Coq) | Properties of the model (strongly verified) | A C++ refinement proof |
| Differential gate | Golden vectors, hash CLI, σ/root CLI | Shared production hash; Coq = C++ |
| Runtime archive | What a specific execution produced | Equivalence or mainnet completion |
| Observer UI | Display | Authority |

---

# Chapter 9 — Ecosystem and Roadmap

RIGOR is a protocol plus a set of **observer and participation**
surfaces. None of the surfaces below is a consensus authority.

| Surface | Role today | Notes |
|---------|------------|-------|
| Website | `www.rigorchain.com` | Product portal; dark technical theme |
| Documentation | `/documentation` | Architecture, RFC, security, formal verification |
| Whitepaper | `/documentation/whitepaper` | Long-form bind of this document |
| Security Architecture | `/documentation/security` | Security hub; minimal script, Zero Trust, verification |
| Minimal Script Architecture | `/documentation/security/minimal-script-architecture` | UTXO / script-minimization chapter |
| Formal Verification | `/documentation/formal-verification` | Freeze status; `equivalence_claimed = false` |
| Wallet | `/wallet` | Qt / Windows packages; ORU, maturity, Convergence, RES |
| Mining | `/mining` | Windows miner + installer scripts; nested guides may be placeholders |
| Explorer | `/explorer` | Block / ORU / RES / fingerprint viewer; nested pages may be placeholders |
| Zero Trust | `/zero-trust-verification` | Browser verifier; TXID and block hash |
| Lab | `/protocol` `/science` `/lab` `/status` `/experiment` | PROOF-034 observer — keep |

**Roadmap (documentation and product surfaces only — not consensus
changes):**

1. Bind this whitepaper at `/documentation/whitepaper` (readable
   long-form; **live**; dark theme: black background, white text, gray cards).
   Keep Zero Trust, Explorer, Mining, Lab, and Experiment. Formal verification
   freeze: `/documentation/formal-verification`. Minimal script
   architecture: `/documentation/security/minimal-script-architecture`.
2. Complete Windows miner guide, checksummed downloads, pool dashboard
   link.
3. Keep Zero Trust, Explorer, Mining placeholders, Lab, and Experiment.
4. Publish chapter HTML from `docs/whitepaper/` without claiming
   equivalence or mainnet completion.

No investment yield is promised. No date is given for filling 21 million
RES.

---

# Appendix

Technical pointers. The chapters above are the public argument. This
appendix keeps the engineering index.

## A. RFC and specification index

| Document | Role |
|----------|------|
| RFC-000 | Consensus constitution |
| RFC-001 | Deterministic selection; ORU schedule; RES mint amount and cap |
| RFC-002 | Supply evolution |
| RFC-008 | Canonical mathematics (σ, integer inequality) |
| RFC-014 | Frozen / windowed `RES_PROBABILITY_TARGET`; `W = 32` |
| RFC-015 | Occam economics (\|F\| ≥ 1, 21M cap; mint amount superseded by Deterministic Reserve Issuance) |
| RFC-017 | Internal specification for **Deterministic Reserve Issuance** (`W(h)` under constant `T`) |
| RFC-018 | Internal specification for **Deflationary Fee Burn** (RES-transfer ORU fee share) |
| SPEC-002 | Authoritative ORU / RES recurrences |
| `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | Freeze authority |
| `docs/mining/CONVERGENCE_ROOT_CANONICAL_SPEC.md` | Convergence Root encoding |
| `docs/architecture/CONSENSUS_FINGERPRINT_LIBRARY.md` | Fingerprint library |

RFCs were **not** edited by this whitepaper task.

## B. Frozen parameters (public)

| Symbol | Value |
|--------|-------|
| ORU subsidy sequence | 1000 → 500 → 250 → 125 |
| `ORU_MIN_BLOCK_SUBSIDY` | 125 ORU |
| Halving interval | 210,000 blocks |
| `MIN_CONVERGENCE_AMOUNT` | 1 ORU |
| `RES_REWARD_GENESIS` / `W(h)` | 40.000000 RES at genesis; `GetResMintReward(h)` thereafter |
| `RES_REWARD_LAST_EPOCH` | 250 (`W(k) = 0` beyond) |
| `RES_MAX_SUPPLY` | 21,000,000 RES |
| `CONVERGENCE_DRAW_WINDOW` | 32 |
| Expected wins per committed ORU | `n / 1024` (`E = 246`, `C = 1024` ORU) |
| Per-draw probability | `n / (1024 · 32)` |
| σ preimage | `txid \|\| C \|\| B_{h-1} \|\| LE32(h)` (100 bytes) |
| Convergence Root boundary | \|F\| < 2 → C = 32 zero bytes |
| Draw gate | \|F\| ≥ 1 (`CONVERGENCE_DRAW_MIN_TXS_FOR_RES`) |

Code SSOT: `core/src/consensus/params.h`,
`core/src/consensus/rigor_consensus_lock.h`. This paper does not modify
those files.

## C. Consensus reject conditions (RES mint)

Public form of fail-closed mint rules:

- `bad-rigor-res-owner-mismatch` — payee is not the winner’s program
- `bad-rigor-res-winner-mismatch` — winner exists but mint is missing or
  wrong
- `bad-rigor-res-without-winner` — mint present with no winner
- `bad-rigor-res-max-supply` — would exceed 21,000,000

## D. Golden vectors (PROOF-034 archive targets)

| Height | Vector | Phenomenon |
|--------|--------|------------|
| 630,000 | GV-ORU-0003 | ORU halving / floor |
| 2,100,000 | GV-RES-0003 | RES cap |
| 2,100,001 | GV-DRAW-0003 | DRAW reject |

Catalog and procedure:
`docs/archive/proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md`.

## E. Mathematical notes

- Hash256 = SHA-256(SHA-256(·))
- Inequality arithmetic is ≥512-bit integer; no floating point
- Shipping: `E = 246`, `C = 1024` ORU per RES mint, expected wins `n / 1024`
  (Deterministic Reserve Issuance). Historical RFC-014 / retired design:
  `2^240 / 2^256 = 1/65536` (`C = 65536` ORU) — teaching slice only, not the
  live target.
- 125 = 5³ is symbolic interpretation of the locked tail, not a
  selection rationale

## F. Verification reports (pointers)

| Document | Role |
|----------|------|
| `docs/verification/FORMAL-VERIFICATION-STATUS.md` | Single current correspondence status (freeze) |
| `docs/verification/FORMAL-VERIFICATION-FREEZE.md` | Operator freeze checklist |
| `docs/verification/FORMAL-VERIFICATION-VERDICT.md` | What the Coq model proves |
| `docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md` | Intentional vector-set splits |
| `docs/verification/README.md` | Formal-verification index |
| `docs/architecture/CONSENSUS_FINGERPRINT_LIBRARY.md` | Fingerprint contract |
| `docs/archive/rfc/RFC_HISTORY.md` | Archived RFC names / retired design |

## G. Prior public narratives (kept, not deleted)

- `docs/WHITEPAPER.md`
- `docs/release/WHITEPAPER_PUBLIC_VERSION.md`
- `docs/release/RIGOR_WHITEPAPER_V1_FINAL.md`
- `docs/whitepaper/chapters/` (pre-restructure chapter files)
- `docs/archive/rfc/RFC_HISTORY.md` (retired-design map)

## H. Website publishing note

Long-form route **`/documentation/whitepaper`** is live on
`web/rigorchain-site/`. Formal verification freeze:
**`/documentation/formal-verification`**. Security architecture:
**`/documentation/security`**. Minimal script architecture:
**`/documentation/security/minimal-script-architecture`**.

Live routes that this paper may cite (do not treat placeholders as
finished product copy):

| Route | Status in `App.tsx` |
|-------|---------------------|
| `/documentation` | Live hub |
| `/documentation/whitepaper` | Live long-form |
| `/documentation/formal-verification` | Live freeze status |
| `/documentation/security` | Live security architecture hub |
| `/documentation/security/minimal-script-architecture` | Live minimal-script chapter |
| `/mining` | Live overview |
| `/wallet` | Live |
| `/explorer` | Live portal |
| `/zero-trust-verification` | Live verifier |

Visual requirements (implementation later): dark technical theme; black
background; white text; gray input/card style; readable long-form
layout. Do not delete Zero Trust, Explorer, Mining, Lab, or Experiment
pages.

## I. Terminology

| Term | Official meaning |
|------|------------------|
| ORU | Medium of exchange asset (fees, subsidy, Convergence weight) |
| RES | Store of value asset (capped; deterministically issued after Convergence — not mined) |
| Deterministic Reserve Issuance | Protocol RES issuance: fixed schedule, 21M cap, Convergence authorization, deterministic `W(h)` |
| Deflationary Fee Burn | Protocol ORU destruction of eligible RES-transfer fee components; not a UTXO or global supply state |
| Convergence | ORU consumption mechanism creating RES eligibility |
| Convergence Root | Deterministic commitment to eligible convergence transactions |
| Draw Candidate Root | Deterministic commitment used for candidate selection after convergence eligibility is established |
| Fingerprint | Verification identity layer (not a consensus-rule replacement) |
| PROOF-034 | Runtime archive campaign (Layer 3). `equivalence_claimed = false` |

---

## Closing

RIGOR exists because one asset cannot honestly be medium of exchange,
store of value, and security budget at once.

It works because **Convergence** turns costly ORU commitments into a
public, deterministic RES allocation.

It is checkable because every node, and any browser verifier, recomputes
the same function from public data.

RIGOR is a two-asset UTXO protocol with a closed script-compliance
surface. Script is the compliance/classification layer. RES is
deterministically issued after protocol-defined conditions — not mined.
Deflationary Fee Burn removes eligible ORU fee components from
circulation without creating a burn UTXO or a global supply table.

RIGOR is not a programmable blockchain attempting unlimited
functionality. RIGOR is a mathematically focused monetary protocol built
on Bitcoin's proven UTXO foundation, strengthened through native dual
assets, strict asset isolation, minimal scripting, and a formal
verification **model**. The production C++ implementation is not claimed
to be formally equivalent.

```
equivalence_claimed = false
```
