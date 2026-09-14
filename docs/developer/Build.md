# Building RIGOR

**Build entry:** `core/`

```
equivalence_claimed = false
documentation_consensus_authority = false
```

Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.

See [CHANGELOG.md](../../CHANGELOG.md) and
[GitHub Releases](https://github.com/rigorchain/rigorchain/releases).

---

## Node

```sh
cd core
./autogen.sh
./configure
make -j"$(nproc)"
```

Platform notes and the dependency list:

- [core/doc/build-unix.md](../../core/doc/build-unix.md)
- [core/doc/build-osx.md](../../core/doc/build-osx.md)
- [core/doc/build-windows.md](../../core/doc/build-windows.md)
- [core/doc/build-freebsd.md](../../core/doc/build-freebsd.md)
- [core/doc/build-netbsd.md](../../core/doc/build-netbsd.md)
- [core/doc/build-openbsd.md](../../core/doc/build-openbsd.md)
- [core/doc/dependencies.md](../../core/doc/dependencies.md)

`core/doc/README.md` is leftover Bitcoin Core packaging text. It is not
the product identity.

Default mainnet ports: P2P **8760**, RPC **8761**, HRP **rg**.

---

## Tests

```sh
cd core
make check
test/functional/test_runner.py
```

---

## Other components

| Component | Directory | Build |
|-----------|-----------|-------|
| SOLO miner | `miner/` | See `miner/README_MINER.md` (`-a rgrpow01`) |
| ORU pool | `pool/` | `cargo build --release` — no RES mint |
| Explorer | `explorer/` | Node; copy `explorer/.env.example` to `.env.local` |
| Website | `web/rigorchain-site/` | Node / Vite |
| Coq model | `formal_verification/coq/` | Coq **8.15.x** (pin 8.15.0) |

```sh
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j"$(nproc)"
```

Do not `apt-install` Ubuntu `coq`. CI pin:
`tools/ci/setup-coq-8.15.0.sh`.

Coq is a model. It is not a refinement proof of `core/src/`.
