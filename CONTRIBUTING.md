Contributing to RIGOR
=====================

Contributions are welcome. This document covers the practical workflow and, more
importantly, the constraints that consensus code is held to — a technically excellent patch
that crosses one of those lines still gets rejected, so it is worth reading them before you
write anything.

The hard constraints
--------------------

These come from [ARCHITECTURE.md](./ARCHITECTURE.md) and are not open to case-by-case
negotiation.

**Storage is immutable.** `CTxOut` does not change. No asset field, no tag, no parallel
identifier stored alongside an output. The on-chain layout stays byte-for-byte compatible
with Bitcoin Core's.

**Assets are parsed, not persisted.** An output's asset is derived by parsing its
`scriptPubKey` every time it is needed. If a patch requires reading an asset label out of
the UTXO set, the design is wrong.

**Witness v0 and v1 only.** P2WPKH, P2WSH, and the v1 RES program. No Taproot or BIP341
semantics, no hidden state machines, no dynamic storage execution model.

**The UTXO set is the only state.** Asset ownership changes by spending old outputs and
creating new ones. An account or balance model as a source of truth for consensus is
rejected on sight.

**Validation is trustless.** A full node decides validity alone. No consensus rule may
depend on an external indexer, an oracle, or a remote API.

**No privileged path.** No administrative key, no upgradeable proxy, no freeze, no
discretionary mint. Script lock is final.

Changing consensus
------------------

Consensus rules are frozen; see `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`. A change to
anything under `core/src/consensus/` needs more than a pull request:

1. **An RFC** in [docs/rfc/](./docs/rfc) stating the rule change normatively, including the
   exact byte layouts and arithmetic. `RFC-008` and `RFC-009` are the models to follow.
2. **A stated reason.** Security fixes and demonstrated incorrectness qualify. "Cleaner" or
   "more general" does not.
3. **Tests that fail before and pass after,** covering the boundary cases, not just the
   happy path.
4. **Golden vectors regenerated** under `formal_verification/`, with the diff explained.
   A vector that changes without an explanation is treated as a defect in the patch.
5. **The Coq development still builds** and stays axiom-free. If the change touches
   something the model covers, update the model.

Note that the Coq development and the C++ node are not mechanically connected — see
[docs/FORMAL-VERIFICATION-VERDICT.md](./docs/FORMAL-VERIFICATION-VERDICT.md) §3. Keeping them
in agreement is a manual obligation that falls on whoever changes either side.

Workflow
--------

Fork, branch, and open a pull request against the default branch. Keep each pull request to
one logical change; a consensus fix bundled with a refactor is much harder to review and
will be asked to split.

Write the commit message for someone reading it in five years with no memory of the
discussion. Say what changed and why it had to change. Prefix with the area — `consensus:`,
`pool:`, `explorer:`, `doc:`, `test:`.

Before you push:

```sh
cd core
make check
test/functional/test_runner.py
```

Match the style of the code around you. `core/` follows Bitcoin Core's conventions, which
are documented in [core/doc/developer-notes.md](./core/doc/developer-notes.md); the Rust
crates under `pool/` follow `rustfmt` and `clippy` defaults.

Review
------

Review is the bottleneck, as it is in any security-critical project, and consensus patches
get read slowly and suspiciously on purpose. Being patient and reviewing other people's
work is the fastest way to get your own reviewed.

When reviewing, say what you actually did — whether you read the diff, built it, or ran it
against a chain — so the maintainer knows what your approval covers.

Security
--------

Do not report vulnerabilities through pull requests or public issues. Follow
[SECURITY.md](./SECURITY.md).
