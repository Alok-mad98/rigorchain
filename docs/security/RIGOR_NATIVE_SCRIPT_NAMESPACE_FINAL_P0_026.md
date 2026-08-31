# RIGOR Native Script Namespace — Freeze Report P0-026-V2

Task: `TASK-RIGOR-NATIVE-BYTE-NAMESPACE-AND-BITCOIN-IDENTITY-ERASURE-P0-026-V2`

## Verdict

| Gate | Value |
|------|--------|
| `BITCOIN_OPCODE_REFERENCE_COUNT` | **0** (live corpus) |
| `GENERAL_SCRIPT_VM` | **NO** |
| `LIVE_OPCODE_EXECUTION` | **0** |
| `PREDICATE_MODEL` | **YES** |
| `FUTURE_UPGRADE_SURFACE` | **RESERVED** (vacant bytes `0xbe`–`0xfe` are `INVALID`, fail closed; not NOP) |

## Live corpus

Counted with word-boundary grep on:

- `core/src/`
- `core/test/` excluding `archive_disabled_bitcoin_tests/`
- `formal_verification/rigor_v01/`

Not counted (history only):

- `core/test/functional/archive_disabled_bitcoin_tests/`
- `core/doc/release-notes/`
- prior P0-025 / P0-026 mapping audits that document the old names

`docs/security/RIGOR_OPCODE_IDENTITY_ERASURE_AUDIT_P0_026.md` records the pre-erasure names as a scan table; it is not live consensus code.

## Production shape

```
UTXO.scriptPubKey
        |
        v
ClassifyRigorTxout          unique classifier
        |
        v
RigorPredicateType          RES_MINT | WITNESS_V0_ORU | WITNESS_V1_RES | UNKNOWN
        |
        v
VerifyRigorPredicate        switch on predicate, not on a byte
        |
        v
Ed25519  or  fail closed
```

Entry files:

- namespace: `core/src/script/rigor_bytes.h` / `rigor_bytes.cpp`
- classify: `core/src/script/rigor_classify.cpp`
- spend: `core/src/script/predicate_verify.cpp` (`VerifyRigorPredicate`)

## Frozen bytes (not changed)

| Item | Value |
|------|--------|
| RETURN marker | `0x6a` |
| RES mint marker | `0xbb` |
| CONVERGE marker | `0xbd` |
| burned | `0xbc` |
| witness v0/20 and v1/32 encodings | unchanged |
| sighash domains / Ed25519 | unchanged |
| mint maturity | unchanged |
| ConnectBlock | unchanged |

`ClassifyRigorByte` maps `0xbe`–`0xff` and `0xbc` to `INVALID`. Future predicates are new ScriptPubKey templates activated by a coordinated fork, not by executing reserved or vacant bytes.

## Wallet

RES ownership is `HaveKey` on the 32-byte program plus the live v1 scriptPubKey in the keystore. The historical CHECKSIG subscript is gone.

## Tests / binaries

- unit binary: `test_rigor`
- Boost module: `RIGOR Test Suite`
- util: `rigor-util`

## Formal

`formal_verification/rigor_v01/RigorByteNamespace.v` compiled (`coqc -R . Rigor -R ../rigor_v01 RigorV01`):

- `byte_class_complete`
- `no_execution_loop` / `no_stack_transition` / `no_opcode_semantics`
- `unknown_predicate_not_spendable`

## Build notes

`Makefile.am` lists `script/rigor_bytes.cpp` and `script/predicate_verify.cpp` in consensus and kernel. `Makefile.in` was updated in place; after pulling, `./autogen.sh && ./configure` is the clean regenerate path.
