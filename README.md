RIGOR
=====

RIGOR is a Bitcoin-derived blockchain with two native assets and one governing idea:
**the script is the asset.**

There is no asset field in the output structure. `CTxOut` is byte-for-byte what Bitcoin
Core stores, and an output's asset is *inferred* by parsing its `scriptPubKey`. Nothing
about ownership lives outside the UTXO set — no balance table, no indexer, no oracle, no
administrative key. A full node decides every question by running the same script
validation it already runs.

Status
------

Pre-mainnet. The consensus rules are frozen; see `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`.
Read [docs/FORMAL-VERIFICATION-VERDICT.md](./docs/FORMAL-VERIFICATION-VERDICT.md) before
forming a view on how much of this is proved — it states both the results and their limits.

The two assets
--------------

**ORU** is the proof-of-work asset. It is mined the way bitcoin is mined: 10-minute target
block time, 8 decimal places, block subsidy paid to the coinbase. The subsidy starts at
1000 ORU and halves every 210,000 blocks, but only three times. From the third halving
onward every block pays 125 ORU forever. ORU has tail emission and therefore **no supply
cap** — a deliberate divergence from Bitcoin, not an oversight.

**RES** is scarce and cannot be mined. It has two decimal places and a hard cap of
21,000,000 RES. Each award mints exactly 100.00 RES, so the cap admits exactly 210,000
awards and then RES issuance stops permanently.

The only way RES enters existence is the convergence draw.

The convergence draw
--------------------

To enter the draw you **burn ORU**. A convergence transaction sends at least 1 ORU to a
provably unspendable output and declares, in the same transaction, the key that would
receive the RES. The burn is real: those ORU are gone whether or not you win.

Each burn becomes a candidate. For every candidate the protocol computes

```
σ = Hash256( txid ‖ C ‖ B_{h-1} ‖ LE32(h) )
```

where `C` commits the whole candidate set and `B_{h-1}` is the parent block hash. A
candidate is eligible when `σ < T · f`, with `f` the amount burned — so burning more buys
proportionally more chance, and nothing else does. Among eligible candidates the winner is
the smallest normalized digest, compared as a cross-product to avoid division, with ties
broken by byte order on the txid. The draw is a pure function of the candidate set: the
same set yields the same winner on every node, in any order it arrived.

Two properties are worth calling out because they are what the design costs:

*The draw is deferred by one block.* Block `h-1` commits the candidate set; block `h` pays
the winner. Every input to `σ` is therefore fixed by a header that already cost a
proof-of-work solution, so a miner cannot grind the draw for free.

*The candidate set spans a 32-block sliding window,* not a single block. Under a one-block
window the producer of block `h-1` chose the candidate set unilaterally and at zero
marginal cost, which made RES allocation track hashrate rather than burn. Widening the
window does not shrink a large producer's share — it removes exclusivity, because
candidates buried in earlier blocks cannot be censored. The eligibility target is divided
by the same 32 so total issuance is unchanged, and a transaction may win at most once
across the window.

Repository layout
-----------------

| Path | What it is |
|------|-----------|
| `core/` | The node. A Bitcoin Core fork; consensus lives in `core/src/consensus/`. |
| `pool/` | Mining pool: stratum server, block assembler, GBT adapter, winner engine. |
| `miner/` | CPU miner. |
| `explorer/` | Block explorer and indexer. |
| `web/` | Official site. |
| `qt/` | Desktop GUI prototype. The shipping Qt wallet is `core/src/qt/`. |
| `installer/` | Windows installer. |
| `formal_verification/` | The Coq development, golden vectors, and differential fuzzing. |
| `docs/` | Whitepaper, RFCs, specifications, security audits, proofs. |

Where to start reading
----------------------

- [docs/WHITEPAPER.md](./docs/WHITEPAPER.md) — the protocol in prose.
- [docs/rfc/](./docs/rfc) — normative rules. `RFC-000` is the consensus constitution;
  `RFC-008` and `RFC-009` fix the mathematics and the byte layouts.
- [docs/specification/](./docs/specification) — state model, economic model, trust anchors,
  and the boundaries of what has been verified.
- [docs/FORMAL-VERIFICATION-VERDICT.md](./docs/FORMAL-VERIFICATION-VERDICT.md) — what the
  Coq development proves, and what it does not.
- [docs/security/](./docs/security) — the pre-mainnet audit trail, including the findings
  that changed consensus.
- [ARCHITECTURE.md](./ARCHITECTURE.md) — the constraints the implementation is held to.

Building
--------

The node uses the Bitcoin Core build system:

```sh
cd core
./autogen.sh && ./configure && make -j"$(nproc)"
make check                       # unit tests
test/functional/test_runner.py   # functional tests
```

Platform notes are in [core/doc/](./core/doc) — `build-unix.md`, `build-osx.md`,
`build-windows.md`. Dependencies are described in [core/doc/dependencies.md](./core/doc/dependencies.md).

The Coq development is built separately and needs Coq 8.15:

```sh
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j"$(nproc)"
```

Contributing
------------

See [CONTRIBUTING.md](./CONTRIBUTING.md). Consensus code is held to the constraints in
[ARCHITECTURE.md](./ARCHITECTURE.md); changes that add asset state outside the script, that
introduce an account model, or that put an administrative key on a consensus path will not
be accepted regardless of their merits elsewhere.

Security
--------

To report a vulnerability, see [SECURITY.md](./SECURITY.md). Please do not open a public
issue for anything affecting consensus or funds.

License
-------

MIT. See [LICENSE](./LICENSE).
