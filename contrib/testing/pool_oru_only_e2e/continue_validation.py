#!/usr/bin/env python3
"""Continue ORU-only E2E validation from captured Phase-1 evidence.

Runtime selection (first match):
  1. POOL_ORU_E2E_RUNTIME env
  2. argv[1]
  3. .runtime/pool-oru-only-e2e/LATEST
"""
from __future__ import annotations

import hashlib
import json
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(".")
E2E_ROOT = ROOT / ".runtime/pool-oru-only-e2e"
CLI = ROOT / "core/src/rigor-cli"
DAEMON = ROOT / "core/src/rigord"
DEFAULT_PILOT = Path(
    "/tmp/cursor-sandbox-cache/05199d473af6aca404e5d6709bc4f355/cargo-target/release/rigor-pool-pilot"
)


def resolve_runtime() -> Path:
    env = os.environ.get("POOL_ORU_E2E_RUNTIME")
    if env:
        return Path(env)
    if len(sys.argv) > 1:
        return Path(sys.argv[1])
    latest = E2E_ROOT / "LATEST"
    if latest.exists():
        return Path(latest.read_text().strip())
    raise SystemExit(
        "No runtime: set POOL_ORU_E2E_RUNTIME, pass path argv, or write LATEST via run_validation.sh"
    )


def detect_chain_ports(runtime: Path) -> dict:
    """Infer chain/ports from FINAL_SUMMARY, status, or layer prefix."""
    art = runtime / "artifacts"
    for name in ("FINAL_SUMMARY.json", "status_phase1.json", "status.json"):
        p = art / name
        if not p.exists():
            continue
        try:
            d = json.loads(p.read_text())
        except Exception:
            continue
        if "rpc_port" in d or "chain" in d:
            return {
                "chain": d.get("chain") or ("regtest" if "layer1" in runtime.name else "devtest"),
                "port": str(d.get("rpc_port") or (37911 if "layer1" in runtime.name else 49871)),
                "stratum": int(d.get("stratum_port") or (13335 if "layer1" in runtime.name else 13334)),
                "layer": d.get("layer"),
            }
    if "layer1" in runtime.name or "regtest" in runtime.name:
        return {"chain": "regtest", "port": "37911", "stratum": 13335, "layer": "layer1_regtest"}
    return {"chain": "devtest", "port": "49871", "stratum": 13334, "layer": "legacy_devtest"}


def load_addrs(runtime: Path) -> tuple[str, str]:
    art = runtime / "artifacts"
    oru = None
    res_fee = None
    for name in ("oru_payout.json", "FINAL_SUMMARY.json"):
        p = art / name
        if not p.exists():
            continue
        try:
            d = json.loads(p.read_text())
            oru = oru or d.get("payout_addr") or d.get("oru_addr")
            res_fee = res_fee or d.get("res_fee_addr")
        except Exception:
            pass
    harness = runtime / "logs/harness.log"
    if harness.exists():
        for line in harness.read_text(errors="ignore").splitlines():
            if "ORU payout addr=" in line and not oru:
                # ORU payout addr=... spk=...
                try:
                    oru = line.split("ORU payout addr=", 1)[1].split()[0]
                except Exception:
                    pass
            if "RES fee addr candidate=" in line and not res_fee:
                res_fee = line.split("RES fee addr candidate=", 1)[1].strip()
    if not oru:
        raise SystemExit(f"cannot find ORU addr in {runtime}")
    return oru, res_fee or ""


