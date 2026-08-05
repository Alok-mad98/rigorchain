#!/usr/bin/env python3
"""
Mine RIGOR Genesis Block
This script mines the genesis block by finding a nonce that satisfies the difficulty target.
"""
import hashlib
import struct
import time

# --- RIGOR Chain Genesis Parameters ---
TIMESTAMP = 1778214026  # 2026-05-08 12:20:26 UTC (today)
TIMESTAMP_MSG = "Selection not computation. Convergence defines state. Naming is consensus."
NBITS = 0x207fffff  # Very low difficulty
VERSION = 1

# Coinbase transaction parameters
GENESIS_REWARD = 1000 * 100_000_000  # 1000 ORU (1 ORU = 100,000,000 satoshis)
# Genesis output script (P2WPKH with dummy key)
GENESIS_OUTPUT_SCRIPT = bytes.fromhex("00141111111111111111111111111111111111111111")

def uint256_from_compact(nbits):
    """Convert compact nBits to target uint256"""
    nsize = nbits >> 24
    nword = nbits & 0x007fffff
    
    if nsize <= 3:
        return nword << (8 * (3 - nsize))
    else:
        return nword << (8 * (nsize - 3))

def create_coinbase_tx(timestamp_msg, output_script, reward):
    """Create the coinbase transaction"""
    # scriptSig: 486604799 (height) << 4 (height encoding) << timestamp_bytes
    timestamp_bytes = timestamp_msg.encode('ascii')
    script_sig = struct.pack('<I', 486604799) + struct.pack('<I', 4) + timestamp_bytes
    
    # Serialize transaction (simplified version for genesis)
    # This is a simplified serialization - the actual Bitcoin serialization is more complex
    tx_version = struct.pack('<I', 1)
    tx_in_count = struct.pack('<I', 1)
    tx_in_prev = b'\x00' * 32
    tx_in_index = struct.pack('<I', 0xffffffff)
    tx_in_script_len = struct.pack('<I', len(script_sig))
    tx_in_script = script_sig
    tx_in_sequence = struct.pack('<I', 0xffffffff)
    
    tx_out_count = struct.pack('<I', 1)
    tx_out_value = struct.pack('<Q', reward)
    tx_out_script_len = struct.pack('<I', len(output_script))
    tx_out_script = output_script
    
    tx_locktime = struct.pack('<I', 0)
    
    tx = (tx_version + tx_in_count + tx_in_prev + tx_in_index + 
          tx_in_script_len + tx_in_script + tx_in_sequence +
          tx_out_count + tx_out_value + tx_out_script_len + tx_out_script +
          tx_locktime)
    
    return tx

def double_sha256(data):
    """Compute double SHA256 hash"""
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def serialize_block_header(version, prev_hash, merkle_root, timestamp, bits, nonce):
    """Serialize block header"""
    return (struct.pack('<I', version) +
            prev_hash[::-1] +  # Reverse for little-endian
            merkle_root[::-1] +  # Reverse for little-endian
            struct.pack('<I', timestamp) +
            struct.pack('<I', bits) +
            struct.pack('<I', nonce))

def mine_genesis_block():
    """Mine the genesis block by finding valid nonce"""
    print(f"--- Mining RIGOR Genesis Block ---")
    print(f"Timestamp: {TIMESTAMP_MSG}")
    print(f"Time: {TIMESTAMP} ({time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime(TIMESTAMP))})")
    print(f"Target (nBits): 0x{NBITS:08x}")
    print(f"Reward: {GENESIS_REWARD / 100_000_000} ORU")
    print()
    
    # Create coinbase transaction
    coinbase_tx = create_coinbase_tx(TIMESTAMP_MSG, GENESIS_OUTPUT_SCRIPT, GENESIS_REWARD)
    merkle_root = double_sha256(coinbase_tx)
    
    print(f"Coinbase TX hash: {merkle_root[::-1].hex()}")
    
    # Calculate target from nBits
    target = uint256_from_compact(NBITS)
    print(f"Target: {target:064x}")
    
    # Mine the block
    prev_hash = b'\x00' * 32
    nonce = 0
    max_nonce = 0xffffffff
    
    print("Mining...")
    start_time = time.time()
    
    while nonce <= max_nonce:
        # Serialize block header
        header = serialize_block_header(VERSION, prev_hash, merkle_root, TIMESTAMP, NBITS, nonce)
        
        # Compute block hash
        block_hash = double_sha256(header)
        block_hash_int = int.from_bytes(block_hash[::-1], 'big')  # Convert to big-endian integer
        
        # Check if hash is below target
        if block_hash_int < target:
            elapsed = time.time() - start_time
            print(f"\n✓ Genesis block found!")
            print(f"Nonce: {nonce}")
            print(f"Block hash: {block_hash[::-1].hex()}")
            print(f"Time elapsed: {elapsed:.2f} seconds")
            print()
            print(f"--- C++ Code Snippet ---")
            print(f"namespace GenesisLockMainnet {{")
            print(f"constexpr uint32_t TIMESTAMP = {TIMESTAMP};")
            print(f"constexpr uint32_t NONCE = {nonce};")
            print(f"constexpr uint32_t NBITS = 0x{NBITS:08x};")
            print(f"constexpr int32_t VERSION = {VERSION};")
            print(f"static const char* const TIMESTAMP_MSG =")
            print(f'    "{TIMESTAMP_MSG}";')
            print(f"}} // namespace GenesisLockMainnet")
            print()
            print(f"consensus.hashGenesisBlock = uint256S(\"0x{block_hash[::-1].hex()}\");")
            return nonce, block_hash[::-1].hex()
        
        nonce += 1
        
        # Progress indicator
        if nonce % 1000000 == 0:
            elapsed = time.time() - start_time
            rate = nonce / elapsed if elapsed > 0 else 0
            print(f"Nonce: {nonce:,} ({rate:,.0f} nonces/sec)", end='\r')
    
    print(f"\n✗ Failed to find valid nonce within range")
    return None, None

if __name__ == "__main__":
    nonce, genesis_hash = mine_genesis_block()
    
    if nonce is not None:
        print(f"\nGenesis block successfully mined!")
        print(f"Use the NONCE value above in chainparams.cpp")
