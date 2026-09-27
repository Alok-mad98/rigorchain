#!/usr/bin/env python3
"""RIGOR Miner Dashboard — localhost read-only visualization on :18083.

Does not mine, sign, or expose wallet secrets. Consensus is not modified.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from aggregator import (
    TITLE,
    DashboardConfig,
    SourceCache,
    convergence_view,
    health_view,
    miner_status_view,
    pool_view,
    res_view,
    snapshot,
    wallet_view,
    workers_view,
)

ROOT = Path(__file__).resolve().parent
PUBLIC = ROOT / "public"
MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
}

CFG = DashboardConfig()
CACHE = SourceCache()
_SNAP: dict | None = None
_SNAP_AT = 0.0
_SNAP_KEY = ""
TTL_SEC = 3.0
STREAM_SEC = 3.0


def requested_address(raw_path: str) -> str | None:
    qs = parse_qs(urlparse(raw_path).query)
    vals = qs.get("address") or qs.get("mining_address") or []
    if not vals:
        return None
    text = str(vals[0]).strip()
    return text or None


def current_snapshot(address: str | None = None) -> dict:
    global _SNAP, _SNAP_AT, _SNAP_KEY
    now = time.time()
    key = address or ""
    if _SNAP is None or now - _SNAP_AT >= TTL_SEC or _SNAP_KEY != key:
        _SNAP = snapshot(CFG, CACHE, address_override=address)
        _SNAP_AT = now
        _SNAP_KEY = key
    return _SNAP


def json_bytes(payload: dict) -> bytes:
    return json.dumps(payload, indent=2).encode("utf-8") + b"\n"


class Handler(BaseHTTPRequestHandler):
    server_version = "RigorMinerDashboard/1.0"

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; connect-src 'self'; img-src 'self' data:; "
            "style-src 'self'; script-src 'self'",
        )
        self.send_header("X-Dashboard-Mode", "read-only")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        self._send(405, b'{"error":"read-only"}\n', "application/json")

    def do_PUT(self) -> None:  # noqa: N802
        self._send(405, b'{"error":"read-only"}\n', "application/json")

    def do_DELETE(self) -> None:  # noqa: N802
        self._send(405, b'{"error":"read-only"}\n', "application/json")

    def do_GET(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            self._send_static(PUBLIC / "index.html")
            return
        if path == "/api/stream":
            self._stream(requested_address(self.path))
            return
        routes = {
            "/healthz": lambda snap: health_view(snap),
            "/api/miner/status": miner_status_view,
            "/api/miner/workers": workers_view,
            "/api/wallet/mining": wallet_view,
            "/api/oru/convergence": convergence_view,
            "/api/res/status": res_view,
            "/api/pool/status": pool_view,
            "/api/status": lambda snap: snap,
        }
        if path in routes:
            try:
                body = json_bytes(routes[path](current_snapshot(requested_address(self.path))))
                self._send(200, body, "application/json")
            except Exception as exc:  # pragma: no cover - defensive
                err = json_bytes({"ok": False, "error": "aggregator failure", "detail": str(exc)})
                self._send(500, err, "application/json")
            return
        if path.startswith("/api/"):
            self._send(404, b'{"error":"not found"}\n', "application/json")
            return
        rel = path.lstrip("/")
        target = (PUBLIC / rel).resolve()
        if PUBLIC.resolve() not in target.parents and target != PUBLIC.resolve():
            self._send(404, b'{"error":"not found"}\n', "application/json")
            return
        if target.is_file():
            self._send_static(target)
            return
        self._send(404, b'{"error":"not found"}\n', "application/json")

    def _stream(self, address: str | None = None) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Dashboard-Mode", "read-only")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; connect-src 'self'; img-src 'self' data:; "
            "style-src 'self'; script-src 'self'",
        )
        self.end_headers()
        try:
            while True:
                body = json.dumps(current_snapshot(address), separators=(",", ":"))
                chunk = f"data: {body}\n\n".encode("utf-8")
                self.wfile.write(chunk)
                self.wfile.flush()
                time.sleep(STREAM_SEC)
        except (BrokenPipeError, ConnectionResetError, TimeoutError, OSError):
            return

    def _send_static(self, path: Path) -> None:
        if not path.is_file():
            self._send(404, b"not found", "text/plain; charset=utf-8")
            return
        ctype = MIME.get(path.suffix.lower(), "application/octet-stream")
        self._send(200, path.read_bytes(), ctype)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="RIGOR Windows miner visualization dashboard")
    parser.add_argument("--dump", action="store_true", help="print one snapshot JSON and exit")
    parser.add_argument("--host", default=None, help="bind host (default 127.0.0.1)")
    parser.add_argument("--port", type=int, default=None, help="bind port (default 18083)")
    args = parser.parse_args(argv)
    if args.host:
        CFG.host = args.host
    if args.port:
        CFG.port = args.port
    if args.dump:
        sys.stdout.write(json.dumps(current_snapshot(), indent=2) + "\n")
        return 0
    if CFG.host not in ("127.0.0.1", "localhost", "::1"):
        print("refusing non-localhost bind; set DASHBOARD_HOST=127.0.0.1", file=sys.stderr)
        return 2
    httpd = ThreadingHTTPServer((CFG.host, CFG.port), Handler)
    print(
        f"{TITLE} http://{CFG.host}:{CFG.port}/  READ-ONLY localhost  SOLO GBT",
        flush=True,
    )
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
