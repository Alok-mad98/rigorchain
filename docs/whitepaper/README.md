# RIGOR Protocol Whitepaper folder

**Start here:** [`RIGOR_PROTOCOL_WHITEPAPER.md`](./RIGOR_PROTOCOL_WHITEPAPER.md)
(Chinese first page: [`RIGOR_PROTOCOL_WHITEPAPER.zh.md`](./RIGOR_PROTOCOL_WHITEPAPER.zh.md)).

That file is the public protocol narrative. Chinese chapter twins are
`chapters/*.zh.md`. It is not protocol SSOT; live RFCs are.

```
equivalence_claimed = false
```

Short companions (audit-sized answers, not a substitute for the paper):

| Document | Question |
|----------|----------|
| [`RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.md`](./RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.md) | One-page identity (Chinese: [`RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.zh.md`](./RIGOR_PROTOCOL_IDENTITY_ONE_PAGE.zh.md)) |
| [`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md) | What is RIGOR? |
| [`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | How does the mathematics run? |
| [`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md) | What is the model, and where does proof stop? (live claims: chapter 8 / SOT) |

Chapters bound into the paper:

| Document | Role |
|----------|------|
| [`chapters/01-single-asset-problem.md`](./chapters/01-single-asset-problem.md) | Chapter 1 — why RIGOR separates two monetary functions |
| [`chapters/01-single-asset-problem.zh.md`](./chapters/01-single-asset-problem.zh.md) | Chapter 1 (Chinese) |
| [`chapters/02-two-assets.md`](./chapters/02-two-assets.md) | Chapter 2 — rules and parameters |
| [`chapters/02-two-assets.zh.md`](./chapters/02-two-assets.zh.md) | Chapter 2 (Chinese) |
| [`chapters/03-native-utxo.md`](./chapters/03-native-utxo.md) | Chapter 3 — native UTXO identities |
| [`chapters/03-native-utxo.zh.md`](./chapters/03-native-utxo.zh.md) | Chapter 3 (Chinese) |
| [`chapters/04-convergence.md`](./chapters/04-convergence.md) | Chapter 4 — Convergence as the one-way bridge |
| [`chapters/04-convergence.zh.md`](./chapters/04-convergence.zh.md) | Chapter 4 (Chinese) |
| [`chapters/05-mathematical-fairness.md`](./chapters/05-mathematical-fairness.md) | Chapter 5 — checkable mathematics |
| [`chapters/05-mathematical-fairness.zh.md`](./chapters/05-mathematical-fairness.zh.md) | Chapter 5 (Chinese) |
| [`chapters/06-mining.md`](./chapters/06-mining.md) | Chapter 6 — proof-of-work, shared height |
| [`chapters/06-mining.zh.md`](./chapters/06-mining.zh.md) | Chapter 6 (Chinese) |
| [`chapters/07-self-proving.md`](./chapters/07-self-proving.md) | Chapter 7 — local verification |
| [`chapters/07-self-proving.zh.md`](./chapters/07-self-proving.zh.md) | Chapter 7 (Chinese) |
| [`chapters/08-formal-boundary.md`](./chapters/08-formal-boundary.md) | Chapter 8 — formal boundary |
| [`chapters/08-formal-boundary.zh.md`](./chapters/08-formal-boundary.zh.md) | Chapter 8 (Chinese) |
| [`chapters/09-product-surfaces.md`](./chapters/09-product-surfaces.md) | Chapter 9 — product surfaces |
| [`chapters/09-product-surfaces.zh.md`](./chapters/09-product-surfaces.zh.md) | Chapter 9 (Chinese) |
| [`chapters/10-security-model.md`](./chapters/10-security-model.md) | Chapter 10 — security model and threat boundaries |
| [`chapters/10-security-model.zh.md`](./chapters/10-security-model.zh.md) | Chapter 10 (Chinese) |
| [`chapters/11-trade-offs.md`](./chapters/11-trade-offs.md) | Chapter 11 — design trade-offs |
| [`chapters/11-trade-offs.zh.md`](./chapters/11-trade-offs.zh.md) | Chapter 11 (Chinese) |
| [`chapters/12-evolution-policy.md`](./chapters/12-evolution-policy.md) | Chapter 12 — deployment and evolution policy |
| [`chapters/12-evolution-policy.zh.md`](./chapters/12-evolution-policy.zh.md) | Chapter 12 (Chinese) |
| [`chapters/10-appendix.md`](./chapters/10-appendix.md) | Appendix — protocol constants and engineering reference |
| [`chapters/10-appendix.zh.md`](./chapters/10-appendix.zh.md) | Appendix (Chinese) |
| [`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md) | Appendix D — Convergence mathematical design note |
| [`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md`](./RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.zh.md) | Appendix D (Chinese) |
| [`chapters/09-glossary.md`](./chapters/09-glossary.md) | Noun list (not Chapter 9 narrative) |
| [`RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md`](./RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md) | Technical paper structure |
| [`chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`](./chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md) | Minimal script chapter |

Implementation facts: [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).
Opcode spec: [`../specification/RIGOR_OPCODE_SPECIFICATION.md`](../specification/RIGOR_OPCODE_SPECIFICATION.md).

RFC corpus (historical, not public identity): [`../archive/rfc/`](../archive/rfc/).
Live PoW name is **OpuHash** = SHA3-256(`"RGRPOW01" || header_80`).
