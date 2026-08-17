# Golden-vector discrepancies (post-verdict expansion)

**Current status**: [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)

**Date**: 2026-08-16
**Set**: `formal_verification/phase3_golden_vectors.json` (315 vectors, tag `hash-kernel-cpp-cli-2026-08-16`)

No byte-level disagreement was found between the Python spec, the Coq kernel (enforced subset plus `CanonicalHashKernel.v` FIPS lemmas), the extracted OCaml hasher, the portable C++ `CSHA256` CLI, and the generated C++ golden cases.

## Intentional splits (not bugs)

| Split | Why |
|---|---|
| 108 Coq-enforced vs 315 full set | SHA-256 `vm_compute` is expensive. Extra `SigmaPoW01` / `RunSelection` / `ConvergenceRoot` samples are `coq_enforce: false` and checked in Python + C++ only. FIPS single-SHA lives in `CanonicalHashKernel.v` rather than duplicating every FIPS row in the harness. |
| `SupplyScaffold` / `BlockTrace` / `WindowIncomplete` | Documentary or structural; they do not execute a kernel function on both sides. |
| `WindowExclusion` C++ cases filter hex strings | They check the A-9-C *rule* against the JSON. `FinalizeWindowedDrawSet` is covered separately by `convergence_consensus_tests.cpp` (`a9c_already_paid_*`). A divergence between the pure filter and the block-level builder would not be caught by the hex-filter cases alone. |
| Hash kernel | Coq `RigorSha256` and extracted `hash256_cli` are the same kernel. Portable C++ `cpp_hash_cli` is production `CSHA256` (C++ Transform). Python `hashlib` remains independent. `test_hash_kernel_tripartite.py` fails the gate on any split. SIMD paths are still Core `SelfTest`, not this gate. This is not a shared C++/Coq binary. |

## Encoding traps that *looked* like discrepancies and are not

1. **SetHex vs raw hash hex.** Selection golden tests mix `ParseHex256` (Bitcoin display / `SetHex`, reversed) for opaque txids and `HashHexToUint256` (forward hash bytes) for σ outputs. Encoder / `SigmaPoW01` expansion vectors store **raw** 32-byte hex (`bytes.hex()`), matching `txid.begin()` memcpy order. Mixing the two orientations is a test bug, not a consensus bug.
2. **Height endianness.** Height 256 is LE `[0, 1, 0, 0]`, not BE. Pinned by `le32_bytes_256` and `test_le32_height_256_not_be`.
3. **`target_prev`.** Not a σ field. `PCORBHash_target_prev_ignored` and `canonical_encoder_has_no_target_prev_slot` lock that.

## Residual risk

A C++ bug outside the hashed preimage and the `already_paid` filter — for example an off-by-one in `BuildConvergenceWindowTail` when walking undo data, or a signed height truncation — can still agree with every vector here and fork on mainnet. That is the Verdict §3.1 boundary.
