# RIGOR Public Whitepaper

**Task:** `TASK-RELEASE-004`  
**Version:** Public (verification-phase)  
**Date:** 2026-06-13  
**Type:** Documentation only — narrative layer; does not modify protocol or code  
**Parent:** `UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md`

| Field | Value |
|-------|-------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| `equivalence_claimed` | **false** |

---

## Abstract

RIGOR is a **dual-asset cryptocurrency system** whose native assets are **ORU** and **RES**. It implements a **deterministic execution protocol** over a UTXO-anchored ledger and operates as a **runtime-verifiable economic state machine**: protocol state advances through observable transitions—mining (OpuHash proof-of-work), block acceptance, convergence (selection over candidate set F), and RES mint (emission under frozen rules). Economic reality is not simulated; it is **executed on live infrastructure** and recorded in a **runtime archive** that serves as the primary truth source for milestone verification (PROOF-034).

During the current verification phase, `equivalence_claimed` remains **false**. Closed testnet is **GO**; public testnet is **CONDITIONAL GO**; mainnet is **NO-GO**.

---

## 1. Introduction

### 1.1 Canonical identity

RIGOR is a two-asset UTXO protocol with a closed script-compliance surface.

It combines ORU and RES native assets, deterministic reserve issuance,
deflationary transaction economics, Ed25519 authentication, OpuHash adaptive
Proof-of-Work, and UTXO-local validation — without a token registry, account
ledger, contract storage, or global asset state.

The formal system verifies protocol models and finite vectors. The production
C++ implementation is not claimed to be formally equivalent
(`equivalence_claimed = false`).

RIGOR must also be understood as three inseparable properties:

1. **Dual-asset cryptocurrency** — ORU and RES are native protocol assets, derived from `scriptPubKey` parsing under frozen consensus rules.
2. **Deterministic execution protocol** — Validating nodes apply a unique transition function given frozen rules and an accepted block sequence.
3. **Runtime-verifiable economic state machine** — State at milestone heights can be persisted, replayed, and checked against golden vector fixtures.

### 1.2 What this document covers

This whitepaper presents the **four-layer public architecture** (Protocol, System, Verification, Laboratory). It is the authoritative public document for full-stack narrative. Surface-specific distillations appear in the website framework and GitHub positioning guides.

### 1.3 Experimental scope

While PROOF-034 executes (mining a regtest archive to height ≥ 2 100 001), all live observations are **preliminary runtime evidence**. Completion of formal proof catalogs (PROOF-001–033) does not substitute for incomplete archive materialization.

---

## 2. Layer 1 — Protocol Layer

### 2.1 Consensus and storage model

RIGOR stores value in standard UTXO outputs `(nValue, scriptPubKey)`. Asset *class* is **inferred from script**, not stored in a parallel field. The locking script is the **compliance/classification layer**: asset class is inferred from `scriptPubKey`. Asset ownership and transfer are enforced through UTXO consumption, script validation, signature verification, and consensus predicates. Script alone does not authorize asset creation. RES is deterministically issued after protocol-defined mint authorization — it is **not mined**.

### 2.2 Native assets: ORU and RES

| Asset | Script path | Role |
|-------|-------------|------|
| **ORU** | Witness v0 | Proof-of-work security and economic coordination asset (subsidy, fees, RES participation cost) |
| **RES** | Witness v1 | Independent scarce resource; allocated by deterministic convergence draw; **not mined** |

Both are **native** to the protocol—not external tokens, not account balances. PoW does not create RES; PoW provides irreversible ordering for fair RES allocation.

### 2.3 Mining (OpuHash)

**Mining exists** as the proof-of-work layer that discovers valid blocks. OpuHash is the consensus PoW function. **CPU mining** is a valid participation path via `rigor-miner-agent` and Stratum job distribution.

Mining supplies **state transition inputs** (valid blocks) and secures ordering. It does not mint RES and does not define convergence or mint semantics.

### 2.4 Convergence (selection system)

**Convergence exists** as a deterministic selection system over candidate transaction set F at protocol boundaries:

- Candidate set cardinality |F| and convergence root are observable.
- **Sigma (σ)** is the cryptographic selection witness binding the winner to F.
- Winner transaction identification is queryable via convergence RPC surfaces.

### 2.5 RES mint (emission system)

**Mint exists** as the RES emission path when protocol predicates are satisfied:

- Mint transactions create new RES-class UTXOs.
- **PCORB** is the structured verification vector in the mint body.
- Emission is bounded by frozen cap rules (milestone at h = 2 100 000).

### 2.6 Draw boundaries

At height-gated boundaries (including h = 2 100 001), draw accept/reject rules apply. These are protocol transitions, not marketing categories.

---

## 3. Layer 2 — System Layer

### 3.1 Deterministic state transitions

Given frozen rules R, validating nodes apply transition function T:

```text
S(t+1) = T(S(t), block_events)
```

State S(t) comprises:

- UTXO set U(t) with ORU and RES subsets under script interpretation
- Convergence observables C(t)
- Mint observables M(t)
- Draw observables D(t)

### 3.2 Execution pipeline

```text
OpuHash mining → Stratum pool → rigord validation → Convergence engine → Mint engine → Archive
```

| Stage | Determinism |
|-------|-------------|
| Mining discovery | Probabilistic wall-clock |
| Block validation, convergence, mint | **Deterministic** given accepted block |

