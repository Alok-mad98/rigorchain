# RIGOR Wallet UX Specification

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only UX specification — **no implementation**

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

**Applies to:** RIGOR Wallet (signing) primary; RIGOR Observer where noted as read-only variant.

**Terminology lock:** `OpuHash`, convergence (not "burn" in UI labels), Mint (not "issue/generate/create" for RES).

---

## Global UX principles

1. **ORU and RES are always visually separated** — never a combined "total balance."
2. **Convergence is a named workflow** — not buried inside generic "send."
3. **Authority-derived facts** — eligibility, winners, probability target come from node/explorer; UI shows source badge.
4. **txid is primary identity** — wtxid secondary, monospace, copyable.
5. **Sync state is plain language** — "Syncing block X of Y" not only IBD flags.
6. **`equivalence_claimed: false`** — shown in About/Compliance; never implies mainnet guarantee.

---

## Journey A — Install → Sync → Receive ORU

**Actor:** New testnet participant  
**Product:** RIGOR Wallet (full node)

### A.1 Install

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | OS installer / store | Install binaries | Welcome → choose data directory |
| 2 | First-run wizard | Create or open wallet | "Create new wallet" / "Restore" / "Watch only" |
| 3 | First-run wizard | Generate descriptor wallet | Seed backup prompt (12/24 words) |
| 4 | First-run wizard | Confirm seed written | Checkbox acknowledgment |
| 5 | Dashboard | Start rigord (embedded or detected) | "Starting node…" |

**Success:** App opens to Dashboard with wallet created.

**Observer variant:** Install Docker stack or open hosted URL → Watch setup (no seed).

### A.2 Sync

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | Dashboard | Poll `getblockchaininfo` | Progress bar: headers / blocks |
| 2 | Dashboard | IBD vs synced | "Syncing…" or "Up to date" |
| 3 | Dashboard | Compare explorer tip (optional) | Lag warning if enabled |
| 4 | Dashboard | Peer count | "Connected to N peers" |

**Success:** `initialblockdownload: false`; height matches network within tolerance.

**Failure paths:**

- No peers → Diagnostics: check firewall, addnode doc link
- Stalled sync → offer reindex guidance (advanced Settings)

### A.3 Receive ORU

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | ORU → Receive | `getnewaddress` (ORU descriptor) | QR + `oru1…` address |
| 2 | ORU → Receive | Copy / share | Copy button + "Send this address to receive ORU" |
| 3 | ORU asset view | Poll balance | Confirmed / unconfirmed ORU |
| 4 | History | Detect incoming tx | New row with confirmations |

**Success:** User shares address; incoming tx appears in History and ORU balance updates.

**Plain-language copy:**

> "ORU is the native mined asset on RIGOR. Share this address to receive ORU. Mining uses OpuHash."

---

## Journey B — Send ORU

**Actor:** Wallet holder  
**Product:** RIGOR Wallet

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | ORU → Send | Load spendable UTXOs | Available ORU balance |
| 2 | Send form | User enters address + amount | ORU-only asset selector (locked) |
| 3 | Send form | Fee estimation | Fee in ORU; total debit |
| 4 | Confirm dialog | Summary | From, to, amount, fee |
| 5 | Confirm dialog | Sign + broadcast | Password if encrypted |
| 6 | History | Mempool → confirmed | Status: pending → confirmed |

**Validation:**

- Reject non-ORU address types for ORU send
- Insufficient balance → inline error
- Double-submit guard on Confirm button

**Observer:** Journey not available (read-only); show disabled state with explanation.

---

## Journey C — Convergence → Receive RES

**Actor:** Wallet holder participating in convergence  
**Product:** RIGOR Wallet (+ Explorer enrichment)

### C.1 Understand convergence

| Step | Screen | Content |
|------|--------|---------|
| 1 | Convergence → Overview | Explain: ORU convergence transaction enters draw window |
| 2 | Convergence → Overview | Show `RES_PROBABILITY_TARGET` (authority field) |
| 3 | Convergence → Activity | Recent events, eligible set size, winner txid |

**Copy (locked terminology):**

> "A convergence transaction commits ORU toward a deterministic draw. If your transaction wins, RES is minted to your wallet."

