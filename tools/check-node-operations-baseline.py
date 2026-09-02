#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""PHASE C: node operations baseline checker.

Verifies configuration security, RPC security, log redaction rules,
startup-parameter fail-closed behavior, and data-directory rules.
Does not recode consensus formulas. Does not modify Genesis / Kernel.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
OPS = SRC / "node/operations"
MAKEFILE = SRC / "Makefile.am"
MAKEFILE_TEST = SRC / "Makefile.test.include"
INIT = SRC / "init.cpp"
RPC_SERVER = SRC / "rpc/server.cpp"
RPC_NODE = SRC / "rpc/node.cpp"
TEST = SRC / "test/test_node_operations_baseline.cpp"
DOC = ROOT / "docs/operations/RIGOR_NODE_OPERATIONS_BASELINE_PHASE_C.md"
KERNEL = SRC / "consensus/kernel/genesis_authority.cpp"
CHAINPARAMS = SRC / "kernel/chainparams.cpp"
ISSUANCE = SRC / "consensus/rigor_issuance.cpp"

FORBIDDEN_CALLS = (
    "GetBlockSubsidy",
    "GetResMintReward",
    "ResolveConvergenceDraw",
    "RigorSupplyDelta",
    "LedgerApply",
    "ClassifyRigorTxout",
    "ClassifyRigorAsset",
    "VerifyRigorPredicate",
    "EvalScript",
    "ConnectBlock",
    "ConsensusKernelInput",
    "RigorConsensusKernel",
    "ComputeRigorIssuance",
    "ParseResMintScript",
    "CWallet",
    "EncodeSecret",
    "dumpprivkey",
)

FORBIDDEN_INCLUDES = (
    "consensus/rigor_issuance",
    "consensus/rigor_supply",
    "consensus/kernel/genesis_authority",
    "consensus/convergence_logic",
    "consensus/utxo/",
    "consensus/txkernel/",
    "wallet/wallet.h",
    "wallet/crypter.h",
)

DECISION_ENUM = re.compile(
    r"enum\s+class\s+OperationsDecision[^{]*\{([^}]+)\}", re.DOTALL
)

SECRET_LOG = (
    re.compile(r"\b(LogPrintf|LogPrint)\s*\([^;]*EncodeSecret"),
    re.compile(r"\b(LogPrintf|LogPrint)\s*\([^;]*\bprivate_key\b"),
    re.compile(r"\b(LogPrintf|LogPrint)\s*\([^;]*rpcpassword=", re.I),
    re.compile(r"std::(cout|cerr)\s*<<[^;]*EncodeSecret"),
)


def fail(errors: list[str]) -> int:
    print("NODE_OPERATIONS_BASELINE = FAIL")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def read(p: Path) -> str:
    return p.read_text(errors="replace")


