# Security Policy

## Reporting a vulnerability

**Do not open a public issue for anything that affects consensus, funds, or node
availability.** A public report on a live chain is itself an attack.

Use GitHub's private vulnerability reporting on this repository:
**Security → Report a vulnerability**. That channel is private to the maintainers and
creates a thread where a fix can be prepared before disclosure.

When reporting, the most useful things you can include are the affected component and
commit, what an attacker gains, and the smallest reproduction you have — a failing unit or
functional test is ideal, but a clear description of the sequence of blocks or transactions
is enough.

## Scope

Highest priority, in order:

1. **Consensus** — anything that lets two honest nodes disagree about the valid chain, or
   that lets a block violate the rules in `core/src/consensus/`. This includes the RES
   draw: grinding the digest, biasing candidate selection, winning twice within the draw
   window, or minting RES without a corresponding ORU burn.
2. **Supply** — minting ORU beyond the per-block subsidy, minting RES beyond the
   21,000,000 cap, or creating spendable value from a convergence burn.
3. **Funds** — anything that spends an output without satisfying its script, or that makes
   a correctly-formed output unspendable.
4. **Node availability** — remote crashes, unbounded memory or disk growth, and consensus
   validation that can be made superlinear by an attacker.

Also in scope, at lower priority: the pool (`pool/`), the explorer (`explorer/`), and the
miner (`miner/`). Note that a pool never custodies RES — it issues ORU only — so pool
compromise has a smaller blast radius than a consensus bug, but a pool that can steer the
convergence set is a consensus-adjacent finding and we want to hear about it.

Out of scope: findings against the vendored third-party code under `pool/web/framework/`
and `pool/web/extensions/` should go upstream. Missing hardening headers on a static site,
and reports produced solely by an automated scanner without a demonstrated impact, are not
useful to us.

## What we already know

Read [docs/FORMAL-VERIFICATION-VERDICT.md](./docs/FORMAL-VERIFICATION-VERDICT.md) §3 before
reporting. It states plainly which properties are machine-checked and which are not — in
particular there is no mechanized refinement between the Coq model and the C++ node, and
the draw is modeled as a function rather than as a game, so no theorem bounds an attacker's
advantage. A finding in one of those gaps is genuinely new information and we want it. A
report that the gap exists is not, because we documented it ourselves.

The known-open items are tracked under [docs/security/](./docs/security).

## Supported versions

RIGOR is pre-mainnet. Until a mainnet release is tagged, only the default branch is
supported and fixes land there.
