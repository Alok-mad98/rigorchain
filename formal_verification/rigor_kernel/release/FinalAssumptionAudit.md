# Final Assumption Audit

Scan of the Level 4.9 kernel cone. No new axioms were added.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

## Forbidden Coq tokens (all `*.v` under `rigor_kernel/`)

CI G-1 and this audit reject:

| Token | Result |
|---|---|
| `Admitted` | absent |
| `admit` | absent |
| `Axiom` | absent |
| `Parameter` | absent |
| `refinement_proved` | absent |
| `equivalence_proved` | absent in this cone |

`ASSUMPTION_AUDIT_PASS`

`NO_ADMITTED=TRUE`
`NO_AXIOMS=TRUE`

## Print Assumptions

Every Category A and Category B theorem in
`release/TheoremClassification.md` is emitted by
`scripts/check_theorem_classification.py --emit` and checked with
`coqtop -batch -l`. Expected: **93** unique names
(62 A + 32 B − 1 shared name `decode_encode_identity`).

Each printed result is:

```
Closed under the global context
```

`ALL_ASSUMPTIONS=CLOSED`

Category C is not in the Print Assumptions driver (documentation /
non-claim). Those theorems are still `Qed` with no `Admitted`.

## Modelling hypotheses (not Coq axioms)

These are **explicit** and must stay visible. They are not hidden
`Axiom`s.

| Hypothesis | Where | Why it is not a Coq axiom |
|---|---|---|
| Abstract PoW digest `H` | `Pow/AbstractHash.v` | Uninterpreted function parameter of theorems; no security proof |
| Draw eligibility C-4 | RES issuance | Input bit / hypothesis of `res_issuance_soundness` |
| Extraction correctness | Extraction layer | Source identity only |
| OCaml compiler | TCB Layer 4+ | `COMPILER_CORRECTNESS=false` |
| CPU / ABI / native binary | TCB OPEN | Not modelled |
| keccak / SHA3 / Ed25519 | `Kernel/CryptoAssumptions.v` | Bool flags `= false`, proved `= false` |
| C++ ConnectBlock | OP-R2 / OP-P1 | Out of cone |

`Unsafe assumption` in the sense of an undeclared axiom: **none found**.

## What an attacker should still probe

1. Whether a Category B reflection lemma is cited as domain soundness.
2. Whether `kernel_boundary_closed` (Category C, `True`) is cited as TCB shrink.
3. Whether Closed `Print Assumptions` is confused with “no modelling hypotheses”.
4. Whether older `formal_verification/coq/` flags (`equivalence_proved = false`)
   are misread as proofs.

Companion: `release/FinalProofInventory.md`, `release/OpenProperties.md`.
