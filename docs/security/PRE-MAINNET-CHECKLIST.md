# Pre-mainnet checklist (operator entry)

**Date:** 2026-09-14 (aligned to RFC-020; original operator cut 2026-09-10)  
**Issuance:** RFC-020 body-tx destroy-to-mint at `destroy_cost(h)`. ORU subsidy is 0 at genesis and **128 ORU** forever after. `mainnet_launched = false`.
**Tree:** this repository
**This page** is the single operator entry for pre-mainnet **security gates**.  
It does not add protocol work. Detailed results live in the linked reports.

**Release theme:** solo mining only. Pool / stratum / web are **not shipped**.

---

## A. Release scope

- **In:** `rigord` / wallet / `getblocktemplate` / `submitblock` / docs.
- **Out:** pool, stratum, YiiMP, pool web. No pool dependency in the shipped artifact set.
- Scope lock: [PRE-MAINNET-SOLO-INVENTORY.md](./PRE-MAINNET-SOLO-INVENTORY.md).

---

## B. Formal verification (maintenance only)

Honesty flags stay **false** (`equivalence_claimed`, `hash_kernel_shared_binary_with_cpp`, `refinement_proved`). This is **not** a refinement proof. Do not expand FV scope here.

PoW / ASERT / `CheckProofOfWork`, Ed25519 sighash, and production mint authorization are **NOT VERIFIED** in Coq. A green differential gate does **not** replace §D unit suites.

From repo root:

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

Expected: `differential gate: PASS` and the three honesty flags printed as false.

- Status: [../verification/FORMAL-VERIFICATION-STATUS.md](../verification/FORMAL-VERIFICATION-STATUS.md)
- Freeze ops: [../verification/FORMAL-VERIFICATION-FREEZE.md](../verification/FORMAL-VERIFICATION-FREEZE.md)
- Do **not** treat [../verification/FORMAL-VERIFICATION-VERDICT.md](../verification/FORMAL-VERIFICATION-VERDICT.md) as rewritten by this checklist.

Optional `COQ=1` / full Coq rebuild is **not** part of the functional runner below.

---

## C. Consensus / economic security suites (RFC-020)

The retired `res_security/` window / reorg / solo-draw runner is **not** a
shipping gate. RFC-020 mint is a body-tx destroy-to-mint at `destroy_cost(h)`;
those suites expect σ, a 32-block window, and a coinbase prize and must
not be treated as live.

**Functional (wallet-enabled `rigord`; from `core/`):**

```bash
cd core
test/functional/test_runner.py \
  feature_createconvergence_mint.py \
  feature_res_destroy_cost_mint.py \
  feature_rigor_convergence_rpc_status_history.py
```

| Order | Suite | Script |
|-------|-------|--------|
| 1 | Wallet mint path | `feature_createconvergence_mint.py` |
| 2 | Mint predicate | `feature_res_destroy_cost_mint.py` |
| 3 | Convergence RPC status | `feature_rigor_convergence_rpc_status_history.py` |

**Required build:** configure Core with wallet, `make` so the functional
framework can start `rigord`. Chain is **devtest** (mockable PoW). No public
network.

Do **not** add pool tests. Do **not** treat a red `res_security/` draw suite
as a release blocker.

Also from repo root:

```bash
./scripts/refactor_guard.sh
```

Expected: L1–L7 PASS. `--full` is optional and is not this checklist’s gate.

---

## D. Unit / build (security gates — not the FV gate)

From `core/` (after `make`). Use the Boost filter below. The retired
`res_security/run_pre_mainnet.sh` `SECURITY_UNIT_SUITES` list is not this gate.

```bash
./src/test/test_rigor --run_test=res_issuance_tests,rigor_fee_tests,res_single_spend_path_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests
```

| Suite | Why it is a security gate |
|-------|---------------------------|
| `res_issuance_tests` | RFC-020 `destroy_cost(h)` pins, `CheckResMintArithmetic`, cap `2²²` RES, flat 128 ORU. |
| `rigor_fee_tests` | RFC-018 half-fee burn. Unchanged ORU arithmetic. |
| `res_single_spend_path_tests` | RES spend path after a body mint. |
| `pow_tests` | ASERT / `CheckProofOfWork`. **NOT VERIFIED** in Coq (no PoW module). |
| `sighash_tests` | P0-018 Ed25519 domain + `rigor_sighash_p0_018.json`. Not a Coq theorem. |
| `script_asset_ownership_freeze_tests` | Script-as-asset classifier. Shared JSON `rigor_script_asset_classifier.json` ≡ Coq `ScriptAsset` harness. |

