# Convergence consensus lock

**See also** [`PROTOCOL-NAMING-TRUTH-SOURCE.md`](./PROTOCOL-NAMING-TRUTH-SOURCE.md) (authoritative `bad-*` rejection codes, RPC replay fields, and `state_hash` / RPL2 naming; `src/` is the source of truth).

**Marker: `CONSENSUS_LOCK_FORGE_V3`**

This document is the normative summary of the **convergence RES draw** as implemented in `src/consensus/` (`rigor_consensus_lock.h`, `convergence_logic.{h,cpp}`, `convergence_root_eligibility.cpp`, `rigor_res.cpp`). Mempool policy and miner ordering are **out of scope** except where noted.

The **economic kernel did not change** in the CONVERGENCE path cleanup: destroy ORU → linear weight `f_i` → lottery `σ_i < T · f_i` → coinbase mints RES. What follows is that kernel, plus the unique window **C** and coinbase mint that the cleanup left as the only commitments.

---

## 0. Kernel that stays fixed

| Piece | Lock | Code |
|--------|------|------|
| Draw formula | `σ_i = Hash256(txid_i ‖ C ‖ B_{h-1} ‖ LE32(h))`, eligible iff `σ_i < T · f_i` | `ComputeSigmaHash` / `SelectionDigest` |
| Frozen T | `RES_PROBABILITY_TARGET` = `floor(2^246 / MIN_CONVERGENCE_AMOUNT) / W` (RFC-017). **Not** parent `nBits`. | `params.h`, `rigor_consensus_lock.h` |
| Window | `CONVERGENCE_DRAW_WINDOW = 32`. The draw paid at height `h` uses unpaid tickets in `[h-32, h-1]`. | A-9-C |
| 1-block lag | Block `h` **pays** the set its **parent** committed. Every σ input is fixed once `h-1` is found. | SECURITY-001-A8 |
| Owner binding | Ticket lock is `<20-byte HASH160(pk32)> <0xbd CONVERGE marker>` (22 bytes). Under Option B, HASH160(pk32) equals HASH160(v1 program). Mint must pay that HASH160. | `res_owner.h`, `CheckResRewardOwner` |
| Weight | `f_i` = ORU `nValue` on the unique CONVERGE-marker output (raw atoms; **not** β = 7/5). | `GetConvergenceAmount` |
| Draw gate | `|F| ≥ CONVERGENCE_DRAW_MIN_TXS_FOR_RES` (`1`) | RFC-015 |

Winner among the eligible set is **uint512 argmin** of `σ` (cross-multiply by weights, then unsigned txid tie-break). See `selection.cpp`.

---

## 1. Ticket (ORU burn)

A convergence transaction is an ordinary ORU spend with **exactly one** positive-value burn:

```
scriptPubKey = <20-byte HASH160(RES program)> <0xbd CONVERGE marker>
```

- CONVERGE marker `0xbd`. Encoding is exactly 22 bytes. The output is not a spendable predicate (`AddCoins` skips it).
- `AddCoins` skips the output (true burn). Amount is `nValue`, not encoded in the script.
- `MIN_CONVERGENCE_AMOUNT` is **1.00 ORU**.
- **Owner** is the HASH160 inside that lock. No sibling RESO RETURN-marker. Retired CVG2 witness v1 is **not** a convergence.
- Byte `0xbc` stays burned (`HasValidOps` still rejects it). Do not reassign.

`HasResOwnerDeclaration` is true iff there is exactly one well-formed lock. Undeclared burns may be valid ORU destruction; they **do not enter F**.

---

## 2. Set F and window C

**F membership** (`ListConvergenceRootCommitmentTxids` / window assemble):

- non-coinbase,
- convergence-shaped (`IsConvergenceTx` / `IsConvergenceCandidate`),
- `GetConvergenceAmount >= MIN_CONVERGENCE_AMOUNT`,
- `HasResOwnerDeclaration`,
- `CheckTxInputs` succeeds against the UTXO view at **block entry**.

**Fee is relay policy, not F membership.** A zero-fee burn that otherwise verifies still enters F. Mempool still uses `DEFAULT_MIN_RELAY_TX_FEE`.

