# Verification Scope

What this release package verifies, and what it does not.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

## Verified

The following kernel-model invariants are proved in Coq (`Qed`,
`Print Assumptions` Closed) and reflected by Coq validators.

| Domain | What is verified |
|---|---|
| UTXO soundness | A valid transaction applied to a well-formed UTXO set consumes inputs, creates outputs, and preserves well-formedness |
| ORU conservation | `ORU_in = ORU_out + fee`; `new_supply = old + subsidy + fees - burn`; fee does not create ORU |
| RES issuance schedule | Positive RES creation requires coinbase, a valid mint shape, `mint_amount <= W(h)`, and a draw-eligibility **hypothesis** |
| Script classification | Recognizer branches are pairwise exclusive; witness does not fall through to mint/converge |
| PoW validity rule | Digest above target is rejected over abstract `H`; compact decode is a lower bound |
| ASERT rule | `asert_next_target` is a deterministic function of parent/anchor inputs; `exp_q` uses `Z.quot`; `num_shifts` uses `Z.div` |

Also documented: mutation resistance of those **Coq** rules, and the
TCB / refinement stop at Coq validators.

The extracted `rigor-kernel-checker` is a generated executable artifact
for testing. It is not a verified layer.

## Not verified

The following are **out of scope**. They are not implied by a passing
kernel run.

| Area | Why it is open |
|---|---|
| OCaml extraction | No extraction refinement theorem |
| Binary artifact | No binary semantic equivalence theorem |
| OCaml compiler / runtime | Trusted, not proved |
| P2P networking | No network, relay, or eclipse model |
| Mempool | Policy / admission are not modelled |
| RPC | No RPC surface in the cone |
| Wallet | Wallet format and signing are out of scope |
| Database | Storage engine and DB format are unchanged and unverified here |
| Miner implementation | No mining-loop or fairness model |
| Cryptographic primitive security | SHA3 / Keccak / Ed25519 security is not proved; PoW uses abstract `H`. Inventory: `../Kernel/CryptoAssumptions.v` |
| Full node behavior | No ConnectBlock / full-node / full-chain simulation |
| Production-code refinement | Differential verification of a comparable fragment only (`RefinementMapping.md`). No Coq-to-C++ refinement |
| Verified compiler / binary equivalence | Documented TCB only (`VerifiedCompilerPath.md`). Not switched |
| Liveness / fairness / finality / reorg | Open (`OpenProperties.md`) |

See `PublicClaims.md` and `../evidence/ClaimsBoundary.md`.
