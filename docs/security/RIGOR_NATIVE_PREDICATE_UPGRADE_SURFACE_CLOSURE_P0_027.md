# RIGOR Native Predicate Upgrade Surface Closure (P0-027)

Task: `TASK-RIGOR-NATIVE-PREDICATE-UPGRADE-SURFACE-CLOSURE-P0-027`  
Date: 2026-08-27  
Tree: `rigor-official-tree`

This is the third-stage close of the Script Predicate architecture. P0-026 erased Bitcoin opcode identifiers and deleted the VM. This task names the opcode namespace, publishes the five-entry predicate registry, routes wallet `IsMine` through that registry, and records the upgrade boundary in Coq.

**No live predicate, ScriptPubKey byte, spend path, sighash, maturity, or ConnectBlock logic was modified.**

---

## Freeze gates

```
BITCOIN_OPCODE_EXECUTION_COUNT=0
LIVE_RIGOR_PREDICATE_COUNT=5
GENERAL_SCRIPT_VM=false          # enforced by lint-no-script-vm.py, not a Coq constant
FUTURE_EXTENSION_MODEL=PredicateRegistry
SOFT_FORK_OPCODE_EXTENSION=false # policy in the registry header, not a proved Boolean
HARD_FORK_PREDICATE_EXTENSION=true
FORMAL_ALIGNMENT=SPEC            # load-bearing proofs vs named specifications: see Formal
```

## Control flow

```
          ScriptPubKey
                |
                v
     AssetTypeFromScriptPubKey     (unchanged)
                |
                v
        RIGOR Predicate ID         LookupRigorPredicate
                |
                v
        Dedicated Verifier         VerifyRigorPredicate
                |
                v
             Ed25519
```

Unknown templates: no ID → `ASSET_INVALID` / `SCRIPT_ERR_RIGOR_SCRIPT_TYPE`.

## Live registry (v0.1)

| `RigorPredicateID` | Template | Asset | Spendable |
|--------------------|----------|-------|-----------|
| `ORU_V0_KEYHASH` | witness v0 / 20 | ORU | yes |
| `RES_V1_NATIVE_KEY` | witness v1 / 32 native | RES | yes |
| `RES_MINT_V0` | `<h> <20> 0xbb` | RES | after maturity |
| `CONVERGE_V0` | `<20> 0xbd` | ORU | **no** (burn) |
| `OP_RETURN_BURN` | `0x6a` + push-only | NONE | **no** (burn) |

Lookup is a total function of `ClassifyRigorTxout`. It does not re-match bytes.

## Opcode namespace

`core/src/script/rigor_opcode_namespace.h`:

| `RigorOpcodeClass` | Role |
|--------------------|------|
| `PUSH_MARKER` | data push |
| `ASSET_MARKER` | `0x6a` / `0xbb` / `0xbd` |
| `RESERVED_INERT` | historic Bitcoin bytes including `0xac` |
| `BURNED` | `0xbc` |
| `INVALID` | vacant / `0xff` |

`IsRigorInstruction` was a constant `false` and has been removed: a helper that cannot start returning true cannot enforce the absence of a VM. The namespace is the five-class partition above; `test/lint/lint-no-script-vm.py` fails the build if evaluator symbols (`EvalScript`, `ExecuteOpcode`, `popstack`, `ConditionStack`, `VerifyScript`) reappear under `core/src/script/` or `core/src/consensus/`. `0xac` is `RIGOR_RESERVED_LEGACY_CHECKSIG` and classifies as `RESERVED_INERT`.

There is no `enum OpcodeInstruction` and no `ExecuteOpcode()`.

## Interpreter / VM

`interpreter.cpp` is gone (P0-026). `EvalScript` is gone. Remaining parser:

- `EvalPushOnlyScript` / `ParseRigorScriptTemplate` — push opcodes + length only
- no conditionals, loops, stack mutation as a VM, arithmetic dispatch, or signature-opcode dispatch

## Wallet

`IsMineInner` keys off `LookupRigorPredicate` + `HaveKey(program)` (and the stored scriptPubKey). It does not recognize `<pk> CHECKSIG`, P2PKH, MULTISIG, or P2SH.

## Future upgrade

Allowed: `RIGOR_PREDICATE_V2` / post-quantum / new asset as a **new registry ID + dedicated verifier** (hard fork / coordinated activation).

Forbidden: `OP_NEW_SIGCHECK`, `OP_NEW_VM`, `OP_NEW_NOP`, treating `0xac` as executable, opening witness v2 as anyone-can-spend.

## Formal

These modules compile with `coqc` (no `Admitted`). That is not the same as a C++ refinement.

| File | Status | What it actually pins |
|------|--------|------------------------|
| `RigorPredicateRegistry.v` | **load-bearing** | `classified_spk_has_predicate`, `unknown_has_no_predicate`, `checksig_is_not_a_predicate` — quantified over `list byte` |
| `RigorWalletOwnership.v` | **load-bearing** | `ownable_scripts_are_classified`, `burn_outputs_are_never_ownable` — about `lookup_predicate`, not a `true`/`false` constant |
| `RigorOpcodeNamespace.v` | **specification** | `opcode_namespace_partition_is_frozen` (95/3/1/91/66). Matches the C++ partition test. Does **not** prove there is no evaluator |
| `RigorUpgradeBoundary.v` | **specification** | `future_id_never_classified`, `future_slot_not_live`. Upgrade-as-predicate-extension is policy in the C++ registry header |

The absence of a Script VM is a C++ fact, guarded by `core/test/lint/lint-no-script-vm.py`. Do not cite a Coq theorem named `no_byte_is_instruction` as that guarantee: the earlier form defined the predicate as `false` and proved `false = false`.

## Tests

- `test/rigor_opcode_namespace_tests.cpp`
- `test/rigor_predicate_registry_tests.cpp`
- `test/rigor_wallet_ownership_tests.cpp`

Must fail: CHECKSIG / CHECKMULTISIG / P2SH / P2WSH / Taproot-shaped / witness v2–v16 spends.  
Must succeed: ORU spend, RES spend, mature RES_MINT spend. CONVERGE and OP_RETURN are registered burns (lookup succeeds, spend fails).

## Unchanged consensus files

`consensus/tx_verify.cpp`, `validation.cpp`, ConnectBlock, `AssetTypeFromScriptPubKey`, Rigor sighash, Ed25519 verify, RES/ORU maturity: **logic not edited**.
