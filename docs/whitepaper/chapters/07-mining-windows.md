# Note — Mining (Windows Workflow)

Narrative Chapter 6 is now
[`06-mining.md`](./06-mining.md)
(Chinese: [`06-mining.zh.md`](./06-mining.zh.md)).
This file keeps the public Windows product path. It does not change
consensus.

# Chapter 7 — Mining (Windows Workflow)

Mining in the official public story is the **Windows CPU solo miner talking to
your own node**.

This chapter does **not** document SOAP background mining, soak archives, or
“read the daemon logs” as the user path.

---

## What mining does

SHA3-256 (`RGRPOW01`) proof-of-work discovers valid headers and orders
blocks. ASERT adjusts difficulty so block time stays on the frozen target
spacing.

Mining **pays ORU**. Mining **does not mint RES**. No policy, local or
delegated, can allocate RES.

---

## Solo is the shipped path

There is no official pool and no `stratum+tcp://` address. The miner speaks
JSON-RPC to the wallet on the same computer, over `127.0.0.1:8761` only.

| | Solo (shipped) | Pool (not the product) |
|---|---|---|
| Work source | `getblocktemplate` from your own node | Stratum from a third party |
| Coinbase | Built by your node, paid to your `rg1q` | Built by the pool operator |
| Trust | Your node validates everything | You trust an operator's dashboard |
| Status in this release | Official | `pool/` exists in-tree, quarantined, not part of the user path |

The miner copies the node's coinbase bytes verbatim. It does not invent an
extra-nonce and it cannot add outputs, so it cannot mint RES even if asked.

---

## Official user path

```
Windows user
    ↓
Download and checksum the miner
    ↓
Start the wallet (local RPC 127.0.0.1:8761)
    ↓
Copy an rg1q payout address
    ↓
Point the miner at your own node
    ↓
Mine ORU
    ↓
Converge ORU
    ↓
Same tx mints RES at destroy_cost(h)
```

| Step | Action | Result |
|------|--------|--------|
| 1 | Download the Windows miner | Verify SHA256 against `SHA256SUMS.txt` from the same release |
| 2 | Start the wallet | Local RPC comes up on `127.0.0.1:8761`; never publish that port |
| 3 | Copy a payout address | Receive tab, must start with `rg1q` (Testnet `trg1q` will be rejected) |
| 4 | Start the miner | `rigor-miner.cmd` reads local RPC credentials and asks for the payout address |
| 5 | Mine ORU | Valid block → ORU coinbase (immature, then trusted) |
| 6 | Converge ORU | Wallet creates a convergence transaction (destroy `B` ORU) |
| 7 | Generate RES | The **same transaction** MAY mint Witness v1 RES at `destroy_cost(h)` when `R > 0 → R · (2²⁵ + h)² ≤ B << 40` |

---

## Windows scenario

1. Start Windows Wallet (`installer/windows/start-wallet.cmd`)
2. Start Windows Miner (`installer/windows/rigor-miner.cmd`)
3. Confirm the miner reports the local node and the current height
4. Observe hashrate and height
5. Observe ORU maturity (immature coinbase vs trusted ORU)
6. Converge ORU; the same transaction mints RES at `destroy_cost(h)` when the predicate holds
7. Observe RES
8. Verify a block hash independently (Zero Trust), not by trusting any dashboard

---

## Two things that surprise new miners

**While the chain is at genesis, `getblocktemplate` refuses to serve.** Height 0
is below minimum chain work, so the node reports initial block download and
returns `-10`. Set `solomining=1` in the wallet's `rigor.conf` to mine height 1.
That flag is RPC policy only: it changes no consensus rule and exposes no port.

**Early blocks arrive faster than the target spacing.** Genesis only seeds a
difficulty; ASERT anchors at height 1 and then converges, and it may move the
target by at most 12.5% per block. If real hashrate arrives well above the seed
assumption, the first few dozen blocks come in quickly while difficulty catches
up. That is the retarget working, not an emission bug.

---

## Sentences that must remain true

- Miners mine **ORU**.
- RES is **not mined**.
- **No Convergence → No RES.**
- No operator policy can mint RES.
- The miner talks to your node, not to the Internet.
- A found block is checked with public data, not with a dashboard opinion.
