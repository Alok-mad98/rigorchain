# Whitepaper Claim Audit

**Reviewer role:** independent protocol / cryptoeconomics reviewer  
**Object:** `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` and bound chapter twins  
**Date:** 2026-09-15  
**Scope:** claim wording only. No protocol change. No RFC change. No formula change.

```
review_modifies_consensus = false
review_modifies_rfc = false
review_rewrites_whitepaper = false
```

Classification:

| Tag | Meaning |
|-----|---------|
| VALID | Directly defined protocol fact, or a clearly scoped non-claim |
| NEEDS QUALIFICATION | Technically defensible, easy to over-read |
| EXCESSIVE | Not entailed by the protocol |

Action: `keep` or `modify` (recommendation only; this audit does not edit the paper).

Primary sentence source is the bound paper. Chapter-file twins are noted when they differ or amplify the claim.

---

## Front matter and identity

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Document status: “Production specification” | Title block | NEEDS QUALIFICATION | modify |
| “Official protocol narrative” | Title block | NEEDS QUALIFICATION | modify |
| Core identity includes “Deterministic Fairness” | Core identity box | NEEDS QUALIFICATION | modify |
| Core identity includes “Zero Trust Verification” | Core identity box | NEEDS QUALIFICATION | modify |
| Architecture includes “Deterministic Reserve Issuance” | Official architecture formula | NEEDS QUALIFICATION | modify |
| Architecture includes “Deflationary Fee Destroy” | Official architecture formula | NEEDS QUALIFICATION | modify |
| Architecture includes “Formal Verification Model” | Official architecture formula | NEEDS QUALIFICATION | modify |
| “deflationary transaction economics” | Architecture identity | NEEDS QUALIFICATION | modify |
| “The formal system verifies protocol models and finite vectors.” | Architecture identity | VALID | keep |
| “The production C++ implementation is not claimed to be formally equivalent.” | Architecture identity | VALID | keep |
| ORU subsidy 128 ORU per block (`h ≥ 1`) | Abstract | VALID | keep |
| RES maximum supply `2²²` RES / `419430400000000` atoms | Abstract | VALID | keep |
| RES is not created by mining subsidy | Abstract | VALID | keep |
| Mint predicate `R · (2²⁵ + h)² ≤ B << 40` | Abstract | VALID | keep |
| `destroy_cost(0) = 102400000000` (1024 ORU per 1 RES) | Abstract | VALID | keep |
| `destroy_cost(h)` is a protocol cost, not a market quote | Abstract | VALID | keep |
| “The protocol defines the admission rule; market demand determines whether participants use it.” | Abstract | VALID | keep |
| “It does not make claims about market adoption, valuation, or future security outcomes.” | Abstract | VALID | keep |

**Notes.**

- “Production specification” collides with `documentation_consensus_authority = false` used elsewhere. A reader of Nakamoto (2008) or the Yellow Paper treats “specification” as normative. Recommend: “Public protocol narrative (not consensus SSOT).”
- “Deterministic Fairness” is defined later as same `(R,B,h)` → same admit/reject. On the cover it reads as equal outcomes.
- “Zero Trust” is a product-surface name. In security literature it is a different claim. The abstract definition is acceptable; the identity box is not.
- “Deflationary” describes circulating-stock reduction if destroy occurs. It is not a price claim, but the adjective is a price word. Prefer “fee destroy” / “circulating-supply sink.”
- “Formal Verification Model” on the architecture list is true only with Chapter 8’s qualifier in the same visual unit.

---

