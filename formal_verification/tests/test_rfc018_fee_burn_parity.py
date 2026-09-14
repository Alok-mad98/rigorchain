"""RFC-018 fee-burn parity: C++ share/display ≡ Python spec ≡ Coq pins.

Does not call coqc. Pins are the lemmas in
``formal_verification/coq/FeeBurn/Accounting.v`` and ``Precision.v``.
Exit 1 on any mismatch. Does not claim C++ refinement.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PYSPEC = REPO / "formal_verification" / "python-spec"
GOLDEN = REPO / "formal_verification" / "golden-vectors" / "rfc018_fee_burn.json"
sys.path.insert(0, str(PYSPEC))

from rfc018_fee_burn import (  # noqa: E402
    COQ_BURN_PINS,
    COQ_CLASS_PINS,
    COQ_DISPLAY_PINS,
    MAX_MONEY,
    ORU_COIN,
    ORU_RES_TRANSFER_FEE_BURN_DEN,
    ORU_RES_TRANSFER_FEE_BURN_NUM,
    RES_ATOMIC_UNIT,
    RES_COIN,
    RES_DISPLAY_DECIMALS,
    RES_MAX_SUPPLY,
    TX_FEE_RES_TRANSFER,
    apply_block_circulating,
    calculate_fee_burn,
    calculate_miner_fee,
    circulating_oru_delta,
    format_res_amount,
    golden_vectors,
    oru_res_transfer_fee_burn_share,
    rollback,
    FeeBurnState,
    FeeTx,
)


def cpp_dump() -> dict:
    src = r"""
