"""Read-only RIGOR miner dashboard aggregator.

Visualizes the Windows SOLO miner account lifecycle. Does not mine, sign, or
change consensus. Never returns RPC secrets, wallet seeds, or private keys.
"""
from __future__ import annotations

import json
import os
import re
import socket
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.error import URLError
from urllib.request import Request, urlopen

SECRET_KEY_RE = re.compile(
    r"(rpcuser|rpcpassword|rpc_user|rpc_password|rpcauth|password|passwd|"
    r"passphrase|walletpass|mnemonic|seed|wif|privkey|private_key|"
    r"authorization|cookie|secret)",
    re.IGNORECASE,
)
WORKER_RE = re.compile(r"worker=([^\s]+)")
STATUS_LINE_RE = re.compile(r"status worker=")
RG1_RE = re.compile(r"^rg1q[0-9a-z]{20,}$", re.IGNORECASE)
RG1P_RE = re.compile(r"^rg1p", re.IGNORECASE)

COINBASE_MATURITY_DISPLAY = 100
DEFAULT_EXPLORER = "http://127.0.0.1:18081"
DEFAULT_OBS = "http://127.0.0.1:18082"
DEFAULT_STRATUM_HOST = "127.0.0.1"
DEFAULT_STRATUM_PORT = 13333
DEFAULT_NODE_RPC_HOST = "127.0.0.1"
DEFAULT_NODE_RPC_PORT = 8761
PRODUCT = "RIGOR v0.1.3"
TITLE = "RIGOR Miner Dashboard"
POW_DIGEST = 'SHA3-256("RGRPOW01" || header_80)'
MINING_PATH = "SOLO GBT"
MINER_PROCESS_NAMES = ("minerd", "rigor-miner", "rigor-miner.exe")
MINER_STATUS_STALE_SEC = 30
DEFAULT_EXPLORER = "http://127.0.0.1:18081"
DEFAULT_OBS = "http://127.0.0.1:18082"
DEFAULT_STRATUM_HOST = "127.0.0.1"
DEFAULT_STRATUM_PORT = 13333
DEFAULT_NODE_RPC_HOST = "127.0.0.1"
DEFAULT_NODE_RPC_PORT = 8761
PRODUCT = "RIGOR v0.1.3"
TITLE = "RIGOR Miner Dashboard"
POW_DIGEST = 'SHA3-256("RGRPOW01" || header_80)'
MINING_PATH = "SOLO GBT"
MINER_PROCESS_NAMES = ("minerd", "rigor-miner", "rigor-miner.exe")


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def env(name: str, default: str | None = None) -> str | None:
    value = os.environ.get(name)
    if value is None or not str(value).strip():
        return default
    return str(value).strip()


def tcp_ok(host: str, port: int, timeout: float = 1.5) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def http_json(url: str, timeout: float = 4.0) -> tuple[Any, str | None]:
    try:
        req = Request(url, headers={"Accept": "application/json"})
        with urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
        if not raw.strip():
            return None, "empty response"
        return json.loads(raw), None
    except (URLError, TimeoutError, OSError, json.JSONDecodeError, ValueError) as exc:
        return None, str(exc)


def strip_secrets(value: Any) -> Any:
    """Drop secret-bearing keys. Never raise on unexpected shapes."""
    if isinstance(value, dict):
        out = {}
        for key, item in value.items():
            if SECRET_KEY_RE.search(str(key)):
                continue
            out[str(key)] = strip_secrets(item)
        return out
    if isinstance(value, list):
        return [strip_secrets(item) for item in value]
    if isinstance(value, str) and SECRET_KEY_RE.search(value) and "=" in value:
        return "[redacted]"
    return value


def fmt_amount(value: Any, unit: str, digits: int) -> str:
    if value is None or value == "":
        return "—"
    try:
        number = float(value)
    except (TypeError, ValueError):
        text = str(value).strip()
        return text if text else "—"
    if number.is_integer():
        return f"{int(number)} {unit}" if digits == 0 else f"{number:.{digits}f} {unit}"
    return f"{number:.{digits}f} {unit}"


def fmt_hashrate(value: Any) -> str:
    if value is None or value == "":
        return "—"
    try:
        hs = float(str(value).replace("H/s", "").strip())
    except (TypeError, ValueError):
        return str(value)
    if hs < 0:
        return "—"
    if hs >= 1_000_000:
        return f"{hs / 1_000_000:.2f} MH/s"
    if hs >= 1_000:
        return f"{hs / 1_000:.2f} kH/s"
    return f"{hs:.1f} H/s"


def fmt_duration(seconds: Any) -> str:
    try:
        total = int(float(str(seconds).rstrip("s")))
    except (TypeError, ValueError):
        text = str(seconds or "").strip()
        return text if text else "—"
    if total < 0:
        return "—"
    hours, rem = divmod(total, 3600)
    minutes, secs = divmod(rem, 60)
    if hours:
        return f"{hours}h {minutes:02d}m"
    if minutes:
        return f"{minutes}m {secs:02d}s"
    return f"{secs}s"


