# RIGOR Database Security Closure And Release Guardian Enablement (P0-091)

**Task:** `TASK-RIGOR-DATABASE-SECURITY-CLOSURE-P0-091`  
**Date:** 2026-09-02  
**Nature:** Audit closure only. No consensus changes. No database format changes.

This TASK does **not** modify the Consensus Kernel, Transaction Kernel, Ledger,
CoinsDB serialization, Coin key format, Scheme A CRC-32C, wallet SQLite schema,
`ClassifyRigorTxout`, `AssetTypeFromScriptPubKey`, or formal theorems. It does
not add an asset column, `ORU_DB`, or `RES_DB`.

It closes the database-security audit using existing P0-087 / P0-088 / P0-089
evidence and adds a **real** Release Guardian `Stage DATABASE_SECURITY`.

Reproduce:

```
bash tools/run-database-security-closure.sh
bash tools/run-database-security-guardian-stage.sh
```

The Guardian orchestrator runs the same stage:

```
bash tools/run-genesis-release-guardian.sh
```

---

## Input evidence (not re-invented)

### P0-087 — architecture audit

Previous: `OPEN` (production field-level `-reindex` not executed; wallet SQLite OPEN).

Now: `CLOSED`, by consuming P0-088 and P0-089 rather than by forging a rebuild
inside the architecture gate.

```
RIGOR_DATABASE_ARCHITECTURE_AUDIT = CLOSED
DATABASE_AUDIT_STATUS             = CLOSED
DATABASE_CONSENSUS_BOUNDARY       = CLOSED
DATABASE_FAIL_CLOSED              = TRUE
SHARED_COIN_MODEL                 = PREFERRED_AND_KEPT
```

### P0-088 — production reindex reconstruction

```
PRODUCTION_REINDEX_AUDIT=PASS
PRODUCTION_REINDEX_CHAINSTATE=PASS
PRODUCTION_FULL_REINDEX=PASS

ORU_PRODUCTION_REBUILD_EQUIVALENCE=PASS
RES_PRODUCTION_REBUILD_EQUIVALENCE=PASS
UTXO_PRODUCTION_REBUILD_EQUIVALENCE=PASS
MINT_PRODUCTION_REBUILD_EQUIVALENCE=PASS
DRAW_PRODUCTION_REBUILD_EQUIVALENCE=PASS

PRODUCTION_REBUILD_DIVERGENCE_COUNT=0
```

Genuine `rigord -reindex-chainstate` and `rigord -reindex`. A harness replica
PASS is still not a production PASS. Reproduce:
`bash tools/run-production-reindex-reconstruction-audit.sh`.

### P0-089 — wallet SQLite

```
WALLET_SQLITE_AUDIT_STATUS=CLOSED
WALLET_SQLITE_CORRUPTION_FAIL_CLOSED=TRUE
WALLET_SQLITE_CRASH_RECOVERY=VERIFIED
WALLET_SQLITE_SCHEMA_BOUNDARY=VERIFIED
WALLET_SQLITE_ASSET_DISPLAY_BOUNDARY=VERIFIED
```

Wallet SQLite remains a **separate** non-consensus trust boundary. Reproduce:
`bash tools/run-wallet-sqlite-audit.sh`.

---

## Verdict flags

```
DATABASE_AUDIT_STATUS=CLOSED
DATABASE_SECURITY_AUDIT_STATUS=CLOSED
DATABASE_SECURITY_STAGE=ADDED
CONSENSUS_CODE_CHANGED=FALSE
DATABASE_FORMAT_CHANGED=FALSE
WALLET_FORMAT_CHANGED=FALSE
FINAL_VERDICT=PASS
```

P0-086 (`DATABASE_RELEASE_GATE=PARTIAL`) is unchanged as a CRC fail-closed
TASK: that gate still does not execute production `-reindex`. The Guardian
overall database release gate is now `CLOSED` because P0-088 / P0-089 closed
the remaining reconstruction and wallet gaps and this TASK wired them into
`Stage DATABASE_SECURITY`.

---

## Release Guardian Stage DATABASE_SECURITY

Previous:

```
RELEASE_GUARDIAN_DATABASE_SECURITY_STAGE=NOT_ADDED
```

Change: add a real stage **only because** `DATABASE_SECURITY_AUDIT_STATUS=CLOSED`.

| Piece | Role |
|-------|------|
| `tools/run-database-security-guardian-stage.sh` | Stage body |
| `tools/run-genesis-release-guardian.sh` | `run_stage "Stage DATABASE_SECURITY" ...` after Stage 5b, before fuzz |
| `.github/workflows/genesis-release-guardian.yml` | Names `Stage DATABASE_SECURITY`; still runs the unique orchestrator |

The stage executes:

```
bash tools/run-production-reindex-reconstruction-audit.sh
bash tools/run-wallet-sqlite-audit.sh
```

It requires these tokens in captured output. Exit 0 of a BLOCKED / OPEN run
is not accepted:

```
PRODUCTION_FULL_REINDEX=PASS
WALLET_SQLITE_AUDIT_STATUS=CLOSED
```

Document presence is not a substitute. The stage needs in-tree
`core/src/rigord` matching wallet / CoinsDB sources (same as P0-088 / P0-089).
Hosted CI without that binary fails closed.

Live execution of `bash tools/run-database-security-guardian-stage.sh` on
2026-09-02 (in-tree `core/src/rigord` mtime 2026-09-02 19:16:15) produced
`PRODUCTION_FULL_REINDEX=PASS`, `WALLET_SQLITE_AUDIT_STATUS=CLOSED`, and
`DATABASE_SECURITY_STAGE_RESULT=PASS`. Genuine `-reindex-chainstate` and
`-reindex` at height 4468, 4745 UTXOs, divergence 0.

---

## What this TASK did not do

- modify Consensus Kernel / Transaction Kernel
- modify CoinsDB serialization / Coin key format / Scheme A CRC
- add an asset column
- add `ORU_DB` / `RES_DB`
- change wallet schema
- enable WAL
- rewrite P0-086 as CLOSED (CRC TASK scope stays PARTIAL)

---

## Artifacts

| Path | Role |
|------|------|
| `docs/security/RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md` | This document |
| `formal_verification/database_security_closure_manifest.json` | Machine verdict |
| `tools/run-database-security-closure.sh` | Closure lint (consumes 087/088/089) |
| `tools/run-database-security-guardian-stage.sh` | Guardian stage body |

---

## Final principle

Architecture CLOSED  
+ production field-level `-reindex` PASS  
+ wallet SQLite CLOSED  
+ Guardian stage that actually re-runs those two audits  

No second CoinsDB. No format rewrite. No forged PASS.
