# High-risk global state fence (W1 / W2 / W7) + W5 script-context cleanup

**Task:** `TASK-ARCH-NO-GLOBAL-FENCE-001`  
**Date:** 2026-08-20  
**Consensus impact:** **NONE** (same inputs ⇒ same accept/reject)  
**Honesty:**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This fence does **not** rewrite mint authorization, the draw inequality `σ < T · f_i`, script lock/unlock, or dual-pool conservation. It de-privileges extra-UTXO ledgers and removes a height-keyed observational cache so they cannot be mistaken for spendable balances or consensus authority.

Related: W-ids originate in the 2026-08-20 UTXO / no-global-state architecture audit (W1 issuance scalars, W2 cumulative CVG2, W7 height-keyed witness map).

---

## Observational-only inventory

| ID | Structure | Role after this fence | Still a ConnectBlock reject? |
|----|-----------|----------------------|------------------------------|
| **W1** | `GetChainResMintTotal` / `GetChainOruTotal`; ObservationalMetrics ledgers | Path-fold **ledger** caches. Not spendable UTXO. Not T / f_i / mint-shape / script inputs. | **No.** `bad-rigor-res-max-supply` / `bad-oru-supply-negative` removed (clean slate; cap authority is Deterministic Reserve Issuance `ResScheduleSum()`) |
| **W2** | `m_cumulative_convergence` / CVGE `m_cumulative_convergence` | Audit / coinbase **commitment**. Prefix-sum of CVG2 amounts. | **Yes, unchanged:** `bad-convergence-cumulative` still compares parent + this block |
| **W7** | Runtime supply witness cache | Observational FVR-F record, keyed by **block hash** | **No.** Never an accept/reject predicate. NDEBUG parent-link `assert` uses parent hash, not height−1 |
| **W5** | Interpreter / `PrecomputedTransactionData` block context | **Removed.** No public API to inject `CBlockIndex` / `CBlock` / `T` / convergence root into `EvalScript` | **No.** Those fields were never read by `EvalScript`; mint stays `ConnectEvaluateResMintTx` |
| **W11** | `-reswalletbootstrap` / `gArgs` in ConnectBlock | Amount is `Consensus::Params::res_wallet_bootstrap_coinbase`, set at `CreateChainParams` | **Yes, unchanged:** `bad-cb-res-amount` still compares coinbase RES to `nFeesRes + bootstrap` |
| **W9** | Coq `Types.v` / `State.v` / `Transition.v` / `Selection.v` | Moved to `formal_verification/coq/legacy_account_model/` | **N/A** (docs/model only) |

Follow-on (this file): **W5** dead interpreter block context — `TASK-ARCH-SCRIPT-CONTEXT-CLEANUP-001`. **W11** mockable-chain bootstrap amount on `Consensus::Params`. **W9** account-model Coq moved to `legacy_account_model/`.

Not in this fence (left as previously classified): W3 tickets, W4 content-addressed caches, W6 mint-maturity flag, W8 Deflationary Fee Burn (no chain cumulative), W10 remaining Coq gap, Coq honesty flags remain `false`.

---

## W1 — Parallel issuance scalars

**Authority for the RES ceiling** is Deterministic Reserve Issuance `ResScheduleSum()` (`static_assert` in `rigor_consensus_lock.h`: schedule cannot exhaust `RES_MAX_SUPPLY` even if every block mints) plus the existing mint path (winner, uniqueness, amount `W(h)`).

`m_chain_res_mint_total` / `m_chain_oru_total` are **not**:

- spendable balances (those are coins in the UTXO set)
- circulating supply (`S_ORU` is issued − Convergence; circulating ORU is UTXO)
- inputs to `T`, `f_i`, PCORB mint authorization, or `EvalScript`

`bad-rigor-res-max-supply` and `bad-oru-supply-negative` are **removed**. Genesis-only / no historical datadir: those rejects were redundant given mint-shape checks + compile-time `ResScheduleSum()`. ObservationalMetrics is a post-validation indexed observation / explorer view only.

Rollback: revert the comments in `validation.cpp`, `chain.h`, `runtime_supply_state.h`, `rigor_consensus_lock.h`. No behavioral rollback needed (behavior was not changed).

---

## W2 — Cumulative burned ORU

`m_cumulative_convergence` is a committed running total of CVG2 amounts, written in the coinbase CVGE payload and checked as `parent + Σ C(this block)`.

**Allowed consensus use:** equality against that commitment (`bad-convergence-cumulative`), computed from the prefix-sum of per-block `m_block_converge_sum` (`GetChainConvergeSum`), not from ObservationalMetrics.

**Forbidden uses (fence):** reading the cumulative into:

- `T` (`RES_PROBABILITY_TARGET`)
- `f_i` (per-candidate `ConvergenceDrawCandidate::weight` = that tx’s CVG2 amount)
- mint authorization (`ConnectEvaluateResMintTx` / PCORB)
- script evaluation

RPC `FillTipConvergenceChainSummary` may display it as an observer projection.

