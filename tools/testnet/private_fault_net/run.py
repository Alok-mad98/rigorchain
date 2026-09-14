#!/usr/bin/env python3
"""Seven-node private regtest fault network. Not public. Not a consensus change."""
from __future__ import annotations

import json
import os
import shutil
import struct
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[3]
RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tools" / "testnet" / "private_fault_net" / "data"
NET = ROOT / "audit" / "network"
RPC_USER = "evidence"
RPC_PASS = "evidence-local"

# node-001 bootstrap, 002 miner, 003 validator, 004 delayed,
# 005 malicious, 006 restart, 007 partition
NODES = {
    "node-001": {"p2p": 38810, "rpc": 38811, "role": "bootstrap"},
    "node-002": {"p2p": 38820, "rpc": 38821, "role": "miner"},
    "node-003": {"p2p": 38830, "rpc": 38831, "role": "validator"},
    "node-004": {"p2p": 38840, "rpc": 38841, "role": "delayed"},
    "node-005": {"p2p": 38850, "rpc": 38851, "role": "malicious"},
    "node-006": {"p2p": 38860, "rpc": 38861, "role": "restart"},
    "node-007": {"p2p": 38870, "rpc": 38871, "role": "partition"},
}


class RpcError(RuntimeError):
    def __init__(self, err: Any):
        self.err = err
        super().__init__(str(err))

    @property
    def message(self) -> str:
        if isinstance(self.err, dict):
            return str(self.err.get("message", self.err))
        return str(self.err)


def rpc(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    port = NODES[node]["rpc"]
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "fn", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://127.0.0.1:{port}/", RPC_USER, RPC_PASS)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=90) as resp:
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


def call_ok(node: str, method: str, *params: Any) -> dict:
    try:
        return {"decision": "accept", "result": rpc(node, method, *params), "error": None}
    except RpcError as exc:
        return {"decision": "reject", "result": None, "error": exc.message}


def datadir(node: str) -> Path:
    return DATA / node


def start(node: str, wipe: bool = True) -> subprocess.Popen:
    d = datadir(node)
    if wipe and d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True, exist_ok=True)
    if not RIGORD.is_file():
        raise RuntimeError(f"missing {RIGORD}")
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
            "-whitelist=127.0.0.1",
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
            raise RuntimeError(f"{node} exited: {(d / 'stderr.log').read_text()[-2000:]}")
        try:
            rpc(node, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"{node} RPC not ready: {last}")


def stop(proc: Optional[subprocess.Popen], node: str) -> None:
    try:
        rpc(node, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=20)
    except Exception:
        proc.kill()


def ensure_wallet(node: str, name: str = "w") -> None:
    try:
        rpc(node, "loadwallet", name)
    except RpcError as exc:
        text = exc.message.lower()
        if "already loaded" in text or "duplicate" in text:
            return
        try:
            rpc(node, "createwallet", name)
        except RpcError:
            pass


def utxo_hash(info: dict) -> Any:
    return info.get("hash_serialized_3") or info.get("hash_serialized_2") or info.get("hash")


def append(path: Path, obj: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(obj, sort_keys=True) + "\n")


def addnode(a: str, b: str) -> None:
    addr = f"127.0.0.1:{NODES[b]['p2p']}"
    try:
        rpc(a, "addnode", addr, "add")
    except RpcError as exc:
        if "already added" not in exc.message.lower():
            raise
    try:
        rpc(a, "addnode", addr, "onetry")
    except RpcError:
        pass


