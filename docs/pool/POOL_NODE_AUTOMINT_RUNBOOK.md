# POOL_NODE_AUTOMINT_RUNBOOK

**Task:** `TASK-POOL-RES-BOUNDARY-IMPLEMENT-003` · Phase 3  
**Audience:** Pool / solo operators running `rigord`  
**Consensus:** Unchanged

---

## 1. Principle

After Option A, the **pool never builds RES mints**. When a convergence draw has a winner, the **node** must attach the body mint inside `CreateNewBlock` / GBT via `auto_res_mint.cpp`.

```text
User wallet Convergence → W=32 eligibility → mempool / chain draw
        ↓
rigord CreateNewBlock (+ TryCreateAutomaticResMintTx)
        ↓
GBT template (opaque to pool)
        ↓
Pool Stratum / submitblock (ORU coordination only)
```

---

## 2. Required node configuration

### `-miningresfeeaddr=<witness-v1-RES address>`

- Witness-v1 RES (ORB2-tagged) address.
- Required when:
  - mempool has RES-domain fee-paying transactions, **or**
  - a parent draw has a winner and automint must attach a body mint (fee / receive script path in assembler).
- Set in `rigor.conf` or CLI; restart `rigord` after change.

Example:

```text
miningresfeeaddr=rg1p...   # witness-v1 RES
```

### Fund the automint bootstrap UTXO

1. Call RPC:

```bash
rigor-cli getautomintfundaddress
```

Returns:

- `address` — bech32 P2WSH fund address (`AutoResMintBootstrap`)
- `scriptPubKey` — hex
- `note` — operational hint

2. Send a small ORU amount to that address (e.g. `sendtoaddress <address> 1.0` with an appropriate fee rate). Mempool confirmation is enough if the fund tx is selected into the same template.

3. Ensure `-miningresfeeaddr` is set, then resume GBT / mining.

If a winner is present and automint cannot fund/assemble, `CreateNewBlock` / `getblocktemplate` fails closed with an explicit error (do **not** invent a pool-side mint).

---

## 3. Operator requirements (pool)

| Requirement | Notes |
|-------------|--------|
| Point pool GBT at a correctly configured `rigord` | Pool consumes templates as opaque |
| Configure ORU coinbase payout script | `RIGOR_POOL_PAYOUT_SCRIPT_HEX` |
| Configure Stratum worker credentials | `RIGOR_POOL_WORKER_CREDENTIALS` |
| Configure `submitblock` RPC | Pool ORU path only |
| **Do not** set pool ORU funding for RES mint | Removed under Option A |
| **Do not** expect Stratum to mint RES | Miners earn ORU only |

---

## 4. Verification

| Check | Expectation |
|-------|-------------|
| `getautomintfundaddress` | Returns P2WSH address |
| GBT with no winner | Template without body mint; pool mines ORU |
| GBT with winner + funded automint | Template may include mint tx as opaque GBT entry |
| Pool metrics `res_mint_events` | Remains `0` |

---

## 5. Code authority (do not modify for pool ops)

| Component | Path |
|-----------|------|
| Template assembly | `core/src/node/miner.cpp` (`CreateNewBlock`) |
| Auto RES mint | `core/src/node/auto_res_mint.cpp` |
| RPC | `getautomintfundaddress`, `getblocktemplate`, `submitblock` |

Wallet Convergence logic and consensus W/T/RES parameters are **out of scope** for pool configuration.
