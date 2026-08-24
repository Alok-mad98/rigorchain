# Glossary — Convergence, Roots, and Where RES Comes From

This chapter is the official noun list for the website and PDF. It does not
change consensus. Terms follow the current freeze, not older “burn / forge /
Merkle root” drafts.

---

## What is Convergence?

**Convergence** is a consensus action: a user commits **at least 1 ORU** into
a **CVG2** output. That output is not spendable. The ORU leaves circulating
supply. It does **not** turn into RES.

What Convergence *does* buy is **weight** `f_i` in a later deterministic
draw. If that commitment is eligible and selected, consensus may mint RES
to the RES program declared with the commitment.

```
ORU committed (CVG2, unspendable)
        ↓
weight f_i in candidate set F
        ↓
deterministic draw
        ↓
maybe a RES mint — never a 1:1 swap
```

Convergence is not mining. It is not a pool payout. It is not an exchange
counter. **No Convergence → no new RES.**

---

## What is `convergence_root`?

**`convergence_root`** (Convergence Root, **`C`**) is the public commitment
to the eligible convergence set.

```
eligible transactions
        ↓
sorted txids
        ↓
Hash256
        ↓
convergence_root  (C)
```

```
C = Hash256( concat( sort_memcmp( txid_i ) ) )
```

- The sort is byte-wise on native txids, not arrival order.
- It is **not** a Merkle tree.
- Anyone with the same eligible list computes the same `C`.
- `C` is bound into the block. Changing one txid changes `C`, and therefore
  every later σ that includes `C`.

Explorers may show `convergence_root`. That display is not a verdict. A
verifier recomputes `C` from the public txid list.

---

## What is `draw_candidate_root`?

**`draw_candidate_root`** (Draw Candidate Root) is the public digest of the
**draw-eligible** subset — the candidates that actually enter the selection
inequality for the active height / window (`W = 32`).

| Name | Field | Commits |
|------|-------|---------|
| Convergence Root | `convergence_root` / `C` | Eligible convergence commitments |
| Draw Candidate Root | `draw_candidate_root` | Draw-eligible candidates for this draw |

They are related and both public. They are not the same object.

- `C` anchors the committed set used by σ.
- `draw_candidate_root` lets a reader check **who was actually in the
  draw** for that window, independently of UI labels.

RPC and explorers may show `draw_candidate_root_committed` (what the block
bound) and `draw_candidate_root_computed` (what a node recomputes). If those
two differ, the published identity does not match the bytes.

Neither root mints RES. They only make the candidate set checkable.

---

## Where does RES come from?

**RES is not mined from coinbase. RES is not a 1:1 conversion of ORU.**

RES appears only when consensus selects a Convergence winner and the paying
block includes **exactly one** RES mint transaction in the **block body**.
That mint creates a new **Witness v1** UTXO (not CVG2) for **exactly
`W(h)` RES**, paid to the RES program declared at Convergence.

| | |
|--|--|
| Ledger origin | A non-coinbase mint in the block body |
| Economic origin | User-initiated Convergence (ORU committed as weight) |
| Amount | `W(h)` RES if there is a winner; otherwise 0 |
| Cap | 21,000,000 RES (Deterministic Reserve Issuance schedule cannot exhaust the cap even if every block mints) |
| Calendar | Mint **amount** is height-indexed; **whether** a mint occurs depends on Convergence |

Burned / committed ORU does **not** become RES. It is subtracted from ORU
supply and used only as `f_i`.

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

PoW does not create RES. PoW only provides irreversible order so the
deterministic allocation can be the same on every honest node.

---

## Contrast: where ORU comes from, where RES comes from

| | **ORU** | **RES** |
|--|---------|---------|
| Role | PoW security budget + fees + Convergence entry weight | Independent scarce asset |
| Classification | Witness v0 (`scriptPubKey` parse) | Witness v1 (not CVG2) |
| Ledger origin | Coinbase subsidy `R(h)` | One non-coinbase mint in the block body |
| Per block | 1000 → 500 → 250 → **125 forever** | **`W(h)` RES** if a winner, else **0** |
| Who receives it | The miner (coinbase script) | The draw winner’s declared RES program — **not** the miner |
| Cap | No hard cap (permanent tail inflation) | **21,000,000 RES** |
| Who can create it | Consensus coinbase rule only | Consensus winner-body mint only |

**One sentence:** ORU is created in **coinbase**. RES is newly minted after
ORU is committed, by a consensus draw, and is written in the **block body**,
not in coinbase.

