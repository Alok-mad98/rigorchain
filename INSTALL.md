Building RIGOR
==============

The node is built with the Bitcoin Core build system, from the `core/` directory:

```sh
cd core
./autogen.sh
./configure
make -j"$(nproc)"
```

Platform-specific instructions and the dependency list live in `core/doc/`:

- [core/doc/build-unix.md](./core/doc/build-unix.md)
- [core/doc/build-osx.md](./core/doc/build-osx.md)
- [core/doc/build-windows.md](./core/doc/build-windows.md)
- [core/doc/build-freebsd.md](./core/doc/build-freebsd.md)
- [core/doc/build-netbsd.md](./core/doc/build-netbsd.md)
- [core/doc/build-openbsd.md](./core/doc/build-openbsd.md)
- [core/doc/dependencies.md](./core/doc/dependencies.md)

Running the tests
-----------------

```sh
cd core
make check                       # C++ unit tests
test/functional/test_runner.py   # Python functional tests
```

Other components
----------------

The pool (`pool/`) and its Rust crates build with `cargo build --release`. The explorer
(`explorer/`) and site (`web/`) are Node projects — copy `explorer/.env.example` to
`.env.local` and adjust it before running the indexer. The Coq development is built with
Coq 8.15:

```sh
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j"$(nproc)"
```
