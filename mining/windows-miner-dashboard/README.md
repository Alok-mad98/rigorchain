# RIGOR Miner Dashboard

Local visualization for the **Windows SOLO miner** account lifecycle.

- URL: `http://127.0.0.1:18083/`
- Mode: read-only, localhost bind
- Mining path: wallet → rigor-miner → local rigord GBT → submitblock
- Consensus: not modified

```bash
cd mining/windows-miner-dashboard
python3 server.py
```

Windows: run `installer/windows/start-miner-dashboard.cmd` or open the URL in a Windows browser after the server is up.

See `docs/mining/RIGOR_WINDOWS_MINER_DASHBOARD.md`.