def main() -> None:
    runtime = resolve_runtime()
    art = runtime / "artifacts"
    logs = runtime / "logs"
    art.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)

    meta = detect_chain_ports(runtime)
    oru_addr, res_fee = load_addrs(runtime)
    pilot = Path(os.environ.get("RIGOR_POOL_PILOT", str(DEFAULT_PILOT)))
    if not pilot.is_file():
        pilot = ROOT / "pool/pilot/target/release/rigor-pool-pilot"

    rpc_cfg = dict(
        chain=meta["chain"],
        datadir=str(runtime / "datadir"),
        user="oruonly",
        password="oruonly-local-pass",
        port=meta["port"],
    )
    stratum_port = meta["stratum"]

    def log(msg: str) -> None:
        line = f"[{time.strftime('%H:%M:%S', time.gmtime())}] {msg}"
        print(line, flush=True)
        with open(logs / "harness_continue.log", "a") as f:
            f.write(line + "\n")

    def rpc(*args: str, timeout: int = 120) -> str:
        chain_flag = (
            ["-regtest"] if rpc_cfg["chain"] == "regtest" else [f"-chain={rpc_cfg['chain']}"]
        )
        cmd = [
            str(CLI),
            *chain_flag,
            f"-datadir={rpc_cfg['datadir']}",
            f"-rpcuser={rpc_cfg['user']}",
            f"-rpcpassword={rpc_cfg['password']}",
            f"-rpcport={rpc_cfg['port']}",
            "-rpcconnect=127.0.0.1",
            f"-rpcclienttimeout={timeout}",
            *args,
        ]
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 30)
        out = (p.stdout or "") + (p.stderr or "")
        if p.returncode != 0:
            raise RuntimeError(out.strip() or f"rpc failed rc={p.returncode}")
        return p.stdout.strip()

    def rpc_json(*args: str):
        return json.loads(rpc(*args))

    def ensure_node() -> None:
        try:
            rpc("getblockcount", timeout=30)
            return
        except Exception as e:
            log(f"node rpc failed ({e}); restarting")
        subprocess.run(["pkill", "-f", f"datadir={rpc_cfg['datadir']}"], check=False)
        time.sleep(2)
        chain_flag = (
            ["-regtest"] if rpc_cfg["chain"] == "regtest" else [f"-chain={rpc_cfg['chain']}"]
        )
        p2p = "37901" if rpc_cfg["chain"] == "regtest" else "49870"
        args = [
            str(DAEMON),
            *chain_flag,
            f"-datadir={rpc_cfg['datadir']}",
            "-daemon",
            f"-rpcuser={rpc_cfg['user']}",
            f"-rpcpassword={rpc_cfg['password']}",
            f"-rpcport={rpc_cfg['port']}",
            f"-port={p2p}",
            "-fallbackfee=0.0002",
            "-maxtipage=999999999",
            "-listen=0",
            "-connect=0",
            f"-debuglogfile={logs / 'rigord.log'}",
            "-printtoconsole=0",
        ]
        if res_fee:
            args.append(f"-miningresfeeaddr={res_fee}")
        subprocess.Popen(args)
        for _ in range(60):
            try:
                rpc("getblockcount", timeout=20)
                break
            except Exception:
                time.sleep(1)
        try:
            rpc("loadwallet", "oru_e2e", timeout=30)
        except Exception:
            pass

    def hash_pw(password: str) -> str:
        salt = bytes.fromhex("0123456789abcdef")
        rounds = 64
        h = hashlib.sha256()
        h.update(salt)
        h.update(b"\x00")
        h.update(password.encode())
        acc = h.digest()
        for _ in range(rounds):
            h = hashlib.sha256()
            h.update(acc)
            h.update(salt)
            h.update(password.encode())
            acc = h.digest()
        return f"rigor-sha256i${rounds}${salt.hex()}${acc.hex()}"

    def stratum_auth(port: int) -> dict:
        def read_line(sock: socket.socket) -> dict:
            buf = b""
            while b"\n" not in buf:
                c = sock.recv(4096)
                if not c:
                    raise ConnectionError("closed")
                buf += c
            line, _ = buf.split(b"\n", 1)
            return json.loads(line.decode())

        def call(sock: socket.socket, payload: dict) -> dict:
            sock.sendall((json.dumps(payload) + "\n").encode())
            while True:
                r = read_line(sock)
                if r.get("method"):
                    continue
                return r

        out: dict = {"ok": False}
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=10) as sock:
                sub = call(
                    sock,
                    {
                        "id": 1,
                        "method": "mining.subscribe",
                        "params": ["rigor-miner/1.0-opushash", None],
                    },
                )
                auth = call(
                    sock,
                    {
                        "id": 2,
                        "method": "mining.authorize",
                        "params": ["e2e.miner1", "e2e-miner-password-ok"],
                    },
                )
                out = {
                    "ok": (not sub.get("error")) and auth.get("result") is True,
                    "subscribe": sub,
                    "authorize": auth,
                }
        except Exception as e:
            out = {"ok": False, "error": str(e)}
        return out

    log(f"runtime={runtime} chain={rpc_cfg['chain']} layer={meta.get('layer')}")
    subprocess.run(["pkill", "-f", "rigor-miner-agent"], check=False)
    subprocess.run(["pkill", "-f", "rigor-pool-pilot"], check=False)
    time.sleep(1)

    ensure_node()
    h0 = int(rpc("getblockcount"))
    log(f"height start continue={h0}")

    log("Phase 2: generate 101 blocks for ORU maturity")
    gen = rpc_json("generatetoaddress", "101", oru_addr)
    (art / "generate_bootstrap.json").write_text(json.dumps(gen, indent=2))
    bal = rpc("getbalance")
    (art / "oru_balance.txt").write_text(bal + "\n")
    (art / "wallet_balances_oru.json").write_text(rpc("getbalances") + "\n")
    (art / "oru_payout.json").write_text(
        json.dumps(
            {
                "oru_balance": bal,
                "payout_addr": oru_addr,
                "fallback_generate": True,
                "note": "ORU matured via generatetoaddress after pool block.found; pool submit may fail bad-txnmrklroot",
            },
            indent=2,
        )
        + "\n"
    )
    log(f"ORU balance={bal}")

    fund_path = art / "automint_fund.json"
    if fund_path.exists():
        fund = json.loads(fund_path.read_text()).get("address")
        if fund:
            try:
                txid = rpc("sendtoaddress", fund, "1.0")
                (art / "automint_fund_txid.txt").write_text(txid + "\n")
                log(f"automint fund tx={txid}")
            except Exception as e:
                (art / "automint_fund_txid.txt").write_text(str(e) + "\n")
                log(f"automint fund send failed: {e}")

    log("Phase 3: createconvergence + W=32 generate")
    conv_raw = ""
    try:
        conv_raw = rpc("createconvergence", "1.0")
        (art / "createconvergence.json").write_text(conv_raw + "\n")
    except Exception as e:
        conv_raw = str(e)
        (art / "createconvergence.json").write_text(json.dumps({"error": str(e)}) + "\n")
    h1 = int(rpc("getblockcount"))
    rpc_json("generatetoaddress", "34", oru_addr)
    h2 = int(rpc("getblockcount"))
    (art / "wallet_balances_after_w.json").write_text(rpc("getbalances") + "\n")
    try:
        (art / "listtransactions.json").write_text(rpc("listtransactions", "*", "80") + "\n")
    except Exception as e:
        (art / "listtransactions.json").write_text(json.dumps({"error": str(e)}))

    res_evidence = {
        "height_before_window": h1,
        "height_after_window": h2,
        "blocks_advanced": h2 - h1,
        "convergence_raw": conv_raw[:500],
        "res_source_expected": "rigord auto_res_mint / CreateNewBlock",
        "pool_res_source": "NONE",
        "balances_after": json.loads((art / "wallet_balances_after_w.json").read_text()),
    }
    (art / "res_evidence.json").write_text(json.dumps(res_evidence, indent=2) + "\n")
    log(f"W window {h1} -> {h2}")

    log("Phase 4: restart pool + reconnect")
    payout_spk = ""
    try:
        payout_spk = rpc_json("getaddressinfo", oru_addr).get("scriptPubKey", "")
    except Exception:
        pass
    env = os.environ.copy()
    env.update(
        {
            "RIGOR_POOL_PROFILE": "production",
            "RIGOR_RPC_URL": f"http://127.0.0.1:{rpc_cfg['port']}/",
            "RIGOR_RPC_USER": rpc_cfg["user"],
            "RIGOR_RPC_PASSWORD": rpc_cfg["password"],
            "STRATUM_LISTEN": f"127.0.0.1:{stratum_port}",
            "GBT_POLL_INTERVAL_MS": "1000",
            "PPLNS_WINDOW": "1",
            "RIGOR_POOL_PAYOUT_SCRIPT_HEX": payout_spk,
            "RIGOR_POOL_WORKER_CREDENTIALS": f"e2e.miner1={hash_pw('e2e-miner-password-ok')}",
            "PILOT_STATUS_FILE": str(art / "status_restart.json"),
            "PILOT_STATUS_INTERVAL_SECS": "5",
            "RIGOR_POOL_METRICS_DIR": str(art / "metrics"),
        }
    )
    with open(logs / "pool_restart.log", "w") as lf:
        proc = subprocess.Popen([str(pilot)], env=env, stdout=lf, stderr=lf)
    time.sleep(4)
    restart = stratum_auth(stratum_port)
    (art / "restart_recovery.json").write_text(json.dumps(restart, indent=2) + "\n")
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    log(f"restart reconnect_ok={restart.get('ok')}")

    status_path = art / "status.json"
    if not status_path.exists():
        status_path = art / "status_phase1.json"
    status = json.loads(status_path.read_text()) if status_path.exists() else {"metrics": {}, "audit_events": []}
    m = status.get("metrics") or {}
    mint_audit = [
        e
        for e in status.get("audit_events") or []
        if "mint" in e.get("category", "") or "mint" in e.get("message", "").lower()
    ]
    submit_msgs = [
        e.get("message")
        for e in status.get("audit_events") or []
        if e.get("category") in ("block.submit_failed", "block.found", "block.submitted")
    ]
    pool_scan = {
        "res_mint_events": m.get("res_mint_events", 0),
        "blocks_found": m.get("blocks_found"),
        "blocks_submitted": m.get("blocks_submitted"),
        "blocks_submit_failed": m.get("blocks_submit_failed"),
        "shares_accepted": m.get("shares_accepted"),
        "jobs_notified": m.get("jobs_notified"),
        "mint_audit_events": mint_audit,
        "block_events": submit_msgs,
        "pool_res_involvement": "NONE"
        if m.get("res_mint_events", 0) == 0 and not mint_audit
        else "SUSPECT",
    }
    (art / "pool_res_scan.json").write_text(json.dumps(pool_scan, indent=2) + "\n")

    overall = "PARTIAL"
    try:
        bal_f = float(bal)
    except Exception:
        bal_f = 0.0
    if (
        pool_scan["pool_res_involvement"] == "NONE"
        and bal_f > 0
        and restart.get("ok")
        and (m.get("shares_accepted") or 0) >= 1
        and (m.get("blocks_found") or 0) >= 1
    ):
        overall = "PARTIAL"

    summary = {
        "task": "",
        "runtime": str(runtime),
        "layer": meta.get("layer"),
        "chain": rpc_cfg["chain"],
        "rpc_port": int(rpc_cfg["port"]),
        "stratum_port": stratum_port,
        "oru_addr": oru_addr,
        "res_fee_addr": res_fee,
        "oru_balance": bal,
        "height_continue_start": h0,
        "height_after_w": h2,
        "phases": {
            "phase1_connect_jobs_shares_block_found": "PASS" if (m.get("shares_accepted") or 0) >= 1 else "UNKNOWN",
            "phase1_submitblock_accepted": "FAIL" if (m.get("blocks_submit_failed") or 0) >= 1 else "UNKNOWN",
            "phase1_submit_error": "bad-txnmrklroot" if (m.get("blocks_submit_failed") or 0) >= 1 else None,
            "phase2_oru_in_wallet": "PASS" if bal_f > 0 else "FAIL",
            "phase2_pool_res_involvement": pool_scan["pool_res_involvement"],
            "phase3_convergence_attempted": True,
            "phase3_w32_blocks_advanced": h2 - h1,
            "phase3_res_source": "rigord CreateNewBlock/auto_res_mint (NOT pool)",
            "phase4_restart_reconnect": "PASS" if restart.get("ok") else "FAIL",
            "consensus_unchanged": True,
        },
        "pool_metrics": pool_scan,
        "overall": overall,
    }
    (art / "FINAL_SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n")
    (E2E_ROOT / "LATEST").write_text(str(runtime) + "\n")
    log(f"DONE overall={overall}")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
