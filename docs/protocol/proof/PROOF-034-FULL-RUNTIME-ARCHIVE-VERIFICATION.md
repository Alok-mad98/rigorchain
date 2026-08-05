 PROOF-034: Full Runtime Archive Verification
#
**Task:** `TASK-PROOF-034`  
**Title:** FULL-RUNTIME-ARCHIVE-VERIFICATION  
**Classification:** ENGINEERING VERIFICATION ONLY  
**Track:** Formal Verification Program — Runtime Archive Layer  
**Status:** PROCEDURE ESTABLISHED — **RUN NOT YET EXECUTED**  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [PROOF-033](./PROOF-033-PD4F-CPP01-NON-CLOSABILITY.md), [PROOF-032](./PROOF-032-PHASE5-TERMINAL-STATUS.md), [PROOF-031](./PROOF-031-PHASE5-MAINNET-FREEZE.md), [PROOF-015](./PROOF-015-A1-A5-BATCH-RUNTIME-OBSERVATION.md), [PROOF-007](./PROOF-007-RUNTIME-VECTOR-MATERIALIZATION.md)  
**Scope:** Documentation and automation procedure only.

**Consensus Impact:** NONE  
**Equivalence Claimed:** false

---

## §1 Purpose

The Phase-5 mathematical proof chain (PROOF-023 through PROOF-033) and mainnet
freeze formalization (PROOF-031/032) are **complete at the Coq/documentation
layer**. That completion does **not** substitute for a **full-chain runtime
archive** that materializes and verifies all twelve Golden Vectors on a real
regtest chain mined to height ≥ 2 100 001.

### Formal Proof ≠ Runtime Archive Verification

| Layer | What it establishes | What it does not establish |
|-------|---------------------|----------------------------|
| **Formal proof (Coq)** | Frozen dependency status, gate baselines, draw impossibility, CPP-01 non-closability in frozen model, Phase-5 terminal verdict | That regtest mirrors were materialized at h=630 000 / 2 100 000 / 2 100 001 |
| **Runtime archive verification** | All 12 vectors ESTABLISHED, harness summary 9 PASS + 3 PARTIAL + 0 FAIL + 0 NOT_EXECUTED, JSON artifacts persisted with hashes | Equivalence, CPP-01 closure, draw closure, or consensus correctness |

This task documents the **final engineering procedure** to produce that archive.
It does **not** attempt equivalence proof, CPP-01 closure, draw closure, or any
protocol/consensus modification.

---

## §2 Current State

Summary aligned with PROOF-031/032/033 terminal baseline:

| Object | Status |
|--------|--------|
| PD-13 | DS_Closed |
| PD-14 | DS_Closed |
| PD-15 | DS_Closed |
| PD4F_OB_02 | DS_Ready |
| PD4F_CPP_01 | DS_Open |
| CPP-01 closable (frozen) | false ([PROOF-033](./PROOF-033-PD4F-CPP01-NON-CLOSABILITY.md)) |
| Phase-5 terminal status | ESTABLISHED ([PROOF-032](./PROOF-032-PHASE5-TERMINAL-STATUS.md)) |
| Mainnet freeze candidate | ESTABLISHED ([PROOF-031](./PROOF-031-PHASE5-MAINNET-FREEZE.md)) |
| `equivalence_claimed` | false |
| Draw closure (FVR-M) | permanently IMPOSSIBLE |
| Golden vectors @ h≤501 | 9 materialized / 12 catalogued |

---

## §3 Remaining Runtime Gap

Three vectors remain **`GVR_NOT_EXECUTED`** at partial-chain observation (h ≤ 501):

| Vector | Required height | Family | Current status @ h≤501 |
|--------|-----------------|--------|-------------------------|
| **GV-ORU-0003** | h ≥ **630 000** | ORU halving / subsidy | NOT_EXECUTED |
| **GV-RES-0003** | h ≥ **2 100 000** | RES cap boundary | NOT_EXECUTED |
| **GV-DRAW-0003** | h ≥ **2 100 001** | DRAW reject fixture (FVR-M) | NOT_EXECUTED |

