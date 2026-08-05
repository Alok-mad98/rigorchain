import hashlib
import time

# --- RIGOR Chain Genesis Parameters ---
# 1. Time: May 22, 2026 00:00:00 UTC
nTime = 1747872000 

# 2. Nonce: Start from 0 (We want the first valid hash)
nNonce = 0

# 3. Timestamp String (The "Coinbase" message)
pszTimestamp = "Selection not computation. Convergence defines state. Naming is consensus."

# 4. Previous Block Hash: All zeros (It's the first block!)
hashPrevBlock = 0

# 5. Merkle Root: All zeros (No transactions yet)
hashMerkleRoot = 0

# --- Calculation Logic ---
# We need to hash: BlockHeader + TimestampString
# BlockHeader is 80 bytes: Version(4) + PrevHash(32) + MerkleRoot(32) + Time(4) + Bits(4) + Nonce(4)

version = 1
nBits = 0x1d00ffff # Standard difficulty target

# Serialize the header
# Little-endian for integers
header = (
    version.to_bytes(4, 'little') +
    hashPrevBlock.to_bytes(32, 'little') +
    hashMerkleRoot.to_bytes(32, 'little') +
    nTime.to_bytes(4, 'little') +
    nBits.to_bytes(4, 'little') +
    nNonce.to_bytes(4, 'little')
)

# Add the timestamp string length and content (VarInt + String)
# Length of string is 73 bytes
timestamp_bytes = pszTimestamp.encode('ascii')
data_to_hash = header + (len(timestamp_bytes)).to_bytes(1, 'little') + timestamp_bytes

# Double SHA256
hash1 = hashlib.sha256(data_to_hash).digest()
hash2 = hashlib.sha256(hash1).digest()

# Reverse byte order for display (Bitcoin standard)
genesis_hash = hash2[::-1].hex()

print(f"--- RIGOR Chain Genesis Parameters ---")
print(f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(nTime))} UTC")
print(f"Nonce: {nNonce}")
print(f"Message: {pszTimestamp}")
print(f"--------------------------------------")
print(f"Genesis Block Hash:")
print(f"0x{genesis_hash}")
print(f"--------------------------------------")
print(f"C++ Code Snippet:")
print(f'consensus.hashGenesisBlock = uint256S("0x{genesis_hash}");')
print()
print("NOTE: This script uses a simplified header+message hash model.")
print("It does NOT reproduce CreateGenesisBlock / rigord genesis hashes.")
print("Authoritative cross-check: rigord getblockhash 0 (LOCAL_VALIDATION §7.1).")
