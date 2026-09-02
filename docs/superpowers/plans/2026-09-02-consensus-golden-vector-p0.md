# Consensus Golden Vector P0 Implementation Plan

> **For agentic workers:** Execute inline. User requested direct implementation of the P0-016 audit priority. Do not wait for a second execution-choice prompt.

**Goal:** Lock v0.1 Consensus Golden Vectors so later code cannot silently change tx validity, signature bytes, sighash field commitment, script-as-asset classification, mint encoding, CONVERGE unspendability, or mainnet ASERT height-1/2 pins.

**Architecture:** SSOT JSON under `formal_verification/vectors/`, copied into `core/src/test/data/` for C++ replay of live consensus functions. Python harnesses reconstruct sighash/tx wire and libsodium signatures. Explorer consumes the same classification JSON. No consensus formula changes except explorer mint encoding (second classifier).

**Tech Stack:** C++ `test_rigor`, Python 3 + libsodium ctypes, explorer TypeScript.

## Global Constraints

- Do not re-enable Bitcoin `tx_valid.json` / `tx_invalid.json` as RIGOR consensus.
- Do not bump `golden_lock.json` phase3 counts unless adding phase3 rows (this task uses new JSON files).
- Python sighash/tx modules remain harness-only, not consensus.
- `refinement_proved` / full-node equivalence claims stay unchanged.

---

### Task 1: Explorer OP_5 fail-closed + classification JSON replay

**Files:** `explorer/src/script.ts`, `explorer/src/script.test.ts`

### Task 2: Sighash mutation JSON + C++/Python lock

**Files:** `formal_verification/vectors/rigor_sighash_mutation_vectors.json`, `python/rigor_sighash.py` (reuse), tests

### Task 3: Ed25519 signature byte goldens

**Files:** `formal_verification/vectors/rigor_ed25519_signature_vectors.json`

### Task 4: Native RIGOR tx / CONVERGE spend / mint hex goldens

**Files:** `formal_verification/vectors/rigor_native_tx_vectors.json`, `python/rigor_tx_wire.py`

### Task 5: ASERT reference JSON (P1)

**Files:** `formal_verification/vectors/rigor_asert_vectors.json`

### Task 6: C++ unified replay + Makefile

**Files:** `core/src/test/rigor_consensus_golden_vector_tests.cpp`, `core/src/Makefile.test.include`
