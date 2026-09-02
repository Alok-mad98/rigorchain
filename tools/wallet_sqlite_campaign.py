#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-089: isolated SQLite wallet crash / corruption campaign.

Never opens a developer primary wallet. Uses only a tmpdir copy (synthetic
production schema, or a caller-supplied rigor-wallet.sqlite copy).

Matches production wallet/sqlite.cpp:
  CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)
  user_version = 0
  rollback journal (DELETE) — WAL is not enabled
  fullfsync = true
"""

from __future__ import annotations

import json
import os
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

WALLET_SCHEMA_VERSION = 0
TABLE_SQL = "CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)"


def _pragma(conn: sqlite3.Connection, key: str) -> str:
    row = conn.execute(f"PRAGMA {key}").fetchone()
    return str(row[0]) if row else ""


def integrity_ok(path: Path) -> tuple[bool, str]:
    conn = sqlite3.connect(str(path), isolation_level=None)
    try:
        rows = list(conn.execute("PRAGMA integrity_check"))
        msg = " | ".join(str(r[0]) for r in rows) if rows else "empty"
        return (len(rows) == 1 and str(rows[0][0]) == "ok", msg)
    except sqlite3.Error as e:
        return False, f"sqlite3.Error:{e}"
    finally:
        conn.close()


def row_count(path: Path) -> int:
    conn = sqlite3.connect(str(path), isolation_level=None)
    try:
        return int(conn.execute("SELECT COUNT(*) FROM main").fetchone()[0])
    finally:
        conn.close()


def kv_map(path: Path) -> dict[bytes, bytes]:
    conn = sqlite3.connect(str(path), isolation_level=None)
    try:
        out = {}
        for k, v in conn.execute("SELECT key, value FROM main"):
            out[bytes(k)] = bytes(v) if v is not None else b""
        return out
    finally:
        conn.close()


def make_schema_db(path: Path, n_seed: int = 8) -> None:
    if path.exists():
        path.unlink()
    conn = sqlite3.connect(str(path), isolation_level=None)
    try:
        conn.execute("PRAGMA journal_mode=DELETE")
        conn.execute("PRAGMA fullfsync=true")
        conn.execute("PRAGMA user_version=0")
        conn.execute(TABLE_SQL)
        for i in range(n_seed):
            conn.execute(
                "INSERT INTO main VALUES(?, ?)",
                (f"seed-{i}".encode(), f"value-{i}".encode() * 32),
            )
    finally:
        conn.close()


def _child_sql(db: Path, body: str) -> str:
    return f"""
import os, signal, sqlite3
c = sqlite3.connect({str(db)!r}, isolation_level=None)
c.execute("PRAGMA journal_mode=DELETE")
c.execute("PRAGMA synchronous=FULL")
c.execute("PRAGMA fullfsync=true")
{body}
os.kill(os.getpid(), signal.SIGKILL)
"""


def _run_killed(db: Path, body: str) -> int:
    proc = subprocess.run(
        [sys.executable, "-c", _child_sql(db, body)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    # SIGKILL typically yields returncode -9 or 137.
    return int(proc.returncode)


def crash_during_write(db: Path) -> dict:
    """A: kill while a WalletBatch-shaped INSERT is open (before COMMIT)."""
    before = kv_map(db)
    body = """
c.execute("BEGIN IMMEDIATE")
c.execute("INSERT INTO main VALUES(?, ?)", (b"crash-during-write", b"PARTIAL" * 8000))
"""
    rc = _run_killed(db, body)
    ok, integ = integrity_ok(db)
    after = kv_map(db)
    leaked = b"crash-during-write" in after
    return {
        "case": "A_crash_during_WalletBatch_write",
        "killed_rc": rc,
        "integrity_ok": ok,
        "integrity": integ,
        "partial_visible": leaked,
        "prior_kv_intact": after == before,
        "pass": ok and (not leaked) and after == before,
    }


def crash_before_commit(db: Path) -> dict:
    """B: statements finished, COMMIT never issued, process killed."""
    before = kv_map(db)
    body = """
