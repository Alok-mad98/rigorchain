# Security Hardening Test Report

**Date:** 2026-09-30
**Binary:** `core/src/test/test_rigor`, rebuilt after the six patches and the regtest assumeutxo pin update.
**Consensus files:** unchanged. See `CONSENSUS_FREEZE_CHECK_BEFORE_SECURITY_PATCH.md`.

## Patches

| Fix | Changed files | Reason | Security impact | Consensus impact |
|-----|---------------|--------|-----------------|------------------|
| Assumeutxo commitment | `core/src/kernel/coinstats.cpp`, `core/src/kernel/coinstats.h`, `core/src/kernel/chainparams.cpp`, `core/src/test/coinstatsindex_tests.cpp`, `core/src/test/validation_tests.cpp` | `hash_serialized_2` dropped height and the coinbase flag because `?:` bound outside the addition. | A snapshot can no longer match after lying about height or coinbase. Regtest pins were regenerated. Mainnet still has no pin. | None. UTXO validity, `ConnectBlock`, and maturity rules are unchanged. MuHash still uses `TxOutSer`. |
| Onion whitelist | `core/src/net.cpp`, `core/src/net.h`, `core/src/test/net_tests.cpp` | An inbound onion accept is from the local Tor daemon. `-whitelist` of that address was applied to the remote peer. | Localhost whitelist no longer grants NoBan, ForceRelay, Mempool, or Relay to onion inbounds. Explicit `-whitebind` flags on the listen socket are unchanged. Clearnet accepts still consult `-whitelist`. | None. |
| Compact-block witness | `core/src/blockencodings.cpp`, `core/src/test/blockencodings_tests.cpp` | `FillBlock` stopped at `CheckBlock`. A witness-merkle mismatch is not in that check. | Reconstruction returns `READ_STATUS_FAILED` when a coinbase witness commitment does not match the reconstructed witness merkle root, or the reserved-nonce stack is not one 32-byte element. A matching commitment with a non-zero nonce still returns `READ_STATUS_OK`. The 32-zero-byte nonce rule stays in `ContextualCheckBlock`. | None. No Taproot behavior. Nonce consensus is not relaxed and is not moved. |
| Log rate limit | `core/src/logging.cpp`, `core/src/logging.h`, `core/src/test/logging_tests.cpp` | Self-connection and invalid-chain discovery lines are unconditional and peer-controlled. | Each of those two sources may write 1 MiB per hour. The first line is always written. One suppression notice follows. Other `LogPrintf` lines, including consensus errors, are not limited. Peer ban rules are unchanged. `validation.cpp` was not edited. | None. |
| Settings write | `core/src/util/settings.cpp`, `core/src/test/settings_tests.cpp` | `WriteSettings` returned true without checking the stream. | A failed temp write returns false. `WriteSettingsFile` already skips `RenameOver` in that case, so the live `settings.json` is left in place. | None. Wallet format is unchanged. |
| 32-bit resource caps | `core/src/node/mempool_args.h`, `core/src/node/mempool_args.cpp`, `core/src/txdb.h`, `core/src/test/mempool_tests.cpp` | Bitcoin Core CVE-2025-46597 is a 32-bit overflow in block resource accounting. The mempool and dbcache caps were already in tree. | `MempoolSizeAllowed` rejects `-maxmempool` above 500 MB on 32-bit. `MaxDbCacheMiB` stays 1024 on 32-bit and 16384 on 64-bit. A 64-bit build does not change its limits. `CheckBlock` was not edited. | None. |

## 32-bit support

Official release artifacts are x86_64 (Linux, Windows, and macOS). RIGOR does not publish a 32-bit production build. `i686-pc-linux-gnu` remains a depends cross-compile triplet only. The caps stay so a 32-bit configure cannot raise `-maxmempool` into the range that overflowed on 32-bit `size_t`. On 64-bit `size_t` the `CheckBlock` multiplication does not wrap, and this series does not change that function.

## Tests run

All of the following completed with `*** No errors detected`.

New cases:

- `coinstatsindex_tests/hash_serialized_commits_height_and_coinbase`
- `net_tests/inbound_onion_does_not_inherit_localhost_whitelist`
- `blockencodings_tests/witness_commitment_mismatch_fails_reconstruction`
- `logging_tests/attacker_controlled_logs_are_rate_limited`
- `settings_tests/write_failure_does_not_replace_settings` (write to `/dev/full` fails; an existing settings file is left unchanged)
- `mempool_tests/thirty_two_bit_resource_caps_do_not_change_64bit`

Regtest pins were regenerated with `RIGOR_GEN_ASSUMEUTXO=1` and `validation_chainstate_tests/rigor_assumeutxo_param_gen`, then the binary was relinked.

Full suites, 48 cases: `blockencodings_tests`, `logging_tests`, `settings_tests`, `mempool_tests`, `net_tests`, `dns_seed_discovery_tests`, `coinstatsindex_tests`.

Consensus, script, transaction validation, wallet, and networking suites, 175 cases: `script_tests`, `script_standard_tests`, `script_parse_tests`, `rigor_opcode_namespace_tests`, `rigor_classification_vector_tests`, `rigor_predicate_registry_tests`, `rigor_sighash_vector_tests`, `test_script_opcode_security`, `transaction_tests`, `txvalidation_tests`, `txvalidationcache_tests`, `validation_tests`, `validation_chainstate_tests`, `validation_chainstatemanager_tests`, `rigor_consensus_kernel_tests`, `rigor_consensus_kernel_bridge_tests`, `test_consensus_kernel_freeze`, `test_consensus_kernel_single_authority`, `consensus_invariants_tests`, `test_pow_boundary_security`, `rigor_connectblock_gbt_vector_tests`, `denialofservice_tests`, `wallet_crypto_tests`, `wallet_transaction_tests`, `walletdb_tests`, `walletload_tests`, `scriptpubkeyman_tests`, `wallet_ed25519_key_consistency_tests`, `wallet_oru_fee_dominance_tests`, `wallet_key_model_status_tests`, `test_wallet_security_boundary`.

`validation_tests/test_assumeutxo` remains disabled in source. Its expected digests were updated to the regenerated pins so they do not go stale. `validation_chainstatemanager_tests` loads the height-110 regtest pin through `ExpectedAssumeutxo` and passed after the relink.

`net_tests/inbound_onion_does_not_inherit_localhost_whitelist` was run again after it was placed in `net_tests`. That rerun reported no errors.