**Window (A-9-C).** The set committed by a source block at height `s` is the unpaid tickets in `[s - W + 1, s]` with `W = 32` (equivalently, the draw paid at `h` uses `[h-W, h-1]`). Already-paid tickets are excluded (`already_paid`). A miner cannot empty the window: tickets in `[s-W+1, s-1]` are already buried.

**C** (`ComputeConvergenceRootFromEligibilityTxids`):

- empty F (`n == 0`): all-zero `uint256()` (no hash);
- nonempty (`n ≥ 1`): `C = Hash256(concat(sort(txids)))` by `uint256` / `memcmp` byte order. A **singleton hashes** `Hash256(txid)`.

There is **no** `|F| < 2 → zeros` shortcut.

**Where C is committed:** after BIP34, `coinbase.vin[0].scriptSig` is exactly two pushes: BIP34 height, then 32-byte C. See [mining-pool-gbt.md](./mining-pool-gbt.md) §2.0. Unique C is this window digest — not a second in-block root, not CVGE.

C does **not** appear in the 80-byte header. It changes the coinbase txid and therefore `hashMerkleRoot`.

---

## 3. Draw at height h (1-block lag)

Block `h` has two independent jobs:

1. **Commit** its own window C in `scriptSig` (the set block `h+1` will draw).
2. **Pay** the winner of the set its parent committed.

```
σ_i = Hash256( txid_i || C || B_{h-1} || LE32(h) )     // 100 bytes exactly
eligible ⇔ σ_i < T · f_i
```

- `txid_i` and `C` come from the parent window.
- `B_{h-1}` is the parent block hash (σ seed). Parent `nBits` is already inside that header; it is **not** a σ slot.
- `h` is the **payout** height (this block).
- `T` is the constitutional constant (frozen). PoW difficulty does not move RES odds.

If nobody is eligible, or `|F| < 1`, there is no winner and this block must not mint.

---

## 4. RES mint (coinbase only)

When the parent draw named a winner and `W(h) > 0` (RFC-017 schedule; `W(k) = 0` after `RES_REWARD_LAST_EPOCH`):

- the coinbase carries **exactly one** RES mint-marker (`0xbb`) output,
- amount is `GetResMintReward(h)`,
- `scriptPubKey` pays the winner's declared HASH160 (`CheckResRewardOwner`).

Validated only by `rigor::ConnectEvaluateCoinbaseResMint`. There is no body-tx mint and no PCORB / RIGR / CVGE consensus object. Extra RETURN-marker outputs are allowed and ignored.

`FindBlockPaidConvergenceTxids` treats the parent-draw winner as paid iff the coinbase has that mint. ConnectBlock resolves the parent draw first, then this block's C. Supply cap is `Consensus::RES_MAX_SUPPLY`.

---

## 5. Maturity (unchanged)

- **`COINBASE_MATURITY` is 100 blocks.** Coinbase ORU cannot fund a burn before that.
- A fresh chain therefore has a ~100-block warm-up with no valid tickets (and no RES mints), even though the schedule function is defined at every height.

---

## 6. What left consensus

These are **not** required, matched, or paired with a mint:

- CVGE coinbase `OP_RETURN` (cumulative / eligible-root),
- RIGR draw-target `OP_RETURN` (parent `nBits` live in the header),
- PCORB carrier / nine-push mint witness / body `ConnectEvaluateResMintTx`.

`convergence_draw.eligible_root` on GBT is an informational hash of the inequality-eligible subset. It is not a coinbase commitment.

---

## References (code)

| Topic | Location |
|--------|----------|
| Numeric locks (W=32, min amount, T schedule) | `consensus/rigor_consensus_lock.h` |
| σ, F, winner, window tail | `convergence_logic.{h,cpp}`, `selection.cpp` |
| C from txid list | `convergence_root_eligibility.cpp` |
| Coinbase `scriptSig` C check | `convergence_root.{h,cpp}`, `ConnectBlock` (`bad-cb-convergence-root`) |
| Owner lock / mint payee | `res_owner.{h,cpp}`, `GetConvergeScriptForPubKey` |
| Coinbase mint | `rigor_res.cpp` (`ConnectEvaluateCoinbaseResMint`) |
| Pool / GBT wire | [mining-pool-gbt.md](./mining-pool-gbt.md) |
| Coinbase maturity | `COINBASE_MATURITY` in `consensus.h` (100) |
