# Extracted SHA-256 kernel and portable C++ CLI

`hash256_kernel.ml` is Coq `RigorSha256.sha256_bytes` / `hash256_bytes` extracted
to OCaml. `hash256_cli` hashes a hex payload. `cpp_hash_cli` is production
`CSHA256` (portable Transform from `core/src/crypto/sha256.cpp`):

```bash
bash ../scripts/extract_hash_kernel.sh
bash ../scripts/build_cpp_hash_cli.sh
./hash256_cli sha256 ""
./cpp_hash_cli sha256 ""
python3 ../tests/test_hash_kernel_tripartite.py
```

Extracted OCaml is the Coq kernel compiled, not a shared object with C++.
The portable C++ CLI is also not the SIMD code paths; those still have to match
the C++ Transform via `SHA256AutoDetect` / `SelfTest`. Agreement is differential,
not a refinement proof.

