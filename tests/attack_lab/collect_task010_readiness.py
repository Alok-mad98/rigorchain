#!/usr/bin/env python3
"""Live datasets: state transitions + reorg + regtest ASERT rows.

Does not change consensus. Not a Coq proof.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import struct
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Optional

ROOT = Path(__file__).resolve().parents[2]
import sys

sys.path.insert(0, str(ROOT / "tools" / "formal_bridge"))
from generate_asert_dataset import OUT as ASERT_CSV  # noqa: E402
from generate_asert_dataset import set_compact, target_hex, write_csv  # noqa: E402

RIGORD = Path(os.environ.get("RIGORD", str(ROOT / "core" / "src" / "rigord")))
DATA = ROOT / "tests" / "attack_lab" / "data_task010"
STATE_DIR = ROOT / "audit" / "formal_bridge" / "state_transition_dataset"
REORG_MD = ROOT / "audit" / "formal_bridge" / "reorg_dataset.md"
RPC_USER = "task010"
RPC_PASS = "task010-local"

PORTS = {
    "S": {"p2p": 38750, "rpc": 38751},
    "A": {"p2p": 38760, "rpc": 38761},
    "B": {"p2p": 38770, "rpc": 38771},
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
    port = PORTS[node]["rpc"]
    path = f"/wallet/{wallet}" if wallet else "/"
    payload = json.dumps({"jsonrpc": "1.0", "id": "t10", "method": method, "params": list(params)}).encode()
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


def call_ok(node: str, method: str, *params: Any, wallet: Optional[str] = None) -> dict:
    try:
        return {"decision": "accept", "result": rpc(node, method, *params, wallet=wallet), "error": None}
    except RpcError as exc:
        return {"decision": "reject", "result": None, "error": exc.message}


def datadir(node: str) -> Path:
    return DATA / f"node-{node.lower()}"


def start(node: str, wipe: bool) -> Any:
    import subprocess

    d = datadir(node)
    if wipe and d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True, exist_ok=True)
    if not RIGORD.is_file():
        raise RuntimeError(f"missing {RIGORD}; build core/src/rigord")
    ports = PORTS[node]
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


def stop(proc: Any, node: str) -> None:
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


def ensure_wallet(node: str, name: str = "t10") -> None:
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


def rev_digest(node: str) -> Optional[str]:
    rev = datadir(node) / "regtest" / "blocks" / "rev00000.dat"
    if not rev.is_file():
        return None
    return hashlib.sha256(rev.read_bytes()).hexdigest()


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def patch_header(block_hex: str, **fields: Any) -> str:
    raw = bytearray(bytes.fromhex(block_hex))
    if "nTime" in fields:
        raw[68:72] = struct.pack("<I", int(fields["nTime"]) & 0xFFFFFFFF)
    if "nBits" in fields:
        raw[72:76] = struct.pack("<I", int(fields["nBits"]) & 0xFFFFFFFF)
    if "hashPrev" in fields:
        raw[4:36] = bytes.fromhex(fields["hashPrev"])
    return bytes(raw).hex()


def collect_state() -> dict:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    proc = start("S", wipe=True)
    rows = []
    try:
        ensure_wallet("S")
        addr = rpc("S", "getnewaddress", wallet="t10")
        rpc("S", "generatetoaddress", 101, addr, wallet="t10")

        before = rpc("S", "gettxoutsetinfo")
        hashes = rpc("S", "generatetoaddress", 1, addr, wallet="t10")
        tip = hashes[0]
        after = rpc("S", "gettxoutsetinfo")
        undo_after = rev_digest("S")
        accept = {
            "id": "ST-001",
            "kind": "accept",
            "direction": "connect",
            "block_hash": tip,
            "state_hash_before": utxo_hash(before),
            "state_hash_after": utxo_hash(after),
            "undo_hash": undo_after,
            "undo_hash_kind": "sha256(regtest/blocks/rev00000.dat); not a CTxUndo object hash",
            "txouts_before": before.get("txouts"),
            "txouts_after": after.get("txouts"),
            "height_before": before.get("height"),
            "height_after": after.get("height"),
            "future_theorem": "connect_disconnect_utxo_inverse",
        }
        write_json(STATE_DIR / "ST-001_accept_connect.json", accept)
        rows.append(accept)

        rpc("S", "invalidateblock", tip)
        after_disc = rpc("S", "gettxoutsetinfo")
        undo_disc = rev_digest("S")
        disconnect = {
            "id": "ST-002",
            "kind": "accept",
            "direction": "disconnect",
            "block_hash": tip,
            "state_hash_before": utxo_hash(after),
            "state_hash_after": utxo_hash(after_disc),
            "undo_hash": undo_disc,
            "undo_hash_kind": "sha256(regtest/blocks/rev00000.dat); not a CTxUndo object hash",
            "txouts_before": after.get("txouts"),
            "txouts_after": after_disc.get("txouts"),
            "height_before": after.get("height"),
            "height_after": after_disc.get("height"),
            "restores_prior": utxo_hash(before) == utxo_hash(after_disc),
            "future_theorem": "connect_disconnect_utxo_inverse",
        }
        write_json(STATE_DIR / "ST-002_accept_disconnect.json", disconnect)
        rows.append(disconnect)

        rpc("S", "reconsiderblock", tip)
        after_re = rpc("S", "gettxoutsetinfo")
        reconnect = {
            "id": "ST-003",
            "kind": "accept",
            "direction": "connect",
            "block_hash": tip,
            "state_hash_before": utxo_hash(after_disc),
            "state_hash_after": utxo_hash(after_re),
            "undo_hash": rev_digest("S"),
            "undo_hash_kind": "sha256(regtest/blocks/rev00000.dat); not a CTxUndo object hash",
            "txouts_before": after_disc.get("txouts"),
            "txouts_after": after_re.get("txouts"),
            "height_before": after_disc.get("height"),
            "height_after": after_re.get("height"),
            "restores_mid": utxo_hash(after) == utxo_hash(after_re),
            "note": "invalidateblock / reconsiderblock of ST-001 tip",
            "future_theorem": "connect_disconnect_utxo_inverse",
        }
        write_json(STATE_DIR / "ST-003_reconsider_connect.json", reconnect)
        rows.append(reconnect)

        before_rej = rpc("S", "gettxoutsetinfo")
        hx = rpc("S", "getblock", tip, 0)
        illegal = patch_header(hx, nBits=0)
        rej = call_ok("S", "submitblock", illegal)
        after_rej = rpc("S", "gettxoutsetinfo")
        reject = {
            "id": "ST-004",
            "kind": "reject",
            "direction": "connect",
            "block_hash": None,
            "submitted": "header nBits patched to 0 on current tip hex",
            "state_hash_before": utxo_hash(before_rej),
            "state_hash_after": utxo_hash(after_rej),
            "undo_hash": None,
            "rejection_reason": rej.get("error") or rej.get("result") or "reject",
            "utxo_unchanged": utxo_hash(before_rej) == utxo_hash(after_rej),
            "future_theorem": "PowValidation.sound",
        }
        write_json(STATE_DIR / "ST-004_reject_illegal_nbits.json", reject)
        rows.append(reject)

        before_rst = rpc("S", "gettxoutsetinfo")
        tip_rst = rpc("S", "getbestblockhash")
        stop(proc, "S")
        proc = start("S", wipe=False)
        ensure_wallet("S")
        after_rst = rpc("S", "gettxoutsetinfo")
        restart = {
            "id": "ST-005",
            "kind": "accept",
            "direction": "restart_recovery",
            "block_hash": tip_rst,
            "state_hash_before": utxo_hash(before_rst),
            "state_hash_after": utxo_hash(after_rst),
            "undo_hash": rev_digest("S"),
            "undo_hash_kind": "sha256(regtest/blocks/rev00000.dat); not a CTxUndo object hash",
            "same_tip": rpc("S", "getbestblockhash") == tip_rst,
            "same_utxo": utxo_hash(before_rst) == utxo_hash(after_rst),
            "future_theorem": "reorg_safety",
        }
        write_json(STATE_DIR / "ST-005_restart_recovery.json", restart)
        rows.append(restart)

        asert_live = []
        for h in (0, 1, rpc("S", "getblockcount")):
            bh = rpc("S", "getblockhash", h)
            hdr = rpc("S", "getblockheader", bh)
            try:
                gbt = rpc("S", "getblocktemplate", {"rules": ["segwit"]})
                next_bits = gbt.get("bits")
            except RpcError:
                next_bits = f"{hdr['bits']:08x}" if isinstance(hdr.get("bits"), int) else str(hdr.get("bits"))
            prev_bits = hdr.get("bits")
            if isinstance(prev_bits, int):
                prev_hex = f"{prev_bits:08x}"
            else:
                prev_hex = str(prev_bits)
            if isinstance(next_bits, int):
                next_hex = f"{next_bits:08x}"
            else:
                next_hex = str(next_bits)
            asert_live.append(
                {
                    "height": hdr.get("height"),
                    "previous_time": hdr.get("time"),
                    "previous_bits": prev_hex,
                    "next_bits": next_hex,
                    "expected_target": target_hex(set_compact(int(next_hex, 16))),
                    "source": "regtest_live",
                    "case": f"regtest_height_{h}_fPowNoRetargeting",
                }
            )
        write_csv(ASERT_CSV, extra=asert_live)
        write_json(STATE_DIR / "index.json", {"cases": rows, "note": "Live regtest. Not a proof."})
        return {
            "tip": tip,
            "invalidate_reconsider": {
                "old_tip": tip,
                "new_tip": rpc("S", "getbestblockhash"),
                "disconnect_count": 1,
                "connect_count": 1,
                "final_state": utxo_hash(after_re),
                "restores_mid": reconnect["restores_mid"],
            },
            "restart": restart,
            "orphan": None,
        }
    finally:
        stop(proc, "S")


def wait_peer(node: str, timeout: float = 30.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        peers = rpc(node, "getpeerinfo")
        if peers:
            return
        time.sleep(0.3)
    raise RuntimeError(f"{node} has no peers")


def collect_reorg(state_meta: dict) -> None:
    pa = start("A", wipe=True)
    pb = start("B", wipe=True)
    try:
        ensure_wallet("A")
        ensure_wallet("B")
        addr_a = rpc("A", "getnewaddress", wallet="t10")
        addr_b = rpc("B", "getnewaddress", wallet="t10")
        rpc("A", "generatetoaddress", 103, addr_a, wallet="t10")
        rpc("B", "generatetoaddress", 108, addr_b, wallet="t10")
        old_a = rpc("A", "getbestblockhash")
        old_b = rpc("B", "getbestblockhash")
        height_a = rpc("A", "getblockcount")
        height_b = rpc("B", "getblockcount")
        case4 = {
            "case": 4,
            "title": "multi-node different tip",
            "old_tip": old_a,
            "peer_tip": old_b,
            "new_tip": None,
            "disconnect_count": None,
            "connect_count": None,
            "final_state": None,
            "a_height": height_a,
            "b_height": height_b,
        }

        hx = rpc("A", "getblock", old_a, 0)
        orphan_hex = patch_header(hx, hashPrev="ab" * 32)
        orphan_res = call_ok("A", "submitblock", orphan_hex)
        case2 = {
            "case": 2,
            "title": "orphan / unknown parent",
            "old_tip": old_a,
            "new_tip": rpc("A", "getbestblockhash"),
            "disconnect_count": 0,
            "connect_count": 0,
            "final_state": utxo_hash(rpc("A", "gettxoutsetinfo")),
            "rejection_reason": orphan_res.get("error") or orphan_res.get("result") or "reject",
        }

        rpc("A", "addnode", f"127.0.0.1:{PORTS['B']['p2p']}", "add")
        rpc("B", "addnode", f"127.0.0.1:{PORTS['A']['p2p']}", "add")
        wait_peer("A")
        deadline = time.time() + 90
        while time.time() < deadline:
            if rpc("A", "getbestblockhash") == rpc("B", "getbestblockhash"):
                break
            time.sleep(0.5)
        new_a = rpc("A", "getbestblockhash")
        new_b = rpc("B", "getbestblockhash")
        synced = new_a == new_b
        # Longest work: B had more blocks; A should adopt B if headers/blocks relayed.
        case1 = {
            "case": 1,
            "title": "longest-chain switch",
            "old_tip": old_a,
            "new_tip": new_a,
            "disconnect_count": height_a if synced and new_a == old_b else None,
            "connect_count": height_b if synced and new_a == old_b else None,
            "final_state": utxo_hash(rpc("A", "gettxoutsetinfo")),
            "synced": synced,
            "note": "A mined 103; B mined 108; then addnode. genesis is the only common ancestor.",
        }
        if synced and new_a == old_b:
            case1["disconnect_count"] = height_a
            case1["connect_count"] = height_b
        case4["new_tip"] = new_a
        case4["final_state"] = utxo_hash(rpc("A", "gettxoutsetinfo"))
        case4["disconnect_count"] = case1["disconnect_count"]
        case4["connect_count"] = case1["connect_count"]

        case3 = {
            "case": 3,
            "title": "invalidate / reconsider",
            **state_meta["invalidate_reconsider"],
        }

        lines = [
 "# Reorg dataset ",
            "",
            "Live `-regtest`. Not a proof. Future names: `chain_selection_sound`, `reorg_safety`.",
            "",
            "| Case | title | old_tip | new_tip | disconnect_count | connect_count | final_state |",
            "|------|-------|---------|---------|------------------|---------------|-------------|",
        ]
        for row in (case1, case2, case3, case4):
            lines.append(
                "| {case} | {title} | `{old}` | `{new}` | {dc} | {cc} | `{fs}` |".format(
                    case=row["case"],
                    title=row["title"],
                    old=row.get("old_tip"),
                    new=row.get("new_tip"),
                    dc=row.get("disconnect_count"),
                    cc=row.get("connect_count"),
                    fs=row.get("final_state"),
                )
            )
        lines.extend(
            [
                "",
                "## Notes",
                "",
                "- Case 1: two isolated miners; A=103 blocks, B=108 blocks; then P2P `addnode`.",
                "- Case 2: `submitblock` with unknown `hashPrev` (`ab`*32). Tip unchanged.",
                f"- Case 2 reject: `{case2['rejection_reason']}`.",
                "- Case 3: `invalidateblock` then `reconsiderblock` on a single node (see ST-002 / ST-003).",
                "- Case 4: tips differed before `addnode`; after sync they share `new_tip`.",
                "- `disconnect_count` / `connect_count` for Case 1 count blocks back to genesis (only common ancestor).",
                "",
                "```",
                f"case2_reject={case2['rejection_reason']}",
                f"case1_synced={case1['synced']}",
                "```",
                "",
            ]
        )
        REORG_MD.write_text("\n".join(lines), encoding="utf-8")
        write_json(STATE_DIR / "reorg_cases.json", {"case1": case1, "case2": case2, "case3": case3, "case4": case4})
    finally:
        stop(pa, "A")
        stop(pb, "B")


def main() -> int:
    if DATA.exists():
        shutil.rmtree(DATA)
    DATA.mkdir(parents=True, exist_ok=True)
    meta = collect_state()
    collect_reorg(meta)
 print(" live datasets written")
    print(STATE_DIR)
    print(REORG_MD)
    print(ASERT_CSV)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
