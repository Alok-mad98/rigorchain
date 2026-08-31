Shared Libraries
================

RIGOR does not ship a C consensus ABI (`libbitcoinconsensus` was removed).

The former library verified scripts with `chain_id = 0` while consensus
`SignatureHash` commits the genesis hash. Renaming it would have advertised a
wrong answer. Script verification is `VerifyRigorPredicate` plus an explicit `chain_id`
on `GenericTransactionSignatureChecker`. There is no second entry point.

The experimental kernel library (`libbitcoinkernel`) is a separate, opt-in
build (`--with-experimental-kernel-lib`) and is not a consensus C ABI.
