# Chapter 10 — Security Model and Threat Boundaries

Chapters 1–9 stated the dual-asset rules, the verification act, and
the formal boundary. This chapter states which failures the protocol
is designed to detect, and which risks remain outside consensus.

The claim is architectural. The protocol defines **detect-and-reject
conditions**. It does not claim that attacks cannot occur, that
hashpower cannot rewrite recent history, or that market participation
will fund security.

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

---

## 10.1 Consensus attack boundaries

Full-node validation is designed to reject invalid state transitions.
The following cases are already named by live reject codes and
classification rules (RFC-020, Chapters 3–4). This section does not
add a rule.

| Case | Protocol response |
|------|-------------------|
| Invalid RES mint | Predicate `R > 0 → R · (2²⁵ + h)² ≤ B << 40` fails; the transaction is invalid |
| Missing `OP_CONVERGE` | `bad-txns-res-mint-no-burn`: a RES output without a visible destroy |
| Underpaid RES mint | `bad-txns-res-mint-underpaid`: destroy does not cover requested `R` |
| Illegal RES → ORU conversion | `illegal-res-to-oru-conversion`: reverse path, or spending a Convergence output |
| Invalid asset classification | Closed templates: non-listed script shapes are not accepted as new monetary outputs |
| Chain reorganisation | Most-work order; mint and coinbase maturity 100 as reorg protection |

A reorganisation can still occur. The protocol does not claim that
history cannot be rewritten under sufficient hashpower. Maturity 100
is a confirmation-depth convention already used for coinbase. It is
not a proof that a reorg of that depth is impossible.

An underpaid mint that a pool or explorer already displays as credited
remains invalid. Display does not admit a state transition. Chapter 7.

---

## 10.2 Economic attack boundaries

The protocol specifies a conversion mechanism. It does not guarantee
market participation.

| Pressure | What the protocol does | What it does not do |
|----------|------------------------|---------------------|
| ORU inflation pressure | Residual inflation level is `1 − ρ`; hardness is demand-led if destroy occurs | Guarantee that `ρ` is high, or that ORU has a terminal cap |
| Insufficient RES demand | Minting simply does not occur; `S_RES` stays below the derived ceiling | Create demand for RES |
| Low destroy participation | Realised stock is `ρ · 2²²`; the ceiling is an upper bound | Fill the ceiling |
| Weak fee market | RFC-018 destroys half of RES-transfer ORU fees; ORU subsidy remains 128 | Guarantee a fee market that replaces issuance |
| Miner incentive pressure | Coinbase pays 128 ORU per block after genesis | Guarantee the external purchasing power of that 128 ORU |

The security budget in protocol units has a floor. The security budget
in an external unit does not. Whether miners find the ORU payment
sufficient is a market outcome.

---

## 10.3 External dependency boundaries

The following are outside consensus, as already implied by Chapters 8
and 9:

- wallet software correctness and UX
- user key custody
- network connectivity and P2P honesty
- hardware reliability and side channels
- third-party services (pools, explorers, websites, indexers)

A full node may reject an illegal block. It cannot make a stolen key
unspendable, a disconnected peer honest, or an explorer’s display
authoritative. Formal verification of the Coq model does not cover
these surfaces, and it does not prove the running C++ binary.

---

## 10.4 Assumptions and limits

This is not a list of defects. It is the explicit model boundary.

The protocol does not prove:

- honest majority
- network behavior
- hardware availability
- market adoption
- exchange rate
- wallet safety
- compiler correctness

Those remain assumptions or external facts. Detect-and-reject
conditions in §10.1 still apply inside the model. They do not convert
the items above into theorems.

---

## 10.5 What this chapter does not claim

- It does not claim that listed attacks cannot occur.
- It does not claim a security comparison with Bitcoin.
- It does not claim that 128 ORU is a sufficient real-world security
  budget.
- It does not claim that C++ is equivalent to the kernel model.
- It does not add a reject code, a maturity rule, or a hashpower
  assumption beyond what earlier chapters already stated.

**The protocol defines detection and rejection. It does not define
market demand.** Chapter 11 states what this architecture intentionally
does not provide.
