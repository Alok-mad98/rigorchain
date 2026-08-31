# Archived / obsolete formal models

**Do not delete historical proofs.** Original `.v` files remain in
`formal_verification/coq/` so the existing ReplayReorgCore / Kernel / Phase
catalogs keep building. This directory is the **classification snapshot** of
what those files mean after the v0.1 spend-surface realignment
(TASK-RIGOR-FORMAL-VERIFICATION-MODEL-REALIGNMENT-AUDIT-P0-022).

The live v0.1 spend model is `formal_verification/rigor_v01/`.

```
equivalence_claimed = false
refinement_proved = false
```

See `OBSOLETE_MODEL_INVENTORY.md` for the full table.

## Snapshots copied here

These are frozen copies of modules whose *semantics* are obsolete or are
explicitly a retired design study. The live tree still compiles the originals.

| Snapshot | Original | Why archived |
|----------|----------|----------------|
| `coq/CvgCovenant.v` | `coq/ReplayReorgCore/CvgCovenant.v` | OP_CVG_CLAIM / covenant mint — C++ has no such opcode |
| `coq/legacy_account_model/README.md` | `coq/legacy_account_model/` | Global account model; not UTXO ConnectBlock |

Draw / σ / SHA-256 / ResSchedule proofs are **not** obsolete. They stay in
`coq/` as the monetary and convergence cone.
