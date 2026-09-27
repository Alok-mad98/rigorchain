#!/usr/bin/env python3
"""Live harvest: ConnectBlock accept + CheckTxInputs reject.

Does not change consensus. Uses -rigorconsensustrace=1 only.
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[2]
RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
RIGOR_CLI = Path(os.environ.get("RIGOR_CLI", str(ROOT / "core" / "src" / "rigor-cli")))
DATA = ROOT / "tests" / "attack_lab" / "data_task007"
OUT = ROOT / "audit" / "formal_bridge" / "kernel_candidate_dataset" / "live_connectblock"
RPC_USER = "task007"
RPC_PASS = "task007-local"
P2P = 38710
RPC = 38711
P2P_OFF = 38720
RPC_OFF = 38721


def rpc(port: int, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "t7", "method": method, "params": list(params)}).encode()
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


def start(datadir: Path, p2p: int, rpcport: int, trace: bool) -> subprocess.Popen:
    if datadir.exists():
        shutil.rmtree(datadir)
    datadir.mkdir(parents=True, exist_ok=True)
    log = (datadir / "stderr.log").open("w")
    args = [
        str(RIGORD),
        "-regtest",
        f"-datadir={datadir}",
        "-bind=127.0.0.1",
        f"-port={p2p}",
        f"-rpcport={rpcport}",
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
    ]
    if trace:
        args.append("-rigorconsensustrace=1")
    proc = subprocess.Popen(args, cwd=str(ROOT), stdout=subprocess.DEVNULL, stderr=log)
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"rigord exited: {(datadir / 'stderr.log').read_text()[-2000:]}")
        try:
            rpc(rpcport, "getblockcount")
            return proc
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(0.2)
    raise RuntimeError(f"RPC not ready: {last}")


def stop(proc: Optional[subprocess.Popen], rpcport: int) -> None:
    try:
        rpc(rpcport, "stop")
    except Exception:
        pass
    if proc is None:
        return
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()


def harvest_on() -> None:
    d = DATA / "on"
    proc = start(d, P2P, RPC, True)
    try:
        rpc(RPC, "createwallet", "t7")
        addr = rpc(RPC, "getnewaddress", wallet="t7")
        hashes = rpc(RPC, "generatetoaddress", 1, addr, wallet="t7")
        mined_hex = rpc(RPC, "getblock", hashes[0], 0)
        # Mature a coinbase, then force a ConnectBlock double-spend reject.
        rpc(RPC, "generatetoaddress", 100, addr, wallet="t7")
        utxos = rpc(RPC, "listunspent", 1, 9999999, [], wallet="t7")
        utxos.sort(key=lambda u: u["amount"], reverse=True)
        u = utxos[0]
        amt = round(float(u["amount"]) - 0.001, 8)
        dest = addr
        raw = rpc(
            RPC,
            "createrawtransaction",
            [{"txid": u["txid"], "vout": u["vout"]}],
            {dest: amt},
        )
        signed = rpc(RPC, "signrawtransactionwithwallet", raw, wallet="t7")
        if not signed.get("complete"):
            raise RuntimeError(signed)
        tx = signed["hex"]
        reject_reason = None
        try:
            rpc(RPC, "generateblock", dest, [tx, tx], wallet="t7")
            reject_reason = "generateblock unexpectedly accepted double-spend"
        except RuntimeError as exc:
            reject_reason = str(exc)
        print("accept_h1", hashes[0])
        print("double_spend_reject", reject_reason)
        (d / "mined_h1.hex").write_text(mined_hex + "\n")
        (d / "reject_reason.txt").write_text(reject_reason + "\n")
        genesis_hex = rpc(RPC, "getblock", rpc(RPC, "getblockhash", 0), 0)
        (d / "genesis.hex").write_text(genesis_hex + "\n")
    finally:
        stop(proc, RPC)

    trace_dir = d / "regtest" / "consensus_trace"
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True, exist_ok=True)
    if not trace_dir.is_dir():
        raise RuntimeError(f"no live traces at {trace_dir}")

    def keep(src: Path, hex_path: Optional[Path]) -> None:
        stem = src.name.replace("-consensus_trace.json", "")
        dest = OUT / stem
        dest.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest / "consensus_trace.json")
        cand = src.with_name(src.name.replace("-consensus_trace.json", "-kernel_candidate.json"))
        if cand.is_file():
            shutil.copy2(cand, dest / "kernel_candidate.json")
        if hex_path and hex_path.is_file():
            shutil.copy2(hex_path, dest / "block.hex")

    h1 = next(trace_dir.glob(f"h1-{hashes[0][:16]}-accept-consensus_trace.json"))
    keep(h1, d / "mined_h1.hex")
    keep(next(trace_dir.glob("h0-*-accept-consensus_trace.json")), d / "genesis.hex")
    keep(next(trace_dir.glob("*-reject-consensus_trace.json")), None)
    print("kept", sorted(p.name for p in OUT.iterdir() if p.is_dir()))


def harvest_off() -> None:
    d = DATA / "off"
    proc = start(d, P2P_OFF, RPC_OFF, False)
    try:
        rpc(RPC_OFF, "createwallet", "t7")
        addr = rpc(RPC_OFF, "getnewaddress", wallet="t7")
        hashes = rpc(RPC_OFF, "generatetoaddress", 1, addr, wallet="t7")
        print("off_accept_h1", hashes[0], "height", rpc(RPC_OFF, "getblockcount"))
    finally:
        stop(proc, RPC_OFF)
    trace_dir = d / "regtest" / "consensus_trace"
    leftover = list(trace_dir.glob("*.json")) if trace_dir.is_dir() else []
    if leftover:
        raise RuntimeError(f"flag-off wrote traces: {leftover}")
    print("flag_off_zero_files")


def main() -> int:
    if not RIGORD.is_file():
        raise SystemExit(f"missing {RIGORD}")
    harvest_on()
    harvest_off()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
