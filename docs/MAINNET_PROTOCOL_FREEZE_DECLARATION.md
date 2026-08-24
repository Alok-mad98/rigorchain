# MAINNET PROTOCOL FREEZE DECLARATION

**Task:** `TASK-MAINNET-FREEZE-001` (superseded economics: `TASK-RFC015-OCCAM-REBALANCE-001`)  
**Effective architecture:** RFC-014 Conv-Decouple-01 (PATH B-PRIME) **v1.2** + **RFC-015 Occam Rebalance** (final economics) + **SECURITY-001-A8 / Sigma-PoW-01** (σ seed + deferred draw) + **SECURITY-001-A9C** (W=32 window)  
**Prior audits:** `TASK-H-CONV-T-DIFF-REVIEW-001` → **A**; `H_RFC_015_OCCAM_REBALANCE_FREEZE_VERDICT.md` → **A**  
**Declaration date:** 2026-05-26 (supersedes 2026-05-25 RFC-014-only freeze)  
**A-8 amendment:** 2026-08-04 — Sigma-CR-01 `bind_commit` / `k = 10` **superseded** by Sigma-PoW-01 (see `docs/security/SECURITY_001_A8_REPORT.md`)  
**Closure alignment:** 2026-08-05 — `TASK-MAINNET-CLOSURE-001` aligns freeze §3.1 with live `params.h` / RFC-014 v1.2 / Coq `Selection.v` (removes obsolete literal `T = 2^240` display)  
**Shipping parameter alignment:** 2026-08-20 — `TASK-RIGOR-SHIPPING-PARAMETER-SYNCHRONIZATION-P0-001` aligns freeze §3.1 / §3.3 with RFC-017 shipping (`E = 246`, `C = 1024` ORU, height-dependent `W(k)`). Historical RFC-014 `E = 240` / `C = 65536` / constant `100 RES` remain historical reference only.  
**Prior baseline tag:** `mainnet-freeze-rfc014-2026-05-25` (`4437597197`)  
**Recommended release tag:** `mainnet-freeze-rfc015-2026-05-26` (economics); post-A9C trees must cite RFC-014 **v1.2** + RFC-017 + this declaration §3.1

---

## 1. Constitutional freeze statement

The RIGOR protocol layer governing **RES convergence eligibility** and **issuance thermodynamics** is hereby declared **FINAL and IMMUTABLE** for public mainnet launch, subject only to an explicit **Level-5 hard-fork constitutional process** as defined in `PROTOCOL_CHANGE_SEVERITY_TABLE.md`.

This declaration incorporates and ratifies:

| Item | State |
|------|--------|
| RFC-014 activation | **FINALIZED** (implementation validated; see `H_CONV_T_DIFF_REVIEW_001.md`) |
| `RES_PROBABILITY_TARGET` | **PERMANENTLY FROZEN** at RFC-017 shipping `E = 246` (see §3.1; **not** the vacuous literal `2^240`) |
| `CONVERGENCE_DRAW_WINDOW` | **PERMANENTLY FROZEN** at **32** (A-9C) |
| PATH B-PRIME | **PERMANENTLY ADOPTED** |
| Variable-T (`SetCompact(parent.nBits)` as inequality RHS) | **PERMANENTLY REJECTED** |
| Dual-T (PoW T ≠ RES T on inequality path) | **PERMANENTLY FORBIDDEN** |
| Activation model | **GENESIS-ONLY** (no fork height, no versionbits, no runtime bridge) |
| Sigma-PoW-01 (SECURITY-001-A8) | **ADOPTED** — σ seed is `B_{h-1}` verbatim; draw deferred one block; Sigma-CR-01 `bind_commit` / `k = 10` **superseded** |
| Cross-difficulty replay closure | **PRESERVED** via parent-header commitment inside `B_{h-1}` (not a separate `target_prev` σ input) |
| Issuance architecture | **FINALIZED** (`σ < RES_PROBABILITY_TARGET · f`, uint512 winner, txid tie-break) |
| RFC-015 Occam rebalance | **SUPERSEDED for mint amount** by RFC-017 `W(k)`; **retained** \|F\| ≥ 1 and 21M RES cap |
| RFC-017 scheduled RES reward | **FINALIZED** (`E = 246`, `C = 1024` ORU per mint, `W(k)` height-dependent, 21M cap) |
| Prior RFC-014-only economics | **SUPERSEDED** (1 RES / draw, \|F\| ≥ 2, 27,182,818 cap — pre-mainnet only) |
| Prior RFC-015 constant mint | **SUPERSEDED** (100 RES / draw — historical reference only) |
| Prior literal `T = 2^240` | **Historical RFC-014 parameter** — superseded by A-6 (v1.1 unit correction), A-9C (v1.2 `/ W`), then RFC-017 `E = 246` — see §3.1 |

