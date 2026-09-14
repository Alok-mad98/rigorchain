"""Two isolated regtest nodes. Not a consensus change. Not a restored architecture."""
from __future__ import annotations

import json
import os
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, List, Optional

ROOT = Path(__file__).resolve().parents[2]
RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
RIGOR_CLI = Path(os.environ.get("RIGOR_CLI", str(ROOT / "core" / "src" / "rigor-cli")))
DATA = ROOT / "tests" / "attack_lab" / "data"
RPC_USER = "attacklab"
RPC_PASS = "attacklab-local"

NODES = {
    "A": {"p2p": 38470, "rpc": 38471},
    "B": {"p2p": 38480, "rpc": 38481},
}


def rpc(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    port = NODES[node]["rpc"]
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "lab", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=60) as resp:
            body = json.loads(resp.read().decode())
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode()
        try:
            body = json.loads(raw)
        except json.JSONDecodeError as err:
            raise RuntimeError(f"{node} {method} HTTP {exc.code}: {raw}") from err
    if body.get("error"):
        raise RpcError(body["error"])
    return body["result"]


class RpcError(RuntimeError):
    def __init__(self, err: Any):
        self.err = err
        super().__init__(str(err))

    @property
    def code(self) -> Any:
        return self.err.get("code") if isinstance(self.err, dict) else None

    @property
    def message(self) -> str:
        if isinstance(self.err, dict):
            return str(self.err.get("message", self.err))
        return str(self.err)


def datadir(node: str) -> Path:
    return DATA / f"node-{node.lower()}"


def start_node(node: str) -> subprocess.Popen:
    d = datadir(node)
    d.mkdir(parents=True, exist_ok=True)
    if not RIGORD.is_file():
        raise RuntimeError(f"missing {RIGORD}; build core/src/rigord")
    ports = NODES[node]
    log = (d / "stderr.log").open("w")
    proc = subprocess.Popen(
        [
            str(RIGORD),
            "-regtest",
            f"-datadir={d}",
            "-bind=127.0.0.1",
            f"-port={ports['p2p']}",
            f"-rpcport={ports['rpc']}",
            f"-rpcuser={RPC_USER}",
            f"-rpcpassword={RPC_PASS}",
            "-rpcbind=127.0.0.1",
            "-rpcallowip=127.0.0.1",
            "-server=1",
            "-listen=1",
            "-dnsseed=0",
            "-discover=0",
            "-listenonion=0",
            "-fallbackfee=0.00001000",
            "-printtoconsole=0",
        ],
        cwd=str(ROOT),
        stdout=subprocess.DEVNULL,
        stderr=log,
    )
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"{node} exited: {(d / 'stderr.log').read_text()[-1500:]}")
        try:
            rpc(node, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"{node} RPC not ready: {last}")


def ensure_wallet(node: str, name: str = "lab") -> None:
    try:
        rpc(node, "loadwallet", name)
    except RpcError as exc:
        text = exc.message.lower()
        if "already loaded" in text or "duplicate" in text:
            return
        if "not found" in text or "failed" in text or "does not exist" in text:
            rpc(node, "createwallet", name)
            return
        try:
            rpc(node, "createwallet", name)
        except RpcError:
            pass


def stop_node(proc: Optional[subprocess.Popen], node: str) -> None:
    try:
        rpc(node, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()


def reset_data() -> None:
    if DATA.exists():
        subprocess.run(["rm", "-rf", str(DATA)], check=False)
    DATA.mkdir(parents=True, exist_ok=True)


def call_ok(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> dict:
    try:
        result = rpc(node, method, *params, wallet=wallet)
        return {"decision": "accept", "result": result, "error": None}
    except RpcError as exc:
        return {"decision": "reject", "result": None, "error": exc.message, "code": exc.code}