### Current partial verification

| Metric | Value |
|--------|-------|
| Materialized (established) | 9 |
| Not executed | 3 |
| Catalog total | 12 |

### Expected final archive state

| Metric | Value |
|--------|-------|
| Materialized | **12** |
| Verified | **12** |
| Harness PASS | **9** |
| Harness PARTIAL | **3** (DRAW family — FVR-M) |
| Harness FAIL | **0** |
| NOT_EXECUTED | **0** |

Full-chain mining to **height 2 100 001** is required. Partial h≤501 observation
confirms harness integration only; it does **not** satisfy this archive task.

---

## §4 Runtime Archive Procedure

All steps are **regtest observational** only. Run from a built `core` tree.

### 4.1 Environment

```bash
export DATADIR="${HOME}/.rigor/golden-vector-archive-regtest"
export ARCHIVE_DIR="${HOME}/.rigor/golden-vector-archive-regtest/archive/proof-034"
mkdir -p "${DATADIR}" "${ARCHIVE_DIR}"

cd ./core
make -j"$(nproc)"
```

### 4.2 Start node

**Required:** `rigord` must be running before materializer unless you pass `--start-node`.
Without a node you will see `Could not connect to the server 127.0.0.1:28761`.

Option A — start node manually:

```bash
./src/rigord -regtest -daemon -datadir="${DATADIR}"
./src/rigor-cli -regtest -datadir="${DATADIR}" -rpcwait getblockcount
./src/rigor-cli -regtest -datadir="${DATADIR}" createwallet golden_vector_archive
```

Option B — materializer starts daemon (recommended one-liner):

```bash
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" --start-node batch
```

### 4.3 Mine full chain and materialize all vectors

Mines regtest to h ≈ **2 100 001** and materializes all twelve fixtures.

**Important:** This step mines on the order of **2.1 million regtest blocks** and
may run for **tens of minutes to several hours** depending on CPU and disk. Between
progress lines the process is **not frozen** — `generatetoaddress` runs in batches
of 1000 blocks with minimal console output.

**Terminal A — run materializer (foreground):**

```bash
# If rigord is not already running, add --start-node
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" --start-node batch
```

Expected progress output (every **1000** blocks):

```
Mining 2099500 blocks to height 2100001 (batch size 1000)...
  height=1000
  height=2000
  ...
  height=630000    ← GV-ORU-0003 becomes materializable
  ...
  height=2100000   ← GV-RES-0003 becomes materializable
  height=2100001   ← GV-DRAW-0003 becomes materializable
Materializing golden vectors (target=batch)...
golden_vector_materializer: done.
```

If you see long pauses with **no new lines**, that is normal between `height=`
updates (each line ≈ 1000 blocks mined).

### 4.3.1 Progress monitor and ETA countdown (recommended)

Open **Terminal B** before or immediately after starting §4.3:

```bash
cd ./core
export DATADIR="${HOME}/.rigor/golden-vector-archive-regtest"
./contrib/tools/golden_vector_archive_monitor.sh --datadir="${DATADIR}"
```

Optional flags: `--target-height=2100001` (default), `--poll-sec=30` (default).

<details><summary>Inline Python alternative (same logic)</summary>

