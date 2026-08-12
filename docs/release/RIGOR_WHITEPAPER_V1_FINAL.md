# RIGOR Whitepaper v1.0 Final

**Task:** `TASK-RELEASE-008` (Official Public Whitepaper — v1.0 FINAL)  
**Version:** 1.0 FINAL (public)  
**Date:** 2026-06-20  
**Type:** Documentation only — narrative layer; does not modify protocol or code  
**Supersedes for public launch:** [WHITEPAPER_PUBLIC_VERSION.md](./WHITEPAPER_PUBLIC_VERSION.md) (verification-phase edition)  
**Parent:** [UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md](./UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md)

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

---

## Abstract

RIGOR is a **deterministic dual-asset cryptocurrency system**. Native assets **ORU** and **RES** evolve under a **frozen UTXO protocol** with **CVG2 convergence** (ORU destruction linked to RES eligibility), **deterministic draw** at constant `T = 2^240`, and **ConnectBlock** execution without global mutable consensus state.

The system is **runtime-verifiable**: economic transitions execute on live infrastructure and are recorded in a **PROOF-034 runtime archive** target at height **2,100,001**. A **SIM-011 simulation layer** provides deterministic replay for structural consistency analysis — **not** domain equivalence.

**Certification status (baseline):** Closed testnet **GO**; public testnet **CONDITIONAL GO**; mainnet **NO-GO** pending PROOF-034. **`equivalence_claimed = false`.**

---

## 1. Introduction

### 1.1 What RIGOR is

RIGOR combines four inseparable properties:

1. **Deterministic cryptocurrency** — validating nodes apply a unique state transition given frozen rules.
2. **Dual native assets** — ORU (Witness v0) and RES (Witness v1 RNG path).
3. **CVG2 convergence economy** — ORU is destroyed via convergence transactions; eligibility feeds RES mint path under frozen rules.
4. **Runtime verifiability** — milestone states checkable against golden vectors in archive evidence.

### 1.2 What RIGOR is not

| Not | Reason |
|-----|--------|
| Equivalence-certified vs formal proof | `equivalence_claimed = false` |
| Simulation-only model | Live mining, pool, nodes operate |
| Account-based ledger | UTXO + script-as-asset only |
| Mainnet-ready (today) | PROOF-034 incomplete @ 2.1M+1 |

### 1.3 Narrative mapping (public layer)

**Current product terminology:** **convergence transaction**, **RES minted**, **deterministic consensus allocation**.  
Legacy “burn” / “forge” wording in older materials is historical narrative only — not current product copy. Frozen specification uses CVG2 / convergence exclusively. Mapping is not protocol equivalence to other systems.

---

## 2. Protocol Layer

### 2.1 Storage and assets

- **UTXO model:** `(nValue, scriptPubKey)` only — no parallel `asset` field.
- **ORU:** Provides PoW security and economic coordination; subsidy schedule → floor **125 ORU** per block.
- **RES:** Independent scarce resource; cap **21,000,000 RES**; **RES minted** **0 or 1** per block when predicates pass (deterministic consensus allocation).

### 2.2 Convergence transaction

Convergence transactions commit ORU via witness v1 CVG2 containers. Outputs are **excluded from UTXO** — commitment under consensus rules, not a transferable spend.

```
S_ORU(h+1) = S_ORU(h) + R(h) − C(h)
```

where `C(h)` is the converge sum of ORU committed by convergence transactions in block `h`.

### 2.3 Deterministic consensus allocation

```
T = RES_PROBABILITY_TARGET   # RFC-014 v1.2 (windowed); not the vacuous literal 2^240
Eligible ⟺ σ < T · weight
```

Structural density follows frozen economics — deterministic inequality, not runtime RNG.

Winner: total order on `(σ, weight, txid)`.

### 2.4 Mining (OpuHash)

Proof-of-work discovers valid blocks and provides ordering. Mining **does not** mint RES and **does not** define draw constant `T` or convergence semantics.

### 2.5 Freeze baseline

**`mainnet-freeze-rfc015-2026-05-26`** — RFC-000 through RFC-015 locked.

---

## 3. Scientific Layer

### 3.1 State machine

```
S(t+1) = F(S(t), B(t), σ(t))
```

State vector: UTXO sets, convergence context, mint observables, height.

### 3.2 Mathematical stack (MATH-001~005)

| Layer | Role |
|-------|------|
| MATH-001 | Axiom system · CVG2 · replay invariants |
| MATH-002 | Measure theory · ORU/RES distributions |
| MATH-003 | Cross-domain mapping · structural scores |
| MATH-004 | Convergence certificate |
| MATH-005 | Mainnet readiness function |

Math documents **structure** — they do not replace runtime consensus.

### 3.3 Domain separation

