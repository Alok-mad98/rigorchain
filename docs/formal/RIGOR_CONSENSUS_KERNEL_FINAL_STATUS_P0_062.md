# Consensus Kernel Final Status (P0-062)

**Task:** `TASK-RIGOR-CONSENSUS-KERNEL-AUDIT-PACKAGE-P0-062`  
**Date:** 2026-08-30  
**Nature:** Repository status for third-party audit. No new protocol rules.

```
CONSENSUS_KERNEL_STATUS     = CLOSED
CONSENSUS_SURFACE_STATUS    = CLOSED
TRANSACTION_KERNEL_STATUS   = CLOSED
POW_BOUNDARY_STATUS         = REGISTERED
GOVERNANCE_STATUS           = CLOSED
AUDIT_PACKAGE_STATUS        = PASS

COQ_ADMITTED                = 0
COQ_AXIOMS                  = 0
PRODUCTION_CONSENSUS_MODIFIED = NO
PRODUCTION_CONSENSUS_AUTHORITY     = RigorConsensusKernel(C++)
FORMAL_REFINEMENT_STATUS           = PROVED
EXTRACTED_KERNEL_PRODUCTION        = FALSE
COMPCERT_BINARY_VERIFIED_KERNEL    = FALSE
COQ_MODEL_CLOSURE           = PROVED
COQ_TO_CPP                  = VECTOR-TESTED
EXTRACTED_TO_BINARY         = NOT-LINKED
PREDICATE_BOUNDARY_VS_PRODUCTION = SAME-FUNCTION
EQUIVALENCE_PROVED          = FALSE
```

---

## Package

| Artifact | Role |
|---|---|
| `formal_verification/rigor_consensus_audit_manifest.json` | audit manifest |
| `formal_verification/release_consensus_vectors.json` | release golden freeze |
| `docs/formal/RIGOR_FORMAL_PROOF_INDEX_P0_062.md` | proof index |
| `docs/formal/RIGOR_CONSENSUS_ATTACK_SURFACE_P0_062.md` | critical vs non-kernel |
| `tools/lint-audit-scope.py` | overclaim reject |
| `tools/reproduce_audit_package.sh` | third-party one-shot |

Success of the one-shot prints
`RIGOR_CONSENSUS_AUDIT_PACKAGE_REPRODUCIBLE`.

---

## Unchanged

```
ISSUANCE_CHANGED = NO
RES_CHANGED      = NO
DRAW_CHANGED     = NO
SUPPLY_CHANGED   = NO
POW_CHANGED      = NO
ASERT_CHANGED    = NO
```

Production remains handwritten C++. Extracted OCaml is not linked.

Kernel proof-to-binary (scope = ledger apply + supply delta, not the
node) is the remaining path to `EXTRACTED_TO_BINARY`. B1 extracts a C99
kernel. `NoDupOutpoints` is already a single unsorted scan so that
extraction does not have to pick between two C++ bodies. Spend
predicates stay behind the Ed25519 oracle; they are not in the first
CompCert object.

---

## Non-goals

This package does not include, and must not be cited as:

- full-node refinement
- UTXO database proof
- LevelDB proof
- network proof
- wallet proof
- miner proof
- CompCert
- VST
- OCaml production switch
