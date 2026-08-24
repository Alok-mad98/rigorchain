# Extracted SHA-256 kernel and portable C++ CLIs

`hash256_kernel.ml` is Coq `RigorSha256.sha256_bytes` / `hash256_bytes` extracted
to OCaml. `hash256_cli` hashes a hex payload. `cpp_hash_cli` is production
`CSHA256` (portable Transform from `core/src/crypto/sha256.cpp`).
`cpp_sigma_root_cli` is production `ComputeSigmaHash` plus
`ComputeConvergenceRootFromEligibilityTxids` (same portable Transform):

```bash
bash ../scripts/extract_hash_kernel.sh
bash ../scripts/build_cpp_hash_cli.sh
bash ../scripts/build_cpp_sigma_root_cli.sh
./hash256_cli sha256 ""
./cpp_hash_cli sha256 ""
./cpp_sigma_root_cli root
python3 ../tests/test_hash_kernel_tripartite.py
python3 ../tests/test_sigma_root_cli.py
```

Extracted OCaml is the Coq kernel compiled, not a shared object with C++.
The portable C++ CLIs are also not the SIMD code paths; those still have to match
the C++ Transform via `SHA256AutoDetect` / `SelfTest`. Agreement is differential,
not a refinement proof. `cpp_sigma_root_cli` does not call
`ComputeConvergenceRootFromConvergenceSet`.

