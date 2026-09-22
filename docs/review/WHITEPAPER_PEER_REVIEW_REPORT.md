# RIGOR Whitepaper Peer Review Report

**Reviewer stance:** independent blockchain protocol / cryptoeconomics review  
**Primary object:** `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`  
**Also read:** chapter twins, Appendix D (`RIGOR_CONVERGENCE_MATHEMATICAL_NOTE.md`), Chapter 8 companions, `docs/verification/FORMAL-VERIFICATION-VERDICT.md`, `formal_verification/rigor_kernel/release/PublicClaims.md`, `docs/protocol/RFC-020.md` / `RFC-018.md` (for consistency only)  
**Date:** 2026-09-15

```
protocol_changes = NONE
consensus_changes = NONE
whitepaper_rewritten_by_this_review = false
```

Style compared against: Nakamoto (2008), Nervos RFC-0015, and typical academic cryptography papers (definitions first, claims scoped, non-claims explicit).

Detailed sentence table: [`WHITEPAPER_CLAIM_AUDIT.md`](./WHITEPAPER_CLAIM_AUDIT.md).  
Bibliographic file requested by this review: [`../whitepaper/REFERENCES.md`](../whitepaper/REFERENCES.md).

---

## Overall assessment

The bound paper is already in a conservative register for a dual-asset PoW design note. It does **not** claim that Bitcoin has been disproved, that markets will adopt RES, that `ρ` will be high, or that C++ equals Coq. Chapter 8, Chapter 10, Chapter 12, and Appendix D are unusually explicit about non-claims relative to typical project whitepapers.

The remaining defects are of three kinds:

1. **Cover-page compression.** Identity boxes and adjectives (“Deterministic Fairness,” “Zero Trust,” “Deflationary,” “Formal Verification Model,” “Production specification”) are stronger than the chapter text they summarise.
2. **One genuine contradiction.** Chapter 2 calls height order “irreversible.” Chapter 6 correctly refuses that claim.
3. **Economic mechanism language** that can be read as a price or security guarantee (mint arbitrage, isolated hoarding, duration of “protection”) even when nearby sentences deny market forecasts.

Relative to Nakamoto (2008), the paper is longer and more self-referential about what it does *not* prove. That humility is appropriate. It should not be spent on the cover and then contradicted in Chapter 2.

**Release recommendation (documentation only):** qualify the identity boxes, remove “irreversible,” and keep Chapter 8’s public sentence intact. Do not treat this review as a protocol change.

---

## Critical issues

### C1. “Irreversible height order” (Chapter 2)

Bound paper and `chapters/02-two-assets.md`:

> Proof-of-work discovers blocks and gives irreversible height order.

Chapter 6 states the opposite qualifier: the protocol does **not** claim that history cannot be rewritten under sufficient hashpower. Nakamoto (2008) likewise treats work as making revision *costly*, not impossible.

**Recommended wording:**

> Proof-of-work discovers blocks and supplies a shared height index among honest nodes that replay the admitted chain.

Glossary line “PoW only provides irreversible order” should match.

This is the only bound-paper sentence classified **EXCESSIVE** as a protocol-security claim.

### C2. Cover identity overclaims relative to Chapter 5 / 7 / 8

| Cover phrase | What the body actually defines | Risk |
|--------------|--------------------------------|------|
| Deterministic Fairness | Same `(R,B,h)` → same admit/reject | Read as equal mint outcomes |
| Zero Trust Verification | Display is not consensus; recompute locally | Read as a Zero-Trust *architecture proof* |
| Formal Verification Model | Selected Coq-model invariants | Read as “the chain is verified” |
| Deflationary Fee Destroy | RFC-018 circulating-ORU sink on RES transfers | Read as guaranteed price deflation |
| Production specification | Narrative; live SSOT is RFC + `core/src/` | Read as Yellow-Paper-grade normativity |

**Recommended cover replacements (documentation):**

- Deterministic Fairness → public mint predicate
- Zero Trust Verification → independent local verification
- Formal Verification Model → Coq kernel *model* (see Chapter 8)
- Deflationary Fee Destroy → fee destroy (RFC-018)
- Production specification → public protocol narrative (not consensus SSOT)

### C3. Shorter “formally verified” phrase outside the bound paper

`RIGOR_PROTOCOL_IDENTITY.md` says “formally verified consensus invariants” and qualifies it in the next paragraph.  
`formal_verification/rigor_kernel/release/PublicClaims.md` still offers a shorter line that drops “Coq model”:

> formally verified critical consensus invariants (consensus kernel only)

That compression is the sentence most likely to be copied onto a website. The bound Chapter 8 sentence is the correct public form. This review does not edit those files; it flags them as leakage risk.

---

## Major issues

### M1. Mint arbitrage as if it were a protocol invariant

Chapters 1, 2, and 4: a secondary RES/ORU ratio above `destroy_cost(h)` is “bounded by mint arbitrage.”

The protocol defines a destroy requirement. Arbitrage is a market hypothesis (capital, inclusion, inventory, latency). RFC-020 does not enforce a secondary price.

**Preferred:**

> While minting is open, a published destroy requirement is an available alternative to buying RES. The protocol does not define a market price.

### M2. “Hoarding is isolated on RES” / “ORU can develop a fee market”

These are design *intentions*, not observed outcomes. Dual assets *permit* separating a reserve narrative from the fee asset. They do not prove that ORU will circulate or that a fee market will form.

**Preferred:**

> The protocol assigns the scarce-stock role to RES so that a reserve narrative need not attach to the miner-payment asset. Whether an ORU fee market forms is external.

### M3. “Longer remaining span of protection”

