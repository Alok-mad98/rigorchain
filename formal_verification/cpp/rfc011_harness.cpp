// RFC-011 Differential Fuzzing C++ Harness
// ==========================================
//
// This program accepts test case input via stdin (JSON) and outputs
// full execution trace for differential fuzzing.
//
// Input format (JSON):
// {
//   "txids": ["hex", ...],
//   "fees": [int, ...],
//   "height": int,
//   "bind_hash": "hex",
//   "target_prev": int
// }
//
// Output format (JSON):
// {
//   "C": "hex",
//   "H": ["hex", ...],
//   "f": [int, ...],
//   "eligible": [bool, ...],
//   "winner_index": int | null,
//   "winner_txid": "hex" | null
// }

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <openssl/sha.h>

// =============================================================================
// SHA256 Implementation (OpenSSL - RFC 6234 compliant)
// =============================================================================

void sha256d(const std::vector<unsigned char>& data, unsigned char* output) {
    unsigned char temp[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), temp);
    SHA256(temp, SHA256_DIGEST_LENGTH, output);
}

// =============================================================================
// Helper Functions
// =============================================================================

std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return ss.str();
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), NULL, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

bool memcmp_compare(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
    size_t min_len = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_len; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return a.size() < b.size();
}

// =============================================================================
// ConvergenceRoot Implementation (RFC-000 §6)
// =============================================================================

std::vector<unsigned char> compute_convergence_root(const std::vector<std::vector<unsigned char>>& txids) {
    // Rule 1: If |F| < 2, return 32 zero bytes
    if (txids.size() < 2) {
        return std::vector<unsigned char>(32, 0);
    }
    
    // Rule 2: Sort by memcmp byte order
    std::vector<std::vector<unsigned char>> sorted_txids = txids;
    std::sort(sorted_txids.begin(), sorted_txids.end(), memcmp_compare);
    
    // Rule 3: Concatenate sorted txids
    std::vector<unsigned char> concatenated;
    for (const auto& txid : sorted_txids) {
        concatenated.insert(concatenated.end(), txid.begin(), txid.end());
    }
    
    // Rule 4: SHA256d
    unsigned char root[32];
    sha256d(concatenated, root);
    
    return std::vector<unsigned char>(root, root + 32);
}

// =============================================================================
// PCORB Hash Implementation (RFC-000 §4)
// =============================================================================

std::vector<unsigned char> compute_pcobr_hash(
    const std::vector<unsigned char>& txid,
    const std::vector<unsigned char>& bind_hash,
    int height,
    uint32_t target_prev,
    const std::vector<unsigned char>& convergence_root)
{
    // Build preimage: txid || bind_hash || LE64(height) || LE32(target_prev) || convergence_root
    std::vector<unsigned char> preimage;
    
    preimage.insert(preimage.end(), txid.begin(), txid.end());
    preimage.insert(preimage.end(), bind_hash.begin(), bind_hash.end());
    
    // LE64(height) - little-endian 64-bit
    for (int i = 0; i < 8; ++i) {
        preimage.push_back(static_cast<unsigned char>((static_cast<uint64_t>(height) >> (i * 8)) & 0xFF));
    }
    
    // LE32(target_prev) - little-endian 32-bit
    for (int i = 0; i < 4; ++i) {
        preimage.push_back(static_cast<unsigned char>((target_prev >> (i * 8)) & 0xFF));
    }
    
    preimage.insert(preimage.end(), convergence_root.begin(), convergence_root.end());
    
    // SHA256d
    unsigned char hash[32];
    sha256d(preimage, hash);
    
    return std::vector<unsigned char>(hash, hash + 32);
}

// =============================================================================
// Winner Selection (RFC-000 §1, §9)
// =============================================================================

std::vector<unsigned char> xor_bytes(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
    std::vector<unsigned char> result;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        result.push_back(a[i] ^ b[i]);
    }
    return result;
}

int select_winner(
    const std::vector<std::vector<unsigned char>>& txids,
    const std::vector<unsigned char>& bind_hash,
    int height,
    uint32_t target_prev,
    const std::vector<unsigned char>& convergence_root)
{
    if (txids.size() < 2) {
        return -1; // No winner
    }
    
    int best_index = -1;
    std::vector<unsigned char> best_hash;
    
    for (size_t i = 0; i < txids.size(); ++i) {
        std::vector<unsigned char> pcobr = compute_pcobr_hash(
            txids[i], bind_hash, height, target_prev, convergence_root
        );
        
        if (best_index == -1 || pcobr < best_hash) {
            best_hash = pcobr;
            best_index = i;
        } else if (pcobr == best_hash) {
            // Tie-break: txid XOR convergence_root, memcmp comparison
            std::vector<unsigned char> key_a = xor_bytes(txids[i], convergence_root);
            std::vector<unsigned char> key_b = xor_bytes(txids[best_index], convergence_root);
            
            if (memcmp_compare(key_a, key_b)) {
                best_hash = pcobr;
                best_index = i;
            }
        }
    }
    
    return best_index;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    try {
        // Simple line-based input format:
        // Line 1: num_txids
        // Line 2: height
        // Line 3: target_prev
        // Line 4: bind_hash (hex)
        // Lines 5+: txids (hex, one per line)
        // Lines after txids: fees (one per line)
        
        std::string line;
        
        // Read num_txids
        std::getline(std::cin, line);
        size_t num_txids = std::stoul(line);
        
        // Read height
        std::getline(std::cin, line);
        int height = std::stoi(line);
        
        // Read target_prev
        std::getline(std::cin, line);
        uint32_t target_prev = std::stoul(line);
        
        // Read bind_hash
        std::getline(std::cin, line);
        std::vector<unsigned char> bind_hash = hex_to_bytes(line);
        
        // Read txids
        std::vector<std::vector<unsigned char>> txids;
        for (size_t i = 0; i < num_txids; ++i) {
            std::getline(std::cin, line);
            txids.push_back(hex_to_bytes(line));
        }
        
        // Read fees
        std::vector<uint64_t> fees;
        for (size_t i = 0; i < num_txids; ++i) {
            std::getline(std::cin, line);
            fees.push_back(std::stoull(line));
        }
        
        // Step 1: Compute ConvergenceRoot (C)
        std::vector<unsigned char> C = compute_convergence_root(txids);
        
        // Step 2: Compute sigma hashes (H) for each txid
        std::vector<std::vector<unsigned char>> H;
        for (const auto& txid : txids) {
            H.push_back(compute_pcobr_hash(txid, bind_hash, height, target_prev, C));
        }
        
        // Step 3: Determine eligibility (all true for now - inequality check not implemented)
        std::vector<bool> eligible(txids.size(), true);
        
        // Step 4: Select winner
        int winner_index = select_winner(txids, bind_hash, height, target_prev, C);
        
        // Simple line-based output format:
        // Line 1: C (hex)
        // Line 2: winner_index (int, -1 if none)
        // Line 3: winner_txid (hex, or "null")
        // Line 4+: H values (hex, one per line)
        
        std::cout << bytes_to_hex(C.data(), C.size()) << std::endl;
        std::cout << winner_index << std::endl;
        
        if (winner_index >= 0) {
            std::cout << bytes_to_hex(txids[winner_index].data(), txids[winner_index].size()) << std::endl;
        } else {
            std::cout << "null" << std::endl;
        }
        
        for (const auto& h : H) {
            std::cout << bytes_to_hex(h.data(), h.size()) << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
