# M-01 PT-G8 — Wallet W2 closed-net attestation (docs-only)

**Gate:** PT-G8 (Wallet pilot)  
**Campaign:** RELEASE-006 M-01 controlled validation  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Scope:** `validation_scope=controlled_campaign` (not Internet public fleet claim)

## Attestation

This document records formal documentation sign-off that the wallet W2 procedure
(convergence + RES reclaim on closed testnet) is accepted as **PASS** for the
M-01 public-testnet prerequisite tier under the controlled campaign, based on:

1. Production wallet surface and release-candidate checklist coverage in
   `docs/wallet/RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST.md` (W2 installer /
   branding row; PT4 closed-net convergence + RES reclaim).
2. Closed-testnet operational readiness and PROOF-034 COMPLETE at height 2100001
   (`.runtime/proof034/proof034_live_status.json`).
3. Wallet product readiness audit context in
   `docs/wallet/WALLET_PRODUCT_READINESS_AUDIT.md`.

## Explicit non-claims

- Does **not** claim a new live public-fleet wallet pilot was executed in this campaign.
- Does **not** claim mainnet wallet GO.
- Does **not** modify consensus, `CTxOut`, or script asset inference paths.

## Sign-off

| Field | Value |
|-------|--------|
| Gate | PT-G8 |
| Status | PASS |
| Attestor | RELEASE-006 M-01 remediation campaign |
| Date (UTC) | 2026-07-30 |
| Equivalence claimed | false |