---

## 2. Immutability declaration (binding)

> **Any future modification of `RES_PROBABILITY_TARGET`, RES issuance semantics, or convergence eligibility derivation constitutes a hard-fork-level constitutional violation.**

Specifically forbidden without Level-5 governance:

- Changing the bit pattern of `RES_PROBABILITY_TARGET`
- Re-sourcing inequality RHS from `nBits`, ASERT, tip, time, or any header field
- Re-introducing deprecated symbols (`T_conv`, `T_FROZEN`, `convergence_target`, variable-T aliases)
- Altering σ preimage layout (100-byte Sigma-PoW-01), witness serialization, or deferred-draw timing
- Re-introducing `bind_commit`, bind depth `k`, or `target_prev` as a σ input
- Introducing activation height, feature flags, or dual-rule transition for RES probability
- Changing `GetResMintReward` / `W(k)` / `RES_REWARD_GENESIS` / epoch length / cutoff, `CONVERGENCE_DRAW_MIN_TXS_FOR_RES`, or `RES_MAX_SUPPLY` (RFC-017 locks; RFC-015 cap and draw gate retained)

Permitted without Level-5 (see severity table): documentation clarifications, non-consensus tooling, RPC field *documentation* that does not change consensus meaning.

---

## 3. Architectural locks

### 3.1 RES probability (RFC-017 shipping — code SSOT)

**Shipping** draw exponent is **`E = 246`**. Mint cost is **`C = 1024` ORU per RES mint**.
Expected wins for `n` ORU burned: `n / 1024`.

**Historical RFC-014 parameter:** `E = 240` ⇒ `2^240 / 2^256 = 1/65536` ⇒ `C = 65536` ORU.
Literal `2^240` was the v1.0 design intent at unit weight `f=1`. Shipping that bit pattern with
`f` in satoshis made eligibility vacuous (SECURITY-001-A / A-6). A-9C then divides by the draw
window so aggregate issuance stays neutral when F spans W blocks. RFC-017 then moves `E` to 246.

**Normative (must match `core/src/consensus/params.h` and `Selection.v`):**

```
CONVERGENCE_DRAW_WINDOW           := 32
CONVERGENCE_DRAW_WINDOW_LOG2      := 5
RES_PROBABILITY_TARGET_UNWINDOWED := floor(2^246 / MIN_CONVERGENCE_AMOUNT)
                                  = floor(2^246 / 1e8)
RES_PROBABILITY_TARGET            := RES_PROBABILITY_TARGET_UNWINDOWED >> 5
                                  = floor(RES_PROBABILITY_TARGET_UNWINDOWED / 32)
```

Equivalent formulation: unwindowed target yields mint cost **`C = 1024` ORU**; windowed
per-draw probability is `p_draw(n) = n / (1024 · 32) = n / 32768`.

Pinned hex (must match `RES_PROBABILITY_TARGET_CONST` / `RES_PROBABILITY_TARGET_UNWINDOWED`):

```
UNWINDOWED: 000000000abcc77118461cefcfdc20d2b36ba7c3d3d4d3d758161697c7068f3b
WINDOWED:   000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479
```

Per-draw probability for n ORU burned: `p_draw(n) = n / (1024 · 32) = n / 32_768`.  
Expected wins per tx across the window: `W · p_draw(n) = n / 1024` (aggregate-neutral vs pre-A9C).

Identical on: mainnet, testnet, signet, regtest, devtest, slowtest (`chainparams.cpp`).

**Historical RFC-014 v1.2 hex (E = 240; not shipping):**

