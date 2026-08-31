#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
import time
from decimal import Decimal


def rpc(cli: str, chain: str, cli_extras: list[str], method: str, *params: str) -> str:
    cmd = [cli, f"-chain={chain}", *cli_extras, method, *params]
    proc = subprocess.run(cmd, check=True, text=True, capture_output=True)
    return proc.stdout.strip()


def rpc_json(cli: str, chain: str, cli_extras: list[str], method: str, *params: str):
    return json.loads(rpc(cli, chain, cli_extras, method, *params))


def current_balance(cli: str, chain: str, cli_extras: list[str]) -> Decimal:
    # getbalance only counts mature spendable funds in loaded wallet(s).
    return Decimal(rpc(cli, chain, cli_extras, "getbalance"))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bootstrap spendable balance for devtest by mining one block when needed."
    )
    parser.add_argument("--cli", default="./src/rigor-cli", help="Path to rigor-cli binary.")
    parser.add_argument("--chain", default="devtest", help="Chain name, default devtest.")
    parser.add_argument(
        "--cli-extra",
        action="append",
        default=[],
        help="Extra CLI arg, repeatable. Example: --cli-extra=-rpcport=18443",
    )
    parser.add_argument("--wait-seconds", type=float, default=1.0, help="Sleep after mining.")
    parser.add_argument("--json", action="store_true", help="Print JSON result.")
    args = parser.parse_args()

    try:
        height_before = int(rpc(args.cli, args.chain, args.cli_extra, "getblockcount"))
        balance_before = current_balance(args.cli, args.chain, args.cli_extra)

        mined_blocks = 0
        mined_address = ""
        if balance_before <= Decimal("0"):
            mined_address = rpc(
                args.cli, args.chain, args.cli_extra, "getnewaddress", "devtest-bootstrap-miner", "legacy"
            )
            rpc(args.cli, args.chain, args.cli_extra, "generatetoaddress", "1", mined_address)
            mined_blocks = 1
            time.sleep(max(0.0, args.wait_seconds))

        height_after = int(rpc(args.cli, args.chain, args.cli_extra, "getblockcount"))
        balance_after = current_balance(args.cli, args.chain, args.cli_extra)

        if balance_after <= Decimal("0"):
            # Some chains with maturity=1 still need one additional block before getbalance reports spendable coinbase.
            if not mined_address:
                mined_address = rpc(
                    args.cli, args.chain, args.cli_extra, "getnewaddress", "devtest-bootstrap-miner", "legacy"
                )
            rpc(args.cli, args.chain, args.cli_extra, "generatetoaddress", "1", mined_address)
            mined_blocks += 1
            time.sleep(max(0.0, args.wait_seconds))
            height_after = int(rpc(args.cli, args.chain, args.cli_extra, "getblockcount"))
            balance_after = current_balance(args.cli, args.chain, args.cli_extra)

        if balance_after <= Decimal("0"):
            raise RuntimeError("Balance is still zero after bootstrap mining. Check wallet load state or maturity setting.")

        result = {
            "ok": True,
            "height_before": height_before,
            "height_after": height_after,
            "balance_before": str(balance_before),
            "balance_after": str(balance_after),
            "mined_blocks": mined_blocks,
            "mined_address": mined_address,
        }
        if args.json:
            print(json.dumps(result, ensure_ascii=False))
        else:
            print(f"height_before={height_before}")
            print(f"height_after={height_after}")
            print(f"balance_before={balance_before}")
            print(f"balance_after={balance_after}")
            print(f"mined_blocks={mined_blocks}")
            if mined_address:
                print(f"mined_address={mined_address}")
            print("bootstrap_ok=1")
        return 0
    except subprocess.CalledProcessError as exc:
        print(exc.stderr or str(exc), file=sys.stderr)
    except Exception as exc:  # noqa: BLE001
        print(str(exc), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
