# RIGOR Deterministic Replay Checklist

**Status:** TASK-H2 **closed** (full scope). See `doc/TASK-H2-verification-report.md`
for verification commands, the H-3-2 functional path, and the `state_hash`
restart-stability fix in `Chainstate::ComputeDeterministicStateHash`.
**TASK-R0-FREEZE-VERIFY (2026-05-13):** Six burn-in artifact trees under
`docs/freeze/lr_artifacts_final/` verified: `getdeterministicstatehash` JSON
matches `getreplaysnapshot` on `state_hash`, `height`, `best_block_hash`, and
`serialized_snapshot` bytes match `final_replaysnapshot_A.rpl2.hex`. LR-09
(10k + four SIGKILL heights) archived alongside LR-03/04/05/10.
**H2/H3 minimal hardening (closure cross-check):** H-3-1 `BatchWrite` mid-batch
flush observability (`BCLog::COINDB` / `txdb.cpp`); H-3-2 `state_hash`
restart stability (`Chainstate::ComputeDeterministicStateHash`,
`feature_chainstate_sigkill_state_hash.py`). Long burn-in harness advances
`setmocktime` to tip `nTime` each batch (`feature_replay_burnin.py`) so
multi‑10k mines do not hit `time-too-new` (harness-only; no consensus change).
**Scope:** Mandatory pre-merge checklist for any PR touching consensus,
chainstate, replay tooling, or LevelDB-adjacent code.
**Authority:** This checklist consolidates the determinism rules already
present in:

- `doc/replay-reconciliation-sop.md` — operator SOP for cross-node reconciliation
  (locks, tip, flush policy, RPC usage; STEP-0103).
- Project root `.cursorrules` — bedrock invariants (UTXO-supreme, script-as-compliance,
  no account model, no admin keys).
- `ARCHITECTURE.md` — script-as-compliance + storage invariant.
- RIGOR AI EXECUTION CONSTITUTION — RULE-1 (determinism), RULE-2 (replay
  equivalence), RULE-3 (no hidden order dependence), RULE-4 (minimal
  consensus surface), RULE-5 (consensus > performance).

**Intent:** Reviewers may reject a PR by citing only the checklist item
number (e.g. "violates A-2"). No new policy is introduced here.

---

## A. Serialization and hashing

- **A-1.** Code on the `ConnectBlock` path, merkle construction, commitment
  hashing, signature domains, and replay artifacts MUST NOT use any
  hand-rolled little-endian / VARINT / uint256 encoder. Use only
  `serialize.h`, `Coin::Serialize`, `COutPoint::Serialize`, `Span<>`,
  `HashWriter`, `CHashWriter`. Violating this causes byte drift between the
  authoritative encoder and the local hand-roll (see TASK-H2 R-1).
- **A-2.** When hashing a **set** of items, use `MuHash3072` (order-free) or
  sort the items first by a fully-deterministic byte-stable key, then hash
  in sorted order. Direct `concat → hash` over an unsorted `vector` is
  forbidden.
- **A-3.** Every hash with cross-version persistence MUST be domain-separated
  with a versioned ASCII tag (example: `"RIGOR/STATE_HASH/v2"`). Any change
  to leaf layout, ordering, or preamble MUST bump the version suffix.

## B. Containers and iteration

- **B-1.** No code path that produces consensus state, replay artifacts, or
  on-disk hashes may serialize an `unordered_map` / `unordered_set` by
  iteration. Lookup (`.find` / `.count`) is permitted.
- **B-2.** `CCoinsMap` may be iterated inside `BatchWrite` only because the
  sink (LevelDB) re-orders by key. Iterating `CCoinsMap` for hashing or
  signing is forbidden.
- **B-3.** Any "sort then process" step MUST use a byte-stable key:
  `uint256` byte-lex, `uint64_t` numeric order, or a `(txid, vout)` pair.
  Sorting by fee, fee-rate, timestamp, mempool insertion order, or any
  other non-stable attribute is forbidden in consensus paths.

## C. Types and arithmetic

- **C-1.** `float` / `double` are forbidden in consensus paths. Ratios MUST
  be cross-multiplied: `a/b < c/d` becomes `a*d < c*b`.
- **C-2.** Any multiplication that can exceed 256 bits MUST be done in a
  width that contains the full product (see TASK-H2 R-3). Implicit
  conversion of `uint64_t` into a 256-bit arithmetic class is a known
  hazard.
- **C-3.** Use fixed-width integer types (`int32_t`, `uint32_t`, `int64_t`,
  `uint64_t`) for any consensus-visible quantity. `int`, `long`, `unsigned`
  are platform-dependent and forbidden in consensus arguments and return
  types.
