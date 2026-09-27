#!/usr/bin/env python3
""": prove a release binary is the current-runtime node (rigord).

Fails closed if the file is the identity stub (RIGOR_RELEASE_IDENTITY=PASS).
Does not touch formal_verification/, Coq, or extract.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

FORBIDDEN_TOKEN = "RIGOR_RELEASE_IDENTITY=PASS"
MIN_DAEMON_BYTES = 1_000_000
START_TIMEOUT_SEC = 90


def die(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_capture(cmd: list[str], timeout: int = 30) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )


def reject_identity_text(label: str, text: str) -> None:
    if FORBIDDEN_TOKEN in text:
        die(f"{label}: identity stub token {FORBIDDEN_TOKEN!r} is not a node")


def check_exists(path: Path) -> None:
    if not path.is_file():
        die(f"missing file {path}")
    print(f"FILE_EXISTS={path}")


def check_sha256(path: Path) -> str:
    digest = sha256_file(path)
    if not digest or len(digest) != 64:
        die(f"sha256 missing for {path}")
    sidecar = path.with_suffix(path.suffix + ".sha256")
    if path.name.endswith(".tar.gz"):
        sidecar = Path(str(path) + ".sha256")
    sidecar.write_text(f"{digest}  {path.name}\n", encoding="utf-8")
    print(f"SHA256 {path.name} {digest}")
    return digest


def check_version(daemon: Path) -> str:
    proc = run_capture([str(daemon), "-version"])
    out = proc.stdout or ""
    reject_identity_text("--version", out)
    if proc.returncode != 0:
        die(f"--version exit {proc.returncode}: {out[:400]}")
    if "RIGOR version" not in out:
        die("--version did not print RIGOR version (not rigord)")
    print(out.rstrip())
    print("VERSION_OK=PASS")
    return out


def check_help(daemon: Path) -> None:
    proc = run_capture([str(daemon), "-help"])
    out = proc.stdout or ""
    reject_identity_text("--help", out)
    if proc.returncode != 0:
        die(f"--help exit {proc.returncode}: {out[:400]}")
    if "Usage:" not in out and "-datadir" not in out:
        die("--help missing Usage/datadir (not a node)")
    print("HELP_OK=PASS")


def wait_debug_log(debug_log: Path, deadline: float) -> None:
    needles = (
        "Done loading",
        "Loading block index",
        "init message",
        "block tree size",
    )
    while time.time() < deadline:
        if debug_log.is_file():
            text = debug_log.read_text(encoding="utf-8", errors="replace")
            reject_identity_text("debug.log", text)
            if any(n in text for n in needles):
                return
        time.sleep(0.4)
    extra = ""
    if debug_log.is_file():
        extra = debug_log.read_text(encoding="utf-8", errors="replace")[-800:]
    die(f"daemon did not reach node init within {START_TIMEOUT_SEC}s\n{extra}")


def check_startup(daemon: Path, cli: Path | None) -> None:
    datadir = Path(tempfile.mkdtemp(prefix="rigor-release-datadir-"))
    port = 18444
    proc: subprocess.Popen[str] | None = None
    try:
        cmd = [
            str(daemon),
            "-regtest",
            f"-datadir={datadir}",
            "-listen=0",
            "-server=1",
            "-connect=0",
            f"-port={port}",
        ]
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        debug_log = datadir / "regtest" / "debug.log"
        wait_debug_log(debug_log, time.time() + START_TIMEOUT_SEC)
        if not (datadir / "regtest").is_dir():
            die("datadir/regtest was not created")
        print(f"DATADIR_CREATED={datadir / 'regtest'}")
        print("NODE_INIT=PASS")

        if cli is not None and cli.is_file():
            rpc = run_capture(
                [str(cli), "-regtest", f"-datadir={datadir}", "getblockcount"],
                timeout=30,
            )
            reject_identity_text("rpc", rpc.stdout or "")
            if rpc.returncode != 0:
                die(f"RPC getblockcount failed: {rpc.stdout}")
            print(f"RPC_GETBLOCKCOUNT={(rpc.stdout or '').strip()}")
            print("RPC_OK=PASS")
            run_capture([str(cli), "-regtest", f"-datadir={datadir}", "stop"], timeout=30)
        else:
            print("RPC_OK=SKIP (no rigor-cli)")
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=20)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
        shutil.rmtree(datadir, ignore_errors=True)
    print("STARTUP_OK=PASS")


def find_in_dir(root: Path, names: tuple[str, ...]) -> Path | None:
    for name in names:
        hits = list(root.rglob(name))
        if hits:
            return hits[0]
    return None


def extract_package(artifact_dir: Path, tmp: Path) -> Path:
    tars = list(artifact_dir.glob("*.tar.gz"))
    zips = list(artifact_dir.glob("*.zip"))
    if tars:
        with tarfile.open(tars[0], "r:gz") as tf:
            tf.extractall(tmp)
        return tmp
    if zips:
        with zipfile.ZipFile(zips[0]) as zf:
            zf.extractall(tmp)
        return tmp
    return artifact_dir


def resolve_bins(args: argparse.Namespace) -> tuple[Path, Path | None]:
    if args.daemon:
        daemon = Path(args.daemon).resolve()
        cli = Path(args.cli).resolve() if args.cli else None
        return daemon, cli
    if not args.artifact_dir:
        die("pass --daemon or --artifact-dir")
    artifact_dir = Path(args.artifact_dir).resolve()
    if not artifact_dir.is_dir():
        die(f"artifact dir missing: {artifact_dir}")
    tmp = Path(tempfile.mkdtemp(prefix="rigor-release-unpack-"))
    unpacked = extract_package(artifact_dir, tmp)
    daemon = find_in_dir(unpacked, ("rigord", "rigord.exe"))
    if daemon is None:
        ident = find_in_dir(unpacked, ("rigor-release-identity", "rigor-release-identity.exe"))
        if ident is not None:
            die(f"package contains identity stub {ident}, not rigord")
        die(f"rigord not found under {artifact_dir}")
    cli = find_in_dir(unpacked, ("rigor-cli", "rigor-cli.exe"))
    return daemon, cli


def main() -> int:
 parser = argparse.ArgumentParser(description=" release binary identity (must be rigord)")
    parser.add_argument("--daemon", help="path to rigord")
    parser.add_argument("--cli", help="path to rigor-cli")
    parser.add_argument("--artifact-dir", help="directory with tar.gz/zip or unpacked bin/")
    parser.add_argument("--skip-start", action="store_true", help="skip empty-datadir startup (cross-built exe)")
    args = parser.parse_args()

    daemon, cli = resolve_bins(args)
    check_exists(daemon)
    if "identity" in daemon.name.lower():
        die(f"{daemon.name} is not a node binary")
    size = daemon.stat().st_size
    if size < MIN_DAEMON_BYTES:
        die(f"{daemon} is {size} bytes; identity stub / not CURRENT_RUNTIME_ARCHITECTURE")
    print(f"DAEMON={daemon}")
    print(f"DAEMON_BYTES={size}")
    check_sha256(daemon)
    check_version(daemon)
    check_help(daemon)
    if args.skip_start:
        print("STARTUP_OK=SKIP")
    else:
        if not os.access(daemon, os.X_OK):
            die(f"{daemon} is not executable")
        check_startup(daemon, cli)
    print("BINARY_IDENTITY=PASS")
    print("ARTIFACT_IS_RIGORD=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as exc:
        die(f"command timed out: {exc}")
