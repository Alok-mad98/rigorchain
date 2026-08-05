# RFC-005: Validation State Machine

**Version:** 1.0 (Canonical Lock)
**Status:** FINAL

**This document MUST be interpreted under RFC-000-FINAL Consensus Constitution**

---

## 1. Abstract

This document defines the only valid execution order for block validation.

Consensus is not only rules — it is ordered state transition.

Any deviation in:
- order
- branching
- state update timing

is a consensus violation.

---

## 2. Global Principle

Validation is a deterministic state machine:
```
STATE(h) → BLOCK(h+1) → STATE(h+1)
```

No step may be reordered.

---

## 3. High-Level Pipeline (Immutable)

```
RECEIVE BLOCK
↓
CheckHeader
↓
CheckPoW
↓
CheckSubsidy
↓
CheckTransactionsBasic
↓
ClassifyTransactions
↓
ComputeEligibleSet F
↓
ComputeConvergeRoot C
↓
Compute sigma for all candidates
↓
SelectWinner
↓
VerifyRESMint
↓
VerifyAllStateTransitions
↓
UpdateSupply
↓
CommitBlock
```

---

## 4. Detailed State Machine

### 4.1 Entry
```
bool ConnectBlock(Block b, State& s)
```

### 4.2 Step 1 — Header Check
```
REQUIRE(CheckHeader(b.header));
```

Includes:
- version
- prev_hash
- timestamp <= now + 2h

### 4.3 Step 2 — PoW
```
REQUIRE(Hash256(b.header) < target_difficulty);
```

### 4.4 Step 3 — Subsidy
```
expected = GetBlockReward(height);
REQUIRE(b.coinbase_reward == expected);
```

Violation:
```
bad-RIGOR-subsidy
```

### 4.5 Step 4 — Basic TX Checks
For each tx:
```
REQUIRE(Deserialize(tx));
REQUIRE(NoDuplicateInputs(tx));
REQUIRE(AmountsValid(tx));
REQUIRE(NoOverflow(tx));
```

### 4.6 Step 5 — Classification
```
for tx in block:
    tx.type = Classify(tx)
```

Must be deterministic:
```
RES_MINT
RES_SPEND
ORU_TRANSFER
```

Ambiguity:
```
REJECT
```

### 4.7 Step 6 — Build Eligible Set F
```
F = []
for tx in block:
    if tx.type == RES_MINT:
        F.append(tx)
```

### 4.8 Step 7 — Compute ConvergeRoot
```
C = ComputeConvergeRoot(F)
```

Rules:
- sort(txids)
- concat
- Hash256

If empty:
```
C = 0x00...00
```

### 4.9 Step 8 — Compute sigma
For each tx ∈ F:
```
tx.H = Hash256(
    txid ||
    C ||
    B_{h-k} ||
    LE32(height)
)
```

Must be exactly:
```
100 bytes input
```

### 4.10 Step 9 — Eligibility Filter
```
eligible = []
for tx in F:
    if tx.H < T * tx.f:
        eligible.append(tx)
```

### 4.11 Step 10 — Winner Selection
Order defined by:
```
a ≺ b  iff  (a.H * b.f) < (b.H * a.f)
```

Algorithm:
```
winner = None
for tx in eligible:
    if winner == None or tx ≺ winner:
        winner = tx
    else if equal:
        if (tx.txid XOR C) < (winner.txid XOR C):
            winner = tx
```

### 4.12 Step 11 — RES Mint Verification
```
if winner != None:
    REQUIRE(exists exactly 1 RES_MINT tx)
    REQUIRE(winner == mint_tx)
    REQUIRE(VerifyConvergeProof(mint_tx))
else:
    REQUIRE(no RES mint tx exists)
```

Violation:
```
bad-RIGOR-mint
```

### 4.13 Step 12 — State Transition
For all tx:
```
REQUIRE(VerifyState(tx))
```

Rules:
- ORU → ORU
- ORU → CONVERGE
- ORU → RES (only winner)
- RES → RES

### 4.14 Step 13 — Supply Update
```
S_ORU += block_reward
S_ORU -= total_converge
if winner:
    S_RES += 1
```

### 4.15 Step 14 — Cap Enforcement
```
REQUIRE(S_RES <= RES_CAP)
```

Violation:
```
bad-RIGOR-RES-cap
```

### 4.16 Step 15 — Commit
```
ApplyUTXOChanges()
SaveState()
```

---

## 5. Forbidden Behaviors

- reordering steps
- lazy evaluation
- partial validation
- floating arithmetic
- non-deterministic branching

---

## 6. Consensus Lock

Execution order is immutable.

---

## Core Execution Path (Locked)

```cpp
for (tx : block.vtx) {

    AssetType asset = GetTransactionInputAsset(tx);

    if (asset == ASSET_ORU) {

        if (HasRESOutput(tx)) {
            REQUIRE(IsRESMintTx(tx));
            REQUIRE(VerifyConvergeProof(tx));
            EvaluateMint(tx);
        }

    } else if (asset == ASSET_RES) {

        EvaluateSpend(tx);

    } else {
        return state.Invalid("bad-asset");
    }
}
```

---

## 7. σ Usage Location (Sole Legal Position)

```cpp
uint256 H_i = ComputeSigmaHash(txid, C, B_h_k, target_prev, height);

// RFC-014 Conv-Decouple-01:
//   T is the constitutional constant `Consensus::Params::RES_PROBABILITY_TARGET`
//   (= 2^240). It is NOT derived from `SetCompact(target_prev)`. `target_prev`
//   participates only inside `bind_commit` (Sigma-CR-01).
const uint256& T = consensus_params.RES_PROBABILITY_TARGET;
if (H_i >= T * f_i) reject;
```

### Forbidden
- ComputeSigmaHash
- RESRandom

### Unified Code Interface (Please Align)

**Hash (σ)**
```cpp
uint256 ComputeSigmaHash(
    const uint256& txid,
    const uint256& C,
    const uint256& B_h_k,
    uint32_t height
);
```

**Proof**
```cpp
struct ConvergeProof {
    uint256 converge_txid;
    uint256 bind_hash;
    uint32_t height;
    uint32_t target_prev;
    uint256 converge_root;
    std::vector<uint8_t> proof_blob;
    std::array<uint8_t, 64> signature;
};
```

**Note**: Code implements extended `ConvergeProof` struct with additional fields (convergence_amount, pubkey, has_sig) for RFC-000 requirements. Per RFC-000-FINAL hierarchy, these extensions override RFC-005 base structure.

**Verification**
```cpp
bool VerifyConvergeProof(const CTransaction& tx);
```

---

**END OF RFC-005**