```bash
export TARGET_HEIGHT=2100001
export POLL_SEC=30
python3 - <<'PY'
import os, subprocess, sys, time

datadir = os.environ["DATADIR"]
target = int(os.environ.get("TARGET_HEIGHT", "2100001"))
poll = float(os.environ.get("POLL_SEC", "30"))
cli = [
    os.path.join(os.getcwd(), "src", "rigor-cli"),
    "-regtest", f"-datadir={datadir}", "getblockcount",
]
milestones = [
    (630000, "GV-ORU-0003"),
    (2100000, "GV-RES-0003"),
    (2100001, "GV-DRAW-0003 / archive complete"),
]

def get_height():
    out = subprocess.check_output(cli, text=True, stderr=subprocess.STDOUT).strip()
    return int(out)

h_start = None
t_start = None
print(f"PROOF-034 monitor: target height={target:,}, poll={poll}s")
print("Ctrl+C to stop monitor only (mining continues in Terminal A)\n")

while True:
    try:
        h = get_height()
    except subprocess.CalledProcessError as e:
        print(f"[{time.strftime('%H:%M:%S')}] RPC not ready — rigord starting? ({e.output.strip()})")
        time.sleep(poll)
        continue
    except FileNotFoundError:
        sys.exit("rigor-cli not found — run from core/ after make")

    now = time.time()
    if h_start is None:
        h_start, t_start = h, now

    done = max(0, h - h_start)
    remain = max(0, target - h)
    pct = min(100.0, 100.0 * h / target)
    elapsed = now - t_start

    if done > 0 and elapsed >= poll:
        rate = done / elapsed  # blocks per second
        eta_sec = remain / rate if rate > 0 else float("inf")
        if eta_sec < 3600:
            eta = f"{eta_sec / 60:.1f} min"
        elif eta_sec < 86400:
            eta = f"{eta_sec / 3600:.1f} h"
        else:
            eta = f"{eta_sec / 86400:.1f} d"
    else:
        eta = "warming up…"

    next_ms = next(((ht, label) for ht, label in milestones if h < ht), None)
    ms = f"next: h={next_ms[0]:,} ({next_ms[1]})" if next_ms else "milestones: all reached"

    print(
        f"[{time.strftime('%H:%M:%S')}] "
        f"height={h:,}/{target:,}  {pct:6.2f}%  "
        f"remain={remain:,}  ETA≈{eta}  |  {ms}",
        flush=True,
    )

    if h >= target:
        print(f"\n✓ Target height {target:,} reached — materializer should finish vector setup shortly.")
        break

    time.sleep(poll)
PY
```

</details>

Lightweight alternative (no ETA, updates every 10 s):

```bash
watch -n 10 "./src/rigor-cli -regtest -datadir=\"${DATADIR}\" getblockcount"
```

**Healthy run indicators**

| Signal | Meaning |
|--------|---------|
| Monitor `height=` increases every poll | Mining active — **not crashed** |
| Terminal A prints `height=` every ~1000 blocks | Materializer loop alive |
| `rigord` process present (`pgrep -a rigord`) | Node running |
| CPU usage on `rigord` during mining | Normal load, not a hang |

**When to worry**

| Signal | Action |
|--------|--------|
| Height unchanged for **>5 min** after mining started | Check Terminal A for errors; `tail` debug.log |
| RPC errors persist in monitor | Restart rigord; ensure wallet exists (§4.2) |
| Height stuck below 2 100 001 after materializer exited | Re-run materializer or mine remainder manually |

Confirm height after materializer completes:

```bash
./src/rigor-cli -regtest -datadir="${DATADIR}" getblockcount
# expect: 2100001
```

Registry output:

```bash
ls -la "${DATADIR}/regtest/golden_vector_fixtures.json"
# or symlink: ${DATADIR}/golden_vector_fixtures.json
```

### 4.4 Verify vectors (full archive acceptance)

```bash
./contrib/tools/golden_vector_verify.sh \
  --datadir="${DATADIR}" \
  --require-full \
  --verify-fvr \
  batch
```

Expected harness summary line:

```
summary: total=12 pass=9 partial=3 fail=0 not_executed=0 established=12 verified=12
```

### 4.5 Export JSON reports and RPC snapshots

