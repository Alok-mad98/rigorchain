#!/usr/bin/env python3
"""One-node smoke: createconvergence mint hunt (debug helper)."""
from __future__ import annotations

import json
import subprocess
import sys
import time
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]  # .../lib -> local_mainnet_simulation -> testing -> contrib -> ROOT
R = ROOT / ".runtime" / "lms-mint-dbg"
PORT = 27992
RIGORD = ROOT / "core" / "src" / "rigord"
CLI = ROOT / "core" / "src" / "rigor-cli"


def cli(*a: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(CLI), "-regtest", f"-rpcport={PORT}", "-rpcuser=u", "-rpcpassword=p", *a],
        capture_output=True,
        text=True,
        timeout=180,
    )


def main() -> int:
    shutil.rmtree(R, ignore_errors=True)
    R.mkdir(parents=True)
    subprocess.run(
        [
            str(RIGORD),
            "-regtest",
            f"-datadir={R}",
            "-server",
            "-listen=0",
            "-rpcuser=u",
            "-rpcpassword=p",
            f"-rpcport={PORT}",
            "-fallbackfee=0.0002",
            "-daemon",
        ],
        check=True,
    )
    for _ in range(50):
        if cli("getblockcount").returncode == 0:
            break
        time.sleep(0.2)
    assert cli("createwallet", "w").returncode == 0
    fee = cli("-rpcwallet=w", "getnewaddress", "fee", "bech32m").stdout.strip()
    print("fee", fee, flush=True)
    cli("stop")
    for _ in range(40):
        lock = R / "regtest" / ".lock"
        if not lock.exists():
            break
        time.sleep(0.25)
    time.sleep(0.5)
    subprocess.run(
        [
            str(RIGORD),
            "-regtest",
            f"-datadir={R}",
            "-server",
            "-listen=0",
            "-rpcuser=u",
            "-rpcpassword=p",
            f"-rpcport={PORT}",
            "-fallbackfee=0.0002",
            f"-miningresfeeaddr={fee}",
            "-daemon",
        ],
        check=True,
    )
    for _ in range(50):
        if cli("getblockcount").returncode == 0:
            break
        time.sleep(0.2)
    cli("loadwallet", "w")
    addr = cli("-rpcwallet=w", "getnewaddress", "m", "bech32").stdout.strip()
    assert cli("-rpcwallet=w", "generatetoaddress", "120", addr).returncode == 0
    for rnd in range(80):
        for _ in range(2):
            r = cli("-rpcwallet=w", "createconvergence", "1000.0")
            if r.returncode != 0:
                cli("-rpcwallet=w", "createconvergence", "100.0")
        r = cli("-rpcwallet=w", "generatetoaddress", "1", addr)
        if r.returncode != 0:
            print("FAIL award", rnd, r.stderr, flush=True)
            cli("stop")
            return 1
        h = int(cli("getblockcount").stdout)
        bh = cli("getblockhash", str(h)).stdout.strip()
        fr = cli("getconvergenceresult", json.dumps(bh))
        d = json.loads(fr.stdout)
        win = d.get("draw_winner_txid", "0" * 64)
        elig = sum(1 for c in d.get("candidates", []) if c.get("draw_eligible"))
        print(
            f"round {rnd} h={h} |F|={d.get('convergence_root_set_size')} elig={elig} win={win[:16]}",
            flush=True,
        )
        r = cli("-rpcwallet=w", "generatetoaddress", "1", addr)
        if r.returncode != 0:
            print("FAIL payout", rnd, r.stderr, flush=True)
            cli("stop")
            return 2
        ph = cli("getbestblockhash").stdout.strip()
        hdr = json.loads(cli("getblockheader", ph).stdout)
        if int(hdr.get("rigor_res_minted", 0) or 0) == 1:
            print("MINT OK", flush=True)
            print(cli("-rpcwallet=w", "getbalances").stdout, flush=True)
            cli("stop")
            return 0
    print("no mint in 80 rounds", flush=True)
    cli("stop")
    return 3


if __name__ == "__main__":
    sys.exit(main())