## Chapter 1 — monetary separation

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| A PoW chain must simultaneously support MoE, SoV, and security incentive | §1 opening | NEEDS QUALIFICATION | modify |
| Single-asset finite supply creates a security-budget tension | §1 opening | NEEDS QUALIFICATION | modify |
| Design context: shares a cryptoeconomic observation; does not copy CKB | Design context | VALID | keep |
| MoE users have little incentive to fund a security budget they barely consume | §1.1 | NEEDS QUALIFICATION | keep |
| SoV users transact infrequently | §1.1 | NEEDS QUALIFICATION | keep |
| “Holders will not tolerate an insecure or captured chain” | Bound § Path B | NEEDS QUALIFICATION | modify |
| Post-cap fee replacement is an unresolved empirical question | Path B | VALID | keep |
| “RIGOR’s answer is not a market promise. It is an asset split at consensus.” | §1 close | VALID | keep |
| ORU: `subsidy(0)=0`, `subsidy(h≥1)=128` | §1.5 / expanded 1.5 | VALID | keep |
| RES is not mined; coinbase never creates it | §1.5 | VALID | keep |
| Reverse conversion forbidden | §1.5 | VALID | keep |
| Unit cost rises with height (duration-proportional payment) | §1.5 | NEEDS QUALIFICATION | modify |
| Ceiling needs no consensus counter because `destroy_cost` is strictly increasing | §1.5 | VALID | keep |
| Realised stock is `ρ · 2²²` | §1.5 | VALID | keep |
| “bounded by mint arbitrage” | Expanded §1.5 | NEEDS QUALIFICATION | modify |
| “Hoarding is isolated on RES.” | Bound §1.6 | NEEDS QUALIFICATION | modify |
| “ORU can develop a fee market without competing with a reserve narrative” | Bound §1.6 | NEEDS QUALIFICATION | modify |
| When RES demand exists, it is an ORU sink | Bound §1.6 | VALID | keep |
| Protocol does not invent demand | Bound §1.6 | VALID | keep |
| Does not assert `ρ` will be high | §1 close | VALID | keep |
| Year-1 issuance ≤ about 6 564 RES at `ρ=100%` | §1 close | VALID | keep |
| Thin market cannot transmit demand into destroy (adoption risk, not consensus hole) | §1 close | VALID | keep |

**Notes.**

- “Must do three jobs” is a design thesis, not a theorem. Qualify: “under the security model this paper considers.”
- Security-budget tension is an open research question (Carlsten et al., 2016), not a disproof of Bitcoin. The paper already says it is not a disproof; keep that sentence adjacent to every strong wording.
- “Holders will not tolerate…” is a behavioural prediction. Prefer: “SoV users, as modelled here, treat insecurity as a direct attack on the stock.”
- Duration-proportional payment is a protocol-cost schedule, not a proof that earlier minters receive more *security*. Hashpower is not a function of mint height.
- Mint arbitrage bounds a *secondary ratio* only if capital, latency, and inventory exist. The protocol publishes a destroy requirement. It does not enforce a market price.

---

## Chapter 2 — rules

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Protocol fixes a transform, not a price or participation rate | §2 opening | VALID | keep |
| “Proof-of-work discovers blocks and gives irreversible height order.” | Bound §2 / `02-two-assets.md` | EXCESSIVE | modify |
| PoW does not create RES | §2 | VALID | keep |
| `RES → ORU` illegal | §2.1 | VALID | keep |
| Inflation rate decays as `1/t`; residual level is `1−ρ` | §2.2 | VALID | keep |
| Under the protocol model, `ρ=100%` corresponds to constant circulating ORU | §2.2 | NEEDS QUALIFICATION | modify |
| Mint predicate as written | §2.3 | VALID | keep |
| Cost is strictly increasing | §2.3 | VALID | keep |
| “earlier participation receives a lower protocol-defined destroy cost” | §2.3 | VALID | keep |
| “pays less per RES for a longer remaining span of protection” | §2.3 | NEEDS QUALIFICATION | modify |
| Receiver is the minting tx RES outputs, not the miner | §2.3 | VALID | keep |
| `RES_DESTROY_SHIFT = 40 = 50 − log₂(1024)` | §2.4 | VALID | keep |
| Pole `h=2²⁵` is median of `ρ=100%` distribution (year 638) | §2.4 | NEEDS QUALIFICATION | modify |
| RFC-018 destroys `floor(fee/2)` of ORU fee on RES transfers | §2.5 | VALID | keep |
| Enforced vs not guaranteed list | §2.6 | VALID | keep |

**Notes.**

- Chapter 6 correctly states history *can* be rewritten under sufficient hashpower. “Irreversible height order” in Chapter 2 is the highest-severity wording error in the bound paper. Replace with “a shared chain-order index among honest nodes replaying the admitted chain.”
- `ρ=100%` ⇒ constant circulating ORU holds only if destroy equals issuance on the intended path (every new subsidy destroyed). State the premise.
- “Year 638” converts blocks to calendar years at 600 s. ASERT targets 600 s; it does not bind `h` to wall-clock time (Chapter 6). Label as observational calendar, not a protocol date.