### 3.3 Distributed execution

RIGOR runs as a network of validating nodes, pool services, and miners. Each node independently applies T; observability surfaces read authoritative RPC fields. The system is **live**, not a offline simulation.

### 3.4 Runtime archive as ground truth

The **runtime archive** (PROOF-034 regtest datadir) is the **primary truth source** for materialization claims at catalogued heights. It persists execution trace τ—the sequence of states and transitions actually executed.

---

## 4. Layer 3 — Verification Layer

### 4.1 PROOF-034 — Full runtime archive verification

PROOF-034 mines a single contiguous regtest chain to h ≥ 2 100 001 and materializes twelve golden vector fixture families.

| Milestone height | Phenomenon |
|------------------|------------|
| 630 000 | ORU halving vector |
| 2 100 000 | RES cap vector |
| 2 100 001 | DRAW reject vector |

**Acceptance:** Harness summary 9 PASS + 3 PARTIAL + 0 FAIL + 0 NOT_EXECUTED; `verification_manifest.json` published; `equivalence_claimed = false`.

### 4.2 Golden vectors

Golden vectors are catalogued input–output specifications at milestone states. They enable independent checking without relying on operator narrative.

### 4.3 Runtime replay system

`golden_vector_verify.sh --require-full` replays archive evidence against fixtures. **Correctness** means harness PASS on τ—not anecdotal observation.

### 4.4 Relation to formal proofs

Formal proof layer (PROOF-001–033, Coq catalogs) establishes logical properties of the frozen model. Runtime verification establishes **what executed** on archive A at height h. Neither automatically implies equivalence between layers.

---

## 5. Layer 4 — Laboratory Layer

### 5.1 Experimental interpretation

During PROOF-034, RIGOR is operated under an explicit experimental regime:

- Rules frozen mid-archive
- Observers may show partial data
- Telemetry supplements but does not override archive

### 5.2 Research framing

Three evidence types coexist:

| Evidence | Source |
|----------|--------|
| Formal | Coq, PROOF-xxx, specifications |
| Runtime | Archive, harness, fixtures |
| Observational | Explorer, GUI prototype, JSONL |

Presenters must name which evidence grade they display (see `RIGOR_SCIENTIFIC_FRAMEWORK.md` §7).

### 5.3 Observability systems

Read-only surfaces:

- Explorer indexer (`/api/status`, block and RES mint APIs)
- GUI prototype (Dashboard, Convergence, RES Mint, Inspector)
- Telemetry JSONL (operator diagnostics)

Observers do not submit blocks, sign transactions, or claim equivalence.

---

## 6. Public Truth Model

| Concept | Definition |
|---------|------------|
| **Truth** | Runtime execution trace τ in the archive |
| **Correctness** | Replay + harness verification against golden vectors |
| **System state** | Observable via RPC and deterministic across validating nodes |

RIGOR is:

- **Live** — production-executable stack
- **Verifiable** — harness-independent checking
- **Not theoretical-only** — formal proofs complement live execution evidence

---

## 7. Release Status and Readiness

Aligned with TASK-RELEASE-003 (`PROJECT_FREEZE_ROADMAP.md`):

| Tier | Verdict | Public implication |
|------|---------|-------------------|
| Closed testnet | **GO** | Operator mesh, mining, convergence RPC operational |
| Public testnet | **CONDITIONAL GO** | Pilot/education only; open gates remain |
| Mainnet | **NO-GO** | No launch, treasury, or exchange-ready claims |

Overall readiness (gate-weighted): **72%** as of 2026-06-11. PROOF-034 completion expected ~80%; mainnet blockers (Guix, treasury, HSM, soak) remain.

**Narrative rule:** Never imply mainnet readiness from archive progress alone.

---

## 8. Prohibited Public Claims

The following are **forbidden** in public materials derived from this whitepaper:

| Category | Examples |
|----------|----------|
| Equivalence | “Certified equivalent,” “equivalence claimed,” “proven identical implementation” |
| Simulation-only | “Paper protocol only,” “no live network” |
| Economic erasure | Describing RIGOR without ORU/RES as native assets |
| Overclaim | Mainnet GO, investment yield, guaranteed value |
| External comparison | Comparative claims naming other systems |

---

## 9. Conclusion

RIGOR is a **dual-asset cryptocurrency system (ORU and RES)** with **live mining, convergence, and mint subsystems**, verified through **runtime archive evidence** as the primary truth source. Public communication must remain **scientific, deterministic, and non-speculative** while `equivalence_claimed = false` and mainnet gates remain open.

---

## References

- [RIGOR_UNIFIED_WHITEPAPER_v1.0.md](./WHITEPAPER.md) — official publication core (TASK-PUBLICATION-001)
- `AXIOMS_v1.0.md` — formal axiom system (TASK-WHITEPAPER-AXIOM-001)
- `UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md`
- `RELEASE_LANGUAGE_STYLE_GUIDE.md`
- `RIGOR_LAB_MANIFESTO.md`
- `ACADEMIC_SYSTEM_DESCRIPTION.md`
- `ENGINEERING_SYSTEM_DESCRIPTION.md`
- [PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md](./protocol/proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md)
- `FINAL_RELEASE_GATE_MATRIX.md`
