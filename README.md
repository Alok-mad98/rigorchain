# RIGOR

RIGOR is a Bitcoin-derived blockchain with two native assets: **ORU**, the
proof-of-work security and economic coordination asset, and **RES**, an
independent scarce resource **minted** through deterministic consensus allocation.

The protocol follows one principle:

> **The script is the compliance layer.**

Asset class is inferred from `scriptPubKey`. Legitimacy is decided by UTXO
commitments, script rules, economic proofs, and deterministic validation — not by
script parsing alone. There is no account model, no external asset database, and
no administrative key on the consensus path.

## Status

| Item | State |
|------|-------|
| Upcoming product identity | **RIGOR v0.1.1-alphaplus** — [AlphaPlus Consensus Freeze](docs/release/ALPHAPLUS_RELEASE_IDENTITY_MIGRATION_REPORT.md) |
| Consensus rules | Frozen — [docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md](docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md) |
| Mainnet checklist | [docs/release/MAINNET_GO_NO_GO_CHECKLIST.md](docs/release/MAINNET_GO_NO_GO_CHECKLIST.md) |
| Formal verification | [docs/verification/FORMAL-VERIFICATION-VERDICT.md](docs/verification/FORMAL-VERIFICATION-VERDICT.md) |
| `equivalence_claimed` | **false** (Coq model ≠ proven C++ refinement) |

## Architecture overview

- Bitcoin-derived **UTXO** ledger (`CTxOut` storage unchanged)
- **80-byte** block header compatibility
- Deterministic full-node validation
- Witness v0 (ORU) and Witness v1 (RES / convergence) only
- No Taproot / BIP341 on the consensus path

Details: [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md)

## Native assets

| Asset | Role |
|-------|------|
| **ORU** | Provides PoW security and economic coordination — Witness v0 |
| **RES** | Independent scarce resource — Witness v1; **RES minted** only via deterministic consensus allocation |

PoW does not create RES. A **convergence transaction** commits ORU under consensus rules.
Successful RES creation is described as **RES minted** through **deterministic consensus allocation**.

Normative constants (RFC-014 v1.2):

```text
RES_PROBABILITY_TARGET = floor(2^240 / MIN_CONVERGENCE_AMOUNT) >> 5
CONVERGENCE_DRAW_WINDOW = 32
```

Code SSOT: `core/src/consensus/params.h`, `core/src/consensus/rigor_consensus_lock.h`.

## Build

See `core/doc/build-*.md` for platform instructions. Typical flow:

```bash
cd core
./autogen.sh
./configure
make -j$(nproc)
```

Binaries: `rigord`, `rigor-cli`, `rigor-qt` (if Qt enabled), `rigor-wallet`.

## Wallet workflows

Creation, backup, restore, send/receive ORU & RES, and convergence (ORU → CVG2):  
[docs/wallet/WALLET_USER_WORKFLOW_GUIDE.md](docs/wallet/WALLET_USER_WORKFLOW_GUIDE.md)

Local acceptance harness: `contrib/testing/local_mainnet_simulation/run_full_acceptance.sh`

## Documentation map

| Topic | Entry |
|-------|-------|
| Whitepaper | [docs/WHITEPAPER.md](docs/WHITEPAPER.md) |
| Architecture | [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md) |
| RFCs | [docs/rfc/](docs/rfc/) |
| Specifications | [docs/specification/](docs/specification/) |
| Security | [SECURITY.md](SECURITY.md), [docs/security/](docs/security/) |
| Release / GO | [docs/release/](docs/release/) |
| Readiness audit | [docs/audit/](docs/audit/) |
| Formal verification | [docs/verification/FORMAL-VERIFICATION-VERDICT.md](docs/verification/FORMAL-VERIFICATION-VERDICT.md) |

## Repository layout

| Path | Purpose |
|------|---------|
| `core/` | Full node, wallet, consensus |
| `docs/` | Normative specs, RFCs, release evidence |
| `contrib/testing/` | Local acceptance harnesses |
| `explorer/`, `miner/`, `pool/`, `web/` | Product components |
| `formal_verification/` | Machine-checked consensus **model** |
| `infra/` | Deployment templates (examples only) |

## License

MIT — see [LICENSE](LICENSE). Upstream Bitcoin Core heritage applies to inherited components.

## Security

See [SECURITY.md](SECURITY.md). Do not expose JSON-RPC to the public Internet.