- **C-4.** Shift amounts MUST satisfy `0 <= shift < bit_width(type)`.
  Shifting `uint64_t` by 64 is undefined behavior.

## D. Time and randomness

- **D-1.** Consensus paths MUST NOT call `GetTime`, `GetTimeMillis`,
  `GetTimeMicros`, `GetSystemTimeInSeconds`, `GetMockTime`, or any other
  wall-clock source. Time-bearing fields MUST come from block headers
  (`nTime`) or ancestor block hashes.
- **D-2.** Consensus paths MUST NOT call `GetRand`, `GetRandBytes`,
  `GetStrongRandBytes`, `FastRandomContext`, `InsecureRand`, or any RNG.
  Randomness used in consensus (e.g. σ_i in the selection law) MUST be
  derived from block header hashes.
- **D-3.** A helper in `consensus/` that calls an RNG or wall clock MUST
  be named so that its non-consensus role is unambiguous
  (`*_for_wallet`, `*_offline_only`), and MUST carry a comment stating
  "NOT FOR CONSENSUS PATHS".

## E. State and lifetime

- **E-1.** Consensus functions MUST NOT use function-local `static`
  variables, thread-local storage, mutable globals, or hidden singleton
  caches whose state survives a process restart.
- **E-2.** Any performance cache (signature cache, script cache, sigops
  cache) MUST be designed so that clearing it does not change the final
  result of `ConnectBlock` — only the speed.
- **E-3.** A snapshot-capture function (e.g.
  `ComputeDeterministicStateHash`) MUST assert
  `view.GetBestBlock() == pindex->GetBlockHash()`. A mismatched pair is a
  silent replay hazard.
- **E-4.** Any function that walks the full coins cursor under `cs_main`
  MUST document its O(UTXO size) lock holding time, and MUST NOT be called
  from the consensus hot path.

## F. Persistence

- **F-1.** Writes to chainstate MUST go through `CCoinsViewDB::BatchWrite`.
  Bypassing that path (e.g. direct file I/O into the chainstate directory)
  is forbidden.
- **F-2.** Any new persistent key-value store MUST use `CDBWrapper`, which
  provides LevelDB byte-lex key ordering, `paranoid_checks`, and checksum
  verification.
- **F-3.** Mempool persistence is not consensus. A failure to replay the
  persisted mempool MUST NOT prevent node startup.

## G. Logging and side effects

- **G-1.** `LogPrintf` and friends inside consensus paths may only write
  text. They MUST NOT modify chainstate, throw, or rely on string side
  effects.
- **G-2.** Diagnostic projections of chainstate (e.g. `ReplaySnapshot`,
  `ChainstateSummary`) MUST be implemented as standalone functions outside
  of `ConnectBlock`. Inlining diagnostic logic into consensus is forbidden.

## H. Tests

- **H-1.** Any new code that hashes a node-wide state MUST ship with
  byte-pinned test vectors covering at least: empty chainstate, regtest
  genesis, and regtest + 3 blocks.  
  **Project mapping:** `state_snapshot_tests.cpp` pins **pre-genesis** bytes and a
  deterministic **in-memory post-genesis** oracle (same encoding surface as
  `regtest + N` without requiring a live `TestChain100Setup` fixture); see TASK-H2
  H-1-2 comments in that file.
- **H-2.** Functional tests MUST include "two nodes synced to the same tip
  → identical state_hash bytes".
- **H-3.** A crash-recovery test MUST verify that `SIGKILL` during
  mid-batch flush still yields the same `state_hash` after restart as a
  cleanly-shut-down node.  
  **Implementation:** `test/functional/feature_chainstate_sigkill_state_hash.py`
  (devtest, `gettxoutsetinfo` + small `-dbbatchsize`, COINDB partial-flush assert,
  then SIGKILL + byte-identical triple).

---

## Explicitly forbidden anti-patterns

- Adding any retry, fallback, or "alternate-encoder" path that activates on
  hash mismatch. The correct response to a mismatch is a hard error.
- "Optimizing" any of the rules above by introducing an order-dependent
  shortcut behind a feature flag.
- Folding script-classification heuristics (e.g. `IsResScript`,
  `IsConvergenceScript`) into a consensus hash. They live in the
  diagnostic layer (`ReplaySnapshot`, `ChainstateSummary`) only.

---

## Citations

- TASK-H2 root-cause ranking R-1 .. R-12 (replay drift root causes).
- `doc/TASK-H2-verification-report.md` — closure verification log (H-3-2 + restart-stable `state_hash`).
- STEP-0101 implementation notes (deterministic chainstate snapshot hash).
- STEP-0102 implementation notes (chainstate diff tool).
