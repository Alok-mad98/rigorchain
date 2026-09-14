#!/usr/bin/env python3
"""Candidate identity tests. Generator is not a Coq KernelInput.

Live node cases skip when rigord is missing (CI without a binary).
"""
from __future__ import annotations

import importlib.util
import json
import os
import shutil
import subprocess
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[2]
GEN_PATH = ROOT / "tools" / "formal_bridge" / "generate_kernel_candidate.py"
TRACES = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "traces.jsonl"
SAMPLE = (
    ROOT
    / "audit"
    / "formal_bridge"
    / "kernel_candidate_dataset"
    / "live_connectblock"
    / "h1-dc73e2e40d8ae06b-accept"
    / "consensus_trace.json"
)
RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tests" / "formal_bridge" / "data_identity"
RPC_USER = "kid"
RPC_PASS = "kid-local"
P2P = 38740
RPC = 38741


def load_gen():
    spec = importlib.util.spec_from_file_location("kigen", GEN_PATH)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


GEN = load_gen()


def hash_of(trace: dict) -> str:
    return GEN.generate_kernel_candidate(trace)["candidate_hash"]


class GeneratorIdentityTests(unittest.TestCase):
    def test_same_trace_one_thousand_times(self) -> None:
        trace = json.loads(SAMPLE.read_text())
        h = hash_of(trace)
        for _ in range(1000):
            self.assertEqual(hash_of(trace), h)

    def test_dataset_replay_two_thousand(self) -> None:
        self.assertTrue(TRACES.is_file(), "missing traces.jsonl; run pack_traces.py")
        lines = [ln for ln in TRACES.read_text().splitlines() if ln.strip()]
        self.assertGreaterEqual(len(lines), 2000)
        for ln in lines:
            obj = json.loads(ln)
            self.assertEqual(hash_of(obj), hash_of(obj))


def rpc(port: int, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "kid", "method": method, "params": list(params)}).encode()
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
            raise RuntimeError(f"{method} HTTP {exc.code}: {raw}") from err
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body["result"]


def start(datadir: Path) -> subprocess.Popen:
    datadir.mkdir(parents=True, exist_ok=True)
    log = (datadir / "stderr.log").open("w")
    proc = subprocess.Popen(
        [
            str(RIGORD),
            "-regtest",
            f"-datadir={datadir}",
            "-bind=127.0.0.1",
            f"-port={P2P}",
            f"-rpcport={RPC}",
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
            "-rigorconsensustrace=1",
        ],
        cwd=str(ROOT),
        stdout=subprocess.DEVNULL,
        stderr=log,
    )
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"rigord exited: {(datadir / 'stderr.log').read_text()[-2000:]}")
        try:
            rpc(RPC, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"RPC not ready: {last}")


def stop(proc: Optional[subprocess.Popen]) -> None:
    try:
        rpc(RPC, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()


def derived_identity(trace: dict) -> dict:
    c = GEN.generate_kernel_candidate(trace)
    return {
        "ki_utxo_ok": c["ki_utxo_ok"],
        "ki_script_ok": c["ki_script_ok"],
        "ki_fees": c["ki_fees"],
        "ki_incoming": c["ki_incoming"],
        "ki_outgoing": c["ki_outgoing"],
        "ki_burn": c["ki_burn"],
        "ki_subsidy": c["ki_subsidy"],
        "ki_res_mint": c["ki_res_mint"],
    }


def latest_h1(trace_dir: Path, prefix: str) -> dict:
    files = sorted(trace_dir.glob(f"h1-{prefix}-*-consensus_trace.json"))
    if not files:
        raise RuntimeError(f"no h1 trace for prefix {prefix}")
    return json.loads(files[-1].read_text())


@unittest.skipUnless(RIGORD.is_file(), "rigord not built")
class LiveIdentityTests(unittest.TestCase):
    def test_invalidate_reconsider_restart_reload(self) -> None:
        if DATA.exists():
            shutil.rmtree(DATA)
        d = DATA / "node"
        proc = start(d)
        try:
            rpc(RPC, "createwallet", "kid")
            addr = rpc(RPC, "getnewaddress", wallet="kid")
            h1 = rpc(RPC, "generatetoaddress", 1, addr, wallet="kid")[0]
            trace_dir = d / "regtest" / "consensus_trace"
            rpc(RPC, "invalidateblock", h1)
            rpc(RPC, "reconsiderblock", h1)
            t0 = latest_h1(trace_dir, h1[:16])
            h0 = hash_of(t0)
            saved = DATA / "saved_h1_trace.json"
            saved.write_text(json.dumps(t0))
            rpc(RPC, "invalidateblock", h1)
            rpc(RPC, "reconsiderblock", h1)
            t1 = latest_h1(trace_dir, h1[:16])
            self.assertEqual(hash_of(t1), h0)
            self.assertEqual(derived_identity(t1), derived_identity(t0))
        finally:
            stop(proc)

        self.assertEqual(hash_of(json.loads(saved.read_text())), h0)

        proc2 = start(d)
        try:
            self.assertEqual(hash_of(json.loads(saved.read_text())), h0)
            rpc(RPC, "invalidateblock", h1)
            rpc(RPC, "reconsiderblock", h1)
            t2 = latest_h1(d / "regtest" / "consensus_trace", h1[:16])
            self.assertEqual(derived_identity(t2), derived_identity(t0))
        finally:
            stop(proc2)


if __name__ == "__main__":
    unittest.main()
