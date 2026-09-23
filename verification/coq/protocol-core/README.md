# Coq — protocol core

**Canonical paths** (do not edit copies here). Live protocol-core objects are
listed by `formal_verification/coq/_CoqProject.protocol-core` and
`formal_verification/rigor_kernel/_CoqProject.kernel-critical`.

Lottery ReplayReorgCore / draw models have been removed. Current Convergence
is the RFC-020 body mint priced by `destroy_cost(h)` /
`CheckResMintArithmetic`.

| Topic | File |
|-------|------|
| RES destroy cost / mint | `formal_verification/coq/ResSchedule.v`, `core/src/consensus/res_issuance.h` |
| Abstract script / CVG2 form | `formal_verification/coq/ScriptMintWitness.v`, `InvalidCvg.v` |
