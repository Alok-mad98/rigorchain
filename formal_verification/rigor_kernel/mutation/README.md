# Kernel mutation tests

Each mutation installs an **incorrect consensus rule** in the Coq
model and recompiles. A correct evidence package must see the
corresponding theorem fail.

- Healthy cone: `make` passes.
- Mutated cone: compile **must** fail (`EXPECTED_FAILURE`).
- A mutant that still compiles is `mutation_passed` and fails CI.

These tests do not claim C++ equivalence. They show that the *model*
proofs are sensitive to the stated kernel rules.

See `expected_failures.md` for the ten cases and
`../evidence/MutationReport.md` for the last recorded run.
