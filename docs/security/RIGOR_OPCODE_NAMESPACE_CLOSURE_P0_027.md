# RIGOR Opcode Namespace Closure (P0-027)

Task: `TASK-RIGOR-NATIVE-PREDICATE-UPGRADE-SURFACE-CLOSURE-P0-027`  
Phase 1 audit. Live trees only: `core/src/script/`, `core/src/wallet/`, `core/src/test/` (not `archive_disabled*`), `formal_verification/rigor_v01/`.

No ScriptPubKey bytes, markers, sighash, maturity, or ConnectBlock logic were changed.

## Production shape (unchanged)

```
UTXO.scriptPubKey
        |
        v
ClassifyRigorTxout          unique classifier
        |
        v
RigorPredicateID            closed registry (P0-027 names)
        |
        v
VerifyRigorPredicate        dedicated verifier
        |
        v
Ed25519  or  fail closed
```

## Scan classification

| Hit | Type | Action |
|-----|------|--------|
| `RIGOR_MARKER_RETURN` `0x6a` | byte marker | keep / already renamed |
| `RIGOR_MARKER_RES_MINT` `0xbb` | byte marker | keep |
| `RIGOR_MARKER_CONVERGE` `0xbd` | byte marker | keep |
| `RIGOR_RESERVED_LEGACY_CHECKSIG` `0xac` | reserved inert | keep name; not a live marker |
| `RIGOR_RESERVED_LEGACY_CHECKMULTISIG` `0xae` | reserved inert | keep name; not executable |
| `RIGOR_RESERVED_LEGACY_HASH160` / `EQUALVERIFY` / `CAT` / `NOP*` | reserved inert | parser/display only |
| `EvalPushOnlyScript` | push parser | keep; alias `ParseRigorScriptTemplate` |
| `EvalScript` / `interpreter.cpp` / `ExecuteOpcode` | dead VM | already deleted (P0-026) |
| `GetOpNameAlias` short names (`ELSE`, `RES_MINT`) | historical compatibility | keep for ParseScript roundtrip; not a VM |
| `TxoutType::PUBKEY` / `PUBKEYHASH` / `SCRIPTHASH` / `MULTISIG` | dead enum arms | unreachable from classifier; IsMine no longer switches on them |
| `test/script_tests.cpp` P2PK/P2PKH fixtures | test only | negative tests (`SCRIPT_ERR_RIGOR_SCRIPT_TYPE`) |
| `core/test/functional/archive_disabled_bitcoin_tests/` | archive | out of live corpus |
| `core/doc/release-notes/` | history | out of live corpus |

## Live identifier freeze (word-boundary)

| Pattern | Live `core/src` + live tests + `rigor_v01` |
|---------|-------------------------------------------|
| `OP_CHECKSIG` | **0** |
| `EvalScript` | **0** |
| `VerifyScript` (function) | **0** |
| `ExecuteOpcode` / `ScriptMachine` / `OpcodeInstruction` | **0** |

`VerifyScriptBench` is a benchmark wrapper around `VerifyRigorPredicate`.

Absence of a VM is enforced by `core/test/lint/lint-no-script-vm.py` (fails the build if `EvalScript` / `ExecuteOpcode` / `popstack` / `ConditionStack` / `VerifyScript` reappear). The Coq modules in `RigorOpcodeNamespace.v` / `RigorUpgradeBoundary.v` specify the byte partition and that `PredFuture` is never classified; they are not a C++ refinement and do not prove `false = false`.

## Wallet

`LegacyScriptPubKeyMan::IsMine` uses `LookupRigorPredicate` then `HaveKey(program)`. PUBKEY / P2PKH / MULTISIG / SCRIPT_HASH arms are gone. Descriptor wallets still match cached scriptPubKeys (the live templates).

## Tools

`rigor-tx outscript` rejects any script that is not `IsRigorConstructibleScript` (CHECKSIG / P2SH / P2WSH / Taproot templates fail closed). Descriptors already reject `pk` / `pkh` / `sh` / `wsh` / `tr`.
