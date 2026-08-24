# RIGOR Documentation Full Parameter Consistency Audit

**Task:** `TASK-RIGOR-DOCUMENTATION-FULL-PARAMETER-CONSISTENCY-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Type:** Audit only — **no file replacements**, no consensus / wallet / miner / RFC / protocol-behavior changes.  
**Honesty:** `equivalence_claimed = false`

Follow-up to [RIGOR_DOCUMENTATION_SHIPPING_POLISH_REPORT.md](./RIGOR_DOCUMENTATION_SHIPPING_POLISH_REPORT.md) (public-surface polish) and [RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md](./RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md).

Shipping lock (RFC-017 / `params.h` / `ResSchedule.v` / Python SSOT):

| Parameter | Shipping value |
|-----------|----------------|
| `E` | **246** |
| Mint cost `C` | **1024 ORU per RES mint** |
| Aggregate expected wins | `n / 1024` |
| `schedule_sum` | **99450437** atoms |
| Epoch | **210000** blocks |
| Cutoff | **250** (`W(k)=0` for `k>250`) |
| `RES_MAX_SUPPLY` | **21,000,000 RES** (`21_000_000_000_000` atoms at `RES_COIN=1_000_000`) |
| Genesis `W(0)` | **40.000000 RES** |

Category **B** requires the exact phrase **Historical RFC-014 parameter** when retaining `2^240` / `1/65536` / `E=240` in a document that a reader might take as live.

---

## 1. Search scope

Terms: `65536`, `1/65536`, `2^240`, `E = 240`, `E=240`, `RES_PROBABILITY_TARGET`, `RES_PROBABILITY_TARGET_UNWINDOWED`, `RES mint cost`, `ORU cost per RES`, `RES_REWARD_AMOUNT`, `RES_REWARD_GENESIS`, `RES schedule`, `RFC-014`, `RFC-017`.

Surfaces: `README.md`, `docs/**`, whitepapers, `web/rigorchain-site/**`, explorer/wallet/release notes, security reports, markdown, code comments, examples. Coq/C++ **identifiers** for `RES_PROBABILITY_TARGET` are listed only when comments or nearby prose assert a numeric exponent.

Claim slogans (PART 5): `fully proven`, `zero risk`, `complete equivalence`, `no scripts`, `scriptless`.

This audit does **not** treat RFC files as Category D. RFCs are frozen historical/normative artifacts (Category C). Wrong **current** descriptions outside `docs/rfc/` are Category D.

---

## 2. Classification summary

| Cat | Meaning | Count (approx.) | Residual risk |
|-----|---------|-----------------|---------------|
| **A** | Current shipping description, matches `E=246` / `C=1024` (and RFC-017 schedule where applicable) | Public README, bound whitepaper, live website pages, SPEC-000/001/003/005 **T rows**, Coq/Python **code** | Low |
| **B** | Historical, with required label (or document-level banner) | Bound Ch.5.1, Appendix E, landing, Science, Lab, ConvergencePanel, V1 banners, `chapters/05` banner | Low if banner is read |
| **C** | Derivation / audit / RFC evolution | RFC-014/017, Selection.v, SECURITY-001-A, prior audits, `pre_a6_*` | None as protocol claims |
| **D** | Incorrect **current** documentation | Freeze SSOT, SPEC mint tables, architecture, explorer, release, RPC help, some inventories | **Medium–High** (operator / verifier confusion) |

---

## 3. Category A — current shipping (correct)

Representative, not every identifier hit:

| Surface | What it states |
|---------|----------------|
| `README.md` | `E = 246`, `C = 1024` ORU per mint |
| Bound whitepaper abstract, §5.1 shipping block, App. B | `E=246`, `C=1024`, `n/1024`; schedule `210000 × 99450437`, cutoff 250, cap 21M |
| Technical structure §6.1 shipping paragraph | Same |
| `chapters/04-convergence.md` shipping identities | Same |
| Website: Landing, Whitepaper, Science, Lab, ConvergencePanel, ProtocolCard | `E=246`, `C=1024` |
| Monetary Policy page | `W(k)`, epoch 210000, cutoff 250, sum pin 99450437, cap 21M |
| Formal Verification page | Allowed FV sentence; no stale `T` |
| SPEC-000/001/003/005 **probability-target rows** | RFC-017 `E=246` + labeled historical `2^240` |
| `Selection.v` / Python `RES_PROBABILITY_TARGET_EXPONENT = 246` | Machine SSOT |
| `rigor_consensus_lock.h` | `RES_REWARD_GENESIS`, epoch 210000, last epoch 250, `ResScheduleSum()==99450437` |

---

## 4. Category B — historical, labeled

Exact required phrase **Historical RFC-014 parameter** present on:

| Document | Notes |
|----------|--------|
| `README.md` | `E=240` / `C=65536` labeled, not shipping |
| Bound whitepaper abstract, §5.1, Appendix E | Teaching slice vs shipping |
| `RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md` §6.1 | Historical block after shipping |
| `chapters/04-convergence.md` | Historical identity block |
| `chapters/05-mathematical-design.md` | **Banner** at top; **body still writes 1/65536 as published aggregate** (see D-lite) |
| V1 cover + V1 structure headers | Banner + shipping abstract |
| Landing, WhitepaperPage, Science, ConvergencePanel | Phrase present |
| SPEC-000/001/003/005 T rows | Phrase present |
| `PROTOCOL-NAMING-TRUTH-SOURCE.md` | Phrase present |
| `UI_LAYOUT_SPEC.md` INV-FAIR | Phrase present |

**B-weak (historical but weaker wording, not the exact phrase):** RFC-017 “Was `2^240`”; freeze “SUPERSEDED”; SECURITY-001-A “Before `2^240`”; Coq “corrected from `2^240`”; prior consistency audits describing old landing copy. Treat as **C** (evolution record) unless the file also claims to be live operator SSOT.

---

## 5. Category C — mathematical derivation / audit / RFC record

| Class | Paths (examples) |
|-------|------------------|
| RFC corpus | `docs/rfc/RFC-014-*.md`, RFC-001/008/012/015 `T=2^240`; RFC-017 comparison `E=240` vs `E=246` |
| Coq/Python evolution comments | `Selection.v` (`C=65536` at `E=240`, `C=1024` at `E=246`); `pre_a6_target_was_vacuous`; `KernelEligibility.v`; Python header A-6 |
| Security incident reports | `SECURITY-001-A-FINAL-REPORT.md`, A-8, `params.h` comment block on vacuity |
| Prior audits of *then-current* copy | `RIGOR_CLAIM_WORDING_AUDIT.md`, `RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md`, `RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md` (residuals **since fixed** on public pages) |
| Whitepaper design/consistency audits | `ALPHAPLUS_*`, `WHITEPAPER_CONTENT_AUDIT_REPORT.md`, `SOURCE_CORRECTIONS.md`, `AUDIT_REPORT.md` |
| Unrelated `65536` | `ResSchedule.v` `2^16` fixed-point divisor; explorer tests `65536.00000000` ORU amounts |

RFC-017 is the **shipping** monetary RFC (Category A for `W(k)` / `C=1024`). RFC-014 remains the **draw-target architecture** RFC; its printed `2^240` literals are Category C, superseded in implementation by A-6, A-9-C, then RFC-017 `E=246`.

---

## 6. Category D — incorrect current documentation (must be fixed later)

These present **live** protocol parameters that do **not** match shipping. This task does not edit them.

### D1 — High: freeze / architecture / SPEC mint (operators treat as SSOT)

| File | Found | Expected | Issue |
|------|-------|----------|--------|
| `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` §3.1 | `UNWINDOWED := floor(2^240 / MIN)`; `p_draw = n/(65536·32)`; expected wins `n/65536` | `E=246`, `C=1024`, `n/1024` | Freeze text still RFC-014 v1.2 **E=240** economics |
| Same, § economics | `RES_REWARD_AMOUNT = 100 RES` (10,000 atoms; `100 * RES_COIN`) | `W(h)`; genesis 40 RES; `RES_COIN=1e6` | Pre-RFC-017 mint + old atom scale |
| `docs/specification/SPEC-000` freeze table | `RES per mint \| 100 RES (RFC-015)` | `W(h)` / RFC-017 | **T row is A; mint row is D** |
| `docs/specification/SPEC-001` §3.7 | `RES mint amount \| 100 RES` | `W(h)` | Same split |
| `docs/specification/SPEC-002-ECONOMIC-MODEL.md` | `RES_REWARD_AMOUNT = 100.00 RES` (= 10,000 atoms); `RES_MAX_SUPPLY` atoms `2,100,000,000` | Schedule `W(h)`; cap **21e12** atoms | Occam freeze table never migrated |
| `docs/specification/SPEC-003` / `SPEC-005` / `SPEC-006` | Recurrence `S_RES += RES_REWARD_AMOUNT` | `GetResMintReward(h)` | Constant-mint language |
| `docs/architecture/RIGOR_DETERMINISM_MAP.md` | `T=2^240` in draw signature and checklist | `E=246` windowed `T` | Live architecture map |
| `docs/architecture/RIGOR_EXECUTION_MODEL.md` | `T = 2^240` | Shipping `T` | Same |
| `docs/architecture/RIGOR_GLOBAL_ARCHITECTURE_REPORT.md` | `T=2^240`; mint 100.00 RES | RFC-017 | Same |

### D2 — Medium: explorer, wallet, Qt, RPC help, GO checklist

| File | Found | Expected |
|------|-------|----------|
| `docs/explorer/RES_INDEXING_RULES.md` | `T = 2^240 (constant)` | Shipping windowed `T` at `E=246` |
| `docs/explorer/EXPLORER_RPC_COMPATIBILITY_MATRIX.md` | verify hex `00010000…` (`2^240`) | Must not equal unwindowed `2^240`; live hex is **not** that pattern |
| `docs/explorer/WITNESS_V1_DISPLAY_RULES.md` | inequality RHS `(2^240)` | `E=246` |
| `docs/wallet/RIGOR_RPC_REQUIREMENTS.md` | `res_probability_target == 2^240 hex` | Fail closed against shipping `T` |
| `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md` | verify == `2^240` hex | Same |
| `docs/release/MAINNET_GO_NO_GO_CHECKLIST.md` C-01 | live T = `floor(2^240/MIN)>>5` | `floor(2^246/MIN)>>5` |
| `core/src/rpc/blockchain.cpp` RPCResult strings | `= floor(2^240 / MIN_CONVERGENCE_AMOUNT)` | User-visible help still A-6 formula, not `E=246` |
| `core/src/script/interpreter.h` comment | same `2^240` formula as current source | Comment lag vs `params.h` |

### D3 — Medium: release / product copy still advertising `T=2^240` or 100 RES

| File | Found |
|------|--------|
| `docs/release/RIGOR_PUBLIC_SYSTEM_POSITIONING.md` | `T=2^240` |
| `docs/release/RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md` | `T = 2^240` |
| `docs/release/RIGOR_MAINNET_RELEASE_PACKAGE.md` | `T=2^240` draw system |
| `docs/release/RIGOR_WHITEPAPER_V1_FINAL.md` abstract | `T = 2^240` (later line correctly says not vacuous literal — **mixed**) |
| `contrib/alpha/README.md` | Observe 100.00 RES minted |
| `docs/mining/RES_BODY_MINT_OPERATIONAL_SPEC.md` | 100.00 RES on body mint |
| `docs/security/RES-ORU-MATURITY-INVENTORY.md` | `RES_REWARD_AMOUNT` 100.00 RES / 10 000 atoms / stale line numbers |
| `docs/wallet/CONVERGENCE_*` / `WALLET_KEY_RECOVERY_*` | 100.00 RES user copy |
| `pool/share-validator/**` | 100.00 RES mint check docs |

### D4 — Low: prior-draft body after a B banner

| File | Found | Note |
|------|--------|------|
| `docs/whitepaper/chapters/05-mathematical-design.md` body | “published aggregate is 1/65536” | Banner is B; **body is D if read in isolation** |
| `RIGOR_PROTOCOL_WHITEPAPER_V1_STRUCTURE.md` §5.1 | `p(n)=n/65536` | Header banner is B; section still teaching-as-current |

### D5 — Archive / dated product reports (100 RES as observed then)

`docs/archive/**`, many `docs/audit/LMS_*`, `docs/release/local_simulation/*`, `PROOF-002/003/004` golden vectors at 100 RES. **Category C** if clearly dated runtime evidence; **D** only if still used as current product spec. Default: **C (dated evidence)** except when the file is still linked as operator SSOT.

---

## 7. PART 3 — Whitepaper table

| Document | Section | Parameter found | Expected | Status |
|----------|---------|-----------------|----------|--------|
| `RIGOR_PROTOCOL_WHITEPAPER.md` | Abstract | `E=246`, `C=1024`; Historical RFC-014 `2^240` | Shipping + label | **A + B** |
| Same | Ch.2 RFC-017 summary | `210000 × 99450437`, cutoff 250, 21M, `W(k)` | RFC-017 | **A** |
| Same | §5.1 | Shipping `n/1024` then labeled `E=240` / `65536` | Shipping first | **A + B** |
| Same | App. B | `E=246`, `C=1024`, epoch 210000, cutoff 250, 21M | Shipping | **A** |
| Same | App. E | Shipping + Historical RFC-014 | Split | **A + B** |
| `RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md` | §6.1 | Shipping `E=246`/`C=1024`; historical `2^240` | Same | **A + B** |
| `chapters/02-two-assets.md` | RES table | `W(h)`, 21M, cutoff 250, RFC-017 formula | RFC-017 | **A** |
| `chapters/04-convergence.md` | T / hash space | Shipping `E=246`/`C=1024`; historical `2^240` | Same | **A + B** |
| `chapters/05-mathematical-design.md` | Banner vs body | Banner shipping; body `1/65536` published | Body should not assert live 65536 | **B header / D body** |
| `chapters/09-glossary.md` | `T` | Frozen `RES_PROBABILITY_TARGET` (no exponent) | Prefer `E=246` | **A incomplete** (not wrong) |
| `chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md` | Cap | 21M RES | 21M | **A** |
| V1 cover | Abstract | Shipping + labeled 65536 | Same | **A + B** |
| V1 structure | Header vs Ch.5 | Banner OK; §5.1 still 65536 identities | Same as ch.05 | **B / D body** |
| `ALPHAPLUS_*` / `SOURCE_CORRECTIONS` / content audits | Various | `1/65536`, Occam 100 RES | Audit-time snapshots | **C** |
| `docs/WHITEPAPER.md` | Prior narrative | Dual assets; no RFC-017 schedule | Labeled prior | **C** |

Cost floor `C/W(k)` in bound Ch.2 and Monetary Policy: **A** (constant `T`, decaying `W`).

---

## 8. PART 4 — Website audit

| Page | Economics | Status |
|------|-----------|--------|
| Landing | `E=246 · C=1024`; Historical RFC-014 `1/65536` | **A + B** |
| Whitepaper | Shipping identities in abstract and §5; historical labeled | **A + B** |
| Science | Shipping + Historical RFC-014 `1/65536` | **A + B** |
| Lab | `E=246`, `C=1024` | **A** |
| ConvergencePanel | Shipping + Historical RFC-014 | **A + B** |
| ProtocolCard FORGE | RFC-017 `E=246`, `C=1024` | **A** |
| Monetary Policy | RFC-017 `W(k)`, 99450437, 21M; does not restate `E` (issuance page) | **A** for schedule |
| Formal Verification | Allowed claim sentence; no stale `T` | **A** (claims) |
| Design specs (`COMPONENT_DESIGN_SYSTEM`, `FRONTEND_SPECIFICATION`, `ARCHITECTURE_OVERVIEW`, `UI_LAYOUT_SPEC`) | `E=246` / `C=1024` | **A** (+ B on UI_LAYOUT) |

**No outdated unlabeled shipping claims** on the seven live pages after the polish task.

---

## 9. PART 5 — Public claim audit

| Phrase | Current usage |
|--------|----------------|
| fully proven / fully proven blockchain | Forbidden-lists only (README, Ch.8, `verification/README.md`, HOW_TO_VERIFY, claim table) |
| zero risk | Forbidden-lists only |
| complete equivalence | Listed as **not claimed** (`equivalence_claimed = false`) |
| no scripts | “Do not claim”; audits reject the slogan |
| scriptless | **No hits** as a feature claim |

Allowed sentence present: *Formally verified protocol invariants with explicit verification boundaries.* (`verification/README.md`, bound Ch.8, Formal Verification page, Documentation library card).

Unrelated: `test_runner.py` “no scripts in the functional tests directory”; `feature_segwit.py` “no scriptsig”.

---

## 10. Required corrections (later tasks — not this audit)

Priority **P0** (wrong SSOT for operators):

1. `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` §3.1 — `E=246`, drop live `n/65536`; replace `RES_REWARD_AMOUNT = 100 RES` with RFC-017 `W(h)`.
2. `docs/specification/SPEC-002` (and SPEC-000/001 mint rows, SPEC-003/005/006 recurrences) — `W(h)`, six-decimal `RES_COIN`, cap atoms `21e12`.
3. `docs/architecture/RIGOR_{DETERMINISM_MAP,EXECUTION_MODEL,GLOBAL_ARCHITECTURE_REPORT}.md` — stop stating live `T=2^240` / 100 RES.
4. Explorer + wallet + Qt RPC verify-hex rows — must not require `2^240` compact hex.
5. `docs/release/MAINNET_GO_NO_GO_CHECKLIST.md` C-01 — `2^246` not `2^240`.
6. RPC help strings in `core/src/rpc/blockchain.cpp` and `interpreter.h` comment — documentation of live `T` (comment-only; still Category D until edited).

Priority **P1** (product copy):

7. Release announcements / positioning / mainnet package `T=2^240`.
8. Mining operational spec, maturity inventory, alpha README, pool validator docs: 100.00 RES → `W(h)`.
9. Whitepaper `chapters/05` body and V1 structure §5.1 — either keep teaching math under a per-section Historical RFC-014 heading or point to bound §5.1.

Priority **P2**:

10. Dated LMS/PROOF archive vectors (100 RES) — add “historical fixture” banners if still linked from current runbooks.

---

## 11. Risk assessment

| Risk | Level | Why |
|------|-------|-----|
| Public website / bound whitepaper / root README wrong `T` | **Low** | Polish task aligned `E=246` / labeled history |
| Operator freeze + SPEC-002 + architecture maps wrong `T` and/or 100 RES mint | **High** | Readers treating freeze/SPEC as live will expect `E=240` windowed T and 100 RES/mint |
| Explorer/wallet “verify == 2^240 hex” | **High** | Check **cannot pass** on shipping `res_probability_target` |
| RPC help `floor(2^240/MIN)` | **Medium** | User-facing; A-6 era, not RFC-017 `E=246` |
| RFC corpus still prints `2^240` | **Accepted** | RFCs are not current operator copy; RFC-017 + code are SSOT |
| Claim slogans | **Low** | Only rejected usage |
| `equivalence_claimed` | **Low** | Remains false |

**Verdict:** Public narrative surfaces (README, bound paper, live site) match RFC-017 shipping economics. **Residual Category D is concentrated in freeze/SPEC/architecture/explorer/release/RPC-help**, plus SPEC mint **100 RES** and freeze **100 RES**, which the polish task explicitly left out of scope.

No files were modified by this audit except the creation of this report.
