# Developer documentation

**Protocol SSOT:** [`../protocol/RFC-020.md`](../protocol/RFC-020.md), [`../protocol/RFC-018.md`](../protocol/RFC-018.md)  
**Implementation:** `core/src/` (realization)

```
equivalence_claimed = false
documentation_consensus_authority = false
```

Protocol specification is normative SSOT.
Implementation follows protocol SSOT.
If this file disagrees with the live RFC, the RFC is correct.

| Document | Purpose |
|----------|---------|
| [`Architecture.md`](./Architecture.md) | Classification / UTXO constraints |
| [`Build.md`](./Build.md) | Node / miner / explorer / Coq build |
| [`RPC.md`](./RPC.md) | JSON-RPC index |
| [`Testing.md`](./Testing.md) | Tests and verification reproduction |
| [`CODE_SSOT_PROTOCOL_FACTS.md`](./CODE_SSOT_PROTOCOL_FACTS.md) | Facts extracted from shipping code |
| [`../architecture/RIGOR_EXECUTION_MODEL.md`](../architecture/RIGOR_EXECUTION_MODEL.md) | ConnectBlock boundary |
| [`../specification/SPEC-000-CONSENSUS-SPECIFICATION.md`](../specification/SPEC-000-CONSENSUS-SPECIFICATION.md) | Consensus facts vs code |
| [`../protocol/RFC-020.md`](../protocol/RFC-020.md) | Live issuance |

Platform build notes that still apply to the autotools tree:

- [`../../core/doc/build-unix.md`](../../core/doc/build-unix.md)
- [`../../core/doc/build-osx.md`](../../core/doc/build-osx.md)
- [`../../core/doc/build-windows.md`](../../core/doc/build-windows.md)
- [`../../core/doc/dependencies.md`](../../core/doc/dependencies.md)

`core/doc/README.md` is leftover Bitcoin Core packaging text.
