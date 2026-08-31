Bitcoin leftover tests that do not apply to RIGOR.

Buried CSV/CLTV/DERSIG/Segwit are genesis-active. There is no anyone-can-spend
output, no secp P2PK coinbase, and no `-testactivationheight`. Live coverage is:

- BIP68 nSequence: `feature_bip68_sequence.py`
- NODE_WITNESS fail-closed: `p2p_witness_mandatory.py`
- DERSIG/Ed25519 encoding: `feature_dersig.py`
- Descriptor import: `wallet_importdescriptors.py` (wpkh/res only)
- Non-ORU/RES outputs: `feature_single_key_outputs.py`

These files are not in `test_runner.py`.