```
Formal proof  ≠  Runtime execution  ≠  Simulation replay
equivalence_claimed = false
```

---

## 4. Runtime Layer

### 4.1 Execution pipeline

```text
OpuHash mining → Stratum pool → rigord validation
    → Convergence draw → RES mint (PCORB) → Archive / RPC
```

### 4.2 Components

| Component | Role |
|-----------|------|
| **Mining** | `rigor-miner-agent` CPU path |
| **Pool** | Stratum · winner engine · mint engine · convergence root |
| **Explorer** | Read-only indexing and API |
| **Wallet** | Observer MVP · Qt prototype (non-custodial hot path excluded from public tree) |
| **Telemetry** | JSONL four-stream model (MINING-024/025) |

### 4.3 Ground truth

**Runtime archive** (PROOF-034) is primary evidence for milestone materialization — not GUI display alone.

---

## 5. Simulation Layer

### 5.1 SIM-011 orchestrator

Coordinates SIM-004→012: replay kernel, state machine, proof validator hooks, determinism lock.

### 5.2 Purpose

- Verify **replay determinism** (identical hashes under identical inputs)
- Compute **structural alignment** with archive fixtures
- Support CI and GUI replay viewer

### 5.3 Non-purpose

Simulation **does not**:
- Write chain state
- Claim equivalence to runtime
- Replace PROOF-034 archive

---

## 6. Lab & Verification Layer

### 6.1 PROOF-034

Mines regtest chain to **h ≥ 2,100,001**; materializes **12 golden vectors**.

| Milestone | Height |
|-----------|--------|
| GV-ORU-0003 | 630,000 |
| GV-RES-0003 | 2,100,000 |
| GV-DRAW-0003 | 2,100,001 |

Expected harness: **9 PASS + 3 PARTIAL + 0 FAIL**.

### 6.2 Evidence grades

| Grade | Source |
|-------|--------|
| Formal | Coq / PROOF-001~033 documentation |
| Runtime | Archive + harness |
| Observational | Explorer · GUI · JSONL |
| Simulation | Replay reports |

Presenters must name grade used.

---

## 7. Release & Certification Layer

### 7.1 Governance pipeline

RELEASE-001 (audit) → RELEASE-002 (gate matrix) → RELEASE-003 (execution) → RELEASE-004 (versioning) → RELEASE-005 (certification engine) → RELEASE-006 (sovereign declaration) → RELEASE-007 (release package) → **RELEASE-008 (this public layer)**.

### 7.2 Certification function

```
Cert(𝓢) → GO | CONDITIONAL_GO | NO_GO
Score_total ∈ [0,1]
```

Mainnet **GO** requires `Score_total ≥ 0.93`, `proof_valid = true`, M-* PASS.

### 7.3 Tier table (baseline)

| Tier | Status |
|------|--------|
| Closed testnet | **GO** |
| Public testnet | **CONDITIONAL GO** |
| Mainnet | **NO-GO** (pending PROOF-034) |

---

## 8. Public Truth Model

| Concept | Definition |
|---------|------------|
| **Execution truth** | Valid blocks accepted by consensus |
| **Archive evidence** | PROOF-034 persisted trace @ milestones |
| **Correctness check** | Harness replay vs golden vectors |
| **Structural consistency** | Alignment scores in `𝓥` — not equivalence |

---

## 9. Prohibited Public Claims

Forbidden until RELEASE-006 declares mainnet **GO** with full gates:

- “Mainnet live” · “Production certified” · “Fully verified”
- “Equivalence confirmed” · `equivalence_claimed = true`
- Investment yield · guaranteed value · comparative superiority claims

See [RELEASE_LANGUAGE_STYLE_GUIDE.md](./RELEASE_LANGUAGE_STYLE_GUIDE.md).

---

## 10. Conclusion

RIGOR v1.0 FINAL presents a **unified public account** of a deterministic dual-asset system under active runtime verification. Public communication remains **transparent, non-equivalent, and tier-accurate** until PROOF-034 completes and certification updates.

```
equivalence_claimed = false
```

---

## References

- [RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md](./RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md)
- [RIGOR_DISCLOSURE_AND_LIMITATIONS_STATEMENT.md](./RIGOR_DISCLOSURE_AND_LIMITATIONS_STATEMENT.md)
- [RIGOR_PUBLIC_SYSTEM_POSITIONING.md](./RIGOR_PUBLIC_SYSTEM_POSITIONING.md)
- [PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md](../proof/PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md)
- [FINAL_RELEASE_GATE_MATRIX.md](./FINAL_RELEASE_GATE_MATRIX.md)
- [RIGOR_MAINNET_FINAL_DECLARATION.md](./RIGOR_MAINNET_FINAL_DECLARATION.md)

---

*End of RIGOR Whitepaper v1.0 Final — TASK-RELEASE-008*