---

## Chapter 3 — native UTXO

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Asset class is inferred from `scriptPubKey` | Ch. 3 | VALID | keep |
| No token registry / account ledger / contract storage | Ch. 3 | VALID | keep |
| Same accepted block bytes → same validity decision | Ch. 3 | VALID | keep |
| Witness v0 = ORU; Witness v1 = RES | Ch. 3 | VALID | keep |
| Convergence output not inserted into the UTXO set | Ch. 3 / 4 | VALID | keep |

No excessive economic claims found in this chapter’s rule statements.

---

## Chapter 4 — Convergence

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| One visible destroy and an integer predicate in the same tx | §4 opening | VALID | keep |
| Does not fix that anyone will destroy | §4 opening | VALID | keep |
| Not mining, not 1:1, not a draw, not a smart contract, not a miner prize | §4.2 | VALID | keep |
| Integer predicate is the exact integer form of the mint condition | §4.2 / expanded §4.5 | VALID | keep |
| Algebraically equivalent to the display-cost condition; no rounding or truncation | Expanded §4.5 | VALID | keep |
| Same `(R,B,h)` → same admit/reject | §4 | VALID | keep |
| Secondary ratio “bounded by mint arbitrage” | Expanded §4.5 | NEEDS QUALIFICATION | modify |
| No per-block mint cap is deliberate | Expanded §4.5 | VALID | keep |
| Mathematical Note is explanatory; introduces no new consensus rules | §4 close | VALID | keep |

---

## Chapter 5 and Appendix D — mathematics

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Display form `destroy_cost(h) = 1024 · (2²⁵+h)² / 2⁵⁰` | §5.2 / Note §1 | VALID | keep |
| Predicate is the exact integer form of the mint condition | §5.2 | VALID | keep |
| Exact integer-preserving equivalence, not approximation | §5.2 / Note §1 | VALID | keep |
| No rounding, floor, truncation, or discarded remainder in the *cleared* relation | Note §1 | VALID | keep |
| Display helper `>> 50` may sit one atom below exact whole-coin destroy | Note §1 | VALID | keep |
| Shift 40 is forced by `50 − 10` | §5.2 | VALID | keep |
| Closed-form integral `(S/P0)·H = 2²²` | §5.3 | NEEDS QUALIFICATION | modify |
| Ceiling is an upper bound, not a calendar promise | §5.4 | VALID | keep |
| Fairness = same public input, same deterministic result | §5.1 | VALID | keep |
| Fairness ≠ equal mint amount, timing, or economic outcome | §5.1 / Note §7 | VALID | keep |
| Left side ~`2¹¹⁰`, right ~`2⁹⁹` vs `__int128` `2¹²⁸` | §5.6 / Note §8 | NEEDS QUALIFICATION | keep |
| Overflow arguments prove bounded integer safety, not C++ refinement | Note §8 | VALID | keep |
| `destroy_cost(h)` is not a price oracle | Note §4 | VALID | keep |
| `2²²` is derived, not a governance parameter | Note §5 | VALID | keep |

**Notes.**

- The live consensus object is the discrete inequality, not the integral. The integral is a closed-form *upper bound* on a continuous relaxation. The paper mostly says this; keep “integral” from being read as the consensus rule.
- Worst-case bit widths are engineering bounds under stated caps (`RES_MAX_SUPPLY`, `MAX_MONEY`, `INT32_MAX`). They are not a Coq overflow theorem about C++. The Note already separates these. Chapter 5 should not be shortened to “proved not to overflow.”

Rejected wording (not found as live claims in Ch. 5 / Appendix D): “optimization,” “approximation” (except as denied), “better formula,” “superior algorithm.”

---

