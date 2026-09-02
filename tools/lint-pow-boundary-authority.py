#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P081: unique PoW-boundary authority lint.

Production PoW accept/reject is CheckProofOfWork only.
Digest is GetRigorPowHash only (SHA3-256("RGRPOW01" || header_80)).
Difficulty is GetNextWorkRequired -> GetNextWorkASERT only.

Does not rewrite SHA256, SHA3, ASERT, ConsensusKernel, TransactionKernel,
or Ledger.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from refinement_status import print_status, records_cpp_authority  # noqa: E402
SRC = ROOT / "core/src"
POW = SRC / "pow.cpp"
POW_H = SRC / "pow.h"
POW_DIGEST = SRC / "rigor_kernel/pow_digest.cpp"
POW_DIGEST_H = SRC / "rigor_kernel/pow_digest.h"
POW_OPUS = SRC / "pow_opus.cpp"
BLOCK_H = SRC / "primitives/block.h"
BLOCK_CPP = SRC / "primitives/block.cpp"
VALIDATION = SRC / "validation.cpp"
KERNEL_H = SRC / "consensus/kernel/genesis_authority.h"
KERNEL_CPP = SRC / "consensus/kernel/genesis_authority.cpp"
MINER = SRC / "node/miner.cpp"
WALLET = SRC / "wallet"
RPC = SRC / "rpc"
DOC = ROOT / "docs/security/RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md"
TEST = SRC / "test/test_pow_boundary_security.cpp"
GATE = ROOT / "tools/run-pow-boundary-security-audit.sh"
FUZZ = ROOT / "fuzz/pow_boundary/pow_boundary_fuzz.cpp"
MANIFEST = ROOT / "formal_verification/pow_boundary_audit_manifest.json"
MAKEFILE_TEST = SRC / "Makefile.test.include"
POW_DIGEST_V = ROOT / "formal_verification/coq/PowDigest.v"
SURFACE_V = ROOT / "formal_verification/coq/ConsensusSurfaceClosure.v"

SKIP_DIR_PARTS = {
    "test",
    "bench",
    "fuzz",
    "qt",
    "ipc",
    "univalue",
    "leveldb",
    "secp256k1",
    "minisketch",
    "crc32c",
}

CHECK_POW_DEF = re.compile(
    r"^bool CheckProofOfWork\s*\(",
    re.MULTILINE,
)
GET_POW_HASH_DEF = re.compile(
    r"^uint256 GetRigorPowHash\s*\(",
    re.MULTILINE,
)
GET_NEXT_DEF = re.compile(
    r"^unsigned int GetNextWorkRequired\s*\(",
    re.MULTILINE,
)
CALC_NEXT_DEF = re.compile(
    r"^unsigned int CalculateNextWorkRequired\s*\(",
    re.MULTILINE,
)
OPUS_DEF = re.compile(
    r"^uint256 GetOpusHash(FromBaseHash)?\s*\(",
    re.MULTILINE,
)
ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")


def fail(errors: list[str]) -> int:
    print("POW_AUTHORITY_COUNT = FAIL")
    print("FAIL: PoW boundary authority lint")
    for e in errors:
        print(f"  - {e}")
    return 1


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*?$", "", text, flags=re.MULTILINE)
    return text


def extract_function(text: str, signature: str) -> str | None:
    idx = text.find(signature)
    if idx < 0:
        return None
    brace = text.find("{", idx)
    if brace < 0:
        return None
    depth = 0
    i = brace
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[idx : i + 1]
        i += 1
    return None


def iter_prod(path: Path):
    if path.is_file():
        yield path
        return
    if not path.is_dir():
        return
    for p in path.rglob("*"):
        if not p.is_file():
            continue
        if any(part in SKIP_DIR_PARTS for part in p.parts):
            continue
        yield p


def collect_defs(pattern: re.Pattern[str], roots: list[Path] | None = None) -> list[str]:
    found: list[str] = []
    search_roots = roots if roots is not None else [SRC]
    for root in search_roots:
        for p in iter_prod(root):
            if p.suffix != ".cpp":
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            if pattern.search(text):
                try:
                    found.append(p.relative_to(ROOT).as_posix())
                except ValueError:
                    found.append(str(p))
    return sorted(set(found))


