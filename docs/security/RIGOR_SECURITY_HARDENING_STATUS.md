# RIGOR Security Hardening Status

**Date:** 2026-09-30
**Source:** the six approved items in `docs/security/BITCOIN_V25_TO_CURRENT_SECURITY_AUDIT.md`.
**Freeze:** `docs/security/CONSENSUS_FREEZE_CHECK_BEFORE_SECURITY_PATCH.md`.

| Fix | Applied | Consensus Impact |
|-----|---------|------------------|
| assumeutxo | yes | none |
| onion whitelist | yes | none |
| compact block | yes | none |
| log limits | yes | none |
| settings write | yes | none |
| 32-bit limits | yes | none |

32-bit note: official releases are x86_64 only. The 500 MB mempool cap and the 1 GiB dbcache cap apply when `sizeof(void*) == 4`. A normal 64-bit build keeps the previous 16 GiB dbcache ceiling and has no 500 MB mempool ceiling. `CheckBlock` was not modified.

These changes improve implementation robustness. They do not modify the RIGOR consensus protocol.
