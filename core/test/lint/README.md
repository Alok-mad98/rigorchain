This folder contains lint scripts.

Running locally
===============

To run linters locally with the same versions as the CI environment, use the included
Dockerfile:

```sh
cd ./ci/lint
docker build -t bitcoin-linter .

cd /root/of/bitcoin/repo
docker run --rm -v $(pwd):/bitcoin -it bitcoin-linter
```

After building the container once, you can simply run the last command any time you
want to lint.


check-doc.py
============
Check for missing documentation of command line options.

commit-script-check.sh
======================
Verification of [scripted diffs](../../doc/developer-notes.md#scripted-diffs).
Scripted diffs are only assumed to run on the latest LTS release of Ubuntu. Running them on other operating systems
might require installing GNU tools, such as GNU sed.

git-subtree-check.sh
====================
Run this script from the root of the repository to verify that a subtree matches the contents of
the commit it claims to have been updated to.

```
Usage: test/lint/git-subtree-check.sh [-r] DIR [COMMIT]
       test/lint/git-subtree-check.sh -?
```

- `DIR` is the prefix within the repository to check.
- `COMMIT` is the commit to check, if it is not provided, HEAD will be used.
- `-r` checks that subtree commit is present in repository.

To do a full check with `-r`, make sure that you have fetched the upstream repository branch in which the subtree is
maintained:
* for `src/secp256k1`: https://github.com/bitcoin-core/secp256k1.git (branch master)
* for `src/leveldb`: https://github.com/bitcoin-core/leveldb-subtree.git (branch bitcoin-fork)
* for `src/crypto/ctaes`: https://github.com/bitcoin-core/ctaes.git (branch master)
* for `src/crc32c`: https://github.com/bitcoin-core/crc32c-subtree.git (branch bitcoin-fork)
* for `src/minisketch`: https://github.com/sipa/minisketch.git (branch master)

To do so, add the upstream repository as remote:

```
git remote add --fetch secp256k1 https://github.com/bitcoin-core/secp256k1.git
```

all-lint.py
===========
Calls other scripts with the `lint-` prefix.

lint-consensus-discipline.py
============================
 H-4-1 / H-4-2 guardrail. Scans the consensus directories
(`core/src/consensus/`, `core/src/consensus_rigor/`) for symbols that
the RIGOR project bans from any consensus path: wall-clock reads
(`GetTime*`), RNG sources (`GetRand*`, `FastRandomContext`), and
hash-table-ordered containers (`std::unordered_map`,
`std::unordered_set`, and their multimap/multiset variants). The
authoritative rule set lives in
`core/doc/replay-determinism-checklist.md`.

Run directly:

```sh
python3 core/test/lint/lint-consensus-discipline.py
```

A documented exemption can be granted by annotating the line with
`NOT FOR CONSENSUS PATHS` or `consensus-discipline-allow`; this is
intended only for comments / strings that reference the forbidden
symbol without invoking it.