---

## The full path — how RES “appears”

```
User locks ≥ 1 ORU in CVG2 (unspendable; leaves ORU supply)
        ↓
Enters candidate set F (window W = 32 blocks)
        ↓
Parent block h−1 commits F
        ↓
Child block h draws with σ
        ↓
Eligible  ⇔  |F| ≥ 1  ∧  f_i ≥ 1  ∧  H_i < T · f_i
        ↓
Exactly one winner by integer cross-multiplication
        ↓
Block h body must contain exactly one RES mint (PCORB mint witness)
        ↓
New W(h) RES Witness v1 UTXO → winner’s declared program
```

`T` is the frozen constitutional constant `RES_PROBABILITY_TARGET`. It does
**not** move with mining difficulty. `W = 32` exists so the block producer
cannot monopolize the whole candidate set for a period
(SECURITY-001-A9-C).

The draw is not a random-number oracle. It is a frozen inequality
(RFC-000 / RFC-014) plus a deterministic total order.

### What miners and pools cannot do

- A miner cannot mint RES by changing a payout address
  (`bad-rigor-res-owner-mismatch`).
- A block that has a winner but omits the mint is invalid
  (`bad-rigor-res-winner-mismatch`).
- A block that has no winner but includes a mint is invalid
  (`bad-rigor-res-without-winner`).
- Pool software is not an issuer. Assembling the mint only **settles** a
  user who already won. It is not a miner faucet.

There is no second issuance door: not miner, not pool, not RPC, not wallet,
not coinbase.

---

## Can RES be over-issued?

**Not at consensus.** An over-issuing block is invalid.

Frozen constants:

| Constant | Value |
|----------|-------|
| `GetResMintReward` / `W(h)` | 40.000000 RES at genesis; height schedule thereafter |
| `RES_MAX_SUPPLY` | 21,000,000 RES |
| `M(h)` | `{0, 1}` — at most one success per block |
| Minimum entry | \|F\| ≥ 1 and at least 1 ORU committed |

Connect-block rules (public form):

- Winner exists ⇒ exactly one mint, amount **must** be `W(h)`
- No winner / F too small ⇒ **zero** mints allowed
- `S_RES + W(h) > 21,000,000` ⇒ reject (`bad-rigor-res-max-supply`)
- Ordinary RES transfers: `res_in ≥ res_out` — a transfer cannot create RES

The only way circulating RES can fall is a user destroying RES
(`res_in > res_out`). That tightens supply. It never loosens the cap.

**Audit sentence:** RES issuance is isolated to the Convergence-winner
body mint.

---

## Is issuance guaranteed?

Three different questions. Three different answers.

### 1. Hard cap — guaranteed

```
S_RES(h)  ≤  21,000,000
```

This is a consensus predicate, not a policy.

### 2. A winner must be paid — guaranteed (fail-closed)

If parent-committed selection has a winner, the paying block **must**
create that `W(h)` RES. The miner cannot swallow the prize, change the
payee, or change the amount. The whole block is invalid if they try.

### 3. A calendar of continuous issuance — **not** guaranteed

New RES enters only through the deterministic Convergence draw. How fast
it appears depends on participation (how much ORU is committed). The
protocol **does** give the mint amount a height schedule (`W(h)`). It
does **not** give a date curve for whether a mint occurs.

Therefore:

| Situation | What happens |
|-----------|----------------|
| Nobody converges | RES minted can stay **0** for a long time (pure PoW blocks) |
| Heavy Convergence | Up to one success per block; amount `W(h)`; the Deterministic Reserve Issuance schedule cannot exhaust 21M even if every block mints |
| First ~100 blocks of a new chain | Coinbase ORU is immature; Convergence cannot spend it; RES warmup is 0 |
| After the reward schedule ends | `W(h) = 0`; only RES transfers remain. ORU PoW continues |

“Issue steadily for 100–1000 years” is an **aspiration**, not a protocol
requirement. `T` has no difficulty-style negative feedback that forces a
mint in every block.

---

## One sentence

**ORU is mined in coinbase. RES is newly minted by a consensus draw after
ORU is committed, written in the block body, never in coinbase.**

The lock is: **`W(h)` RES × at most once per block × 21 million hard cap.**

What is protocol-enforced: **cannot print extra; a winner must be paid `W(h)`.**  
What is not guaranteed: **a mint every block, or a year-by-year fill of 21M.**
