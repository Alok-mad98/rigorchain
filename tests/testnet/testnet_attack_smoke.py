#!/usr/bin/env python3
"""Network attack smoke — behaviour exists, not a security proof."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from rpc_util import (
    ROOT,
    TOOLS,
    reset_chain,
    rpc,
    start_stack,
    stop_stack,
    wait_peers,
    wait_rpc,
    wait_tips_equal,
    sh,
)
from test_multinode_sync import mine_one


def _start_stranger() -> tuple[subprocess.Popen, Path]:
    tmp = Path(tempfile.mkdtemp(prefix="rigor-stranger-"))
    conf = tmp / "rigor.conf"
    conf.write_text(
        "\n".join(
            [
                "testnet=1",
                "",
                "[test]",
                "listen=1",
                "server=1",
                "bind=127.0.0.1",
                "port=18790",
                "rpcport=18791",
                "rpcbind=127.0.0.1",
                "rpcallowip=127.0.0.1",
                "rpcuser=rigortest",
                "rpcpassword=rigortest-local-only",
                "dnsseed=0",
                "discover=0",
                "listenonion=0",
                "whitelist=127.0.0.1",
                "addnode=127.0.0.1:18760",
                "",
            ]
        ),
        encoding="utf-8",
    )
    rigord = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
    log = tmp / "stranger.stderr"
    proc = subprocess.Popen(
        [
            str(rigord),
            "-testnet",
            f"-datadir={tmp}",
            f"-conf={conf}",
            "-printtoconsole=0",
        ],
        cwd=str(ROOT),
        stdout=subprocess.DEVNULL,
        stderr=log.open("w"),
    )
    return proc, tmp


def stranger_rpc(method: str, *params):
    import urllib.request

    payload = json.dumps({"jsonrpc": "1.0", "id": "s", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request("http://127.0.0.1:18791/", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, "http://127.0.0.1:18791/", "rigortest", "rigortest-local-only")
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    with opener.open(req, timeout=30) as resp:
        body = json.loads(resp.read().decode())
    if body.get("error"):
        raise RuntimeError(body["error"])
    return body["result"]


def main() -> int:
    reset_chain()
    stranger = None
    stranger_dir = None
    try:
        start_stack()
        wait_peers("node-001", 1)
        mined = mine_one()
        wait_tips_equal()
        print(f"BASE_TIP={mined}")

        # 1. stranger connects
        stranger, stranger_dir = _start_stranger()
        deadline = time.time() + 45
        connected = False
        last = None
        while time.time() < deadline:
            try:
                last = stranger_rpc("getpeerinfo")
                if last:
                    connected = True
                    break
            except Exception as exc:  # noqa: BLE001
                last = exc
            time.sleep(0.5)
        if not connected:
            err = ""
            if stranger_dir is not None:
                p = stranger_dir / "stranger.stderr"
                if p.is_file():
                    err = p.read_text(encoding="utf-8", errors="replace")[-2000:]
            raise RuntimeError(f"stranger did not connect: {last}\n{err}")
        print(f"STRANGER_PEERS={len(last)}")

        # 2. duplicate block broadcast
        tip = rpc("node-002", "getbestblockhash")
        raw = rpc("node-002", "getblock", tip, 0)
        dup = None
        try:
            dup = rpc("node-002", "submitblock", raw)
        except RuntimeError as exc:
            dup = str(exc)
        print(f"DUPLICATE_SUBMIT={dup!r}")

        # 3. invalid block rejected
        rejected = False
        try:
            rpc("node-003", "submitblock", "00")
        except RuntimeError:
            rejected = True
        if not rejected:
            # submitblock often returns a string reject reason instead of throwing
            reason = rpc("node-003", "submitblock", "00")
            if reason in (None, "duplicate", "inconclusive"):
                raise RuntimeError(f"invalid block not rejected: {reason!r}")
            rejected = True
            print(f"INVALID_REJECT={reason!r}")
        else:
            print("INVALID_REJECT=exception")

        # 4. disconnect / reconnect miner
        sh("bash", str(TOOLS / "stop-all.sh"))
        time.sleep(1)
        sh("bash", str(TOOLS / "start-bootstrap.sh"))
        sh("bash", str(TOOLS / "start-miner.sh"))
        sh("bash", str(TOOLS / "start-validator.sh"))
        wait_rpc("node-001")
        wait_rpc("node-002")
        wait_rpc("node-003")
        wait_peers("node-002", 1)
        print("RECONNECT=PASS")

        # 5. restart recovers chainstate
        tips = wait_tips_equal()
        if tips["node-001"]["hash"] != mined:
            raise RuntimeError(f"restart tip mismatch {tips}")
        print("RECOVER=" + json.dumps(tips))
        print("TESTNET_ATTACK_SMOKE=PASS")
        return 0
    finally:
        if stranger is not None:
            stranger.terminate()
            try:
                stranger.wait(timeout=10)
            except subprocess.TimeoutExpired:
                stranger.kill()
        stop_stack()
        if stranger_dir is not None:
            subprocess.run(["rm", "-rf", str(stranger_dir)], check=False)


if __name__ == "__main__":
    sys.exit(main())
