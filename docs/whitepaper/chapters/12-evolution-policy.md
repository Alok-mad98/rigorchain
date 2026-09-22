# Chapter 12 — Deployment and Evolution Policy

Chapter 11 recorded the omitted capabilities. This chapter states how
the deployed protocol is intended to be maintained: what is frozen,
what may change as software, and what would require an explicit RFC.

The claim is policy, not a schedule. This chapter does not promise a
date, a hard fork, a feature, or a market outcome.

```
frozen consensus rules
        ≠
frozen wallets, explorers, and docs
        ≠
a promise of zero defects
```

---

## 12.1 Consensus freeze

RFC-018 and RFC-020 are the live issuance and fee-destroy rules.
They are treated as frozen for the running protocol described in this
paper. The running C++ node remains the source of truth for what is
enforced.

Updates to wallets, explorers, mining tools, and documentation are
not protocol upgrades. Chapter 9 already stated that completing a
guide or a page does not change the predicate, the 128 ORU subsidy,
the classifier, or the derived ceiling.

Documentation additions are not consensus changes. Only consensus
code together with the live RFC rules defines protocol behavior.
This paper may clarify wording. It does not, by existing, alter
admit-or-reject.

---

## 12.2 RFC process

A future change to consensus would require, at minimum:

- an explicit RFC stating the intended rule
- public discussion
- an implementation
- tests
- review

This list is a process outline. It is not a commitment to any
particular upgrade, timeline, or activation method. Historical RFC
numbers that are not live rules remain historical (appendix). This
paper does not edit RFC texts.

---

## 12.3 Software evolution boundary

May change without being a consensus change:

- wallet UI
- explorer presentation
- mining tools and GBT operational paths
- documentation and this paper’s wording, provided rules are not
  rewritten

Require strict review, because they *are* consensus if they change
admit-or-reject:

- consensus rules
- transaction validation
- asset-creation rules (mint locus, predicate, reverse ban,
  classification)

A display remaining a display (Chapter 9) still applies after a
wallet release.

---

## 12.4 Release philosophy

RIGOR does not promise:

- absence of bugs
- permanent operational stability
- market value, demand, or `ρ`

It does commit to keep public:

- the rules (RFC-018, RFC-020, and the running node)
- verification paths (full-node recomputation; selected-field tools)
- implementation boundaries (model ≠ C++; display ≠ validity)

Protocol design does not create market demand. When demand exists,
the published mechanisms translate it into security-related flows.
Whether demand arrives remains an external factor.

---

## 12.5 What this chapter does not claim

- It does not schedule a fork or name a next consensus RFC.
- It does not claim the running binary is bug-free.
- It does not claim the Coq model covers C++.
- It does not write a product release as a protocol upgrade.

**Rules public. Verification paths public. Implementation boundaries
public.** Lookup of frozen constants follows in the appendix.