### C.2 Perform convergence

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | Convergence → Send | Load ORU balance | Amount to converge |
| 2 | Convergence form | Round / fee hints from RPC | Estimated fee, eligible context |
| 3 | Confirm | `createconvergence` RPC | Summary + warnings |
| 4 | History | Convergence txid recorded | Link to tx viewer |
| 5 | Convergence → Activity | Poll explorer/RPC | Event appears when indexed |

### C.3 Receive RES (mint)

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | RES asset view | Detect mint via wallet + explorer | RES balance increase |
| 2 | RES → Mint history | Row per mint | mint txid, bind txid, bind height |
| 3 | RES detail | Linkage fields | convergence root, convergence txid, amount |
| 4 | Notification (optional) | Mint confirmed | "RES minted — view details" |

**Mandatory RES row fields** (per `WALLET_RES_DISPLAY_RULES.md`):

- mint txid, bind txid, bind height, convergence root, convergence txid, RES amount

**Forbidden in UI:** combined ORU+RES total; token badges; `rigor_target`, `T_conv`.

---

## Journey D — Restore wallet

**Actor:** Returning user or disaster recovery  
**Product:** RIGOR Wallet

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | First-run or File → Restore | Choose restore method | Seed / descriptor file / wallet.dat |
| 2 | Restore wizard | Validate mnemonic | Word entry grid |
| 3 | Restore wizard | Optional passphrase | BIP39 passphrase field |
| 4 | Restore wizard | Rescan blockchain | Progress: rescanning… |
| 5 | Dashboard | Sync completes | Balances restored |
| 6 | ORU + RES views | Post-rescan | Asset balances + history |

**Safety:**

- Clear warning: "Anyone with your seed controls your ORU and RES"
- Never upload seed to explorer or network services

**Observer variant:** Import watch addresses / txids only — no seed path.

---

## Journey E — Observe convergence history

**Actor:** Watcher, auditor, educator  
**Product:** RIGOR Observer (primary); RIGOR Wallet read-only tabs (secondary)

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | Watch → Add | Save to Watch Registry | ORU addr / RES addr / txid / conv txid |
| 2 | Convergence → Activity | Fetch `/convergence/events` | Paginated event table |
| 3 | Event row expand | Winner, eligible count, target | `RES_PROBABILITY_TARGET` |
| 4 | Event → tx link | Open Transaction Viewer | Full convergence metadata |
| 5 | Filter | By height range / address | Scoped history |

**Table columns (minimum):**

| Column | Source |
|--------|--------|
| Height | Explorer |
| Convergence txid | Explorer |
| Winner txid | Explorer / RPC |
| Eligible set size | Authority |
| RES probability target | Authority |
| Mint status | Explorer RES mint linkage |

---

## Journey F — Inspect protocol state

**Actor:** Advanced user, operator  
**Product:** Both

| Step | Screen | System action | User sees |
|------|--------|---------------|-----------|
| 1 | Dashboard | Chain + explorer health | Height, tip hash, peers, sync |
| 2 | Diagnostics | RPC `getblockchaininfo`, `getnetworkinfo` | Raw + friendly summary |
| 3 | Explorer panel | Chain tip compare | Node vs indexer lag |
| 4 | Compliance | `/compliance` | `equivalence_claimed: false`, policy version |
| 5 | Protocol snapshot (advanced) | Supply metrics if exposed | ORU/RES supply totals (dashboard target) |
| 6 | Logs (operator) | rigord debug tail | Filterable log view |

**Prototype carryover (future rigor-qt tab):**

- Convergence Inspector — round-level detail
- Protocol Storyboard — educational timeline (read-only)

---

## Journey map summary

```text
        Install ──► Sync ──► Receive ORU (A)
                              │
                              ▼
                         Send ORU (B)
                              │
                              ▼
                    Convergence send (C.2)
                              │
                              ▼
                      RES mint visible (C.3)

        Restore (D)          Observe history (E)          Inspect state (F)
        parallel paths       observer-first               all products
```

---

## Accessibility & error UX (target)

| Requirement | Target |
|-------------|--------|
| Keyboard navigation | All primary flows without mouse |
| Screen reader | Labels on balances, tx status, warnings |
| Error tone | Plain language + "What you can do" |
| Loading states | Skeleton or spinner per panel; never blank |
| Offline | Clear "Node disconnected" banner |

---

## Output table

| Field | Status |
|-------|--------|
| UX Specification | **COMPLETE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_UX_SPEC — TASK-WALLET-PROGRAM-002*
