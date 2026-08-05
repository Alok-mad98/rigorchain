#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from decimal import Decimal

# Native-domain asset tag byte in raw tx output encoding (wire; RPC asset ORU/ORUM).
ASSET_ORU_WIRE = 0


def rpc(cli: str, chain: str, cli_extras: list[str], method: str, *params: str) -> str:
    cmd = [cli, f"-chain={chain}", *cli_extras, method, *params]
    proc = subprocess.run(cmd, check=True, text=True, capture_output=True)
    return proc.stdout.strip()


def rpc_json(cli: str, chain: str, cli_extras: list[str], method: str, *params: str):
    return json.loads(rpc(cli, chain, cli_extras, method, *params))


def normalize_hex(data: str) -> str:
    value = data.lower().strip()
    if value.startswith("0x"):
        value = value[2:]
    if len(value) == 0:
        raise ValueError("convergence_data_hex cannot be empty")
    if len(value) % 2 != 0:
        raise ValueError("convergence_data_hex must have even hex length")
    int(value, 16)
    return value


def as_btc_string(amount: str) -> str:
    dec = Decimal(amount)
    if dec < Decimal("0"):
        raise ValueError("amount must be >= 0")
    return format(dec, "f")


def to_sats(amount: str) -> int:
    return int((Decimal(amount) * Decimal(100_000_000)).to_integral_value())


def encode_varint(n: int) -> bytes:
    if n < 0xFD:
        return bytes([n])
    if n <= 0xFFFF:
        return b"\xfd" + n.to_bytes(2, "little")
    if n <= 0xFFFFFFFF:
        return b"\xfe" + n.to_bytes(4, "little")
    return b"\xff" + n.to_bytes(8, "little")


def make_op_return_script(data_hex: str) -> bytes:
    data = bytes.fromhex(data_hex)
    if len(data) <= 75:
        return b"\x6a" + bytes([len(data)]) + data
    if len(data) <= 255:
        return b"\x6a\x4c" + bytes([len(data)]) + data
    if len(data) <= 65535:
        return b"\x6a\x4d" + len(data).to_bytes(2, "little") + data
    raise ValueError("convergence_data_hex too large for OP_RETURN")


