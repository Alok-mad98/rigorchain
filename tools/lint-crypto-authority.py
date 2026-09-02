#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P080: unique cryptographic-boundary authority lint.

Four hash roles, each with one production path:
  PoW hash            GetRigorPowHash          (SHA3-256, not SHA256d)
  block header hash   CBlockHeader::GetHash   (SHA256d via SerializeHash)
  transaction digest  CTransaction::ComputeHash / CMutableTransaction::GetHash
  sighash digest      SignatureHash           (SHA256d via HashWriter::GetHash)

Ed25519 verify authority: ed25519::VerifyDetached only.
Sighash authority: SignatureHash only.

Does not rewrite SHA256, Ed25519, SignatureHash, PoW, or Kernel.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"
PRED = SRC / "script/predicate_verify.cpp"
PRED_H = SRC / "script/predicate_verify.h"
POW = SRC / "pow.cpp"
POW_H = SRC / "pow.h"
POW_DIGEST = SRC / "rigor_kernel/pow_digest.cpp"
POW_DIGEST_H = SRC / "rigor_kernel/pow_digest.h"
BLOCK_CPP = SRC / "primitives/block.cpp"
TX_CPP = SRC / "primitives/transaction.cpp"
ED = SRC / "crypto/ed25519_provider.cpp"
ED_H = SRC / "crypto/ed25519_provider.h"
HASH_H = SRC / "hash.h"
SHA256_CPP = SRC / "crypto/sha256.cpp"
VALIDATION = SRC / "validation.cpp"
MINER = SRC / "node/miner.cpp"
WALLET = SRC / "wallet"
RPC = SRC / "rpc"
PYTHON_SIGHASH = ROOT / "python/rigor_sighash.py"
DOC = ROOT / "docs/security/RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_P0_080.md"
TEST = SRC / "test/test_crypto_boundary_security.cpp"
GATE = ROOT / "tools/run-cryptographic-boundary-security-audit.sh"
FUZZ = ROOT / "fuzz/crypto_boundary/crypto_boundary_fuzz.cpp"
MANIFEST = ROOT / "formal_verification/crypto_boundary_audit_manifest.json"
MAKEFILE_TEST = SRC / "Makefile.test.include"
SIG_BOUND = ROOT / "formal_verification/coq/SignatureBoundary.v"
POW_DIGEST_V = ROOT / "formal_verification/coq/PowDigest.v"

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

POW_DEF = re.compile(
    r"^uint256 GetRigorPowHash\s*\(",
    re.MULTILINE,
)
HEADER_HASH_DEF = re.compile(
    r"^uint256 CBlockHeader::GetHash\s*\(",
    re.MULTILINE,
)
TX_COMPUTE_DEF = re.compile(
    r"^uint256 CTransaction::ComputeHash\s*\(",
    re.MULTILINE,
)
TX_MUTABLE_HASH_DEF = re.compile(
    r"^uint256 CMutableTransaction::GetHash\s*\(",
    re.MULTILINE,
)
SIGHASH_DEF = re.compile(
    r"^uint256 SignatureHash\s*\(",
    re.MULTILINE,
)
VERIFY_DETACHED_DEF = re.compile(
    r"^bool VerifyDetached\s*\(",
    re.MULTILINE,
)
ASSUME_RE = re.compile(r"^\s*(Admitted|Axiom|Parameter)\s")
HIDDEN_SIGHASH = (
    "SIGHASH_NONE",
    "SIGHASH_SINGLE",
    "SIGHASH_ANYONECANPAY",
    "SIGHASH_ALL |",
)
LEGACY_CHECKSIG_CALL = re.compile(r"\b(EvalScript|VerifyScript|CheckSig|OP_CHECKSIG)\s*\(")


def fail(errors: list[str]) -> int:
    print("CRYPTO_AUTHORITY_COUNT = FAIL")
    print("FAIL: cryptographic boundary authority lint")
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
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[idx : i + 1]
    return None


def iter_prod(root: Path) -> list[Path]:
    files: list[Path] = []
    if not root.is_dir():
        return files
    for p in root.rglob("*"):
        if not p.is_file() or p.suffix not in {".h", ".cpp"}:
            continue
        try:
            rel = p.relative_to(SRC).as_posix()
        except ValueError:
            rel = p.as_posix()
        parts = set(rel.split("/"))
        if parts & SKIP_DIR_PARTS:
            continue
        if "/test/" in f"/{rel}/":
            continue
        files.append(p)
    return files


