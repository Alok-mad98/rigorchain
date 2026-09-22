# Kernel binary

Standalone checker built from the extracted Coq validators.

```
cd formal_verification/rigor_kernel
make
./extraction/build.sh
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh
```

Binary: `rigor-kernel-checker`

```
./rigor-kernel-checker test_vectors/res_schedule.json
VALID
```

or `INVALID`.

This is a **generated executable artifact for testing**, not a proved
executable implementation. It does not claim extraction semantic
preservation, binary semantic equivalence, C++ consensus equivalence,
full-node correctness, network correctness, wallet correctness, or
cryptographic security.

`BINARY_SEMANTIC_EQUIVALENCE=false`