def wait_tip(node: str, tip: str, timeout: float = 45.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if rpc(node, "getbestblockhash") == tip:
            return True
        time.sleep(0.3)
    return False


def disconnect_all(node: str) -> None:
    for p in rpc(node, "getpeerinfo"):
        try:
            rpc(node, "disconnectnode", "", p["id"])
        except RpcError:
            try:
                rpc(node, "disconnectnode", p.get("addr"))
            except RpcError:
                pass


def patch_nbits(block_hex: str, nbits: int) -> str:
    raw = bytearray(bytes.fromhex(block_hex))
    raw[72:76] = struct.pack("<I", nbits & 0xFFFFFFFF)
    return bytes(raw).hex()


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n")


def run() -> dict:
    if DATA.exists():
        shutil.rmtree(DATA)
    NET.mkdir(parents=True, exist_ok=True)
    for name in ("block_events.jsonl", "peer_events.jsonl", "reorg_events.jsonl"):
        p = NET / name
        if p.exists():
            p.unlink()

    procs: dict[str, subprocess.Popen] = {}
    summary: dict[str, Any] = {"nodes": list(NODES), "faults": []}
    try:
        for n in NODES:
            procs[n] = start(n, wipe=True)
            append(NET / "peer_events.jsonl", {"node": n, "event": "start", "role": NODES[n]["role"]})

        ensure_wallet("node-002")
        addr = rpc("node-002", "getnewaddress", wallet="w")
        addnode("node-002", "node-001")
        addnode("node-003", "node-001")
        addnode("node-001", "node-002")
        addnode("node-001", "node-003")

        before = rpc("node-002", "gettxoutsetinfo")
        t0 = time.time()
        hashes = rpc("node-002", "generatetoaddress", 110, addr, wallet="w")
        tip = hashes[-1]
        after = rpc("node-002", "gettxoutsetinfo")
        wait_tip("node-001", tip)
        wait_tip("node-003", tip)
        delay_ms = int((time.time() - t0) * 1000)
        append(
            NET / "block_events.jsonl",
            {
                "height": 110,
                "hash": tip,
                "previous": rpc("node-002", "getblockheader", tip).get("previousblockhash"),
                "time": rpc("node-002", "getblockheader", tip).get("time"),
                "tx_count": 1,
                "validation": "accept",
                "utxo_before": utxo_hash(before),
                "utxo_after": utxo_hash(after),
                "propagation_ms_001_003": delay_ms,
                "future_theorem": "network assumption",
            },
        )

        # latency / packet delay: hold node-004 off the mesh, then join late
        time.sleep(2.0)
        addnode("node-004", "node-001")
        late_ok = wait_tip("node-004", tip, timeout=30)
        append(
            NET / "peer_events.jsonl",
            {
                "node": "node-004",
                "event": "delayed_join",
                "caught_up": late_ok,
                "bootstrap_source": "127.0.0.1:38810",
                "future_theorem": "network assumption",
            },
        )
        summary["faults"].append({"fault": "latency_packet_delay", "ok": late_ok})

        # malicious peer: sync, then submit illegal nBits (not a consensus change)
        addnode("node-005", "node-001")
        wait_tip("node-005", tip, timeout=30)
        hx = rpc("node-002", "getblock", tip, 0)
        bad = call_ok("node-005", "submitblock", patch_nbits(hx, 0))
        append(
            NET / "block_events.jsonl",
            {
                "height": 110,
                "hash": None,
                "validation": "reject",
                "rejection_layer": bad.get("error") or bad.get("result") or "reject",
                "node": "node-005",
                "future_theorem": "validation_soundness",
            },
        )
        summary["faults"].append({"fault": "malicious_invalid_block", "reason": bad.get("error") or bad.get("result")})

        # temporary disconnect + reconnect of validator
        disconnect_all("node-003")
        time.sleep(0.5)
        addnode("node-003", "node-001")
        recon = wait_tip("node-003", tip, timeout=20)
        append(
            NET / "peer_events.jsonl",
            {
                "node": "node-003",
                "event": "reconnect",
                "ok": recon,
                "future_theorem": "network assumption",
            },
        )
        summary["faults"].append({"fault": "temporary_disconnect", "ok": recon})

        # restart node
        tip_before = rpc("node-001", "getbestblockhash")
        utxo_before = utxo_hash(rpc("node-006", "gettxoutsetinfo"))
        addnode("node-006", "node-001")
        wait_tip("node-006", tip_before, timeout=30)
        utxo_mid = utxo_hash(rpc("node-006", "gettxoutsetinfo"))
        stop(procs["node-006"], "node-006")
        procs["node-006"] = start("node-006", wipe=False)
        same_tip = rpc("node-006", "getbestblockhash") == tip_before
        same_utxo = utxo_hash(rpc("node-006", "gettxoutsetinfo")) == utxo_mid
        append(
            NET / "peer_events.jsonl",
            {
                "node": "node-006",
                "event": "restart",
                "same_tip": same_tip,
                "same_utxo": same_utxo,
                "future_theorem": "persistence_correctness",
            },
        )
        summary["faults"].append({"fault": "node_restart", "same_tip": same_tip, "same_utxo": same_utxo})

        # chain split + rejoin (partition node mines a longer private fork)
        addnode("node-007", "node-001")
        wait_tip("node-007", tip, timeout=30)
        old_tip = rpc("node-002", "getbestblockhash")
        disconnect_all("node-007")
        ensure_wallet("node-007")
        addr7 = rpc("node-007", "getnewaddress", wallet="w")
        rpc("node-002", "generatetoaddress", 2, addr, wallet="w")
        fork = rpc("node-007", "generatetoaddress", 5, addr7, wallet="w")
        split_a = rpc("node-002", "getbestblockhash")
        split_b = fork[-1]
        addnode("node-007", "node-001")
        addnode("node-001", "node-007")
        deadline = time.time() + 40
        while time.time() < deadline:
            if rpc("node-002", "getbestblockhash") == rpc("node-007", "getbestblockhash"):
                break
            time.sleep(0.4)
        new_tip = rpc("node-002", "getbestblockhash")
        adopted = new_tip == split_b
        append(
            NET / "reorg_events.jsonl",
            {
                "case": "partition_rejoin",
                "old_tip": old_tip,
                "miner_tip_during_split": split_a,
                "partition_tip": split_b,
                "new_tip": new_tip,
                "disconnect_count": 2 if adopted else None,
                "connect_count": 5 if adopted else None,
                "final_state": utxo_hash(rpc("node-002", "gettxoutsetinfo")),
                "future_theorem": "chain_selection",
            },
        )
        summary["faults"].append(
            {
                "fault": "chain_split_rejoin",
                "old_tip": old_tip,
                "new_tip": new_tip,
                "adopted_longer_fork": adopted,
            }
        )

        orphan = call_ok("node-003", "submitblock", patch_nbits(rpc("node-002", "getblock", new_tip, 0), 0))
        # unknown-parent orphan: flip prev
        raw = bytearray(bytes.fromhex(rpc("node-002", "getblock", new_tip, 0)))
        raw[4:36] = bytes.fromhex("cd" * 32)
        orphan2 = call_ok("node-003", "submitblock", bytes(raw).hex())
        append(
            NET / "block_events.jsonl",
            {
                "validation": "reject",
                "orphan": True,
                "rejection_layer": orphan2.get("error") or orphan2.get("result") or orphan.get("error"),
                "future_theorem": "network assumption",
            },
        )

        summary["final_tips"] = {n: rpc(n, "getbestblockhash") for n in NODES}
        summary["peer_counts"] = {n: len(rpc(n, "getpeerinfo")) for n in NODES}
        summary["bootstrap_source"] = "127.0.0.1:38810"
        write_json(NET / "fault_network_summary.json", summary)

        lines = [
            "# Private fault network (7 nodes)",
            "",
            "Private `-regtest` only. Not public. Environment evidence, not a consensus proof.",
            "",
            "| id | role | p2p | rpc |",
            "|----|------|-----|-----|",
        ]
        for n, p in NODES.items():
            lines.append(f"| {n} | {p['role']} | {p['p2p']} | {p['rpc']} |")
        lines += [
            "",
            "## Faults covered",
            "",
            "- latency / delayed join (`node-004`)",
            "- temporary disconnect / reconnect (`node-003`)",
            "- node restart (`node-006`)",
            "- chain split + rejoin (`node-007` vs miner)",
            "- malicious invalid block (`node-005`)",
            "- orphan / unknown parent",
            "",
            f"- longer fork adopted: `{adopted}`",
            f"- restart same tip/utxo: `{same_tip}` / `{same_utxo}`",
            f"- delayed join caught up: `{late_ok}`",
            "",
            "Future names: `chain_selection`, `utxo_inverse`, `network assumption`, `persistence_correctness`.",
            "",
        ]
        (NET / "fault_network.md").write_text("\n".join(lines), encoding="utf-8")
        return summary
    finally:
        for n, proc in procs.items():
            stop(proc, n)


def main() -> int:
    summary = run()
    print(json.dumps({k: summary[k] for k in ("faults", "bootstrap_source") if k in summary}, indent=2))
    print(NET)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
