# References

This file is bibliographic context for the RIGOR protocol whitepaper. It
does not add a consensus rule, a parameter, or a market forecast.

RIGOR should be evaluated as an independent protocol design.

```
documentation_consensus_authority = false
```

---

## Bitcoin

Satoshi Nakamoto, *Bitcoin: A Peer-to-Peer Electronic Cash System*,
2008. <https://bitcoin.org/bitcoin.pdf>

**Purpose.** Proof-of-work origin; UTXO ownership transfer; public
replay of a shared transaction history without a trusted intermediary;
costly revision of chain order (longest / most-work chain in the
original presentation).

RIGOR retains permissionless PoW ordering and UTXO-based transfer. It
explores a different native-asset assignment. Citing Nakamoto is not a
claim of Bitcoin compatibility, and not a claim that Bitcoin has been
disproved.

---

## CKB cryptoeconomics

Nervos Network, RFC-0015, *Crypto-Economics of the Nervos Common
Knowledge Base*.

**Purpose.** Prior discussion of monetary-function conflict: medium of
exchange, store of value, and security-budget pressures can pull
against one another when assigned to a single native asset.

RIGOR **references the economic observation**, not the implementation
model. RIGOR does not copy CKB’s state-capacity, state-rent, or
smart-contract platform design. It applies the observation through a
separate dual-asset UTXO architecture (ORU / RES / Convergence) as
specified in RFC-020 and RFC-018.

Do not read this citation as “RIGOR is based on CKB.”

---

## Fee security-budget research

Long-term miner payment after subsidy decline is an open research
question in proof-of-work systems. It is not a settled theorem.

Miles Carlsten, Harry Kalodner, S. Matthew Weinberg, and Arvind
Narayanan, *On the Instability of Bitcoin Without the Block Reward*,
ACM CCS, 2016.

**Purpose.** Background on miner incentives and the post-subsidy
security-budget discussion. Other academic and industry writing
addresses the same open question; this entry is representative, not
exhaustive.

RIGOR approaches the question by separating miner payment (ORU) from
the scarce-stock role (RES) at the consensus asset layer. That is an
alternative design exploration.

**This bibliography does not claim** that RIGOR solves all fee-security
problems, that 128 ORU is sufficient in any external unit, or that any
fee market will or will not form.

---

## Related RIGOR documents (not external literature)

| Document | Role |
|----------|------|
| [`RIGOR_PROTOCOL_WHITEPAPER.md`](./RIGOR_PROTOCOL_WHITEPAPER.md) | Public protocol narrative |
| [`chapters/04-convergence.md`](./chapters/04-convergence.md) | Convergence as the one-way bridge |
| [`chapters/05-mathematical-fairness.md`](./chapters/05-mathematical-fairness.md) | Predicate and derived ceiling |
| [`chapters/08-formal-boundary.md`](./chapters/08-formal-boundary.md) | Model ≠ running binary |
| [`chapters/10-security-model.md`](./chapters/10-security-model.md) | Detect-and-reject vs external assumptions |
| [`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md) | Appendix D; explanatory algebra |
| [`../protocol/RFC-020.md`](../protocol/RFC-020.md) | Live RES issuance |
| [`../protocol/RFC-018.md`](../protocol/RFC-018.md) | RES-transfer ORU fee destroy |

Live admit-or-reject is defined by consensus code together with the
live RFC rules. These references do not change that.