Retired draw-unit suites (`res_proof_math_tests` winner/omit, `convergence_root_tests`
eligibility root) are **not** this gate.

Related (maturity undo flag): `--run_test=coins_tests/txundo_formatter_res_mint_flag_roundtrip`.

`run_pre_mainnet.sh` runs `SECURITY_UNIT_SUITES` when `src/test/test_rigor` exists; `SKIP_UNIT=1` skips it. Missing binary is a skip, not a functional-suite failure.

The FV checklist defaults to `SKIP_TEST_BITCOIN=1`. Do **not** treat a green FV gate as a substitute for the suites above.

Project-standard unit suite (`make check` / default `test_rigor` without a filter) as applicable for the release cut → **PASS**.

---

## E. Known residual risks (not closed)

Do not treat these as PASS because the suites above are green:

| Residual | Notes |
|----------|--------|
| P2P eclipse / network-level withholding | Out of suite scope |
| Hashrate share α with matching burns | Out of suite scope; RFC-020 has no draw window |
| Live path to exact cap (`419430400000000` atoms) | Cap is a compile-time identity; cannot fund it in a functional test |
| Network / mempool beyond covered cases | Consensus cannot prove censorship of a never-received tx |

Theme reports list further residuals (stale GBT sibling submit, wallet vs consensus confirmation off-by-one). **Pool is out of scope**, not a residual of a shipped component.

---

## F. Testnet observation (manual)

Not automated. Record notes before a mainnet declaration:

- [ ] Solo mine; confirm a body-tx mint (`createconvergence` / `OP_CONVERGE` + Witness v1 RES).
- [ ] On a real mint: reject RES spend at depth `< nCoinbaseMaturity`; accept at maturity (typically 100; reorg protection, not a draw wait). Do not rely on wallet `res.trusted` alone.
- [ ] Restart node; short reorg smoke (`invalidateblock` / competing tip as available).

---

## G. Go / No-Go (fill at cut time)

**Solo theme (this checklist):**

- [ ] A — scope is solo; pool not shipped.
- [ ] B — differential gate PASS; honesty flags false.
- [ ] C — RFC-020 functional trio PASS (`feature_createconvergence_mint.py`, `feature_res_destroy_cost_mint.py`, `feature_rigor_convergence_rpc_status_history.py`).
- [ ] D — unit filter PASS (`res_issuance_tests`, `rigor_fee_tests`, `res_single_spend_path_tests`, plus PoW / sighash / classifier). `refactor_guard.sh` L1–L7 PASS.
- [ ] E — residuals acknowledged, not treated as closed.
- [ ] F — testnet notes attached.

**Solo Go** only if A–F are complete for this cut. This page does **not**
authorize a “refactor complete” or mainnet flag day.

**Full mainnet declaration:** requires this checklist complete **plus** testnet notes (F) and whatever ops/release items live outside this security page (seeds, signed binaries, soak). This checklist does **not** by itself authorize a mainnet flag day. `mainnet_launched` stays false until that declaration.

---

## Entry-point inventory

| Path | Role |
|------|------|
| `docs/security/PRE-MAINNET-CHECKLIST.md` | **This file** — operator checklist |
| `core/test/functional/feature_createconvergence_mint.py` | Wallet body-tx mint |
| `core/test/functional/feature_res_destroy_cost_mint.py` | `CheckResMintArithmetic` |
| `core/test/functional/feature_rigor_convergence_rpc_status_history.py` | Convergence RPC status |
| `./scripts/refactor_guard.sh` | L1–L7 RES-boundary guard |
| `./src/test/test_rigor --run_test=res_issuance_tests,rigor_fee_tests,res_single_spend_path_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests` | Security unit gates. Not the FV gate. |
| `formal_verification/scripts/run_differential_gate.sh` | FV maintenance gate (not in the functional runner) |
