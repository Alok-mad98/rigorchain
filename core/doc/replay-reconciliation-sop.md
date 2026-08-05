# RIGOR — Deterministic Replay Reconciliation SOP (STEP-0103)

**Status:** Operational hardening (documentation + CI).  
**Scope:** How operators and reviewers compare two nodes **without** changing
consensus semantics, retry logic, or the definitions of STEP-0101 / STEP-0102
artifacts.

**Authority:** Aligns with `doc/replay-determinism-checklist.md` (especially
**A-2**: multiset hashing uses **MuHash3072** or explicit sort-then-hash),
`.cursorrules`, and the RIGOR execution constitution (determinism first).

---

## 1. Purpose

- Reduce **false positives** (different lock tip, different RPC inputs, stale
  caches).
- Detect **true replay drift** (different `state_hash` or CSM1 wire for the
  same declared tip).

---

## 2. Pre-sample invariants (mandatory)

Perform **all** checks before capturing `state_hash` or `getchainstatesummary`:

1. **Same network** — matching `chainparams` / datadir intent.
2. **Same height and same tip block hash** — compare `getblockchaininfo` (or
   equivalent) on both sides.
3. **`cs_main`-equivalent consistency** — RPC handlers that touch chainstate
   already take `LOCK(cs_main)`; do not interleave local scripts that read
   chainstate without the same guarantees.
4. **`CoinsTip().GetBestBlock() == pindex->GetBlockHash()`** — the pair
   `(view, pindex)` must be coherent; STEP-0101 asserts this on the snapshot
   path.
5. **Optional flush window** — if your procedure requires bit-identical disk
   images (not just identical consensus state in RAM), define a single
   project-wide rule, e.g. `invalidateblock` / shutdown / `stopatheight` **or**
   an explicit `flush` RPC if enabled — and use the **same** rule on both
   nodes. Default RPC snapshots use the in-memory view at tip; they do not
   replace a chain-wide flush protocol unless you add one consistently.

---

## 3. Authoritative artifacts

| Artifact | Role |
|----------|------|
| `state_hash` from `getdeterministicstatehash` / STEP-0101 | **Primary** cross-node fingerprint of chainstate at tip. |
| `chainstatesummary_hex` from `getchainstatesummary` | Fixed-size **CSM1** blob: coarse counters, bucket drill-down, selection projection. |
| `diffchainstatesummaries` | Returns **semantic first difference** (`logical`) and **wire first byte** (`serialized`). |

Always record: height, tip hash, RPC parameters, and timestamps (wall clock
**outside** the preimage — for logs only).

---

## 4. Comparison procedure

1. Capture `state_hash` on A and B. If equal, UTXO multiset commitment matches
   under the current STEP-0101 definition.
2. If investigating further, capture `getchainstatesummary` on both (same tip
   sanity: optional `blockhash` **must** equal active tip where enforced).
3. Run `diffchainstatesummaries` on the two hex blobs.
4. If `logical.field == "utxo_bucket_hashes"`, record **`bucket_index`** and
   narrow investigation to that txid high-byte bucket.
5. If `logical` and `serialized` disagree on **valid** CSM1 inputs, treat that
   as a **tooling bug** candidate (should be rare); file an issue with both
   blobs attached as hex.

---

## 5. Non-goals / rejected changes (project-specific rationale)

### 5.1 OPUSHASH vs MuHash (UTXO / chainstate digest)

In this repository **OpuHash / OPUSHASH** refers to **Proof-of-Work** hashing
(`pow_opus.cpp`, `doc/opushash.md`). It is **not** a multiset commitment primitive.

STEP-0101 commits the UTXO set with **MuHash3072** over canonical
`(COutPoint || Coin::Serialize)` leaves so that **LevelDB iterator order does
not matter**. Replacing MuHash with PoW-style OPUSHASH on that path would:

- Change every `state_hash` (breaks replay equivalence with deployed meaning).
- Contradict `replay-determinism-checklist.md` **A-2**.

**Policy:** Keep MuHash for UTXO multiset / `state_hash`. Use OPUSHASH only in
the PoW pipeline where it is defined today.

### 5.2 RES-mint field names on `CBlockIndex`

The RES-mint-related block index members are named **`has_res`**, **`last_res_mint_block_hash`**,
**`m_block_res_mint_sum`**, and the memory-only cache **`m_chain_res_mint_total`**
(`chain.h` / `CDiskBlockIndex` serialization). Legacy `*_orbis_*` identifiers were
removed; **serialization order and field types are unchanged** (same on-disk bytes
as before the rename).

**Do not conflate with ORU:** **`m_chain_oru_total` / `m_block_converge_sum`** are
RFC-002 ORU supply / convergence aggregates — not RES. Renaming them to `*_res_*`
would be semantically wrong.

**Policy:** Any future change that **reorders** persisted `CDiskBlockIndex` fields or
changes encodings remains a **storage / compatibility** project requiring a
versioned index bump and migration.

---

## 6. Platform and compiler expectations

Existing CI already runs **GCC** and **Clang + libc++** unit tests
(`linux-tests.yml`) to catch STL / iteration / UB differences.

**Endian / 32-bit:** RIGOR wire formats use explicit little-endian fixed-width
fields; do not add `memcpy` of unpadded structs into digests. Full multi-arch
CI is optional; this SOP requires at least the two toolchain rows above on
each PR touching consensus-adjacent replay code.

---

## 7. Minimal hardening checklist (merge gate for replay-touching PRs)

- [ ] No new digest path uses raw `unordered_*` iteration for hashing.
- [ ] No RNG, wall time, pointer values, or static counters feed preimage
      construction for `state_hash` / CSM1.
- [ ] Any change to leaf or trailer layout bumps the STEP-0101 domain tag / CSM1
      magic per project rules.
- [ ] `state_snapshot_tests` and `state_diff_tests` pass locally.
- [ ] UBSan replay-artifact job (see CI) is green when replay modules change.

---

## 8. Residual hazards (summary)

| Hazard | Mitigation |
|--------|-------------|
| False positive from mismatched tip or RPC | Follow §2–§4 strictly. |
| True drift from serialization bug | Byte-pin tests + UBSan job. |
| Disk vs RAM view mismatch | Document flush policy if you need disk parity. |
| Accidental conflation of RES and ORU names | Never rename ORU fields to `res`. |
