# Boundary Mutation Audit

Which P0-109 mutations sit **inside** the kernel proof boundary, and
which mutation *classes* are **outside** the cone (not run, not claimed).

## Inside kernel proof

These mutate a kernel-model rule. The corresponding Coq proof must
fail (`EXPECTED_FAILURE`). They do not touch the C++ node, network,
wallet, or database.

| ID | Incorrect rule | Inside cone |
|---|---|---|
| M1 | `W(h) + 1` | Yes — RES schedule |
| M2 | `mint < W(h)` instead of `<=` | Yes — RES issuance |
| M3 | remove `- burn` | Yes — ORU conservation |
| M4 | fee creates ORU (`out - in`) | Yes — fee / no hidden inflation |
| M5 | ASERT `Z.quot` → `Z.div` | Yes — ASERT operator split |
| M6 | claim `decode(encode(t)) = t` | Yes — compact bits |
| M7 | classifier overlap (v0 as `T_MINT`) | Yes — script recognizers |
| M8 | invert PoW target comparison | Yes — PoW target |
| M9 | allow double spend (`apply_body` skips consume) | Yes — UTXO |
| M10 | drop coinbase-only from `res_created` | Yes — RES issuance |

Recorded run: `evidence/MutationReport.md` (10 / 10 `EXPECTED_FAILURE`).

## Outside kernel

The following mutation *classes* are **not** part of this cone.
They are listed so an auditor does not treat kernel mutation
resistance as node, wallet, or storage verification.

| Class | Why it is outside |
|---|---|
| Network mutation | No P2P / relay model |
| Wallet mutation | Wallet format and signing are out of scope |
| Database corruption | Storage engine is out of scope |
| RPC mutation | No RPC surface |
| C++ consensus edit | `src/` / `core/src/**` are frozen; not mutated here |
| Crypto-primitive mutation | No SHA3 / Ed25519 implementation in the cone |

A network, wallet, database, or RPC mutant would not be expected to
fail a kernel theorem, because those components are outside
`refinement_scope`.