def build_raw_unsigned_tx(
    txid_hex: str,
    vout: int,
    convergence_value_sats: int,
    convergence_script: bytes,
    change_script_hex: str,
    change_value_sats: int,
) -> str:
    version = (2).to_bytes(4, "little")
    vin_cnt = encode_varint(1)
    prev_txid_le = bytes.fromhex(txid_hex)[::-1]
    prev_vout = vout.to_bytes(4, "little")
    script_sig = b""
    script_sig_len = encode_varint(len(script_sig))
    sequence = bytes.fromhex("fdffffff")
    vin = prev_txid_le + prev_vout + script_sig_len + script_sig + sequence

    change_script = bytes.fromhex(change_script_hex)
    vout_cnt = encode_varint(2)
    convergence_out = (
        convergence_value_sats.to_bytes(8, "little", signed=True)
        + bytes([ASSET_ORU_WIRE])
        + encode_varint(len(convergence_script))
        + convergence_script
    )
    change_out = (
        change_value_sats.to_bytes(8, "little", signed=True)
        + bytes([ASSET_ORU_WIRE])
        + encode_varint(len(change_script))
        + change_script
    )
    locktime = (0).to_bytes(4, "little")
    return (version + vin_cnt + vin + vout_cnt + convergence_out + change_out + locktime).hex()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build/sign/broadcast convergence tx (with OP_RETURN data output) on devtest."
    )
    parser.add_argument("--cli", default="./src/rigor-cli", help="Path to bitcoin-cli compatible binary.")
    parser.add_argument("--chain", default="devtest", help="Chain name, default devtest.")
    parser.add_argument(
        "--cli-extra",
        action="append",
        default=[],
        help="Extra CLI arg, repeatable. Example: --cli-extra=-rpcport=18443",
    )
    parser.add_argument("--amount", required=True, help="Amount sent to a wallet-owned address.")
    parser.add_argument("--convergence-data-hex", required=True, help="Hex payload for OP_RETURN output.")
    parser.add_argument("--subtract-fee-from-amount", action="store_true", help="Subtract fee from recipient amount.")
    parser.add_argument("--fee-sats", type=int, default=1000, help="Fixed fee in sats.")
    parser.add_argument("--max-burn-amount", default="10.0", help="Allow unspendable burn up to this amount (ORUM).")
    parser.add_argument(
        "--min-priority",
        type=float,
        default=-1.0,
        help="Require mempool eviction_score (CONVERGENCE_POLICY_V1_LOCK) >= this value. -1 disables.",
    )
    parser.add_argument("--json", action="store_true", help="Print JSON result.")
    args = parser.parse_args()

    try:
        convergence_data_hex = normalize_hex(args.convergence_data_hex)
        send_amount = as_btc_string(args.amount)
        convergence_sats = to_sats(send_amount)
        if convergence_sats < 0:
            raise ValueError("amount must be >= 0")
        if args.fee_sats <= 0:
            raise ValueError("fee-sats must be > 0")

        # Must spend ORUM-domain coin: select a spendable native-domain UTXO (RPC asset ORU/ORUM).
        utxos = rpc_json(args.cli, args.chain, args.cli_extra, "listunspent")
        needed = convergence_sats + args.fee_sats
        selected = None
        for u in utxos:
            if not u.get("spendable", False):
                continue
            asset = str(u.get("asset", "")).upper()
            if asset not in ("ORU", "ORUM", "BTC"):
                continue
            sats = to_sats(str(u["amount"]))
            if sats > needed:
                selected = (u, sats)
                break
        if selected is None:
            raise RuntimeError(
                "no suitable spendable ORUM-domain (ORU) utxo found; mine coinbase to the wallet first"
            )

        utxo, input_sats = selected
        change_sats = input_sats - needed
        if change_sats <= 0:
            raise RuntimeError("selected ORUM-domain utxo does not cover amount+fee")

        change_addr = rpc(args.cli, args.chain, args.cli_extra, "getrawchangeaddress", "legacy")
        addr_info = rpc_json(args.cli, args.chain, args.cli_extra, "getaddressinfo", change_addr)
        change_spk = addr_info["scriptPubKey"]
        convergence_script = make_op_return_script(convergence_data_hex)
        raw_unsigned = build_raw_unsigned_tx(
            txid_hex=utxo["txid"],
            vout=int(utxo["vout"]),
            convergence_value_sats=convergence_sats,
            convergence_script=convergence_script,
            change_script_hex=change_spk,
            change_value_sats=change_sats,
        )
        signed = rpc_json(args.cli, args.chain, args.cli_extra, "signrawtransactionwithwallet", raw_unsigned)
        if not signed.get("complete", False):
            raise RuntimeError(f"signrawtransactionwithwallet incomplete: {json.dumps(signed, ensure_ascii=False)}")
        raw_hex = signed["hex"]
        txid = rpc(args.cli, args.chain, args.cli_extra, "sendrawtransaction", raw_hex, "0", args.max_burn_amount)
        mempool_entry = rpc_json(args.cli, args.chain, args.cli_extra, "getmempoolentry", txid)
        decoded = rpc_json(args.cli, args.chain, args.cli_extra, "decoderawtransaction", raw_hex)
        fm = mempool_entry.get("convergence_meta", {})
        fb = mempool_entry.get("convergence", {})
        convergence_priority = fm.get("eviction_score", fb.get("eviction_score", fm.get("priority", fb.get("priority"))))

        has_op_return = any(
            vout.get("scriptPubKey", {}).get("type") in {"nulldata", "op_return"}
            for vout in decoded.get("vout", [])
        )
        if not has_op_return:
            raise RuntimeError("Built tx does not contain OP_RETURN output")
        if args.min_priority >= 0:
            if convergence_priority is None:
                raise RuntimeError("convergence eviction_score / priority not found in mempool entry")
            if float(convergence_priority) < args.min_priority:
                raise RuntimeError(f"convergence eviction_score {convergence_priority} below required minimum {args.min_priority}")

        result = {
            "ok": True,
            "txid": txid,
            "recipient": "",
            "requested_amount": send_amount,
            "convergence_data_hex": convergence_data_hex,
            "has_op_return": has_op_return,
            "convergence_priority": convergence_priority,
            "mempool_fee_base": mempool_entry.get("fees", {}).get("base"),
            "utxo_txid": utxo["txid"],
            "utxo_vout": int(utxo["vout"]),
            "fee_sats": args.fee_sats,
            "mempool_entry": mempool_entry,
        }
        if args.json:
            print(json.dumps(result, ensure_ascii=False))
        else:
            print(f"txid={txid}")
            print(f"recipient={recipient}")
            print(f"requested_amount={send_amount}")
            print(f"has_op_return={int(has_op_return)}")
            print("convergence_tx_ok=1")
        return 0
    except subprocess.CalledProcessError as exc:
        print(exc.stderr or str(exc), file=sys.stderr)
    except Exception as exc:  # noqa: BLE001
        print(str(exc), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
