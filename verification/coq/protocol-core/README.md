# Coq — protocol core

**Canonical paths** (do not edit copies here; build from `formal_verification/coq/`):

| Topic | File |
|-------|------|
| Dual-pool / block RES net | `formal_verification/coq/ReplayReorgCore/DualPoolConservation.v` |
| Apply-tx / apply-block | `formal_verification/coq/ReplayReorgCore/StateTransition.v` |
| Supply / ORU bound / RES cap on apply | `formal_verification/coq/ReplayReorgCore/SupplyConservation.v` |
| Script shape lift | `formal_verification/coq/ReplayReorgCore/ScriptShape.v`, `ScriptShapeLift.v` |
| Explicit non-claims | `formal_verification/coq/ReplayReorgCore/OpaqueBoundary.v` |
| Replay / rollback | `formal_verification/coq/ReplayReorgCore/ReplayInvariants.v` |
| Abstract script / CVG2 form | `formal_verification/coq/ScriptMintWitness.v`, `InvalidCvg.v` |

**Example theorems:** `apply_block_res_net_bounded_by_scheduled_mints`, `apply_block_succeeds_implies_script_shape`, `we_do_not_claim_cm_draw_won_is_owner_pcorb`.