```
UNWINDOWED: 00000000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3c
WINDOWED:   0000000000015798ee2308c39df9fb841a566d74f87a7a9a7aeb02c2d2f8e0d1
```

**Authority order when docs disagree:** `rigor_consensus_lock.h` + `params.h` → Coq `Selection.v` →
RFC-017 → RFC-014 v1.2 (architecture) → this declaration → older freeze prose.

### 3.2 Sigma-PoW-01 (SECURITY-001-A8; supersedes Sigma-CR-01)

```
σ_i = Hash256( txid || C || B_{h-1} || LE32(height) )   // 100 bytes; B_{h-1} verbatim
```

- Slot `[64..95]` is the **parent block hash**, not `bind_commit`.
- Draw timing: height `h` pays the set committed by block `h-1`.
- `target_prev` remains a PCORB witness field checked against the parent header; it is **not** a σ input.
- Historical Sigma-CR-01 (`bind_commit = Hash256(B_{h-k} || LE32(target_prev))`, `k = 10`) is **superseded** pre-mainnet.

### 3.3 Issuance (RFC-017 — shipping mainnet economics)

Height-dependent deterministic schedule `W(k)` (must match `GetResMintReward` in `rigor_consensus_lock.h`):

```
RES_COIN                 := 1_000_000 atoms per 1.000000 RES
RES_REWARD_GENESIS       := 40.000000 RES   (W(0))
RES_REWARD_EPOCH         := 210000 blocks
RES_REWARD_LAST_EPOCH    := 250
W(k)                     := floor(RES_REWARD_GENESIS · (k+1)^(-3/2))   for 0 ≤ k ≤ 250
                         := 0                                          for k > 250
GetResMintReward(h)      := W(floor(h / 210000))
RES_MAX_SUPPLY           := 21,000,000 RES  (21_000_000_000_000 atoms)
```

- Draw gate: `|F| >= 1` (`CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1`) — retained from RFC-015
- Minimum convergence per tx: `MIN_CONVERGENCE_AMOUNT = 1 ORU` (unchanged)
- **Historical RFC-015 parameter:** constant `RES_REWARD_AMOUNT = 100 RES` per mint — superseded by `W(k)`

### 3.4 Genesis (unchanged hashes)

Six-network genesis block header hashes verified by local `rigord getblockhash 0` (STEP 6A). RFC-014 did not alter genesis bytes.

---

## 4. Final grep verification (production)

Executed 2026-05-25 against `core/src` (read-only).

| Gate | Command / criterion | Result |
|------|---------------------|--------|
| Zero production RES paths using `SetCompact(target_prev)` as T | `rg 'ConvergenceDrawInequalityHolds.*SetCompact'` + manual path audit | **PASS** — none |
| Zero `T_conv` in `core/src` | `rg 'T_conv\|T_FROZEN\|convergence_target' core/src` | **PASS** — none |
| Zero `T_FROZEN` in `core/src` | (same) | **PASS** |
| Zero live `convergence_target` symbol in `core/src` | (same) | **PASS** |
| Canonical inequality symbol | `RES_PROBABILITY_TARGET` in `params.h`, `chainparams`, `convergence_logic`, `validation`, `res_proof` | **PASS** |
| Legacy RPC names | `rigor_target` / `res_draw_target` — comments only in `rpc/blockchain.cpp` | **PASS** — no variable-T semantics |

Full transcript: `CONV_T_GREP_GATE_EVIDENCE.md` (activation audit) + table above.

---

## 5. Release engineering gate (non-constitutional)

| Item | Status |
|------|--------|
| Git baseline | Commit containing RFC-015 + freeze docs (see `RELEASE_BASELINE_MANIFEST.md` §1) |
| RFC-014 probability architecture | **FROZEN** (constant-T PATH B-PRIME; shipping exponent is RFC-017 `E = 246`) |
| RFC-017 economics | **COMMITTED** (`W(k)`, `C = 1024` ORU; RFC-015 `|F| ≥ 1` and 21M cap retained) |
| Local validation (configure, make, check) | **PASS** (operator, 2026-05-26) |
| Required before binary release | Annotated tag `mainnet-freeze-rfc015-2026-05-26` on freeze commit |