def coq_assumption_counts(path: Path) -> tuple[int, int, int]:
    admitted = axioms = parameters = 0
    if not path.is_file():
        return admitted, axioms, parameters
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = ASSUME_RE.match(line)
        if not m:
            continue
        kind = m.group(1)
        if kind == "Admitted":
            admitted += 1
        elif kind == "Axiom":
            axioms += 1
        else:
            parameters += 1
    return admitted, axioms, parameters


def main() -> int:
    errors: list[str] = []

    for required in (POW, POW_H, POW_DIGEST, POW_DIGEST_H, BLOCK_H, VALIDATION, KERNEL_H,
                     DOC, TEST, GATE, FUZZ, MANIFEST, POW_DIGEST_V, SURFACE_V):
        if not required.is_file():
            errors.append(f"missing {required.relative_to(ROOT)}")

    check_defs = collect_defs(CHECK_POW_DEF)
    if check_defs != ["core/src/pow.cpp"]:
        errors.append(f"CheckProofOfWork cpp definitions: {check_defs}")

    hash_defs = collect_defs(GET_POW_HASH_DEF)
    if hash_defs != ["core/src/rigor_kernel/pow_digest.cpp"]:
        errors.append(f"GetRigorPowHash cpp definitions: {hash_defs}")

    next_defs = collect_defs(GET_NEXT_DEF)
    if next_defs != ["core/src/pow.cpp"]:
        errors.append(f"GetNextWorkRequired cpp definitions: {next_defs}")

    calc_defs = collect_defs(CALC_NEXT_DEF)
    if calc_defs != ["core/src/pow.cpp"]:
        errors.append(f"CalculateNextWorkRequired cpp definitions: {calc_defs}")

    pow_cpp = POW.read_text(encoding="utf-8", errors="replace") if POW.is_file() else ""
    pow_h = POW_H.read_text(encoding="utf-8", errors="replace") if POW_H.is_file() else ""
    digest_cpp = POW_DIGEST.read_text(encoding="utf-8", errors="replace") if POW_DIGEST.is_file() else ""
    digest_h = POW_DIGEST_H.read_text(encoding="utf-8", errors="replace") if POW_DIGEST_H.is_file() else ""
    val = VALIDATION.read_text(encoding="utf-8", errors="replace") if VALIDATION.is_file() else ""
    val_code = strip_comments(val)

    check_fn = extract_function(pow_cpp, "bool CheckProofOfWork")
    if not check_fn:
        errors.append("CheckProofOfWork body missing")
    else:
        check_code = strip_comments(check_fn)
        if "GetRigorPowHash" not in check_code:
            errors.append("CheckProofOfWork does not call GetRigorPowHash")
        if re.search(r"\bGetOpusHash(FromBaseHash)?\s*\(", check_code):
            errors.append("CheckProofOfWork still calls GetOpusHash")
        if "SHA256d" in check_code or "SerializeHash" in check_code:
            errors.append("CheckProofOfWork uses SHA256d as the PoW digest")
        if "fNegative" not in check_code or "fOverflow" not in check_code:
            errors.append("CheckProofOfWork missing compact-target fail-closed")
        if "bnTarget == 0" not in check_code:
            errors.append("CheckProofOfWork missing zero-target REJECT")
        if "powLimit" not in check_code:
            errors.append("CheckProofOfWork missing powLimit clamp")
        if "return true" not in check_code or "return false" not in check_code:
            errors.append("CheckProofOfWork is not a binary ACCEPT/REJECT")

    if re.search(r"CheckProofOfWork\s*\(\s*const uint256", pow_h):
        errors.append("CheckProofOfWork(uint256) overload reintroduced")

    next_fn = extract_function(pow_cpp, "unsigned int GetNextWorkRequired")
    if not next_fn:
        errors.append("GetNextWorkRequired body missing")
    else:
        next_code = strip_comments(next_fn)
        if "GetNextWorkASERT" not in next_code:
            errors.append("GetNextWorkRequired does not call GetNextWorkASERT")
        if "CalculateNextWorkRequired" in next_code:
            errors.append("GetNextWorkRequired calls legacy CalculateNextWorkRequired")

    asert_fn = extract_function(pow_cpp, "unsigned int GetNextWorkASERT")
    if not asert_fn:
        errors.append("GetNextWorkASERT body missing")
    else:
        asert_code = strip_comments(asert_fn)
        if "ASERT_RADIX" not in asert_code:
            errors.append("GetNextWorkASERT missing aserti3-2d radix")
        if "ClampTargetToPrev" not in asert_code:
            errors.append("GetNextWorkASERT missing per-block clamp")

    calc_fn = extract_function(pow_cpp, "unsigned int CalculateNextWorkRequired")
    if not calc_fn:
        errors.append("CalculateNextWorkRequired body missing")
    if "GetNextWorkRequired(" not in val:
        errors.append("validation.cpp does not call GetNextWorkRequired")
    if "CalculateNextWorkRequired(" in val_code:
        errors.append("validation.cpp calls legacy CalculateNextWorkRequired")
    if "CheckProofOfWork(" not in val:
        errors.append("validation.cpp does not call CheckProofOfWork")
    if re.search(r"\bGetOpusHash(FromBaseHash)?\s*\(", val_code):
        errors.append("validation.cpp calls GetOpusHash")
    if re.search(r"\bGetRigorPowHash\s*\(", val_code):
        errors.append("validation.cpp hashes PoW itself instead of CheckProofOfWork")

    connect = extract_function(val, "bool CChainState::ConnectBlock") or extract_function(
        val, "bool Chainstate::ConnectBlock"
    )
    if not connect:
        errors.append("ConnectBlock body missing")
    else:
        connect_code = strip_comments(connect)
        if "CheckProofOfWork" in connect_code:
            errors.append("ConnectBlock contains a second PoW check (PoW belongs at header admission)")
        if "GetRigorPowHash" in connect_code:
            errors.append("ConnectBlock computes a PoW digest")
        if "RigorConsensusKernel::EvaluateConnect" not in connect:
            errors.append("ConnectBlock does not call RigorConsensusKernel::EvaluateConnect")

    kernel_h = KERNEL_H.read_text(encoding="utf-8", errors="replace") if KERNEL_H.is_file() else ""
    if 'PRODUCTION_CONSENSUS_AUTHORITY[] = "RigorConsensusKernel(C++)"' not in kernel_h:
        errors.append("PRODUCTION_CONSENSUS_AUTHORITY must be RigorConsensusKernel(C++)")
    if "inline constexpr bool EXTRACTED_KERNEL_PRODUCTION = false" not in kernel_h:
        errors.append("EXTRACTED_KERNEL_PRODUCTION must be false")
    kernel_cpp = KERNEL_CPP.read_text(encoding="utf-8", errors="replace") if KERNEL_CPP.is_file() else ""
    kernel_code = strip_comments(kernel_cpp)
    if re.search(r"\bCheckProofOfWork\s*\(", kernel_code):
        errors.append("ConsensusKernel implements PoW")
    if re.search(r"\bGetRigorPowHash\s*\(", kernel_code):
        errors.append("ConsensusKernel computes PoW digest")
    if re.search(r"\bGetNextWorkRequired\s*\(", kernel_code):
        errors.append("ConsensusKernel implements ASERT")

    digest_fn = extract_function(digest_cpp, "uint256 GetRigorPowHash")
    if not digest_fn:
        errors.append("GetRigorPowHash body missing")
    else:
        dcode = strip_comments(digest_fn)
        if "BuildPowPreimage" not in dcode:
            errors.append("GetRigorPowHash does not call BuildPowPreimage")
        if "Sha3_256Short" not in dcode:
            errors.append("GetRigorPowHash does not call Sha3_256Short")
        if "SHA256" in dcode or "CHash256" in dcode:
            errors.append("GetRigorPowHash uses SHA256")
        if "GetOpusHash" in dcode:
            errors.append("GetRigorPowHash falls back to OpuHash")

    if "RGRPOW01" not in digest_h and "POW_DOMAIN_TAG" not in digest_h:
        errors.append("PoW domain tag missing")
    if "SHA3-256" not in digest_h:
        errors.append("GetRigorPowHash is not documented as SHA3-256")
    if "POW_HEADER_LEN" not in digest_h or "80" not in digest_h:
        errors.append("POW_HEADER_LEN is not 80")

    block_h = BLOCK_H.read_text(encoding="utf-8", errors="replace") if BLOCK_H.is_file() else ""
    if "READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce)" not in block_h:
        errors.append("CBlockHeader SERIALIZE_METHODS field list drifted")
    extra_header_fields = ("nAuxPow", "hashMix", "nNonce64", "extraNonce", "nSolution")
    serialize_fn = extract_function(block_h, "SERIALIZE_METHODS(CBlockHeader")
    if serialize_fn:
        for extra in extra_header_fields:
            if extra in serialize_fn:
                errors.append(f"CBlockHeader gained extra field {extra}")

    block_cpp = BLOCK_CPP.read_text(encoding="utf-8", errors="replace") if BLOCK_CPP.is_file() else ""
    gethash = extract_function(block_cpp, "uint256 CBlockHeader::GetHash")
    if not gethash:
        errors.append("CBlockHeader::GetHash body missing")
    elif "SerializeHash" not in gethash:
        errors.append("CBlockHeader::GetHash is not SerializeHash (SHA256d identity)")

    opus_callers = []
    for p in iter_prod(SRC):
        if p.suffix != ".cpp":
            continue
        if p.name in {"pow_opus.cpp"}:
            continue
        text = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        if re.search(r"\bGetOpusHash(FromBaseHash)?\s*\(", text):
            try:
                opus_callers.append(p.relative_to(ROOT).as_posix())
            except ValueError:
                opus_callers.append(str(p))
    if opus_callers:
        errors.append(f"production GetOpusHash callers (legacy PoW path): {opus_callers}")

    def forbid_defs(path: Path, label: str) -> None:
        files = [path] if path.is_file() else list(iter_prod(path))
        for p in files:
            if p.suffix != ".cpp":
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            try:
                rel = p.relative_to(ROOT).as_posix()
            except ValueError:
                rel = str(p)
            if CHECK_POW_DEF.search(text):
                errors.append(f"{label} defines CheckProofOfWork in {rel}")
            if GET_POW_HASH_DEF.search(text):
                errors.append(f"{label} defines GetRigorPowHash in {rel}")
            if GET_NEXT_DEF.search(text):
                errors.append(f"{label} defines GetNextWorkRequired in {rel}")

    forbid_defs(MINER, "miner")
    forbid_defs(WALLET, "wallet")
    forbid_defs(RPC, "rpc")

    rpc_mining = SRC / "rpc/mining.cpp"
    if rpc_mining.is_file():
        rpc_text = rpc_mining.read_text(encoding="utf-8", errors="replace")
        rpc_code = strip_comments(rpc_text)
        if "skippow" in rpc_code.lower() or "bypasspow" in rpc_code.lower():
            errors.append("RPC mining exposes a PoW override")
        if "CheckProofOfWork" not in rpc_text and "GenerateBlock" in rpc_text:
            errors.append("RPC GenerateBlock does not consult CheckProofOfWork")

    if MAKEFILE_TEST.is_file():
        inc = MAKEFILE_TEST.read_text(encoding="utf-8", errors="replace")
        if "test_pow_boundary_security.cpp" not in inc:
            errors.append("test_pow_boundary_security.cpp not in Makefile.test.include")

    powv = POW_DIGEST_V.read_text(encoding="utf-8", errors="replace") if POW_DIGEST_V.is_file() else ""
    if "equivalence_claimed = false" not in powv and "equivalence_claimed   = false" not in powv:
        errors.append("PowDigest.v must keep equivalence_claimed = false")
    if "refinement_proved   = false" not in powv and "refinement_proved = false" not in powv:
        errors.append("PowDigest.v must keep SHA3 model refinement_proved = false")

    surface = SURFACE_V.read_text(encoding="utf-8", errors="replace") if SURFACE_V.is_file() else ""
    if "Theorem pow_boundary_accept_iff_le" not in surface:
        errors.append("existing PowBoundary theorem pow_boundary_accept_iff_le missing")
    if "Definition PowKernelBoundary" not in surface:
        errors.append("existing PowKernelBoundary missing")
    admitted, axioms, parameters = coq_assumption_counts(SURFACE_V)
    if admitted or axioms or parameters:
        errors.append(
            f"ConsensusSurfaceClosure Coq assumptions drifted: ADMITTED={admitted} AXIOMS={axioms} PARAMETERS={parameters}"
        )

    doc = DOC.read_text(encoding="utf-8", errors="replace") if DOC.is_file() else ""
    for req in (
        "POW_AUTHORITY_COUNT                 = 1",
        "HASH_AUTHORITY_COUNT                = 1",
        "ASERT_AUTHORITY_COUNT               = 1",
        "TARGET_VALIDATION_STATUS            = PASS",
        "HEADER_SERIALIZATION_STATUS         = PASS",
        "FUZZ_STATUS                         = PASS",
        "CONSENSUS_BYPASS_STATUS             = NONE",
        "PRODUCTION_CONSENSUS_MODIFIED = NO",
        "EQUIVALENCE_PROVED                     = FALSE",
        "REFINEMENT_PROVED",
        "SHA256",
        "51%",
        "RIGOR_POW_BOUNDARY_SECURITY_AUDIT_CLOSED",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")
    forbidden_claims = (
        "SHA256 is proven secure",
        "PoW is economically secure",
        "therefore 51% attacks cannot occur",
        "EQUIVALENCE_PROVED = TRUE",
        "EQUIVALENCE_PROVED=true",
    )
    for bad in forbidden_claims:
        if bad in doc:
            errors.append(f"document makes forbidden claim: {bad}")

    man = MANIFEST.read_text(encoding="utf-8", errors="replace") if MANIFEST.is_file() else ""
    for req in (
        '"pow_authority_count": 1',
        '"hash_authority_count": 1',
        '"asert_authority_count": 1',
        '"target_validation_status": "PASS"',
        '"header_serialization_status": "PASS"',
        '"fuzz_status": "PASS"',
        '"consensus_bypass_status": "NONE"',
        '"production_consensus_modified": false',
        '"equivalence_proved": false',
        "RIGOR_POW_BOUNDARY_SECURITY_AUDIT_CLOSED",
    ):
        if req not in man:
            errors.append(f"manifest missing {req}")

    if errors:
        return fail(errors)

    print("POW_AUTHORITY_COUNT = 1")
    print("HASH_AUTHORITY_COUNT = 1")
    print("ASERT_AUTHORITY_COUNT = 1")
    print("TARGET_VALIDATION_STATUS = PASS")
    print("POW_DECISION_AUTHORITY = CheckProofOfWork")
    print("POW_DIGEST_AUTHORITY = GetRigorPowHash")
    print("BLOCK_HASH_AUTHORITY = CBlockHeader::GetHash")
    print("DIFFICULTY_AUTHORITY = GetNextWorkRequired")
    print("LEGACY_POW_PATH = NONE")
    print("MINER_POW_AS_AUTHORITY = NONE")
    print("WALLET_POW_AS_AUTHORITY = NONE")
    print("RPC_POW_OVERRIDE = NONE")
    print("OPUSHASH_CONSENSUS_CALLERS = NONE")
    print_status()
    print("CONSENSUS_MODIFIED = NO")
    print("PASS: unique PoW boundary authorities")
    return 0


if __name__ == "__main__":
    sys.exit(main())