```bash
# Harness report (also written by verify script)
cp "${DATADIR}/golden_vector_harness_report.json" \
   "${ARCHIVE_DIR}/golden_vector_harness_report.json"

# Fixture registry
cp "${DATADIR}/regtest/golden_vector_fixtures.json" \
   "${ARCHIVE_DIR}/golden_vector_fixtures.json"

# Phase-5 final summary @ terminal height
H2100001="$(./src/rigor-cli -regtest -datadir="${DATADIR}" getblockhash 2100001)"
./src/rigor-cli -regtest -datadir="${DATADIR}" getphase5summaryfinal \
  | python3 -m json.tool > "${ARCHIVE_DIR}/phase5_summary_final.json"

# Runtime supply snapshot @ h=2_100_001
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimesupply "${H2100001}" \
  | python3 -m json.tool > "${ARCHIVE_DIR}/runtime_supply_snapshot.json"

# Optional: batch harness RPC raw (if not copied above)
./src/rigor-cli -regtest -datadir="${DATADIR}" getruntimevectorstatus batch true \
  | python3 -m json.tool > "${ARCHIVE_DIR}/getruntimevectorstatus_batch.json"
```

### 4.6 Store archive hashes (verification manifest)

```bash
cd "${ARCHIVE_DIR}"
sha256sum \
  golden_vector_harness_report.json \
  golden_vector_fixtures.json \
  phase5_summary_final.json \
  runtime_supply_snapshot.json \
  > SHA256SUMS

python3 - <<'PY' > verification_manifest.json
import hashlib, json, os, time
from pathlib import Path

archive = Path(os.environ["ARCHIVE_DIR"])
files = [
    "golden_vector_harness_report.json",
    "golden_vector_fixtures.json",
    "phase5_summary_final.json",
    "runtime_supply_snapshot.json",
]
entries = {}
for name in files:
    p = archive / name
    h = hashlib.sha256(p.read_bytes()).hexdigest()
    entries[name] = {"sha256": h, "bytes": p.stat().st_size}

manifest = {
    "task": "PROOF-034",
    "classification": "ENGINEERING_VERIFICATION_ONLY",
    "equivalence_claimed": False,
    "consensus_impact": "NONE",
    "chain_height": 2100001,
    "harness_acceptance": {
        "pass": 9,
        "partial": 3,
        "fail": 0,
        "not_executed": 0,
        "established": 12,
        "verified": 12,
    },
    "pd4f_cpp01": "DS_Open",
    "draw_equivalence": "PROHIBITED",
    "generated_at_unix": int(time.time()),
    "artifacts": entries,
}
print(json.dumps(manifest, indent=2))
PY

sha256sum verification_manifest.json >> SHA256SUMS
```

Archive directory layout:

```
${ARCHIVE_DIR}/
  golden_vector_harness_report.json
  golden_vector_fixtures.json
  phase5_summary_final.json
  runtime_supply_snapshot.json
  verification_manifest.json
  SHA256SUMS
```

---

## §5 Runtime Archive Artifacts

| Artifact | Source | Purpose |
|----------|--------|---------|
| `golden_vector_harness_report.json` | `golden_vector_verify.sh` / `getruntimevectorstatus batch` | Per-vector GVR status, summary counts, `equivalence_claimed` |
| `golden_vector_fixtures.json` | `<datadir>/regtest/` after materializer | ESTABLISHED fixture registry (12 vectors) |
| `phase5_summary_final.json` | `getphase5summaryfinal` @ h=2 100 001 | Phase-5 terminal snapshot RPC export |
| `runtime_supply_snapshot.json` | `getruntimesupply <blockhash>` | FVR-B supply mirror at archive height |
| `verification_manifest.json` | PROOF-034 procedure §4.6 | SHA-256 digests + acceptance metadata |

Optional companion files (not required for acceptance):

- `getruntimevectorstatus_batch.json` — raw batch RPC with FVR field rows
- `SHA256SUMS` — checksum file for offline integrity checks

---

## §6 Acceptance Criteria

Archive is **accepted** only when **all** of the following hold on a chain with
**height ≥ 2 100 001**:

| Criterion | Required value |
|-----------|----------------|
| Harness `pass` | **9** |
| Harness `partial` | **3** (GV-DRAW-0001/0002/0003 under FVR-M) |
| Harness `fail` | **0** |
| Harness `not_executed` | **0** |
| Vectors `established` | **12** |
| Vectors `verified` | **12** |
| `equivalence_claimed` | **false** (root and every vector report) |
| `draw_equivalence` | **PROHIBITED** (FVR-M; DRAW vectors GVR_PARTIAL only) |
| PD4F_CPP_01 | **DS_Open** (Coq/RPC observational; not closed) |
| `mismatch_count` | **0** per established vector |

