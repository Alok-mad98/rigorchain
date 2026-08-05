# RIGOR Architecture Constitution

**Four Layers of Understanding**

---

## Layer 1 — Mathematical Core

### The Selection Machine

#### Intuition

This system is essentially not "computing" but doing something simpler:
Among all candidates, select the single optimal result.

#### Core Rule

The entire system does only one thing:
Calculate a score → select the smallest one.

Formally:
```
H_i < T * f_i
winner = argmin(H_i / f_i)
```

You can understand it as:
- H_i = luck (randomness)
- f_i = weight (investment)

Who has the best "luck / cost" wins.

#### Why This Matters

Traditional blockchain:
- Execute scripts
- Run virtual machine
- Process complex logic

RIGOR:
- Does not execute any program
- Only does selection

Complexity is completely eliminated.

#### Immutability (Core Constitution)

- No computational logic allowed
- No formula changes allowed
- No non-linear weights allowed
- Always only one inequality

---

## Layer 2 — Consensus Structure

### Block / Converge / SIGMA

#### Intuition

Each block does three things:
1. Collect candidates (Converge)
2. Generate randomness (SIGMA)
3. Select winner

#### Key Components

### 1. Converge

All participating transactions are compressed into one:
Convergence Root (C)

Function:
Change any candidate → entire result changes completely

Core anti-cheat mechanism.

### 2. SIGMA (Deterministic Randomness)

Not a random number, but:
Randomness "computed" from block state.

Characteristics:
- Verifiable
- Unpredictable
- Uncontrollable

### 3. Winner Selection

All nodes compute independently → get the same winner

No voting, no divergence.

#### Immutability

- No external randomness sources allowed
- No miner manipulation allowed
- No candidate sorting cheating allowed
- All node results must be identical

---

## Layer 3 — State System

### StateTag + Dual Asset

#### Intuition

This system has no "programs", only "states".

Asset ≠ Script
Asset = State Tag

#### StateTag

Each output is just a simple structure:
TYPE + parameters

For example:
- Ownership
- Destruction (Convergen)
- RES claim

#### Verification Method

Not execute code, but:
Match type → execute fixed rule

Similar to:
```
if TYPE == A → use rule A
if TYPE == B → use rule B
```

#### Dual Asset System

##### ORU (Base Asset)

Produced via PoW
Used for payment / Convergen

##### RES (Scarce Asset)

Obtained by "destroying ORU"
Maximum 1 per block

#### Key Relationship

ORU = Energy
RES = Result

No ORU → No RES

#### Immutability

- No script execution allowed
- No VM allowed
- No complex logic allowed
- Only finite state + fixed rules

---

## Layer 4 — Economic System

### Self-Regulating Supply

#### Intuition

This system has no manual parameter adjustment.
All economic behavior = automatic balancing

#### ORU (Base Currency)

Produced by mining
Halved every 210,000 blocks
Eventually stabilizes

Similar to energy supply.

#### RES (Scarce Product)

Production rule:
Must destroy ORU to obtain
Limit:
Maximum 1 per block
Total cap 21,000,000 RES

#### Key Mechanism

Want RES → Must pay ORU

Result:
Demand ↑ → ORU destroyed ↑ → RES harder to obtain

Automatic adjustment.

#### Fee Mechanism

Uses:
Fee Rate Auction

Characteristics:
- Completely market-determined
- No manual adjustment needed
- Automatic anti-spam

#### System Behavior

More participation → Cost rises → Competition strengthens → Convergence stabilizes

#### Immutability

- No manual parameter adjustment allowed
- No additional incentive mechanisms allowed
- No hidden inflation allowed
- All rules hardcoded

---

## Summary (Four Layers Unified)

### One-Sentence Understanding of the Entire System

This is a deterministic selection system driven by mathematical inequality.

### Four-Layer Correspondence

| Layer | Essence |
|-------|---------|
| Mathematical Core | Selection (inequality) |
| Consensus Structure | State compression + random mapping |
| State System | No execution, only tags |
| Economic System | Self-regulating supply-demand |

### Most Important Conclusion

This system does not execute logic
This system does not interpret rules
This system does not allow change

It does only one thing:
Among all possibilities, select the single correct one.

---

## Final Constitutional Declaration

RIGOR is not a programmable blockchain.
RIGOR is a deterministic selection system.

State → Hash → Compare → Select → Verify

Nothing more.

---

**END OF RIGOR ARCHITECTURE CONSTITUTION**
