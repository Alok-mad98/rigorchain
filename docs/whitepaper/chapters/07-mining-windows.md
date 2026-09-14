# Note — Mining (Windows Workflow)

Narrative Chapter 6 is now
[`06-mining.md`](./06-mining.md)
(Chinese: [`06-mining.zh.md`](./06-mining.zh.md)).
This file keeps the public Windows product path. It does not change
consensus.

# Chapter 7 — Mining (Windows Workflow)

Mining in the official public story is the **Windows miner workflow**.

This chapter does **not** document SOAP background mining, soak archives, or
“read the daemon logs” as the user path.

---

## What mining does

SHA3-256 (`RGRPOW01`) proof-of-work discovers valid headers and orders
blocks. ASERT adjusts difficulty so block time stays on the frozen target
spacing.

Mining **pays ORU**. Mining **does not mint RES**. A pool cannot allocate
RES by policy.

---

## Official user path

```
Windows user
    ↓
Download miner
    ↓
Connect wallet
    ↓
Connect pool
    ↓
Mine ORU
    ↓
Converge ORU
    ↓
Same tx mints RES at destroy_cost(h)
```

| Step | Action | Result |
|------|--------|--------|
| 1 | Download the Windows miner | Installer / package; verify checksum when published |
| 2 | Connect wallet | Start Windows wallet; set receive / payout address |
| 3 | Connect pool | Point the miner at stratum |
| 4 | Mine ORU | Shares → valid blocks → ORU coinbase (immature, then trusted) |
| 5 | Converge ORU | Wallet creates a convergence transaction (destroy `B` ORU) |
| 6 | Generate RES | The **same transaction** MAY mint Witness v1 RES at `destroy_cost(h)` when `R > 0 → R · (2²⁵ + h)² ≤ B << 40` |

---

## Windows scenario

1. Start Windows Wallet (`installer/windows/start-wallet.cmd`)
2. Start Windows Miner (`installer/windows/rigor-miner.cmd` or the pool miner agent)
3. Connect the miner to the pool
4. Observe shares and height
5. Observe ORU maturity (immature coinbase vs trusted ORU)
6. Converge ORU; the same transaction mints RES at `destroy_cost(h)` when authorized (no waiting / selected draw)
7. Observe RES
8. Verify a block hash independently (Zero Trust), not by trusting the pool UI

---

## Sentences that must remain true

- Miners mine **ORU**.
- RES is **not mined**.
- **No Convergence → No RES.**
- Pool policy cannot mint RES.
- A found block is checked with public data, not with a dashboard opinion.
