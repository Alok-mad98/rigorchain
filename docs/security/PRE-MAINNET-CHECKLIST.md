# Pre-mainnet checklist (operator entry)

**Date:** 2026-08-18  
**Tree:** `~/rigor-official-tree`  
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

## C. Consensus / economic security suites

**One command** (wallet-enabled `rigord`/`bitcoind` build; from `core/` or via the script’s `cd`):

```bash
cd ~/rigor-official-tree/core
./test/functional/res_security/run_pre_mainnet.sh
```

Fail-fast. Extra args are forwarded to each Python test (tests already pass `--descriptors`). Help: `./test/functional/res_security/run_pre_mainnet.sh --help`.

| Order | Suite | Script | Report |
|-------|-------|--------|--------|
| 1 | Window / reorg / RES monopoly | `feature_res_window_reorg_res_attacks.py` | [PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md](./PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md) |
| 2 | Solo-mining path (no pool) | `feature_res_solo_security.py` | [PRE-MAINNET-SOLO-SECURITY.md](./PRE-MAINNET-SOLO-SECURITY.md) |
| 3 | RES mint + ORU coinbase maturity | `feature_res_maturity.py` | [RES-ORU-MATURITY.md](./RES-ORU-MATURITY.md) |

Harness notes: `core/test/functional/res_security/README.md`. Matrix fragments: [SECURITY_TEST_MATRIX.md](./SECURITY_TEST_MATRIX.md).

**Required build:** configure Core with wallet, `make` so the functional framework can start `rigord`. Chain is **devtest** (mockable PoW). No public network.

Do **not** add pool tests to this runner.

---

## D. Unit / build (security gates — not the FV gate)

From `core/` (after `make`). The suite list is `SECURITY_UNIT_SUITES` in
`core/test/functional/res_security/run_pre_mainnet.sh` — do not maintain a
second copy here.

```bash
./test/functional/res_security/run_pre_mainnet.sh
# or, units only:
./src/test/test_rigor --run_test=res_orum_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests,res_proof_math_tests,convergence_root_tests
```

| Suite | Why it is a security gate |
|-------|---------------------------|
| `pow_tests` | ASERT / `CheckProofOfWork`. **NOT VERIFIED** in Coq (no PoW module). |
| `sighash_tests` | P0-018 Ed25519 domain + `rigor_sighash_p0_018.json`. Not a Coq theorem. |
| `script_asset_ownership_freeze_tests` | Script-as-asset classifier. Shared JSON `rigor_script_asset_classifier.json` ≡ Coq `ScriptAsset` harness. |
| `res_proof_math_tests` | Mint evaluator: owner / extra mint / omit (evaluator allows omit) / no winner / W=0. Coq `winner_must_mint` is **not** CI-033. ConnectBlock `bad-rigor-res-winner-mismatch` is `feature_rigor_convergence_rng_consensus.py`. |
| `convergence_root_tests` | Coin-view wrapper ≡ eligibility-list root (including N=0). |
| `res_orum_tests` | Dual-pool + mint maturity. |

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
| Hashrate share α with matching burns over long windows | A-9-C removes exclusivity, not proportional share |
| Live path to exact RES cap (21e6) | Cap is unit / ConnectBlock pinned; cannot fund cap in a functional test |
| Network / mempool beyond covered cases | Consensus cannot prove censorship of a never-received tx |

Theme reports list further residuals (stale GBT sibling submit, wallet vs consensus confirmation off-by-one). **Pool is out of scope**, not a residual of a shipped component.

---

## F. Testnet observation (manual)

Not automated. Record notes before a mainnet declaration:

- [ ] Solo mine; confirm convergence participation (tickets in F / GBT).
- [ ] On a real mint: reject RES spend at depth `< nCoinbaseMaturity`; accept at maturity (same SSOT as coinbase, typically 100). Do not rely on wallet `res.trusted` alone.
- [ ] Restart node; short reorg smoke (`invalidateblock` / competing tip as available).

---

## G. Go / No-Go (fill at cut time)

**Solo theme (this checklist):**

- [ ] A — scope is solo; pool not shipped.
- [ ] B — differential gate PASS; honesty flags false.
- [ ] C — `run_pre_mainnet.sh` PASS (window + solo + maturity).
- [ ] D — `run_pre_mainnet.sh` unit step PASS (`SECURITY_UNIT_SUITES`), or the same Boost filter run by hand.
- [ ] E — residuals acknowledged, not treated as closed.
- [ ] F — testnet notes attached.

**Solo Go** only if A–F are complete for this cut. Theme Go statements: window [report](./PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md), solo [report](./PRE-MAINNET-SOLO-SECURITY.md), maturity [report](./RES-ORU-MATURITY.md).

**Full mainnet declaration:** requires this checklist complete **plus** testnet notes (F) and whatever ops/release items live outside this security page (seeds, signed binaries, soak). This checklist does **not** by itself authorize a mainnet flag day.

---

## Entry-point inventory

| Path | Role |
|------|------|
| `docs/security/PRE-MAINNET-CHECKLIST.md` | **This file** — operator checklist |
| `core/test/functional/res_security/run_pre_mainnet.sh` | Primary functional runner (window + solo + maturity) |
| `core/test/functional/res_security/run_suite.sh` | Window suite only |
| `core/test/functional/res_security/run_solo_suite.sh` | Solo suite only |
| `core/test/functional/feature_res_window_reorg_res_attacks.py` | Window / reorg / monopoly |
| `core/test/functional/feature_res_solo_security.py` | Solo security |
| `core/test/functional/feature_res_maturity.py` | Maturity |
| `./src/test/test_rigor --run_test=res_orum_tests,pow_tests,sighash_tests,script_asset_ownership_freeze_tests,res_proof_math_tests,convergence_root_tests` | Security unit gates (PoW / sighash / mint-auth / classifier / root wrapper). Not the FV gate. |
| `formal_verification/scripts/run_differential_gate.sh` | FV maintenance gate (not in the functional runner) |
