# RIGOR

RIGOR is a dual-asset UTXO protocol built on script-derived asset
classification, stateless monetary architecture, and formally verified
consensus invariants.

The protocol follows one principle:

> **The script is the compliance/classification layer.**

Asset class is inferred from `scriptPubKey`. Asset ownership and transfer are
enforced through UTXO consumption, script validation, signature verification,
and consensus predicates. Script alone does not authorize asset creation.
There is no account model, no explorer asset ledger, and no administrative
key on the consensus path.

“Formally verified consensus invariants” are properties of the protocol
model. This repository does not claim C++ equivalence
(`equivalence_claimed = false`).

## Public core whitepapers

| Question | Paper |
|----------|-------|
| What is RIGOR? | [docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md](docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md) |
| How does the mathematics run? | [docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md](docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) |
| Why is it trustworthy? | [docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md](docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md) |

## Status

| Item | State |
|------|-------|
| Upcoming product identity | **RIGOR v0.1.2-alphaplus** — [AlphaPlus Consensus Freeze](docs/release/ALPHAPLUS_RELEASE_IDENTITY_MIGRATION_REPORT.md) |
| Consensus rules | Frozen — [docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md](docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md) |
| Mainnet checklist | [docs/release/MAINNET_GO_NO_GO_CHECKLIST.md](docs/release/MAINNET_GO_NO_GO_CHECKLIST.md) |
| Formal verification | [verification/README.md](verification/README.md) (public map); [verdict](docs/verification/FORMAL-VERIFICATION-VERDICT.md) |
| `equivalence_claimed` | **false** (Coq model ≠ proven C++ refinement) |

## Architecture overview

Public identity: [docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md](docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md)

```
RIGOR =
    Dual Native Assets
  + UTXO Monetary Model
  + Deterministic Issuance
  + Deflationary Mechanisms
  + Probabilistic Selection
  + Adaptive Proof-of-Work
  + Cryptographic Ownership
```

Implementation constraints:
[docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md)

Consensus mathematics:
[docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md](docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md)

Explorer and indexer are observation systems. They display
consensus-derived data.

## Native assets

| Asset | Role |
|-------|------|
| **ORU** | Base transaction asset, fees, mining subsidy, commitment cost |
| **RES** | Reserve asset; deterministic issuance after Convergence — not a PoW reward |

PoW does not create RES. Fee burn is not a UTXO and not a wallet balance.

`equivalence_claimed = false`. This repository does not claim a fully proven
blockchain or C++ ↔ Coq equivalence.

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

Creation, backup, restore, send/receive ORU & RES, and Convergence (`OP_CONVERGE`):  
[docs/wallet/WALLET_USER_WORKFLOW_GUIDE.md](docs/wallet/WALLET_USER_WORKFLOW_GUIDE.md)

Local acceptance harness: `contrib/testing/local_mainnet_simulation/run_full_acceptance.sh`

## Documentation map

| Topic | Entry |
|-------|-------|
| What is RIGOR? | [docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md](docs/whitepaper/RIGOR_PROTOCOL_IDENTITY.md) |
| How the mathematics runs | [docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md](docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) |
| Why it is trustworthy | [docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md](docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md) |
| Narrative whitepaper | [docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md](docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md) (not a fourth public core) |
| RFCs | [docs/archive/rfc/](docs/archive/rfc/) (historical corpus; not public identity) |
| Architecture | [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md) (implementation constraints) |
| Specifications | [docs/specification/](docs/specification/) |
| Security | [SECURITY.md](SECURITY.md), [docs/security/](docs/security/), [minimal script architecture](docs/security/minimal-script-architecture.md) |
| Release / GO | [docs/release/](docs/release/) |
| Readiness audit | [docs/audit/](docs/audit/) |
| Formal verification | [docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md](docs/whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md) (public identity) · [verification/README.md](verification/README.md) (operator map); [HOW_TO_VERIFY](docs/security/HOW_TO_VERIFY_RIGOR.md) |

## Repository layout

| Path | Purpose |
|------|---------|
| `core/` | Full node, wallet, consensus |
| `docs/` | Normative specs, RFCs, release evidence |
| `contrib/testing/` | Local acceptance harnesses |
| `explorer/`, `miner/`, `pool/`, `web/` | Product components |
| `verification/` | Public verification architecture map (indexes into `formal_verification/`) |
| `formal_verification/` | Machine-checked consensus **model**, golden vectors, differential gate |
| `infra/` | Deployment templates (examples only) |

## License

MIT — see [LICENSE](LICENSE). Upstream Bitcoin Core heritage applies to inherited components.

## Security

See [SECURITY.md](SECURITY.md). Do not expose JSON-RPC to the public Internet.