Rollback: revert comments. Do not change the equality check without an accept/reject equivalence test.

---

## W7 — `g_witness_by_height` → hash index

Confirmed: the witness map is stored **after** ConnectBlock accept/reject decisions. Release builds do not read it to accept or reject a block. The former NDEBUG `assert` looked up `height - 1`, which is a fork-overwrite footgun, not a mainnet predicate.

**Change (observational cache only):**

- Key: `block_hash`, not height. Name: `g_witness_by_block_hash`.
- Height-only getters/invalidators **removed** (structural fence).
- ConnectBlock debug parent check uses `mirror.parent_hash`.
- Disconnect/reorg invalidates by `pindex` hash.
- RPC `getruntimesupplywitness` / `…byheight` resolve the **active chain** block, then load by hash.

Two forks at the same height no longer overwrite. No new process-global state was added; the existing map was rekeyed.

Rollback: restore height-keyed `g_witness_by_height` and the old RPC lookups. That would reintroduce the fork-overwrite bug. Prefer not to.

---

## W5 — Dead interpreter rigor context

`SetRigorResEvalContext` wrote parent, height, block, eval-tx, convergence root, and `T` onto `PrecomputedTransactionData`. `EvalScript` / `VerifyWitnessProgram` never read those fields (SECURITY-001-A7 already removed the 11-item PCORB script path). They were a future injection surface: a later change could start reading chain-global environment from the script checker.

**Removed (no EvalScript semantic change):**

- `PrecomputedTransactionData` rigor fields and `SetRigorResEvalContext`
- `RigorSigmaEvalSigningContext` and the `rigor_eval` argument on `SignTransaction`
- `CheckInputScripts` / `CheckInputsFromMempoolAndCache` parent / height / block / root / `T` arguments

Scripts remain a function of the spent UTXO and the spending transaction (sighash, `nLockTime`, `nSequence`). Mint authorization remains `rigor::ConnectEvaluateResMintTx` in ConnectBlock, **before** `CheckInputScripts`. PCORB, CVG2, and asset-class rules are untouched.

There is no remaining public entry that injects block-level rigor context into the interpreter.

Rollback: restore the dead fields and setters. That would re-open the injection surface. Prefer not to.

---

## W11 — Test-chain `gArgs` out of the consensus predicate

`VerifyCoinbaseConvergenceAndResLayout` (`bad-cb-res-amount`) used to call `DevTestResWalletBootstrap`, which read `gArgs.GetBoolArg("-reswalletbootstrap")` on the ConnectBlock path.

**Change (same accept/reject for a given `CChainParams`):**

- `Consensus::Params::res_wallet_bootstrap_coinbase` defaults to 0
- `CreateChainParams` / `RegTestOptions` / `MockableChainOptions` copy `-reswalletbootstrap` → `RES_COIN` only for mockable chains
- ConnectBlock reads the params field; `devtest_res_bootstrap.cpp` no longer includes `util/system.h` / `gArgs`
- Mainnet / testnet / signet remain 0 even if the CLI flag is set

Functional tests still pass `-reswalletbootstrap` at node start; `SelectParams` → `CreateChainParams` injects the amount. Unit tests inject via `ArgsManager` before `CreateChainParams` or via `RegTestOptions`.

Rollback: restore the `gArgs` read. That would put CLI back into a consensus predicate. Prefer not to.

---

## W9 — Account-model Coq kernel deprecated

`Types.v`, `State.v`, `Transition.v`, `Selection.v` now live under `formal_verification/coq/legacy_account_model/`. They model global supply scalars / list-of-tx blocks, **not** the C++ UTXO set. Researchers should start at `ReplayReorgCore/`. Honesty flags stay `false`.

---

## What this task did not change

- Draw inequality `σ < T · f_i` and weight source
- Mint authorization (winner, unique mint, `W(h)` amount)
- Script asset class / lock / unlock
- Dual-pool conservation
- Honesty flags (remain `false`)
- No new global mutable consensus state

---

## Tests

- `runtime_supply_witness_chain_tests`: hash lookup, latest pointer, **same-height forks do not overwrite**
- Existing FVR witness / CHAS / catalog unit tests updated to hash keys
- Cap pin is observational: `runtime_supply_invariant_tests` `res_cap_violation_fails_res_l2` (not a ConnectBlock reject)
- W5: existing `script_tests`, `rigor_witness_tx_tests`, `res_orum_tests`, `res_proof_math_tests`, `res_single_spend_path_tests`, `convergence_consensus_tests`, `validation_tests` — any script/mint result change is a fail-and-rollback
- W11: `res_orum_tests` / `w11_res_wallet_bootstrap_injected_at_chain_construction` — CLI injects `RES_COIN` on mockable chains, 0 on mainnet; `RegTestOptions` injection
- Guardrail: `python3 core/test/lint/lint-no-global-state.py` (also `core/test/lint/all-lint.py` and `.github/workflows/no-global-state-lint.yml`)
