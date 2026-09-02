#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""R6: mutants of frozen constants / LedgerApply early-returns must be killed.

A mutant is killed if it fails to compile (static_assert lock) or if the
issuance / ledger vector tests fail. A surviving mutant is a gate failure.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
TOOLS = ROOT / "tools"
LOCK = SRC / "consensus/rigor_consensus_lock.h"
ISSUANCE = SRC / "consensus/rigor_issuance.cpp"
LEDGER = SRC / "consensus/ledger/ledger_transition.cpp"
VECTORS = ROOT / "formal_verification/consensus_kernel_refinement_vectors.json"
LEDGER_TEST = SRC / "test/test_ledger_transition_refinement.cpp"

CXX = os.environ.get("CXX") or shutil.which("g++") or shutil.which("c++")
CLI_SOURCES = [
    TOOLS / "consensus_kernel_refinement_cli.cpp",
    TOOLS / "consensus_kernel_refinement_log_stub.cpp",
    SRC / "consensus/rigor_issuance.cpp",
    SRC / "consensus/rigor_supply.cpp",
    SRC / "script/rigor_classify.cpp",
    SRC / "script/script.cpp",
    SRC / "uint256.cpp",
    SRC / "util/strencodings.cpp",
    SRC / "consensus/convergence_logic.cpp",
    SRC / "consensus_rigor/sigma_hash.cpp",
    SRC / "crypto/sha256.cpp",
    SRC / "arith_uint256.cpp",
]


def run(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        cwd=cwd,
        capture_output=True,
        text=True,
        timeout=90,
    )


def compile_cli(overlay: Path, bin_path: Path, issuance_src: Path | None = None) -> bool:
    sources = []
    for s in CLI_SOURCES:
        if s.name == "rigor_issuance.cpp" and issuance_src is not None:
            sources.append(str(issuance_src))
        else:
            sources.append(str(s))
    r = run(
        [CXX, "-O0", "-std=c++17", f"-I{overlay}", f"-I{SRC}", "-o", str(bin_path), *sources]
    )
    return r.returncode == 0


def issuance_pins_hold(cli: Path) -> bool:
    vec = json.loads(VECTORS.read_text(encoding="utf-8"))
    for pin in vec["issuance"]:
        r = run([str(cli), "issuance", str(pin["height"])])
        if r.returncode != 0:
            return False
        got = json.loads(r.stdout)
        if int(got["oru_subsidy"]) != int(pin["oru_subsidy"]):
            return False
    return True


def compile_ledger_test(ledger_src: Path, bin_path: Path) -> bool:
    r = run(
        [
            CXX,
            "-O0",
            "-std=c++17",
            f"-I{SRC}",
            "-o",
            str(bin_path),
            str(LEDGER_TEST),
            str(ledger_src),
            str(SRC / "consensus/rigor_issuance.cpp"),
            str(TOOLS / "consensus_kernel_refinement_log_stub.cpp"),
            str(SRC / "uint256.cpp"),
            str(SRC / "util/strencodings.cpp"),
        ]
    )
    return r.returncode == 0


def ledger_test_holds(bin_path: Path) -> bool:
    r = run([str(bin_path)])
    return r.returncode == 0


def overlay_lock(tmp: Path, original: str, old: str, new: str) -> Path:
    dest_dir = tmp / "consensus"
    dest_dir.mkdir(parents=True, exist_ok=True)
    text = original.replace(old, new, 1)
    dest = dest_dir / "rigor_consensus_lock.h"
    dest.write_text(text, encoding="utf-8")
    return tmp


# Every `return out;` in LedgerApply is mutated. A survivor is a gate failure
# unless it appears below with an argument for why removing the guard cannot
# change LedgerApply's output on any input. Do not add an entry to silence a
# survivor: an entry is a claim that the mutant is semantically equivalent, and
# it must be justified from the code, not from the test suite's coverage.
EQUIVALENT_MUTANTS: dict[str, str] = {
    "if (!AmountOk(in.consensus.oru_issuance) || !AmountOk(in.consensus.res_mint)) return out;":
        "Both operands are re-checked by AmountOk inside the AddMoney calls that "
        "build `credited`, which run before value_ok is assigned. Either failure "
        "returns with value_ok still false, i.e. the same output.",
    "if (!value_ok) return out;":
        "Reaching `out.accept = true` requires both identity gates. Summing them "
        "gives credited - rhs = in.converge, and in.converge >= 0 is enforced "
        "earlier, so value_ok is already true on every accepting path. On a "
        "rejecting path out.value_ok was assigned before this return.",
    "if (!AddMoney(in.burn, in.converge, oru_sinks)) return out;":
        "The mutant leaves oru_sinks = 0 and then adds only the fee. Accepting "
        "with oru_sinks = fee forces credited >= credited + in.burn, i.e. "
        "in.burn = 0, and then burn + converge <= MAX_MONEY always holds because "
        "converge is range-checked earlier. So the guard cannot fail on any path "
        "that would reach accept.",
    "if (!AddMoney(in_oru, in.consensus.oru_issuance, oru_left)) return out;":
        "in_all = in_oru + in_res >= in_oru, so in_oru + oru_issuance can only "
        "overflow when in_all + oru_issuance already did. That earlier AddMoney "
        "returns first, making this guard unreachable.",
    "if (oru_left < oru_sinks) return out;":
        "Both operands are in [0, MAX_MONEY], so oru_left - oru_sinks is negative "
        "exactly when this triggers, while out_oru >= 0 comes from SumKnownAssets. "
        "The following inequality therefore rejects the same inputs.",
    "if (in_res > std::numeric_limits<CAmount>::max() - in.consensus.res_mint) return out;":
        "in_res and res_mint are both bounded by MAX_MONEY, which is three orders "
        "of magnitude below INT64_MAX. The condition is unsatisfiable: dead guard "
        "kept as defence in depth.",
}


