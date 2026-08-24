# RIGOR Technical Whitepaper — Official Structure

**Task:** `TASK-ALPHAPLUS-TECHNICAL-WHITEPAPER-DESIGN-AUDIT-001`  
**Document:** Engineering-level technical whitepaper architecture (not an RFC)  
**Status:** Structure established — body not written in this task  
**Date:** 2026-08-18  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

This document defines the chapter architecture of the **RIGOR Technical Whitepaper**.
It is distinct from the protocol whitepaper (`RIGOR_PROTOCOL_WHITEPAPER.md`).

| Paper | Role | Planned site route |
|-------|------|--------------------|
| Protocol whitepaper | Protocol introduction (economics + Convergence teaching) | `/documentation/whitepaper` |
| Technical whitepaper | Engineering specification (architecture + verification surface) | `/documentation/technical-whitepaper` |

Public mathematics: [`RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md).
Historical RFC corpus: [`../archive/rfc/`](../archive/rfc/).
Engineering specs: [`../specification/`](../specification/). This structure does not modify them.

---

## Publication identity

| Field | Value |
|-------|-------|
| Title | RIGOR Technical Whitepaper |
| Version | Architecture (pre-prose) |
| Product | RIGOR / AlphaPlus |
| Voice | Engineering, deterministic, non-speculative |
| `equivalence_claimed` | **false** |

**Required non-claims:** no protocol-to-binary equivalence; no mainnet completion; no investment return; RES is not mined; explorers/pools/websites are not consensus authorities; no “no scripts”; no “formally proven blockchain”; no “script is the asset.”

**Official architecture formula:**

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    4 Core Monetary Script Families
  +
    UTXO Local Validation
  +
    Ed25519 Authentication
  +
    OpuHash + ASERT PoW
  +
    Deterministic Reserve Issuance
  +
    Deflationary Fee Burn
  +
    Formal Verification Model
  +
    No Global Asset State
```

**Architecture identity:** RIGOR is a two-asset UTXO protocol with a closed
script-compliance surface. Script is the compliance/classification layer.
The formal system verifies protocol models and finite vectors. The production
C++ implementation is not claimed to be formally equivalent
(`equivalence_claimed = false`).

---

## Abstract (required)

Explain:

**RIGOR is a Bitcoin-derived UTXO blockchain introducing two native assets with different monetary identities while preserving Bitcoin-style verification simplicity.**

Required points:

- Code foundation is Bitcoin Core v25 (UTXO set, 80-byte header, deterministic ConnectBlock-style validation).
- RIGOR does **not** attempt to replace Bitcoin’s security philosophy.
- RIGOR **extends** the UTXO architecture with an additional native asset identity.
- ORU and RES are first-class consensus assets, not tokens, contracts, colored coins, inscriptions, Runes, or Ordinals.
- Script surface is minimized; there is no general-purpose smart-contract VM.
- Proof-of-work is OpuHash over the Bitcoin-style header id.
- RES allocation is deterministic Convergence mathematics, independently recomputable.
- Formal verification improves confidence; `equivalence_claimed` remains `false`.

```
equivalence_claimed = false
```

---

## Chapter 1 — Bitcoin Architecture Foundation

**Purpose:** State what is inherited before stating what is added.

**Positioning:** RIGOR does not attempt to replace Bitcoin security philosophy. RIGOR extends the UTXO architecture with additional native asset identity.

### 1.1 Bitcoin Core v25 code foundation

- Node heritage: Bitcoin Core v25 (`CLIENT_VERSION_MAJOR = 25`, package `25.0.0` in `core/configure.ac`).
- Product release identity (`RIGOR_RELEASE_VERSION`) is separate from that packed Core version and must not be conflated.
- Inherited surfaces: UTXO set (`Coin` / `CCoinsView`), 80-byte `CBlockHeader`, transaction serialization with optional witness, merkle root over txids, mempool + ConnectBlock validation pipeline, P2P block relay patterns.

### 1.2 UTXO model inheritance

```
TxOut = (nValue, scriptPubKey)
```

- No account model as consensus truth.
- No parallel asset field in `CTxOut`.
- Ownership changes only by spending old outputs and creating new ones.
- Dual-asset **class** is inferred from `scriptPubKey`; dual-asset **legitimacy** is consensus (conservation, mint predicates, convergence rules).

### 1.3 PoW security model

- Blocks are ordered by proof-of-work and parent linkage.
- Difficulty adjustment is ASERT (mainnet half-life locked at `172800` seconds).
- The PoW **function** is OpuHash (Chapter 4), not Bitcoin SHA256d-as-work. The header **id** remains SHA256d of the 80-byte header.
- Genesis is work-exempt after valid `nBits`; all later headers must satisfy OpuHash vs target.

### 1.4 Deterministic validation

```
same accepted block bytes  →  same validity decision
```

Independent full nodes recompute roots, σ, winner, mint, and PoW. Invalid modifications fail published checks. This is Bitcoin-style verification simplicity applied to a dual-asset ledger.

### 1.5 Minimized execution surface

Bitcoin’s validation philosophy (explicit rules, no hidden authority) is preserved. RIGOR additionally **rejects** Taproot/BIP341, secp256k1 spend signatures, and a general-purpose VM (Chapter 3). Block serialized size and weight are capped at **1,000,000** (RFC-015 Alpha1) — a deliberate reduction of Bitcoin’s envelope, not a soft-fork of Bitcoin.

**Sources:** RFC-015 Alpha1 consensus model §3–5; `docs/architecture/ARCHITECTURE.md`; `core/src/consensus/tx_check.cpp`.

---

## Chapter 2 — Two Native UTXO Assets Architecture

**This chapter is critical.** It must not be a restatement of the protocol paper’s economic essay. It must define **Native Asset Identity** inside UTXO consensus.

### 2.1 Native Asset Identity Layer

Most Bitcoin extensions add:

- colored assets / metadata layers
- general-purpose scripts / contracts
- external indexing systems (inscriptions, ordinals, runes)

RIGOR instead creates a **Native Asset Identity Layer** inside the UTXO consensus architecture:

- Asset class is a consensus classification of `scriptPubKey` (witness v0 / v1).
- Asset amounts are the output `nValue` in that class’s unit.
- Conservation is enforced per asset pool (ORU and RES separately).
- No overlay indexer is required for a full node to know what an output is.

Both ORU and RES are:

- first-class consensus assets
- equal protocol identities (neither is “just metadata on the other”)
- directly validated by node consensus
- not external layers

### 2.2 ORU — Medium of Exchange asset

Properties that must be stated:

- native UTXO asset
- consensus-level identity
- **not** a token
- **not** a contract asset
- **not** a wrapped asset
- **not** a colored coin
- **not** an inscription
- **not** a Rune
- **not** Ordinals

Operational properties (cite frozen rules; do not invent):

| Property | Frozen rule |
|----------|-------------|
| Script path | Witness v0 |
| Unit | `1 ORU = 100,000,000` base units |
| Subsidy sequence | `1000 → 500 → 250 → 125` |
| Tail | **125 ORU** per block (terminal of the reduction sequence) |
| Roles | fees, miner subsidy, Convergence entry weight |
| Ledger origin | Coinbase subsidy `R(h)` |

### 2.3 RES — Store of Value asset

Properties that must be stated:

- native UTXO asset
- independent monetary identity
- maximum supply **21,000,000**
- generated through the Convergence mechanism
- **not** a derived token
- **not** smart-contract issuance

Operational properties:

| Property | Frozen rule |
|----------|-------------|
| Script path | Witness v1 (not CVG2) |
| Unit | `1 RES = 1,000,000` atoms |
| Per successful mint | **`W(h)`** (40.000000 RES at genesis; Deterministic Reserve Issuance) |
| Per block | `M(h) ∈ {0,1}` |
| Ledger origin | Non-coinbase mint in the **block body** |
| Receiver | Winner’s declared RES program — **not** the miner |

```
PoW  →  ORU  →  Convergence  →  RES
```

PoW does **not** create RES. `No Convergence → No RES`.

### 2.4 RIGOR Monetary Architecture (required subsection)

Explain that RIGOR separates monetary functions. Do not mix the ORU supply
ledger, circulating UTXO supply, Convergence destruction, and Deflationary
Fee Burn. Each remains a distinct quantity.

**ORU:** PoW security asset; transaction fee asset; destroyed through
Convergence; affected by Deflationary Fee Burn.

**RES:** reserve asset; Deterministic Reserve Issuance; capped supply;
created only through protocol-defined mint authorization. Do not write
“RES mining.”

**Deflationary Fee Burn:** protocol-level ORU destruction of eligible
RES-transfer fee components. Not a global consensus supply state, not a
wallet balance, not a UTXO, not a miner-controlled parameter.

**Deterministic Reserve Issuance:** maximum supply **21,000,000 RES**. RES is
not created by Proof-of-Work; it is deterministically issued after
protocol-defined conditions. Actual circulation depends on Convergence
participation — do not claim that 21M will necessarily be reached. Mint amount
is `W(h)` (40.000000 RES at genesis; polynomial decay by epoch; zero after
epoch 250). Formally verified *monetary invariants* in the Coq schedule model;
`equivalence_claimed = false`.

### 2.5 Difference from overlay designs

| Overlay pattern | RIGOR |
|-----------------|-------|
| Colored coin / metadata on existing UTXO | Native classification in consensus |
| Inscription / Ordinals / Runes | Not used |
| ERC-20-style contract asset | No contract VM |
| Wrapped / bridged representation | Independent UTXO identity |
| External indexer as source of truth | Full node validates both assets |

**Sources:** RFC-015 Alpha1 §5; RFC-001 / RFC-002 / SPEC-002; `PROTOCOL_TERMINOLOGY_ALIGNMENT.md`.

---

## Chapter 3 — Script Minimization Philosophy

**Audit the implementation. Do not claim “no scripts.”**

### 3.1 What exists

| System | Exists? | Role |
|--------|---------|------|
| V0 scripts | **Yes** | ORU: P2WPKH (20-byte) and P2WSH (32-byte program) |
| V1 scripts | **Yes** | RES key-path (32-byte Ed25519 program). Live Convergence is `OP_CONVERGE`, not a v1 program. |
| Witness rules | **Yes** | BIP141-shaped witness programs; v1 is **not** Taproot |
| Validation paths | **Yes** | `IsAllowedScript` + `CheckTxOutAssetWitnessBinding` + script interpreter |
| `SigVersion::BASE` | Interpreter remnant | Bare / P2SH heritage; new P2PKH/P2SH outputs are consensus-invalid |
| `SigVersion::WITNESS_V0` | **Yes** | P2WPKH implied script; P2WSH inner-script execution |
| Taproot / Tapscript | **No** | Removed from consensus path |
| General-purpose VM | **No** | Not implemented |

Consensus gate (`tx_check.cpp`): only witness v0 and witness v1 with a 32-byte program (plus zero-value OP_RETURN) are allowed as chain outputs. secp256k1 CHECKSIG hard-fails (`SCRIPT_ERR_SECP256K1_DISABLED`). Spend signatures are Ed25519.

### 3.2 Required wording

> RIGOR minimizes programmable script surface. It does not implement a general-purpose smart contract VM.

Script is the **compliance layer** (validity conditions). Asset class is inferred from the program; legitimacy still requires UTXO + economic proofs + consensus.

### 3.3 Security philosophy of minimization

More programmable surfaces create:

- larger attack surface
- more consensus complexity
- more unexpected states

RIGOR philosophy:

- deterministic rules
- limited execution
- explicit validation
- reduced consensus attack surface

P2WSH remains the largest inherited execution surface (inner script at `WITNESS_V0` with Ed25519-only CHECKSIG). The technical paper must say that honestly. Opcode-level inventory is a later appendix if confirmed (audit §12.2).

**Sources:** RFC-015 Alpha1 §5–6; `script/interpreter.h`; `docs/SCRIPT_STRUCTURE.md` (byte examples only; ignore stale ECDSA/Merkle/Taproot prose).

---

## Chapter 4 — OpuHash Proof-of-Work

Create a technical explanation from **verified implementation information only**.

### 4.1 Algorithm overview (verified)

1. Serialize the 80-byte header (Bitcoin layout).
2. `base_hash = SHA256d(header)` (`CBlockHeader::GetHash()`).
3. Fill a **64 KiB** buffer with ChaCha20 keystream keyed by `base_hash` (IV/counter zero).
4. Mix **256** rounds: index from `ReadLE32(current_h) ^ (round << 24)`, load 8 LE bytes, `Hash256(current_h || mem_val)`.
5. Final `pow_hash = Hash256(base_hash || mixed_hash)`.
6. Valid iff compact `nBits` is well-formed **and** `pow_hash ≤ target` (genesis exempt).

Do not invent extra rounds, salts, or GPU-specific steps. Do not treat ASIC/CPU marketing sentences in `opushash_v3.md` as measured facts.

### 4.2 Relationship to PoW security

- OpuHash is the consensus work function. It does not mint RES.
- Difficulty (ASERT) sets `nBits`; the Convergence draw target `T` is independent.
- Header admission must run this check (not compact-shape-only).

### 4.3 Mining workflow

```
Miner:
candidate block
      |
      v
block header
      |
      v
OpuHash
      |
      v
Proof validation
      |
      v
network acceptance
```

Network acceptance still includes merkle root, Convergence commitments, script/witness checks, and RES mint fail-closed rules — PoW is necessary, not sufficient.

### 4.4 Block validation path

`CheckProofOfWork(header_hash, nBits, params)` → `GetOpusHashFromBaseHash` → compare to compact target. Then full `ConnectBlock` (unchanged by this documentation task).

**Sources:** `core/src/pow_opus.cpp`, `core/src/pow.cpp` (read-only), `core/doc/opushash_v3.md`, `core/doc/miner-opushash.md`, `rigor_consensus_lock.h`.

---

## Chapter 5 — Convergence Consensus Mathematics

Keep Convergence_Root and draw_candidate_root **separate**.

### 5.1 Convergence_Root

```
C = Hash256(concat(sort(eligible txids)))
```

Properties:

- deterministic
- publicly reproducible
- order independent (sort is canonical)
- `|F| < 2` → 32 zero bytes

`F` is the consensus membership of eligible CVG2 commitments in the window `W = 32` (already-paid winners excluded). Users and miners do not choose the sort key.

### 5.2 draw_candidate_root

Explain separately. After `F` and `C` are fixed, each candidate is tested against `H_i < T · f_i`. `draw_candidate_root` commits that **inequality-eligible subset**. Empty subset hashes empty concatenation.

### 5.3 Candidate generation, winner selection, fairness boundary

```
σ = Hash256(
  txid ||
  C ||
  B(h-1) ||
  LE32(height)
)
```

- `f_i = converge_amount` (linear; no `amount^{7/5}`).
- Eligibility: `|F| ≥ 1` ∧ `f_i ≥ 1` ∧ `H_i < T · f_i`.
- Winner: integer cross-multiply `H_i · f_j < H_j · f_i`; txid tie-break.
- Deferred: height `h` pays the set committed by `h-1`.

**Fairness boundary:** no participant can predict the result before the final transaction set **and** the parent hash exist. This is a hash-space property, not hidden randomness.

Neither root is a Merkle tree. RPC copy that says “Merkle” is not this paper.

**Sources:** RFC-000, RFC-008, RFC-014 v1.2, RFC-015 Occam; consistency audit.

---

## Chapter 6 — Mathematical Fairness

No personality references. No Knuth references. No emotional marketing.

### 6.1 Probability — shipping `E = 246`, `C = 1024`

**Deterministic Reserve Issuance:** unwindowed exponent `E = 246` ⇒ mint cost `C = 1024` ORU per RES mint. Aggregate expected wins per committed ORU ≈ `n / 1024`. Windowing (`W = 32`) changes per-draw chance (`n / (1024 · 32)`), not the aggregate identity.

It is not a guaranteed winning probability, not a fixed mining probability, and not a marketing round number.

**Historical RFC-014 parameter** (teaching slice only; not an active consensus parameter):

```
2^240 / 2^256 = 1 / 2^16 = 1 / 65536
```

Do not present `1/65536` as an active consensus parameter.

### 6.2 Reduction sequence

```
1000 → 500 → 250 → 125
```

**125 is the terminal value of the reduction sequence.** After three halvings (interval 210,000), the ORU subsidy locks. Avoid claiming it was “chosen because of mathematical beauty.” Protocol meaning: miner payment in the utility asset does not decay to zero. Fairness of RES allocation is the published integer draw rule, not the numeral 125.

---

## Chapter 7 — Self-Proving Blockchain

### 7.1 Verification chain

```
Converge transaction
        ↓
script commitment
        ↓
Convergence_Root
        ↓
draw_candidate_root
        ↓
coinbase commitment
        ↓
merkle root
        ↓
block header
        ↓
PoW verification
```

“Script commitment” means the CVG2 (and related) `scriptPubKey` bytes that make the economic action an on-chain UTXO fact — not a smart contract.

Any honest node recomputes this chain from public bytes. No trusted authority is required. Unequal bytes fail validation. The protocol does not claim cheating is “impossible”; it claims failed checks are detectable.

### 7.2 Zero Trust Verification

Public data → independent calculation → accept / reject. Website, explorer, wallet, and pool are observers.

### 7.3 Observational fingerprint (optional short subsection)

Consensus Fingerprint is a **verification identity layer**, not a consensus-rule replacement. Cite `docs/architecture/CONSENSUS_FINGERPRINT_LIBRARY.md`.

```
equivalence_claimed = false
```

---

## Chapter 8 — Formal Verification Boundary

Must clearly separate three layers. Public entry: `verification/README.md`.
Reproduce: `docs/security/HOW_TO_VERIFY_RIGOR.md`.

Allowed claim: Formally verified protocol invariants with explicit verification boundaries.
Forbidden: “fully proven blockchain”, “bug free”, “zero risk”.

### Layer 1 — Frozen protocol rules

RFC / SPEC / `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`. This layer **is** the protocol. A model or archive does not rewrite it.

### Layer 2 — Scientific model

Coq development (`formal_verification/coq`). Theorems are about **the model**. Strongly verified model ≠ shipping-binary identity.

### Layer 3 — Runtime archive

Differential gate (golden vectors, tripartite hash, σ/root CLI) and empirical campaigns such as PROOF-034. Finite traces. Not refinement.

Maintain:

```
equivalence_claimed = false
```

Formal verification improves confidence but does not claim completed equivalence unless proven. Cite `FORMAL-VERIFICATION-STATUS.md` and `FORMAL-VERIFICATION-VERDICT.md`. Do not paste a fake completeness certificate.

---

## Chapter 9 — Mining Architecture

**Priority:** Windows miner experience. Do not focus on hidden background mining.

### 9.1 User flow

```
Windows Miner
        ↓
Wallet
        ↓
ORU mining rewards
        ↓
Convergence participation
        ↓
RES generation
```

Mining must be visible and understandable: shares, height, ORU maturity, Convergence state, RES result. Operator logs are not the primary UX.

### 9.2 Protocol facts that must remain true

- Miners mine **ORU** (OpuHash).
- RES is **not** mined.
- Pool policy cannot allocate RES.
- A found block is checked by independent verification, not by trusting a dashboard.

Cite `docs/mining/ALPHAPLUS_WINDOWS_MINER_DASHBOARD_MVP.md` as product detail; keep this chapter architectural.

---

## Chapter 10 — Security Philosophy

Security priority:

1. Reduce consensus complexity
2. Reduce execution surface
3. Preserve deterministic validation
4. Avoid unnecessary programmability
5. Make verification possible by independent nodes

Implications already frozen (describe, do not propose changes):

- Two native assets instead of an overlay token system
- Witness v0/v1 only; no Taproot VM
- Ed25519-only spends
- Integer-only Convergence math
- Fail-closed RES mint
- OpuHash at header admission
- Zero Trust recomputation

This chapter is philosophy constrained by implementation, not a wishlist.

---

## Cross-cutting rules

1. **Consensus unchanged.** This structure is not a change proposal.
2. **Native UTXO language.** Do not call ORU or RES tokens.
3. **Two roots stay distinct.** Convergence_Root ≠ draw_candidate_root.
4. **Scripts exist; VM does not.**
5. **OpuHash only from verified sources.**
6. **Windows miner is the public mining path.**
7. **`equivalence_claimed = false` everywhere it would otherwise be implied true.**
8. **Integer math.** Do not introduce floats as consensus.

---

## Future prose work (out of scope here)

When written, the body should live at:

```
docs/whitepaper/RIGOR_TECHNICAL_WHITEPAPER.md
```

Optional later chapter files may mirror this outline. Until then, **this structure document is the technical whitepaper architecture.** RFC, kernel, PoW, and validation files remain untouched.
