# RIGOR Protocol Consistency Final Audit

**Task:** `TASK-RIGOR-PROTOCOL-CONSISTENCY-FINAL-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Cross-surface consistency after Minimal Script documentation, RFC-017, Formal Verification Coverage Audit, and Public Verification Architecture.  
**Consensus / wallet / miner / RFC impact:** **NONE** (read-only vs those trees).  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

**Deliverables:**

| # | Document |
|---|----------|
| 1 | This file |
| 2 | [RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md](./RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md) |
| 3 | [RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md](./RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md) |
| 4 | [RIGOR_CLAIM_WORDING_AUDIT.md](./RIGOR_CLAIM_WORDING_AUDIT.md) |

No push, merge, or tag accompanies this audit.

---

## Executive verdict

**The shipping protocol is described consistently** across C++ consensus locks, RFC-017, the Coq schedule/convergence/dual-pool models, the Python SSOT, golden-vector pins, the bound protocol whitepaper’s monetary and script chapters, the five live documentation pages, and the public `verification/` map.

**Honesty flags are consistent:** `equivalence_claimed = false` everywhere it is a protocol statement. Forbidden slogans (“fully proven blockchain”, “zero risk”, “no scripts”, “complete implementation equivalence”) appear only as **rejected** wording.

**Residual issues are documentation-only.** They do not change consensus. Highest-signal residual: public copy that still leads with the **65536 teaching identity** while shipping draw economics are **`E = 246`, `C = 1024` ORU** (RFC-017 / `Selection.v` / Appendix B of the bound paper).

| Area | Result |
|------|--------|
| PART 1 Claim slogans / `equivalence_claimed` | **PASS** |
| PART 2 Dual-asset architecture | **PASS** (slogan not used as five locked words) |
| PART 3 Minimal script architecture | **PASS** |
| PART 4 Monetary policy numbers | **PASS** (RFC-017 cone) |
| PART 4 Teaching vs shipping `T` | **RESIDUAL** (Ch.5 / landing / root README) |
| PART 5 FV claim boundary | **PASS** |
| PART 6 Verification architecture | **PASS** (links resolve in-tree) |
| PART 7 Whitepaper traceability | **Matrix created** |
| PART 8 Website | **PASS** with sitemap + 65536 residuals |

---

## Surfaces audited

| Layer | Canonical path |
|-------|----------------|
| Code | `core/src/consensus/rigor_consensus_lock.h` (RES schedule / cap); classify / interpreter / `ConnectBlock` mint (not modified) |
| Specification | `docs/rfc/RFC-017-SCHEDULED-RES-REWARD.md` (not modified); dual-asset / draw RFCs as cited |
| Coq | `formal_verification/coq/` (`ResSchedule.v`, `Selection.v`, ReplayReorgCore, Kernel harnesses) |
| Python | `formal_verification/spec/rigor_consensus_spec.py` |
| Golden vectors | `phase3_golden_vectors.json`, `golden_lock.json` (**422 / 215**) |
| Whitepaper | `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` |
| Website | `web/rigorchain-site/src/pages/{Whitepaper,SecurityArchitecture,MinimalScriptArchitecture,MonetaryPolicy,FormalVerification,Documentation,Landing}Page.tsx` |
| Security / verification docs | `docs/security/*`, `docs/verification/*`, `verification/*` |

Prior narrative `docs/WHITEPAPER.md` (2026-06-13) remains labeled prior; it is not the live protocol paper. Chapter drafts under `docs/whitepaper/chapters/01–09` are kept drafts except `RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md` (bound as Ch.3). Technical paper **structure** (`RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md`) is a design scaffold, not the live site paper.

---

## PART 1 — Repository claim consistency

Full occurrence classes: [RIGOR_CLAIM_WORDING_AUDIT.md](./RIGOR_CLAIM_WORDING_AUDIT.md).

| Phrase | Result |
|--------|--------|
| fully proven / fully proven blockchain | Forbidden-list only |
| mathematically proven blockchain | Forbidden-list / explicit reject |
| zero risk | Forbidden-list only |
| no scripts / scriptless / zero scripts | No feature claim; audits say do **not** claim this |
| complete equivalence | Listed as **not claimed** |
| `equivalence_claimed` | **false** on every honesty surface; Coq definition `false`; site `EQUIVALENCE_CLAIMED = false`; AppShell faults on `true` |

`equivalence_claimed = true` appears only as a **prohibition**. Coq `negb equivalence_claimed = true` proves the flag is false.

**PART 1 PASS.**

---

## PART 2 — Dual-asset architecture

Public descriptions agree:

| Property | Where it is stated |
|----------|-------------------|
| Native UTXO assets | Whitepaper Ch.2–3; Minimal Script §4; registry; root README |
| Same protocol layer | “Both exist at the same protocol layer”; no parent-child token |
| Not colored coins / inscriptions / Ordinals / Runes | Whitepaper Ch.3; website whitepaper + Minimal Script |
| Not wrapped assets / smart-contract tokens | Same (wrapped assets included after prior script audit) |
| Class from `scriptPubKey` | README, Protocol page, whitepaper Ch.3 |

Exact slogan **“native dual-asset UTXO architecture”** is not a locked catchphrase. Canonical wording is:

> Bitcoin-derived UTXO architecture, a native dual-asset system, a restricted script environment, and a deterministic validation system.

**Semantic PASS.** Slogan uniformity: residual low.

---

## PART 3 — Minimal script architecture

| Required | Status |
|----------|--------|
| Constrained scripts | Stated |
| Does **not** have a general-purpose VM or arbitrary contracts | Stated on Ch.3, security note, website |
| Witness v0: ORU spend | Registry + Minimal Script table |
| Witness v1: RES key-path | Registry + Minimal Script table |
| Witness v1: CVG2 Convergence commitments (ORU-domain, unspendable) | Ch.3 / website; Ch.2 tables still compress ORU as “Witness v0” only |
| No document says “RIGOR has zero scripts” | Confirmed |
| “RIGOR minimizes script complexity rather than eliminating scripts.” | Present on architecture surfaces |

Ch.2 property tables still label ORU script path as Witness v0 without the CVG2 v1 row. Ch.3 is the canonical correction (prior audit I-5). Not a consensus disagreement.

**PART 3 PASS.**

---

## PART 4 — Monetary policy consistency

### RFC-017 cone (aligned)

| Symbol | RFC-017 | C++ lock | Coq `ResSchedule.v` | Python spec | Website Monetary Policy | Whitepaper Ch.2 / App. B |
|--------|---------|----------|---------------------|-------------|-------------------------|--------------------------|
| `RES_COIN` | 1 000 000 | 1000000 | 1000000 | 1_000_000 | 1_000_000 | 1 RES = 1,000,000 atoms |
| `W(0)` | 40 000 000 atoms | `40 * RES_COIN` | same | same | 40.000000 | 40.000000 at genesis |
| Epoch length | 210 000 | 210000 | 210000 | 210_000 | `height / 210000` | 210,000 |
| Last paying epoch | 250 | 250 | 250 | 250 | k>250 → 0 | 250 |
| `schedule_sum` | 99 450 437 | static_assert 99450437 | `schedule_sum_value` | assert 99_450_437 | 99450437 atoms | 210000 × 99450437 |
| Cap | 21e6 RES | 21e6 × RES_COIN | same | same | 21,000,000 | 21,000,000 |
| Formula | isqrt fixed-point | same expression | `Z.sqrt` / shifts | `math.isqrt` | floor(40·RES_COIN·(k+1)^(−3/2)) | same |

Parity gate: `formal_verification/tests/test_rfc017_schedule_parity.py` (C++ formula mirror ≡ Python ≡ Coq pins for `k = 0..251`).

Website pins `{0,1,10,100,250,251}` match Coq `reward_pin_table` / the parity test dictionary.

### Residual: teaching 65536 vs shipping E=246

Shipping (code + Coq + RFC-017 + whitepaper **abstract** + Appendix B + ProtocolCard):

- `E = 246` ⇒ expected wins per committed ORU ≈ `n / 1024`
- Mint cost `C = 1024` ORU (`mint_cost_is_1024_oru`)

Still written as if live, or without the abstract’s caveat:

| Location | Issue |
|----------|--------|
| Bound paper §5.1 | “published aggregate identity is `1/65536`” |
| Website whitepaper §5 | Same identities |
| Landing “65536 principle” | `P(success) = 1/65536` unlabeled |
| Technical structure §6.1 | Heading still `1 / 65536` |
| Root `README.md` | RFC-014 formula with `2^240` |
| Python spec comment (~line 730) | Comment says production `2^240`; **code** uses exponent 246 |

Abstract + Appendix B of the bound paper already distinguish teaching vs shipping. Ch.5 body was not fully rewritten after RFC-017.

**Monetary numbers PASS. Probability-identity copy RESIDUAL (docs only).**

---

## PART 5 — Formal verification claim boundary

Allowed public sentence is present on Ch.8, `verification/README.md`, Formal Verification page, Documentation library card, and the public claim table.

Not found as assertions:

- Entire blockchain formally proven  
- All bugs impossible  
- Complete implementation equivalence  

Outside-boundary lists (mining, difficulty, wallet, P2P, complete C++ equivalence) match the coverage audit and Formal Verification page.

**PART 5 PASS.**

---

## PART 6 — Verification architecture

Public entry points:

| Required | Path | Exists |
|----------|------|--------|
| Public map | `verification/README.md` | Yes |
| How to reproduce | `docs/security/HOW_TO_VERIFY_RIGOR.md` | Yes |
| Claim traceability | `docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md` | Yes |
| Golden vectors | `verification/golden-vectors/README.md` → `phase3_golden_vectors.json` / `golden_lock.json` | Yes |
| Differential tests | `verification/differential-tests/README.md` → `run_differential_gate.sh` | Yes |
| Coq map | `verification/coq/{README,protocol-core,monetary-policy,convergence}/README.md` | Yes |
| Python spec map | `verification/python-spec/README.md` | Yes |

Canonical machines remain under `formal_verification/` (no duplicated `.v` / JSON in `verification/`).

Relative links checked from `verification/README.md` and HOW_TO_VERIFY (including `docs/verification/FV-RELEASE-CHECKLIST.md`, STATUS, claim table, assumption boundary): **in-tree targets exist**.

Living lock cited as **422 / 215** on verification README, Formal Verification page, bound Ch.8, `golden_lock.json`. Historical 324/117 and RC0 414/207 are labeled snapshots.

**PART 6 PASS.**

---

## PART 7 — Whitepaper final traceability

Created: [RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md](./RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md).

Columns: Whitepaper claim · Implementation location · Formal artifact · Verification status.

Rows cover script architecture, dual assets, RES monetary policy, Convergence consensus, and formal verification.

---

## PART 8 — Website final review

Full page checklist: [RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md](./RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md).

Five documentation pages exist, are routed, and agree on dual assets, constrained scripts, RFC-017 pins, and FV boundaries.

Residuals: Monetary Policy missing from `sitemap.ts` Documentation children (library card + route exist); 65536 on landing / condensed Ch.5; GitHub `blob/main` may trail local docs.

---

## What this audit did not do

- No consensus, wallet, miner, or RFC file edits  
- No change to Coq, Python spec, or golden vectors  
- No website source edits  
- No push / merge / tag  

Unrelated dirty files in the working tree (`core/src/Makefile.test.include`, generated `bitcoin-config.h`, fuzz sources, other security-audit edits) were **not** included in this task’s commit.

---

## Recommended follow-ups (documentation only)

1. Relabel landing “65536 principle” as historical teaching identity; point at `E = 246` / `C = 1024`.  
2. Restate that caveat at the start of whitepaper §5.1 (bound + site).  
3. Update root README RFC-014 line to shipping `E = 246` (windowed `T`).  
4. Add Monetary Policy to `sitemap.ts` Documentation children.  
5. Optionally add a CVG2 row to Ch.2 ORU script-path tables so they match Ch.3.

None of these is required to keep `equivalence_claimed = false` or RFC-017 arithmetic aligned.