def fmt_share_time(ts: Any) -> str:
    try:
        epoch = int(ts)
        return datetime.fromtimestamp(epoch, timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    except (TypeError, ValueError, OSError):
        return "—"


def connected_label(ok: bool) -> str:
    return "CONNECTED" if ok else "DISCONNECTED"


def online_label(ok: bool) -> str:
    return "ONLINE" if ok else "OFFLINE"


def process_named(names: tuple[str, ...]) -> bool:
    """Best-effort local process scan. Never raises; never reads process env."""
    proc = Path("/proc")
    if not proc.is_dir():
        return False
    wanted = tuple(n.lower() for n in names)
    try:
        pid_dirs = list(proc.iterdir())
    except OSError:
        return False
    for pid_dir in pid_dirs:
        if not pid_dir.name.isdigit():
            continue
        try:
            comm = (pid_dir / "comm").read_text(encoding="utf-8", errors="replace").strip().lower()
            cmdline = (
                (pid_dir / "cmdline")
                .read_bytes()
                .replace(b"\x00", b" ")
                .decode("utf-8", errors="replace")
                .lower()
            )
        except OSError:
            continue
        for name in wanted:
            if name == comm or name in cmdline:
                return True
    return False


def normalize_mining_address(value: str | None) -> tuple[str | None, str | None]:
    """Return (rg1q address, error). RES rg1p addresses are rejected."""
    if value is None or not str(value).strip():
        return None, None
    addr = str(value).strip()
    if RG1P_RE.match(addr):
        return None, "RES address (rg1p…) is not a SOLO payout. Use an ORU rg1q… address."
    if RG1_RE.match(addr):
        return addr, None
    return None, "Not an ORU payout address. Paste a rg1q… address."


def discover_wallet_datadirs() -> list[Path]:
    """Windows miner wallet locations only. Do not inherit a Linux soak ~/.rigor."""
    found: list[Path] = []
    appdata = env("APPDATA")
    if appdata:
        found.append(Path(appdata) / "RIGOR")
    users = Path("/mnt/c/Users")
    if users.is_dir():
        try:
            names = list(users.iterdir())
        except OSError:
            names = []
        for user in names:
            if user.name in ("Public", "Default", "Default User", "All Users"):
                continue
            found.append(user / "AppData" / "Roaming" / "RIGOR")
    out: list[Path] = []
    seen: set[str] = set()
    for path in found:
        key = str(path)
        if key in seen:
            continue
        seen.add(key)
        if path.is_dir():
            out.append(path)
    return out


def datadir_looks_like_wallet(path: Path) -> bool:
    return any(
        (path / name).exists()
        for name in (".cookie", "rpc-auth.conf", "wallet.dat", "chainstate", "rigor.conf")
    )


def discover_wallet_datadir() -> Path | None:
    configured = env("WALLET_DATADIR")
    if configured:
        path = Path(configured)
        return path if path.exists() else None
    for path in discover_wallet_datadirs():
        if datadir_looks_like_wallet(path):
            return path
    return None


def discover_rigor_cli() -> str | None:
    configured = env("WALLET_CLI")
    if configured:
        return configured
    here = Path(__file__).resolve().parent
    candidates = [
        here.parent / "rigor-cli.exe",
        here.parent.parent / "rigor-cli.exe",
        Path.cwd() / "rigor-cli.exe",
        here.parent / "rigor-cli",
        Path("/usr/local/bin/rigor-cli"),
        Path("/usr/bin/rigor-cli"),
    ]
    for path in candidates:
        if path.is_file():
            return str(path)
    return None


def rpc_auth_from_datadir(datadir: str | None) -> tuple[str | None, str | None]:
    """Load rpcuser/rpcpassword for CLI only. Never returned in dashboard JSON."""
    if not datadir:
        return None, None
    path = Path(datadir) / "rpc-auth.conf"
    if not path.is_file():
        return None, None
    user = password = None
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None, None
    for line in lines:
        raw = line.strip()
        if raw.lower().startswith("rpcuser="):
            user = raw.split("=", 1)[1].strip()
        elif raw.lower().startswith("rpcpassword="):
            password = raw.split("=", 1)[1].strip()
    return user, password


def load_miner_status(candidates: list[Path]) -> tuple[dict[str, Any] | None, bool]:
    path = next((p for p in candidates if p.exists()), None)
    if path is None:
        return None, False
    try:
        age = time.time() - path.stat().st_mtime
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except (OSError, json.JSONDecodeError, TypeError, ValueError):
        return None, False
    if not isinstance(data, dict):
        return None, False
    live = age <= MINER_STATUS_STALE_SEC
    data["_stale"] = not live
    return data, live


class DashboardConfig:
    def __init__(self) -> None:
        self.host = env("DASHBOARD_HOST", "127.0.0.1") or "127.0.0.1"
        self.port = int(env("DASHBOARD_PORT", "18083") or "18083")
        self.explorer_url = (env("EXPLORER_URL", DEFAULT_EXPLORER) or DEFAULT_EXPLORER).rstrip("/")
        self.obs_url = (env("OBS_URL", DEFAULT_OBS) or DEFAULT_OBS).rstrip("/")
        self.stratum_host = env("POOL_STRATUM_HOST", DEFAULT_STRATUM_HOST) or DEFAULT_STRATUM_HOST
        self.stratum_port = int(env("POOL_STRATUM_PORT", str(DEFAULT_STRATUM_PORT)) or DEFAULT_STRATUM_PORT)
        self.node_rpc_host = env("NODE_RPC_HOST", DEFAULT_NODE_RPC_HOST) or DEFAULT_NODE_RPC_HOST
        self.node_rpc_port = int(env("NODE_RPC_PORT", str(DEFAULT_NODE_RPC_PORT)) or DEFAULT_NODE_RPC_PORT)
        self.platform = env("MINER_PLATFORM", "Windows") or "Windows"
        self.mining_address = env("MINING_ADDRESS")
        # Wallet RPC: WALLET_* wins. Otherwise discover Windows %APPDATA%\RIGOR only.
        self.wallet_cli = env("WALLET_CLI") or discover_rigor_cli()
        discovered_datadir = discover_wallet_datadir()
        self.wallet_datadir = env("WALLET_DATADIR") or (str(discovered_datadir) if discovered_datadir else None)
        self.wallet_rpc_port = env("WALLET_RPC_PORT")
        self.wallet_name = env("WALLET_NAME")
        self.wallet_rpc_user = env("WALLET_RPC_USER")
        self.wallet_rpc_password = env("WALLET_RPC_PASSWORD")
        self.pool_status_file = env("POOL_STATUS_FILE")
        self.miner_log = env("MINER_LOG")
        self.miner_status_file = env("MINER_STATUS_FILE") or env("RIGOR_MINER_STATUS_FILE")
        home = Path.home()
        tree = Path(__file__).resolve().parents[2]
        appdata = env("APPDATA")
        self.pool_status_candidates = [
            Path(p)
            for p in [
                self.pool_status_file,
                str(home / "alphaplus-validation/pool/status.json"),
                str(Path.cwd() / ".runtime/pool-pilot/status.json"),
                str(tree / ".runtime/pool-pilot/status.json"),
            ]
            if p
        ]
        self.miner_log_candidates = [
            Path(p)
            for p in [
                self.miner_log,
                str(home / "alphaplus-validation/pool/logs/miner-agent.log"),
                str(tree / ".runtime/miner/minerd.log"),
                str(Path.cwd() / ".runtime/miner/minerd.log"),
            ]
            if p
        ]
        self.miner_status_candidates = [
            Path(p)
            for p in [
                self.miner_status_file,
                str(Path(appdata) / "RIGOR" / "miner-status.json") if appdata else "",
                str(home / ".rigor" / "miner-status.json"),
                str(tree / ".runtime" / "miner" / "miner-status.json"),
                str(Path.cwd() / "miner-status.json"),
            ]
            if p
        ]
        try:
            user_root = Path("/mnt/c/Users")
            user_iter = list(user_root.iterdir()) if user_root.is_dir() else []
        except OSError:
            user_iter = []
        for user_dir in user_iter:
            if user_dir.name in ("Public", "Default", "Default User", "All Users"):
                continue
            self.miner_status_candidates.append(user_dir / "AppData" / "Roaming" / "RIGOR" / "miner-status.json")


class SourceCache:
    def __init__(self) -> None:
        self._pool: tuple[float, dict[str, Any]] | None = None
        self._pool_path: Path | None = None

    def load_pool_status(self, candidates: list[Path]) -> tuple[dict[str, Any] | None, str | None]:
        path = next((p for p in candidates if p.exists()), None)
        if path is None:
            return None, "pool status.json not found"
        mtime = path.stat().st_mtime
        if self._pool and self._pool_path == path and self._pool[0] == mtime:
            return self._pool[1], None
        try:
            data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
        except (OSError, json.JSONDecodeError) as exc:
            return None, str(exc)
        if not isinstance(data, dict):
            return None, "pool status.json is not an object"
        self._pool = (mtime, data)
        self._pool_path = path
        return data, None


def first_existing(paths: list[Path]) -> Path | None:
    return next((p for p in paths if p.exists()), None)


def parse_miner_log(path: Path | None) -> dict[str, Any] | None:
    if path is None or not path.exists():
        return None
    try:
        size = path.stat().st_size
        with path.open("rb") as handle:
            handle.seek(max(0, size - 16384))
            chunk = handle.read().decode("utf-8", errors="replace")
    except OSError:
        return None
    last = None
    for line in chunk.splitlines():
        if STATUS_LINE_RE.search(line):
            last = line.strip()
    if not last:
        return None
    rec: dict[str, Any] = {"raw": last}
    for tok in last.split():
        if "=" in tok:
            key, value = tok.split("=", 1)
            rec[key] = value
    return rec


def worker_stats_from_audit(events: Any) -> dict[str, dict[str, Any]]:
    workers: dict[str, dict[str, Any]] = {}
    if not isinstance(events, list):
        return workers
    for event in events:
        if not isinstance(event, dict):
            continue
        message = str(event.get("message") or "")
        match = WORKER_RE.search(message)
        if not match:
            continue
        name = match.group(1)
        row = workers.setdefault(
            name,
            {
                "worker": name,
                "accepted": 0,
                "rejected": 0,
                "last_share_ts": None,
                "authorized": False,
            },
        )
        category = str(event.get("category") or "")
        if category == "auth.ok":
            row["authorized"] = True
        elif category == "share.accepted":
            row["accepted"] += 1
            row["last_share_ts"] = event.get("timestamp")
        elif category in ("share.rejected", "share.stale"):
            row["rejected"] += 1
            row["last_share_ts"] = event.get("timestamp")
    return workers


def wallet_cli_json(cfg: DashboardConfig, method: str, extra: list[str] | None = None) -> tuple[Any, str | None]:
    if not cfg.wallet_cli:
        return None, "WALLET_CLI not configured"
    args = [cfg.wallet_cli]
    if cfg.wallet_datadir:
        args.append(f"-datadir={cfg.wallet_datadir}")
    if cfg.wallet_rpc_port:
        args.append(f"-rpcport={cfg.wallet_rpc_port}")
    if cfg.wallet_rpc_user:
        args.append(f"-rpcuser={cfg.wallet_rpc_user}")
    if cfg.wallet_rpc_password:
        args.append(f"-rpcpassword={cfg.wallet_rpc_password}")
    if not cfg.wallet_rpc_user and not cfg.wallet_rpc_password:
        auth_user, auth_pass = rpc_auth_from_datadir(cfg.wallet_datadir)
        if auth_user:
            args.append(f"-rpcuser={auth_user}")
        if auth_pass:
            args.append(f"-rpcpassword={auth_pass}")
    if cfg.wallet_name:
        args.append(f"-rpcwallet={cfg.wallet_name}")
    args.append(method)
    if extra:
        args.extend(extra)
    try:
        proc = subprocess.run(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=12,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return None, str(exc)
    text = (proc.stdout or "").replace("\r", "")
    if proc.returncode != 0:
        return None, text.strip()[:240] or f"exit {proc.returncode}"
    start = min([i for i in (text.find("{"), text.find("[")) if i >= 0], default=-1)
    if start < 0:
        return None, "no JSON in wallet RPC output"
    try:
        return json.loads(text[start:]), None
    except json.JSONDecodeError as exc:
        return None, str(exc)


def pick_mining_address(cfg: DashboardConfig, received: Any, override: str | None = None) -> tuple[str | None, str | None]:
    addr, err = normalize_mining_address(override)
    if err:
        return None, err
    if addr:
        return addr, None
    addr, err = normalize_mining_address(cfg.mining_address)
    if err:
        return None, err
    if addr:
        return addr, None
    if isinstance(received, list):
        for row in received:
            if not isinstance(row, dict):
                continue
            found, _ = normalize_mining_address(str(row.get("address") or ""))
            if found:
                return found, None
    return None, None


def contribution_counts(events: Any) -> dict[str, int]:
    pending = eligible = selected = settled = 0
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict):
                continue
            minted = bool(event.get("mint_height")) or str(event.get("result") or "").lower().startswith("minted")
            awarded = bool(event.get("award_height")) or bool(event.get("is_winner"))
            is_eligible = bool(event.get("draw_eligible") or event.get("ever_eligible"))
            if minted:
                settled += 1
            elif awarded:
                selected += 1
            elif is_eligible:
                eligible += 1
            else:
                pending += 1
    return {
        "pending": pending,
        "eligible": eligible,
        "selected": selected,
        "settled": settled,
    }


def next_action(node_ok: bool, miner_ok: bool) -> str:
    """Operator hint for the miner path. Wallet GUI is not required to mine."""
    if not node_ok:
        return "Start local rigord so GBT is available on 127.0.0.1:8761"
    if not miner_ok:
        return "Start Windows Miner (installer/windows/rigor-miner.cmd)"
    return "SOLO mining is live. Watch height, ORU maturity, and Convergence."


def mining_status_label(node_ok: bool, miner_ok: bool) -> str:
    if miner_ok and node_ok:
        return "MINING"
    if node_ok:
        return "WAITING"
    return "DISCONNECTED"


def snapshot(cfg: DashboardConfig, cache: SourceCache, address_override: str | None = None) -> dict[str, Any]:
    explorer_ok = tcp_ok("127.0.0.1", 18081)
    obs_ok = tcp_ok("127.0.0.1", 18082)
    node_ok = tcp_ok(cfg.node_rpc_host, cfg.node_rpc_port)
    stratum_ok = tcp_ok(cfg.stratum_host, cfg.stratum_port)
    explorer_status, explorer_err = http_json(f"{cfg.explorer_url}/api/status")
    explorer_health, _ = http_json(f"{cfg.explorer_url}/healthz")
    res_events_payload, _ = http_json(f"{cfg.explorer_url}/api/res/events?limit=50")
    res_mints_payload, _ = http_json(f"{cfg.explorer_url}/api/res/mints?limit=10")
    obs_status, _ = http_json(f"{cfg.obs_url}/api/status") if obs_ok else (None, "obs unreachable")
    obs_pool, _ = http_json(f"{cfg.obs_url}/api/pool") if obs_ok else (None, "obs unreachable")

    pool_raw, pool_err = cache.load_pool_status(cfg.pool_status_candidates)
    miner_log = parse_miner_log(first_existing(cfg.miner_log_candidates))
    miner_proc = process_named(MINER_PROCESS_NAMES)
    miner_status_file, miner_status_live = load_miner_status(cfg.miner_status_candidates)
    metrics = (pool_raw or {}).get("metrics") if isinstance(pool_raw, dict) else {}
    if not isinstance(metrics, dict):
        metrics = {}
    audit = (pool_raw or {}).get("audit_events") if isinstance(pool_raw, dict) else []
    workers_from_audit = worker_stats_from_audit(audit)

    obs = obs_status if isinstance(obs_status, dict) else {}
    obs_pool_d = obs_pool if isinstance(obs_pool, dict) else {}
    exp = explorer_status if isinstance(explorer_status, dict) else {}
    block_health = exp.get("block_health") if isinstance(exp.get("block_health"), dict) else {}
    eco = exp.get("economic_snapshot") if isinstance(exp.get("economic_snapshot"), dict) else {}
    conv = exp.get("convergence_health") if isinstance(exp.get("convergence_health"), dict) else {}
    mining_activity = exp.get("mining_activity") if isinstance(exp.get("mining_activity"), dict) else {}
    chain_identity = exp.get("chain_identity") if isinstance(exp.get("chain_identity"), dict) else {}
    obs_explorer = obs.get("explorer") if isinstance(obs.get("explorer"), dict) else {}
    if not eco:
        eco = {
            "res_minted_total_display": obs_explorer.get("res_minted_total_display"),
            "network_res_minted_display": obs_explorer.get("res_minted_total_display"),
            "oru_immature_coinbase_display": ((obs_explorer.get("coinbase_summary") or {}) if isinstance(obs_explorer.get("coinbase_summary"), dict) else {}).get("amount_display"),
        }

    height = block_health.get("current_height_node") or block_health.get("current_height_indexer")
    if height is None and isinstance(obs.get("linux_a"), dict):
        height = obs["linux_a"].get("height")

    miners_connected = (pool_raw or {}).get("miners_connected")
    if miners_connected is None:
        miners_connected = obs_pool_d.get("miners_connected") or obs_pool_d.get("workers") or 0
    try:
        miners_connected_n = int(miners_connected or 0)
    except (TypeError, ValueError):
        miners_connected_n = 0

    miner_alive = miner_proc or miner_status_live
    if miner_status_file and miner_status_file.get("mining") is True and miner_status_live:
        miner_alive = True

    accepted = None
    rejected = None
    blocks_submitted = None
    if miner_status_live and miner_status_file:
        accepted = miner_status_file.get("blocks_accepted")
        rejected = miner_status_file.get("blocks_rejected")
        try:
            blocks_submitted = int(accepted or 0) + int(rejected or 0)
        except (TypeError, ValueError):
            blocks_submitted = accepted

    agent = miner_log if miner_alive else None
    worker_name = "rigor-miner" if miner_alive else "—"
    if miner_status_live and miner_status_file and miner_status_file.get("payout"):
        worker_name = "rigor-miner"

    submitted = accepted
    hashrate = None
    mining_time = "—"
    last_share = "—"
    last_submit = "—"
    gbt_height = None
    if miner_status_live and miner_status_file:
        hashrate = miner_status_file.get("hashrate_hs")
        if hashrate in (None, ""):
            hashrate = miner_status_file.get("hashrate")
        last_submit = str(miner_status_file.get("last_submit") or "—")
        gbt_height = miner_status_file.get("gbt_height")
        last_share = last_submit
    if gbt_height not in (None, "", 0, "0") and height is None:
        height = gbt_height

    wallet_balances, wallet_err = wallet_cli_json(cfg, "getbalances")
    received, _ = wallet_cli_json(cfg, "listreceivedbyaddress", ["0", "true"])
    status_payout = None
    if miner_status_file:
        status_payout, _ = normalize_mining_address(str(miner_status_file.get("payout") or ""))
    address, address_error = pick_mining_address(cfg, received, address_override or status_payout)
    addr_intel = None
    if address:
        addr_intel, _ = http_json(f"{cfg.explorer_url}/api/address/{address}")
        if not isinstance(addr_intel, dict):
            addr_intel = None
    windows_wallet = None
    if isinstance(obs.get("wallets"), dict):
        windows_wallet = obs["wallets"].get("windows_valwallet")

    wallet_source = "unavailable"
    oru = {}
    res = {}
    if isinstance(wallet_balances, dict) and isinstance(wallet_balances.get("oru"), dict):
        oru = wallet_balances.get("oru") or {}
        res = wallet_balances.get("res") or {}
        wallet_source = "wallet_rpc"
    elif isinstance(windows_wallet, dict) and isinstance(windows_wallet.get("oru"), dict):
        oru = windows_wallet.get("oru") or {}
        res = windows_wallet.get("res") or {}
        wallet_source = "windows_wallet"
    elif addr_intel:
        balances = addr_intel.get("balances") if isinstance(addr_intel.get("balances"), dict) else {}
        oru_bal = balances.get("oru") if isinstance(balances.get("oru"), dict) else {}
        res_bal = balances.get("res") if isinstance(balances.get("res"), dict) else {}
        trusted = oru_bal.get("display")
        current_res_display = res_bal.get("display")
        oru = {"trusted": trusted, "immature": None}
        res = {"trusted": current_res_display, "immature": 0}
        wallet_source = "explorer_address"
        tip = None
        try:
            tip = int(height) if height not in (None, "—") else None
        except (TypeError, ValueError):
            tip = None
        immature_atoms = 0
        utxos = addr_intel.get("oru_utxos") if isinstance(addr_intel.get("oru_utxos"), list) else []
        if tip is not None:
            for utxo in utxos:
                if not isinstance(utxo, dict) or utxo.get("spent"):
                    continue
                try:
                    uheight = int(utxo.get("height"))
                    atoms = int(utxo.get("amount_atoms") or 0)
                except (TypeError, ValueError):
                    continue
                if tip - uheight + 1 < COINBASE_MATURITY_DISPLAY:
                    immature_atoms += atoms
        if immature_atoms:
            oru["immature"] = immature_atoms / 100_000_000
    wallet_connected = wallet_source != "unavailable"

    immature = oru.get("immature") if isinstance(oru, dict) else None
    trusted = oru.get("trusted") if isinstance(oru, dict) else None
    spendable = "NO"
    try:
        spendable_n = float(str(trusted or 0).split()[0])
        spendable = "YES" if spendable_n > 0 else "NO"
    except (TypeError, ValueError):
        spendable = "NO"

    events = []
    if isinstance(res_events_payload, dict) and isinstance(res_events_payload.get("events"), list):
        events = res_events_payload["events"]
    contrib = contribution_counts(events)
    last_winner = eco.get("last_winner_txid") or conv.get("last_winner_txid")
    winner_present = bool(last_winner) and str(last_winner) not in ("", "0" * 64, "-", "None")
    conv_status = "WINNER" if winner_present else "WAITING"

    current_res = res.get("trusted") if isinstance(res, dict) else None
    pending_res = res.get("immature") if isinstance(res, dict) else None
    if current_res in (None, "") and not address and eco.get("res_minted_total_display"):
        current_res = eco.get("network_res_minted_display") or eco.get("res_minted_total_display")
    if pending_res in (None, ""):
        pending_res = 0
    mints = []
    if isinstance(res_mints_payload, dict) and isinstance(res_mints_payload.get("mints"), list):
        mints = res_mints_payload["mints"]
    last_mint = mints[0] if mints else None
    event_count = eco.get("convergence_event_count")
    if event_count is None:
        event_count = len(events)

    node_label = connected_label(node_ok or explorer_ok or height is not None)
    miner_status = mining_status_label(node_ok or explorer_ok, miner_alive)
    submitted_n = 0
    try:
        submitted_n = int(blocks_submitted or 0)
    except (TypeError, ValueError):
        submitted_n = 0
    submit_status = "ACTIVE" if miner_alive and str(last_submit).lower() in ("accepted", "ok") else "WAITING"
    last_hashrate = fmt_hashrate(hashrate) if not isinstance(hashrate, str) else str(hashrate)
    if isinstance(hashrate, str) and hashrate not in ("", "—"):
        last_hashrate = hashrate
    hashrate_display = last_hashrate if miner_alive else "—"
    worker_path = MINING_PATH
    product_label = str(chain_identity.get("product") or PRODUCT)
    pool_visible = bool(stratum_ok)

    lifecycle = [
        {"id": "user", "label": "Windows User", "status": "ACTIVE", "detail": cfg.platform},
        {
            "id": "wallet",
            "label": "Wallet",
            "status": connected_label(wallet_connected),
            "detail": address or "rg1q… payout",
        },
        {
            "id": "node",
            "label": "Local rigord",
            "status": node_label,
            "detail": f"{cfg.node_rpc_host}:{cfg.node_rpc_port}",
        },
        {
            "id": "miner",
            "label": "rigor-miner",
            "status": online_label(miner_alive),
            "detail": str(worker_name),
        },
        {
            "id": "submit",
            "label": "submitblock",
            "status": submit_status,
            "detail": f"GBT {gbt_height if gbt_height not in (None, '') else '—'} · {last_submit}",
        },
        {
            "id": "maturity",
            "label": "ORU Maturity",
            "status": "MATURING" if (immature not in (None, "", 0, 0.0, "0")) else "READY" if spendable == "YES" else "WAITING",
            "detail": f"immature {fmt_amount(immature, 'ORU', 8)}",
        },
        {
            "id": "convergence",
            "label": "Convergence",
            "status": conv_status,
            "detail": "display only",
        },
        {
            "id": "res",
            "label": "RES Result",
            "status": "VISIBLE" if current_res not in (None, "", 0, 0.0, "0") else "WAITING",
            "detail": fmt_amount(current_res, "RES", 2) if current_res not in (None, "") else "not mined",
        },
    ]

    scenario = {
        "start_windows_wallet": wallet_connected,
        "start_local_node": node_ok or explorer_ok or height is not None,
        "start_windows_miner": miner_alive,
        "connect_miner_gbt": node_ok or miner_alive,
        "observe_height": height not in (None, 0, "0"),
        "observe_oru_maturity": immature is not None or trusted is not None or eco.get("oru_immature_coinbase_display") is not None,
        "observe_wallet_balance": wallet_connected or address is not None,
        "observe_convergence": True,
        "observe_res_result": True,
        "verify_independently": explorer_ok,
    }

    payload = {
        "generated_at": utc_now(),
        "product": product_label,
        "title": TITLE,
        "persona": "I am a Windows miner. What is happening to my mining account?",
        "mining_path": MINING_PATH,
        "algorithm": POW_DIGEST,
        "read_only": True,
        "bind": f"{cfg.host}:{cfg.port}",
        "next_action": next_action(node_ok or explorer_ok, miner_alive),
        "address_error": address_error,
        "payout_address": address,
        "links": {
            "explorer": "http://127.0.0.1:18081",
            "observability": "http://127.0.0.1:18082",
            "dashboard": f"http://127.0.0.1:{cfg.port}/",
        },
        "lifecycle": lifecycle,
        "scenario": scenario,
        "miner": {
            "wallet": address or "rg1xxxxxxxx",
            "mining_status": miner_status,
            "mining_path": MINING_PATH,
            "algorithm": POW_DIGEST,
            "miner": online_label(miner_alive),
            "platform": cfg.platform,
            "node": node_label,
            "pool": connected_label(stratum_ok),
            "current_height": height if height is not None else "—",
            "mining_time": mining_time,
            "hashrate": hashrate_display,
            "gbt_height": gbt_height if gbt_height not in (None, "") else "—",
            "last_submit": last_submit,
            "rpc": f"{cfg.node_rpc_host}:{cfg.node_rpc_port}",
            "stratum": f"{cfg.stratum_host}:{cfg.stratum_port}",
            "difficulty": block_health.get("difficulty"),
            "best_block_hash": block_health.get("best_block_hash"),
        },
        "workers": [
            {
                "worker": worker_name if worker_name != "—" else "rigor-miner",
                "status": "Online" if miner_alive else "Offline",
                "os": cfg.platform,
                "hashrate": last_hashrate,
                "shares_submitted": submitted if submitted is not None else (int(accepted or 0) + int(rejected or 0)),
                "accepted": accepted if accepted is not None else 0,
                "rejected": rejected if rejected is not None else 0,
                "submitted_blocks": submitted_n,
                "last_share": last_share,
                "path": worker_path,
            }
        ],
        "wallet": {
            "address": address or "rg1xxxxxxxx",
            "source": wallet_source,
            "immature_oru": fmt_amount(immature, "ORU", 8) if immature is not None else "—",
            "trusted_oru": fmt_amount(trusted, "ORU", 8) if trusted is not None else "—",
            "spendable": spendable if (wallet_connected or address) else "NO",
            "immature_coinbase_note": "Immature Coinbase is not spendable until maturity.",
            "trusted_oru_note": "Trusted ORU is the spendable mining balance.",
            "coinbase_maturity_display": COINBASE_MATURITY_DISPLAY,
            "error": address_error or (None if wallet_connected or address else (wallet_err or "Windows wallet not connected")),
        },
        "convergence": {
            "status": conv_status,
            "mining_contribution": contrib,
            "pending": contrib["pending"],
            "eligible": contrib["eligible"],
            "selected": contrib["selected"],
            "settled": contrib["settled"],
            "active_convergences": conv.get("active_convergences"),
            "pending_windows": conv.get("pending_windows"),
            "last_winner": last_winner if winner_present else "-",
            "note": "No Convergence → No RES. Display only. Consensus draw logic is not modified.",
        },
        "res": {
            "current_res": fmt_amount(current_res, "RES", 2) if isinstance(current_res, (int, float)) else (current_res or "0"),
            "pending": fmt_amount(pending_res, "RES", 2) if isinstance(pending_res, (int, float)) else (pending_res or "0"),
            "last_winner": last_winner if winner_present else "-",
            "convergence_events": event_count if event_count is not None else "-",
            "last_mint_height": (last_mint or {}).get("block_height") if isinstance(last_mint, dict) else None,
            "network_res_minted_display": eco.get("network_res_minted_display") or eco.get("res_minted_total_display") or "0",
            "note": "RES is not mined. RES appears after Convergence settlement. The pool does not pay RES.",
        },
        "pool": {
            "connected_miners": miners_connected_n,
            "accepted_shares": accepted if accepted is not None else 0,
            "rejected_shares": rejected if rejected is not None else 0,
            "block_candidates": (pool_raw or {}).get("active_jobs") or obs_pool_d.get("active_jobs") or mining_activity.get("pending_blocks_active_jobs") or 0,
            "submitted_blocks": submitted_n,
            "stratum": f"{cfg.stratum_host}:{cfg.stratum_port}",
            "stratum_ok": stratum_ok,
            "secondary": True,
            "quarantined": True,
            "visible": pool_visible,
            "note": "Quarantined observer. Not the SOLO mining path and not a consensus authority.",
        },
        "sources": {
            "wallet": wallet_connected,
            "node": node_ok,
            "explorer": explorer_ok,
            "miner": miner_alive,
            "miner_status_file": bool(miner_status_live),
            "observability": obs_ok,
            "pool_status": pool_visible,
            "miner_log": miner_log is not None,
            "wallet_rpc": wallet_source == "wallet_rpc",
            "explorer_error": explorer_err,
            "pool_error": pool_err,
            "explorer_health": bool(explorer_health),
        },
        "metrics": {
            "hashrate": hashrate_display,
            "height": height if height is not None else "—",
            "trusted_oru": fmt_amount(trusted, "ORU", 8) if trusted is not None else "—",
            "immature_oru": fmt_amount(immature, "ORU", 8) if immature is not None else "—",
            "convergence": conv_status,
            "mining_status": miner_status,
            "gbt_height": gbt_height if gbt_height not in (None, "") else "—",
            "last_submit": last_submit,
        },
    }
    return strip_secrets(payload)


def miner_status_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {
        "generated_at": snap.get("generated_at"),
        "product": snap.get("product"),
        "title": snap.get("title"),
        "next_action": snap.get("next_action"),
        "payout_address": snap.get("payout_address"),
        "address_error": snap.get("address_error"),
        "metrics": snap.get("metrics"),
        "sources": snap.get("sources"),
        **(snap.get("miner") or {}),
        "lifecycle": snap.get("lifecycle"),
        "scenario": snap.get("scenario"),
        "links": snap.get("links"),
    }


def workers_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {"generated_at": snap.get("generated_at"), "workers": snap.get("workers") or []}


def wallet_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {"generated_at": snap.get("generated_at"), **(snap.get("wallet") or {})}


def convergence_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {"generated_at": snap.get("generated_at"), **(snap.get("convergence") or {})}


def res_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {"generated_at": snap.get("generated_at"), **(snap.get("res") or {})}


def pool_view(snap: dict[str, Any]) -> dict[str, Any]:
    return {"generated_at": snap.get("generated_at"), **(snap.get("pool") or {})}


def health_view(snap: dict[str, Any]) -> dict[str, Any]:
    miner = snap.get("miner") or {}
    sources = snap.get("sources") or {}
    return {
        "ok": True,
        "read_only": True,
        "localhost": True,
        "product": snap.get("product"),
        "mining_path": snap.get("mining_path"),
        "generated_at": snap.get("generated_at"),
        "height": miner.get("current_height"),
        "miner": miner.get("miner"),
        "node": miner.get("node"),
        "pool": miner.get("pool"),
        "sources": sources,
    }
