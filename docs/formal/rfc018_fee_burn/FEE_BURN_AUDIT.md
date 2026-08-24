# RFC-018 RES-transfer ORU fee burn — Formal Audit

| Field | Value |
|-------|-------|
| **RFC** | [RFC-018-RES-TRANSFER-ORU-FEE-BURN.md](../../rfc/RFC-018-RES-TRANSFER-ORU-FEE-BURN.md) |
| **Primary Coq modules** | `formal_verification/coq/fee-burn/` |
| **Python spec** | `formal_verification/python-spec/rfc018_fee_burn.py` |
| **Golden vectors** | `formal_verification/golden-vectors/rfc018_fee_burn.json` |
| **Parity gate** | `formal_verification/tests/test_rfc018_fee_burn_parity.py` |
| **C++ lock** | `Consensus::OruResTransferFeeBurnShare` in `core/src/consensus/rigor_consensus_lock.h` |
| **Honesty** | `equivalence_claimed = false`; not a ConnectBlock refinement |

This page records what the Coq development proves about RFC-018. It does not
claim runtime–Coq equivalence for the full node.

---

## Parameters (unchanged)

| Name | Value |
|------|-------|
| `ORU_RES_TRANSFER_FEE_BURN_NUM` | `1` |
| `ORU_RES_TRANSFER_FEE_BURN_DEN` | `2` (50% of RES-transfer ORU fees) |
| `RES_COIN` | `1_000_000` (six decimals) |
| `RES_ATOMIC_UNIT` | `1` |

No consensus parameter was retargeted. Shipping comments/tests that still used
the retired `1 RES = 100` atom scale were corrected to six decimals.

---

## Named properties

| # | Property | Coq theorem | Check |
|---|----------|-------------|-------|
| 1 | Fee-burn determinism: same tx, same class ⇒ same ORU burn | `fee_burn_determinism` | Coq + share pins |
| 2 | Circulating ORU change = subsidy − destroyed fee | `supply_conservation` | Coq + mixed-block vector |
| 3 | RES isolation: fee burn cannot mint/burn RES or alter the cap | `res_isolation` / `res_isolation_block` | Coq + isolation vector |
| 4 | Reorg: rollback restores ORU supply and RES balances | `reorg_safety` | Coq + rollback vector |
| 5 | Replay: same block body ⇒ same burn | `replay_safety` | Coq + replay vector |
| 6 | Precision: `1 RES = 1_000_000` atoms; display six decimals | `precision_consistency` | Coq + C++ `FormatResAmount` |

C++ `m_chain_oru_total` remains the RFC-002 ledger scalar
`S' = S + subsidy − converge` and does **not** subtract fee burns (RFC-018 §5).
The circulating (UTXO) view proved here is `≤` that scalar whenever a burn
share or voluntary underclaim occurs (`circulating_le_ledger`).
Supply-layer names: `fee_burn_cannot_enter_circulating_supply`,
`cvg2_locked_outputs_excluded_from_circulating`,
`wallet_balance_matches_spendable_utxo_model` in `CirculatingSemantics.v`.

---

## Display (wallet / explorer / RPC)

| Surface | Implementation | Expected |
|---------|----------------|----------|
| RPC / explorer `value` | `ValueFromAmountAsset` → `FormatResAmount` | six fractional digits |
| Wallet Qt | `BitcoinUnits::formatRes` (`num_decimals = 6`) | same |
| Explorer rules | `docs/explorer/RES_INDEXING_RULES.md` | `RES_COIN = 1_000_000` |

Pins: `1 RES` → `1.000000`; `0.1 RES` → `0.100000`.

---

## Remaining Admits / Axioms

**None** in `fee-burn/*.v`.

---

## Not claimed

- `core/src/validation.cpp` ConnectBlock = Coq `apply_block_circulating`
- Classification `IsORBClaimTx` as a Coq predicate over scripts
- Folding fee burns into `m_chain_oru_total`
