# Serialization Boundary

Deterministic encoding of Coq kernel objects.

**Claim:** encoding contract verified. Crypto layer not verified.

## Cover

| Object | Encoder | Identity |
|---|---|---|
| UTXO state | `encode_utxo` | `same_utxo_same_encoding` |
| ORU state | `encode_oru` / `decode_oru` | `decode_encode_identity` |
| RES schedule | `encode_res_schedule` | `same_res_schedule_same_encoding` |
| PoW target | `encode_pow_target` | `same_pow_target_same_encoding` |

Headline theorems in `Kernel/SerializationBoundary.v`:

- `encoding_deterministic`
- `same_state_same_encoding`
- `decode_encode_identity` (ORU fixed-width list)

`Print Assumptions` Closed. No `Admitted` / `Axiom` / `Parameter`.

Encoding contract and extraction source-identity are proved.
Cryptographic primitives and a full extraction refinement are **not**
proved (`Kernel/CryptoAssumptions.v`,
`encoding_contract_proved_crypto_not_proved`).

## In scope

Kernel-model field tuples as `list Z`. Same Coq state, same encoding.

## Out of scope

- Cryptographic hash
- Network / P2P serialization
- Transaction wire format
- Block file / database format
- C++ `serialize` equivalence