Height-rising `destroy_cost` is a duration-sensitive *cost schedule*. It does not purchase future hashpower. Security is paid in ORU subsidy and fees, block by block (Chapter 6). Earlier minters pay fewer ORU per RES; they do not receive a coupon on future work.

### M4. Calendar “year 638” and the continuous integral

`h = 2²⁵` as the `ρ=100%` median is a statement about the closed-form curve. “Year 638” assumes 600 s spacing. Chapter 6: ASERT does not bind `h` to wall-clock time.

The integral `(S/P0)·H = 2²²` is a continuous relaxation used as an **upper bound**. Consensus evaluates a discrete inequality. Keep that distinction in any shortened Chapter 5.

### M5. `ρ = 100%` and “constant circulating ORU”

True only on the theoretical path where destroy matches issuance as modelled. State the premise; do not let “constant supply” leak as an ORU hard cap (the paper correctly says ORU has none).

### M6. Economic behavioural sentences in Chapter 1

“Holders will not tolerate an insecure chain” is sociology. RFC-0015-style papers state *incentive conflict*; they do not predict holder psychology. Keep the incentive table; drop the will-not-tolerate voice.

---

## Minor wording improvements

1. Chapter 7 “self-proving”: replace with “independently checkable.” A block does not prove itself without a verifier.
2. “Zero Trust” as a product URL name can stay if Chapter 7 keeps “not a substitute for a full node.” Avoid the identity box.
3. Chapter 5 overflow “about `2¹¹⁰`”: keep as an engineering bound under stated caps; do not say “proved.”
4. Abstract “Deterministic consensus rules” for RES mint: accurate; “deterministic issuance” on the architecture list is easier to read as “issuance will occur.”
5. Chinese twins should receive the same C1–C2 edits if the English bound paper is edited later. This review did not re-audit every `.zh.md` sentence.
6. Appendix labelling: frozen parameters, engineering index, references, and the mathematical note are present, but the bound appendix still uses A–E internally while Contents lists “Appendix D” for the math note. Align labels in a later documentation pass.
7. Title “Zero-Trust Verification” repeats C2.

No “optimisation formula,” “approximation,” or “superior algorithm” claims were found as live descriptions of the mint predicate.

---

## Recommended changes

Documentation-only. **Do not** change RFC-020, RFC-018, consensus code, Coq, or parameters.

1. Replace Chapter 2 / glossary “irreversible height order.”
2. Soften cover identity boxes (C2).
3. Qualify mint-arbitrage and hoarding-isolation sentences (M1–M2).
4. Split “lower destroy cost” from “more protection” (M3).
5. Mark year-638 and the integral as observational / bounding, not consensus clocks (M4).
6. Keep Chapter 8’s long public sentence; retire the short PublicClaims variant that drops “Coq model” (C3).
7. Point the bibliography to [`REFERENCES.md`](../whitepaper/REFERENCES.md) from the bound “References and Design Context” section in a later docs pass (no new economic claim).

Priority: **C1, C2, C3** before public release of the PDF/site extract. M1–M6 can ship in the same wording pass.

---

## Claims intentionally not made

The bound paper, correctly, does **not** claim:

- market adoption of ORU or RES
- an external price or exchange rate
- that `ρ` will be high or that `2²²` will be filled
- that 128 ORU is sufficient security in an external unit
- honest majority, network honesty, or hardware availability (Chapter 10.4)
- that Bitcoin’s fee-security question is closed
- that RIGOR is strictly better than a single-asset design (Chapter 11)
- C++ ≡ Coq, `ConnectBlock` refinement, compiler correctness, or a verified running binary (Chapter 8)
- that documentation or this review changes admit-or-reject (Chapter 12)

Those non-claims should remain. The work of this review is to stop nearby sentences from being compressed into the claims the paper already refuses.

---

## Phase notes (task checklist)

| Phase | Result |
|-------|--------|
| 1 Claim audit | [`WHITEPAPER_CLAIM_AUDIT.md`](./WHITEPAPER_CLAIM_AUDIT.md) |
| 2 Economic claims | No “RES will gain value” / “ORU must maintain value” / “users will adopt” as bare promises. Residual: arbitrage, hoarding isolation, fee-market capability. |
| 3 Mathematics | Predicate remains exact integer-preserving equivalence. Overflow ≠ formal C++ proof. |
| 4 Formal boundary | Bound Chapter 8 is sound. Leakage risk is identity/PublicClaims compression. |
| 5 Protocol vs implementation | Keyword scan: bound paper is careful. “Irreversible” is the main protocol-security overclaim. Repo-wide “formally verified” still appears with required qualifiers; the short form is the hazard. |
| 6 Structure | 1–12 + appendix is sufficient. Assumptions and threat boundaries exist (Ch. 10). References exist and are now also a standalone file. No roadmap/marketing chapter should be added. |
| 7 References | [`../whitepaper/REFERENCES.md`](../whitepaper/REFERENCES.md) |

**Essential missing sections?** Not in substance. Optional later documentation (not new chapters): move Assumptions to a one-page front-matter list; align appendix letters A–D. Do not add adoption, roadmap, or marketing chapters.

---

## Publication hardening status

```
Status:
Publication hardening completed.

Protocol changes:
NONE

Consensus changes:
NONE

Mathematical model changes:
NONE

Changes:
Documentation clarity only.
```

Follow-up (2026-09-15): cover identity boxes were qualified; Chapter 2
“irreversible height order” was replaced with publicly verifiable
ordering and increasing rewrite cost; Convergence economic-limitation
paragraphs were added; a Publication Boundary section was added.
Formulas, RFC-020, RFC-018, and consensus code were not changed.