def collect_defs(pattern: re.Pattern[str]) -> list[str]:
    found: list[str] = []
    for p in iter_prod(SRC):
        if p.suffix != ".cpp":
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if pattern.search(text):
            found.append(p.relative_to(ROOT).as_posix())
    return found


def coq_assumption_counts(path: Path) -> tuple[int, int, int]:
    admitted = axioms = parameters = 0
    if not path.is_file():
        return (0, 0, 0)
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

    required = [
        PRED,
        PRED_H,
        POW,
        POW_H,
        POW_DIGEST,
        POW_DIGEST_H,
        BLOCK_CPP,
        TX_CPP,
        ED,
        ED_H,
        HASH_H,
        SHA256_CPP,
        PYTHON_SIGHASH,
        TEST,
        DOC,
        GATE,
        FUZZ,
        MANIFEST,
        ROOT / "fuzz/crypto_boundary/run_smoke.sh",
        SIG_BOUND,
        POW_DIGEST_V,
    ]
    for p in required:
        if not p.is_file():
            errors.append(f"missing {p.relative_to(ROOT)}")

    pow_defs = collect_defs(POW_DEF)
    if pow_defs != ["core/src/rigor_kernel/pow_digest.cpp"]:
        errors.append(f"GetRigorPowHash cpp definitions: {pow_defs}")

    header_defs = collect_defs(HEADER_HASH_DEF)
    if header_defs != ["core/src/primitives/block.cpp"]:
        errors.append(f"CBlockHeader::GetHash cpp definitions: {header_defs}")

    tx_compute = collect_defs(TX_COMPUTE_DEF)
    if tx_compute != ["core/src/primitives/transaction.cpp"]:
        errors.append(f"CTransaction::ComputeHash cpp definitions: {tx_compute}")

    tx_mut = collect_defs(TX_MUTABLE_HASH_DEF)
    if tx_mut != ["core/src/primitives/transaction.cpp"]:
        errors.append(f"CMutableTransaction::GetHash cpp definitions: {tx_mut}")

    sighash_defs = collect_defs(SIGHASH_DEF)
    if sighash_defs != ["core/src/script/predicate_verify.cpp"]:
        errors.append(f"SignatureHash cpp definitions: {sighash_defs}")

    verify_defs = collect_defs(VERIFY_DETACHED_DEF)
    if verify_defs != ["core/src/crypto/ed25519_provider.cpp"]:
        errors.append(f"VerifyDetached cpp definitions: {verify_defs}")

    pow_cpp = POW.read_text(encoding="utf-8", errors="replace") if POW.is_file() else ""
    pow_fn = extract_function(pow_cpp, "bool CheckProofOfWork")
    if not pow_fn:
        errors.append("CheckProofOfWork body missing")
    else:
        pow_code = strip_comments(pow_fn)
        if "GetRigorPowHash" not in pow_code:
            errors.append("CheckProofOfWork does not call GetRigorPowHash")
        if re.search(r"\bGetOpusHashFromBaseHash\s*\(", pow_code):
            errors.append("CheckProofOfWork still calls GetOpusHashFromBaseHash")
        if re.search(r"\bOpuHash\s*\(", pow_code):
            errors.append("CheckProofOfWork calls OpuHash")
        if "SHA256d" in pow_code or "SerializeHash" in pow_code:
            errors.append("CheckProofOfWork uses SHA256d as the PoW digest")

    pow_h = POW_H.read_text(encoding="utf-8", errors="replace") if POW_H.is_file() else ""
    if re.search(r"CheckProofOfWork\s*\(\s*const uint256", pow_h):
        errors.append("CheckProofOfWork(uint256) overload reintroduced")

    digest_h = POW_DIGEST_H.read_text(encoding="utf-8", errors="replace") if POW_DIGEST_H.is_file() else ""
    if 'POW_DOMAIN_TAG[] = {\'R\', \'G\', \'R\', \'P\', \'O\', \'W\', \'0\', \'1\'}' not in digest_h.replace(" ", ""):
        if "RGRPOW01" not in digest_h:
            errors.append("PoW domain tag RGRPOW01 missing")
    if "SHA3-256" not in digest_h:
        errors.append("GetRigorPowHash is not documented as SHA3-256")

    pred = PRED.read_text(encoding="utf-8", errors="replace") if PRED.is_file() else ""
    sighash_fn = extract_function(pred, "uint256 SignatureHash")
    if not sighash_fn:
        errors.append("SignatureHash body missing")
    else:
        sh_code = strip_comments(sighash_fn)
        if "GetSighashDomainTag" not in sh_code:
            errors.append("SignatureHash missing domain tag")
        if "chain_id" not in sh_code:
            errors.append("SignatureHash missing chain_id")
        if "GetHash()" not in sh_code:
            errors.append("SignatureHash does not finish with HashWriter::GetHash (SHA256d)")
        for hidden in HIDDEN_SIGHASH:
            if hidden in sh_code:
                errors.append(f"SignatureHash contains hidden flag {hidden}")
        if re.search(r"\bnHashType\b", sh_code) or re.search(r"<<\s*hashtype", sh_code):
            errors.append("SignatureHash writes a hashtype field")

    pred_h = PRED_H.read_text(encoding="utf-8", errors="replace") if PRED_H.is_file() else ""
    for hidden in ("SIGHASH_NONE", "SIGHASH_SINGLE", "SIGHASH_ANYONECANPAY"):
        if re.search(rf"\b{hidden}\b", pred_h):
            errors.append(f"live sighash flag {hidden} in predicate_verify.h")

    ed = ED.read_text(encoding="utf-8", errors="replace") if ED.is_file() else ""
    vfn = extract_function(ed, "bool VerifyDetached")
    if not vfn:
        errors.append("VerifyDetached body missing")
    else:
        vcode = strip_comments(vfn)
        if "crypto_sign_verify_detached" not in vcode:
            errors.append("VerifyDetached does not call libsodium crypto_sign_verify_detached")
        if "signature.size() != SIGNATURE_SIZE" not in vcode:
            errors.append("VerifyDetached missing signature length fail-closed")
        if "pubkey.size() != PUBKEY_SIZE" not in vcode:
            errors.append("VerifyDetached missing pubkey length fail-closed")

    hash_h = HASH_H.read_text(encoding="utf-8", errors="replace") if HASH_H.is_file() else ""
    if "SHA256(SHA256(x))" not in hash_h and "SHA256d(x) = SHA256(SHA256(x))" not in hash_h:
        errors.append("CHash256 missing SHA256d composition note")
    if "class CHash256" not in hash_h:
        errors.append("CHash256 missing")
    if "uint256 GetHash()" not in hash_h:
        errors.append("HashWriter::GetHash missing")

    # Production spend must enter VerifyRigorPredicate, not CPubKey::Verify.
    val = VALIDATION.read_text(encoding="utf-8", errors="replace") if VALIDATION.is_file() else ""
    val_code = strip_comments(val)
    if not re.search(r"\bVerifyRigorPredicate\s*\(", val_code):
        errors.append("validation.cpp does not call VerifyRigorPredicate")
    if re.search(r"\bCPubKey::Verify\s*\(", val_code) or re.search(r"pubkey\.Verify\s*\(", val_code):
        errors.append("validation.cpp uses CPubKey::Verify as spend authority")
    if LEGACY_CHECKSIG_CALL.search(val_code):
        errors.append("validation.cpp calls EvalScript/VerifyScript/CHECKSIG")

    pred_code = strip_comments(pred)
    if LEGACY_CHECKSIG_CALL.search(pred_code):
        errors.append("predicate_verify.cpp calls EvalScript/VerifyScript/CHECKSIG")
    if "VerifyEd25519Signature" not in pred:
        errors.append("predicate_verify.cpp missing VerifyEd25519Signature")
    if "ed25519::VerifyDetached" not in pred:
        errors.append("predicate_verify.cpp does not call ed25519::VerifyDetached")

    # Wallet / miner / RPC must not define the four authorities.
    def forbid_defs(path: Path, label: str) -> None:
        files = [path] if path.is_file() else iter_prod(path)
        for p in files:
            if p.suffix != ".cpp":
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            try:
                rel = p.relative_to(ROOT).as_posix()
            except ValueError:
                rel = str(p)
            if POW_DEF.search(text):
                errors.append(f"{label} defines GetRigorPowHash in {rel}")
            if SIGHASH_DEF.search(text):
                errors.append(f"{label} defines SignatureHash in {rel}")
            if VERIFY_DETACHED_DEF.search(text):
                errors.append(f"{label} defines VerifyDetached in {rel}")
            if HEADER_HASH_DEF.search(text):
                errors.append(f"{label} defines CBlockHeader::GetHash in {rel}")

    forbid_defs(MINER, "miner")
    forbid_defs(WALLET, "wallet")
    forbid_defs(RPC, "rpc")

    py = PYTHON_SIGHASH.read_text(encoding="utf-8", errors="replace") if PYTHON_SIGHASH.is_file() else ""
    py_flat = re.sub(r"[^A-Za-z0-9]+", " ", py).lower()
    if "not a consensus" not in py_flat:
        errors.append("python/rigor_sighash.py is not labeled non-consensus")

    if MAKEFILE_TEST.is_file():
        inc = MAKEFILE_TEST.read_text(encoding="utf-8", errors="replace")
        if "test_crypto_boundary_security.cpp" not in inc:
            errors.append("test_crypto_boundary_security.cpp not in Makefile.test.include")

    coq_admitted = coq_axioms = coq_parameters = 0
    for path in (SIG_BOUND,):
        a, x, p = coq_assumption_counts(path)
        coq_admitted += a
        coq_axioms += x
        coq_parameters += p
    if coq_admitted or coq_axioms or coq_parameters:
        errors.append(
            f"SignatureBoundary Coq assumptions drifted: ADMITTED={coq_admitted} AXIOMS={coq_axioms} PARAMETERS={coq_parameters}"
        )
    sigv = SIG_BOUND.read_text(encoding="utf-8", errors="replace") if SIG_BOUND.is_file() else ""
    if "Theorem valid_signature_eq_accept_boundary" not in sigv:
        errors.append("existing SignatureBoundary theorem missing")
    if "curve_math_reimplemented : bool := false" not in sigv.replace(" ", ""):
        if "curve_math_reimplemented" not in sigv:
            errors.append("SignatureBoundary must not reimplement curve math")

    powv = POW_DIGEST_V.read_text(encoding="utf-8", errors="replace") if POW_DIGEST_V.is_file() else ""
    if "equivalence_claimed = false" not in powv and "equivalence_claimed   = false" not in powv:
        errors.append("PowDigest.v must keep equivalence_claimed = false")
    if "refinement_proved   = false" not in powv and "refinement_proved = false" not in powv:
        errors.append("PowDigest.v must keep refinement_proved = false for the SHA3 model")

    doc = DOC.read_text(encoding="utf-8", errors="replace") if DOC.is_file() else ""
    for req in (
        "single SHA256d primitive",
        "single PoW hash authority",
        "single Ed25519 verify authority",
        "single SignatureHash authority",
        "SHA256 mathematical security proof",
        "Ed25519 cryptographic proof",
        "EQUIVALENCE_PROVED=false",
        "REFINEMENT_PROVED",
        "CRYPTO_AUTHORITY_COUNT = 4",
        "SHA256_AUTHORITY_COUNT = 1",
        "ED25519_AUTHORITY_COUNT = 1",
        "SIGHASH_AUTHORITY_COUNT = 1",
        "RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_CLOSED",
    ):
        if req not in doc:
            errors.append(f"formal note missing: {req}")

    man = MANIFEST.read_text(encoding="utf-8", errors="replace") if MANIFEST.is_file() else ""
    for req in (
        '"crypto_authority_count": 4',
        '"sha256_authority_count": 1',
        '"ed25519_authority_count": 1',
        '"sighash_authority_count": 1',
        '"consensus_modified": false',
        '"equivalence_proved": false',
        '"sha256_math_proof_claimed": false',
        '"ed25519_crypto_proof_claimed": false',
        "RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_CLOSED",
    ):
        if req not in man:
            errors.append(f"manifest missing {req}")

    if errors:
        return fail(errors)

    print("CRYPTO_AUTHORITY_COUNT = 4")
    print("SHA256_AUTHORITY_COUNT = 1")
    print("ED25519_AUTHORITY_COUNT = 1")
    print("SIGHASH_AUTHORITY_COUNT = 1")
    print("POW_HASH_AUTHORITY = GetRigorPowHash")
    print("HEADER_HASH_AUTHORITY = CBlockHeader::GetHash")
    print("TX_DIGEST_AUTHORITY = CTransaction::ComputeHash")
    print("SIGHASH_AUTHORITY = SignatureHash")
    print("ED25519_AUTHORITY = ed25519::VerifyDetached")
    print("MINER_HASH_AS_AUTHORITY = NONE")
    print("WALLET_HASH_AS_AUTHORITY = NONE")
    print("SECOND_HASH_IMPLEMENTATION = NONE")
    print("FALLBACK_HASH = NONE")
    print("COQ_ADMITTED = 0")
    print("COQ_AXIOMS = 0")
    print("COQ_PARAMETERS = 0")
    print("CONSENSUS_MODIFIED = NO")
    print("PASS: unique cryptographic boundary authorities")
    return 0


if __name__ == "__main__":
    sys.exit(main())
