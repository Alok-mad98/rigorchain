# Rigor cpuminer (minerd) - Mining Client

This is a modified version of cpuminer that fully supports Rigor's GetBlockTemplate (GBT) protocol, including the convergence_root calculation required by Rigor consensus rules.

## Freeze Notes (TASK-MINING-002)

This client conforms to **MINING-CONTRACT-v1**. Two consensus-required corrections
were applied to the coinbase RIGOR draw-target commitment:

1. **Tag bytes**: payload prefix is now `R`,`I`,`G`,`R` (`RigorTags::RIGOR`),
   matching `core/src/script/standard.cpp::IsRigorCommitment`. The previous
   `A`,`R`,`D`,`A` bytes caused `bad-rigor-commitment-missing`.
2. **`prev_nbits` source**: payload tail (LE32) is now sourced from the
   `prev_nbits` field of `getblocktemplate` (== `pindexPrev->nBits`), not the
   template `bits` field (== next-block target). The previous behaviour
   triggered `bad-rigor-target-mismatch` at every difficulty-retarget boundary.

Miners MUST connect to a node exposing `prev_nbits` via `getblocktemplate`
(rigord ≥ MINING-CONTRACT-v1). The client refuses to build a coinbase if the
field is missing.

## Key Modifications

### 1. Convergence Root Calculation
- **Added**: `compute_convergence_root()` function that implements Rigor's normative convergence_root calculation
- **Algorithm**:
  1. If fewer than 2 txids, convergence_root is all-zero (32 bytes)
  2. Sort txids in ascending order (uint256 memcmp order)
  3. Concatenate all txids in their raw binary form (32 bytes each)
  4. Compute Hash256 (double SHA256) of the concatenated data
- **Reference**: Implements the logic from `core/src/consensus/convergence_root.cpp`

### 2. Coinbase Transaction Construction
- **Added**: Embedding of convergence_root in coinbase scriptSig
- **Format**: BIP34 height + OP_PUSHBYTES_32 + 32-byte convergence_root
- **Reference**: Matches the format used in `core/src/node/miner.cpp`

### 3. CTxOut Serialization Fix
- **Fixed**: Removed incorrect asset byte that was being added after nValue
- **Correct Format**: CTxOut serializes only `nValue` (8 bytes) + `scriptPubKey` (compact size + data)
- **Reference**: Based on `core/src/primitives/transaction.h` which shows AssetType is inferred from scriptPubKey, not serialized

### 4. GBT RPC Extension
- **Node Side**: Added `convergence_txids` field to `getblocktemplate` RPC response
- **Miner Side**: Parses `convergence_txids` array from GBT JSON
- **Purpose**: Provides miners with the txids needed to compute convergence_root

## Building

### Prerequisites
- GCC/G++ compiler
- libcurl development headers
- pthread library
- jansson (included in compat/)

### Compilation
```bash
cd ./cpuminer
./autogen.sh
./configure
make
```

The compiled binary will be `./minerd`.

## Running

### Basic Usage
```bash
./minerd -a sha256d -o http://127.0.0.1:48771 -u miningtest -p miningpass123
```

### Parameters
- `-a sha256d`: Use SHA256d algorithm (Rigor uses OpusHash internally, but GBT provides the block template)
- `-o http://host:port`: RPC URL of the Rigor node
- `-u username`: RPC username
- `-p password`: RPC password
- `-t N`: Number of mining threads (default: number of CPU cores)

### Testing with Local Node
Use the provided test script:
```bash
cd .
./test_mining_local.sh
```

This will:
1. Start a devtest Rigor node
2. Launch minerd to mine blocks
3. Display mining progress and balance

## Architecture Decision

**Key Principle**: The miner adapts to the node's serialization rules, not the other way around.

This approach ensures:
- Consensus correctness: The miner generates blocks that match the node's exact validation logic
- Maintainability: Changes to consensus rules only need to be made in the node
- Compatibility: The miner works with any Rigor node that implements the standard GBT protocol

## Files Modified

### cpuminer Side
- `cpu-miner.c`: Added convergence_root calculation, coinbase scriptSig modification, CTxOut serialization fix

### Node Side
- `core/src/rpc/mining.cpp`: Added `convergence_txids` field to getblocktemplate RPC

## Verification

The miner has been tested successfully:
- Mined 150+ blocks on devtest network
- Balance increased from 1000 to 155000 ORU
- All blocks accepted by the node's AcceptBlock validation

## Consensus Compliance

This implementation strictly follows Rigor consensus rules as defined in:
- `core/src/consensus/convergence_root.cpp` - Convergence root calculation
- `core/src/consensus/convergence_root.h` - Convergence set definition
- `core/src/primitives/transaction.h` - Transaction serialization
- `core/src/node/miner.cpp` - Coinbase construction

## Troubleshooting

### "JSON invalid convergence_txids"
- Ensure the node is running the modified version with convergence_txids support
- Check that the node is on devtest or a network that supports convergence

### Blocks rejected by node
- Verify the node version includes the convergence_txids GBT extension
- Check that the miner is using the correct RPC credentials
- Review the node's debug.log for specific rejection reasons

## License

This is a modified version of cpuminer, originally licensed under GPL v2.
