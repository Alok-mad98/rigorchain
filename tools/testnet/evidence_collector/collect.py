#!/usr/bin/env python3
"""Private-testnet evidence collector. Environment + consensus join. Not a proof."""
from __future__ import annotations

import argparse
import json
import shutil
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[3]


def utc_day() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%d")


def rpc_url(host: str, port: int, user: str, password: str, method: str, *params: Any, wallet: Optional[str] = None) -> Any:
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "ev", "method": method, "params": list(params)}).encode()
    req = urllib.request.Request(f"http://{host}:{port}{path}", data=payload, method="POST")
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://{host}:{port}/", user, password)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr))
    try:
        with opener.open(req, timeout=30) as resp:
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


def append_jsonl(path: Path, obj: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    obj.setdefault("collected_at", datetime.now(timezone.utc).isoformat())
    with path.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(obj, sort_keys=True, separators=(",", ":")) + "\n")


def utxo_hash(info: dict) -> Any:
    return info.get("hash_serialized_3") or info.get("hash_serialized_2") or info.get("hash")


def load_state(path: Path) -> dict:
    if path.is_file():
        return json.loads(path.read_text())
    return {"last_height": -1, "last_hash": None, "last_utxo": None, "peers": {}, "last_tips": []}


def save_state(path: Path, state: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n")


def snapshot(node: dict, out_day: Path, state: dict) -> dict:
    host = node.get("host", "127.0.0.1")
    port = int(node["rpc"])
    user = node.get("rpcuser", "evidence")
    password = node.get("rpcpassword", "evidence-local")
    name = node["id"]

    def call(method: str, *params: Any) -> Any:
        return rpc_url(host, port, user, password, method, *params)

    best = call("getbestblockhash")
    height = call("getblockcount")
    hdr = call("getblockheader", best)
    blk = call("getblock", best, 1)
    utxo = call("gettxoutsetinfo")
    peers = call("getpeerinfo")
    tips = call("getchaintips")
    now_ms = int(time.time() * 1000)

    prev_h = state.get("last_height", -1)
    prev_hash = state.get("last_hash")
    prev_utxo = state.get("last_utxo")
    cur_utxo = utxo_hash(utxo)

    if height > prev_h and prev_h >= 0:
        append_jsonl(
            out_day / "connect_event.jsonl",
            {
                "node": name,
                "kind": "connect",
                "future_theorem": "utxo_inverse",
                "old_utxo_hash": prev_utxo,
                "new_utxo_hash": cur_utxo,
                "hash": best,
                "height": height,
            },
        )
        append_jsonl(
            out_day / "block_event.jsonl",
            {
                "node": name,
                "height": height,
                "hash": best,
                "previous": hdr.get("previousblockhash"),
                "time": hdr.get("time"),
                "tx_count": blk.get("nTx") or len(blk.get("tx") or []),
                "validation": "accept",
                "utxo_before": prev_utxo,
                "utxo_after": cur_utxo,
                "future_theorem": "network assumption",
            },
        )
        append_jsonl(
            out_day / "validation_event.jsonl",
            {
                "node": name,
                "hash": best,
                "validation": "accept",
                "rejection_layer": None,
                "future_theorem": "validation_soundness",
            },
        )
    elif height < prev_h:
        append_jsonl(
            out_day / "disconnect_event.jsonl",
            {
                "node": name,
                "kind": "disconnect",
                "future_theorem": "utxo_inverse",
                "new_utxo_hash": prev_utxo,
                "old_utxo_hash": cur_utxo,
                "old_hash": prev_hash,
                "new_hash": best,
                "height": height,
            },
        )
        append_jsonl(
            out_day / "reorg_event.jsonl",
            {
                "node": name,
                "old_tip": prev_hash,
                "new_tip": best,
                "disconnect_count": prev_h - height,
                "connect_count": 0,
                "fork_depth": max((t.get("branchlen") or 0) for t in tips) if tips else 0,
                "future_theorem": "chain_selection",
            },
        )
    elif prev_hash and prev_hash != best:
        append_jsonl(
            out_day / "reorg_event.jsonl",
            {
                "node": name,
                "old_tip": prev_hash,
                "new_tip": best,
                "disconnect_count": None,
                "connect_count": None,
                "fork_depth": max((t.get("branchlen") or 0) for t in tips) if tips else 0,
                "future_theorem": "chain_selection",
            },
        )

    known = state.setdefault("peers", {})
    seen_ids = set()
    for p in peers:
        pid = str(p.get("id"))
        seen_ids.add(pid)
        addr = p.get("addr")
        if pid not in known:
            known[pid] = {"addr": addr, "first_ms": now_ms}
            append_jsonl(
                out_day / "peer_event.jsonl",
                {
                    "node": name,
                    "event": "connect",
                    "peer_id": p.get("id"),
                    "addr": addr,
                    "peer_count": len(peers),
                    "future_theorem": "network assumption",
                },
            )
        known[pid]["last_ms"] = now_ms
        known[pid]["lifetime_ms"] = now_ms - known[pid]["first_ms"]
    for pid in list(known):
        if pid not in seen_ids:
            rec = known.pop(pid)
            append_jsonl(
                out_day / "peer_event.jsonl",
                {
                    "node": name,
                    "event": "disconnect",
                    "peer_id": pid,
                    "addr": rec.get("addr"),
                    "lifetime_ms": rec.get("lifetime_ms"),
                    "peer_count": len(peers),
                    "future_theorem": "network assumption",
                },
            )

    if state.get("expect_restart"):
        append_jsonl(
            out_day / "restart_event.jsonl",
            {
                "node": name,
                "hash": best,
                "utxo": cur_utxo,
                "same_tip": best == state.get("restart_hash"),
                "same_utxo": cur_utxo == state.get("restart_utxo"),
                "future_theorem": "persistence_correctness",
            },
        )
        state["expect_restart"] = False

    state["last_height"] = height
    state["last_hash"] = best
    state["last_utxo"] = cur_utxo
    state["last_tips"] = tips
    state["peer_count"] = len(peers)
    return state


def prune(out_root: Path, keep_days: int) -> None:
    if keep_days <= 0 or not out_root.is_dir():
        return
    days = sorted([p for p in out_root.iterdir() if p.is_dir() and len(p.name) == 10])
    for path in days[:-keep_days]:
        shutil.rmtree(path, ignore_errors=True)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Collect private-network evidence JSONL")
    p.add_argument("--config", type=Path, default=Path(__file__).with_name("config.example.json"))
    p.add_argument("--out", type=Path, default=ROOT / "evidence")
    p.add_argument("--once", action="store_true")
    p.add_argument("--interval", type=int, default=60)
    p.add_argument("--prune-days", type=int, default=30)
    return p.parse_args()


def main() -> int:
    args = parse_args()
    cfg = json.loads(args.config.read_text()) if args.config.is_file() else {"nodes": []}
    nodes = cfg.get("nodes") or []
    if not nodes:
        print("no nodes in config; nothing to collect")
        return 0
    state_path = args.out / ".collector_state.json"
    state_all = load_state(state_path)

    def tick() -> None:
        day = args.out / utc_day()
        for node in nodes:
            nid = node["id"]
            try:
                state_all[nid] = snapshot(node, day, state_all.get(nid) or {})
            except Exception as exc:  # noqa: BLE001
                append_jsonl(
                    day / "peer_event.jsonl",
                    {"node": nid, "event": "rpc_error", "error": str(exc), "future_theorem": "network assumption"},
                )
        save_state(state_path, state_all)
        prune(args.out, args.prune_days)

    tick()
    if args.once:
        print(f"collected {utc_day()} -> {args.out}")
        return 0
    while True:
        time.sleep(max(1, args.interval))
        tick()


if __name__ == "__main__":
    raise SystemExit(main())