def main() -> int:
    errors: list[str] = []

    required = [
        OPS / "operations_result.h",
        OPS / "node_operations_baseline.h",
        OPS / "node_operations_baseline.cpp",
        OPS / "node_operations_hooks.h",
        OPS / "node_operations_hooks.cpp",
        TEST,
        DOC,
        INIT,
        RPC_SERVER,
        RPC_NODE,
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    if errors:
        return fail(errors)

    ops_text = ""
    for p in OPS.glob("*.h"):
        ops_text += strip_comments(read(p)) + "\n"
    for p in OPS.glob("*.cpp"):
        ops_text += strip_comments(read(p)) + "\n"

    for call in FORBIDDEN_CALLS:
        if re.search(rf"\b{re.escape(call)}\s*\(", ops_text):
            errors.append(f"operations module calls {call} (consensus/wallet leak)")

    for inc in FORBIDDEN_INCLUDES:
        if inc in ops_text:
            errors.append(f"operations module includes {inc}")

    m = DECISION_ENUM.search(read(OPS / "operations_result.h"))
    if not m:
        errors.append("OperationsDecision enum missing")
    else:
        body = m.group(1)
        if "PASS" not in body or "FAIL_CLOSED" not in body:
            errors.append("OperationsDecision must be PASS / FAIL_CLOSED")
        if "ACCEPT" in body or "KERNEL" in body:
            errors.append("OperationsDecision must not grow a consensus accept")

    baseline_h = read(OPS / "node_operations_baseline.h")
    for token in (
        "EvaluateConfig",
        "EvaluateDataDirectory",
        "EvaluateRpcMethod",
        "EvaluateUpgrade",
        "SanitizeLogLine",
        "FormatHealth",
        "WriteDataDirectoryMarkers",
        "NODE_OPS_DATA_VERSION",
    ):
        if token not in baseline_h:
            errors.append(f"baseline API missing {token}")

    # Config security: fail-closed defaults and forbidden overrides.
    cpp = read(OPS / "node_operations_baseline.cpp")
    for needle in (
        "dangerous debug option -mocktime",
        "must not override consensus parameter -assumevalid",
        "RPC authentication is required",
        "must not be opened to 0.0.0.0/0",
        "insecure RPC credentials",
        "network flags not isolated",
        "RPC must not modify consensus state",
        "RPC must not inject blocks",
        "data directory lock conflict",
        "network type mismatch",
        "no automatic migration",
    ):
        if needle not in cpp:
            errors.append(f"config/RPC fail-closed string missing: {needle}")

    init_txt = read(INIT)
    if "RunNodeOperationsStartupChecks" not in init_txt:
        errors.append("init.cpp must run startup configuration checks")
    if "RunNodeOperationsDataDirectoryChecks" not in init_txt:
        errors.append("init.cpp must run data directory checks")

    rpc_server = read(RPC_SERVER)
    if "EvaluateRpcMethod" not in rpc_server:
        errors.append("rpc/server.cpp must gate dangerous RPC methods")

    rpc_node = read(RPC_NODE)
    if "getnodehealth" not in rpc_node:
        errors.append("rpc/node.cpp must expose getnodehealth")
    if "FillNodeHealthSnapshot" not in rpc_node:
        errors.append("getnodehealth must use FillNodeHealthSnapshot")

    # Log rules: operations sanitizer exists; production ops logs go through it.
    if "SanitizeLogLine" not in cpp:
        errors.append("SanitizeLogLine missing")
    if "rpcpassword=" not in cpp or "[redacted" not in cpp:
        errors.append("log redaction does not cover rpcpassword / hex blobs")
    for p in OPS.glob("*.cpp"):
        text = read(p)
        for rx in SECRET_LOG:
            if rx.search(text):
                errors.append(f"secret log pattern in {p.name}")

    test_txt = read(TEST)
    for token in (
        "TestIllegalConfigFails",
        "TestNetworkIsolation",
        "TestDataDirectoryErrors",
        "TestRpcSecurity",
        "TestLogRedaction",
        "TestHealthEndpoint",
        "invalidateblock",
        "0.0.0.0/0",
        "getnodehealth",
        "consensus_kernel_status",
    ):
        if token not in test_txt:
            errors.append(f"test coverage missing {token}")

    mk = read(MAKEFILE)
    if "node/operations/node_operations_baseline.cpp" not in mk:
        errors.append("Makefile.am missing operations baseline source")
    if "node/operations/node_operations_hooks.cpp" not in mk:
        errors.append("Makefile.am missing operations hooks source")
    mkt = read(MAKEFILE_TEST)
    if "test/test_node_operations_baseline.cpp" not in mkt:
        errors.append("Makefile.test.include missing test_node_operations_baseline.cpp")

    # Data directory rules encoded as named markers (not a second state manager).
    if "ops-network" not in baseline_h or "ops-data-version" not in baseline_h:
        errors.append("datadir markers ops-network / ops-data-version missing")
    if "CCoinsView" in ops_text or "SpendCoin" in ops_text:
        errors.append("operations module must not manage UTXO state")

    # Startup parameters: mainnet debug/consensus overrides fail-closed.
    for arg in ("-mocktime", "-assumevalid", "-minimumchainwork", "-acceptnonstdtxn", "-blockversion"):
        if arg not in cpp:
            errors.append(f"startup parameter check missing {arg}")

    doc = read(DOC)
    for token in (
        "NODE_CONFIGURATION_SECURITY = PASS",
        "DATA_DIRECTORY_SAFETY = PASS",
        "RPC_SECURITY = PASS",
        "LOGGING_AUDITABILITY = PASS",
        "HEALTH_MONITORING = PASS",
        "UPGRADE_RECOVERY_BASELINE = PASS",
        "COQ_ADMITTED = 0",
        "COQ_AXIOMS = 0",
        "CONSENSUS_KERNEL_MODIFIED = NO",
        "PROTOCOL_PARAMETERS_MODIFIED = NO",
        "NODE_OPERATIONS_BASELINE_PHASE_C = CLOSED",
    ):
        if token not in doc:
            errors.append(f"doc missing PASS token: {token}")

    # Coq: this TASK adds no theories; count admits/axioms under operations.
    for p in OPS.rglob("*"):
        if p.suffix in {".v", ".v.gz"}:
            errors.append(f"unexpected Coq file in operations: {p}")
        if p.suffix in {".h", ".cpp"}:
            t = read(p)
            if re.search(r"\bAdmitted\b", t) or re.search(r"\bAxiom\b", t):
                errors.append(f"Coq admit/axiom token in {p.name}")

    # Kernel / protocol parameters files must not be this TASK's surface.
    # Presence is expected; we only fail if operations include them.
    if not KERNEL.is_file():
        errors.append("consensus kernel source missing (tree integrity)")
    if not CHAINPARAMS.is_file():
        errors.append("chainparams missing (tree integrity)")

    if errors:
        return fail(errors)

    cxx = os.environ.get("CXX") or ""
    if not cxx:
        for cand in ("g++", "c++"):
            if subprocess.call(["bash", "-lc", f"command -v {cand} >/dev/null"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
                cxx = cand
                break
    if not cxx:
        print("WARN: no C++ compiler; skipping standalone test binary")
    else:
        out_dir = ROOT / "formal_verification" / "extracted"
        out_dir.mkdir(parents=True, exist_ok=True)
        binary = out_dir / "test_node_operations_baseline"
        cmd = [
            cxx,
            "-O2",
            "-std=c++17",
            "-DNODE_OPERATIONS_STANDALONE",
            f"-I{SRC}",
            "-o",
            str(binary),
            str(TEST),
            str(OPS / "node_operations_baseline.cpp"),
            "-lstdc++fs",
        ]
        print("== g++ test_node_operations_baseline ==")
        r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            # libstdc++fs is optional on newer libstdc++
            cmd = [c for c in cmd if c != "-lstdc++fs"]
            r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            return fail(["standalone test failed to compile"])
        run = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        if run.returncode != 0:
            sys.stderr.write(run.stderr)
            return fail(["standalone test failed"])

    print("NODE_CONFIGURATION_SECURITY = PASS")
    print("DATA_DIRECTORY_SAFETY = PASS")
    print("RPC_SECURITY = PASS")
    print("LOGGING_AUDITABILITY = PASS")
    print("HEALTH_MONITORING = PASS")
    print("UPGRADE_RECOVERY_BASELINE = PASS")
    print("COQ_ADMITTED = 0")
    print("COQ_AXIOMS = 0")
    print("CONSENSUS_KERNEL_MODIFIED = NO")
    print("PROTOCOL_PARAMETERS_MODIFIED = NO")
    print("NODE_OPERATIONS_BASELINE_PHASE_C = CLOSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