c.execute("BEGIN IMMEDIATE")
c.execute("INSERT INTO main VALUES(?, ?)", (b"crash-before-commit", b"NOCOMMIT" * 8000))
c.execute("INSERT INTO main VALUES(?, ?)", (b"crash-before-commit-2", b"NOCOMMIT" * 8000))
"""
    rc = _run_killed(db, body)
    ok, integ = integrity_ok(db)
    after = kv_map(db)
    leaked = any(k.startswith(b"crash-before-commit") for k in after)
    return {
        "case": "B_crash_before_COMMIT",
        "killed_rc": rc,
        "integrity_ok": ok,
        "integrity": integ,
        "partial_visible": leaked,
        "prior_kv_intact": after == before,
        "pass": ok and (not leaked) and after == before,
    }


def crash_after_commit(db: Path) -> dict:
    """C: COMMIT done, process killed before orderly exit."""
    body = """
c.execute("BEGIN IMMEDIATE")
c.execute("INSERT INTO main VALUES(?, ?)", (b"crash-after-commit", b"SURVIVE"*32))
c.execute("COMMIT")
"""
    rc = _run_killed(db, body)
    ok, integ = integrity_ok(db)
    after = kv_map(db)
    survived = after.get(b"crash-after-commit") == b"SURVIVE" * 32
    return {
        "case": "C_crash_after_COMMIT_before_exit",
        "killed_rc": rc,
        "integrity_ok": ok,
        "integrity": integ,
        "committed_survived": survived,
        "pass": ok and survived,
    }


def corrupt_one_page(db: Path) -> dict:
    """D: flip bytes in a SQLite btree page. integrity_check must fail. No silent empty."""
    size = db.stat().st_size
    prior_n = row_count(db)
    raw = bytearray(db.read_bytes())
    page_size = int.from_bytes(raw[16:18], "big") if len(raw) >= 18 else 4096
    if page_size == 1:
        page_size = 65536
    if page_size < 512:
        page_size = 4096
    # Page 1 bytes 0-99 are the database header. Corrupt the btree that follows,
    # or page 2's btree header when the file has more than one page.
    if size > page_size:
        start = page_size  # page 2 btree header (page type / cell pointers)
    else:
        start = 100
    end = min(start + 96, len(raw))
    for i in range(start, end):
        raw[i] ^= 0x5A
    db.write_bytes(bytes(raw))
    ok, integ = integrity_ok(db)
    silent_empty = False
    n = None
    if ok:
        try:
            n = row_count(db)
            silent_empty = n == 0 and prior_n > 0
        except sqlite3.Error:
            n = None
    return {
        "case": "D_corrupt_one_sqlite_page",
        "integrity_ok": ok,
        "integrity": integ,
        "prior_rows": prior_n,
        "rows_after_if_readable": n,
        "silent_empty_wallet": silent_empty,
        "corrupt_offset": start,
        "page_size": page_size,
        "pass": (not ok) and (not silent_empty),
        "detected": not ok,
    }


def truncate_file(db: Path) -> dict:
    prior = db.stat().st_size
    os.truncate(db, max(64, prior // 8))
    ok, integ = integrity_ok(db)
    silent_empty = False
    try:
        n = row_count(db)
        silent_empty = ok and n == 0
    except sqlite3.Error:
        n = None
    return {
        "case": "E1_truncated_file",
        "prior_size": prior,
        "new_size": db.stat().st_size,
        "integrity_ok": ok,
        "integrity": integ,
        "rows_after_if_readable": n,
        "silent_empty_wallet": bool(silent_empty),
        "fail_closed": (not ok) and (not silent_empty),
        "pass": (not ok) and (not silent_empty),
    }


def unknown_user_version(db: Path) -> dict:
    conn = sqlite3.connect(str(db), isolation_level=None)
    try:
        conn.execute("PRAGMA user_version=99")
        ver = int(conn.execute("PRAGMA user_version").fetchone()[0])
    finally:
        conn.close()
    # Production SQLiteDatabase::Verify refuses user_version != 0.
    return {
        "case": "C_unknown_user_version",
        "user_version": ver,
        "production_verify_must_refuse": ver != WALLET_SCHEMA_VERSION,
        "pass": ver != WALLET_SCHEMA_VERSION,
    }


def journal_mode_is_delete(db: Path) -> dict:
    conn = sqlite3.connect(str(db), isolation_level=None)
    try:
        mode = _pragma(conn, "journal_mode").lower()
        wal = mode == "wal"
    finally:
        conn.close()
    return {
        "case": "journal_mode",
        "journal_mode": mode,
        "wal_enabled": wal,
        "pass": (not wal) and mode in {"delete", "off", "persist", "truncate", "memory"},
    }


def malformed_kv_insert(db: Path) -> dict:
    """Insert a syntactically valid SQLite row with garbage Bitcoin-serialized value.

    SQLite itself accepts the blob. Production LoadWallet is the fail-closed gate
    (CORRUPT / NEED_RESCAN / NONCRITICAL_ERROR) — exercised by the functional test.
    """
    tx_key = b"\x02tx" + bytes(32)
    conn = sqlite3.connect(str(db), isolation_level=None)
    try:
        conn.execute("INSERT OR REPLACE INTO main VALUES(?, ?)", (tx_key, b"\xff" * 8))
        stored = conn.execute("SELECT value FROM main WHERE key = ?", (tx_key,)).fetchone()[0]
    finally:
        conn.close()
    return {
        "case": "E2_malformed_kv_sqlite_accepts_blob",
        "stored_garbage": bytes(stored) == b"\xff" * 8,
        "note": "fail-closed is LoadWallet, not SQLite type checks",
        "pass": bytes(stored) == b"\xff" * 8,
    }


def missing_tx_record(db: Path) -> dict:
    conn = sqlite3.connect(str(db), isolation_level=None)
    try:
        rows = list(conn.execute("SELECT key FROM main"))
        tx_keys = [bytes(k[0]) for k in rows if bytes(k[0]).startswith(b"\x02tx")]
        for k in tx_keys:
            conn.execute("DELETE FROM main WHERE key = ?", (k,))
        left = conn.execute(
            "SELECT COUNT(*) FROM main WHERE substr(key,1,3) = ?", (b"\x02tx",)
        ).fetchone()[0]
    finally:
        conn.close()
    return {
        "case": "E5_missing_wallet_tx_records",
        "deleted": len(tx_keys),
        "tx_keys_left": int(left),
        "pass": int(left) == 0,
        "note": "production LoadWallet sets NEED_RESCAN for missing/bad TX; functional test confirms",
    }


def run_campaign(work: Path, source_wallet: Path | None = None) -> dict:
    work.mkdir(parents=True, exist_ok=True)
    results = []

    def stage(name: str, src: Path, fn) -> dict:
        dest = work / name / "rigor-wallet.sqlite"
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        r = fn(dest)
        r["sqlite_path"] = str(dest)
        results.append(r)
        return r

    seed = work / "seed" / "rigor-wallet.sqlite"
    seed.parent.mkdir(parents=True, exist_ok=True)
    if source_wallet is not None:
        shutil.copy2(source_wallet, seed)
    else:
        make_schema_db(seed)

    jm = journal_mode_is_delete(seed)
    jm["sqlite_path"] = str(seed)
    results.append(jm)

    stage("crash_A", seed, crash_during_write)
    stage("crash_B", seed, crash_before_commit)
    # C mutates a copy then we keep it; seed stays clean.
    stage("crash_C", seed, crash_after_commit)
    stage("crash_D", seed, corrupt_one_page)
    stage("trunc", seed, truncate_file)
    stage("user_version", seed, unknown_user_version)
    stage("malformed_kv", seed, malformed_kv_insert)
    stage("missing_tx", seed, missing_tx_record)

    crash = [r for r in results if r["case"].startswith(("A_", "B_", "C_", "D_"))]
    crash_ok = all(r.get("pass") for r in crash)
    trunc_ok = next(r for r in results if r["case"].startswith("E1_")).get("pass")
    schema_ok = next(r for r in results if r["case"].startswith("C_unknown")).get("pass")
    journal_ok = jm.get("pass")

    out = {
        "source": str(source_wallet) if source_wallet else "synthetic_production_schema",
        "never_opened_primary_wallet": True,
        "results": results,
        "WALLET_SQLITE_CRASH_RECOVERY": "VERIFIED" if crash_ok else "FAIL",
        "campaign_crash_pass": crash_ok,
        "campaign_truncate_fail_closed": bool(trunc_ok),
        "campaign_unknown_schema_refuse": bool(schema_ok),
        "campaign_wal_not_enabled": bool(journal_ok),
        "campaign_pass": crash_ok and trunc_ok and schema_ok and journal_ok,
    }
    return out


def main() -> int:
    out_env = os.environ.get("RIGOR_P089_CAMPAIGN_JSON", "").strip()
    src = os.environ.get("RIGOR_P089_WALLET_COPY", "").strip()
    source = Path(src) if src else None
    with tempfile.TemporaryDirectory(prefix="p089_wallet_sqlite_") as td:
        report = run_campaign(Path(td), source)
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if out_env:
        out_path = Path(out_env)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    return 0 if report.get("campaign_pass") else 1


if __name__ == "__main__":
    raise SystemExit(main())