def ledger_reject_returns(text: str) -> list[tuple[int, str]]:
    """All `return out;` sites in LedgerApply, with their source line."""
    start = text.find("LedgerKernelOutput LedgerApply")
    accept = text.find("out.accept = true;", start)
    if start < 0 or accept < 0:
        raise RuntimeError("cannot locate LedgerApply body")
    body = text[start:accept]
    hits: list[tuple[int, str]] = []
    for m in re.finditer(r"return out;", body):
        line_start = body.rfind("\n", 0, m.start()) + 1
        line = body[line_start : body.find("\n", m.start())].strip()
        hits.append((start + m.start(), line))
    return hits


def comment_return_at(text: str, pos: int) -> str:
    if text[pos : pos + 11] != "return out;":
        raise RuntimeError(f"not a return out; at {pos}")
    return text[:pos] + "((void)0);" + text[pos + 11 :]


def main() -> int:
    if CXX is None:
        print("FAIL: no C++ compiler")
        return 1

    killed = 0
    survived: list[str] = []
    exempt: list[tuple[str, str]] = []
    lock_orig = LOCK.read_text(encoding="utf-8")
    issuance_orig = ISSUANCE.read_text(encoding="utf-8")
    ledger_orig = LEDGER.read_text(encoding="utf-8")

    constant_mutants = [
        (
            "ORU_INITIAL_BLOCK_SUBSIDY+1",
            "inline constexpr CAmount ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN;",
            "inline constexpr CAmount ORU_INITIAL_BLOCK_SUBSIDY = 1001 * COIN;",
        ),
        (
            "ORU_SUBSIDY_HALVING_INTERVAL-1",
            "inline constexpr int ORU_SUBSIDY_HALVING_INTERVAL = 210000;",
            "inline constexpr int ORU_SUBSIDY_HALVING_INTERVAL = 209999;",
        ),
        (
            "ORU_SUBSIDY_MAX_SHIFTS+1",
            "inline constexpr int ORU_SUBSIDY_MAX_SHIFTS = 3;",
            "inline constexpr int ORU_SUBSIDY_MAX_SHIFTS = 4;",
        ),
        (
            "ORU_GENESIS_BLOCK_SUBSIDY+1",
            "inline constexpr CAmount ORU_GENESIS_BLOCK_SUBSIDY = 0;",
            "inline constexpr CAmount ORU_GENESIS_BLOCK_SUBSIDY = 1;",
        ),
    ]

    with tempfile.TemporaryDirectory(prefix="rigor-mut-") as td:
        tmp = Path(td)
        cli = tmp / "cli"
        for name, old, new in constant_mutants:
            overlay_lock(tmp, lock_orig, old, new)
            ok_compile = compile_cli(tmp, cli)
            if not ok_compile:
                killed += 1
                continue
            if issuance_pins_hold(cli):
                survived.append(name)
            else:
                killed += 1

        # Formula mutant: genesis height gate.
        mut_iss = tmp / "rigor_issuance.cpp"
        mut_iss.write_text(
            issuance_orig.replace("if (nHeight == 0)", "if (nHeight == 1)", 1),
            encoding="utf-8",
        )
        overlay_lock(tmp, lock_orig, "", "")  # restore original lock via rewrite
        (tmp / "consensus" / "rigor_consensus_lock.h").write_text(lock_orig, encoding="utf-8")
        if not compile_cli(tmp, cli, issuance_src=mut_iss):
            killed += 1
        elif issuance_pins_hold(cli):
            survived.append("GetBlockSubsidy genesis height==1")
        else:
            killed += 1

        # LedgerApply early-return mutants: full domain, no regex filter.
        sites = ledger_reject_returns(ledger_orig)
        if len(sites) < 20:
            print(f"FAIL: expected >=20 LedgerApply reject returns, found {len(sites)}")
            return 1
        stale = [line for line in EQUIVALENT_MUTANTS if line not in {s for _, s in sites}]
        if stale:
            print("FAIL: EQUIVALENT_MUTANTS entries no longer match any source line")
            for line in stale:
                print(f"  - {line}")
            return 1
        ledger_bin = tmp / "ledger_test"
        for i, (pos, line) in enumerate(sites):
            mut_led = tmp / f"ledger_{i}.cpp"
            mut_led.write_text(comment_return_at(ledger_orig, pos), encoding="utf-8")
            name = f"LedgerApply return[{i}]: {line}"
            if not compile_ledger_test(mut_led, ledger_bin):
                killed += 1
                continue
            if not ledger_test_holds(ledger_bin):
                killed += 1
            elif line in EQUIVALENT_MUTANTS:
                exempt.append((line, EQUIVALENT_MUTANTS[line]))
            else:
                survived.append(name)

    total = killed + len(survived) + len(exempt)
    if survived:
        print("FAIL: kernel mutation test — surviving mutants")
        for s in survived:
            print(f"  SURVIVED: {s}")
        print(f"killed={killed} survived={len(survived)} exempt={len(exempt)} total={total}")
        return 1

    print("KERNEL_MUTATION=PASS")
    print(f"killed={killed} survived=0 exempt={len(exempt)} total={total}")
    for line, why in exempt:
        print(f"  EXEMPT: {line}")
        print(f"          {why}")
    print("COQ_TO_CPP=VECTOR-TESTED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
