# Binary Negative Vector Report

INVALID-001 … INVALID-005 against `rigor-kernel-checker`.

These tests prove only:

> the binary harness detects obvious corruption

They do **not** prove binary semantic equivalence.

`BINARY_SEMANTIC_EQUIVALENCE=false`

## Results

| Vector id | Input | Expected | Actual | Failure reason |
|---|---|---|---|---|
| INVALID-001 | UTXO output `MAX_MONEY+1` = `10^18+1` | INVALID | INVALID | UTXO overflow: value outside `money_range` |
| INVALID-002 | ORU `oru_out=10 > oru_in=7` | INVALID | INVALID | ORU hidden inflation |
| INVALID-003 | RES `mint_amount=40000001 > W(0)=40000000` | INVALID | INVALID | RES cap violation |
| INVALID-004 | ASERT `te=-1000000`, `tau=43200`, `expected_q=0` | INVALID | INVALID | Extreme negative time with wrong (non-negative) quotient |
| INVALID-005 | PoW `compact_t=-1` | INVALID | INVALID | Compact target invalid encoding |

`NEGATIVE_VECTOR_TEST_COMPLETE=TRUE`
`NEGATIVE_VECTORS_RUN=5`

Paths: `kernel_binary/vectors/invalid/INVALID-00N.json`.
Harness: `kernel_binary/run_invalid_vectors.sh`.
