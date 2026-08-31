# P0-020 Implementation Plan (pre-code)

Task: `TASK-RIGOR-SCRIPT-ASSET-CONSENSUS-ROOT-FREEZE-IMPLEMENTATION-P0-020`  
Audit accepted: P0-019. Option B: `RES v1 program = pk32`.

## 1. Plan

1. Change `ed25519::ResWitnessProgram` to copy `pk32` (identity). Spend checks `pk == program`, then Ed25519 verify under that key. Witness stack stays `<sig><pk>`.
2. Rename mint/converge helpers to `*ForPubKey`. Comments and GBT: 20-byte = `HASH160(pk32)` = `HASH160(v1 program)` because program is pk.
3. Wallet send: only `AssetTypeFromScriptPubKey`. Delete version→asset helpers.
4. Explorer/indexer/Python: C++ table including `INVALID`. No unknown→NONE, no v0/32→ORU, no RES2→RES.
5. Delete `IsORBMintTx`, `IsV1Program`. Keep implied 25-byte P2WPKH template in the classifier for signing only (maps to `INVALID` on chain). Restrict pool payout classifier to allowed scripts. **Follow-up:** the 25-byte template was removed from `ClassifyRigorTxout`; P2WPKH `IsMine` uses `HaveKey` directly. `combo()` migration now emits `wpkh()`.
6. Negative unit tests for SHA256-program mint stuckness, HASH160 mismatch, unknown scripts.
7. Docs + freeze declaration. Coq: Option B comments; document remaining `SmwNone` vs `INVALID` gap if lemmas cannot take a fourth kind in this pass.

## 2. Files (primary)

Consensus/crypto: `ed25519_provider.*`, `interpreter.*`, `pubkey.*`, `res_proof.*`, `res_owner.h`, `script.h`, `rigor_classify.cpp`, `standard.*`, `tx_verify.cpp`, `convergenceres.cpp`, `rpc/mining.cpp`

Wallet/descriptor: `asset_domain.*`, `rpcspend.cpp`, `spend.cpp`, `scriptpubkeyman.cpp`, `descriptor.cpp`, `sign.cpp`

Explorer: `script.ts`, tests, importer type if needed

Tests: new freeze tests; update witness/descriptor/convergence fixtures

Formal: `rigor_consensus_spec.py`, `ScriptMintWitness.v` comments

Docs: freeze declaration, script registry, wallet UX spec

## 3. Consensus impact

**Hard fork vs current tree.** Relative to launched mainnet: none (no history).

No `CTxOut.asset_type`. No new primitives. CVG2/RES2 remain uncreatable.

## 4. Test strategy

Unit: `test_bitcoin` — witness, descriptor, mint owner, new freeze suite.  
Functional: targeted script/wallet if time; full suite if unit is green.  
Fuzz: compile `res_mint_eval` / `script` if the binary exists.  
Formal: Python spec import; Coq comments (full re-proof of ReplayReorgCore is a remaining risk if classifier lemmas still map CVG2→ORU).

## 5. Execution results (not committed)

Shipped Option B: `ResWitnessProgram` copies `pk32`; spend requires `pk == program` then Ed25519 verify. Single classifier remains `rigor_classify.cpp`. Explorer/indexer/Python/pool payout consume the same admit-set (`INVALID` fail-closed). Freeze declaration and script registry no longer say `SHA256(pk)`.

**Passed:** `script_asset_ownership_freeze_tests` and related `test_bitcoin` suites (script, descriptor, RES spend, wallet send parity after `wpkh()` helper); explorer `script.test.ts`; pool `block-assembler` payout tests; Python `test_p0_018_script_class`; fuzz objects `res_mint_eval` / `script` / `res_checktxinputs` compile; functional `feature_res_descriptor_signing`, `feature_res_maturity`.

**Not a full Coq re-proof.** `ScriptMintWitness.v` comments record Option B and the remaining `SmwNone` vs `ASSET_INVALID` / CVG2→ORU model gap.

## 6. Remaining risks (before commit)

1. Coq replay still treats unknown scripts as `SmwNone` and CVG2 as `SmwORU`. C++ is fail-closed `ASSET_INVALID`. Do not equate the two.
2. This configure has `ENABLE_FUZZ` off; fuzz objects compiled, libFuzzer binary not run.
3. Full functional suite not run. `feature_rigor_dual_native` failed in this environment because Python `hashlib.ripemd160` is unavailable (OpenSSL 3), not because of Option B.
4. Hard fork versus the previous tree (Option A `SHA256(pk)`). No mainnet history.
