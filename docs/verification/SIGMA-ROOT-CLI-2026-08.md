# Test-only σ / Convergence-Root CLI (2026-08)

**Mode**: Gate follow-up. Not a refinement proof.  
**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md).  
Current freeze: [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md).

**Honesty (unchanged):**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

---

## What is checked

Portable test driver `formal_verification/extracted/cpp_sigma_root_cli` (built by `scripts/build_cpp_sigma_root_cli.sh`) links production:

| Mode | Production function | Source linked |
|------|---------------------|---------------|
| `sigma` | `rigor::ComputeSigmaHash` | `core/src/consensus_rigor/sigma_hash.cpp` |
| `root` | `Consensus::ComputeConvergenceRootFromEligibilityTxids` | `core/src/consensus/convergence_root_eligibility.cpp` |

Hashing is portable `CSHA256` from `core/src/crypto/sha256.cpp` (no `USE_ASM` / SIMD), same as `cpp_hash_cli`.

Gate: `run_differential_gate.sh` step 4 runs `tests/test_sigma_root_cli.py` against existing golden families:

| Family | What the CLI must match |
|--------|-------------------------|
| `SigmaEncoder` (10) | 32-byte σ **and** 100-byte preimage (`txid ‖ C ‖ B_{h-1} ‖ LE32(h)`) |
| `SigmaPoW01` (50) | 32-byte σ digest |
| `ConvergenceRoot` (25) | 32-byte root; `n < 2` → 32 zero bytes **before** hashing |

Hex on the CLI is raw `uint256` memory order (Python `bytes.hex()`), including `encoding: sethex` rows after the same decode the Python oracle uses.

Build (no `bitcoind` / `test_bitcoin` required):

```bash
bash formal_verification/scripts/build_cpp_sigma_root_cli.sh
./formal_verification/extracted/cpp_sigma_root_cli sigma [--preimage] <txid> <croot> <prev> <height>
./formal_verification/extracted/cpp_sigma_root_cli root [<txid> ...]
```

The pure root function was moved to `convergence_root_eligibility.cpp` so this CLI does not compile coins/script. Node / kernel still link that same `.cpp` (`Makefile.am`). Behavior is unchanged.

---

## What this is still not

- **Not refinement.** Finite JSON vs one C++ binary. No mechanized `Coq = C++`.
- **Not a shared hash object.** Portable Transform only; node SIMD still goes through Core `SelfTest`.
- **Not** `ComputeConvergenceRootFromConvergenceSet` (coin view / `ListConvergenceRootCommitmentTxids`).
- **Not** winner selection, window, mempool, P2P.
- Coq extract of `sigma_preimage_bytes` / `convergence_root_from_txid_bytes` is **deferred** (hash extract already exists; encoder/root extract is optional).
- Stale `test_bitcoin` can still miss BOOST-only suites; σ and root for these families no longer depend on rebuilding it.

---

## Residual

Encoder preimage printed by `--preimage` is assembled in the CLI with the public `SIGMA_OFF_*` constants from `sigma_hash.h` (the same constants `ComputeSigmaHash` uses). σ itself is the production function. Root `std::sort` / `n < 2` zeros are the production function. Coq `OrderIndependence` remains a model theorem.
