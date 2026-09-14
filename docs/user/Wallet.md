# RIGOR Wallet User Workflow Guide

| Field | Value |
|-------|-------|
| **Assets** | **ORU** / **RES** |
| **Document status** | Production documentation |
| **Aligned** | P0-097 implementation facts |
| **Date** | 2026-08-05 |
| **Audience** | Operators and end users of `rigor-cli` / `rigor-qt` |
| **Consensus impact** | **NONE** |

All required user workflows are implemented in wallet RPC (and Qt where noted). This guide is the product-facing closure of creation, backup, restore, send, receive, convergence transactions, and RES receive/send.

**Prefer `backupwallet` / `restorewallet` over `dumpwallet` / `importwallet` for RES** — dump/import does not fully round-trip RES Ed25519 material.

---

## Prerequisites

- Synced `rigord` (or embedded node via Qt)
- Wallet enabled build
- RPC auth configured (`rpcauth` / cookie); **never** expose RPC publicly

---

## 1. Creation

```bash
rigor-cli createwallet "main"
# or Qt: File → Create Wallet (descriptor wallet)
```

Success: wallet loads; `getwalletinfo` returns name and balances.

---

## 2. Receive ORU

```bash
rigor-cli -rpcwallet=main getnewaddress "" "ORU"
```

Qt: **Receive** → address type **ORU**. Share bech32 `rg1q…` (Witness v0).

---

## 3. Receive RES

```bash
rigor-cli -rpcwallet=main getnewaddress "" "RES"
```

Qt: **Receive** → address type **RES**. Share Bech32m `rg1p…` (Witness v1).

RES can also arrive in the **same Convergence transaction** that destroys ORU, when `R · (2²⁵+h)² ≤ B << 40`. There is no draw payout and no separate “claim” RPC.

---

## 4. Send ORU

```bash
rigor-cli -rpcwallet=main sendtoaddress "<oru_address>" 1.0
# or: sendtoaddress "<addr>" 1.0 "" "" true true null "unset" null "ORU"
```

Qt: **Send** → asset **ORU**. Network fees are paid in ORU only.

---

## 5. Send RES

```bash
rigor-cli -rpcwallet=main sendres "<res_address>" 1.00
# equivalent: sendtoaddress with asset=RES
```

Qt: **Send** → asset **RES** (uses send path with RES domain checks). Destination must be Witness v1 RES.

---

## 6. Convergence transaction

Creates a convergence transaction that commits ORU into an unspendable CVG2 output so the entry can participate in deterministic consensus allocation. Minimum **1 ORU**.

```bash
rigor-cli -rpcwallet=main createconvergence 1.0
```

Qt: **Convergence** page / wizard — amount ≥ 1 ORU.

Notes:

- This is **not** a mining subsidy and does **not** guarantee that RES is minted.
- Eligibility and winner selection are consensus-deterministic over the windowed candidate set.
- Product terminology: **convergence transaction**; successful RES creation is **RES minted** through **deterministic consensus allocation**.
- **ORU provides PoW security and economic coordination.**
---

## 7. Backup

```bash
rigor-cli -rpcwallet=main backupwallet /secure/path/wallet-backup.dat
```

Qt: **File → Backup Wallet**. Store offline; encrypt the host.

---

## 8. Restore

```bash
rigor-cli restorewallet "restored" /secure/path/wallet-backup.dat
```

Qt: restore wallet activity → select backup file. Rescan as prompted until balances match expectation.

---

## 9. Smoke checklist (operator)

| Step | Command / UI | Pass |
|------|--------------|------|
| Create | `createwallet` | ☐ |
| Receive ORU | `getnewaddress "" ORU` | ☐ |
| Receive RES | `getnewaddress "" RES` | ☐ |
| Send ORU | `sendtoaddress` | ☐ |
| Send RES | `sendres` | ☐ |
| Converge | `createconvergence 1.0` | ☐ |
| Backup | `backupwallet` | ☐ |
| Restore | `restorewallet` | ☐ |

Automated rehearsal exists under `contrib/testing/release006_production_rehearsal/` and `contrib/testing/release008_local_production/` (operator-run against a local node).

---

## 10. Installer

Windows NSIS: `installer/windows/rigor_installer.nsi` + `scripts/build/build_nsis_installer.sh` → staged under `dist/windows/installers/`.  
Linux/macOS: build `rigor-qt` / `rigord` / `rigor-cli` per `doc/build-*.md`; package via release pipeline when Guix/signed artifacts are produced.

---

## Related

- [RIGOR_WALLET_USER_JOURNEYS.md](./RIGOR_WALLET_USER_JOURNEYS.md)
- [WALLET_TESTNET_OPERATION.md](./WALLET_TESTNET_OPERATION.md)
- [ARCHITECTURE.md](../../ARCHITECTURE.md)