## Chapter 6 — PoW

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Mining pays ORU; mining does not mint RES | §6 | VALID | keep |
| Mining security and reserve issuance are intentionally separate | Expanded §6 | VALID | keep |
| Does not claim honest majority | §6.7 | VALID | keep |
| Does not claim history cannot be rewritten under sufficient hashpower | §6.1 / §6.7 | VALID | keep |
| OpuHash = SHA3-256(`"RGRPOW01" \|\| header_80`) for PoW validity | §6 | VALID | keep |
| Block identity remains SHA256d | §6 | VALID | keep |
| ASERT does not enter the mint predicate | §6 | VALID | keep |

This chapter is the correct qualifier for Chapter 2’s “irreversible” sentence.

---

## Chapters 7–9 — verification, formal boundary, products

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Display is observation; node decides | §7 | VALID | keep |
| Explorer displays; wallet constructs; verifier recomputes | Expanded §7 | VALID | keep |
| Fingerprint is observational, not a consensus rule | §7.5 | VALID | keep |
| Formal verification of the protocol model ≠ C++ equivalence ≠ running-binary proof | §8 | VALID | keep |
| Verified object is the mathematical model; running implementation is an engineering artifact | §8 | VALID | keep |
| Public sentence: machine-checked invariants *inside the Coq kernel model* | §8.5 | VALID | keep |
| Not yes: running C++ node equals the model | §8.5 | VALID | keep |
| Website is not authority; explorer is not consensus; pool is not issuer | §9 | VALID | keep |
| Product completeness ≠ consensus completeness | §9 | VALID | keep |
| “That is self-proving” | Expanded §7.3 | NEEDS QUALIFICATION | modify |

**Notes.**

- “Self-proving” overclaims. The block is valid because recomputation matches commitments, not because it proves itself to a stranger without a verifier. Prefer the Chapter 7 title: local / independent verification.

---

## Chapters 10–12 — security, trade-offs, evolution

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| Protocol defines detect-and-reject conditions | §10 | VALID | keep |
| Does not claim attacks cannot occur | §10 | VALID | keep |
| Does not guarantee market participation | §10.2 | VALID | keep |
| Security budget in protocol units has a floor; in an external unit it does not | §10.2 | VALID | keep |
| Assumptions and limits: honest majority, network, hardware, adoption, FX, wallet, compiler not proved | Expanded §10.4 | VALID | keep |
| Dual-asset design is a different trade-off, not “better than one asset” | §11.2 | VALID | keep |
| Not presented as strictly better than Bitcoin | §11.4 | VALID | keep |
| Documentation additions are not consensus changes | §12.1 | VALID | keep |
| Only consensus code + live RFC rules define protocol behavior | Expanded §12.1 | VALID | keep |
| Does not promise bugs absent, operational stability, market value, demand, or `ρ` | §12.4 | VALID | keep |

---

## Companion papers (not rewritten here)

| Claim | Location | Classification | Action |
|-------|----------|----------------|--------|
| “formally verified consensus invariants” (qualified in the next paragraph as model properties) | `RIGOR_PROTOCOL_IDENTITY.md` | NEEDS QUALIFICATION | modify |
| Shorter public phrase “formally verified critical consensus invariants (consensus kernel only)” | `formal_verification/rigor_kernel/release/PublicClaims.md` | NEEDS QUALIFICATION | modify |
| Glossary: “Hard cap — guaranteed” | `chapters/09-glossary.md` | NEEDS QUALIFICATION | modify |
| Glossary: “PoW only provides irreversible order” | `chapters/09-glossary.md` | EXCESSIVE | modify |

The identity paper immediately qualifies the phrase. The *shorter* PublicClaims line drops “Coq model” and was previously flagged in independent formal-bridge review. It is the most likely sentence to leak onto a website badge.

---

## Summary counts (bound paper + named companions)

| Classification | Count (this table) |
|----------------|--------------------|
| VALID | majority of rule statements |
| NEEDS QUALIFICATION | cover identity, economic mechanism language, calendar conversions, integral vs discrete |
| EXCESSIVE | “irreversible height order” (Ch. 2 / glossary); no “solves Bitcoin” or “superior monetary system” found in the bound paper |

No live sentence of the form “RIGOR creates a superior monetary system” or “RIGOR is formally verified” (unqualified) was found in `RIGOR_PROTOCOL_WHITEPAPER.md`. Residual risk is compression of qualified sentences, not absence of Chapter 8.
