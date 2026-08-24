# Tightening — 2026-08-18

**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md)
or the honesty boundary in [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)
([FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md)).

This is a maintenance pass, not a new verification campaign.

## Honesty (unchanged)

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

The model is strongly verified; implementation correspondence is tighter than
the original baseline and includes the tripartite hash + differential gates.
It is **not** a refinement proof.

## Replay-model fix (mandatory)

**Before.** `apply_tx_utxo` created every output at
`MkOutPoint hash32_placeholder (tx_index + i)`. Every transaction in every
block shared the same txid, so:

- two blocks both applying a tx at index 0 overwrote each other's UTXOs;
- “spend must delete” was refutable even without duplicate txids;
- `not_recreated` had to be assumed rather than derived.

**After.** `TxAbs` carries an abstract `tx_id : Hash32` (not a computed hash;
this layer still has no SHA-256). Creates are `(tx_id, 0) .. (tx_id, n-1)`.
`tx_index` remains the in-block position only.

| Theorem | Status |
|---------|--------|
| `utxo_spend_removes_output_distinct_txid` | New. Unconditional when `outpoint_txid op <> tx_id tx`. |
| `distinct_txid_not_recreated` | New. Derives `not_recreated` from distinct identity. |
| `utxo_spend_removes_output` | Restated; `not_recreated` no longer depends on `tx_index`. |
| `utxo_spend_removes_output_unconditional_is_false` | Kept. Counterexample is now **txid reuse**, not the placeholder shortcut. |

Residual limitation: a transaction that reuses the txid of an outpoint it
spends can re-insert that outpoint (BIP30-class). That is a real duplicate-txid
edge, not a modelling defect. No new axioms.

Rebuild: `ReplayReorgCore/ReplayInvariants.vo` and dependents compiled clean
under Coq 8.15.0.

## Golden vectors

Nine high-risk rows added (315 → **324** shared, 108 → **117** Coq-enforced):

| ID | Why |
|----|-----|
| `REORG-A9C-008` | Restored branch then re-pay first winner |
| `REORG-A9C-009` / `010` | Fork A prefix paid vs fork B suffix paid |
| `A9C-W32-drop_first` / `drop_last` / `all_paid` | Window width `W=32` exclusion edges |
| `A9C-NDW-000` | Paid txid stays excluded under candidate permutation |
| `INQ-Z-000` | `f=0` is ineligible |
| `ENC-W-000` | Encoder height = `W` |

Tag: `tightening-replay-txid-2026-08-18`. Finite vectors are still not equivalence.

## Gate

`formal_verification/scripts/run_differential_gate.sh`:

- prints vector count and tag after artifact freshness;
- on mismatch, prints regenerator commands;
- PASS line repeats the three honesty flags;
- `COQ=1` compiles `ReplayInvariants.v` then `Kernel/GoldenHarness.v`.

Differential + tripartite gate: **PASS** (`SKIP_TEST_BITCOIN=1` in this run;
portable C++ CLI still covers CSHA256/FIPS). GoldenHarness rebuilt clean.