Absence of a release git tag does **not** invalidate this constitutional freeze declaration; it is a **process** requirement for `RELEASE_BASELINE_MANIFEST.md`.

---

## 6. Cross-references

| Document | Role |
|----------|------|
| `CONSENSUS_IMMUTABILITY_MATRIX.md` | Subsystem immutability matrix |
| `RFC_FREEZE_INDEX.md` | RFC freeze index |
| `RELEASE_BASELINE_MANIFEST.md` | Hashes, genesis, inventories |
| `PROTOCOL_CHANGE_SEVERITY_TABLE.md` | Change severity levels |
| `FINAL_CONSTITUTIONAL_SUMMARY.md` | Executive transition narrative |
| `H_CONV_T_DIFF_REVIEW_001.md` | Activation constitutional audit (Verdict A) |
| `docs/archive/rfc/RFC-015-OCCAM-REBALANCE.md` | Historical Occam rebalance (constant 100 RES; superseded mint amount) |
| `docs/archive/rfc/RFC-017-SCHEDULED-RES-REWARD.md` | Shipping RES schedule `W(k)` and `E = 246` |
| `docs/freeze/H_RFC_015_OCCAM_REBALANCE_FREEZE_VERDICT.md` | RFC-015 freeze verdict (Verdict A) |

---

## Cryptographic boundary (TASK-011, 2026-08-24)

The following spending / key-management rules are frozen for mainnet. Changing any of them is a Level-5 consensus fork.

| Rule | Frozen value |
|------|----------------|
| Signature algorithm | Ed25519 only (`libsodium` ≥ 1.0.18); secp256k1 CHECKSIG is disabled |
| HD derivation | Hardened-only (SLIP-0010). Unhardened BIP32 and public `CPubKey::Derive` fail closed |
| Default wallet paths | `wpkh` / `res` use `/0'/*'` or `/1'/*'` (not `/0/*`) |
| Watch-only HD | Ranged xpub cannot expand hardened children; import `addr()` / 32-byte hex |
| `res()` public form | Prints the 32-byte pubkey; never the 64-byte secret |
| Witness v1 RES program | `SHA256(32-byte Ed25519 pubkey)`; spend is `<sig><pk>` |
| ORU P2WPKH program | `HASH160(32-byte Ed25519 pubkey)` |
| `OP_RES_MINT` | Coinbase-only; BIP143 (`WITNESS_V0`) sighash; owner = `HASH160(pk)` |
| `SIGHASH_SINGLE` out of range | Hash is `uint256::ZERO` (not `ONE`); signature rejected |
| RES2 spend | Deleted; RES2 programs are not creatable |
| v1 sighash domain | BIP143 plus `uint32_t{1}` (`SigVersion::WITNESS_V1_RES`) |

---

## FINAL RESULT

**A. MAINNET-FREEZE-READY (RFC-015 FINAL)**

---

## REQUIRED OUTPUT FIELDS

| Field | Value |
|-------|--------|
| **Risk Classification** | LOW (constitutional layer stable); MEDIUM (release commit/tag pending) |
| **Consensus Impact** | NONE from this task (documentation only) |
| **Replay Impact** | NONE — freeze documents existing replay-preserving activation |
| **Determinism Impact** | NONE |
| **Governance Impact** | HIGH — immutability declaration binds future change process |
| **Formal Verification Impact** | NONE — freeze ratifies compiled Coq inventory |
| **Immutability Classification** | **CONSTITUTIONAL-FROZEN** for RES probability, eligibility derivation, and RFC-015 supply/cardinality |
| **Minimal Safe Recommendation** | Tag `mainnet-freeze-rfc015-2026-05-26`; do not alter frozen consensus paths |

---

**Signed:** Freeze documentation layer (`TASK-MAINNET-FREEZE-001` + `TASK-RFC015-OCCAM-REBALANCE-001`)  
**Authority chain:** H-RES-001 → H-RES-003 → H-RFC-014-REVIEW-001 → RFC-015 Occam rebalance → `H_RFC_015_OCCAM_REBALANCE_FREEZE_VERDICT` → this declaration
