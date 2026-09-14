# Binary Artifact Index

Generated executable artifact for testing. Not a proved executable
implementation. Not a node, wallet, network stack, or cryptographic
library.

```
Artifact:

rigor-kernel-checker


Generated from:

ExtractedKernel.ml


Role:

generated executable artifact for testing


Not:

proved executable implementation
```

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`BINARY_SEMANTIC_EQUIVALENCE=false`

## Build

```
cd formal_verification/rigor_kernel
make
./extraction/build.sh
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh
```

| Item | Path |
|---|---|
| Artifact | `kernel_binary/rigor-kernel-checker` |
| Extracted source | `extraction/ExtractedKernel.ml` |
| Driver | `kernel_binary/main.ml` |
| Trust manifest | `kernel_binary/TRUST_MANIFEST.md` |

## Golden vectors

Positive and negative cases. A stuck-`VALID` checker must fail.

| Vector | Domain | Expect | Role |
|---|---|---|---|
| `utxo_valid.json` | UTXO | VALID | happy path |
| `utxo_invalid.json` | UTXO | INVALID | empty inputs |
| `utxo_over_max.json` | UTXO | INVALID | `MAX_MONEY + 1` (fits in 63-bit `int`) |
| `oru_conservation.json` | ORU | VALID | happy path |
| `oru_invalid.json` | ORU | INVALID | `oru_out > oru_in` |
| `res_schedule.json` | RES | VALID | happy path |
| `res_invalid.json` | RES | INVALID | non-coinbase mint |
| `script_classification.json` | Script | VALID | happy path |
| `pow_target.json` | PoW | VALID | happy path |
| `pow_invalid.json` | PoW | INVALID | digest above target |
| `asert_negative_time.json` | ASERT | VALID | small negative `te` |
| `asert_invalid.json` | ASERT | INVALID | wrong expected `q` |

Harness result `BINARY_VECTOR_TESTS_PASS=TRUE` means the generated
artifact matched these declared expectations. It does **not** mean
the binary is proved equivalent to the Coq validators.

The binary contains the six `check_*` validators. It does **not**
contain a node, network, wallet, database, or crypto primitive
implementation. See `kernel_binary/TRUST_MANIFEST.md`.
