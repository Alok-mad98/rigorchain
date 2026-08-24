# Chapter 4 — Convergence Mechanism

## What is Convergence?

**Convergence** is the consensus act of committing **≥ 1 ORU** into a
**CVG2** output. The ORU becomes unspendable and leaves circulating supply.
It does not convert 1:1 into RES. It becomes draw weight `f_i`.

If the commitment enters candidate set **F**, survives the window
(`W = 32`), and is selected, the paying block must mint **`W(h)` RES** in
its **body** to the winner’s declared Witness v1 program.

Short definitions of `convergence_root`, `draw_candidate_root`, and the
RES origin path: [Chapter 9 — Glossary](./09-glossary.md).

A **convergence transaction** is that CVG2 commitment. RES is not a pool
payout and not a miner-chosen bonus.

```
Eligible convergence transactions
        ↓
sorted txids
        ↓
Hash256
        ↓
Convergence Root
        ↓
σ for each candidate
        ↓
H_i < T · f_i
        ↓
at most one RES mint when predicates pass
```

---

## What is `convergence_root`?

```
eligible transactions
        ↓
sorted txids
        ↓
Hash256
        ↓
Convergence Root
```

Canonical form:

```
C = Hash256( concat( sort_memcmp( txid_i ) ) )
```

| Rule | Meaning |
|------|---------|
| Input | Native 32-byte txids of eligible convergence transactions |
| Order | Unsigned byte-wise sort — not arrival order, not miner preference |
| Concatenation | Sorted bytes, no delimiters, no per-leaf Merkle tree |
| Hash | Hash256 = SHA-256(SHA-256(·)) |

`C` is a function of the **set**, not of gossip order. Two honest nodes that
see the same eligible transactions compute the same root.

This is **not** a Merkle root of transaction hashes. Older drafts used that
language. The official commitment is a single Hash256 over the concatenated
sorted txids.

---

## What is `draw_candidate_root`?

The **Draw Candidate Root** (`draw_candidate_root`) is the public digest of
the **draw-eligible** subset — the candidates that enter the selection
inequality for the active height / window.

| Root | Commits |
|------|---------|
| Convergence Root (`C`) | Eligible convergence commitments bound into the block |
| Draw Candidate Root | Draw-eligible candidates for the active draw |

Explorers may display both. Neither display is a verdict. A verifier
recomputes the roots from public lists and compares.

---

## Selection (σ)

Each candidate receives a deterministic digest. Under the frozen
Sigma-PoW-01 rule:

```
σ_i  =  Hash256( txid_i  ||  C  ||  B_{h-1}  ||  LE32(h) )
```

- `C` is the Convergence Root.
- `B_{h-1}` is the **parent block hash**, taken verbatim.
- `h` is the height of the paying block.
- The preimage is exactly **100 bytes**.

The block at height `h` pays the winner of the set committed by block
`h-1`. A producer cannot re-roll the draw by editing the paying block’s
own candidate list after the fact: every σ input is already committed.

`T` on the inequality is **not** the parent PoW target. `T` is the frozen
constant `RES_PROBABILITY_TARGET` (Deterministic Reserve Issuance: **`E = 246`**, windowed).
Mint cost **`C = 1024` ORU per RES mint**. Difficulty adjusts **block finding**.
It does not rewrite RES probability.

Weight is linear:

```
f_i  =  converge_amount(tx_i)
```

There is no `amount^{7/5}` term. Exponentiation, floating point, and
ad-hoc scaling are forbidden.

Eligibility:

```
H_i  <  T · f_i
```

Winner (integer cross-multiplication, not floating division):

```
i ≺ j   iff   H_i · f_j  <  H_j · f_i
winner  =  minimal element under ≺
```

Ties break on txid, byte-wise.

The public form `winner = argmin(H_i / f_i)` is the same order, written
for readers. Consensus implements the cross-multiply form.

---

## What a hash space is

A cryptographic hash such as SHA-256 maps arbitrary input to a fixed
256-bit output. The output space has

```
2^256
```

possible values — about `1.16 × 10^77`. The result is not assigned by a
committee. Absent a break of SHA-256, no one can choose the digest in
advance.

The **shipping** probability identity is a slice of that space with
**`E = 246`**:

```
C = 2^(256-E) = 1024 ORU per RES mint
p(n ORU)            =  min(1, n / 1024)
p_draw(n)           =  n / (1024 · 32)
expected wins       =  n / 1024
```

**Historical RFC-014 parameter** (not an active consensus parameter):

```
2^240 / 2^256  =  1 / 2^16  =  1 / 65536
```

That teaching constant implied `C = 65536` ORU. The long-run **shipping**
identity is **n / 1024**. The window spreads participation; it does not invent
a private probability.

---

## Fairness

Fairness is cryptographic and procedural, not social.

| Property | Meaning |
|----------|---------|
| **Deterministic** | Same accepted block and same candidate set ⇒ same `C`, same σ, same winner |
| **Unpredictable** | Before the parent hash and the committed set exist, no one knows `C` or σ |
| **No privileged participant** | No admin key, no pool override, no website authority, no premine path |
| **Publicly verifiable** | Anyone recomputes roots and selection from public bytes |
| **Cryptographic probability** | Weight is committed ORU. Probability is a hash-space inequality, not discretion |

If a miner changes the eligible set, `C` changes, and **every** candidate’s
σ changes. There is no quiet edit that favors one transaction while leaving
the published root intact.

Fairness does **not** mean “everyone receives RES”. It means there is no
privileged path around the published function.

**No hidden process. The published aggregate is 1/1024 per ORU of
committed weight (`E = 246`, `C = 1024` ORU per mint).**
