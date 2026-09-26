#!/usr/bin/env python3
"""Lightweight secret / operator-leak scanner.

Default: staged files only (`git diff --cached --name-only`).
`--all`: every tracked file (CI, PR into main).

Not a consensus check. Not a substitute for human review.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAX_BYTES = 2 * 1024 * 1024

FORBIDDEN_ROOTS = ("ops-internal/", "ops/", "internal/", "private/")
SECRET_NAMES = {
    ".env",
    "credentials.json",
    "docker-compose.production.yml",
}
SECRET_SUFFIXES = (".pem", ".key", ".secret")
EXAMPLE_ENV_OK = (".example.env", "config.example.env")

PEM_PRIVATE = re.compile(
    r"-----BEGIN (OPENSSH|RSA|EC|DSA|PGP) PRIVATE KEY-----"
)
GITHUB_TOKEN = re.compile(r"\b(ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9_]{20,}\b")
GITHUB_PAT = re.compile(r"\bgithub_pat_[A-Za-z0-9_]{20,}\b")
AWS_KEY = re.compile(r"\bAKIA[0-9A-Z]{16}\b")
# Conf/env style only. Do not use IGNORECASE: rpcUser in TypeScript is a field name.
ASSIGNMENT = re.compile(
    r"^\s*(?:export\s+)?"
    r"(rpcpassword|rpcuser|password|passwd|secret|token|api[_-]?key|"
    r"AWS_SECRET_ACCESS_KEY|TOKEN|PASSWORD|SECRET|"
    r"private_key|mnemonic|seed_phrase|seedphrase)"
    r"\s*[=:]\s*(.+?)\s*$"
)
SSH_KEY_PATH = re.compile(
    r"(?i)(C:\\Users\\[^\\\s]+\\.ssh\\|/home/[^/\s]+/\.ssh/id_|~/.ssh/id_(rsa|ed25519|ecdsa))"
)
SSH_FINGERPRINT = re.compile(r"SHA256:[A-Za-z0-9+/]{20,}")
SERVER_IP_FILLED = re.compile(
    r"(?i)SERVER_IP\s*\|\s*`?[0-9]{1,3}(?:\.[0-9]{1,3}){3}"
)
# Standalone dotted-quad. Reject kernel versions like 6.6.87.2-microsoft.
IPV4 = re.compile(
    r"(?<![0-9.])((?:[0-9]{1,3}\.){3}[0-9]{1,3})(?![\w.-])"
)
SSH_PRIV_CONTEXT = re.compile(
    r"(?i)(ssh\s+-i\s+\S+|IdentityFile\s+\S+|authorized_keys)"
)

PLACEHOLDER = re.compile(
    r"(?i)(<|…|\.\.\.|CHANGE_ME|changeme|example|placeholder|GENERATE|"
    r"TODO|your-|secret\)|local-only|dummy|not-a-secret|replace|"
    r"e2e|rigortest|\btest\b|\$\{|\$[A-Z_]|secrets\.|"
    r"<(pw|password|token|key))"
)
CODE_RHS = re.compile(r"[()\[\]{}]|[.](?:get|split|strip|env)|process\.|os\.|None\b|null\b|undefined\b")

DOC_PREFIXES = ("docs/", "audit/")
SKIP_INFRA_PREFIXES = (
    "core/",
    "archive/",
    "pool/",
    "formal_verification/",
    "contrib/",
    ".github/workflows/",
)


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    rule: str
    reason: str
    action: str


def git(args: list[str]) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True)


def staged_paths() -> list[str]:
    out = git(["diff", "--cached", "--name-only", "--diff-filter=ACMR"])
    return [p for p in out.splitlines() if p]


def tracked_paths() -> list[str]:
    out = git(["ls-files", "-z"])
    return [p for p in out.split("\0") if p]


def is_forbidden_path(path: str) -> bool:
    return path.startswith(FORBIDDEN_ROOTS) or path in {
        "ops-internal",
        "ops",
        "internal",
        "private",
    }


def is_secret_filename(path: str) -> bool:
    base = os.path.basename(path)
    if base in SECRET_NAMES:
        return True
    if any(base.endswith(ok) for ok in EXAMPLE_ENV_OK):
        return False
    if base.endswith(".env"):
        return True
    if any(base.endswith(suf) for suf in SECRET_SUFFIXES):
        return True
    if path in ("deployment/production.yaml",) or path.startswith("deployment/"):
        if path.endswith((".yaml", ".yml")):
            return True
    return False


def is_doc_like(path: str) -> bool:
    if path.startswith(SKIP_INFRA_PREFIXES):
        return False
    return path.startswith(DOC_PREFIXES)


def skip_assignment(path: str) -> bool:
    return path.startswith(SKIP_INFRA_PREFIXES)


def valid_ipv4(text: str) -> bool:
    parts = text.split(".")
    if len(parts) != 4:
        return False
    try:
        octs = [int(p) for p in parts]
    except ValueError:
        return False
    if any(o < 0 or o > 255 for o in octs):
        return False
    if octs[0] == 0:
        return False
    # Windows/NSIS version tuples (25.0.0.0), not hosts.
    if octs[1] == 0 and octs[2] == 0 and octs[3] == 0:
        return False
    return True


def is_loopback_or_unspecified(ip: str) -> bool:
    return ip in {"127.0.0.1", "0.0.0.0"}


def is_rfc1918(ip: str) -> bool:
    a, b, *_ = (int(x) for x in ip.split("."))
    if a == 10:
        return True
    if a == 192 and b == 168:
        return True
    if a == 172 and 16 <= b <= 31:
        return True
    return False


def is_docnet(ip: str) -> bool:
    a, b, *_ = (int(x) for x in ip.split("."))
    return (a, b) in {(192, 0), (198, 51), (203, 0)}


def is_placeholder(value: str) -> bool:
    v = value.strip().strip("'\"`")
    if v[:1] in "bfruFBRU" and len(v) > 1 and v[1:2] in {"'", '"'}:
        v = v[2:].strip().strip("'\"`")
    if not v or v in {"x", "…", "None", "True", "False", "null", "undefined"}:
        return True
    if PLACEHOLDER.search(v):
        return True
    if v.startswith("$"):
        return True
    if re.fullmatch(r"[A-Z][A-Z0-9_=]{3,}", v):
        return True
    return False


def is_code_assignment(value: str) -> bool:
    return CODE_RHS.search(value) is not None


def read_text(path: Path) -> str | None:
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) > MAX_BYTES:
        return None
    if b"\0" in data[:8192]:
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        try:
            return data.decode("latin-1")
        except UnicodeDecodeError:
            return None


def decode_git_blob(data: bytes) -> str | None:
    if len(data) > MAX_BYTES:
        return None
    if b"\0" in data[:8192] or data.startswith((b"\x89PNG", b"\xff\xd8\xff", b"GIF8")):
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


def blob_for(path: str, *, staged: bool) -> str | None:
    if staged:
        try:
            return decode_git_blob(subprocess.check_output(["git", "show", f":{path}"], cwd=ROOT))
        except subprocess.CalledProcessError:
            pass
    candidate = Path(path)
    if candidate.is_absolute():
        return read_text(candidate)
    return read_text(ROOT / path) or read_text(candidate)


def scan_text(path: str, text: str) -> list[Finding]:
    findings: list[Finding] = []
    action_secret = "Remove secret or move file to ops-internal."
    action_ops = "Move operator details to local ops-internal/; do not commit."

    for i, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith(("#", "//")):
            continue

        if PEM_PRIVATE.search(line):
            findings.append(
                Finding(
                    path,
                    i,
                    "PRIVATE KEY",
                    "Private key material cannot enter the public repository.",
                    action_secret,
                )
            )
        if GITHUB_TOKEN.search(line) or GITHUB_PAT.search(line):
            findings.append(
                Finding(
                    path,
                    i,
                    "TOKEN",
                    "GitHub token cannot enter the public repository.",
                    action_secret,
                )
            )
        if AWS_KEY.search(line):
            findings.append(
                Finding(
                    path,
                    i,
                    "TOKEN",
                    "Cloud access key cannot enter the public repository.",
                    action_secret,
                )
            )

        if not skip_assignment(path):
            m = ASSIGNMENT.match(line)
            if m:
                value = m.group(2)
                if not is_code_assignment(value) and not is_placeholder(value):
                    findings.append(
                        Finding(
                            path,
                            i,
                            "CREDENTIAL",
                            f"Credential assignment {m.group(1)}= cannot enter the public repository.",
                            action_secret,
                        )
                    )

        if is_doc_like(path):
            if SSH_KEY_PATH.search(line):
                findings.append(
                    Finding(
                        path,
                        i,
                        "SSH",
                        "SSH private-key path is operator access information.",
                        action_ops,
                    )
                )
            if SSH_FINGERPRINT.search(line):
                findings.append(
                    Finding(
                        path,
                        i,
                        "SSH",
                        "SSH key fingerprint is operator access information.",
                        action_ops,
                    )
                )
            if SERVER_IP_FILLED.search(line):
                findings.append(
                    Finding(
                        path,
                        i,
                        "INFRASTRUCTURE",
                        "Filled server inventory cannot enter the public repository.",
                        action_ops,
                    )
                )
            if SSH_PRIV_CONTEXT.search(line) and re.search(
                r"\b([0-9]{1,3}\.){3}[0-9]{1,3}\b", line
            ):
                ip_hit = next(
                    (
                        m.group(0)
                        for m in IPV4.finditer(line)
                        if valid_ipv4(m.group(0)) and not is_loopback_or_unspecified(m.group(0))
                    ),
                    None,
                )
                if ip_hit:
                    findings.append(
                        Finding(
                            path,
                            i,
                            "SSH",
                            "SSH command bound to a non-loopback host is operator access information.",
                            action_ops,
                        )
                    )
            for m in IPV4.finditer(line):
                ip = m.group(0)
                if not valid_ipv4(ip) or is_loopback_or_unspecified(ip) or is_docnet(ip):
                    continue
                if is_rfc1918(ip):
                    findings.append(
                        Finding(
                            path,
                            i,
                            "PRIVATE IP",
                            f"Internal address {ip} belongs in ops-internal/, not public docs.",
                            action_ops,
                        )
                    )
                else:
                    findings.append(
                        Finding(
                            path,
                            i,
                            "INFRASTRUCTURE",
                            f"Public IPv4 {ip} is operator host detail.",
                            action_ops,
                        )
                    )

    return findings


def scan_path(path: str, *, staged: bool) -> list[Finding]:
    findings: list[Finding] = []
    action_ops = "Move operator details to local ops-internal/; do not commit."
    action_secret = "Remove secret or move file to ops-internal."
    if is_forbidden_path(path):
        findings.append(
            Finding(
                path,
                0,
                "INTERNAL PATH",
                "ops-internal/ ops/ internal/ private/ must not be committed.",
                action_ops,
            )
        )
        return findings
    if is_secret_filename(path):
        findings.append(
            Finding(
                path,
                0,
                "SECRET FILE",
                "Secret or production-deploy filename is not allowed.",
                action_secret,
            )
        )
        return findings
    text = blob_for(path, staged=staged)
    if text is None:
        return findings
    findings.extend(scan_text(path, text))
    return findings


def print_findings(findings: list[Finding]) -> None:
    for f in findings:
        print("SECURITY CHECK FAILED")
        print()
        print("File:")
        print(f.path)
        print()
        print("Line:")
        print(f.line)
        print()
        print("Rule:")
        print(f.rule)
        print()
        print("Reason:")
        print(f.reason)
        print()
        print("Action:")
        print(f.action)
        print()


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Scan for secrets and operator leaks.")
    p.add_argument(
        "--all",
        action="store_true",
        help="Scan every tracked file (CI). Default is staged files only.",
    )
    p.add_argument(
        "--paths",
        nargs="+",
        help="Scan these paths instead of git (self-test).",
    )
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    if args.paths:
        paths = args.paths
        staged = False
    elif args.all:
        paths = tracked_paths()
        staged = False
    else:
        paths = staged_paths()
        staged = True
        if not paths:
            return 0

    findings: list[Finding] = []
    for path in paths:
        findings.extend(scan_path(path, staged=staged))

    if findings:
        print_findings(findings)
        return 1
    print(f"security_scan: pass ({len(paths)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