#include <consensus/rigor_consensus_lock.h>
#include <consensus/res_amount.h>
#include <cstdio>
int main() {
  using namespace Consensus;
  std::printf("num %d\n", ORU_RES_TRANSFER_FEE_BURN_NUM);
  std::printf("den %d\n", ORU_RES_TRANSFER_FEE_BURN_DEN);
  std::printf("res_coin %lld\n", (long long)RES_COIN);
  std::printf("res_atom %lld\n", (long long)RES_ATOMIC_UNIT);
  std::printf("res_cap %lld\n", (long long)RES_MAX_SUPPLY);
  const CAmount fees[] = {0, 1, 2, 3, 7, 1000,
      COIN, 125 * COIN, MAX_MONEY, MAX_MONEY - 1};
  for (CAmount f : fees)
    std::printf("burn %lld %lld\n", (long long)f,
                (long long)OruResTransferFeeBurnShare(f));
  std::printf("disp %s\n", FormatResAmount(0).c_str());
  std::printf("disp %s\n", FormatResAmount(RES_COIN).c_str());
  std::printf("disp %s\n", FormatResAmount(RES_COIN / 10).c_str());
  std::printf("disp %s\n", FormatResAmount(RES_ATOMIC_UNIT).c_str());
  return 0;
}
"""
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "rfc018.cpp"
        bin_path = Path(td) / "rfc018"
        cpp.write_text(src)
        cmd = [
            "g++",
            "-std=c++17",
            f"-I{REPO / 'core' / 'src'}",
            "-O0",
            "-o",
            str(bin_path),
            str(cpp),
        ]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        lines = subprocess.check_output([str(bin_path)], text=True).strip().splitlines()
    out: dict = {"burn": {}, "disp": []}
    for line in lines:
        parts = line.split()
        if parts[0] == "num":
            out["num"] = int(parts[1])
        elif parts[0] == "den":
            out["den"] = int(parts[1])
        elif parts[0] == "res_coin":
            out["res_coin"] = int(parts[1])
        elif parts[0] == "res_atom":
            out["res_atom"] = int(parts[1])
        elif parts[0] == "res_cap":
            out["res_cap"] = int(parts[1])
        elif parts[0] == "burn":
            out["burn"][int(parts[1])] = int(parts[2])
        elif parts[0] == "disp":
            out["disp"].append(parts[1])
    return out


def main() -> int:
    errors: list[str] = []
    cpp = cpp_dump()

    if cpp["num"] != ORU_RES_TRANSFER_FEE_BURN_NUM:
        errors.append(f"NUM C++={cpp['num']} Python={ORU_RES_TRANSFER_FEE_BURN_NUM}")
    if cpp["den"] != ORU_RES_TRANSFER_FEE_BURN_DEN:
        errors.append(f"DEN C++={cpp['den']} Python={ORU_RES_TRANSFER_FEE_BURN_DEN}")
    if cpp["res_coin"] != RES_COIN or RES_COIN != 100_000_000 or RES_COIN in (100, 1_000_000):
        errors.append(f"RES_COIN C++={cpp['res_coin']} Python={RES_COIN}")
    if cpp["res_atom"] != RES_ATOMIC_UNIT:
        errors.append(f"RES_ATOMIC_UNIT C++={cpp['res_atom']}")
    if cpp["res_cap"] != RES_MAX_SUPPLY:
        errors.append(f"RES_MAX_SUPPLY C++={cpp['res_cap']} Python={RES_MAX_SUPPLY}")
    if cpp["res_cap"] == 21_000_000 * 100:
        errors.append("RES_MAX_SUPPLY still uses the retired 2-decimal lock")

    for fee, burn in COQ_BURN_PINS:
        py = oru_res_transfer_fee_burn_share(fee)
        c = cpp["burn"].get(fee)
        if py != burn:
            errors.append(f"share fee={fee} Python={py} Coq={burn}")
        if c != burn:
            errors.append(f"share fee={fee} C++={c} Coq={burn}")

    for fee, cls, burn, miner in COQ_CLASS_PINS:
        if calculate_fee_burn(fee, cls) != burn:
            errors.append(f"class burn {cls} fee={fee}")
        if calculate_miner_fee(fee, cls) != miner:
            errors.append(f"class miner {cls} fee={fee}")

    for (atoms, text), got in zip(COQ_DISPLAY_PINS, cpp["disp"]):
        py = format_res_amount(atoms)
        if py != text:
            errors.append(f"display Python {atoms}: {py!r} != {text!r}")
        if got != text:
            errors.append(f"display C++ {atoms}: {got!r} != {text!r}")
        if RES_DISPLAY_DECIMALS != 8:
            errors.append("RES_DISPLAY_DECIMALS is not 8")

    pre = FeeBurnState(oru=10 * ORU_COIN, res=40 * RES_COIN)
    txs = [FeeTx("oru_only", 4000), FeeTx(TX_FEE_RES_TRANSFER, 6000)]
    subsidy = 128 * ORU_COIN
    post = apply_block_circulating(pre, txs, subsidy, 0)
    replay = apply_block_circulating(pre, txs, subsidy, 0)
    if post is None or replay is None:
        errors.append("circulating apply failed")
    else:
        expected_delta = circulating_oru_delta(subsidy, calculate_fee_burn(6000, TX_FEE_RES_TRANSFER), 0)
        if post.oru - pre.oru != expected_delta:
            errors.append(f"supply conservation {post.oru - pre.oru} != {expected_delta}")
        if post.res != pre.res:
            errors.append("RES isolation failed")
        if post != replay:
            errors.append("replay safety failed")
        restored = rollback(post, pre)
        if restored.oru != pre.oru or restored.res != pre.res:
            errors.append("reorg rollback failed")

    vec = golden_vectors()
    committed = json.loads(GOLDEN.read_text())
    if vec != committed:
        errors.append("golden-vectors/rfc018_fee_burn.json drifted from python-spec")

    if errors:
        print("RFC-018 fee-burn parity FAILED:")
        for e in errors:
            print(" ", e)
        return 1
    print("RFC-018 fee-burn parity OK")
    print(f"  share pins: {len(COQ_BURN_PINS)}")
    print(f"  class pins: {len(COQ_CLASS_PINS)}")
    print(f"  display: 1 RES -> {format_res_amount(RES_COIN)}; "
          f"0.1 RES -> {format_res_amount(RES_COIN // 10)}")
    print("  circulating ΔORU = subsidy − burn; RES unchanged")
    print("  equivalence_claimed=false")
    return 0


if __name__ == "__main__":
    sys.exit(main())