Automated gate:

```bash
./contrib/tools/golden_vector_verify.sh \
  --datadir="${DATADIR}" \
  --require-full \
  batch
# exit code 0 required
```

Manual cross-checks on `phase5_summary_final.json`:

- `"equivalence_claimed": false`
- `"draw_permanently_non_closable": true`
- `"pd_4f_cpp_01": "OPEN"`
- `"golden_vector_total": 12`
- `"golden_vector_materialized": 9` (Coq frozen h≤501 catalog) **or** harness `established=12` after full run

After full materialization, harness report `established=12` is authoritative for
runtime archive; Coq catalog h≤501 counts remain unchanged (no Coq modification).

---

## §7 Freeze Compatibility

This procedure explicitly preserves:

| Domain | Impact |
|--------|--------|
| Consensus | **NONE** — regtest observational mirrors only |
| FVR semantics | **NONE** — harness reads existing FVR layers |
| `state_hash` | **NONE** — no validation rule changes |
| Monetary policy | **NONE** — fixtures use declared normative values |
| Protocol freeze ([PROOF-031](./PROOF-031-PHASE5-MAINNET-FREEZE.md)) | **NONE** — archive is read-only export |

No Coq proofs, protocol freeze declaration, or CPP-01 status are modified by
this task.

---

## §8 Final Status

| Field | Status |
|-------|--------|
| **Runtime Archive Verification** | **NOT YET EXECUTED** |
| Full Runtime Archive Procedure | **ESTABLISHED** (this document) |
| Archive Artifact Specification | **ESTABLISHED** |
| Acceptance Criteria | **ESTABLISHED** |
| Remaining Runtime Gap | **DOCUMENTED** (3 vectors / h≥630 000–2 100 001) |

Execution completes when an operator produces a real **2 100 001-height** regtest
chain, passes `golden_vector_verify.sh --require-full`, and publishes the
§5 artifact bundle with §4.6 manifest hashes.

Until then:

- Mathematical proof chain: **complete** (PROOF-033 / PROOF-032)
- Full runtime archive: **pending engineering execution**

---

## §9 Required Output Table

| Field | Status |
|-------|--------|
| Full Runtime Archive Procedure | **ESTABLISHED** |
| Archive Artifact Specification | **ESTABLISHED** |
| Acceptance Criteria | **ESTABLISHED** |
| Remaining Runtime Gap | **DOCUMENTED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## §10 Revision History

| Version | Date | Change |
|---------|------|--------|
| 1.0 | 2026-06-06 | TASK-PROOF-034 — full runtime archive verification procedure |

---

## Appendix A — Local execution entry (with progress monitor)

Two-terminal workflow for the full **2 100 001** archive run:

**Terminal A — mine + materialize + verify + export**

```bash
export DATADIR="${HOME}/.rigor/golden-vector-archive-regtest"
export ARCHIVE_DIR="${DATADIR}/archive/proof-034"
mkdir -p "${DATADIR}" "${ARCHIVE_DIR}"
cd ./core
make -j"$(nproc)"

# --start-node starts rigord if not already running (creates wallet inside materializer)
./contrib/tools/golden_vector_materializer.sh --datadir="${DATADIR}" --start-node batch

./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" --require-full --verify-fvr batch
# then §4.5 exports + §4.6 manifest
```

**Terminal B — ETA countdown (start before or with Terminal A)**

```bash
cd ./core
export DATADIR="${HOME}/.rigor/golden-vector-archive-regtest"
./contrib/tools/golden_vector_archive_monitor.sh --datadir="${DATADIR}"
```

Stop monitor with `Ctrl+C` only; mining in Terminal A continues until
`golden_vector_materializer: done.`
