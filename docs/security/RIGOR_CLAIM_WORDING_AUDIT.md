# RIGOR Claim Wording Audit

**Task:** `TASK-RIGOR-PROTOCOL-CONSISTENCY-FINAL-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Repository-wide wording of proof, script, and equivalence claims.  
**Consensus / wallet / miner / RFC impact:** **NONE**  
**Honesty:** `equivalence_claimed = false`

Companion: [RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md](./RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md).

This report classifies every searched occurrence. It does not change consensus.

---

## Search terms

| Term | Hits in public/docs/web/Coq honesty surfaces | Classification |
|------|-----------------------------------------------|----------------|
| `fully proven` / `fully proven blockchain` | Forbidden-list only | **Non-claim** |
| `mathematically proven blockchain` / `mathematically proven complete blockchain` | Forbidden-list / explicit reject | **Non-claim** |
| `zero risk` | Forbidden-list only | **Non-claim** |
| `bug free` / `bug-free` | Forbidden-list / “not bug-free” | **Non-claim** |
| `no scripts` | “Do not claim” / audit of prior false wording | **Non-claim** |
| `scriptless` | **None** | Absent |
| `zero scripts` / `has zero scripts` | Audit text: protocol does *not* have zero scripts | **Non-claim** |
| `complete equivalence` / `Complete C++ equivalence` | Listed under **Not claimed** | **Non-claim** |
| `equivalence_claimed` | Ubiquitous; value **false** wherever asserted | **Consistent false** |

No public page, whitepaper chapter, verification README, or Coq honesty lock **asserts** any of the forbidden slogans as a protocol fact.

---

## `equivalence_claimed = false` (required invariant)

**Asserted false (representative, not exhaustive):**

| Surface | How the flag is locked |
|---------|------------------------|
| `README.md` | Status table: **false** |
| Protocol whitepaper | Header + Ch.2 / Ch.3 / Ch.8 / closing |
| Technical whitepaper structure | Header + Ch.8 |
| `verification/README.md` and sub-READMEs | Honesty block |
| `docs/security/HOW_TO_VERIFY_RIGOR.md` | Honesty block; PASS ≠ `true` |
| `docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md` | Honesty + NOT row |
| Coq `DrawBridgeProgress.v` | `Definition equivalence_claimed : bool := false` + lemma |
| Coq Phase-5 audits | `negb equivalence_claimed = true` (proves the flag is false) |
| `formal_verification/golden_lock.json` gate | `check_golden_lock.py` fails if the Coq lock is not false |
| Website `version.ts` | `EQUIVALENCE_CLAIMED = false as const` |
| Website `AppShell` | Constitutional fault banner if explorer reports `true` |
| Footer / Documentation / Whitepaper / FV / Monetary Policy / Minimal Script / Security / Status / Experiment / Lab | Displayed `false` |

**Mentions of `equivalence_claimed = true`:** all are **prohibitions** (“not a license to set”, “FORBIDDEN”, “What PASS does not mean”). None set the flag.

**Coq `negb equivalence_claimed = true`:** machine-checked statement that the flag is false. Not a public claim of implementation equivalence.

**Inventory JSON names** such as `golden_vector_equivalence_claimed_false`: audit lemmas, not marketing copy.

**Verdict:** `equivalence_claimed = false` is consistent everywhere it is a protocol honesty statement.

---

## Forbidden slogans — occurrence classes

### Class A — Explicit forbidden lists (correct)

Used on: protocol whitepaper Ch.8, technical structure Ch.8, `verification/README.md`, `HOW_TO_VERIFY_RIGOR.md`, `RIGOR_FORMAL_CLAIM_TRACEABILITY.md`, `FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md`, Formal Verification website page, Monetary Policy page (“complete monetary correctness”).

These **reject** “fully proven blockchain”, “bug free”, “zero risk”, “mathematically proven complete blockchain”.

### Class B — Teaching / historical contrast (correct when labeled)

Whitepaper abstract and website whitepaper abstract: `2^240 / 2^256 = 1/65536` is **not** the shipping target; RFC-017 uses `E = 246`.

Zero Trust page: does **not** claim attacks are “mathematically proven impossible” except named Coq-model theorems, which remain model theorems.

### Class C — Residual public copy that can be misread (documentation only)

| Location | Text | Risk |
|----------|------|------|
| Landing `/` “65536 principle” | `P(success) = 2^−16 = 1/65536` without teaching-vs-shipping label | Reader may think this is shipping `T` |
| Website whitepaper §5 | Same 65536 identities as Ch.5 of the bound paper | Abstract already corrects; section body does not repeat `E = 246` |
| Bound whitepaper §5.1 | “published aggregate identity is `1/65536`” | Conflicts with abstract + Appendix B (`n / 1024`, `E = 246`) unless read as historical teaching |
| `RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md` §6.1 | Heading “Probability — `1 / 65536`” | Structure draft; not the live protocol paper |
| Root `README.md` | `RES_PROBABILITY_TARGET = floor(2^240 / MIN_CONVERGENCE_AMOUNT) >> 5` cited as RFC-014 v1.2 | Stale vs shipping `E = 246` in C++ / Coq / Python / RFC-017 |

These are **not** “fully proven” / “zero risk” / “no scripts” claims. They are **probability-identity drift** (see main audit PART 4 residual).

### Class D — Unrelated code comments

`core/test/functional/test_runner.py` “no scripts in the functional tests directory” and `feature_segwit.py` “no scriptsig” are Bitcoin-test English, not protocol architecture claims.

---

## Allowed public FV sentence

**Required:** *Formally verified protocol invariants with explicit verification boundaries.*

| Surface | Present? |
|---------|----------|
| `verification/README.md` | Yes (“Allowed claim”) |
| Protocol whitepaper Ch.8 | Yes (opening + freeze subsection) |
| Technical structure Ch.8 | Yes |
| `/documentation/formal-verification` | Yes (header muted + Overview; variant “within an explicitly defined verification boundary” also used) |
| `/documentation` library card | Yes |
| `RIGOR_FORMAL_CLAIM_TRACEABILITY.md` | Yes (“Citation rule”) |
| `HOW_TO_VERIFY_RIGOR.md` | Variant with “within an explicitly defined verification boundary” |
| Monetary Policy page | Scoped variant: “formally verified **monetary** invariants” plus forbidden complete-correctness sentence |

**Not allowed (confirmed absent as assertions):**

- Entire blockchain formally proven  
- All bugs impossible  
- Complete implementation equivalence  
- Fully proven / mathematically proven complete blockchain  

---

## Script wording

| Required | Status |
|----------|--------|
| “RIGOR minimizes script complexity rather than eliminating scripts.” | Present on whitepaper Ch.3, security note, registry, website Security + Minimal Script + whitepaper snippet |
| Constrained scripts exist | Present |
| No general-purpose VM / no arbitrary contracts | Present |
| Do **not** say “RIGOR has zero scripts” / “no scripts” as a feature | No such feature claim found |

---

## Dual-asset wording

Public architecture copy agrees ORU and RES are native UTXO identities at the same protocol layer, not colored coins, inscriptions, Ordinals, Runes, wrapped assets, or smart-contract tokens.

Exact five-word slogan **“native dual-asset UTXO architecture”** is **not** used as a locked phrase. Closest canonical sentence:

> RIGOR is a Bitcoin-derived UTXO architecture, a native dual-asset system, …

Semantic agreement is consistent. Slogan uniformity is residual (low).

---

## Verdict

**Claim-boundary PASS** for proof slogans and `equivalence_claimed`.  
**Residual documentation drift** is the 65536 teaching identity vs shipping `E = 246`, plus README’s RFC-014 `2^240` line — not a flip of honesty flags.
