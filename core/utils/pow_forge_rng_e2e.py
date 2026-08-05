#!/usr/bin/env python3
"""
Devtest E2E: mine to height 200, convergence 1 + 10, RES block, ORU/RES fee checks.
Requires: rigord already running with fresh datadir; wallet loaded.

RES mint gate (important consensus note):
  - N = RNG_MIN_FORGE_TXS (=2): the block must contain at least N distinct convergence
    transactions (non-coinbase), counted only inside that block.
  - No cross-block comparison: parent/sibling blocks' convergence txs do not count.
  - If convergence_txs_in_block < N => that block has no convergence draw winner and no coupled body RES mint.
"""
from __future__ import annotations

import csv
import json
import os
import subprocess
import sys
import time
from decimal import Decimal
from pathlib import Path


def rpc_cmd(base: list[str], *args: str) -> str:
    r = subprocess.run(base + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip() or r.stdout.strip() or "rpc failed")
    return r.stdout.strip()


def rpc_json(base: list[str], *args: str):
    return json.loads(rpc_cmd(base, *args))


def is_convergence_tx(tx: dict) -> bool:
    for vo in tx.get("vout", []):
        if vo.get("asset") != "ORU":
            continue
        if Decimal(str(vo.get("value", 0))) <= 0:
            continue
        sp = vo.get("scriptPubKey", {})
        if sp.get("type") == "nulldata" and "convergence" in sp.get("asm", ""):
            return True
    return False


