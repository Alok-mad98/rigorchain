"""JSON-RPC helper for the private 3-node testnet. Not a consensus test."""
from __future__ import annotations

import json
import os
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "testnet"
RPC_USER = "rigortest"
RPC_PASS = "rigortest-local-only"

NODES = {
    "node-001": 18761,
    "node-002": 18771,
    "node-003": 18781,
}


def rpc(node: str, method: str, *params, wallet: Optional[str] = None):
    port = NODES[node]
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "tn", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}{path}",
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=30) as resp:
            body = json.loads(resp.read().decode())
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode()
        try:
            body = json.loads(raw)
        except json.JSONDecodeError as err:
            raise RuntimeError(f"{node} {method} HTTP {exc.code}: {raw}") from err
    if body.get("error"):
        raise RuntimeError(f"{node} {method}: {body['error']}")
    return body["result"]


def wait_rpc(node: str, timeout: float = 60.0) -> None:
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        try:
            rpc(node, "getblockcount")
            return
        except Exception as exc:  # noqa: BLE001 — poll until up
            last = exc
            time.sleep(0.5)
    raise RuntimeError(f"{node} RPC not ready: {last}")


def sh(*args: str, check: bool = True) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    proc = subprocess.run(args, cwd=ROOT, check=False, env=env, text=True, capture_output=True)
    if check and proc.returncode != 0:
        raise RuntimeError(
            f"cmd {args} exited {proc.returncode}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
    return proc


def start_stack() -> None:
    sh("bash", str(TOOLS / "start-bootstrap.sh"))
    sh("bash", str(TOOLS / "start-miner.sh"))
    sh("bash", str(TOOLS / "start-validator.sh"))
    for node in NODES:
        wait_rpc(node)


def stop_stack() -> None:
    sh("bash", str(TOOLS / "stop-all.sh"), check=False)


def reset_chain() -> None:
    sh("bash", str(TOOLS / "reset-chain.sh"), check=False)


def wait_peers(node: str, minimum: int = 1, timeout: float = 45.0) -> int:
    deadline = time.time() + timeout
    last = 0
    while time.time() < deadline:
        last = len(rpc(node, "getpeerinfo"))
        if last >= minimum:
            return last
        time.sleep(0.5)
    raise RuntimeError(f"{node} peer count {last} < {minimum}")


def wait_tips_equal(timeout: float = 45.0) -> dict:
    deadline = time.time() + timeout
    last = {}
    while time.time() < deadline:
        last = {n: {"height": rpc(n, "getblockcount"), "hash": rpc(n, "getbestblockhash"), "peers": len(rpc(n, "getpeerinfo"))} for n in NODES}
        hashes = {row["hash"] for row in last.values()}
        heights = {row["height"] for row in last.values()}
        if len(hashes) == 1 and len(heights) == 1:
            return last
        time.sleep(0.5)
    raise RuntimeError(f"tips diverged or not synced: {last}")