def fee_comment(fee_val) -> str:
    try:
        d = Decimal(str(fee_val))
    except Exception:
        return "unknown"
    if d < 0:
        return f"ORU fee paid (~{-d} from negative fee field)"
    return str(d)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    cli = str(root / "src/rigor-cli")
    datadir = os.environ.get("RINGCORE_DATADIR", "/tmp/ringcore-pow-orbis-test")
    rpcport = os.environ.get("RINGCORE_RPCPORT", "")
    wallet = "devtestwallet"
    csv_path = root / "pow_rng_full_test.csv"
    convergence_builder = root / "utils" / "convergence_tx_builder.py"

    base = [cli, "-chain=devtest", f"-datadir={datadir}", f"-rpcwallet={wallet}"]
    if rpcport:
        base.insert(-1, f"-rpcport={rpcport}")

    rows: list[dict] = []

    def log(phase: str, **kwargs):
        rows.append({"ts": int(time.time()), "phase": phase, **kwargs})

    h0 = int(rpc_cmd(base, "getblockcount"))
    bal0 = Decimal(rpc_cmd(base, "getbalance"))
    log("start", block_height=str(h0), balance_oru=str(bal0))

    addr_mine = rpc_cmd(base, "getnewaddress", "miner", "legacy")
    target_h = 200
    while int(rpc_cmd(base, "getblockcount")) < target_h:
        cur = int(rpc_cmd(base, "getblockcount"))
        chunk = min(50, target_h - cur)
        rpc_json(base, "generatetoaddress", str(chunk), addr_mine)

    h1 = int(rpc_cmd(base, "getblockcount"))
    bal1 = Decimal(rpc_cmd(base, "getbalance"))
    expected = Decimal(200 * 1000)
    log(
        "after_mine_200",
        block_height=str(h1),
        balance_oru=str(bal1),
        expected_subsidy_oru=str(expected),
        balance_ok=str(bal1 >= expected * Decimal("0.99")),
        note="Wallet mines 200 blocks at 1000 ORU; genesis 50 not in wallet",
    )

    def run_convergence():
        r = subprocess.run(
            [
                sys.executable,
                str(convergence_builder),
                "--cli",
                cli,
                "--chain",
                "devtest",
                "--cli-extra=-datadir=" + datadir,
                "--cli-extra=-rpcwallet=" + wallet,
            ]
            + ([f"--cli-extra=-rpcport={rpcport}"] if rpcport else [])
            + [
                "--amount",
                "1",
                "--convergence-data-hex",
                "464f524745",
                "--min-priority",
                "-1",
                "--json",
            ],
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip() or r.stdout.strip())
        return json.loads(r.stdout)["txid"]

    tx_f1 = run_convergence()
    rpc_json(base, "generatetoaddress", "1", rpc_cmd(base, "getnewaddress", "m1", "legacy"))
    log("convergence_1", txid=tx_f1)

    tx10 = []
    for _ in range(10):
        tx10.append(run_convergence())
    rpc_json(base, "generatetoaddress", "1", rpc_cmd(base, "getnewaddress", "m2", "legacy"))
    log("convergence_10", txids="|".join(tx10))

    run_convergence()
    run_convergence()
    bh_arr = rpc_json(base, "generatetoaddress", "1", rpc_cmd(base, "getnewaddress", "rngmine", "legacy"))
    bh = bh_arr[0]
    blk2 = rpc_json(base, "getblock", bh, "2")
    height = blk2.get("height")
    cb = blk2["tx"][0]
    orbis_amt = Decimal(0)
    orbis_n = 0
    for vo in cb["vout"]:
        if vo.get("asset") == "RES":
            orbis_amt += Decimal(str(vo.get("value", 0)))
            orbis_n += 1

    n_convergence = sum(1 for tx in blk2.get("tx", [])[1:] if is_convergence_tx(tx))
    if n_convergence == 0:
        # Fallback: serialized convergence tag in tx hex
        for tx in blk2.get("tx", [])[1:]:
            if "464f524745" in json.dumps(tx):
                n_convergence += 1
    log(
        "orbis_block",
        block_hash=bh,
        block_height=str(height),
        orbis_coinbase_oru=str(orbis_amt),
        orbis_vout_count=str(orbis_n),
        distinct_convergence_txs_in_block=str(n_convergence),
        orbis_rules=(
            "RES may mint on any block height: ResDrawTargetForBlock(pprev) vs convergence draw; "
            "exactly one convergence winner + inequality; retarget on mint (GetNextRNGTarget window)."
        ),
    )

    recv = rpc_cmd(base, "getnewaddress", "recv-oru", "legacy")
    tx_pay = rpc_cmd(base, "sendtoaddress", recv, "10")
    info = rpc_json(base, "gettransaction", tx_pay)
    log("oru_transfer", txid=tx_pay, fee=str(info.get("fee", "")), fee_detail=fee_comment(info.get("fee")))

    rpc_json(base, "generatetoaddress", "1", rpc_cmd(base, "getnewaddress", "cfm", "legacy"))

    orbis_idx = None
    for vo in cb["vout"]:
        if vo.get("asset") == "RES":
            orbis_idx = int(vo["n"])
            break

    if orbis_idx is not None:
        recv_rng = rpc_cmd(base, "getnewaddress", "recv-orbis", "legacy")
        change_rng = rpc_cmd(base, "getrawchangeaddress", "legacy")
        vo = cb["vout"][orbis_idx]
        val = Decimal(str(vo["value"]))
        send_amt = Decimal("0.5")
        # All outputs must share one asset (RES); implicit fee = input - outputs.
        change_amt = val - send_amt - Decimal("0.00001")
        if change_amt <= 0:
            log("orbis_transfer", status="skipped", reason="RES utxo too small for split")
        else:
            ins = json.dumps([{"txid": cb["txid"], "vout": orbis_idx}])
            outs = json.dumps(
                {
                    recv_rng: {"amount": format(send_amt, "f"), "asset": "RES"},
                    change_rng: {"amount": format(change_amt, "f"), "asset": "RES"},
                }
            )
            raw = rpc_cmd(base, "createrawtransaction", ins, outs)
            signed = rpc_json(base, "signrawtransactionwithwallet", raw)
            if signed.get("complete"):
                tx_rng = rpc_cmd(base, "sendrawtransaction", signed["hex"])
                implied_fee = str(val - send_amt - change_amt)
                log(
                    "orbis_transfer",
                    txid=tx_rng,
                    orbis_implied_fee_units=implied_fee,
                    fee_detail="network fee is ORU-only; RES leg must balance in/out",
                )
                rpc_json(base, "generatetoaddress", "1", rpc_cmd(base, "getnewaddress", "cfm2", "legacy"))
                recv_bal = Decimal(rpc_cmd(base, "getreceivedbyaddress", recv_rng, "1"))
                log("orbis_recv_balance", address=recv_rng, received=str(recv_bal))
            else:
                log("orbis_transfer", status="sign_failed", detail=json.dumps(signed)[:400])
    else:
        log("orbis_transfer", status="skipped", reason="no RES in coinbase")

    fieldnames = [
        "ts",
        "phase",
        "block_height",
        "balance_oru",
        "expected_subsidy_oru",
        "balance_ok",
        "txid",
        "txids",
        "fee",
        "fee_detail",
        "note",
        "block_hash",
        "orbis_coinbase_oru",
        "orbis_vout_count",
        "distinct_convergence_txs_in_block",
        "orbis_rules",
        "status",
        "reason",
        "address",
        "received",
        "fundraw_fee",
        "orbis_implied_fee_units",
        "detail",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in fieldnames})

    print(str(csv_path), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
